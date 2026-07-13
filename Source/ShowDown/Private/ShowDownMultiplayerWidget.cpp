#include "ShowDownMultiplayerWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"

namespace
{
UObject* MultiplayerPretendardRegularFont()
{
	return LoadObject<UObject>(nullptr, TEXT("/Game/UI/Font/Pretendard/static/Pretendard-Regular_Font.Pretendard-Regular_Font"));
}
FSlateBrush MultiplayerFlatBlackBrush(float Alpha)
{
	FSlateBrush Brush; Brush.DrawAs = ESlateBrushDrawType::Box;
	Brush.TintColor = FSlateColor(FLinearColor(0, 0, 0, Alpha)); Brush.Margin = FMargin(0); return Brush;
}
FSlateBrush MultiplayerFlatBrush(const FLinearColor& Color)
{
	FSlateBrush Brush; Brush.DrawAs = ESlateBrushDrawType::Box;
	Brush.TintColor = FSlateColor(Color); Brush.Margin = FMargin(0); return Brush;
}
FButtonStyle MultiplayerJoinButtonStyle()
{
	FButtonStyle Style;
	Style.SetNormal(MultiplayerFlatBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.78f)));
	Style.SetHovered(MultiplayerFlatBrush(FLinearColor(0.13f, 0.13f, 0.13f, 0.92f)));
	Style.SetPressed(MultiplayerFlatBrush(FLinearColor(0.20f, 0.20f, 0.20f, 0.96f)));
	Style.SetDisabled(MultiplayerFlatBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.36f)));
	return Style;
}
const FLinearColor PanelColor(0.0f, 0.0f, 0.0f, 0.68f);
const FLinearColor SectionColor(0.0f, 0.0f, 0.0f, 0.58f);
const FLinearColor PrimaryButtonColor(0.0f, 0.0f, 0.0f, 0.82f);
const FLinearColor DarkButtonColor(0.0f, 0.0f, 0.0f, 0.72f);
const FLinearColor TextMutedColor(0.67f, 0.74f, 0.76f, 1.0f);
}

void UShowDownPublicRoomEntryWidget::SetRoomInfo(const FShowDownPublicRoomInfo& InRoomInfo)
{
	RoomInfo = InRoomInfo;
	RefreshRoomInfo();
}

void UShowDownPublicRoomEntryWidget::RefreshRoomInfo()
{
	if (Text_RoomName)
	{
		Text_RoomName->SetText(FText::FromString(RoomInfo.RoomName.IsEmpty() ? RoomInfo.RoomCode : RoomInfo.RoomName));
	}
	if (Text_RoomCode)
	{
		Text_RoomCode->SetText(FText::FromString(FString::Printf(TEXT("#%s"), *RoomInfo.RoomCode)));
	}
	if (Text_PlayerCount)
	{
		Text_PlayerCount->SetText(FText::FromString(FString::Printf(TEXT("플레이어: %d/%d"), RoomInfo.CurrentPlayers, RoomInfo.MaxPlayers)));
	}
}

