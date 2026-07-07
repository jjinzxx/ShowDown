#include "ShowDownLobbyWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "ShowDownGameStateBase.h"
#include "Styling/CoreStyle.h"

namespace
{
int32 GetLobbyExpectedPlayerCount(const UGameInstance* GameInstance)
{
	return 4;
}
}

TSharedRef<SWidget> UShowDownLobbyWidget::RebuildWidget()
{
	BuildDefaultLayout();
	return Super::RebuildWidget();
}

void UShowDownLobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Start)
	{
		Button_Start->OnClicked.AddDynamic(this, &UShowDownLobbyWidget::HandleStartClicked);
	}

	if (Button_Leave)
	{
		Button_Leave->OnClicked.AddDynamic(this, &UShowDownLobbyWidget::HandleLeaveClicked);
	}

	RefreshLobbyText();
}

void UShowDownLobbyWidget::NativeDestruct()
{
	if (Button_Start)
	{
		Button_Start->OnClicked.RemoveDynamic(this, &UShowDownLobbyWidget::HandleStartClicked);
	}

	if (Button_Leave)
	{
		Button_Leave->OnClicked.RemoveDynamic(this, &UShowDownLobbyWidget::HandleLeaveClicked);
	}

	Super::NativeDestruct();
}

void UShowDownLobbyWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	ParticipantRefreshElapsed += InDeltaTime;
	if (ParticipantRefreshElapsed >= 0.25f)
	{
		ParticipantRefreshElapsed = 0.0f;
		RefreshParticipantText();
	}
}

void UShowDownLobbyWidget::SetLobbyInfo(const FString& RoomCode, bool bIsHost)
{
	CachedRoomCode = RoomCode;
	bCachedIsHost = bIsHost;
	ParticipantRefreshElapsed = 0.0f;
	RefreshLobbyText();
}

void UShowDownLobbyWidget::ShowStatusMessage(const FString& Message, const FLinearColor& Color)
{
	if (Text_Status)
	{
		Text_Status->SetText(FText::FromString(Message));
		Text_Status->SetColorAndOpacity(FSlateColor(Color));
	}
}

void UShowDownLobbyWidget::BuildDefaultLayout()
{
	if (!WidgetTree)
	{
		return;
	}

	if (WidgetTree->RootWidget && Text_Title && Text_Code && Text_Status && Text_Players && Button_Start && Button_Leave)
	{
		return;
	}

	Text_Title = nullptr;
	Text_Code = nullptr;
	Text_Status = nullptr;
	Text_Players = nullptr;
	Button_Start = nullptr;
	Button_Leave = nullptr;

	UCanvasPanel* CanvasRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = CanvasRoot;

	UBorder* PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelBorder"));
	PanelBorder->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.02f, 0.9f));
	PanelBorder->SetPadding(FMargin(28.0f));

	UVerticalBox* RootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RootBox"));
	PanelBorder->SetContent(RootBox);

	if (UCanvasPanelSlot* PanelSlot = CanvasRoot->AddChildToCanvas(PanelBorder))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetAutoSize(false);
		PanelSlot->SetSize(FVector2D(460.0f, 420.0f));
		PanelSlot->SetPosition(FVector2D::ZeroVector);
	}

	Text_Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Title"));
	Text_Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Text_Title->SetJustification(ETextJustify::Center);
	Text_Title->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 28));

	if (UVerticalBoxSlot* TitleSlot = RootBox->AddChildToVerticalBox(Text_Title))
	{
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));
	}

	Text_Code = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Code"));
	Text_Code->SetColorAndOpacity(FSlateColor(FLinearColor::Yellow));
	Text_Code->SetJustification(ETextJustify::Center);
	Text_Code->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 24));

	if (UVerticalBoxSlot* CodeSlot = RootBox->AddChildToVerticalBox(Text_Code))
	{
		CodeSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));
	}

	Text_Players = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Players"));
	Text_Players->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Text_Players->SetAutoWrapText(true);
	Text_Players->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 16));
	if (UVerticalBoxSlot* PlayersSlot = RootBox->AddChildToVerticalBox(Text_Players))
	{
		PlayersSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));
	}

	Button_Start = CreateMenuButton(TEXT("Start Game"));
	if (UVerticalBoxSlot* StartSlot = RootBox->AddChildToVerticalBox(Button_Start))
	{
		StartSlot->SetPadding(FMargin(0.0f, 4.0f));
	}

	Button_Leave = CreateMenuButton(TEXT("나가기"));
	if (UVerticalBoxSlot* LeaveSlot = RootBox->AddChildToVerticalBox(Button_Leave))
	{
		LeaveSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 16.0f));
	}

	Text_Status = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Status"));
	Text_Status->SetJustification(ETextJustify::Center);
	Text_Status->SetAutoWrapText(true);
	Text_Status->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 16));

	if (UVerticalBoxSlot* StatusSlot = RootBox->AddChildToVerticalBox(Text_Status))
	{
		StatusSlot->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 0.0f));
	}
}

