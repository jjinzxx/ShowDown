#include "ShowDownBetStatusWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

namespace
{
	constexpr int32 DefaultMaxBulletDots = 6;
	constexpr float BetStatusPulseSeconds = 0.42f;

	FSlateFontInfo MakeDefaultFont(int32 Size)
	{
		return FSlateFontInfo(FCoreStyle::GetDefaultFont(), Size);
	}

	FText MakeCircleText(bool bFilled)
	{
		return FText::FromString(FString::Chr(static_cast<TCHAR>(bFilled ? 0x25CF : 0x25CB)));
	}
}

void UShowDownBetStatusWidget::SetBetStatus(
	const FText& DisplayName,
	const FText& StatusText,
	int32 FilledBulletCount,
	int32 MaxBulletCount,
	FLinearColor AccentColor)
{
	const int32 NewMaxBulletCount = FMath::Clamp(MaxBulletCount, 1, 12);
	const int32 NewFilledBulletCount = FMath::Clamp(FilledBulletCount, 0, NewMaxBulletCount);
	const bool bShouldPulse =
		NewFilledBulletCount != CachedFilledBulletCount
		|| !StatusText.ToString().Equals(CachedStatusText.ToString(), ESearchCase::CaseSensitive);
	const EShowDownBetStatusPulseMode NewPulseMode = ResolvePulseMode(StatusText.ToString());

	CachedDisplayName = DisplayName;
	CachedStatusText = StatusText;
	CachedFilledBulletCount = NewFilledBulletCount;
	CachedMaxBulletCount = NewMaxBulletCount;
	CachedAccentColor = AccentColor;
	if (bShouldPulse)
	{
		PulseMode = NewPulseMode;
		PulseRemainingSeconds = BetStatusPulseSeconds;
	}

	RefreshVisuals();
}

TSharedRef<SWidget> UShowDownBetStatusWidget::RebuildWidget()
{
	BuildDefaultWidget();
	return Super::RebuildWidget();
}

void UShowDownBetStatusWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BuildDefaultWidget();
	RefreshVisuals();
}

void UShowDownBetStatusWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (PulseRemainingSeconds <= 0.0f || !BulletRow)
	{
		return;
	}

	PulseRemainingSeconds = FMath::Max(0.0f, PulseRemainingSeconds - InDeltaTime);
	const float Alpha = PulseRemainingSeconds / BetStatusPulseSeconds;
	const float Progress = 1.0f - Alpha;
	const float Wave = FMath::Max(0.0f, FMath::Sin(Progress * PI * 3.0f));
	const float Strength = PulseMode == EShowDownBetStatusPulseMode::Raise
		? 0.18f
		: (PulseMode == EShowDownBetStatusPulseMode::Call ? 0.09f : 0.12f);
	const float Scale = PulseMode == EShowDownBetStatusPulseMode::Fold
		? 1.0f - Wave * 0.05f * Alpha
		: 1.0f + Wave * Strength * Alpha;
	const float Opacity = PulseMode == EShowDownBetStatusPulseMode::Fold
		? 0.62f + Wave * 0.28f * Alpha
		: 0.78f + Wave * 0.22f * Alpha;
	const float VerticalOffset = PulseMode == EShowDownBetStatusPulseMode::Raise
		? -4.0f * Wave * Alpha
		: (PulseMode == EShowDownBetStatusPulseMode::Fold ? 2.0f * Wave * Alpha : 0.0f);

	BulletRow->SetRenderTransform(FWidgetTransform(
		FVector2D(0.0f, VerticalOffset),
		FVector2D(Scale, Scale),
		FVector2D::ZeroVector,
		0.0f));
	BulletRow->SetRenderOpacity(Opacity);
	if (AccentBorder)
	{
		AccentBorder->SetRenderOpacity(0.72f + Wave * 0.28f * Alpha);
	}
	if (BackgroundBorder)
	{
		BackgroundBorder->SetRenderOpacity(PulseMode == EShowDownBetStatusPulseMode::Fold
			? 0.84f + Wave * 0.16f * Alpha
			: 1.0f);
	}

	if (PulseRemainingSeconds <= 0.0f)
	{
		BulletRow->SetRenderTransform(FWidgetTransform());
		BulletRow->SetRenderOpacity(1.0f);
		PulseMode = EShowDownBetStatusPulseMode::Generic;
		if (AccentBorder)
		{
			AccentBorder->SetRenderOpacity(1.0f);
		}
		if (BackgroundBorder)
		{
			BackgroundBorder->SetRenderOpacity(1.0f);
		}
	}
}

