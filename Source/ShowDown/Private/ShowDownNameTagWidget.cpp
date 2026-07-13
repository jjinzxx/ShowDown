#include "ShowDownNameTagWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"

namespace
{
	constexpr float OverheadChatVisibleSeconds = 3.2f;
	constexpr float OverheadChatIntroSeconds = 0.18f;
	constexpr float OverheadChatFadeSeconds = 0.32f;
	constexpr float OverheadChatPushSeconds = 0.16f;
	constexpr float OverheadChatAnimationInterval = 1.0f / 30.0f;
	constexpr float OverheadChatMaxWidth = 260.0f;
	constexpr float OverheadChatPushDistance = 34.0f;
	constexpr float SpeakingIndicatorInterpSpeed = 10.0f;
	constexpr int32 MaxOverheadChatBubbleCount = 3;

	float EaseOutCubic(float Alpha)
	{
		const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
		const float InverseAlpha = 1.0f - ClampedAlpha;
		return 1.0f - InverseAlpha * InverseAlpha * InverseAlpha;
	}
}

void UShowDownNameTagWidget::SetDisplayName(const FText& NewDisplayName)
{
	CachedDisplayName = NewDisplayName;
	if (NameText)
	{
		NameText->SetText(CachedDisplayName);
	}
}

void UShowDownNameTagWidget::SetLives(int32 NewLives)
{
	CachedLives = FMath::Max(0, NewLives);
}

void UShowDownNameTagWidget::SetStatusText(const FText& NewStatusText)
{
	CachedStatusText = NewStatusText;
	if (StatusText)
	{
		StatusText->SetText(CachedStatusText);
		StatusText->SetVisibility(CachedStatusText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
}

void UShowDownNameTagWidget::SetTurnActive(bool bNewTurnActive)
{
	bCachedTurnActive = bNewTurnActive;
	RefreshNameBackgroundColor();
}

void UShowDownNameTagWidget::ShowOverheadChatMessage(const FText& NewChatText)
{
	const FString TrimmedChatText = NewChatText.ToString().TrimStartAndEnd();
	if (TrimmedChatText.IsEmpty())
	{
		return;
	}

	BuildDefaultWidget();
	if (!WidgetTree || !ChatStack)
	{
		return;
	}

	UWorld* World = GetWorld();
	const double CurrentTimeSeconds = World ? World->GetTimeSeconds() : 0.0;
	for (FShowDownOverheadChatBubbleEntry& ExistingBubble : ChatBubbles)
	{
		if (ExistingBubble.Background)
		{
			ExistingBubble.PushStartTimeSeconds = CurrentTimeSeconds;
			ExistingBubble.PushDistance = OverheadChatPushDistance;
		}
	}

	TrimOverheadChatBubbleCount();

	FShowDownOverheadChatBubbleEntry NewBubble;
	NewBubble.StartTimeSeconds = CurrentTimeSeconds;

	NewBubble.Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	NewBubble.Background->SetPadding(FMargin(10.0f, 5.0f));
	NewBubble.Background->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.68f));
	NewBubble.Background->SetVisibility(ESlateVisibility::HitTestInvisible);
	NewBubble.Background->SetRenderTransformPivot(FVector2D(0.5f, 1.0f));

	NewBubble.Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	NewBubble.Text->SetText(FText::FromString(TrimmedChatText.Left(240)));
	NewBubble.Text->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	NewBubble.Text->SetJustification(ETextJustify::Center);
	NewBubble.Text->SetAutoWrapText(true);
	NewBubble.Text->SetWrapTextAt(OverheadChatMaxWidth);
	NewBubble.Text->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 15));
	NewBubble.Text->SetShadowOffset(FVector2D(0.0f, 1.0f));
	NewBubble.Text->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f));
	NewBubble.Background->SetContent(NewBubble.Text);

	if (UVerticalBoxSlot* ChatSlot = ChatStack->AddChildToVerticalBox(NewBubble.Background))
	{
		ChatSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		ChatSlot->SetHorizontalAlignment(HAlign_Center);
	}

	ChatBubbles.Add(NewBubble);
	UpdateChatBubbleAnimation();
	RefreshChatBubbleTimer();
}

void UShowDownNameTagWidget::SetSpeakingIndicatorVisible(bool bVisible)
{
	bCachedSpeakingIndicatorVisible = bVisible;
	BuildDefaultWidget();
	if (bVisible && SpeakingIndicatorText)
	{
		SpeakingIndicatorText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

TSharedRef<SWidget> UShowDownNameTagWidget::RebuildWidget()
{
	BuildDefaultWidget();
	return Super::RebuildWidget();
}

void UShowDownNameTagWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BuildDefaultWidget();
	SetDisplayName(CachedDisplayName);
	SetLives(CachedLives);
	SetStatusText(CachedStatusText);
	SetTurnActive(bCachedTurnActive);
	SetSpeakingIndicatorVisible(bCachedSpeakingIndicatorVisible);
}

void UShowDownNameTagWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChatBubbleAnimationTimerHandle);
	}

	Super::NativeDestruct();
}

void UShowDownNameTagWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UpdateSpeakingIndicatorAnimation(InDeltaTime);
}

void UShowDownNameTagWidget::BuildDefaultWidget()
{
	if (!WidgetTree || NameText)
	{
		return;
	}

	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("NameTagRoot"));

	ChatStack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("OverheadChatStack"));

	NameBackground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("NameTagBackground"));
	NameBackground->SetPadding(FMargin(10.0f, 4.0f));

	NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NameText"));
	NameText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	NameText->SetJustification(ETextJustify::Center);
	NameText->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 18));
	NameText->SetShadowOffset(FVector2D(0.0f, 1.0f));
	NameText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));

	SpeakingIndicatorText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SpeakingIndicatorText"));
	SpeakingIndicatorText->SetText(FText::FromString(TEXT("말 하는 중...")));
	SpeakingIndicatorText->SetColorAndOpacity(FSlateColor(FLinearColor(0.70f, 0.92f, 1.0f, 1.0f)));
	SpeakingIndicatorText->SetJustification(ETextJustify::Center);
	SpeakingIndicatorText->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 12));
	SpeakingIndicatorText->SetShadowOffset(FVector2D(0.0f, 1.0f));
	SpeakingIndicatorText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));
	SpeakingIndicatorText->SetVisibility(ESlateVisibility::Collapsed);
	SpeakingIndicatorText->SetRenderOpacity(0.0f);

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
	StatusText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	StatusText->SetJustification(ETextJustify::Center);
	StatusText->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 15));
	StatusText->SetShadowOffset(FVector2D(0.0f, 1.0f));
	StatusText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));

	UHorizontalBox* NameRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("NameRow"));
	if (UHorizontalBoxSlot* NameTextSlot = NameRow->AddChildToHorizontalBox(NameText))
	{
		NameTextSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UHorizontalBoxSlot* SpeakingSlot = NameRow->AddChildToHorizontalBox(SpeakingIndicatorText))
	{
		SpeakingSlot->SetPadding(FMargin(7.0f, 0.0f, 0.0f, 0.0f));
		SpeakingSlot->SetVerticalAlignment(VAlign_Center);
	}

	NameBackground->SetContent(NameRow);

	UHorizontalBox* NameTagLine = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		TEXT("NameTagLine"));
	if (UHorizontalBoxSlot* NameBackgroundSlot = NameTagLine->AddChildToHorizontalBox(NameBackground))
	{
		NameBackgroundSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UVerticalBoxSlot* ChatStackSlot = Root->AddChildToVerticalBox(ChatStack))
	{
		ChatStackSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));
		ChatStackSlot->SetHorizontalAlignment(HAlign_Center);
	}

	if (UVerticalBoxSlot* NameSlot = Root->AddChildToVerticalBox(NameTagLine))
	{
		NameSlot->SetHorizontalAlignment(HAlign_Center);
	}

	if (UVerticalBoxSlot* StatusSlot = Root->AddChildToVerticalBox(StatusText))
	{
		StatusSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 0.0f));
		StatusSlot->SetHorizontalAlignment(HAlign_Center);
	}

	WidgetTree->RootWidget = Root;
	SetLives(CachedLives);
	RefreshNameBackgroundColor();
}

void UShowDownNameTagWidget::UpdateSpeakingIndicatorAnimation(float InDeltaTime)
{
	if (!SpeakingIndicatorText)
	{
		return;
	}

	const float TargetOpacity = bCachedSpeakingIndicatorVisible ? 1.0f : 0.0f;
	CurrentSpeakingIndicatorOpacity = InDeltaTime > 0.0f
		? FMath::FInterpTo(CurrentSpeakingIndicatorOpacity, TargetOpacity, InDeltaTime, SpeakingIndicatorInterpSpeed)
		: TargetOpacity;

	const bool bShouldShow = bCachedSpeakingIndicatorVisible || CurrentSpeakingIndicatorOpacity > 0.02f;
	SpeakingIndicatorText->SetVisibility(bShouldShow
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed);
	SpeakingIndicatorText->SetRenderOpacity(CurrentSpeakingIndicatorOpacity);
	SpeakingIndicatorText->SetRenderTranslation(FVector2D((1.0f - CurrentSpeakingIndicatorOpacity) * -4.0f, 0.0f));
}

