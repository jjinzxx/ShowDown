#include "ShowDownMultiRankWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

namespace
{
UObject* PretendardRegularFont()
{
	return LoadObject<UObject>(nullptr, TEXT("/Game/UI/Font/Pretendard/static/Pretendard-Regular_Font.Pretendard-Regular_Font"));
}
}

void UShowDownMultiRankWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindButtons();
	RefreshRanking();
}

void UShowDownMultiRankWidget::NativeDestruct()
{
	if (Button_Restart)
	{
		Button_Restart->OnClicked.RemoveDynamic(this, &UShowDownMultiRankWidget::HandleRestartClicked);
	}
	if (Button_MainMenu)
	{
		Button_MainMenu->OnClicked.RemoveDynamic(this, &UShowDownMultiRankWidget::HandleMainMenuClicked);
	}

	Super::NativeDestruct();
}

TSharedRef<SWidget> UShowDownMultiRankWidget::RebuildWidget()
{
	if (!WidgetTree || !WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}
	return Super::RebuildWidget();
}

void UShowDownMultiRankWidget::SetRanking(const TArray<FString>& PlayerNames)
{
	CachedPlayerNames = PlayerNames;
	RefreshRanking();
}

void UShowDownMultiRankWidget::SetRestartStatus(const FString& StatusText)
{
	if (Text_RestartStatus)
	{
		Text_RestartStatus->SetText(FText::FromString(StatusText));
	}
}

void UShowDownMultiRankWidget::HandleRestartClicked()
{
	SetRestartStatus(TEXT("재시작 동의 완료. 다른 참가자를 기다리는 중..."));
	OnRestartRequested.Broadcast();
}

void UShowDownMultiRankWidget::HandleMainMenuClicked()
{
	OnMainMenuRequested.Broadcast();
}

void UShowDownMultiRankWidget::BuildDefaultLayout()
{
	if (!WidgetTree)
	{
		return;
	}

	if (WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* CanvasRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = CanvasRoot;

	UBorder* ScreenDim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ScreenDim"));
	ScreenDim->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.35f));
	if (UCanvasPanelSlot* DimSlot = CanvasRoot->AddChildToCanvas(ScreenDim))
	{
		DimSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		DimSlot->SetOffsets(FMargin(0.0f));
	}

	UBorder* PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelBorder"));
	PanelBorder->SetBrushColor(FLinearColor(0.55f, 0.55f, 0.55f, 0.72f));
	if (UCanvasPanelSlot* PanelSlot = CanvasRoot->AddChildToCanvas(PanelBorder))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetSize(FVector2D(1120.0f, 760.0f));
		PanelSlot->SetPosition(FVector2D::ZeroVector);
	}

	UVerticalBox* RootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RootBox"));
	PanelBorder->SetContent(RootBox);

	Text_Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Title"));
	Text_Title->SetText(FText::FromString(TEXT("결과 발표")));
	Text_Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Text_Title->SetJustification(ETextJustify::Center);
	Text_Title->SetFont(FSlateFontInfo(PretendardRegularFont(), 52));
	if (UVerticalBoxSlot* TitleSlot = RootBox->AddChildToVerticalBox(Text_Title))
	{
		TitleSlot->SetPadding(FMargin(0.0f, 58.0f, 0.0f, 68.0f));
		TitleSlot->SetHorizontalAlignment(HAlign_Fill);
		TitleSlot->SetVerticalAlignment(VAlign_Top);
	}

	VerticalBox_RankList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VerticalBox_RankList"));
	if (UVerticalBoxSlot* ListSlot = RootBox->AddChildToVerticalBox(VerticalBox_RankList))
	{
		ListSlot->SetPadding(FMargin(240.0f, 0.0f, 220.0f, 0.0f));
		ListSlot->SetHorizontalAlignment(HAlign_Fill);
		ListSlot->SetVerticalAlignment(VAlign_Top);
		ListSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	Text_RestartStatus = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_RestartStatus"));
	Text_RestartStatus->SetText(FText::GetEmpty());
	Text_RestartStatus->SetColorAndOpacity(FSlateColor(FLinearColor(0.0f, 1.0f, 0.18f, 1.0f)));
	Text_RestartStatus->SetJustification(ETextJustify::Center);
	Text_RestartStatus->SetFont(FSlateFontInfo(PretendardRegularFont(), 22));
	if (UVerticalBoxSlot* StatusSlot = RootBox->AddChildToVerticalBox(Text_RestartStatus))
	{
		StatusSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));
		StatusSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	UHorizontalBox* ButtonBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ButtonBox"));
	if (UVerticalBoxSlot* ButtonBoxSlot = RootBox->AddChildToVerticalBox(ButtonBox))
	{
		ButtonBoxSlot->SetPadding(FMargin(140.0f, 0.0f, 140.0f, 58.0f));
		ButtonBoxSlot->SetHorizontalAlignment(HAlign_Fill);
		ButtonBoxSlot->SetVerticalAlignment(VAlign_Bottom);
	}

	Button_Restart = CreateMenuButton(TEXT("Button_Restart"), TEXT("재시작"));
	if (UHorizontalBoxSlot* RestartSlot = ButtonBox->AddChildToHorizontalBox(Button_Restart))
	{
		RestartSlot->SetPadding(FMargin(0.0f, 0.0f, 120.0f, 0.0f));
		RestartSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		RestartSlot->SetHorizontalAlignment(HAlign_Fill);
		RestartSlot->SetVerticalAlignment(VAlign_Center);
	}

	Button_MainMenu = CreateMenuButton(TEXT("Button_MainMenu"), TEXT("메인 메뉴"));
	if (UHorizontalBoxSlot* MainMenuSlot = ButtonBox->AddChildToHorizontalBox(Button_MainMenu))
	{
		MainMenuSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		MainMenuSlot->SetHorizontalAlignment(HAlign_Fill);
		MainMenuSlot->SetVerticalAlignment(VAlign_Center);
	}

	BindButtons();
	RefreshRanking();
}