TSharedRef<SWidget> UShowDownPublicRoomEntryWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		return Super::RebuildWidget();
	}

	UVerticalBox* RootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RoomEntryRoot"));
	WidgetTree->RootWidget = RootBox;

	UBorder* CardBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RoomEntryBorder"));
	CardBorder->SetBrushColor(SectionColor);
	CardBorder->SetPadding(FMargin(18.0f, 14.0f));
	if (UVerticalBoxSlot* CardSlot = RootBox->AddChildToVerticalBox(CardBorder))
	{
		CardSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
	}

	UVerticalBox* CardBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RoomEntryBox"));
	CardBorder->SetContent(CardBox);

	UHorizontalBox* HeaderBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RoomEntryHeader"));
	if (UVerticalBoxSlot* HeaderSlot = CardBox->AddChildToVerticalBox(HeaderBox))
	{
		HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
	}

	Text_RoomName = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_RoomName"));
	Text_RoomName->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Text_RoomName->SetFont(FSlateFontInfo(MultiplayerPretendardRegularFont(), 22));
	if (UHorizontalBoxSlot* NameSlot = HeaderBox->AddChildToHorizontalBox(Text_RoomName))
	{
		NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	Text_RoomCode = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_RoomCode"));
	Text_RoomCode->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.75f, 0.32f, 1.0f)));
	Text_RoomCode->SetFont(FSlateFontInfo(MultiplayerPretendardRegularFont(), 16));
	Text_RoomCode->SetJustification(ETextJustify::Right);
	if (UHorizontalBoxSlot* CodeSlot = HeaderBox->AddChildToHorizontalBox(Text_RoomCode))
	{
		CodeSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}

	Text_PlayerCount = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_PlayerCount"));
	Text_PlayerCount->SetColorAndOpacity(FSlateColor(TextMutedColor));
	Text_PlayerCount->SetFont(FSlateFontInfo(MultiplayerPretendardRegularFont(), 15));
	if (UVerticalBoxSlot* PlayerSlot = CardBox->AddChildToVerticalBox(Text_PlayerCount))
	{
		PlayerSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
	}

	Button_Join = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_JoinPublicRoom"));
	// Use fill-only brushes so the idle join button has no default Slate
	// outline. Hover/press are still obvious through the brighter fill.
	Button_Join->SetStyle(MultiplayerJoinButtonStyle());
	Button_Join->SetBackgroundColor(FLinearColor::White);
	UTextBlock* ButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_JoinPublicRoom"));
	ButtonText->SetText(FText::FromString(TEXT("로비 참가")));
	ButtonText->SetJustification(ETextJustify::Center);
	ButtonText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	ButtonText->SetFont(FSlateFontInfo(MultiplayerPretendardRegularFont(), 18));
	Button_Join->SetContent(ButtonText);
	if (UVerticalBoxSlot* ButtonSlot = CardBox->AddChildToVerticalBox(Button_Join))
	{
		ButtonSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
	}
	RefreshRoomInfo();

	return Super::RebuildWidget();
}

void UShowDownPublicRoomEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (Button_Join)
	{
		Button_Join->OnClicked.AddDynamic(this, &UShowDownPublicRoomEntryWidget::HandleJoinClicked);
	}
}

void UShowDownPublicRoomEntryWidget::NativeDestruct()
{
	if (Button_Join)
	{
		Button_Join->OnClicked.RemoveDynamic(this, &UShowDownPublicRoomEntryWidget::HandleJoinClicked);
	}
	Super::NativeDestruct();
}

void UShowDownPublicRoomEntryWidget::HandleJoinClicked()
{
	OnJoinRequested.Broadcast(RoomInfo.SearchResultIndex);
}

TSharedRef<SWidget> UShowDownMultiplayerWidget::RebuildWidget()
{
	BuildDefaultLayout();
	return Super::RebuildWidget();
}

void UShowDownMultiplayerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Host)
	{
		Button_Host->OnClicked.AddDynamic(this, &UShowDownMultiplayerWidget::HandleHostClicked);
	}

	if (Button_PrivateHost)
	{
		Button_PrivateHost->OnClicked.AddDynamic(this, &UShowDownMultiplayerWidget::HandlePrivateHostClicked);
	}

	if (Button_Join)
	{
		Button_Join->OnClicked.AddDynamic(this, &UShowDownMultiplayerWidget::HandleJoinClicked);
	}

	if (Button_RefreshRooms)
	{
		Button_RefreshRooms->OnClicked.AddDynamic(this, &UShowDownMultiplayerWidget::HandleRefreshRoomsClicked);
	}

	if (Button_Back)
	{
		Button_Back->OnClicked.AddDynamic(this, &UShowDownMultiplayerWidget::HandleBackClicked);
	}

	ShowStatusMessage(TEXT("공개방을 새로고침하거나 방 코드를 입력하세요."), FLinearColor::White);
	OnRefreshRoomsRequested.Broadcast();
}