void UShowDownNameTagWidget::RefreshNameBackgroundColor()
{
	if (!NameBackground)
	{
		return;
	}

	const FLinearColor BackgroundColor = bCachedTurnActive
		? FLinearColor(0.05f, 0.55f, 0.16f, 0.78f)
		: FLinearColor(0.0f, 0.0f, 0.0f, 0.58f);
	NameBackground->SetBrushColor(BackgroundColor);
}

void UShowDownNameTagWidget::UpdateChatBubbleAnimation()
{
	if (ChatBubbles.IsEmpty())
	{
		RefreshChatBubbleTimer();
		return;
	}

	const UWorld* World = GetWorld();
	const double CurrentTimeSeconds = World ? World->GetTimeSeconds() : 0.0;
	for (int32 BubbleIndex = ChatBubbles.Num() - 1; BubbleIndex >= 0; --BubbleIndex)
	{
		FShowDownOverheadChatBubbleEntry& Bubble = ChatBubbles[BubbleIndex];
		if (!Bubble.Background)
		{
			ChatBubbles.RemoveAt(BubbleIndex);
			continue;
		}

		const float ElapsedSeconds = static_cast<float>(CurrentTimeSeconds - Bubble.StartTimeSeconds);
		if (ElapsedSeconds >= OverheadChatVisibleSeconds)
		{
			if (ChatStack)
			{
				ChatStack->RemoveChild(Bubble.Background);
			}
			ChatBubbles.RemoveAt(BubbleIndex);
			continue;
		}

		const float IntroAlpha = EaseOutCubic(ElapsedSeconds / OverheadChatIntroSeconds);
		const float FadeStartSeconds = OverheadChatVisibleSeconds - OverheadChatFadeSeconds;
		const float FadeAlpha = ElapsedSeconds >= FadeStartSeconds
			? 1.0f - FMath::Clamp((ElapsedSeconds - FadeStartSeconds) / OverheadChatFadeSeconds, 0.0f, 1.0f)
			: 1.0f;
		const float PushElapsedSeconds = static_cast<float>(CurrentTimeSeconds - Bubble.PushStartTimeSeconds);
		const float PushAlpha = Bubble.PushDistance > 0.0f
			? EaseOutCubic(PushElapsedSeconds / OverheadChatPushSeconds)
			: 1.0f;
		const float PushOffset = Bubble.PushDistance * (1.0f - PushAlpha);
		if (PushAlpha >= 1.0f)
		{
			Bubble.PushDistance = 0.0f;
		}

		const float Opacity = FMath::Clamp(IntroAlpha * FadeAlpha, 0.0f, 1.0f);
		const float IntroOffset = (1.0f - IntroAlpha) * 8.0f;
		const float FadeOffset = ElapsedSeconds >= FadeStartSeconds ? -(1.0f - FadeAlpha) * 5.0f : 0.0f;
		const float Scale = 0.96f + 0.04f * IntroAlpha;

		Bubble.Background->SetRenderOpacity(Opacity);
		Bubble.Background->SetRenderTransform(FWidgetTransform(
			FVector2D(0.0f, IntroOffset + FadeOffset + PushOffset),
			FVector2D(Scale, Scale),
			FVector2D::ZeroVector,
			0.0f));
	}

	RefreshChatBubbleTimer();
}

void UShowDownNameTagWidget::HideOverheadChatMessage()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChatBubbleAnimationTimerHandle);
	}

	if (ChatStack)
	{
		for (const FShowDownOverheadChatBubbleEntry& Bubble : ChatBubbles)
		{
			if (Bubble.Background)
			{
				ChatStack->RemoveChild(Bubble.Background);
			}
		}
	}
	ChatBubbles.Reset();
}

void UShowDownNameTagWidget::TrimOverheadChatBubbleCount()
{
	while (ChatBubbles.Num() >= MaxOverheadChatBubbleCount)
	{
		const FShowDownOverheadChatBubbleEntry& OldestBubble = ChatBubbles[0];
		if (ChatStack && OldestBubble.Background)
		{
			ChatStack->RemoveChild(OldestBubble.Background);
		}
		ChatBubbles.RemoveAt(0);
	}
}

void UShowDownNameTagWidget::RefreshChatBubbleTimer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (ChatBubbles.IsEmpty())
	{
		World->GetTimerManager().ClearTimer(ChatBubbleAnimationTimerHandle);
		return;
	}

	if (!World->GetTimerManager().IsTimerActive(ChatBubbleAnimationTimerHandle))
	{
		World->GetTimerManager().SetTimer(
			ChatBubbleAnimationTimerHandle,
			this,
			&UShowDownNameTagWidget::UpdateChatBubbleAnimation,
			OverheadChatAnimationInterval,
			true);
	}
}
