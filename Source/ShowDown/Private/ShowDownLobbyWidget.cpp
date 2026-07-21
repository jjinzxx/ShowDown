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
UObject* LobbyPretendardRegularFont()
{
	return LoadObject<UObject>(nullptr, TEXT("/Game/UI/Font/Pretendard/static/Pretendard-Regular_Font.Pretendard-Regular_Font"));
}
FSlateBrush LobbyFlatBlackBrush(float Alpha)
{
	FSlateBrush Brush; Brush.DrawAs = ESlateBrushDrawType::Box;
	Brush.TintColor = FSlateColor(FLinearColor(0, 0, 0, Alpha)); Brush.Margin = FMargin(0); return Brush;
}
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
		Button_Start->OnClicked.AddUniqueDynamic(this, &UShowDownLobbyWidget::HandleStartClicked);
	}

	if (Button_Leave)
	{
		Button_Leave->OnClicked.AddUniqueDynamic(this, &UShowDownLobbyWidget::HandleLeaveClicked);
	}
	if (Button_KickPlayer2)
	{
		Button_KickPlayer2->OnClicked.AddUniqueDynamic(this, &UShowDownLobbyWidget::HandleKickPlayer2Clicked);
	}
	if (Button_KickPlayer3)
	{
		Button_KickPlayer3->OnClicked.AddUniqueDynamic(this, &UShowDownLobbyWidget::HandleKickPlayer3Clicked);
	}
	if (Button_KickPlayer4)
	{
		Button_KickPlayer4->OnClicked.AddUniqueDynamic(this, &UShowDownLobbyWidget::HandleKickPlayer4Clicked);
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
	if (Button_KickPlayer2)
	{
		Button_KickPlayer2->OnClicked.RemoveDynamic(this, &UShowDownLobbyWidget::HandleKickPlayer2Clicked);
	}
	if (Button_KickPlayer3)
	{
		Button_KickPlayer3->OnClicked.RemoveDynamic(this, &UShowDownLobbyWidget::HandleKickPlayer3Clicked);
	}
	if (Button_KickPlayer4)
	{
		Button_KickPlayer4->OnClicked.RemoveDynamic(this, &UShowDownLobbyWidget::HandleKickPlayer4Clicked);
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

void UShowDownLobbyWidget::SetLobbyInfo(const FString& RoomName, const FString& RoomCode, bool bIsHost)
{
	CachedRoomName = RoomName;
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

void UShowDownLobbyWidget::SetInteractionPending(bool bPending)
{
	bInteractionPending = bPending;
	if (Button_Start)
	{
		Button_Start->SetIsEnabled(!bPending && HasMinimumPlayersToStart());
	}
	if (Button_Leave)
	{
		Button_Leave->SetIsEnabled(!bPending);
	}
	for (UButton* KickButton : {Button_KickPlayer2, Button_KickPlayer3, Button_KickPlayer4})
	{
		if (KickButton)
		{
			KickButton->SetIsEnabled(!bPending);
		}
	}
}

void UShowDownLobbyWidget::BuildDefaultLayout()
{
	if (!WidgetTree)
	{
		return;
	}

	// Preserve the layout authored in WBP_Lobby. The C++ tree below is only a
	// fallback for direct construction of the native widget class.
	if (WidgetTree->RootWidget)
	{
		return;
	}

	Text_Code = nullptr;
	Text_RoomName = nullptr;
	Text_Status = nullptr;
	Text_Players = nullptr;
	Text_ParticipantHeader = nullptr;
	Text_Player1 = nullptr;
	Text_Player2 = nullptr;
	Text_Player3 = nullptr;
	Text_Player4 = nullptr;
	Button_Start = nullptr;
	Button_Leave = nullptr;
	Button_KickPlayer2 = nullptr;
	Button_KickPlayer3 = nullptr;
	Button_KickPlayer4 = nullptr;

	UCanvasPanel* CanvasRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = CanvasRoot;

	UBorder* PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelBorder"));
	PanelBorder->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.68f));
	PanelBorder->SetPadding(FMargin(28.0f));

	UVerticalBox* RootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RootBox"));
	PanelBorder->SetContent(RootBox);

	if (UCanvasPanelSlot* PanelSlot = CanvasRoot->AddChildToCanvas(PanelBorder))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetAutoSize(false);
		PanelSlot->SetSize(FVector2D(620.0f, 560.0f));
		PanelSlot->SetPosition(FVector2D::ZeroVector);
	}

	Text_Code = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Code"));
	Text_Code->SetColorAndOpacity(FSlateColor(FLinearColor::Yellow));
	Text_Code->SetJustification(ETextJustify::Center);
	Text_Code->SetFont(FSlateFontInfo(LobbyPretendardRegularFont(), 24));

	if (UVerticalBoxSlot* CodeSlot = RootBox->AddChildToVerticalBox(Text_Code))
	{
		CodeSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));
	}

	Text_RoomName = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_RoomName"));
	Text_RoomName->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Text_RoomName->SetJustification(ETextJustify::Center);
	Text_RoomName->SetFont(FSlateFontInfo(LobbyPretendardRegularFont(), 20));
	if (UVerticalBoxSlot* NameSlot = RootBox->AddChildToVerticalBox(Text_RoomName)) NameSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	Text_Players = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Players"));
	Text_Players->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Text_Players->SetAutoWrapText(true);
	Text_Players->SetFont(FSlateFontInfo(LobbyPretendardRegularFont(), 16));
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
	Text_Status->SetFont(FSlateFontInfo(LobbyPretendardRegularFont(), 16));

	if (UVerticalBoxSlot* StatusSlot = RootBox->AddChildToVerticalBox(Text_Status))
	{
		StatusSlot->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 0.0f));
	}
}