void UShowDownMultiplayerWidget::NativeDestruct()
{
	if (Button_Host)
	{
		Button_Host->OnClicked.RemoveDynamic(this, &UShowDownMultiplayerWidget::HandleHostClicked);
	}

	if (Button_PrivateHost)
	{
		Button_PrivateHost->OnClicked.RemoveDynamic(this, &UShowDownMultiplayerWidget::HandlePrivateHostClicked);
	}

	if (Button_Join)
	{
		Button_Join->OnClicked.RemoveDynamic(this, &UShowDownMultiplayerWidget::HandleJoinClicked);
	}

	if (Button_RefreshRooms)
	{
		Button_RefreshRooms->OnClicked.RemoveDynamic(this, &UShowDownMultiplayerWidget::HandleRefreshRoomsClicked);
	}

	if (Button_Back)
	{
		Button_Back->OnClicked.RemoveDynamic(this, &UShowDownMultiplayerWidget::HandleBackClicked);
	}

	Super::NativeDestruct();
}

void UShowDownMultiplayerWidget::ShowStatusMessage(const FString& Message, const FLinearColor& Color)
{
	if (Text_Status)
	{
		Text_Status->SetText(FText::FromString(Message));
		Text_Status->SetColorAndOpacity(FSlateColor(Color));
	}
}

void UShowDownMultiplayerWidget::SetPublicRooms(const TArray<FShowDownPublicRoomInfo>& Rooms)
{
	if (!ScrollBox_PublicRooms)
	{
		return;
	}

	ScrollBox_PublicRooms->ClearChildren();
	PublicRoomEntries.Reset();

	if (Rooms.Num() == 0)
	{
		UTextBlock* EmptyText = CreateTextBlock(TEXT("참가 가능한 공개방이 없습니다."), 18, TextMutedColor, ETextJustify::Center);
		EmptyText->SetAutoWrapText(true);
		ScrollBox_PublicRooms->AddChild(EmptyText);
		return;
	}

	for (const FShowDownPublicRoomInfo& RoomInfo : Rooms)
	{
		UShowDownPublicRoomEntryWidget* EntryWidget = CreateWidget<UShowDownPublicRoomEntryWidget>(
			GetOwningPlayer(),
			UShowDownPublicRoomEntryWidget::StaticClass()
		);
		if (!EntryWidget)
		{
			continue;
		}

		EntryWidget->SetRoomInfo(RoomInfo);
		EntryWidget->OnJoinRequested.AddDynamic(this, &UShowDownMultiplayerWidget::HandlePublicRoomJoinRequested);
		PublicRoomEntries.Add(EntryWidget);
		ScrollBox_PublicRooms->AddChild(EntryWidget);
	}
}