void UShowDownBetStatusWidget::BuildDefaultWidget()
{
	if (!WidgetTree || BackgroundBorder)
	{
		return;
	}

	USizeBox* RootSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BetStatusRoot"));
	RootSizeBox->SetMinDesiredWidth(226.0f);
	RootSizeBox->SetMinDesiredHeight(70.0f);

	BackgroundBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BetStatusBackground"));
	BackgroundBorder->SetPadding(FMargin(9.0f, 6.0f, 9.0f, 7.0f));
	BackgroundBorder->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.72f));
	BackgroundBorder->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	RootSizeBox->SetContent(BackgroundBorder);

	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BetStatusStack"));
	BackgroundBorder->SetContent(Stack);

	USizeBox* AccentSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BetStatusAccentBox"));
	AccentSizeBox->SetHeightOverride(3.0f);
	AccentBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BetStatusAccent"));
	AccentBorder->SetBrushColor(CachedAccentColor);
	AccentSizeBox->SetContent(AccentBorder);
	if (UVerticalBoxSlot* AccentSlot = Stack->AddChildToVerticalBox(AccentSizeBox))
	{
		AccentSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f));
	}

	UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BetStatusHeader"));
	if (UVerticalBoxSlot* HeaderSlot = Stack->AddChildToVerticalBox(HeaderRow))
	{
		HeaderSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BetStatusName"));
	NameText->SetFont(MakeDefaultFont(14));
	NameText->SetColorAndOpacity(FSlateColor(FLinearColor(0.96f, 0.98f, 1.0f, 1.0f)));
	NameText->SetShadowOffset(FVector2D(0.0f, 1.0f));
	NameText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
	if (UHorizontalBoxSlot* NameSlot = HeaderRow->AddChildToHorizontalBox(NameText))
	{
		NameSlot->SetHorizontalAlignment(HAlign_Left);
		NameSlot->SetVerticalAlignment(VAlign_Center);
		FSlateChildSize FillSize;
		FillSize.SizeRule = ESlateSizeRule::Fill;
		NameSlot->SetSize(FillSize);
	}

	StatusTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BetStatusText"));
	StatusTextBlock->SetFont(MakeDefaultFont(12));
	StatusTextBlock->SetColorAndOpacity(FSlateColor(CachedAccentColor));
	StatusTextBlock->SetJustification(ETextJustify::Right);
	StatusTextBlock->SetShadowOffset(FVector2D(0.0f, 1.0f));
	StatusTextBlock->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.58f));
	if (UHorizontalBoxSlot* StatusSlot = HeaderRow->AddChildToHorizontalBox(StatusTextBlock))
	{
		StatusSlot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
		StatusSlot->SetHorizontalAlignment(HAlign_Right);
		StatusSlot->SetVerticalAlignment(VAlign_Center);
	}

	BulletRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BetBulletDots"));
	BulletRow->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	if (UVerticalBoxSlot* BulletSlot = Stack->AddChildToVerticalBox(BulletRow))
	{
		BulletSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
		BulletSlot->SetHorizontalAlignment(HAlign_Center);
	}

	WidgetTree->RootWidget = RootSizeBox;
}

void UShowDownBetStatusWidget::RefreshVisuals()
{
	BuildDefaultWidget();
	if (!WidgetTree)
	{
		return;
	}

	if (BackgroundBorder)
	{
		BackgroundBorder->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.72f));
	}
	if (AccentBorder)
	{
		AccentBorder->SetBrushColor(CachedAccentColor);
	}
	if (NameText)
	{
		NameText->SetText(CachedDisplayName);
	}
	if (StatusTextBlock)
	{
		StatusTextBlock->SetText(CachedStatusText);
		StatusTextBlock->SetColorAndOpacity(FSlateColor(CachedAccentColor));
		StatusTextBlock->SetVisibility(CachedStatusText.IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::HitTestInvisible);
	}

	RefreshBulletDots();
}

void UShowDownBetStatusWidget::RefreshBulletDots()
{
	if (!WidgetTree || !BulletRow)
	{
		return;
	}

	if (BulletDotTexts.Num() != CachedMaxBulletCount)
	{
		BulletRow->ClearChildren();
		BulletDotTexts.Reset();
		for (int32 DotIndex = 0; DotIndex < CachedMaxBulletCount; ++DotIndex)
		{
			UTextBlock* DotText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			DotText->SetFont(MakeDefaultFont(18));
			DotText->SetShadowOffset(FVector2D(0.0f, 1.0f));
			DotText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.48f));
			DotText->SetJustification(ETextJustify::Center);
			if (UHorizontalBoxSlot* DotSlot = BulletRow->AddChildToHorizontalBox(DotText))
			{
				DotSlot->SetPadding(FMargin(2.0f, 0.0f));
				DotSlot->SetHorizontalAlignment(HAlign_Center);
				DotSlot->SetVerticalAlignment(VAlign_Center);
			}
			BulletDotTexts.Add(DotText);
		}
	}

	for (int32 DotIndex = 0; DotIndex < BulletDotTexts.Num(); ++DotIndex)
	{
		UTextBlock* DotText = BulletDotTexts[DotIndex];
		if (!DotText)
		{
			continue;
		}

		const bool bFilled = DotIndex < CachedFilledBulletCount;
		DotText->SetText(MakeCircleText(bFilled));
		DotText->SetColorAndOpacity(FSlateColor(bFilled
			? CachedAccentColor
			: FLinearColor(0.42f, 0.46f, 0.50f, 0.76f)));
	}
}

EShowDownBetStatusPulseMode UShowDownBetStatusWidget::ResolvePulseMode(const FString& StatusText) const
{
	if (StatusText.StartsWith(TEXT("FOLD"), ESearchCase::IgnoreCase))
	{
		return EShowDownBetStatusPulseMode::Fold;
	}
	if (StatusText.StartsWith(TEXT("RAISE"), ESearchCase::IgnoreCase))
	{
		return EShowDownBetStatusPulseMode::Raise;
	}
	if (StatusText.StartsWith(TEXT("CALL"), ESearchCase::IgnoreCase)
		|| StatusText.StartsWith(TEXT("CHECK"), ESearchCase::IgnoreCase))
	{
		return EShowDownBetStatusPulseMode::Call;
	}

	return EShowDownBetStatusPulseMode::Generic;
}
