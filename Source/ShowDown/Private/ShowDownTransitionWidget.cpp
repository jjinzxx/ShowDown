#include "ShowDownTransitionWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CircularThrobber.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace
{
UObject* TransitionPretendardRegularFont()
{
	return LoadObject<UObject>(nullptr, TEXT("/Game/UI/Font/Pretendard/static/Pretendard-Regular_Font.Pretendard-Regular_Font"));
}
}

void UShowDownTransitionWidget::SetTransitionText(const FString& Title, const FString& Detail)
{
	CachedTitle = Title.IsEmpty() ? TEXT("LOADING") : Title;
	CachedDetail = Detail.IsEmpty() ? TEXT("잠시만 기다려 주세요.") : Detail;
	AnimationElapsed = 0.0f;
	RefreshText();
}

void UShowDownTransitionWidget::Dismiss(float FadeOutDuration)
{
	bDismissing = true;
	DismissElapsed = 0.0f;
	DismissDuration = FMath::Max(0.01f, FadeOutDuration);
}

TSharedRef<SWidget> UShowDownTransitionWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		return Super::RebuildWidget();
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TransitionRoot"));
	WidgetTree->RootWidget = RootCanvas;

	UBorder* Dimmer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TransitionDimmer"));
	Dimmer->SetBrushColor(FLinearColor(0.008f, 0.012f, 0.016f, 0.94f));
	if (UCanvasPanelSlot* DimmerSlot = RootCanvas->AddChildToCanvas(Dimmer))
	{
		DimmerSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		DimmerSlot->SetOffsets(FMargin(0.0f));
	}

	UVerticalBox* ContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TransitionContent"));
	if (UCanvasPanelSlot* ContentSlot = RootCanvas->AddChildToCanvas(ContentBox))
	{
		ContentSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		ContentSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		ContentSlot->SetAutoSize(false);
		ContentSlot->SetSize(FVector2D(760.0f, 260.0f));
		ContentSlot->SetPosition(FVector2D::ZeroVector);
	}

	Text_Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_TransitionTitle"));
	Text_Title->SetJustification(ETextJustify::Center);
	Text_Title->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.78f, 0.34f, 1.0f)));
	Text_Title->SetFont(FSlateFontInfo(TransitionPretendardRegularFont(), 31));
	if (UVerticalBoxSlot* TitleSlot = ContentBox->AddChildToVerticalBox(Text_Title))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Fill);
		TitleSlot->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 26.0f));
	}

	UCircularThrobber* Throbber = WidgetTree->ConstructWidget<UCircularThrobber>(UCircularThrobber::StaticClass(), TEXT("TransitionThrobber"));
	Throbber->SetNumberOfPieces(12);
	Throbber->SetPeriod(0.7f);
	Throbber->SetRadius(22.0f);
	FSlateBrush ThrobberBrush = Throbber->GetImage();
	ThrobberBrush.TintColor = FSlateColor(FLinearColor(0.93f, 0.96f, 1.0f, 1.0f));
	Throbber->SetImage(ThrobberBrush);
	if (UVerticalBoxSlot* ThrobberSlot = ContentBox->AddChildToVerticalBox(Throbber))
	{
		ThrobberSlot->SetHorizontalAlignment(HAlign_Center);
		ThrobberSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 24.0f));
	}

	Text_Detail = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_TransitionDetail"));
	Text_Detail->SetJustification(ETextJustify::Center);
	Text_Detail->SetAutoWrapText(true);
	Text_Detail->SetColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.78f, 0.82f, 1.0f)));
	Text_Detail->SetFont(FSlateFontInfo(TransitionPretendardRegularFont(), 18));
	if (UVerticalBoxSlot* DetailSlot = ContentBox->AddChildToVerticalBox(Text_Detail))
	{
		DetailSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	RefreshText();
	return Super::RebuildWidget();
}

void UShowDownTransitionWidget::NativeConstruct()
{
	Super::NativeConstruct();
	AnimationElapsed = 0.0f;
	bDismissing = false;
	DismissElapsed = 0.0f;
	SetRenderOpacity(0.0f);
	SetVisibility(ESlateVisibility::Visible);
	RefreshText();
}

void UShowDownTransitionWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	AnimationElapsed += FMath::Max(0.0f, InDeltaTime);
	if (bDismissing)
	{
		DismissElapsed += FMath::Max(0.0f, InDeltaTime);
		const float FadeOutAlpha = FMath::Clamp(DismissElapsed / DismissDuration, 0.0f, 1.0f);
		SetRenderOpacity(1.0f - FMath::InterpEaseIn(0.0f, 1.0f, FadeOutAlpha, 2.0f));
		if (FadeOutAlpha >= 1.0f)
		{
			RemoveFromParent();
		}
		return;
	}

	const float FadeAlpha = FMath::Clamp(AnimationElapsed / 0.18f, 0.0f, 1.0f);
	SetRenderOpacity(FMath::InterpEaseOut(0.0f, 1.0f, FadeAlpha, 2.0f));

	if (Text_Detail)
	{
		const float Pulse = 0.78f + 0.22f * (0.5f + 0.5f * FMath::Sin(AnimationElapsed * 3.2f));
		Text_Detail->SetRenderOpacity(Pulse);
	}
}

void UShowDownTransitionWidget::RefreshText()
{
	if (Text_Title)
	{
		Text_Title->SetText(FText::FromString(CachedTitle));
	}

	if (Text_Detail)
	{
		Text_Detail->SetText(FText::FromString(CachedDetail));
	}
}