void UShowDownMultiRankWidget::RefreshRanking()
{
	if (!WidgetTree || !VerticalBox_RankList)
	{
		return;
	}

	VerticalBox_RankList->ClearChildren();

	for (int32 Index = 0; Index < CachedPlayerNames.Num(); ++Index)
	{
		const FString RankLabel = FString::Printf(TEXT("%d등"), Index + 1);
		const FString PlayerName = CachedPlayerNames[Index].IsEmpty() ? TEXT("Unknown") : CachedPlayerNames[Index];

		UHorizontalBox* RankRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		UTextBlock* RankLabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		RankLabelText->SetText(FText::FromString(RankLabel));
		RankLabelText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		RankLabelText->SetJustification(ETextJustify::Right);
		RankLabelText->SetFont(FSlateFontInfo(PretendardRegularFont(), 28));
		if (UHorizontalBoxSlot* LabelSlot = RankRow->AddChildToHorizontalBox(RankLabelText))
		{
			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			LabelSlot->SetHorizontalAlignment(HAlign_Fill);
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}

		UTextBlock* PlayerNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		PlayerNameText->SetText(FText::FromString(PlayerName));
		PlayerNameText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		PlayerNameText->SetJustification(ETextJustify::Left);
		PlayerNameText->SetFont(FSlateFontInfo(PretendardRegularFont(), 28));
		if (UHorizontalBoxSlot* NameSlot = RankRow->AddChildToHorizontalBox(PlayerNameText))
		{
			NameSlot->SetPadding(FMargin(42.0f, 0.0f, 0.0f, 0.0f));
			NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			NameSlot->SetHorizontalAlignment(HAlign_Fill);
			NameSlot->SetVerticalAlignment(VAlign_Center);
		}

		if (UVerticalBoxSlot* RankSlot = VerticalBox_RankList->AddChildToVerticalBox(RankRow))
		{
			RankSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 10.0f));
			RankSlot->SetHorizontalAlignment(HAlign_Fill);
		}
	}
}

void UShowDownMultiRankWidget::BindButtons()
{
	if (Button_Restart && !Button_Restart->OnClicked.IsAlreadyBound(this, &UShowDownMultiRankWidget::HandleRestartClicked))
	{
		Button_Restart->OnClicked.AddDynamic(this, &UShowDownMultiRankWidget::HandleRestartClicked);
	}
	if (Button_MainMenu && !Button_MainMenu->OnClicked.IsAlreadyBound(this, &UShowDownMultiRankWidget::HandleMainMenuClicked))
	{
		Button_MainMenu->OnClicked.AddDynamic(this, &UShowDownMultiRankWidget::HandleMainMenuClicked);
	}
}

UButton* UShowDownMultiRankWidget::CreateMenuButton(const FName& WidgetName, const FString& LabelText)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), WidgetName);
	Button->SetBackgroundColor(FLinearColor(0.82f, 0.82f, 0.82f, 1.0f));

	UTextBlock* ButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ButtonText->SetText(FText::FromString(LabelText));
	ButtonText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	ButtonText->SetJustification(ETextJustify::Center);
	ButtonText->SetFont(FSlateFontInfo(PretendardRegularFont(), 30));
	Button->SetContent(ButtonText);

	return Button;
}