void UShowDownMultiplayerWidget::BuildDefaultLayout()
{
	if (!WidgetTree)
	{
		return;
	}

	// A Widget Blueprint owns its authored tree. Do not replace it while the
	// Designer or runtime instance is rebuilding; only create the C++ fallback
	// when no authored root widget exists.
	if (WidgetTree->RootWidget)
	{
		return;
	}

	Text_Status = nullptr;
	Button_Host = nullptr;
	Button_PrivateHost = nullptr;
	Button_Join = nullptr;
	Button_RefreshRooms = nullptr;
	Button_Back = nullptr;
	EditableTextBox_RoomCode = nullptr;
	EditableTextBox_RoomName = nullptr;
	ScrollBox_PublicRooms = nullptr;

	UCanvasPanel* CanvasRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = CanvasRoot;

	UBorder* PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelBorder"));
	PanelBorder->SetBrushColor(PanelColor);
	PanelBorder->SetPadding(FMargin(24.0f));
	if (UCanvasPanelSlot* PanelSlot = CanvasRoot->AddChildToCanvas(PanelBorder))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetAutoSize(false);
		PanelSlot->SetSize(FVector2D(1320.0f, 740.0f));
		PanelSlot->SetPosition(FVector2D::ZeroVector);
	}

	UVerticalBox* RootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RootBox"));
	PanelBorder->SetContent(RootBox);

	UHorizontalBox* HeaderBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HeaderBox"));
	if (UVerticalBoxSlot* HeaderSlot = RootBox->AddChildToVerticalBox(HeaderBox))
	{
		HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));
	}

	UTextBlock* TitleText = CreateTextBlock(TEXT("MULTIPLAY"), 32, FLinearColor::White);
	if (UHorizontalBoxSlot* TitleSlot = HeaderBox->AddChildToHorizontalBox(TitleText))
	{
		TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	Button_Back = CreateMenuButton(TEXT("닫기"));
	SetButtonColor(Button_Back, FLinearColor(0.45f, 0.08f, 0.12f, 1.0f));
	if (UHorizontalBoxSlot* BackSlot = HeaderBox->AddChildToHorizontalBox(Button_Back))
	{
		BackSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}

	UHorizontalBox* BodyBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BodyBox"));
	if (UVerticalBoxSlot* BodySlot = RootBox->AddChildToVerticalBox(BodyBox))
	{
		BodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UBorder* PublicPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PublicRoomsPanel"));
	PublicPanel->SetBrushColor(SectionColor);
	PublicPanel->SetPadding(FMargin(18.0f));
	if (UHorizontalBoxSlot* PublicSlot = BodyBox->AddChildToHorizontalBox(PublicPanel))
	{
		PublicSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		PublicSlot->SetPadding(FMargin(0.0f, 0.0f, 14.0f, 0.0f));
	}

	UVerticalBox* PublicBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PublicRoomsBox"));
	PublicPanel->SetContent(PublicBox);

	UHorizontalBox* PublicHeader = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PublicRoomsHeader"));
	if (UVerticalBoxSlot* PublicHeaderSlot = PublicBox->AddChildToVerticalBox(PublicHeader))
	{
		PublicHeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));
	}

	UTextBlock* PublicTitle = CreateTextBlock(TEXT("공개 로비 목록"), 24, FLinearColor::White);
	if (UHorizontalBoxSlot* PublicTitleSlot = PublicHeader->AddChildToHorizontalBox(PublicTitle))
	{
		PublicTitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	Button_RefreshRooms = CreateMenuButton(TEXT("목록 새로고침"));
	SetButtonColor(Button_RefreshRooms, PrimaryButtonColor);
	if (UHorizontalBoxSlot* RefreshSlot = PublicHeader->AddChildToHorizontalBox(Button_RefreshRooms))
	{
		RefreshSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}

	ScrollBox_PublicRooms = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ScrollBox_PublicRooms"));
	if (UVerticalBoxSlot* RoomsSlot = PublicBox->AddChildToVerticalBox(ScrollBox_PublicRooms))
	{
		RoomsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UBorder* SidePanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SidePanel"));
	SidePanel->SetBrushColor(SectionColor);
	SidePanel->SetPadding(FMargin(18.0f));
	if (UHorizontalBoxSlot* SideSlot = BodyBox->AddChildToHorizontalBox(SidePanel))
	{
		SideSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		SideSlot->SetPadding(FMargin(14.0f, 0.0f, 0.0f, 0.0f));
	}

	UVerticalBox* SideBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SideBox"));
	SidePanel->SetContent(SideBox);

	UTextBlock* ModeTitle = CreateTextBlock(TEXT("접속 방식"), 22, FLinearColor::White);
	SideBox->AddChildToVerticalBox(ModeTitle);

	EditableTextBox_RoomName = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("EditableTextBox_RoomName"));
	EditableTextBox_RoomName->SetHintText(FText::FromString(TEXT("방 이름 입력")));
	FEditableTextBoxStyle NameFieldStyle = EditableTextBox_RoomName->WidgetStyle;
	NameFieldStyle.SetBackgroundImageNormal(MultiplayerFlatBlackBrush(0.62f)).SetBackgroundImageHovered(MultiplayerFlatBlackBrush(0.68f)).SetBackgroundImageFocused(MultiplayerFlatBlackBrush(0.72f)).SetBackgroundImageReadOnly(MultiplayerFlatBlackBrush(0.52f));
	EditableTextBox_RoomName->WidgetStyle = NameFieldStyle;
	if (UVerticalBoxSlot* NameSlot = SideBox->AddChildToVerticalBox(EditableTextBox_RoomName)) NameSlot->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 10.0f));

	Button_Host = CreateMenuButton(TEXT("공개방 만들기"));
	SetButtonColor(Button_Host, PrimaryButtonColor);
	if (UVerticalBoxSlot* HostSlot = SideBox->AddChildToVerticalBox(Button_Host))
	{
		HostSlot->SetPadding(FMargin(0.0f, 14.0f, 0.0f, 10.0f));
	}

	Button_PrivateHost = CreateMenuButton(TEXT("비공개방 만들기"));
	SetButtonColor(Button_PrivateHost, DarkButtonColor);
	if (UVerticalBoxSlot* PrivateHostSlot = SideBox->AddChildToVerticalBox(Button_PrivateHost))
	{
		PrivateHostSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
	}

	UTextBlock* DirectTitle = CreateTextBlock(TEXT("직접 연결"), 22, FLinearColor::White);
	if (UVerticalBoxSlot* DirectTitleSlot = SideBox->AddChildToVerticalBox(DirectTitle))
	{
		DirectTitleSlot->SetPadding(FMargin(0.0f, 18.0f, 0.0f, 8.0f));
	}

	UTextBlock* DirectDescription = CreateTextBlock(TEXT("친구가 공유한 6자리 방 코드를 입력하면 기존 방식 그대로 입장합니다."), 15, TextMutedColor);
	DirectDescription->SetAutoWrapText(true);
	if (UVerticalBoxSlot* DescriptionSlot = SideBox->AddChildToVerticalBox(DirectDescription))
	{
		DescriptionSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
	}

	EditableTextBox_RoomCode = WidgetTree->ConstructWidget<UEditableTextBox>(
		UEditableTextBox::StaticClass(),
		TEXT("EditableTextBox_RoomCode")
	);
	EditableTextBox_RoomCode->SetHintText(FText::FromString(TEXT("방 코드 입력")));
	FEditableTextBoxStyle FieldStyle = EditableTextBox_RoomCode->WidgetStyle;
	FieldStyle.SetBackgroundImageNormal(MultiplayerFlatBlackBrush(0.62f)).SetBackgroundImageHovered(MultiplayerFlatBlackBrush(0.68f)).SetBackgroundImageFocused(MultiplayerFlatBlackBrush(0.72f)).SetBackgroundImageReadOnly(MultiplayerFlatBlackBrush(0.52f));
	EditableTextBox_RoomCode->WidgetStyle = FieldStyle;
	if (UVerticalBoxSlot* CodeSlot = SideBox->AddChildToVerticalBox(EditableTextBox_RoomCode))
	{
		CodeSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
	}

	Button_Join = CreateMenuButton(TEXT("코드로 참가"));
	SetButtonColor(Button_Join, DarkButtonColor);
	if (UVerticalBoxSlot* JoinSlot = SideBox->AddChildToVerticalBox(Button_Join))
	{
		JoinSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));
	}

	UTextBlock* InfoTitle = CreateTextBlock(TEXT("서버 정보"), 22, FLinearColor::White);
	if (UVerticalBoxSlot* InfoTitleSlot = SideBox->AddChildToVerticalBox(InfoTitle))
	{
		InfoTitleSlot->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 8.0f));
	}

	UTextBlock* InfoText = CreateTextBlock(TEXT("포트: EOS P2P\n최대 인원: 4명\n공개방: 목록에서 참가\n비공개/친구방: 코드로 참가"), 15, TextMutedColor);
	InfoText->SetAutoWrapText(true);
	if (UVerticalBoxSlot* InfoSlot = SideBox->AddChildToVerticalBox(InfoText))
	{
		InfoSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	Text_Status = CreateTextBlock(TEXT(""), 16, FLinearColor::White, ETextJustify::Center);
	Text_Status->SetAutoWrapText(true);
	if (UVerticalBoxSlot* StatusSlot = RootBox->AddChildToVerticalBox(Text_Status))
	{
		StatusSlot->SetPadding(FMargin(0.0f, 16.0f, 0.0f, 0.0f));
	}
}

