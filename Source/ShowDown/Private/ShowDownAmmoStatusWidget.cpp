#include "ShowDownAmmoStatusWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

void UShowDownAmmoStatusWidget::SetAmmoStatus(
	const FText& NewText,
	int32 FontSize,
	const FLinearColor& TextColor,
	const FLinearColor& BackgroundColor)
{
	CachedText = NewText;
	CachedFontSize = FMath::Clamp(FontSize, 8, 160);
	CachedTextColor = TextColor;
	CachedBackgroundColor = BackgroundColor;
	BuildDefaultWidget();
	RefreshVisuals();
}

TSharedRef<SWidget> UShowDownAmmoStatusWidget::RebuildWidget()
{
	BuildDefaultWidget();
	return Super::RebuildWidget();
}

void UShowDownAmmoStatusWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildDefaultWidget();
	RefreshVisuals();
}

void UShowDownAmmoStatusWidget::BuildDefaultWidget()
{
	if (!WidgetTree || AmmoText)
	{
		return;
	}

	Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("AmmoStatusBackground"));
	Background->SetPadding(FMargin(18.0f, 7.0f));
	Background->SetVisibility(ESlateVisibility::HitTestInvisible);

	AmmoText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AmmoStatusText"));
	AmmoText->SetJustification(ETextJustify::Center);
	AmmoText->SetShadowOffset(FVector2D(0.0f, 2.0f));
	AmmoText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f));
	Background->SetContent(AmmoText);
	WidgetTree->RootWidget = Background;
	RefreshVisuals();
}

void UShowDownAmmoStatusWidget::RefreshVisuals()
{
	if (Background)
	{
		Background->SetBrushColor(CachedBackgroundColor);
	}
	if (AmmoText)
	{
		AmmoText->SetText(CachedText);
		AmmoText->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), CachedFontSize));
		AmmoText->SetColorAndOpacity(FSlateColor(CachedTextColor));
	}
}