UButton* UShowDownLobbyWidget::CreateMenuButton(const FString& Label)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	FButtonStyle Style; Style.SetNormal(LobbyFlatBlackBrush(0.72f)).SetHovered(LobbyFlatBlackBrush(0.78f)).SetPressed(LobbyFlatBlackBrush(0.84f)).SetDisabled(LobbyFlatBlackBrush(0.36f)); Button->SetStyle(Style);
	UTextBlock* ButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ButtonText->SetText(FText::FromString(Label));
	ButtonText->SetJustification(ETextJustify::Center);
	ButtonText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	ButtonText->SetFont(FSlateFontInfo(LobbyPretendardRegularFont(), 18));
	Button->SetContent(ButtonText);
	return Button;
}

void UShowDownLobbyWidget::RefreshLobbyText()
{
	if (Text_Code)
	{
		Text_Code->SetText(FText::FromString(FString::Printf(TEXT("방 코드: %s"), *CachedRoomCode)));
	}
	if (Text_RoomName)
	{
		Text_RoomName->SetText(FText::FromString(CachedRoomName.IsEmpty() ? TEXT("이름 없음") : CachedRoomName));
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
	if (!Text_Players && !Text_Player1 && !Text_Player2 && !Text_Player3 && !Text_Player4)
	{
		return;
	}

	const AShowDownGameStateBase* ShowDownGameState = GetWorld()
		? GetWorld()->GetGameState<AShowDownGameStateBase>()
		: nullptr;
	if (!ShowDownGameState)
	{
		if (Button_Start)
		{
			Button_Start->SetIsEnabled(false);
		}
		const int32 ExpectedPlayerCount = GetLobbyExpectedPlayerCount(GetGameInstance());
		const FString LoadingText = FString::Printf(TEXT("참가자 목록 (0/%d)"), ExpectedPlayerCount);
		if (Text_ParticipantHeader)
		{
			Text_ParticipantHeader->SetText(FText::FromString(LoadingText));
		}
		for (UTextBlock* PlayerText : {Text_Player1, Text_Player2, Text_Player3, Text_Player4})
		{
			if (PlayerText)
			{
				PlayerText->SetText(FText::FromString(TEXT("참가자 정보를 불러오는 중...")));
			}
		}
		for (UButton* KickButton : {Button_KickPlayer2, Button_KickPlayer3, Button_KickPlayer4})
		{
			if (KickButton)
			{
				KickButton->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		const FString LegacyLoadingText = LoadingText + TEXT("\n참가자 정보를 불러오는 중...");
		if (Text_Players && CachedParticipantText != LegacyLoadingText)
		{
			CachedParticipantText = LegacyLoadingText;
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
	if (Button_Start)
	{
		Button_Start->SetIsEnabled(!bInteractionPending && Slots.Num() >= 2);
	}
	if (Text_ParticipantHeader)
	{
		Text_ParticipantHeader->SetText(FText::FromString(
			FString::Printf(TEXT("참가자 목록 (%d/%d)"), Slots.Num(), ExpectedPlayerCount)));
	}

	auto FindSlot = [&Slots](EShowDownPlayerSlot PlayerSlotValue) -> const FShowDownNetworkPlayerSlot*
	{
		return Slots.FindByPredicate([PlayerSlotValue](const FShowDownNetworkPlayerSlot& Candidate)
		{
			return Candidate.Slot == PlayerSlotValue && Candidate.bConnected;
		});
	};
	auto RefreshRow = [this, &FindSlot](
		EShowDownPlayerSlot PlayerSlotValue,
		UTextBlock* PlayerText,
		UButton* KickButton)
	{
		const FShowDownNetworkPlayerSlot* PlayerSlot = FindSlot(PlayerSlotValue);
		if (PlayerText)
		{
			FString DisplayName = PlayerSlot && !PlayerSlot->DisplayName.IsEmpty()
				? PlayerSlot->DisplayName
				: TEXT("빈 자리");
			if (PlayerSlot && PlayerSlotValue == EShowDownPlayerSlot::Player1)
			{
				DisplayName += TEXT("  (방장)");
			}
			PlayerText->SetText(FText::FromString(DisplayName));
			PlayerText->SetColorAndOpacity(FSlateColor(PlayerSlot
				? FLinearColor::White
				: FLinearColor(0.55f, 0.55f, 0.55f, 1.0f)));
		}
		if (KickButton)
		{
			const bool bCanKick = bCachedIsHost && PlayerSlot && PlayerSlotValue != EShowDownPlayerSlot::Player1;
			KickButton->SetVisibility(bCanKick ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			KickButton->SetIsEnabled(bCanKick && !bInteractionPending);
		}
	};

	RefreshRow(EShowDownPlayerSlot::Player1, Text_Player1, nullptr);
	RefreshRow(EShowDownPlayerSlot::Player2, Text_Player2, Button_KickPlayer2);
	RefreshRow(EShowDownPlayerSlot::Player3, Text_Player3, Button_KickPlayer3);
	RefreshRow(EShowDownPlayerSlot::Player4, Text_Player4, Button_KickPlayer4);

	FString ParticipantText = FString::Printf(TEXT("참가자 목록 (%d/%d)"), Slots.Num(), ExpectedPlayerCount);
	for (const FShowDownNetworkPlayerSlot& PlayerSlot : Slots)
	{
		const FString DisplayName = PlayerSlot.DisplayName.IsEmpty() ? TEXT("이름 없는 플레이어") : PlayerSlot.DisplayName;
		ParticipantText += FString::Printf(
			TEXT("\n%d번  %s%s"),
			static_cast<int32>(PlayerSlot.Slot),
			*DisplayName,
			PlayerSlot.Slot == EShowDownPlayerSlot::Player1 ? TEXT(" (방장)") : TEXT(""));
	}

	if (Text_Players && CachedParticipantText != ParticipantText)
	{
		CachedParticipantText = ParticipantText;
		Text_Players->SetText(FText::FromString(CachedParticipantText));
	}
}

bool UShowDownLobbyWidget::HasMinimumPlayersToStart() const
{
	const AShowDownGameStateBase* ShowDownGameState = GetWorld()
		? GetWorld()->GetGameState<AShowDownGameStateBase>()
		: nullptr;
	return ShowDownGameState && ShowDownGameState->PlayerSlots.Num() >= 2;
}

void UShowDownLobbyWidget::HandleStartClicked()
{
	if (bInteractionPending)
	{
		return;
	}
	if (!HasMinimumPlayersToStart())
	{
		ShowStatusMessage(TEXT("게임을 시작하려면 최소 2명이 필요합니다."), FLinearColor::Red);
		return;
	}

	SetInteractionPending(true);
	ShowStatusMessage(TEXT("게임을 시작하는 중..."), FLinearColor::Yellow);
	OnStartRequested.Broadcast();
}

void UShowDownLobbyWidget::HandleLeaveClicked()
{
	if (bInteractionPending)
	{
		return;
	}

	SetInteractionPending(true);
	OnLeaveRequested.Broadcast();
}

void UShowDownLobbyWidget::RequestKickForSlot(EShowDownPlayerSlot PlayerSlotValue)
{
	if (!bCachedIsHost || bInteractionPending || PlayerSlotValue == EShowDownPlayerSlot::Player1)
	{
		return;
	}

	const AShowDownGameStateBase* ShowDownGameState = GetWorld()
		? GetWorld()->GetGameState<AShowDownGameStateBase>()
		: nullptr;
	if (!ShowDownGameState)
	{
		ShowStatusMessage(TEXT("참가자 정보를 확인할 수 없습니다."), FLinearColor::Red);
		return;
	}

	const FShowDownNetworkPlayerSlot* Target = ShowDownGameState->PlayerSlots.FindByPredicate(
		[PlayerSlotValue](const FShowDownNetworkPlayerSlot& Candidate)
		{
			return Candidate.Slot == PlayerSlotValue && Candidate.bConnected;
		});
	if (!Target || Target->PlayerId.IsEmpty())
	{
		ShowStatusMessage(TEXT("이미 나간 참가자입니다."), FLinearColor::Yellow);
		RefreshParticipantText();
		return;
	}

	ShowStatusMessage(
		FString::Printf(TEXT("%s 님을 방에서 내보내는 중..."), *Target->DisplayName),
		FLinearColor::Yellow);
	SetInteractionPending(true);
	OnKickRequested.Broadcast(Target->PlayerId);
}

void UShowDownLobbyWidget::HandleKickPlayer2Clicked()
{
	RequestKickForSlot(EShowDownPlayerSlot::Player2);
}

void UShowDownLobbyWidget::HandleKickPlayer3Clicked()
{
	RequestKickForSlot(EShowDownPlayerSlot::Player3);
}

void UShowDownLobbyWidget::HandleKickPlayer4Clicked()
{
	RequestKickForSlot(EShowDownPlayerSlot::Player4);
}