UButton* UShowDownMultiplayerWidget::CreateMenuButton(const FString& Label)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	FButtonStyle Style; Style.SetNormal(MultiplayerFlatBlackBrush(0.72f)).SetHovered(MultiplayerFlatBlackBrush(0.78f)).SetPressed(MultiplayerFlatBlackBrush(0.84f)).SetDisabled(MultiplayerFlatBlackBrush(0.36f)); Button->SetStyle(Style);
	UTextBlock* ButtonText = CreateTextBlock(Label, 18, FLinearColor::White, ETextJustify::Center);
	Button->SetContent(ButtonText);
	return Button;
}

UTextBlock* UShowDownMultiplayerWidget::CreateTextBlock(
	const FString& Text,
	int32 FontSize,
	const FLinearColor& Color,
	ETextJustify::Type Justification)
{
	UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TextBlock->SetText(FText::FromString(Text));
	TextBlock->SetColorAndOpacity(FSlateColor(Color));
	TextBlock->SetJustification(Justification);
	TextBlock->SetFont(FSlateFontInfo(MultiplayerPretendardRegularFont(), FontSize));
	return TextBlock;
}

void UShowDownMultiplayerWidget::SetButtonColor(UButton* Button, const FLinearColor& Color)
{
	if (Button)
	{
		Button->SetBackgroundColor(Color);
	}
}