UButton* UShowDownLobbyWidget::CreateMenuButton(const FString& Label)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	UTextBlock* ButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ButtonText->SetText(FText::FromString(Label));
	ButtonText->SetJustification(ETextJustify::Center);
	ButtonText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	ButtonText->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 18));
	Button->SetContent(ButtonText);
	return Button;
}

void UShowDownLobbyWidget::RefreshLobbyText()
{
	if (Text_Title)
	{
		Text_Title->SetText(FText::FromString(bCachedIsHost ? TEXT("Room Created") : TEXT("Waiting Room")));
	}

	if (Text_Code)
	{
		Text_Code->SetText(FText::FromString(FString::Printf(TEXT("Code: %s"), *CachedRoomCode)));
	}

	if (Button_Start)
	{
		Button_Start->SetVisibility(bCachedIsHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	ShowStatusMessage(
		bCachedIsHost ? TEXT("방 코드를 공유하고, 모두 입장하면 게임을 시작하세요.") : TEXT("방장이 게임을 시작할 때까지 기다리는 중입니다."),
		FLinearColor::White
	);
	RefreshParticipantText();
}

void UShowDownLobbyWidget::RefreshParticipantText()
{
	if (!Text_Players)
	{
		return;
	}

	const AShowDownGameStateBase* ShowDownGameState = GetWorld()
		? GetWorld()->GetGameState<AShowDownGameStateBase>()
		: nullptr;
	if (!ShowDownGameState)
	{
		const int32 ExpectedPlayerCount = GetLobbyExpectedPlayerCount(GetGameInstance());
		const FString LoadingText = FString::Printf(TEXT("참여자 (0/%d)\n참가자 정보를 불러오는 중..."), ExpectedPlayerCount);
		if (CachedParticipantText != LoadingText)
		{
			CachedParticipantText = LoadingText;
			Text_Players->SetText(FText::FromString(CachedParticipantText));
		}
		return;
	}

	TArray<FShowDownNetworkPlayerSlot> Slots = ShowDownGameState->PlayerSlots;
	Slots.Sort([](const FShowDownNetworkPlayerSlot& Left, const FShowDownNetworkPlayerSlot& Right)
	{
		return static_cast<uint8>(Left.Slot) < static_cast<uint8>(Right.Slot);
	});

	const int32 ExpectedPlayerCount = GetLobbyExpectedPlayerCount(GetGameInstance());
	FString ParticipantText = FString::Printf(TEXT("참여자 (%d/%d)"), Slots.Num(), ExpectedPlayerCount);
	for (const FShowDownNetworkPlayerSlot& PlayerSlot : Slots)
	{
		const FString DisplayName = PlayerSlot.DisplayName.IsEmpty() ? TEXT("이름 없는 플레이어") : PlayerSlot.DisplayName;
		ParticipantText += FString::Printf(TEXT("\n%d번  %s"), static_cast<int32>(PlayerSlot.Slot), *DisplayName);
	}

	if (CachedParticipantText != ParticipantText)
	{
		CachedParticipantText = ParticipantText;
		Text_Players->SetText(FText::FromString(CachedParticipantText));
	}
}

void UShowDownLobbyWidget::HandleStartClicked()
{
	ShowStatusMessage(TEXT("게임을 시작하는 중..."), FLinearColor::Yellow);
	OnStartRequested.Broadcast();
}

void UShowDownLobbyWidget::HandleLeaveClicked()
{
	OnLeaveRequested.Broadcast();
}
