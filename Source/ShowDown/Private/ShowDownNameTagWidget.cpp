#include "ShowDownNameTagWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

void UShowDownNameTagWidget::SetDisplayName(const FText& NewDisplayName)
{
	CachedDisplayName = NewDisplayName;
	if (NameText)
	{
		NameText->SetText(CachedDisplayName);
	}
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
	SetStatusText(CachedStatusText);
	SetTurnActive(bCachedTurnActive);
}

void UShowDownNameTagWidget::BuildDefaultWidget()
{
	if (!WidgetTree || NameText)
	{
		return;
	}

	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("NameTagRoot"));

	NameBackground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("NameTagBackground"));
	NameBackground->SetPadding(FMargin(10.0f, 4.0f));

	NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NameText"));
	NameText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	NameText->SetJustification(ETextJustify::Center);
	NameText->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 18));
	NameText->SetShadowOffset(FVector2D(0.0f, 1.0f));
	NameText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
	StatusText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	StatusText->SetJustification(ETextJustify::Center);
	StatusText->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 15));
	StatusText->SetShadowOffset(FVector2D(0.0f, 1.0f));
	StatusText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));

	NameBackground->SetContent(NameText);

	if (UVerticalBoxSlot* NameSlot = Root->AddChildToVerticalBox(NameBackground))
	{
		NameSlot->SetHorizontalAlignment(HAlign_Center);
	}

	if (UVerticalBoxSlot* StatusSlot = Root->AddChildToVerticalBox(StatusText))
	{
		StatusSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 0.0f));
		StatusSlot->SetHorizontalAlignment(HAlign_Center);
	}

	WidgetTree->RootWidget = Root;
	RefreshNameBackgroundColor();
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