void UShowDownMultiplayerWidget::HandleHostClicked()
{
	const FString RoomName = EditableTextBox_RoomName ? EditableTextBox_RoomName->GetText().ToString().TrimStartAndEnd() : TEXT("");
	if (RoomName.IsEmpty()) { ShowStatusMessage(TEXT("방 이름을 입력하세요."), FLinearColor::Red); return; }
	ShowStatusMessage(TEXT("공개방을 생성하는 중..."), FLinearColor::Yellow);
	OnHostRequested.Broadcast(RoomName);
}

void UShowDownMultiplayerWidget::HandlePrivateHostClicked()
{
	const FString RoomName = EditableTextBox_RoomName ? EditableTextBox_RoomName->GetText().ToString().TrimStartAndEnd() : TEXT("");
	if (RoomName.IsEmpty()) { ShowStatusMessage(TEXT("방 이름을 입력하세요."), FLinearColor::Red); return; }
	ShowStatusMessage(TEXT("비공개방을 생성하는 중..."), FLinearColor::Yellow);
	OnPrivateHostRequested.Broadcast(RoomName);
}

void UShowDownMultiplayerWidget::HandleJoinClicked()
{
	const FString RoomCode = EditableTextBox_RoomCode
		? EditableTextBox_RoomCode->GetText().ToString().TrimStartAndEnd()
		: TEXT("");

	if (RoomCode.IsEmpty())
	{
		ShowStatusMessage(TEXT("방 코드를 입력하세요."), FLinearColor::Red);
		return;
	}

	ShowStatusMessage(TEXT("방 코드로 검색 중..."), FLinearColor::Yellow);
	OnJoinRequested.Broadcast(RoomCode);
}

void UShowDownMultiplayerWidget::HandleRefreshRoomsClicked()
{
	ShowStatusMessage(TEXT("공개방 목록을 새로고침하는 중..."), FLinearColor::Yellow);
	OnRefreshRoomsRequested.Broadcast();
}

void UShowDownMultiplayerWidget::HandlePublicRoomJoinRequested(int32 SearchResultIndex)
{
	ShowStatusMessage(TEXT("공개방에 참가하는 중..."), FLinearColor::Yellow);
	OnJoinPublicRoomRequested.Broadcast(SearchResultIndex);
}

void UShowDownMultiplayerWidget::HandleBackClicked()
{
	OnBackRequested.Broadcast();
}
