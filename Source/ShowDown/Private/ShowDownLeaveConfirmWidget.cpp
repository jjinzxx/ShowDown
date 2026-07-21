#include "ShowDownLeaveConfirmWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "ShowDownPlayerController.h"
#include "Styling/CoreStyle.h"

void UShowDownLeaveConfirmWidget::SetOwningShowDownController(AShowDownPlayerController* InController)
{
	OwningShowDownController = InController;
}

void UShowDownLeaveConfirmWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildDefaultLayout();

	if (Button_Confirm)
	{
		Button_Confirm->OnClicked.AddUniqueDynamic(this, &UShowDownLeaveConfirmWidget::HandleConfirmClicked);
	}
	if (Button_Cancel)
	{
		Button_Cancel->OnClicked.AddUniqueDynamic(this, &UShowDownLeaveConfirmWidget::HandleCancelClicked);
	}
}

void UShowDownLeaveConfirmWidget::NativeDestruct()
{
	if (Button_Confirm)
	{
		Button_Confirm->OnClicked.RemoveDynamic(this, &UShowDownLeaveConfirmWidget::HandleConfirmClicked);
	}
	if (Button_Cancel)
	{
		Button_Cancel->OnClicked.RemoveDynamic(this, &UShowDownLeaveConfirmWidget::HandleCancelClicked);
	}

	Super::NativeDestruct();
}

void UShowDownLeaveConfirmWidget::BuildDefaultLayout()
{
	if (!WidgetTree || (WidgetTree->RootWidget && Button_Confirm && Button_Cancel))
	{
		return;
	}

	UCanvasPanel* CanvasRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = CanvasRoot;

	UBorder* PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ConfirmPanel"));
	PanelBorder->SetBrushColor(FLinearColor(0.015f, 0.015f, 0.015f, 0.96f));
	PanelBorder->SetPadding(FMargin(28.0f));
	if (UCanvasPanelSlot* PanelSlot = CanvasRoot->AddChildToCanvas(PanelBorder))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetSize(FVector2D(430.0f, 210.0f));
	}

	UVerticalBox* ContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ContentBox"));
	PanelBorder->SetContent(ContentBox);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Title"));
	Title->SetText(FText::FromString(TEXT("게임에서 나갈까요?")));
	Title->SetJustification(ETextJustify::Center);
	Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Title->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 25));
	if (UVerticalBoxSlot* ChildSlot = ContentBox->AddChildToVerticalBox(Title))
	{
		ChildSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));
	}

	UTextBlock* Description = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Description"));
	Description->SetText(FText::FromString(TEXT("진행 중인 게임은 포기 처리됩니다.")));
	Description->SetJustification(ETextJustify::Center);
	Description->SetColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.78f, 0.78f)));
	Description->SetAutoWrapText(true);
	if (UVerticalBoxSlot* ChildSlot = ContentBox->AddChildToVerticalBox(Description))
	{
		ChildSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 26.0f));
	}

	UHorizontalBox* ButtonBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ButtonBox"));
	if (UVerticalBoxSlot* ChildSlot = ContentBox->AddChildToVerticalBox(ButtonBox))
	{
		ChildSlot->SetHorizontalAlignment(HAlign_Center);
	}

	Button_Cancel = CreateDialogButton(TEXT("취소"), FLinearColor(0.18f, 0.18f, 0.18f, 1.0f));
	if (UHorizontalBoxSlot* ChildSlot = ButtonBox->AddChildToHorizontalBox(Button_Cancel))
	{
		ChildSlot->SetPadding(FMargin(4.0f));
	}

	Button_Confirm = CreateDialogButton(TEXT("나가기"), FLinearColor(0.55f, 0.08f, 0.06f, 1.0f));
	if (UHorizontalBoxSlot* ChildSlot = ButtonBox->AddChildToHorizontalBox(Button_Confirm))
	{
		ChildSlot->SetPadding(FMargin(4.0f));
	}
}

UButton* UShowDownLeaveConfirmWidget::CreateDialogButton(const FString& Label, const FLinearColor& Color)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Button->SetBackgroundColor(Color);
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetText(FText::FromString(Label));
	Text->SetJustification(ETextJustify::Center);
	Text->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Text->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 18));
	Button->SetContent(Text);
	return Button;
}

void UShowDownLeaveConfirmWidget::HandleConfirmClicked()
{
	if (OwningShowDownController)
	{
		OwningShowDownController->ConfirmLeaveMultiplayerMatch();
	}
}

void UShowDownLeaveConfirmWidget::HandleCancelClicked()
{
	if (OwningShowDownController)
	{
		OwningShowDownController->CancelLeaveMultiplayerMatch();
	}
}
