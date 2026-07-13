// Fill out your copyright notice in the Description page of Project Settings.

#include "ShowDownGameModeBase.h"

#include "Card.h"
#include "CardSystem.h"
#include "Collector.h"
#include "CollectorAISystem.h"
#include "BettingSystem.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "RoundResolver.h"
#include "RouletteSystem.h"
#include "SDCardPlacementAnchor.h"
#include "SDLLMSubsystem.h"
#include "SDMultiplayerSeatAnchor.h"
#include "ShowDownTypes.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneSequencePlaybackSettings.h"
#include "PlayerPawn.h"
#include "Presentation/SDCardRevealLayout.h"
#include "Presentation/SDBetActionPanelActor.h"
#include "Presentation/SDSelfShotGunActor.h"
#include "SDPlayerSeat.h"
#include "SDPlayerState.h"
#include "ShowDownCharacter.h"
#include "ShowDownEosSubsystem.h"
#include "ShowDownGameStateBase.h"
#include "ShowDownHubFlowManager.h"
#include "ShowDownPlayerController.h"
#include "SupabaseSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerState.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
	int32 GetSeatIndexFromPlayerSlot(EShowDownPlayerSlot Slot)
	{
		switch (Slot)
		{
		case EShowDownPlayerSlot::Player1: return 0;
		case EShowDownPlayerSlot::Player2: return 1;
		case EShowDownPlayerSlot::Player3: return 2;
		case EShowDownPlayerSlot::Player4: return 3;
		case EShowDownPlayerSlot::None:
		default: return INDEX_NONE;
		}
	}

	int32 GetMultiplayerTurnOrderIndex(EShowDownPlayerSlot Slot)
	{
		switch (Slot)
		{
		case EShowDownPlayerSlot::Player1: return 0;
		case EShowDownPlayerSlot::Player3: return 1;
		case EShowDownPlayerSlot::Player2: return 2;
		case EShowDownPlayerSlot::Player4: return 3;
		case EShowDownPlayerSlot::None:
		default: return MAX_int32;
		}
	}

	bool IsBeforeInMultiplayerTurnOrder(const ASDPlayerState* Left, const ASDPlayerState* Right)
	{
		const int32 LeftOrder = GetMultiplayerTurnOrderIndex(Left ? Left->ShowDownSlot : EShowDownPlayerSlot::None);
		const int32 RightOrder = GetMultiplayerTurnOrderIndex(Right ? Right->ShowDownSlot : EShowDownPlayerSlot::None);
		if (LeftOrder != RightOrder)
		{
			return LeftOrder < RightOrder;
		}

		const EShowDownPlayerSlot LeftSlot = Left ? Left->ShowDownSlot : EShowDownPlayerSlot::None;
		const EShowDownPlayerSlot RightSlot = Right ? Right->ShowDownSlot : EShowDownPlayerSlot::None;
		return static_cast<uint8>(LeftSlot) < static_cast<uint8>(RightSlot);
	}

	bool SortByMultiplayerTurnOrder(const ASDPlayerState& Left, const ASDPlayerState& Right)
	{
		return IsBeforeInMultiplayerTurnOrder(&Left, &Right);
	}

	FRotator BuildForeheadCardRotationOffsetFacingLocation(const USceneComponent* HeadSlot, const FVector& FocusLocation)
	{
		if (!HeadSlot)
		{
			return FRotator::ZeroRotator;
		}

		FVector FaceDirection = FocusLocation - HeadSlot->GetComponentLocation();
		FaceDirection.Z = 0.0f;
		if (FaceDirection.IsNearlyZero())
		{
			return FRotator::ZeroRotator;
		}

		FRotator DesiredRotation = HeadSlot->GetComponentRotation();
		DesiredRotation.Yaw = FaceDirection.Rotation().Yaw;

		FRotator RotationOffset =
			(HeadSlot->GetComponentQuat().Inverse() * DesiredRotation.Quaternion()).Rotator();
		RotationOffset.Normalize();
		return RotationOffset;
	}

	bool IsActiveNetworkPlayerController(const APlayerController* PlayerController)
	{
		return PlayerController
			&& PlayerController->Player != nullptr
			&& PlayerController->PlayerState != nullptr;
	}

	bool IsPlaceholderNetworkPlayerName(const FString& PlayerName)
	{
		const FString TrimmedName = PlayerName.TrimStartAndEnd();
		if (TrimmedName.IsEmpty())
		{
			return true;
		}

		if (TrimmedName.Equals(TEXT("Player"), ESearchCase::IgnoreCase))
		{
			return true;
		}

		if (TrimmedName.StartsWith(TEXT("Player "), ESearchCase::IgnoreCase))
		{
			FString Suffix = TrimmedName.RightChop(7).TrimStartAndEnd();
			return !Suffix.IsEmpty() && Suffix.IsNumeric();
		}

		return false;
	}

	FString GetNetworkPlayerDisplayName(const ASDPlayerState* PlayerState)
	{
		if (!PlayerState)
		{
			return FString();
		}

		const FString PlayerName = PlayerState->GetPlayerName().TrimStartAndEnd().Left(32);
		if (!IsPlaceholderNetworkPlayerName(PlayerName))
		{
			return PlayerName;
		}

		return TEXT("Connecting...");
	}

	bool StatusTextStartsWith(const FString& Text, const TCHAR* Prefix)
	{
		return Text.StartsWith(Prefix, ESearchCase::IgnoreCase);
	}

	struct FShowDownBetStatusLaneState
	{
		EShowDownPlayerSlot Slot = EShowDownPlayerSlot::None;
		FString DisplayName;
		int32 BulletCount = 0;
		bool bCurrentTurn = false;
		bool bNeedsToMatchBet = false;
		bool bFolded = false;
		bool bRouletteTarget = false;
		int32 RouletteBulletCount = 0;
		FString ActionText;
	};

	FLinearColor GetBetStatusAccentColor(const FShowDownBetStatusLaneState& LaneState)
	{
		const FString ActionText = LaneState.ActionText.TrimStartAndEnd();
		if (LaneState.bFolded || StatusTextStartsWith(ActionText, TEXT("FOLD")))
		{
			return FLinearColor(1.0f, 0.10f, 0.08f, 1.0f);
		}
		if (LaneState.bRouletteTarget)
		{
			return FLinearColor(1.0f, 0.16f, 0.08f, 1.0f);
		}
		if (LaneState.bCurrentTurn)
		{
			return FLinearColor(0.12f, 1.0f, 0.28f, 1.0f);
		}
		if (StatusTextStartsWith(ActionText, TEXT("CALL")))
		{
			return FLinearColor(0.05f, 0.88f, 1.0f, 1.0f);
		}
		if (StatusTextStartsWith(ActionText, TEXT("CHECK")))
		{
			return FLinearColor(1.0f, 0.86f, 0.12f, 1.0f);
		}
		if (StatusTextStartsWith(ActionText, TEXT("RAISE")))
		{
			return FLinearColor(1.0f, 0.42f, 0.04f, 1.0f);
		}

		return FLinearColor(0.82f, 0.86f, 0.92f, 1.0f);
	}

	FString GetBetStatusLabel(const FShowDownBetStatusLaneState& LaneState)
	{
		const FString ActionText = LaneState.ActionText.TrimStartAndEnd();
		if (LaneState.bRouletteTarget)
		{
			return TEXT("TARGET");
		}
		if (LaneState.bFolded)
		{
			return TEXT("FOLD");
		}
		if (LaneState.bCurrentTurn)
		{
			return TEXT("TURN");
		}
		if (!ActionText.IsEmpty())
		{
			return ActionText.Left(12);
		}

		return FString();
	}

	EShowDownPlayerSlot GetPlayerSlotByIndex(int32 SlotIndex)
	{
		static const EShowDownPlayerSlot AllSlots[] = {
			EShowDownPlayerSlot::Player1,
			EShowDownPlayerSlot::Player2,
			EShowDownPlayerSlot::Player3,
			EShowDownPlayerSlot::Player4
		};

		return AllSlots[FMath::Clamp(SlotIndex, 0, UE_ARRAY_COUNT(AllSlots) - 1)];
	}

	EShowDownSide GetMultiplayerLayoutSideForPlayerIndex(int32 PlayerIndex)
	{
		return PlayerIndex == 1 ? EShowDownSide::Collector : EShowDownSide::Player;
	}

	AShowDownCharacter* FindActiveCharacterForPlayerSlot(UWorld* World, EShowDownPlayerSlot Slot)
	{
		if (!World || Slot == EShowDownPlayerSlot::None)
		{
			return nullptr;
		}

		for (TActorIterator<AShowDownCharacter> It(World); It; ++It)
		{
			AShowDownCharacter* CandidateCharacter = *It;
			if (IsValid(CandidateCharacter)
				&& CandidateCharacter->IsCharacterSceneActive()
				&& CandidateCharacter->GetPlayerSlot() == Slot)
			{
				return CandidateCharacter;
			}
		}

		return nullptr;
	}

	bool TryGetMultiplayerPlacementRole(
		EShowDownPlayerSlot Slot,
		bool bForeheadSlot,
		ESDCardPlacementRole& OutRole)
	{
		switch (Slot)
		{
		case EShowDownPlayerSlot::Player1:
			OutRole = bForeheadSlot
				? ESDCardPlacementRole::PlayerForehead
				: ESDCardPlacementRole::PlayerHand;
			return true;
		case EShowDownPlayerSlot::Player2:
			OutRole = bForeheadSlot
				? ESDCardPlacementRole::OpponentForehead
				: ESDCardPlacementRole::OpponentHand;
			return true;
		case EShowDownPlayerSlot::Player3:
			OutRole = bForeheadSlot
				? ESDCardPlacementRole::Player3Forehead
				: ESDCardPlacementRole::Player3Hand;
			return true;
		case EShowDownPlayerSlot::Player4:
			OutRole = bForeheadSlot
				? ESDCardPlacementRole::Player4Forehead
				: ESDCardPlacementRole::Player4Hand;
			return true;
		case EShowDownPlayerSlot::None:
		default:
			return false;
		}
	}

	FVector ResolveSingleTableCenter(UWorld* World)
	{
		if (!World)
		{
			return FVector(-5457.0f, -670.0f, 324.0f);
		}

		TArray<AActor*> AnchorActors;
		UGameplayStatics::GetAllActorsOfClass(World, ASDCardPlacementAnchor::StaticClass(), AnchorActors);

		FVector HandAnchorSum = FVector::ZeroVector;
		int32 HandAnchorCount = 0;
		FVector AnyAnchorSum = FVector::ZeroVector;
		int32 AnyAnchorCount = 0;
		for (AActor* AnchorActor : AnchorActors)
		{
			const ASDCardPlacementAnchor* Anchor = Cast<ASDCardPlacementAnchor>(AnchorActor);
			if (!Anchor)
			{
				continue;
			}

			const FVector AnchorLocation = Anchor->GetActorLocation();
			AnyAnchorSum += AnchorLocation;
			++AnyAnchorCount;
			if (Anchor->IsHandAnchor())
			{
				HandAnchorSum += AnchorLocation;
				++HandAnchorCount;
			}
		}

		if (HandAnchorCount > 0)
		{
			return HandAnchorSum / static_cast<float>(HandAnchorCount);
		}

		if (AnyAnchorCount > 0)
		{
			FVector Center = AnyAnchorSum / static_cast<float>(AnyAnchorCount);
			Center.Z -= 30.0f;
			return Center;
		}

		return FVector(-5457.0f, -670.0f, 324.0f);
	}

	FTransform BuildSingleTableSeatTransform(const FVector& TableCenter, int32 SeatIndex)
	{
		static const FVector SeatOffsets[] = {
			FVector(-85.0f, 0.0f, 45.0f),
			FVector(85.0f, 0.0f, 45.0f),
			FVector(0.0f, -200.0f, 45.0f),
			FVector(0.0f, 200.0f, 45.0f)
		};
		static const float SeatYaws[] = { 0.0f, 180.0f, 90.0f, -90.0f };

		const int32 ClampedSeatIndex = FMath::Clamp(SeatIndex, 0, UE_ARRAY_COUNT(SeatOffsets) - 1);
		return FTransform(
			FRotator(0.0f, SeatYaws[ClampedSeatIndex], 0.0f),
			TableCenter + SeatOffsets[ClampedSeatIndex]);
	}
}

AShowDownGameModeBase::AShowDownGameModeBase()
{
	// GameState를 AShowDownGameStateBase로 고정합니다.
	// 이게 없으면 기본 AGameStateBase가 생성되어 GetGameState<AShowDownGameStateBase>()가
	// 항상 null이 되고, OnGameOver를 포함한 모든 GameState 이벤트가 broadcast되지 않습니다.
	GameStateClass = AShowDownGameStateBase::StaticClass();
	PlayerStateClass = ASDPlayerState::StaticClass();
	PlayerControllerClass = AShowDownPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	bUseSeamlessTravel = true;
	CardClass = ACard::StaticClass();

	CardSystem = CreateDefaultSubobject<UCardSystem>(TEXT("CardSystem"));
	CollectorAISystem = CreateDefaultSubobject<UCollectorAISystem>(TEXT("CollectorAISystem"));
	BettingSystem = CreateDefaultSubobject<UBettingSystem>(TEXT("BettingSystem"));
	RoundResolver = CreateDefaultSubobject<URoundResolver>(TEXT("RoundResolver"));
	RouletteSystem = CreateDefaultSubobject<URouletteSystem>(TEXT("RouletteSystem"));

	FShowDownStageRule Stage1;
	Stage1.StartingLives = 3;
	Stage1.MinimumBet = 1;
	Stage1.CollectorBluffRate = 0.15f;
	Stage1.CollectorAggression = 0.5f;
	Stage1.bSevenFoldLoadsSix = true;

	FShowDownStageRule Stage2;
	Stage2.StartingLives = 3;
	Stage2.MinimumBet = 1;
	Stage2.CollectorBluffRate = 0.25f;
	Stage2.CollectorAggression = 0.55f;
	Stage2.bSevenFoldLoadsSix = true;

	FShowDownStageRule Stage3;
	Stage3.StartingLives = 3;
	Stage3.MinimumBet = 2;
	Stage3.CollectorBluffRate = 0.3f;
	Stage3.CollectorAggression = 0.75f;
	Stage3.bSevenFoldLoadsSix = true;

	StageRules = { Stage1, Stage2, Stage3 };
}

UClass* AShowDownGameModeBase::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	// Maps can keep their authored single-player pawn, but every network player
	// must receive a replicated pawn when joining a listen-server match.
	if (GetWorld() && GetWorld()->GetNetMode() != NM_Standalone)
	{
		return APlayerPawn::StaticClass();
	}

	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

void AShowDownGameModeBase::BeginPlay()
{
	Super::BeginPlay();

	bool bInMultiplayerLobby = false;
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UShowDownEosSubsystem* EosSubsystem = GameInstance->GetSubsystem<UShowDownEosSubsystem>())
		{
			bInMultiplayerLobby = EosSubsystem->IsInMultiplayerLobby();
		}
	}

	const bool bNetworkedMatch = GetNetMode() != NM_Standalone;

	// 레벨에 HubFlowManager가 있으면 싱글플레이 버튼을 누를 때까지 시작을 미룹니다.
	// 허브가 없는 테스트 레벨(ShowDown_Test 등)에서는 기존처럼 곧장 시작합니다.
	const bool bHubControlsStart =
		UGameplayStatics::GetActorOfClass(GetWorld(), AShowDownHubFlowManager::StaticClass()) != nullptr
		&& (!bNetworkedMatch || bInMultiplayerLobby);

	if (bNetworkedMatch && !bInMultiplayerLobby)
	{
		StartMultiplayerGame();
		return;
	}

	if (bAutoStartOnBeginPlay && !bHubControlsStart)
	{
		StartSinglePlayer();
	}
}

void AShowDownGameModeBase::StartSinglePlayer()
{
	if (bSinglePlayerMatchStarted)
	{
		return;
	}
	bSinglePlayerMatchStarted = true;
	ClearInitialCardDealPresentation();
	bInitialCardDealPresentationPlayed = false;
	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->SetMatchMode(EShowDownMatchMode::SinglePlayer);
	}

	ConfigureSinglePlayerCharacters();
	FindCollector();
	PlaySinglePlayerIntroThenStartStage();
}

void AShowDownGameModeBase::StartMultiplayerGame()
{
	if (bMultiplayerMatchStarted)
	{
		return;
	}

	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->SetMatchMode(EShowDownMatchMode::Multiplayer);
	}

	ConfigureMultiplayerCharacters(TArray<ASDPlayerState*>());
	FindCollector();
	RefreshNetworkPlayerSlots();
	GetWorldTimerManager().ClearTimer(MultiplayerStartTimerHandle);
	GetWorldTimerManager().SetTimer(
		MultiplayerStartTimerHandle,
		this,
		&AShowDownGameModeBase::TryStartMultiplayerMatch,
		0.5f,
		false);

	UE_LOG(LogTemp, Log, TEXT("Multiplayer game structure initialized."));
}

void AShowDownGameModeBase::RefreshMultiplayerLobbyPlayers()
{
	if (HasAuthority())
	{
		RefreshNetworkPlayerSlots();
	}
}

void AShowDownGameModeBase::SetMultiplayerVoiceTalking(AController* RequestingController, bool bIsTalking)
{
	if (!HasAuthority() || GetNetMode() == NM_Standalone || !RequestingController)
	{
		return;
	}

	const ASDPlayerState* TalkingPlayer = RequestingController->GetPlayerState<ASDPlayerState>();
	if (!TalkingPlayer || TalkingPlayer->ShowDownSlot == EShowDownPlayerSlot::None)
	{
		return;
	}

	for (AShowDownCharacter* Character : GetShowDownCharacters())
	{
		if (IsValid(Character) && Character->GetPlayerSlot() == TalkingPlayer->ShowDownSlot)
		{
			Character->SetVoiceTalking(bIsTalking);
			break;
		}
	}
}

void AShowDownGameModeBase::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (bMultiplayerMatchStarted)
	{
		if (AShowDownPlayerController* ShowDownController = Cast<AShowDownPlayerController>(NewPlayer))
		{
			ShowDownController->ClientShowStatusMessage(TEXT("게임이 이미 시작되어 입장할 수 없습니다."));
		}
		NewPlayer->ClientTravel(TEXT("/Game/Maps/L_ShowdownMain"), TRAVEL_Absolute);
		return;
	}

	if (GetNetMode() != NM_Standalone)
	{
		if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
		{
			ShowDownGameState->SetMatchMode(EShowDownMatchMode::Multiplayer);
		}
	}

	if (ASDPlayerState* ShowDownPlayerState = NewPlayer ? NewPlayer->GetPlayerState<ASDPlayerState>() : nullptr)
	{
		if (ShowDownPlayerState->ShowDownSlot == EShowDownPlayerSlot::None)
		{
			ShowDownPlayerState->SetShowDownSlot(FindNextOpenPlayerSlot());
		}
		if (ShowDownPlayerState->ShowDownSlot == EShowDownPlayerSlot::None)
		{
			if (AShowDownPlayerController* ShowDownController = Cast<AShowDownPlayerController>(NewPlayer))
			{
				ShowDownController->ClientShowStatusMessage(TEXT("방이 가득 찼습니다. 최대 4명까지 입장할 수 있습니다."));
			}
			NewPlayer->ClientTravel(TEXT("/Game/Maps/L_ShowdownMain"), TRAVEL_Absolute);
			return;
		}
		ShowDownPlayerState->SetHostPlayer(ShowDownPlayerState->ShowDownSlot == EShowDownPlayerSlot::Player1);
	}

	RefreshNetworkPlayerSlots();
}

void AShowDownGameModeBase::Logout(AController* Exiting)
{
	if (ASDPlayerState* ShowDownPlayerState = Exiting ? Exiting->GetPlayerState<ASDPlayerState>() : nullptr)
	{
		HandleMultiplayerPlayerDisconnected(ShowDownPlayerState);
		if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
		{
			ShowDownGameState->ClearPlayerSlot(ShowDownPlayerState->ShowDownSlot);
		}
	}

	Super::Logout(Exiting);
	RefreshNetworkPlayerSlots();
}

void AShowDownGameModeBase::ResetForHubReturn()
{
	// This GameMode stays alive while the hub UI replaces the table flow. Stop
	// every callback bound to it so a delayed reveal, roulette, or AI retry from
	// the previous game cannot mutate the freshly reset board.
	GetWorldTimerManager().ClearAllTimersForObject(this);
	ClearMultiplayerRoundTimers();
	ClearCardRevealPresentationTimers();
	ClearInitialCardDealPresentation();
	bInitialCardDealPresentationPlayed = false;
	bSinglePlayerMatchStarted = false;
	ClearBetBulletPresentation();
	if (ActiveSelfShotGunActor)
	{
		ActiveSelfShotGunActor->OnGunPresentationFinished.RemoveDynamic(
			this,
			&AShowDownGameModeBase::HandleSelfShotGunPresentationFinished);
		ActiveSelfShotGunActor->OnGunFired.RemoveDynamic(
			this,
			&AShowDownGameModeBase::HandleSelfShotGunShotResolved);
		ActiveSelfShotGunActor->OnGunEmptyFired.RemoveDynamic(
			this,
			&AShowDownGameModeBase::HandleSelfShotGunShotResolved);
	}

	bBettingPhase = false;
	bHasPendingRoundReveal = false;
	bHasPendingFoldReveal = false;
	bCollectorActionPresentationInProgress = false;
	bSelfShotGunPresentationInProgress = false;
	bPlayerHasActedInBetting = false;
	bCollectorHasActedInBetting = false;
	bCollectorBetDecisionInProgress = false;
	bBossChatReplyInFlight = false;
	bHasPendingBossChatReply = false;
	PendingBossChatReplyDialogue.Empty();
	bPendingSelfShotRouletteResult = false;
	CardPlacementDelayContinuation = TFunction<void()>();
	CollectorActionPresentationContinuation = TFunction<void()>();
	SelfShotGunResultContinuation = TFunction<void()>();
	SelfShotGunPresentationContinuation = TFunction<void()>();
	ClearPendingMultiplayerGunResult();
	QueuedCollectorActionPresentationContinuations.Reset();
	ActiveSelfShotGunActor = nullptr;

	ClearForeheadCards();
	ClearHandCards();

	UE_LOG(LogTemp, Log, TEXT("Board reset for hub return."));
}

void AShowDownGameModeBase::PlayerSelectedCard(ACard* SelectedCard)
{
	PlayerSelectedCardFromController(nullptr, SelectedCard);
}

void AShowDownGameModeBase::PlayerSelectedCardFromController(AController* SubmittingController, ACard* SelectedCard)
{
	if (GetNetMode() != NM_Standalone)
	{
		if (bMultiplayerMatchStarted)
		{
			HandleMultiplayerSelectedCard(GetPlayerStateForController(SubmittingController), SelectedCard);
		}
		return;
	}

	if (bInitialCardDealPresentationInProgress
		|| bCollectorActionPresentationInProgress
		|| bCollectorBetDecisionInProgress)
	{
		return;
	}

	if (!IsValid(SelectedCard))
	{
		return;
	}

	if (!CardSystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("CardSystem is missing on %s."), *GetName());
		return;
	}

	if (!SelectedCard->IsCardSelectable() || !PlayerState.HandCards.Contains(SelectedCard))
	{
		UE_LOG(LogTemp, Warning, TEXT("Rejected selected card %s because it is not in the player's selectable hand."),
			*SelectedCard->GetName());
		return;
	}

	if (CollectorState.ForeheadCard)
	{
		UE_LOG(LogTemp, Warning, TEXT("Collector already has a forehead card."));
		return;
	}

	USceneComponent* CollectorHeadSlot = GetHeadSlotForSide(EShowDownSide::Collector);
	if (!CollectorHeadSlot)
	{
		UE_LOG(LogTemp, Warning, TEXT("Opponent forehead slot is missing. Add an SDCardPlacementAnchor with PlacementRole=OpponentForehead, or keep an old fallback slot."));
		return;
	}

	CurrentRoundPlayerGaveRank = SelectedCard->Rank;
	RecordCurrentRoundAction(FString::Printf(TEXT("Player gave Collector forehead card rank %d."), CurrentRoundPlayerGaveRank));
	UE_LOG(LogTemp, Log, TEXT("GameMode received selected card: %s"), *SelectedCard->GetName());
	BroadcastCardSelectedAction(EShowDownSide::Player);

	CardSystem->RemoveCardFromHand(PlayerState.HandCards, SelectedCard);
	ReflowHandCards(EShowDownSide::Player);
	//콜렉터의 이마로 카드 이동
	CollectorState.ForeheadCard = SelectedCard;

	CardSystem->MoveCardToSlotWithRotationOffset(
		SelectedCard,
		CollectorHeadSlot,
		true,
		GetForeheadCardRotationOffsetForSide(EShowDownSide::Collector, CollectorHeadSlot));

	WaitForCardPlacementThen(SelectedCard, [this]()
	{
		PlayCollectorActionPresentationThen([this]()
		{
			if (PlayerState.ForeheadCard)
			{
				StartBettingPhase();
			}
			else
			{
				CollectorGiveCardToPlayer();
			}
		});
	});
}

void AShowDownGameModeBase::DealInitialHand()
{
	if (!CardClass)
	{
		CardClass = ACard::StaticClass();
	}

	if (!CardClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("CardClass is not assigned on %s."), *GetName());
		return;
	}

	if (!CardSystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("CardSystem is missing on %s."), *GetName());
		return;
	}

	USceneComponent* PlayerHandSlot = GetHandSlotForSide(EShowDownSide::Player);
	if (!PlayerHandSlot)
	{
		UE_LOG(LogTemp, Warning, TEXT("Player hand slot is missing. Add an SDCardPlacementAnchor with PlacementRole=PlayerHand, or keep an old fallback slot."));
		return;
	}

	USceneComponent* CollectorHandSlot = GetHandSlotForSide(EShowDownSide::Collector);
	if (!CollectorHandSlot)
	{
		UE_LOG(LogTemp, Warning, TEXT("Opponent hand slot is missing. Add an SDCardPlacementAnchor with PlacementRole=OpponentHand, or keep an old fallback slot."));
		return;
	}

	const FSDCardHandLayoutSettings PlayerHandLayout = ResolveHandLayoutSettings(EShowDownSide::Player);
	const FSDCardHandLayoutSettings CollectorHandLayout = ResolveHandLayoutSettings(EShowDownSide::Collector);

	ClearHandCards();
	CardSystem->ResetDeck(2);
	CardSystem->ShuffleDeck();
	ActiveCardDeckCopies = 2;
	InitialCardDealDeckCopies = 2;

	TArray<int32> PlayerRanks;
	if (!CardSystem->DealCards(HandCount, PlayerRanks))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to deal player hand."));
		return;
	}

	TArray<int32> CollectorRanks;
	if (!CardSystem->DealCards(HandCount, CollectorRanks))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to deal collector hand."));
		return;
	}

	CardSystem->SpawnHandCards(
		this,
		CardClass,
		PlayerHandSlot,
		PlayerRanks,
		PlayerHandLayout,
		true,
		true,
		PlayerState.HandCards);

	ApplyCardMotionForSide(EShowDownSide::Player, PlayerState.HandCards);

	// 확인용으로 true. 나중에는 false로 바꾸면 콜렉터 손패가 뒷면이 됩니다.
	CardSystem->SpawnHandCards(
		this,
		CardClass,
		CollectorHandSlot,
		CollectorRanks,
		CollectorHandLayout,
		true,
		false,
		CollectorState.HandCards);
	ApplyCardMotionForSide(EShowDownSide::Collector, CollectorState.HandCards);
	SetInitialDealDeckVisual(CardSystem->GetRemainingCardCount(), ActiveCardDeckCopies * 7);

	UE_LOG(LogTemp, Log, TEXT("Single player deck: ranks 1-7 x2, total 14 cards."));
	UE_LOG(LogTemp, Log, TEXT("Player hand count: %d"), PlayerState.HandCards.Num());
	UE_LOG(LogTemp, Log, TEXT("Collector hand count: %d"), CollectorState.HandCards.Num());
}

void AShowDownGameModeBase::StartInitialCardDealPresentation(
	int32 DeckCopies,
	bool bMultiplayer,
	TFunction<void()>&& Continuation)
{
	ClearInitialCardDealPresentation();
	InitialCardDealPresentationContinuation = MoveTemp(Continuation);
	InitialCardDealDeckCopies = FMath::Clamp(DeckCopies, 2, 4);
	bInitialCardDealIsMultiplayer = bMultiplayer;

	if (!bUseInitialCardDealPresentation || !GetWorld())
	{
		if (TryImmediateInitialCardDealFallback())
		{
			FinishInitialCardDealPresentation();
		}
		else
		{
			StopInitialCardDealOnFailure(TEXT("immediate fallback failed"));
		}
		return;
	}

	if (!CardClass)
	{
		CardClass = ACard::StaticClass();
	}
	if (!CardClass || !CardSystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("Initial card deal presentation could not start because the card class or card system is missing."));
		if (TryImmediateInitialCardDealFallback())
		{
			FinishInitialCardDealPresentation();
		}
		else
		{
			StopInitialCardDealOnFailure(TEXT("card class or card system missing"));
		}
		return;
	}

	bInitialCardDealPresentationInProgress = true;
	bInitialCardDealShowcaseStarted = false;
	InitialCardDealCameraReadySlots.Reset();
	bInitialCardSpatialCacheValid = false;
	bInitialCardDeckBoundsCacheValid = false;
	bInitialCardTableSurfaceCacheValid = false;
	CachedInitialCardShowcasePadRadius = 0.0f;
	CachedInitialCardFlatSlotCenters.Reset();
	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->SetPhase(EShowDownPhase::None);
	}
	SetInitialCardDealInputLocked(true);

	if (bInitialCardDealIsMultiplayer)
	{
		// A reliable camera-ready acknowledgement normally starts the showcase.
		// The timeout keeps a disconnected or extremely slow client from blocking
		// the match forever; late clients seek into replicated card motion.
		ScheduleInitialCardDealAction(5.0f, [this]()
		{
			BeginInitialCardDeckShowcase();
		});
		return;
	}

	BeginInitialCardDeckShowcase();
}

void AShowDownGameModeBase::StartHandRedealPresentation(
	bool bMultiplayer,
	TFunction<void()>&& Continuation)
{
	ClearInitialCardDealPresentation();
	InitialCardDealPresentationContinuation = MoveTemp(Continuation);
	bInitialCardDealIsMultiplayer = bMultiplayer;

	if (!CardClass)
	{
		CardClass = ACard::StaticClass();
	}
	if (!GetWorld() || !CardSystem || !CardClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Card redeal presentation could not start because its world, card system, or card class is missing."));
		TFunction<void()> SavedContinuation = MoveTemp(InitialCardDealPresentationContinuation);
		if (SavedContinuation)
		{
			SavedContinuation();
		}
		return;
	}

	bInitialCardDealPresentationInProgress = true;
	bInitialCardDealShowcaseStarted = true;
	InitialCardDealCameraReadySlots.Reset();
	bInitialCardSpatialCacheValid = false;
	bInitialCardDeckBoundsCacheValid = false;
	bInitialCardTableSurfaceCacheValid = false;
	CachedInitialCardShowcasePadRadius = 0.0f;
	CachedInitialCardFlatSlotCenters.Reset();
	SetInitialCardDealInputLocked(true);
	RefreshInitialCardDealSpatialCache();

	TArray<ACard*> CardsInDealOrder;
	TArray<FTransform> FlatTransforms;
	TArray<FTransform> FinalTransforms;
	int32 ParticipantCount = 0;
	int32 DeckRemainingBeforeDeal = 0;
	const bool bPrepared = bMultiplayer
		? PrepareMultiplayerRedealHands(
			CardsInDealOrder,
			FlatTransforms,
			FinalTransforms,
			ParticipantCount,
			DeckRemainingBeforeDeal)
		: PrepareSinglePlayerRedealHands(
			CardsInDealOrder,
			FlatTransforms,
			FinalTransforms,
			ParticipantCount,
			DeckRemainingBeforeDeal);

	if (!bPrepared)
	{
		UE_LOG(LogTemp, Warning, TEXT("Card redeal preparation failed. Falling back to an immediate fresh hand."));
		TFunction<void()> SavedContinuation = MoveTemp(InitialCardDealPresentationContinuation);
		ClearInitialCardDealPresentation();
		if (bMultiplayer)
		{
			DealMultiplayerHands();
		}
		else
		{
			DealInitialHand();
		}
		if (SavedContinuation)
		{
			SavedContinuation();
		}
		return;
	}

	AnimatePreparedOpeningHands(
		CardsInDealOrder,
		FlatTransforms,
		FinalTransforms,
		ParticipantCount,
		DeckRemainingBeforeDeal);
}

void AShowDownGameModeBase::NotifyInitialCardDealCameraReady(AController* ReadyController)
{
	if (!HasAuthority()
		|| !bInitialCardDealPresentationInProgress
		|| !bInitialCardDealIsMultiplayer
		|| bInitialCardDealShowcaseStarted)
	{
		return;
	}

	ASDPlayerState* ReadyPlayer = GetPlayerStateForController(ReadyController);
	if (!ReadyPlayer || ReadyPlayer->Lives <= 0 || !MultiplayerPlayers.Contains(ReadyPlayer))
	{
		return;
	}
	InitialCardDealCameraReadySlots.Add(ReadyPlayer->ShowDownSlot);

	int32 RequiredReadyCount = 0;
	for (const ASDPlayerState* Player : MultiplayerPlayers)
	{
		if (Player && Player->Lives > 0)
		{
			++RequiredReadyCount;
		}
	}
	if (RequiredReadyCount >= 2 && InitialCardDealCameraReadySlots.Num() >= RequiredReadyCount)
	{
		BeginInitialCardDeckShowcase();
	}
}

bool AShowDownGameModeBase::TryImmediateInitialCardDealFallback()
{
	if (!CardSystem)
	{
		return false;
	}

	if (bInitialCardDealIsMultiplayer)
	{
		DealMultiplayerHands();
		int32 CompleteAliveHands = 0;
		int32 AlivePlayers = 0;
		for (const ASDPlayerState* Player : MultiplayerPlayers)
		{
			if (Player && Player->Lives > 0)
			{
				++AlivePlayers;
				if (Player->HandCards.Num() == HandCount)
				{
					++CompleteAliveHands;
				}
			}
		}
		return AlivePlayers >= 2 && CompleteAliveHands == AlivePlayers;
	}

	DealInitialHand();
	return PlayerState.HandCards.Num() == HandCount
		&& CollectorState.HandCards.Num() == HandCount;
}

void AShowDownGameModeBase::StopInitialCardDealOnFailure(const TCHAR* Reason)
{
	UE_LOG(LogTemp, Error, TEXT("Initial card deal stopped safely: %s"), Reason ? Reason : TEXT("unknown failure"));
	if (ASDSelfShotGunActor* GunActor = FindSelfShotGunActor())
	{
		GunActor->SetOpeningCardShowcaseStowed(false);
	}
	for (FTimerHandle& TimerHandle : InitialCardDealPresentationTimerHandles)
	{
		GetWorldTimerManager().ClearTimer(TimerHandle);
	}
	InitialCardDealPresentationTimerHandles.Reset();
	InitialCardDealPresentationContinuation = TFunction<void()>();
	bInitialCardDealPresentationInProgress = true;
	bInitialCardDealShowcaseStarted = false;
	SetInitialDealDeckVisual(0, 0);
	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->SetPhase(EShowDownPhase::None);
	}
	SetInitialCardDealInputLocked(true);
}

void AShowDownGameModeBase::SetInitialCardDealInputLocked(bool bLocked) const
{
	if (!GetWorld())
	{
		return;
	}
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		if (AShowDownPlayerController* PlayerController = Cast<AShowDownPlayerController>(Iterator->Get()))
		{
			if (!IsActiveNetworkPlayerController(PlayerController))
			{
				continue;
			}

			PlayerController->ClientSetInitialCardDealInputLocked(bLocked);
		}
	}
}

void AShowDownGameModeBase::SetInitialDealDeckVisual(
	int32 RemainingCards,
	int32 TotalCards) const
{
	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->SetInitialDealDeckVisualState(
			InitialDealDeckSourceActorTag,
			RemainingCards,
			TotalCards);
	}
}

void AShowDownGameModeBase::RefreshInitialCardDealSpatialCache() const
{
	CachedInitialCardTableCenter = ResolveSingleTableCenter(GetWorld());
	CachedInitialCardReferenceHandSlot = GetHandSlotForSide(EShowDownSide::Player);
	if (bInitialCardDealIsMultiplayer)
	{
		for (ASDPlayerState* Player : MultiplayerPlayers)
		{
			if (Player && Player->ShowDownSlot == EShowDownPlayerSlot::Player1)
			{
				if (const ASDCardPlacementAnchor* PlayerAnchor = GetHandAnchorForPlayerSlot(Player->ShowDownSlot))
				{
					CachedInitialCardReferenceHandSlot = PlayerAnchor->GetSlotComponent();
				}
				break;
			}
		}
	}
	bInitialCardSpatialCacheValid = true;
	ResolveInitialCardDeckTop();
	ResolveInitialCardTableSurfaceZ();
	// The showcase cards lie on the table. Raising them above the gun made the
	// cards float high enough for their shadows to look like a second grid.
	CachedInitialCardShowcasePlaneZ = CachedInitialCardTableSurfaceZ + 0.05f;
	CachedInitialCardShowcaseCenter = CachedInitialCardTableCenter;

}

void AShowDownGameModeBase::BeginInitialCardDeckShowcase()
{
	if (!bInitialCardDealPresentationInProgress || bInitialCardDealShowcaseStarted || !GetWorld())
	{
		return;
	}

	bInitialCardDealShowcaseStarted = true;
	RefreshInitialCardDealSpatialCache();
	if (ASDSelfShotGunActor* GunActor = FindSelfShotGunActor())
	{
		GunActor->SetOpeningCardShowcaseStowed(true);
	}

	const int32 CardCount = InitialCardDealDeckCopies * 7;
	// Showcase the real cards at the same visual scale used by the final hand.
	const float GridVisualScale = 1.0f;
	SetInitialDealDeckVisual(CardCount, CardCount);
	const float BeatDelay = FMath::Max(0.0f, InitialDealBeatDelay);
	const float LeadInSeconds = bInitialCardDealIsMultiplayer ? 0.55f : 0.65f;
	const float RevealStaggerSeconds = 0.075f;
	const float RevealMoveDuration = FMath::Max(0.65f, InitialDealCardMoveDuration);
	const float RevealHoldSeconds = FMath::Max(BeatDelay, InitialDealShowcaseHoldDuration);
	const float FlipDuration = FMath::Max(0.35f, RevealMoveDuration * 0.8f);
	const float FlipStaggerSeconds = 0.04f;
	const float GatherDuration = FMath::Max(0.45f, RevealMoveDuration);
	const float GatherStaggerSeconds = 0.04f;

	for (int32 CardIndex = 0; CardIndex < CardCount; ++CardIndex)
	{
		const float DeckHeightAlpha = static_cast<float>(CardCount - CardIndex)
			/ static_cast<float>(CardCount);
		const FTransform StackTransform = BuildInitialCardStackTransform(DeckHeightAlpha);
		ACard* Card = GetWorld()->SpawnActor<ACard>(CardClass, StackTransform);
		if (!Card)
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to spawn opening deck card %d/%d."), CardIndex + 1, CardCount);
			TFunction<void()> SavedContinuation = MoveTemp(InitialCardDealPresentationContinuation);
			ClearInitialCardDealPresentation();
			InitialCardDealPresentationContinuation = MoveTemp(SavedContinuation);
			if (TryImmediateInitialCardDealFallback())
			{
				FinishInitialCardDealPresentation();
			}
			else
			{
				StopInitialCardDealOnFailure(TEXT("opening card spawn and immediate fallback failed"));
			}
			return;
		}

		Card->SetCard((CardIndex % 7) + 1);
		// The number is configured once. From this point on, only physical card
		// rotation decides whether the front or back can be seen.
		Card->SetFaceUp(true);
		Card->SetSelectable(false);
		Card->SetHandOwnerSlot(EShowDownPlayerSlot::None);
		// The placed decorative deck remains the only visible pile. Runtime card
		// actors stay hidden inside it until each card starts its showcase move.
		Card->SetActorHiddenInGame(true);
		// Set the showcase size while hidden so the card does not visibly grow as
		// it leaves the placed deck.
		Card->MoveToPresentationTransform(StackTransform, GridVisualScale, 0.12f, 0.0f, false);
		Card->ForceNetUpdate();
		InitialCardDealDeckCards.Add(Card);

		const FTransform GridTransform = BuildInitialCardGridTransform(CardIndex, InitialCardDealDeckCopies, false);
		const TWeakObjectPtr<ACard> WeakCard(Card);
		ScheduleInitialCardDealAction(LeadInSeconds + CardIndex * RevealStaggerSeconds,
			[this, WeakCard, GridTransform, GridVisualScale, RevealMoveDuration, CardIndex, CardCount]()
			{
				if (ACard* LiveCard = WeakCard.Get())
				{
					SetInitialDealDeckVisual(CardCount - CardIndex - 1, CardCount);
					LiveCard->SetActorHiddenInGame(false);
					LiveCard->MoveToPresentationTransform(
						GridTransform,
						GridVisualScale,
						RevealMoveDuration,
						24.0f,
						false,
						false);
					LiveCard->ForceNetUpdate();
				}
			});
	}

	const float RevealFinishedAt = LeadInSeconds
		+ FMath::Max(0, CardCount - 1) * RevealStaggerSeconds
		+ RevealMoveDuration;
	const float FlipStartedAt = RevealFinishedAt + RevealHoldSeconds;
	for (int32 CardIndex = 0; CardIndex < InitialCardDealDeckCards.Num(); ++CardIndex)
	{
		const TWeakObjectPtr<ACard> WeakCard(InitialCardDealDeckCards[CardIndex]);
		const FTransform FaceDownGridTransform = BuildInitialCardGridTransform(CardIndex, InitialCardDealDeckCopies, true);
		ScheduleInitialCardDealAction(FlipStartedAt + CardIndex * FlipStaggerSeconds,
			[WeakCard, FaceDownGridTransform, GridVisualScale, FlipDuration]()
			{
				if (ACard* LiveCard = WeakCard.Get())
				{
					LiveCard->MoveToPresentationTransform(
						FaceDownGridTransform,
						GridVisualScale,
						FlipDuration,
						0.0f,
						false,
						false);
				}
			});
	}

	const float FlipFinishedAt = FlipStartedAt
		+ FMath::Max(0, CardCount - 1) * FlipStaggerSeconds
		+ FlipDuration;
	const float GatherStartedAt = FlipFinishedAt + BeatDelay;
	for (int32 CardIndex = 0; CardIndex < InitialCardDealDeckCards.Num(); ++CardIndex)
	{
		const TWeakObjectPtr<ACard> WeakCard(InitialCardDealDeckCards[CardIndex]);
		const float RestoredDeckHeightAlpha = static_cast<float>(CardIndex + 1)
			/ static_cast<float>(CardCount);
		const FTransform StackTransform = BuildInitialCardStackTransform(RestoredDeckHeightAlpha);
		ScheduleInitialCardDealAction(GatherStartedAt + CardIndex * GatherStaggerSeconds,
			[WeakCard, StackTransform, GridVisualScale, GatherDuration]()
			{
				if (ACard* LiveCard = WeakCard.Get())
				{
					LiveCard->MoveToPresentationTransform(
						StackTransform,
						GridVisualScale,
						GatherDuration,
						30.0f,
						false);
				}
			});
		ScheduleInitialCardDealAction(
			GatherStartedAt + CardIndex * GatherStaggerSeconds + GatherDuration,
			[this, WeakCard, CardIndex, CardCount]()
			{
				if (ACard* LiveCard = WeakCard.Get())
				{
					LiveCard->SetActorHiddenInGame(true);
					LiveCard->ForceNetUpdate();
				}
				SetInitialDealDeckVisual(CardIndex + 1, CardCount);
			});
	}

	const float GatherFinishedAt = GatherStartedAt
		+ FMath::Max(0, CardCount - 1) * GatherStaggerSeconds
		+ GatherDuration;
	ScheduleInitialCardDealAction(GatherFinishedAt + 0.05f, [this]()
	{
		for (ACard* Card : InitialCardDealDeckCards)
		{
			if (IsValid(Card))
			{
				Card->SetActorHiddenInGame(true);
				Card->ForceNetUpdate();
			}
		}
		SetInitialDealDeckVisual(InitialCardDealDeckCards.Num(), InitialCardDealDeckCards.Num());
		if (ASDSelfShotGunActor* GunActor = FindSelfShotGunActor())
		{
			GunActor->SetOpeningCardShowcaseStowed(false);
		}
	});
	ScheduleInitialCardDealAction(GatherFinishedAt + FMath::Max(BeatDelay, 0.08f), [this]()
	{
		StartInitialCardDealFromStack();
	});
}

void AShowDownGameModeBase::StartInitialCardDealFromStack()
{
	if (!bInitialCardDealPresentationInProgress)
	{
		return;
	}

	TArray<ACard*> CardsInDealOrder;
	TArray<FTransform> FlatTransforms;
	TArray<FTransform> FinalTransforms;
	int32 ParticipantCount = 0;
	const bool bPrepared = bInitialCardDealIsMultiplayer
		? PrepareMultiplayerOpeningHands(CardsInDealOrder, FlatTransforms, FinalTransforms, ParticipantCount)
		: PrepareSinglePlayerOpeningHands(CardsInDealOrder, FlatTransforms, FinalTransforms, ParticipantCount);

	if (!bPrepared)
	{
		UE_LOG(LogTemp, Warning, TEXT("Initial card presentation hand preparation failed. Falling back to the immediate deal."));
		TFunction<void()> SavedContinuation = MoveTemp(InitialCardDealPresentationContinuation);
		ClearInitialCardDealPresentation();
		InitialCardDealPresentationContinuation = MoveTemp(SavedContinuation);
		if (TryImmediateInitialCardDealFallback())
		{
			FinishInitialCardDealPresentation();
		}
		else
		{
			StopInitialCardDealOnFailure(TEXT("hand preparation and immediate fallback failed"));
		}
		return;
	}

	const int32 DeckRemainingBeforeDeal = CardSystem
		? CardSystem->GetRemainingCardCount() + CardsInDealOrder.Num()
		: CardsInDealOrder.Num();
	AnimatePreparedOpeningHands(
		CardsInDealOrder,
		FlatTransforms,
		FinalTransforms,
		ParticipantCount,
		DeckRemainingBeforeDeal);
}

bool AShowDownGameModeBase::PrepareSinglePlayerOpeningHands(
	TArray<ACard*>& OutCardsInDealOrder,
	TArray<FTransform>& OutFlatTransforms,
	TArray<FTransform>& OutFinalTransforms,
	int32& OutParticipantCount)
{
	USceneComponent* PlayerHandSlot = GetHandSlotForSide(EShowDownSide::Player);
	USceneComponent* CollectorHandSlot = GetHandSlotForSide(EShowDownSide::Collector);
	if (!CardSystem || !PlayerHandSlot || !CollectorHandSlot || InitialCardDealDeckCards.Num() < HandCount * 2)
	{
		return false;
	}

	const TArray<USceneComponent*> HandSlots = { PlayerHandSlot, CollectorHandSlot };
	const ASDCardPlacementAnchor* PlayerFlatAnchor = GetHandAnchorForSide(EShowDownSide::Player);
	const ASDCardPlacementAnchor* CollectorFlatAnchor = GetHandAnchorForSide(EShowDownSide::Collector);
	const TArray<USceneComponent*> FlatSlots = {
		PlayerFlatAnchor && PlayerFlatAnchor->GetSlotComponent()
			? PlayerFlatAnchor->GetSlotComponent()
			: PlayerHandSlot,
		CollectorFlatAnchor && CollectorFlatAnchor->GetSlotComponent()
			? CollectorFlatAnchor->GetSlotComponent()
			: CollectorHandSlot
	};
	const TArray<FSDCardHandLayoutSettings> HandLayouts = {
		ResolveHandLayoutSettings(EShowDownSide::Player),
		ResolveHandLayoutSettings(EShowDownSide::Collector)
	};
	TArray<TArray<int32>> RanksByParticipant;
	RanksByParticipant.SetNum(2);
	CardSystem->ResetDeck(2);
	CardSystem->ShuffleDeck();
	ActiveCardDeckCopies = 2;
	for (int32 CardIndex = 0; CardIndex < HandCount; ++CardIndex)
	{
		for (int32 ParticipantIndex = 0; ParticipantIndex < 2; ++ParticipantIndex)
		{
			TArray<int32> DealtRank;
			if (!CardSystem->DealCards(1, DealtRank) || DealtRank.Num() != 1)
			{
				return false;
			}
			RanksByParticipant[ParticipantIndex].Add(DealtRank[0]);
		}
	}

	PlayerState.HandCards.Reset();
	CollectorState.HandCards.Reset();
	OutParticipantCount = 2;
	for (int32 CardIndex = 0; CardIndex < HandCount; ++CardIndex)
	{
		for (int32 ParticipantIndex = 0; ParticipantIndex < 2; ++ParticipantIndex)
		{
			const int32 Rank = RanksByParticipant[ParticipantIndex][CardIndex];
			const int32 DeckCardIndex = InitialCardDealDeckCards.IndexOfByPredicate([Rank](const TObjectPtr<ACard>& Card)
			{
				return IsValid(Card) && Card->Rank == Rank;
			});
			if (DeckCardIndex == INDEX_NONE)
			{
				return false;
			}

			ACard* Card = InitialCardDealDeckCards[DeckCardIndex];
			InitialCardDealDeckCards.RemoveAt(DeckCardIndex);
			Card->SetSelectable(false);
			Card->SetHandOwnerSlot(EShowDownPlayerSlot::None);
			if (ParticipantIndex == 0)
			{
				PlayerState.HandCards.Add(Card);
			}
			else
			{
				CollectorState.HandCards.Add(Card);
			}

			OutCardsInDealOrder.Add(Card);
			OutFlatTransforms.Add(BuildInitialFlatCardTransform(FlatSlots[ParticipantIndex], CardIndex, HandCount));
			OutFinalTransforms.Add(CardSystem->BuildHandCardTransform(
				HandSlots[ParticipantIndex],
				HandLayouts[ParticipantIndex],
				CardIndex,
				HandCount));
		}
	}

	ApplyCardMotionForSide(EShowDownSide::Player, PlayerState.HandCards);
	ApplyCardMotionForSide(EShowDownSide::Collector, CollectorState.HandCards);
	UE_LOG(LogTemp, Log, TEXT("Single player opening deck: 14 cards shown, 10 cards dealt round-robin."));
	return OutCardsInDealOrder.Num() == HandCount * OutParticipantCount;
}

bool AShowDownGameModeBase::PrepareMultiplayerOpeningHands(
	TArray<ACard*>& OutCardsInDealOrder,
	TArray<FTransform>& OutFlatTransforms,
	TArray<FTransform>& OutFinalTransforms,
	int32& OutParticipantCount)
{
	if (!CardSystem)
	{
		return false;
	}

	TArray<ASDPlayerState*> Participants;
	TArray<USceneComponent*> HandSlots;
	TArray<USceneComponent*> FlatSlots;
	TArray<FSDCardHandLayoutSettings> HandLayouts;
	for (ASDPlayerState* Player : MultiplayerPlayers)
	{
		if (!Player || Player->Lives <= 0)
		{
			continue;
		}

		USceneComponent* HandSlot = GetHandSlotForPlayerState(Player);
		if (!HandSlot)
		{
			return false;
		}
		Participants.Add(Player);
		HandSlots.Add(HandSlot);
		const ASDCardPlacementAnchor* FlatAnchor = GetHandAnchorForPlayerSlot(Player->ShowDownSlot);
		FlatSlots.Add(FlatAnchor && FlatAnchor->GetSlotComponent()
			? FlatAnchor->GetSlotComponent()
			: HandSlot);
		HandLayouts.Add(ResolveHandLayoutSettingsForPlayerState(Player));
	}

	OutParticipantCount = Participants.Num();
	if (OutParticipantCount < 2 || OutParticipantCount > 4
		|| InitialCardDealDeckCards.Num() < HandCount * OutParticipantCount)
	{
		return false;
	}

	TArray<TArray<int32>> RanksByParticipant;
	RanksByParticipant.SetNum(OutParticipantCount);
	CardSystem->ResetDeck(OutParticipantCount);
	CardSystem->ShuffleDeck();
	ActiveCardDeckCopies = OutParticipantCount;
	for (int32 CardIndex = 0; CardIndex < HandCount; ++CardIndex)
	{
		for (int32 ParticipantIndex = 0; ParticipantIndex < OutParticipantCount; ++ParticipantIndex)
		{
			TArray<int32> DealtRank;
			if (!CardSystem->DealCards(1, DealtRank) || DealtRank.Num() != 1)
			{
				return false;
			}
			RanksByParticipant[ParticipantIndex].Add(DealtRank[0]);
		}
	}

	for (ASDPlayerState* Player : Participants)
	{
		Player->ClearHand();
		Player->CurrentBet = 0;
	}

	for (int32 CardIndex = 0; CardIndex < HandCount; ++CardIndex)
	{
		for (int32 ParticipantIndex = 0; ParticipantIndex < OutParticipantCount; ++ParticipantIndex)
		{
			ASDPlayerState* Player = Participants[ParticipantIndex];
			const int32 Rank = RanksByParticipant[ParticipantIndex][CardIndex];
			const int32 DeckCardIndex = InitialCardDealDeckCards.IndexOfByPredicate([Rank](const TObjectPtr<ACard>& Card)
			{
				return IsValid(Card) && Card->Rank == Rank;
			});
			if (DeckCardIndex == INDEX_NONE)
			{
				return false;
			}

			ACard* Card = InitialCardDealDeckCards[DeckCardIndex];
			InitialCardDealDeckCards.RemoveAt(DeckCardIndex);
			Card->SetSelectable(false);
			Card->SetHandOwnerSlot(Player->ShowDownSlot);
			Player->AddHandCard(Card);
			OutCardsInDealOrder.Add(Card);
			OutFlatTransforms.Add(BuildInitialFlatCardTransform(FlatSlots[ParticipantIndex], CardIndex, HandCount));
			OutFinalTransforms.Add(CardSystem->BuildHandCardTransform(
				HandSlots[ParticipantIndex],
				HandLayouts[ParticipantIndex],
				CardIndex,
				HandCount));
		}
	}

	for (int32 ParticipantIndex = 0; ParticipantIndex < Participants.Num(); ++ParticipantIndex)
	{
		ASDPlayerState* Player = Participants[ParticipantIndex];
		ApplyCardMotionForPlayerState(Player, Player->HandCards);
		Player->ForceNetUpdate();
	}
	UE_LOG(LogTemp, Log, TEXT("Multiplayer opening deck: %d cards shown, %d cards dealt round-robin."),
		OutParticipantCount * 7,
		OutParticipantCount * HandCount);
	return OutCardsInDealOrder.Num() == HandCount * OutParticipantCount;
}

bool AShowDownGameModeBase::PrepareSinglePlayerRedealHands(
	TArray<ACard*>& OutCardsInDealOrder,
	TArray<FTransform>& OutFlatTransforms,
	TArray<FTransform>& OutFinalTransforms,
	int32& OutParticipantCount,
	int32& OutDeckRemainingBeforeDeal)
{
	USceneComponent* PlayerHandSlot = GetHandSlotForSide(EShowDownSide::Player);
	USceneComponent* CollectorHandSlot = GetHandSlotForSide(EShowDownSide::Collector);
	if (!CardSystem
		|| !CardClass
		|| !PlayerHandSlot
		|| !CollectorHandSlot
		|| PlayerState.HandCards.Num() > 0
		|| CollectorState.HandCards.Num() > 0)
	{
		return false;
	}

	constexpr int32 ParticipantCount = 2;
	if (ActiveCardDeckCopies <= 0 || CardSystem->GetRemainingCardCount() < ParticipantCount)
	{
		CardSystem->ResetDeck(ParticipantCount);
		CardSystem->ShuffleDeck();
		ActiveCardDeckCopies = ParticipantCount;
	}
	InitialCardDealDeckCopies = ParticipantCount;
	OutDeckRemainingBeforeDeal = CardSystem->GetRemainingCardCount();
	const int32 CardsPerParticipant = FMath::Min(
		HandCount,
		OutDeckRemainingBeforeDeal / ParticipantCount);
	if (CardsPerParticipant <= 0)
	{
		return false;
	}

	const TArray<USceneComponent*> HandSlots = { PlayerHandSlot, CollectorHandSlot };
	const ASDCardPlacementAnchor* PlayerFlatAnchor = GetHandAnchorForSide(EShowDownSide::Player);
	const ASDCardPlacementAnchor* CollectorFlatAnchor = GetHandAnchorForSide(EShowDownSide::Collector);
	const TArray<USceneComponent*> FlatSlots = {
		PlayerFlatAnchor && PlayerFlatAnchor->GetSlotComponent()
			? PlayerFlatAnchor->GetSlotComponent()
			: PlayerHandSlot,
		CollectorFlatAnchor && CollectorFlatAnchor->GetSlotComponent()
			? CollectorFlatAnchor->GetSlotComponent()
			: CollectorHandSlot
	};
	const TArray<FSDCardHandLayoutSettings> HandLayouts = {
		ResolveHandLayoutSettings(EShowDownSide::Player),
		ResolveHandLayoutSettings(EShowDownSide::Collector)
	};
	const int32 DeckVisualTotalCards = FMath::Max(ParticipantCount, ActiveCardDeckCopies) * 7;

	OutParticipantCount = ParticipantCount;
	for (int32 CardIndex = 0; CardIndex < CardsPerParticipant; ++CardIndex)
	{
		for (int32 ParticipantIndex = 0; ParticipantIndex < ParticipantCount; ++ParticipantIndex)
		{
			TArray<int32> DealtRank;
			if (!CardSystem->DealCards(1, DealtRank) || DealtRank.Num() != 1)
			{
				for (ACard* SpawnedCard : OutCardsInDealOrder)
				{
					if (IsValid(SpawnedCard))
					{
						SpawnedCard->Destroy();
					}
				}
				PlayerState.HandCards.Reset();
				CollectorState.HandCards.Reset();
				return false;
			}

			const int32 DealIndex = CardIndex * ParticipantCount + ParticipantIndex;
			const float DeckHeightAlpha = static_cast<float>(OutDeckRemainingBeforeDeal - DealIndex)
				/ static_cast<float>(DeckVisualTotalCards);
			const FTransform StackTransform = BuildInitialCardStackTransform(DeckHeightAlpha);
			ACard* Card = GetWorld()->SpawnActor<ACard>(CardClass, StackTransform);
			if (!Card)
			{
				for (ACard* SpawnedCard : OutCardsInDealOrder)
				{
					if (IsValid(SpawnedCard))
					{
						SpawnedCard->Destroy();
					}
				}
				PlayerState.HandCards.Reset();
				CollectorState.HandCards.Reset();
				return false;
			}

			Card->SetActorHiddenInGame(true);
			Card->SetCard(DealtRank[0]);
			Card->SetFaceUp(true);
			Card->SetSelectable(false);
			Card->SetHandOwnerSlot(EShowDownPlayerSlot::None);
			Card->MoveToPresentationTransform(StackTransform, 1.0f, 0.12f, 0.0f, false);
			if (ParticipantIndex == 0)
			{
				PlayerState.HandCards.Add(Card);
			}
			else
			{
				CollectorState.HandCards.Add(Card);
			}

			OutCardsInDealOrder.Add(Card);
			OutFlatTransforms.Add(BuildInitialFlatCardTransform(
				FlatSlots[ParticipantIndex],
				CardIndex,
				CardsPerParticipant));
			OutFinalTransforms.Add(CardSystem->BuildHandCardTransform(
				HandSlots[ParticipantIndex],
				HandLayouts[ParticipantIndex],
				CardIndex,
				CardsPerParticipant));
		}
	}

	ApplyCardMotionForSide(EShowDownSide::Player, PlayerState.HandCards);
	ApplyCardMotionForSide(EShowDownSide::Collector, CollectorState.HandCards);
	UE_LOG(LogTemp, Log, TEXT("Single-player redeal: %d cards each, %d cards remain in the deck."),
		CardsPerParticipant,
		CardSystem->GetRemainingCardCount());
	return OutCardsInDealOrder.Num() == CardsPerParticipant * ParticipantCount;
}

bool AShowDownGameModeBase::PrepareMultiplayerRedealHands(
	TArray<ACard*>& OutCardsInDealOrder,
	TArray<FTransform>& OutFlatTransforms,
	TArray<FTransform>& OutFinalTransforms,
	int32& OutParticipantCount,
	int32& OutDeckRemainingBeforeDeal)
{
	if (!CardSystem || !CardClass)
	{
		return false;
	}

	TArray<ASDPlayerState*> Participants;
	TArray<USceneComponent*> HandSlots;
	TArray<USceneComponent*> FlatSlots;
	TArray<FSDCardHandLayoutSettings> HandLayouts;
	for (ASDPlayerState* Player : MultiplayerPlayers)
	{
		if (!Player || Player->Lives <= 0)
		{
			continue;
		}
		if (Player->HandCards.Num() > 0)
		{
			return false;
		}

		USceneComponent* HandSlot = GetHandSlotForPlayerState(Player);
		if (!HandSlot)
		{
			return false;
		}
		Participants.Add(Player);
		HandSlots.Add(HandSlot);
		const ASDCardPlacementAnchor* FlatAnchor = GetHandAnchorForPlayerSlot(Player->ShowDownSlot);
		FlatSlots.Add(FlatAnchor && FlatAnchor->GetSlotComponent()
			? FlatAnchor->GetSlotComponent()
			: HandSlot);
		HandLayouts.Add(ResolveHandLayoutSettingsForPlayerState(Player));
	}

	OutParticipantCount = Participants.Num();
	if (OutParticipantCount < 2 || OutParticipantCount > 4)
	{
		return false;
	}

	if (ActiveCardDeckCopies <= 0 || CardSystem->GetRemainingCardCount() < OutParticipantCount)
	{
		CardSystem->ResetDeck(OutParticipantCount);
		CardSystem->ShuffleDeck();
		ActiveCardDeckCopies = OutParticipantCount;
	}
	InitialCardDealDeckCopies = FMath::Clamp(ActiveCardDeckCopies, 2, 4);
	OutDeckRemainingBeforeDeal = CardSystem->GetRemainingCardCount();
	const int32 CardsPerParticipant = FMath::Min(
		HandCount,
		OutDeckRemainingBeforeDeal / OutParticipantCount);
	if (CardsPerParticipant <= 0)
	{
		return false;
	}
	const int32 DeckVisualTotalCards = FMath::Max(OutParticipantCount, ActiveCardDeckCopies) * 7;

	for (ASDPlayerState* Player : Participants)
	{
		Player->ClearHand();
		Player->CurrentBet = 0;
	}

	for (int32 CardIndex = 0; CardIndex < CardsPerParticipant; ++CardIndex)
	{
		for (int32 ParticipantIndex = 0; ParticipantIndex < OutParticipantCount; ++ParticipantIndex)
		{
			TArray<int32> DealtRank;
			if (!CardSystem->DealCards(1, DealtRank) || DealtRank.Num() != 1)
			{
				for (ACard* SpawnedCard : OutCardsInDealOrder)
				{
					if (IsValid(SpawnedCard))
					{
						SpawnedCard->Destroy();
					}
				}
				for (ASDPlayerState* Player : Participants)
				{
					Player->ClearHand();
					Player->ForceNetUpdate();
				}
				return false;
			}

			const int32 DealIndex = CardIndex * OutParticipantCount + ParticipantIndex;
			const float DeckHeightAlpha = static_cast<float>(OutDeckRemainingBeforeDeal - DealIndex)
				/ static_cast<float>(DeckVisualTotalCards);
			const FTransform StackTransform = BuildInitialCardStackTransform(DeckHeightAlpha);
			ACard* Card = GetWorld()->SpawnActor<ACard>(CardClass, StackTransform);
			if (!Card)
			{
				for (ACard* SpawnedCard : OutCardsInDealOrder)
				{
					if (IsValid(SpawnedCard))
					{
						SpawnedCard->Destroy();
					}
				}
				for (ASDPlayerState* Player : Participants)
				{
					Player->ClearHand();
					Player->ForceNetUpdate();
				}
				return false;
			}

			ASDPlayerState* Player = Participants[ParticipantIndex];
			Card->SetActorHiddenInGame(true);
			Card->SetCard(DealtRank[0]);
			Card->SetFaceUp(true);
			Card->SetSelectable(false);
			Card->SetHandOwnerSlot(Player->ShowDownSlot);
			Card->MoveToPresentationTransform(StackTransform, 1.0f, 0.12f, 0.0f, false);
			Player->AddHandCard(Card);

			OutCardsInDealOrder.Add(Card);
			OutFlatTransforms.Add(BuildInitialFlatCardTransform(
				FlatSlots[ParticipantIndex],
				CardIndex,
				CardsPerParticipant));
			OutFinalTransforms.Add(CardSystem->BuildHandCardTransform(
				HandSlots[ParticipantIndex],
				HandLayouts[ParticipantIndex],
				CardIndex,
				CardsPerParticipant));
		}
	}

	for (ASDPlayerState* Player : Participants)
	{
		ApplyCardMotionForPlayerState(Player, Player->HandCards);
		Player->ForceNetUpdate();
	}
	UE_LOG(LogTemp, Log, TEXT("Multiplayer redeal: %d cards each for %d players, %d cards remain in the deck."),
		CardsPerParticipant,
		OutParticipantCount,
		CardSystem->GetRemainingCardCount());
	return OutCardsInDealOrder.Num() == CardsPerParticipant * OutParticipantCount;
}

void AShowDownGameModeBase::AnimatePreparedOpeningHands(
	const TArray<ACard*>& CardsInDealOrder,
	const TArray<FTransform>& FlatTransforms,
	const TArray<FTransform>& FinalTransforms,
	int32 ParticipantCount,
	int32 DeckRemainingBeforeDeal)
{
	if (ParticipantCount <= 0
		|| CardsInDealOrder.Num() != FlatTransforms.Num()
		|| CardsInDealOrder.Num() != FinalTransforms.Num())
	{
		StopInitialCardDealOnFailure(TEXT("prepared hand animation data was inconsistent"));
		return;
	}

	const int32 DealCardCount = CardsInDealOrder.Num();
	const int32 DeckVisualTotalCards = FMath::Max(
		FMath::Max(2, ActiveCardDeckCopies) * 7,
		DeckRemainingBeforeDeal);
	const int32 SafeDeckRemainingBeforeDeal = FMath::Clamp(
		DeckRemainingBeforeDeal,
		DealCardCount,
		DeckVisualTotalCards);
	SetInitialDealDeckVisual(SafeDeckRemainingBeforeDeal, DeckVisualTotalCards);
	// Cards are hidden while restacking, so they can be prepared at their full
	// deal size before becoming visible. No scale-up should occur in flight.
	const float StackVisualScale = 1.0f;
	for (int32 DeckIndex = 0; DeckIndex < InitialCardDealDeckCards.Num(); ++DeckIndex)
	{
		if (ACard* DeckCard = InitialCardDealDeckCards[DeckIndex])
		{
			DeckCard->SetActorHiddenInGame(true);
			DeckCard->MoveToPresentationTransform(
				BuildInitialCardStackTransform(1.0f),
				StackVisualScale,
				0.12f,
				0.0f,
				false);
		}
	}
	for (int32 DealIndex = 0; DealIndex < CardsInDealOrder.Num(); ++DealIndex)
	{
		if (ACard* DealCard = CardsInDealOrder[DealIndex])
		{
			// Prepare every hidden card at the current top of the progressively
			// shrinking decorative deck. The full-size card is already in place before
			// its replicated movement begins, so clients never see a floating source.
			const float DeckHeightAlpha = static_cast<float>(SafeDeckRemainingBeforeDeal - DealIndex)
				/ static_cast<float>(DeckVisualTotalCards);
			DealCard->MoveToPresentationTransform(
				BuildInitialCardStackTransform(DeckHeightAlpha),
				StackVisualScale,
				0.12f,
				0.0f,
				false);
			DealCard->SetActorHiddenInGame(true);
		}
	}

	const float BeatDelay = FMath::Max(0.0f, InitialDealBeatDelay);
	const float DealLeadInSeconds = 0.30f;
	const float DealStaggerSeconds = 0.16f;
	const float DealMoveDuration = FMath::Max(0.65f, InitialDealCardMoveDuration);
	const float DealBounceStrength = FMath::Clamp(InitialDealCardBounceStrength, 0.0f, 1.0f);
	for (int32 DealIndex = 0; DealIndex < CardsInDealOrder.Num(); ++DealIndex)
	{
		const TWeakObjectPtr<ACard> WeakCard(CardsInDealOrder[DealIndex]);
		const FTransform FlatTransform = FlatTransforms[DealIndex];
		ScheduleInitialCardDealAction(DealLeadInSeconds + DealIndex * DealStaggerSeconds,
			[this, WeakCard, FlatTransform, DealMoveDuration, DealBounceStrength, DealIndex,
				SafeDeckRemainingBeforeDeal, DeckVisualTotalCards]()
			{
				SetInitialDealDeckVisual(
					SafeDeckRemainingBeforeDeal - DealIndex - 1,
					DeckVisualTotalCards);
				if (ACard* LiveCard = WeakCard.Get())
				{
					LiveCard->SetActorHiddenInGame(false);
					LiveCard->MoveToPresentationTransform(
						FlatTransform,
						1.0f,
						DealMoveDuration,
						34.0f,
						true,
						false,
						DealBounceStrength);
					LiveCard->ForceNetUpdate();
				}
			});
	}

	const float FlatDealFinishedAt = DealLeadInSeconds
		+ FMath::Max(0, CardsInDealOrder.Num() - 1) * DealStaggerSeconds
		+ DealMoveDuration;
	const float LiftStartedAt = FlatDealFinishedAt + BeatDelay;
	const float LiftStepSeconds = 0.32f;
	const float LiftMoveDuration = FMath::Max(0.1f, InitialDealHandMoveDuration);
	const float LiftArcHeight = FMath::Max(0.0f, InitialDealHandMoveArcHeight);
	const int32 CardsPerParticipant = CardsInDealOrder.Num() / ParticipantCount;
	for (int32 CardIndex = 0; CardIndex < CardsPerParticipant; ++CardIndex)
	{
		const float StartDelay = LiftStartedAt + CardIndex * LiftStepSeconds;
		for (int32 ParticipantIndex = 0; ParticipantIndex < ParticipantCount; ++ParticipantIndex)
		{
			const int32 DealIndex = CardIndex * ParticipantCount + ParticipantIndex;
			if (!CardsInDealOrder.IsValidIndex(DealIndex))
			{
				continue;
			}

			const TWeakObjectPtr<ACard> WeakCard(CardsInDealOrder[DealIndex]);
			const FTransform FinalTransform = FinalTransforms[DealIndex];
			ScheduleInitialCardDealAction(StartDelay,
				[WeakCard, FinalTransform, LiftMoveDuration, LiftArcHeight]()
				{
					if (ACard* LiveCard = WeakCard.Get())
					{
						LiveCard->MoveToPresentationTransform(
							FinalTransform,
							1.0f,
							LiftMoveDuration,
							LiftArcHeight,
							false);
					}
				});
		}
	}

	const float LiftFinishedAt = LiftStartedAt
		+ FMath::Max(0, CardsPerParticipant - 1) * LiftStepSeconds
		+ LiftMoveDuration;
	TArray<TWeakObjectPtr<ACard>> WeakCardsInDealOrder;
	WeakCardsInDealOrder.Reserve(CardsInDealOrder.Num());
	for (ACard* Card : CardsInDealOrder)
	{
		WeakCardsInDealOrder.Add(Card);
	}
	ScheduleInitialCardDealAction(LiftFinishedAt + BeatDelay,
		[this, WeakCardsInDealOrder, FinalTransforms]()
		{
			for (int32 CardIndex = 0; CardIndex < WeakCardsInDealOrder.Num(); ++CardIndex)
			{
				if (ACard* Card = WeakCardsInDealOrder[CardIndex].Get())
				{
					Card->MoveToHandTransform(FinalTransforms[CardIndex]);
				}
			}
			FinishInitialCardDealPresentation();
		});
}

void AShowDownGameModeBase::FinishInitialCardDealPresentation()
{
	TFunction<void()> Continuation = MoveTemp(InitialCardDealPresentationContinuation);
	if (ASDSelfShotGunActor* GunActor = FindSelfShotGunActor())
	{
		GunActor->SetOpeningCardShowcaseStowed(false);
	}
	for (ACard* Card : InitialCardDealDeckCards)
	{
		if (IsValid(Card))
		{
			Card->Destroy();
		}
	}
	InitialCardDealDeckCards.Reset();
	for (FTimerHandle& TimerHandle : InitialCardDealPresentationTimerHandles)
	{
		GetWorldTimerManager().ClearTimer(TimerHandle);
	}
	InitialCardDealPresentationTimerHandles.Reset();
	bInitialCardDealPresentationInProgress = false;
	bInitialCardDealPresentationPlayed = true;
	bInitialCardDealShowcaseStarted = false;
	InitialCardDealCameraReadySlots.Reset();
	const int32 DeckVisualTotalCards = FMath::Max(2, ActiveCardDeckCopies) * 7;
	SetInitialDealDeckVisual(
		CardSystem ? CardSystem->GetRemainingCardCount() : 0,
		DeckVisualTotalCards);
	SetInitialCardDealInputLocked(false);
	UE_LOG(LogTemp, Log, TEXT("Initial card deal presentation completed."));

	if (Continuation)
	{
		Continuation();
	}
}

void AShowDownGameModeBase::ClearInitialCardDealPresentation(bool bDestroyDeckCards)
{
	const bool bWasInProgress = bInitialCardDealPresentationInProgress;
	if (ASDSelfShotGunActor* GunActor = FindSelfShotGunActor())
	{
		GunActor->SetOpeningCardShowcaseStowed(false);
	}
	for (FTimerHandle& TimerHandle : InitialCardDealPresentationTimerHandles)
	{
		GetWorldTimerManager().ClearTimer(TimerHandle);
	}
	InitialCardDealPresentationTimerHandles.Reset();
	if (bDestroyDeckCards)
	{
		for (ACard* Card : InitialCardDealDeckCards)
		{
			if (IsValid(Card))
			{
				Card->Destroy();
			}
		}
	}
	InitialCardDealDeckCards.Reset();
	InitialCardDealPresentationContinuation = TFunction<void()>();
	bInitialCardDealPresentationInProgress = false;
	bInitialCardDealShowcaseStarted = false;
	InitialCardDealCameraReadySlots.Reset();
	bInitialCardSpatialCacheValid = false;
	bInitialCardDeckBoundsCacheValid = false;
	bInitialCardTableSurfaceCacheValid = false;
	CachedInitialCardShowcasePadRadius = 0.0f;
	CachedInitialCardFlatSlotCenters.Reset();
	CachedInitialCardReferenceHandSlot.Reset();
	if (bWasInProgress)
	{
		SetInitialDealDeckVisual(0, 0);
		SetInitialCardDealInputLocked(false);
	}
}

void AShowDownGameModeBase::ScheduleInitialCardDealAction(float DelaySeconds, TFunction<void()>&& Action)
{
	if (!Action)
	{
		return;
	}
	if (DelaySeconds <= KINDA_SMALL_NUMBER)
	{
		Action();
		return;
	}

	FTimerHandle TimerHandle;
	FTimerDelegate TimerDelegate;
	TimerDelegate.BindWeakLambda(this, [Action = MoveTemp(Action)]() mutable
	{
		if (Action)
		{
			Action();
		}
	});
	GetWorldTimerManager().SetTimer(TimerHandle, TimerDelegate, DelaySeconds, false);
	InitialCardDealPresentationTimerHandles.Add(TimerHandle);
}

FTransform AShowDownGameModeBase::BuildInitialCardGridTransform(int32 CardIndex, int32 DeckCopies, bool bFaceDown) const
{
	const int32 SafeCopies = FMath::Clamp(DeckCopies, 2, 4);
	const int32 ColumnIndex = FMath::Clamp(CardIndex % 7, 0, 6);
	const int32 RowIndex = FMath::Clamp(CardIndex / 7, 0, SafeCopies - 1);
	const FVector TableCenter = bInitialCardSpatialCacheValid
		? CachedInitialCardTableCenter
		: ResolveSingleTableCenter(GetWorld());
	const FVector ShowcaseCenter = bInitialCardSpatialCacheValid
		? CachedInitialCardShowcaseCenter
		: TableCenter;
	USceneComponent* ReferenceHandSlot = bInitialCardSpatialCacheValid
		? CachedInitialCardReferenceHandSlot.Get()
		: GetHandSlotForSide(EShowDownSide::Player);

	FVector TowardTableCenter = ReferenceHandSlot
		? TableCenter - ReferenceHandSlot->GetComponentLocation()
		: FVector::ForwardVector;
	TowardTableCenter.Z = 0.0f;
	TowardTableCenter = TowardTableCenter.GetSafeNormal();
	if (TowardTableCenter.IsNearlyZero())
	{
		TowardTableCenter = FVector::ForwardVector;
	}
	const FVector GridRight = FVector::CrossProduct(FVector::UpVector, TowardTableCenter).GetSafeNormal();
	const FVector TowardPlayer = -TowardTableCenter;
	const float ColumnFromCenter = static_cast<float>(ColumnIndex) - 3.0f;
	const float RowFromCenter = static_cast<float>(RowIndex) - static_cast<float>(SafeCopies - 1) * 0.5f;
	const float ColumnSpacing = FMath::Max(10.0f, InitialDealShowcaseGridSpacing.X);
	// A full-size BP_Card is slightly longer than ten units, so keep enough row
	// separation for every rank to remain visible without card overlap.
	const float RowSpacing = FMath::Max(12.0f, InitialDealShowcaseGridSpacing.Y);
	FVector Location = ShowcaseCenter
		+ GridRight * (ColumnFromCenter * ColumnSpacing)
		+ TowardPlayer * (RowFromCenter * RowSpacing);
	Location.Z = (bInitialCardSpatialCacheValid
		? CachedInitialCardShowcasePlaneZ
		: ResolveInitialCardTableSurfaceZ() + 0.05f)
		+ CardIndex * 0.015f;
	return FTransform(BuildInitialFlatCardRotation(TowardTableCenter, bFaceDown), Location);
}

FTransform AShowDownGameModeBase::BuildInitialCardStackTransform(float DeckHeightAlpha) const
{
	const FVector TableCenter = bInitialCardSpatialCacheValid
		? CachedInitialCardTableCenter
		: ResolveSingleTableCenter(GetWorld());
	USceneComponent* ReferenceHandSlot = bInitialCardSpatialCacheValid
		? CachedInitialCardReferenceHandSlot.Get()
		: GetHandSlotForSide(EShowDownSide::Player);

	FVector TowardTableCenter = ReferenceHandSlot
		? TableCenter - ReferenceHandSlot->GetComponentLocation()
		: FVector::ForwardVector;
	TowardTableCenter.Z = 0.0f;
	TowardTableCenter = TowardTableCenter.GetSafeNormal();
	if (TowardTableCenter.IsNearlyZero())
	{
		TowardTableCenter = FVector::ForwardVector;
	}
	FVector Location = ResolveInitialCardDeckTop();
	// The decorative deck shrinks with its bottom fixed. Prepare hidden runtime
	// cards at that same moving top so every revealed/dealt card emerges from the
	// visible pile rather than the original full-height top.
	Location.Z = FMath::Lerp(
		CachedInitialCardDeckBottomZ,
		Location.Z,
		FMath::Clamp(DeckHeightAlpha, 0.0f, 1.0f)) + 0.04f;
	return FTransform(BuildInitialFlatCardRotation(TowardTableCenter, true), Location);
}

FTransform AShowDownGameModeBase::BuildInitialFlatCardTransform(
	USceneComponent* HandSlot,
	int32 CardIndex,
	int32 CardCount) const
{
	if (!HandSlot || CardCount <= 0)
	{
		return FTransform::Identity;
	}

	const float TableSurfaceZ = ResolveInitialCardTableSurfaceZ();
	const FVector TableCenter = bInitialCardSpatialCacheValid
		? CachedInitialCardTableCenter
		: ResolveSingleTableCenter(GetWorld());

	const FVector HandSlotLocation = HandSlot->GetComponentLocation();
	FVector SlotCenter = HandSlotLocation;
	float ClosestSlotDistanceSquared = TNumericLimits<float>::Max();
	for (const FVector& CandidateSlotCenter : CachedInitialCardFlatSlotCenters)
	{
		const float DistanceSquared = FVector::DistSquared2D(CandidateSlotCenter, HandSlotLocation);
		if (DistanceSquared < ClosestSlotDistanceSquared)
		{
			ClosestSlotDistanceSquared = DistanceSquared;
			SlotCenter = CandidateSlotCenter;
		}
	}

	FVector TowardTableCenter = TableCenter - SlotCenter;
	TowardTableCenter.Z = 0.0f;
	TowardTableCenter = TowardTableCenter.GetSafeNormal();
	if (TowardTableCenter.IsNearlyZero())
	{
		TowardTableCenter = HandSlot->GetForwardVector().GetSafeNormal2D();
	}
	if (TowardTableCenter.IsNearlyZero())
	{
		TowardTableCenter = FVector::ForwardVector;
	}

	const float CardFromCenter = static_cast<float>(CardIndex) - static_cast<float>(CardCount - 1) * 0.5f;
	const FVector CardRight = FVector::CrossProduct(FVector::UpVector, TowardTableCenter).GetSafeNormal();
	const float OverlapStep = FMath::Clamp(InitialDealFlatCardSpacing, 0.0f, 4.0f);
	FVector Location = SlotCenter + CardRight * (CardFromCenter * OverlapStep);
	Location.Z = TableSurfaceZ + 0.05f + CardIndex * 0.02f;
	return FTransform(BuildInitialFlatCardRotation(TowardTableCenter, true), Location);
}

FQuat AShowDownGameModeBase::BuildInitialFlatCardRotation(const FVector& TowardTableCenter, bool bFaceDown) const
{
	FVector CardLongAxis = TowardTableCenter.GetSafeNormal2D();
	if (CardLongAxis.IsNearlyZero())
	{
		CardLongAxis = FVector::ForwardVector;
	}
	// BP_Card's authored front normal is actor -X (after the mesh component's
	// relative rotation), so face-up places -X toward world up.
	const FQuat FaceUpRotation = FRotationMatrix::MakeFromXZ(-FVector::UpVector, CardLongAxis).ToQuat();
	return bFaceDown
		? (FaceUpRotation * FQuat(FVector::UpVector, PI)).GetNormalized()
		: FaceUpRotation;
}

FVector AShowDownGameModeBase::ResolveInitialCardDeckTop() const
{
	if (bInitialCardDeckBoundsCacheValid)
	{
		return CachedInitialCardDeckTop;
	}

	const FVector TableCenter = bInitialCardSpatialCacheValid
		? CachedInitialCardTableCenter
		: ResolveSingleTableCenter(GetWorld());
	AStaticMeshActor* TaggedDeckActor = nullptr;
	AStaticMeshActor* AuthoredDeckActor = nullptr;
	float TaggedDeckDistanceSquared = TNumericLimits<float>::Max();
	float AuthoredDeckDistanceSquared = TNumericLimits<float>::Max();
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			AStaticMeshActor* MeshActor = *It;
			const UStaticMeshComponent* MeshComponent = It->GetStaticMeshComponent();
			const UStaticMesh* StaticMesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr;
			if (!MeshActor || !StaticMesh)
			{
				continue;
			}

			const float DistanceSquared = FVector::DistSquared2D(MeshComponent->Bounds.Origin, TableCenter);
			if (!InitialDealDeckSourceActorTag.IsNone()
				&& MeshActor->ActorHasTag(InitialDealDeckSourceActorTag)
				&& DistanceSquared < TaggedDeckDistanceSquared)
			{
				TaggedDeckActor = MeshActor;
				TaggedDeckDistanceSquared = DistanceSquared;
			}
			if (StaticMesh->GetPathName() == TEXT("/Game/Fab/Card/SM_carddummyMesh.SM_carddummyMesh")
				&& DistanceSquared < AuthoredDeckDistanceSquared)
			{
				AuthoredDeckActor = MeshActor;
				AuthoredDeckDistanceSquared = DistanceSquared;
			}
		}
	}

	AStaticMeshActor* DeckActor = TaggedDeckActor ? TaggedDeckActor : AuthoredDeckActor;
	if (DeckActor && DeckActor->GetStaticMeshComponent())
	{
		const FBoxSphereBounds Bounds = DeckActor->GetStaticMeshComponent()->Bounds;
		CachedInitialCardDeckTop = FVector(
			Bounds.Origin.X,
			Bounds.Origin.Y,
			Bounds.Origin.Z + Bounds.BoxExtent.Z);
		CachedInitialCardDeckBottomZ = Bounds.Origin.Z - Bounds.BoxExtent.Z;
		bInitialCardDeckBoundsCacheValid = true;
		UE_LOG(LogTemp, Log, TEXT("Initial deal cards use placed deck source %s%s."),
			*DeckActor->GetName(),
			TaggedDeckActor ? TEXT(" (tagged)") : TEXT(" (authored mesh fallback)"));
		return CachedInitialCardDeckTop;
	}

	CachedInitialCardDeckTop = TableCenter;
	CachedInitialCardDeckBottomZ = TableCenter.Z;
	bInitialCardDeckBoundsCacheValid = true;
	UE_LOG(LogTemp, Warning, TEXT("No placed initial-deal deck source was found; using the table centre fallback."));
	return CachedInitialCardDeckTop;
}

float AShowDownGameModeBase::ResolveInitialCardTableSurfaceZ() const
{
	if (bInitialCardTableSurfaceCacheValid)
	{
		return CachedInitialCardTableSurfaceZ;
	}

	const FVector ApproximateTableCenter = bInitialCardSpatialCacheValid
		? CachedInitialCardTableCenter
		: ResolveSingleTableCenter(GetWorld());
	const UStaticMeshComponent* CardDecComponent = nullptr;
	float CardDecDistanceSquared = TNumericLimits<float>::Max();
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			const UStaticMeshComponent* MeshComponent = It->GetStaticMeshComponent();
			const UStaticMesh* StaticMesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr;
			if (!StaticMesh || StaticMesh->GetPathName() != TEXT("/Game/Fab/Table/CardDec.CardDec"))
			{
				continue;
			}

			const float DistanceSquared = FVector::DistSquared2D(MeshComponent->Bounds.Origin, ApproximateTableCenter);
			if (DistanceSquared < CardDecDistanceSquared)
			{
				CardDecComponent = MeshComponent;
				CardDecDistanceSquared = DistanceSquared;
			}
		}
	}

	if (CardDecComponent)
	{
		const FBoxSphereBounds Bounds = CardDecComponent->Bounds;
		CachedInitialCardTableSurfaceZ = Bounds.Origin.Z + Bounds.BoxExtent.Z;
		CachedInitialCardShowcasePadRadius = FMath::Min(Bounds.BoxExtent.X, Bounds.BoxExtent.Y);
		CachedInitialCardTableCenter.X = Bounds.Origin.X;
		CachedInitialCardTableCenter.Y = Bounds.Origin.Y;
		CachedInitialCardTableCenter.Z = CachedInitialCardTableSurfaceZ;

		// These are the four white outlined slot centres authored into CardDec's
		// Line mesh. Transforming the local points keeps them exact when the table
		// is moved or uniformly scaled in the level.
		static const FVector LocalFlatSlotCenters[] = {
			FVector(-59.971f, 0.034f, 127.354f),
			FVector(59.971f, 0.034f, 127.354f),
			FVector(0.000f, 60.005f, 127.354f),
			FVector(0.000f, -59.937f, 127.354f)
		};
		CachedInitialCardFlatSlotCenters.Reset(UE_ARRAY_COUNT(LocalFlatSlotCenters));
		const FTransform CardDecTransform = CardDecComponent->GetComponentTransform();
		for (const FVector& LocalSlotCenter : LocalFlatSlotCenters)
		{
			CachedInitialCardFlatSlotCenters.Add(CardDecTransform.TransformPosition(LocalSlotCenter));
		}
	}
	else
	{
		if (!bInitialCardDeckBoundsCacheValid)
		{
			ResolveInitialCardDeckTop();
		}
		CachedInitialCardTableSurfaceZ = bInitialCardDeckBoundsCacheValid
			? CachedInitialCardDeckBottomZ
			: ApproximateTableCenter.Z;
		CachedInitialCardShowcasePadRadius = 0.0f;
		CachedInitialCardFlatSlotCenters.Reset();
	}

	bInitialCardTableSurfaceCacheValid = true;
	return CachedInitialCardTableSurfaceZ;
}

void AShowDownGameModeBase::FindCollector()
{
	Collector = Cast<ACollector>(
		UGameplayStatics::GetActorOfClass(GetWorld(), ACollector::StaticClass())
	);

	if (!Collector)
	{
		if (!GetHandAnchorForSide(EShowDownSide::Collector)
			&& !GetForeheadAnchorForSide(EShowDownSide::Collector)
			&& !GetSeatForSide(EShowDownSide::Collector))
		{
			UE_LOG(LogTemp, Warning, TEXT("No opponent placement found. Add OpponentHand/OpponentForehead SDCardPlacementAnchor actors, or keep an old fallback slot."));
		}
	}
}

void AShowDownGameModeBase::PlayCollectorActionPresentation()
{
	PlayCollectorActionPresentationThen(TFunction<void()>());
}

void AShowDownGameModeBase::WaitForCardPlacementThen(ACard* Card, TFunction<void()>&& Continuation)
{
	CardPlacementDelayContinuation = MoveTemp(Continuation);

	const float PlacementDelay = IsValid(Card) ? Card->GetSlotAttachMotionTotalSeconds() : 0.0f;
	if (PlacementDelay <= KINDA_SMALL_NUMBER)
	{
		FinishCardPlacementWait();
		return;
	}

	GetWorldTimerManager().ClearTimer(CardPlacementDelayHandle);
	GetWorldTimerManager().SetTimer(
		CardPlacementDelayHandle,
		this,
		&AShowDownGameModeBase::FinishCardPlacementWait,
		PlacementDelay,
		false);
}

void AShowDownGameModeBase::FinishCardPlacementWait()
{
	GetWorldTimerManager().ClearTimer(CardPlacementDelayHandle);

	TFunction<void()> Continuation = MoveTemp(CardPlacementDelayContinuation);
	CardPlacementDelayContinuation = TFunction<void()>();
	if (Continuation)
	{
		Continuation();
	}
}

void AShowDownGameModeBase::PlayCollectorActionPresentationThen(TFunction<void()>&& Continuation)
{
	if (bCollectorActionPresentationInProgress || bCollectorBetDecisionInProgress)
	{
		QueuedCollectorActionPresentationContinuations.Add(MoveTemp(Continuation));
		return;
	}

	if (!Collector)
	{
		FindCollector();
	}

	if (!Collector || !Collector->bEnableActionSpin)
	{
		if (Continuation)
		{
			Continuation();
		}
		return;
	}

	bCollectorActionPresentationInProgress = true;
	CollectorActionPresentationContinuation = MoveTemp(Continuation);
	RefreshBetActionPanel();

	Collector->PlayActionSpin();

	const float PresentationSeconds = Collector->GetActionSpinTotalSeconds();
	if (PresentationSeconds <= KINDA_SMALL_NUMBER)
	{
		FinishCollectorActionPresentation();
		return;
	}

	GetWorldTimerManager().SetTimer(
		CollectorActionPresentationTimerHandle,
		this,
		&AShowDownGameModeBase::FinishCollectorActionPresentation,
		PresentationSeconds,
		false);
}

void AShowDownGameModeBase::FinishCollectorActionPresentation()
{
	GetWorldTimerManager().ClearTimer(CollectorActionPresentationTimerHandle);

	bCollectorActionPresentationInProgress = false;

	TFunction<void()> Continuation = MoveTemp(CollectorActionPresentationContinuation);
	CollectorActionPresentationContinuation = TFunction<void()>();
	if (Continuation)
	{
		Continuation();
	}
	RefreshBetActionPanel();

	if (!bCollectorActionPresentationInProgress && QueuedCollectorActionPresentationContinuations.Num() > 0)
	{
		TFunction<void()> NextContinuation = MoveTemp(QueuedCollectorActionPresentationContinuations[0]);
		QueuedCollectorActionPresentationContinuations.RemoveAt(0);
		PlayCollectorActionPresentationThen(MoveTemp(NextContinuation));
	}
}

void AShowDownGameModeBase::BroadcastCardSelectedAction(EShowDownSide Side) const
{
	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->OnCardSelected.Broadcast(Side);
	}
}

void AShowDownGameModeBase::BroadcastBetActionCommitted(
	EShowDownSide Side,
	EShowDownBetAction Action,
	int32 TargetBet) const
{
	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->OnBetActionCommitted.Broadcast(Side, Action, TargetBet);
	}

	const FString ActorName = Side == EShowDownSide::Player ? TEXT("Player") : TEXT("Collector");
	switch (Action)
	{
	case EShowDownBetAction::Check:
		BroadcastSystemChatMessage(FString::Printf(TEXT("%s님이 %d발 장전 상태로 체크했습니다."), *ActorName, TargetBet));
		break;
	case EShowDownBetAction::Call:
		BroadcastSystemChatMessage(FString::Printf(TEXT("%s님이 %d발로 콜했습니다."), *ActorName, TargetBet));
		break;
	case EShowDownBetAction::Raise:
		BroadcastSystemChatMessage(FString::Printf(TEXT("%s님이 %d발 장전했습니다."), *ActorName, TargetBet));
		break;
	case EShowDownBetAction::Fold:
		BroadcastSystemChatMessage(FString::Printf(TEXT("%s님이 폴드했습니다."), *ActorName));
		break;
	default:
		break;
	}
}

void AShowDownGameModeBase::BroadcastMultiplayerCardSelectedAction(ASDPlayerState* Player) const
{
	if (!Player || Player->ShowDownSlot == EShowDownPlayerSlot::None)
	{
		return;
	}

	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->OnMultiplayerCardSelected.Broadcast(Player->ShowDownSlot);
	}
}

void AShowDownGameModeBase::BroadcastMultiplayerBetActionCommitted(
	ASDPlayerState* Player,
	EShowDownBetAction Action,
	int32 TargetBet) const
{
	if (!Player || Player->ShowDownSlot == EShowDownPlayerSlot::None)
	{
		return;
	}

	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->OnMultiplayerBetActionCommitted.Broadcast(Player->ShowDownSlot, Action, TargetBet);
	}
}

void AShowDownGameModeBase::BroadcastSystemChatMessage(const FString& Message) const
{
	const FString TrimmedMessage = Message.TrimStartAndEnd().Left(240);
	if (TrimmedMessage.IsEmpty())
	{
		return;
	}

	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->BroadcastChatMessage(TEXT("System"), TrimmedMessage);
	}
}

void AShowDownGameModeBase::PlaySelfShotGunPresentationThen(
	EShowDownSide TargetSide,
	bool bLiveRound,
	TFunction<void()>&& ResultContinuation,
	TFunction<void()>&& PresentationContinuation)
{
	auto ResolveWithoutGun = [this, TargetSide, bLiveRound, &ResultContinuation, &PresentationContinuation]() mutable
	{
		if (bSelfShotGunPresentationInProgress)
		{
			if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
			{
				ShowDownGameState->OnRouletteResult.Broadcast(TargetSide, bLiveRound);
			}
			if (ResultContinuation)
			{
				ResultContinuation();
			}
			PlayCollectorActionPresentationThen(MoveTemp(PresentationContinuation));
			return;
		}

		bSelfShotGunPresentationInProgress = true;
		ActiveSelfShotGunActor = nullptr;
		SelfShotGunResultContinuation = MoveTemp(ResultContinuation);
		SelfShotGunPresentationContinuation = [this, Continuation = MoveTemp(PresentationContinuation)]() mutable
		{
			PlayCollectorActionPresentationThen(MoveTemp(Continuation));
		};
		bPendingSelfShotRouletteResult = true;
		bPendingSelfShotLiveRound = bLiveRound;
		PendingSelfShotTargetSide = TargetSide;
		ResolvePendingSelfShotGunResult();
		FinishSelfShotGunPresentation();
	};

	if (GetNetMode() != NM_Standalone)
	{
		ResolveWithoutGun();
		return;
	}

	if (bSelfShotGunPresentationInProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("Self shot gun presentation is already running. Falling back to collector presentation."));
		ResolveWithoutGun();
		return;
	}

	ASDSelfShotGunActor* GunActor = FindSelfShotGunActor();
	if (!GunActor || !GunActor->CanInteract_Implementation(nullptr))
	{
		UE_LOG(LogTemp, Warning, TEXT("Self shot gun actor is missing or busy. Falling back to collector presentation."));
		ResolveWithoutGun();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Playing self shot gun presentation. Target=%s Result=%s"),
		TargetSide == EShowDownSide::Player ? TEXT("Player") : TEXT("Collector"),
		bLiveRound ? TEXT("Live") : TEXT("Empty"));

	bSelfShotGunPresentationInProgress = true;
	GetWorldTimerManager().ClearTimer(SelfShotHitRecoveryWaitTimerHandle);
	ActiveSelfShotGunActor = GunActor;
	SelfShotGunResultContinuation = MoveTemp(ResultContinuation);
	SelfShotGunPresentationContinuation = MoveTemp(PresentationContinuation);
	bPendingSelfShotRouletteResult = true;
	bPendingSelfShotLiveRound = bLiveRound;
	PendingSelfShotTargetSide = TargetSide;
	GunActor->OnGunPresentationFinished.AddUniqueDynamic(
		this,
		&AShowDownGameModeBase::HandleSelfShotGunPresentationFinished);
	GunActor->OnGunFired.AddUniqueDynamic(
		this,
		&AShowDownGameModeBase::HandleSelfShotGunShotResolved);
	GunActor->OnGunEmptyFired.AddUniqueDynamic(
		this,
		&AShowDownGameModeBase::HandleSelfShotGunShotResolved);
	AActor* ShotTargetActor = nullptr;
	bool bHasShotSourceLocation = false;
	bool bHasShotAimLocation = false;
	bool bHasShotRotationOffset = false;
	FVector ShotSourceLocation = FVector::ZeroVector;
	FVector ShotAimLocation = FVector::ZeroVector;
	FRotator ShotRotationOffset = FRotator::ZeroRotator;
	if (AShowDownCharacter* TargetCharacter = FindSingleRouletteCharacter(TargetSide))
	{
		if (GunActor->TryResolveCharacterPresentationShot(
			TargetCharacter,
			ShotSourceLocation,
			ShotAimLocation,
			&ShotRotationOffset))
		{
			ShotTargetActor = TargetCharacter;
			bHasShotSourceLocation = true;
			bHasShotAimLocation = true;
			bHasShotRotationOffset = true;
		}
	}

	if (!bHasShotSourceLocation && TargetSide == EShowDownSide::Collector)
	{
		ShotTargetActor = Collector
			? Collector
			: UGameplayStatics::GetActorOfClass(this, ACollector::StaticClass());

		USceneComponent* CollectorHeadSlot = GetHeadSlotForSide(EShowDownSide::Collector);
		if (IsValid(CollectorState.ForeheadCard))
		{
			ShotSourceLocation = CollectorState.ForeheadCard->GetActorLocation();
			bHasShotSourceLocation = true;
		}
		else if (CollectorHeadSlot)
		{
			ShotSourceLocation = CollectorHeadSlot->GetComponentLocation();
			bHasShotSourceLocation = true;
		}

		if (bHasShotSourceLocation)
		{
			ShotAimLocation = ShotSourceLocation;
			bHasShotAimLocation = true;

			FVector SourcePullDirection = FVector::ZeroVector;
			if (SourcePullDirection.IsNearlyZero() && CollectorHeadSlot)
			{
				SourcePullDirection = CollectorHeadSlot->GetForwardVector().GetSafeNormal();
			}
			if (SourcePullDirection.IsNearlyZero())
			{
				SourcePullDirection = FVector::ForwardVector;
			}

			ShotSourceLocation -= SourcePullDirection * GunActor->GetTargetShotSourcePullDistance();
		}
	}

	if (bHasShotSourceLocation && bHasShotAimLocation)
	{
		if (bHasShotRotationOffset)
		{
			GunActor->UseGunWithForcedResultAtTargetFromLocationAimRotationAndCamera(
				bLiveRound,
				ShotTargetActor,
				ShotSourceLocation,
				ShotAimLocation,
				ShotRotationOffset,
				nullptr);
		}
		else
		{
			GunActor->UseGunWithForcedResultAtTargetFromLocationAimAndCamera(
				bLiveRound,
				ShotTargetActor,
				ShotSourceLocation,
				ShotAimLocation,
				nullptr);
		}
	}
	else if (bHasShotSourceLocation)
	{
		GunActor->UseGunWithForcedResultAtTargetFromLocationAndCamera(
			bLiveRound,
			ShotTargetActor,
			ShotSourceLocation,
			nullptr);
	}
	else
	{
		GunActor->UseGunWithForcedResultAtTarget(bLiveRound, ShotTargetActor);
	}
}

void AShowDownGameModeBase::HandleSelfShotGunPresentationFinished()
{
	FinishSelfShotGunPresentation();
}

void AShowDownGameModeBase::HandleSelfShotGunShotResolved()
{
	ResolvePendingSelfShotGunResult();
}

void AShowDownGameModeBase::FinishSelfShotGunPresentation()
{
	// Resolve an interrupted gun first so the character recovery sequence exists
	// before deciding whether the round may continue.
	ResolvePendingSelfShotGunResult();
	if (bPendingSelfShotLiveRound)
	{
		if (AShowDownCharacter* TargetCharacter = FindSingleRouletteCharacter(PendingSelfShotTargetSide))
		{
			const float RemainingRecoveryTime = TargetCharacter->GetHitRecoveryPresentationRemainingTime();
			if (RemainingRecoveryTime > KINDA_SMALL_NUMBER)
			{
				GetWorldTimerManager().SetTimer(
					SelfShotHitRecoveryWaitTimerHandle,
					this,
					&AShowDownGameModeBase::FinishSelfShotGunPresentation,
					RemainingRecoveryTime + 0.01f,
					false);
				return;
			}
		}
	}
	GetWorldTimerManager().ClearTimer(SelfShotHitRecoveryWaitTimerHandle);

	if (ActiveSelfShotGunActor)
	{
		ActiveSelfShotGunActor->OnGunPresentationFinished.RemoveDynamic(
			this,
			&AShowDownGameModeBase::HandleSelfShotGunPresentationFinished);
		ActiveSelfShotGunActor->OnGunFired.RemoveDynamic(
			this,
			&AShowDownGameModeBase::HandleSelfShotGunShotResolved);
		ActiveSelfShotGunActor->OnGunEmptyFired.RemoveDynamic(
			this,
			&AShowDownGameModeBase::HandleSelfShotGunShotResolved);
	}

	bSelfShotGunPresentationInProgress = false;
	ActiveSelfShotGunActor = nullptr;
	bPendingSelfShotLiveRound = false;

	TFunction<void()> Continuation = MoveTemp(SelfShotGunPresentationContinuation);
	SelfShotGunPresentationContinuation = TFunction<void()>();
	if (Continuation)
	{
		Continuation();
	}
}

void AShowDownGameModeBase::ResolvePendingSelfShotGunResult()
{
	BroadcastPendingSelfShotRouletteResult();
	TFunction<void()> ResultContinuation = MoveTemp(SelfShotGunResultContinuation);
	SelfShotGunResultContinuation = TFunction<void()>();
	if (ResultContinuation)
	{
		ResultContinuation();
	}
}

void AShowDownGameModeBase::BroadcastPendingSelfShotRouletteResult()
{
	if (!bPendingSelfShotRouletteResult)
	{
		return;
	}

	bPendingSelfShotRouletteResult = false;
	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->OnRouletteResult.Broadcast(PendingSelfShotTargetSide, bPendingSelfShotLiveRound);
	}
}

ASDSelfShotGunActor* AShowDownGameModeBase::FindSelfShotGunActor() const
{
	return Cast<ASDSelfShotGunActor>(UGameplayStatics::GetActorOfClass(this, ASDSelfShotGunActor::StaticClass()));
}

AShowDownCharacter* AShowDownGameModeBase::FindSingleRouletteCharacter(EShowDownSide TargetSide) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	AShowDownCharacter* LocalPlayerFallback = nullptr;
	AShowDownCharacter* PlayerRoleFallback = nullptr;
	for (TActorIterator<AShowDownCharacter> It(World); It; ++It)
	{
		AShowDownCharacter* CandidateCharacter = *It;
		if (!IsValid(CandidateCharacter) || !CandidateCharacter->IsCharacterSceneActive())
		{
			continue;
		}

		if (TargetSide == EShowDownSide::Player)
		{
			if (CandidateCharacter->IsLocalPlayerCharacter())
			{
				LocalPlayerFallback = CandidateCharacter;
			}

			if (CandidateCharacter->GetCharacterRole() == EShowDownCharacterRole::Player)
			{
				if (CandidateCharacter->GetPlayerSlot() == EShowDownPlayerSlot::Player1)
				{
					return CandidateCharacter;
				}
				if (!PlayerRoleFallback)
				{
					PlayerRoleFallback = CandidateCharacter;
				}
			}
		}
		else if (TargetSide == EShowDownSide::Collector
			&& CandidateCharacter->GetCharacterRole() == EShowDownCharacterRole::Opponent)
		{
			return CandidateCharacter;
		}
	}

	if (TargetSide == EShowDownSide::Player)
	{
		return LocalPlayerFallback ? LocalPlayerFallback : PlayerRoleFallback;
	}

	return nullptr;
}

void AShowDownGameModeBase::PlaySinglePlayerIntroThenStartStage()
{
	if (!bPlaySinglePlayerIntro || !HasAuthority())
	{
		StartStage(0);
		return;
	}

	AShowDownCharacter* PlayerCharacter = FindSingleRouletteCharacter(EShowDownSide::Player);
	AShowDownCharacter* CollectorCharacter = FindSingleRouletteCharacter(EShowDownSide::Collector);
	SinglePlayerIntroPlayerCharacter = PlayerCharacter;
	SinglePlayerIntroCollectorCharacter = CollectorCharacter;
	bSinglePlayerIntroFallbackActive = false;

	if (PlaySinglePlayerIntroSequence(PlayerCharacter, CollectorCharacter))
	{
		return;
	}

	if (PlaySinglePlayerIntroFallback(PlayerCharacter, CollectorCharacter))
	{
		return;
	}

	StartStage(0);
}

bool AShowDownGameModeBase::PlaySinglePlayerIntroSequence(
	AShowDownCharacter* PlayerCharacter,
	AShowDownCharacter* CollectorCharacter)
{
	if (!SinglePlayerIntroSequence || !GetWorld())
	{
		return false;
	}

	FMovieSceneSequencePlaybackSettings PlaybackSettings;
	PlaybackSettings.bDisableMovementInput = true;
	PlaybackSettings.bDisableLookAtInput = true;
	PlaybackSettings.FinishCompletionStateOverride = EMovieSceneCompletionModeOverride::ForceKeepState;

	ALevelSequenceActor* SequenceActor = nullptr;
	ULevelSequencePlayer* SequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(
		this,
		SinglePlayerIntroSequence,
		PlaybackSettings,
		SequenceActor);

	if (!SequencePlayer || !SequenceActor)
	{
		return false;
	}

	if (PlayerCharacter && !SinglePlayerIntroPlayerBindingTag.IsNone())
	{
		TArray<AActor*> BoundActors;
		BoundActors.Add(PlayerCharacter);
		SequenceActor->SetBindingByTag(SinglePlayerIntroPlayerBindingTag, BoundActors, false);
	}

	if (CollectorCharacter && !SinglePlayerIntroCollectorBindingTag.IsNone())
	{
		TArray<AActor*> BoundActors;
		BoundActors.Add(CollectorCharacter);
		SequenceActor->SetBindingByTag(SinglePlayerIntroCollectorBindingTag, BoundActors, false);
	}

	ActiveSinglePlayerIntroSequenceActor = SequenceActor;
	ActiveSinglePlayerIntroSequencePlayer = SequencePlayer;
	bSinglePlayerIntroFallbackActive = false;
	SequencePlayer->OnFinished.AddDynamic(this, &AShowDownGameModeBase::HandleSinglePlayerIntroSequenceFinished);
	SequencePlayer->Play();

	UE_LOG(LogTemp, Log, TEXT("Single-player intro sequence started: %s"), *SinglePlayerIntroSequence->GetName());
	return true;
}

bool AShowDownGameModeBase::PlaySinglePlayerIntroFallback(
	AShowDownCharacter* PlayerCharacter,
	AShowDownCharacter* CollectorCharacter)
{
	if (!bUseSinglePlayerIntroFallbackWhenNoSequence
		|| SinglePlayerIntroFallbackDuration <= KINDA_SMALL_NUMBER
		|| SinglePlayerIntroFallbackStartDistance <= KINDA_SMALL_NUMBER
		|| (!PlayerCharacter && !CollectorCharacter)
		|| !GetWorld())
	{
		return false;
	}

	const FVector TableCenter = ResolveSingleTableCenter(GetWorld());
	auto BuildStartTransform = [this, TableCenter](const FTransform& TargetTransform)
	{
		FVector Direction = TargetTransform.GetLocation() - TableCenter;
		Direction.Z = 0.0f;
		if (!Direction.Normalize())
		{
			Direction = -TargetTransform.GetRotation().GetForwardVector();
			Direction.Z = 0.0f;
			if (!Direction.Normalize())
			{
				Direction = FVector::ForwardVector;
			}
		}

		FTransform StartTransform = TargetTransform;
		StartTransform.SetLocation(TargetTransform.GetLocation() + Direction * SinglePlayerIntroFallbackStartDistance);
		return StartTransform;
	};

	if (PlayerCharacter)
	{
		SinglePlayerIntroPlayerTargetTransform = PlayerCharacter->GetActorTransform();
		SinglePlayerIntroPlayerStartTransform = BuildStartTransform(SinglePlayerIntroPlayerTargetTransform);
		PlayerCharacter->SetActorTransform(SinglePlayerIntroPlayerStartTransform);
	}

	if (CollectorCharacter)
	{
		SinglePlayerIntroCollectorTargetTransform = CollectorCharacter->GetActorTransform();
		SinglePlayerIntroCollectorStartTransform = BuildStartTransform(SinglePlayerIntroCollectorTargetTransform);
		CollectorCharacter->SetActorTransform(SinglePlayerIntroCollectorStartTransform);
	}

	bSinglePlayerIntroFallbackActive = true;
	SinglePlayerIntroFallbackStartTime = GetWorld()->GetTimeSeconds();
	GetWorldTimerManager().SetTimer(
		SinglePlayerIntroFallbackTimerHandle,
		this,
		&AShowDownGameModeBase::UpdateSinglePlayerIntroFallback,
		1.0f / 60.0f,
		true);

	UE_LOG(LogTemp, Log, TEXT("Single-player intro fallback started."));
	return true;
}

void AShowDownGameModeBase::UpdateSinglePlayerIntroFallback()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		FinishSinglePlayerIntro();
		return;
	}

	const float Duration = FMath::Max(KINDA_SMALL_NUMBER, SinglePlayerIntroFallbackDuration);
	const float Alpha = FMath::Clamp((World->GetTimeSeconds() - SinglePlayerIntroFallbackStartTime) / Duration, 0.0f, 1.0f);
	const float EasedAlpha = FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 2.0f);
	auto LerpActorTransform = [](AActor* Actor, const FTransform& From, const FTransform& To, float TransformAlpha)
	{
		if (!Actor)
		{
			return;
		}

		const FVector Location = FMath::Lerp(From.GetLocation(), To.GetLocation(), TransformAlpha);
		const FQuat Rotation = FQuat::Slerp(From.GetRotation(), To.GetRotation(), TransformAlpha);
		const FVector Scale = FMath::Lerp(From.GetScale3D(), To.GetScale3D(), TransformAlpha);
		Actor->SetActorTransform(FTransform(Rotation, Location, Scale));
	};

	if (SinglePlayerIntroPlayerCharacter)
	{
		LerpActorTransform(
			SinglePlayerIntroPlayerCharacter,
			SinglePlayerIntroPlayerStartTransform,
			SinglePlayerIntroPlayerTargetTransform,
			EasedAlpha);
	}

	if (SinglePlayerIntroCollectorCharacter)
	{
		LerpActorTransform(
			SinglePlayerIntroCollectorCharacter,
			SinglePlayerIntroCollectorStartTransform,
			SinglePlayerIntroCollectorTargetTransform,
			EasedAlpha);
	}

	if (Alpha >= 1.0f)
	{
		FinishSinglePlayerIntro();
	}
}

void AShowDownGameModeBase::HandleSinglePlayerIntroSequenceFinished()
{
	FinishSinglePlayerIntro();
}

void AShowDownGameModeBase::FinishSinglePlayerIntro()
{
	GetWorldTimerManager().ClearTimer(SinglePlayerIntroFallbackTimerHandle);

	if (bSinglePlayerIntroFallbackActive && SinglePlayerIntroPlayerCharacter)
	{
		SinglePlayerIntroPlayerCharacter->SetActorTransform(SinglePlayerIntroPlayerTargetTransform);
	}
	if (bSinglePlayerIntroFallbackActive && SinglePlayerIntroCollectorCharacter)
	{
		SinglePlayerIntroCollectorCharacter->SetActorTransform(SinglePlayerIntroCollectorTargetTransform);
	}

	if (ActiveSinglePlayerIntroSequencePlayer)
	{
		ActiveSinglePlayerIntroSequencePlayer->OnFinished.RemoveDynamic(
			this,
			&AShowDownGameModeBase::HandleSinglePlayerIntroSequenceFinished);
	}

	if (ActiveSinglePlayerIntroSequenceActor)
	{
		ActiveSinglePlayerIntroSequenceActor->Destroy();
	}

	ActiveSinglePlayerIntroSequencePlayer = nullptr;
	ActiveSinglePlayerIntroSequenceActor = nullptr;
	SinglePlayerIntroPlayerCharacter = nullptr;
	SinglePlayerIntroCollectorCharacter = nullptr;
	bSinglePlayerIntroFallbackActive = false;

	StartStage(0);
}

TArray<AShowDownCharacter*> AShowDownGameModeBase::GetShowDownCharacters() const
{
	TArray<AShowDownCharacter*> Characters;
	UWorld* World = GetWorld();
	if (!World)
	{
		return Characters;
	}

	for (TActorIterator<AShowDownCharacter> It(World); It; ++It)
	{
		AShowDownCharacter* Character = *It;
		if (IsValid(Character))
		{
			Characters.Add(Character);
		}
	}

	Characters.Sort([](const AShowDownCharacter& Left, const AShowDownCharacter& Right)
	{
		return Left.GetName().Compare(Right.GetName()) < 0;
	});

	return Characters;
}

void AShowDownGameModeBase::ConfigureSinglePlayerCharacters()
{
	if (!HasAuthority())
	{
		return;
	}

	TArray<AShowDownCharacter*> Characters = GetShowDownCharacters();
	if (Characters.Num() <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("No ShowDownCharacter actors found for single-player character setup."));
		return;
	}

	AShowDownCharacter* PlayerCharacter = nullptr;
	AShowDownCharacter* PlayerRoleFallback = nullptr;
	AShowDownCharacter* AnyCharacterFallback = nullptr;
	for (AShowDownCharacter* Character : Characters)
	{
		if (!IsValid(Character))
		{
			continue;
		}

		if (!AnyCharacterFallback)
		{
			AnyCharacterFallback = Character;
		}

		if (Character->GetCharacterRole() == EShowDownCharacterRole::Player)
		{
			if (Character->GetPlayerSlot() == EShowDownPlayerSlot::Player1)
			{
				PlayerCharacter = Character;
				break;
			}

			if (!PlayerRoleFallback)
			{
				PlayerRoleFallback = Character;
			}
		}
	}

	if (!PlayerCharacter)
	{
		PlayerCharacter = PlayerRoleFallback ? PlayerRoleFallback : AnyCharacterFallback;
	}

	AShowDownCharacter* OpponentCharacter = nullptr;
	AShowDownCharacter* PlayerTwoFallback = nullptr;
	AShowDownCharacter* OtherCharacterFallback = nullptr;
	for (AShowDownCharacter* Character : Characters)
	{
		if (!IsValid(Character) || Character == PlayerCharacter)
		{
			continue;
		}

		if (!OtherCharacterFallback)
		{
			OtherCharacterFallback = Character;
		}

		if (Character->GetCharacterRole() == EShowDownCharacterRole::Opponent)
		{
			OpponentCharacter = Character;
			break;
		}

		if (!PlayerTwoFallback && Character->GetPlayerSlot() == EShowDownPlayerSlot::Player2)
		{
			PlayerTwoFallback = Character;
		}
	}

	if (!OpponentCharacter)
	{
		OpponentCharacter = PlayerTwoFallback ? PlayerTwoFallback : OtherCharacterFallback;
	}

	for (AShowDownCharacter* Character : Characters)
	{
		if (!IsValid(Character))
		{
			continue;
		}

		const bool bActive = Character == PlayerCharacter || Character == OpponentCharacter;
		Character->SetCharacterSceneActive(bActive);
	}

	if (PlayerCharacter)
	{
		FString PlayerSkinId = AShowDownCharacter::GetDefaultCharacterSkinId();
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (const USupabaseSubsystem* SupabaseSubsystem = GameInstance->GetSubsystem<USupabaseSubsystem>())
			{
				const FString EquippedSkinId = SupabaseSubsystem->GetEquippedSkinId(TEXT("character"));
				if (!EquippedSkinId.TrimStartAndEnd().IsEmpty())
				{
					PlayerSkinId = EquippedSkinId;
				}
			}
		}

		PlayerCharacter->SetCharacterIdentity(
			EShowDownCharacterRole::Player,
			EShowDownPlayerSlot::Player1,
			TEXT("Player"));
		PlayerCharacter->SetCharacterSkinId(PlayerSkinId);
		PlayerCharacter->SetCharacterLives(PlayerState.Lives);
		PlayerCharacter->SetCharacterSceneActive(true);
	}

	if (OpponentCharacter)
	{
		FString OpponentDisplayName = TEXT("상대");
		GConfig->GetString(TEXT("ShowDown.UserSettings"), TEXT("CharacterName"), OpponentDisplayName, GGameUserSettingsIni);
		OpponentDisplayName = OpponentDisplayName.TrimStartAndEnd();
		if (OpponentDisplayName.IsEmpty()) OpponentDisplayName = TEXT("상대");
		OpponentCharacter->SetCharacterIdentity(
			EShowDownCharacterRole::Opponent,
			EShowDownPlayerSlot::None,
			OpponentDisplayName);
		OpponentCharacter->SetCharacterLives(CollectorState.Lives);
		OpponentCharacter->SetCharacterSceneActive(true);
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("Single-player characters configured. Player=%s Opponent=%s Hidden=%d"),
		PlayerCharacter ? *PlayerCharacter->GetName() : TEXT("None"),
		OpponentCharacter ? *OpponentCharacter->GetName() : TEXT("None"),
		FMath::Max(0, Characters.Num() - (PlayerCharacter ? 1 : 0) - (OpponentCharacter ? 1 : 0)));
}

void AShowDownGameModeBase::ConfigureMultiplayerCharacters(const TArray<ASDPlayerState*>& Players)
{
	if (!HasAuthority())
	{
		return;
	}

	TArray<AShowDownCharacter*> Characters = GetShowDownCharacters();
	if (Characters.Num() <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("No ShowDownCharacter actors found for multiplayer character setup."));
		return;
	}

	TArray<ASDPlayerState*> SortedPlayers = Players;
	SortedPlayers.RemoveAll([](const ASDPlayerState* Player)
	{
		return !Player || Player->ShowDownSlot == EShowDownPlayerSlot::None;
	});
	SortedPlayers.Sort(SortByMultiplayerTurnOrder);

	TSet<AShowDownCharacter*> AssignedCharacters;
	for (ASDPlayerState* Player : SortedPlayers)
	{
		if (!Player)
		{
			continue;
		}

		AShowDownCharacter* AssignedCharacter = nullptr;
		for (AShowDownCharacter* Character : Characters)
		{
			if (IsValid(Character)
				&& !AssignedCharacters.Contains(Character)
				&& Character->GetPlayerSlot() == Player->ShowDownSlot)
			{
				AssignedCharacter = Character;
				break;
			}
		}

		if (!AssignedCharacter)
		{
			for (AShowDownCharacter* Character : Characters)
			{
				if (IsValid(Character)
					&& !AssignedCharacters.Contains(Character)
					&& Character->GetCharacterRole() == EShowDownCharacterRole::Player)
				{
					AssignedCharacter = Character;
					break;
				}
			}
		}

		if (!AssignedCharacter)
		{
			for (AShowDownCharacter* Character : Characters)
			{
				if (IsValid(Character) && !AssignedCharacters.Contains(Character))
				{
					AssignedCharacter = Character;
					break;
				}
			}
		}

		if (!AssignedCharacter)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("Not enough ShowDownCharacter actors for multiplayer slot %d (%s)."),
				static_cast<int32>(Player->ShowDownSlot),
				*Player->GetPlayerName());
			continue;
		}

		AssignedCharacters.Add(AssignedCharacter);
		AssignedCharacter->SetCharacterIdentity(
			EShowDownCharacterRole::Player,
			Player->ShowDownSlot,
			GetNetworkPlayerDisplayName(Player));
		AssignedCharacter->SetCharacterSkinId(Player->GetEquippedCharacterSkinId());
		AssignedCharacter->SetCharacterLives(Player->Lives);
		AssignedCharacter->SetCharacterSceneActive(
			Player->Lives > 0 && !MultiplayerRoundSpectators.Contains(Player));
	}

	for (AShowDownCharacter* Character : Characters)
	{
		if (IsValid(Character) && !AssignedCharacters.Contains(Character))
		{
			Character->SetCharacterSceneActive(false);
		}
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("Multiplayer characters configured. Active=%d Hidden=%d"),
		AssignedCharacters.Num(),
		FMath::Max(0, Characters.Num() - AssignedCharacters.Num()));
}

void AShowDownGameModeBase::RefreshMultiplayerCharacterVisibility()
{
	if (!HasAuthority())
	{
		return;
	}

	TMap<EShowDownPlayerSlot, bool> AliveBySlot;
	for (ASDPlayerState* Player : MultiplayerPlayers)
	{
		if (IsValid(Player) && Player->ShowDownSlot != EShowDownPlayerSlot::None)
		{
			AliveBySlot.Add(
				Player->ShowDownSlot,
				Player->Lives > 0 && !MultiplayerRoundSpectators.Contains(Player));
		}
	}

	for (AShowDownCharacter* Character : GetShowDownCharacters())
	{
		if (!IsValid(Character)
			|| Character->GetCharacterRole() != EShowDownCharacterRole::Player
			|| Character->GetPlayerSlot() == EShowDownPlayerSlot::None)
		{
			continue;
		}

		if (const bool* bAlive = AliveBySlot.Find(Character->GetPlayerSlot()))
		{
			Character->SetCharacterSceneActive(*bAlive);
		}
		else
		{
			Character->SetCharacterSceneActive(false);
		}
	}
}

float AShowDownGameModeBase::ResolveMultiplayerRouletteResultDelay() const
{
	const ASDSelfShotGunActor* GunActor = FindSelfShotGunActor();
	return GunActor ? GunActor->GetShotResolveDelay() : 0.0f;
}

float AShowDownGameModeBase::ResolveMultiplayerRoulettePresentationDelay(bool bLiveRound) const
{
	const ASDSelfShotGunActor* GunActor = FindSelfShotGunActor();
	return GunActor
		? GunActor->GetPresentationFinishDelay(bLiveRound)
		: ResolveMultiplayerRouletteResultDelay();
}

float AShowDownGameModeBase::CalculateRoulettePresentationFinishDelay(
	float ResultDelay,
	float GunPresentationDelay,
	float HitRecoveryDuration)
{
	const float SafeResultDelay = FMath::Max(0.0f, ResultDelay);
	return FMath::Max(
		FMath::Max(0.0f, GunPresentationDelay),
		SafeResultDelay + FMath::Max(0.0f, HitRecoveryDuration));
}

void AShowDownGameModeBase::CollectorGiveCardToPlayer()
{
	if (!CardSystem){
		UE_LOG(LogTemp, Warning, TEXT("CardSystem is missing on %s."), *GetName());
		return;
	}

	if (!CollectorAISystem){
		UE_LOG(LogTemp, Warning, TEXT("CollectorAISystem is missing on %s."), *GetName());
		return;
	}

	if (PlayerState.ForeheadCard){
		UE_LOG(LogTemp, Warning, TEXT("Player already has a forehead card."));
		return;
	}

	USceneComponent* PlayerHeadSlot = GetPlayerHeadSlot();
	if (!PlayerHeadSlot){
		UE_LOG(LogTemp, Warning, TEXT("Player forehead slot is missing. Add an SDCardPlacementAnchor with PlacementRole=PlayerForehead, or keep an old fallback slot."));
		return;
	}

	if (CollectorState.HandCards.Num() <= 0){
		UE_LOG(LogTemp, Warning, TEXT("Collector has no hand cards."));
		return;
	}

	ACard* ChosenCard = CollectorAISystem->ChooseCardActorToGive(CollectorState.HandCards);
	if (!ChosenCard)
	{
		UE_LOG(LogTemp, Warning, TEXT("Collector AI failed to choose a card."));
		return;
	}

	CardSystem->RemoveCardFromHand(CollectorState.HandCards, ChosenCard);
	ReflowHandCards(EShowDownSide::Collector);

	PlayerState.ForeheadCard = ChosenCard;
	CurrentRoundCollectorGaveRank = ChosenCard->Rank;
	RecordCurrentRoundAction(FString::Printf(TEXT("Collector gave Player forehead card rank %d."), CurrentRoundCollectorGaveRank));
	BroadcastCardSelectedAction(EShowDownSide::Collector);

	// 플레이어는 자기 이마 카드를 보면 안 되므로 false
	CardSystem->MoveCardToSlotWithRotationOffset(
		ChosenCard,
		PlayerHeadSlot,
		false,
		GetForeheadCardRotationOffsetForSide(EShowDownSide::Player, PlayerHeadSlot));

	UE_LOG(LogTemp, Log, TEXT("Collector gave card to player: %s, Rank: %d"), *ChosenCard->GetName(), ChosenCard->Rank);

	WaitForCardPlacementThen(ChosenCard, [this]()
	{
		PlayCollectorActionPresentationThen([this]()
		{
			if (CollectorState.ForeheadCard)
			{
				StartBettingPhase();
			}
			else
			{
				SetPlayerHandSelectable(true);
				if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
				{
					ShowDownGameState->SetPhase(EShowDownPhase::SelectCard);
					ShowDownGameState->SetNameTagSingleRoundStatus(0, 0, EShowDownSide::Player, EShowDownPlayerSlot::Player1);
				}
			}
		});
	});
}

void AShowDownGameModeBase::StartBettingPhase()
{
	const FShowDownStageRule* StageRule = GetCurrentStageRule();
	if (!BettingSystem || !StageRule)
	{
		UE_LOG(LogTemp, Warning, TEXT("BettingSystem is missing on %s."), *GetName());
		return;
	}

	bBettingPhase = true;
	PlayerState.CurrentBet = StageRule->MinimumBet;
	CollectorState.CurrentBet = StageRule->MinimumBet;
	BettingRaisesLeft = 6;
	bHasLastRaiser = false;
	bPlayerHasActedInBetting = false;
	bCollectorHasActedInBetting = false;
	bCollectorBetDecisionInProgress = false;
	SetPlayerHandSelectable(false);

	BettingSystem->ResetBetting(StageRule->MinimumBet);
	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->SetPhase(EShowDownPhase::Betting);
		ShowDownGameState->OnBetChanged.Broadcast(EShowDownSide::Player, PlayerState.CurrentBet);
		ShowDownGameState->OnBetChanged.Broadcast(EShowDownSide::Collector, CollectorState.CurrentBet);
		ShowDownGameState->SetNameTagSingleRoundStatus(
			StageRule->MinimumBet,
			StageRule->MinimumBet,
			CurrentRoundFirstSide,
			EShowDownPlayerSlot::Player1);
	}
	ClearBetBulletTransientState();
	ClearBetBulletActionHistory();
	RefreshBetBulletPresentation();
	ShowEventDebugMessage(FString::Printf(TEXT("베팅 시작: 기본 %d발"), StageRule->MinimumBet));

	UE_LOG(LogTemp, Log, TEXT("Betting phase started. Stage %d, MinimumBet %d. Q=Check/Call, 1~5=Raise extra bullets, R=Fold"),
		CurrentStageIndex + 1,
		StageRule->MinimumBet);

	if (CurrentRoundFirstSide == EShowDownSide::Collector)
	{
		ResolveCollectorBetResponse();
	}
}

void AShowDownGameModeBase::PlayerCheck()
{
	if (bCollectorActionPresentationInProgress || bCollectorBetDecisionInProgress)
	{
		return;
	}

	if (!bBettingPhase || !BettingSystem || !CollectorAISystem)
	{
		return;
	}

	const int32 CurrentBet = BettingSystem->GetCurrentBet();
	if (CurrentBet > PlayerState.CurrentBet)
	{
		BettingSystem->Call(EShowDownSide::Player);
		PlayerState.CurrentBet = CurrentBet;
		bPlayerHasActedInBetting = true;
		bBettingPhase = false;
		if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
		{
			ShowDownGameState->OnBetChanged.Broadcast(EShowDownSide::Player, PlayerState.CurrentBet);
			ShowDownGameState->SetNameTagSingleRoundStatus(
				PlayerState.CurrentBet,
				CollectorState.CurrentBet,
				EShowDownSide::Player,
				EShowDownPlayerSlot::None);
		}
		BroadcastBetActionCommitted(EShowDownSide::Player, EShowDownBetAction::Call, PlayerState.CurrentBet);
		RecordSingleBetBulletAction(EShowDownSide::Player, EShowDownBetAction::Call);
		RefreshBetBulletPresentation();
		RecordCurrentRoundAction(FString::Printf(TEXT("Player called to %d."), PlayerState.CurrentBet));
		ShowEventDebugMessage(FString::Printf(TEXT("플레이어 콜: %d발"), PlayerState.CurrentBet));
		UE_LOG(LogTemp, Log, TEXT("Player Call %d"), CurrentBet);
		PlayCollectorActionPresentationThen([this]()
		{
			FinishBettingAndResolveRound();
		});
		return;
	}

	BettingSystem->Check(EShowDownSide::Player);
	PlayerState.CurrentBet = CurrentBet;
	bPlayerHasActedInBetting = true;
	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->OnBetChanged.Broadcast(EShowDownSide::Player, PlayerState.CurrentBet);
	}
	BroadcastBetActionCommitted(EShowDownSide::Player, EShowDownBetAction::Check, PlayerState.CurrentBet);
	RecordSingleBetBulletAction(EShowDownSide::Player, EShowDownBetAction::Check);
	RecordCurrentRoundAction(FString::Printf(TEXT("Player checked at %d."), PlayerState.CurrentBet));
	ShowEventDebugMessage(FString::Printf(TEXT("플레이어 체크: %d발"), PlayerState.CurrentBet));
	UE_LOG(LogTemp, Log, TEXT("Player Check"));

	if (bCollectorHasActedInBetting)
	{
		bBettingPhase = false;
		if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
		{
			ShowDownGameState->SetNameTagSingleRoundStatus(
				PlayerState.CurrentBet,
				CollectorState.CurrentBet,
				EShowDownSide::Player,
				EShowDownPlayerSlot::None);
		}
		RefreshBetBulletPresentation();
		PlayCollectorActionPresentationThen([this]()
		{
			FinishBettingAndResolveRound();
		});
		return;
	}

	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->SetNameTagSingleRoundStatus(
			PlayerState.CurrentBet,
			CollectorState.CurrentBet,
			EShowDownSide::Collector,
			EShowDownPlayerSlot::Player1);
	}
	RefreshBetBulletPresentation();
	PlayCollectorActionPresentationThen([this]()
	{
		ResolveCollectorBetResponse();
	});
}

void AShowDownGameModeBase::PlayerRaise()
{
	if (!BettingSystem)
	{
		return;
	}

	const int32 CurrentBet = BettingSystem->GetCurrentBet();
	PlayerRaiseTo(CurrentBet + 1);
}

void AShowDownGameModeBase::PlayerRaiseTo(int32 BulletCount)
{
	if (bCollectorActionPresentationInProgress || bCollectorBetDecisionInProgress)
	{
		return;
	}

	if (!bBettingPhase || !BettingSystem || !CollectorAISystem)
	{
		return;
	}

	const int32 CurrentBet = BettingSystem->GetCurrentBet();
	const int32 RequestedBet = BulletCount < 0 ? CurrentBet + FMath::Abs(BulletCount) : BulletCount;
	const int32 NewBet = FMath::Clamp(RequestedBet, 1, 6);
	if (BettingSystem->RaiseTo(EShowDownSide::Player, NewBet))
	{
		PlayerState.CurrentBet = NewBet;
		bPlayerHasActedInBetting = true;
		BettingRaisesLeft = FMath::Max(0, BettingRaisesLeft - 1);
		bHasLastRaiser = true;
		LastRaiser = EShowDownSide::Player;
		if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
		{
			ShowDownGameState->OnBetChanged.Broadcast(EShowDownSide::Player, PlayerState.CurrentBet);
			ShowDownGameState->SetNameTagSingleRoundStatus(
				PlayerState.CurrentBet,
				CollectorState.CurrentBet,
				EShowDownSide::Collector,
				EShowDownPlayerSlot::Player1);
		}
		BroadcastBetActionCommitted(EShowDownSide::Player, EShowDownBetAction::Raise, PlayerState.CurrentBet);
		RecordSingleBetBulletAction(EShowDownSide::Player, EShowDownBetAction::Raise);
		RefreshBetBulletPresentation();
		RecordCurrentRoundAction(FString::Printf(TEXT("Player raised to %d."), PlayerState.CurrentBet));
		ShowEventDebugMessage(FString::Printf(TEXT("플레이어 레이즈: %d발"), PlayerState.CurrentBet));
		UE_LOG(LogTemp, Log, TEXT("Player Raise to %d"), NewBet);
		PlayCollectorActionPresentationThen([this]()
		{
			ResolveCollectorBetResponse();
		});
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Player Raise failed. Current bet: %d, requested: %d"),
			BettingSystem->GetCurrentBet(),
			NewBet);
	}
}

void AShowDownGameModeBase::PlayerFold()
{
	if (bCollectorActionPresentationInProgress || bCollectorBetDecisionInProgress)
	{
		return;
	}

	if (!bBettingPhase || !BettingSystem)
	{
		return;
	}

	BettingSystem->Fold(EShowDownSide::Player);
	bPlayerHasActedInBetting = true;

	bBettingPhase = false;
	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->OnBetChanged.Broadcast(EShowDownSide::Player, PlayerState.CurrentBet);
		ShowDownGameState->SetNameTagSingleRoundStatus(
			PlayerState.CurrentBet,
			CollectorState.CurrentBet,
			EShowDownSide::Player,
			EShowDownPlayerSlot::None);
	}
	BroadcastBetActionCommitted(EShowDownSide::Player, EShowDownBetAction::Fold, PlayerState.CurrentBet);
	RecordSingleBetBulletAction(EShowDownSide::Player, EShowDownBetAction::Fold);
	RefreshBetBulletPresentation();
	RecordCurrentRoundAction(FString::Printf(TEXT("Player folded at %d."), PlayerState.CurrentBet));
	ShowEventDebugMessage(FString::Printf(TEXT("플레이어 폴드: %d발"), PlayerState.CurrentBet));
	UE_LOG(LogTemp, Log, TEXT("Player Fold"));
	PlayCollectorActionPresentationThen([this]()
	{
		ResolveFold(EShowDownSide::Player);
	});
}

void AShowDownGameModeBase::RequestPlayerBetAction(EShowDownBetAction Action, int32 TargetBet)
{
	RequestPlayerBetActionFromController(nullptr, Action, TargetBet);
}

void AShowDownGameModeBase::RequestPlayerBetActionFromController(
	AController* SubmittingController,
	EShowDownBetAction Action,
	int32 TargetBet)
{
	if (GetNetMode() != NM_Standalone)
	{
		if (bMultiplayerMatchStarted)
		{
			HandleMultiplayerBetAction(GetPlayerStateForController(SubmittingController), Action, TargetBet);
		}
		return;
	}

	switch (Action)
	{
	case EShowDownBetAction::Check:
	case EShowDownBetAction::Call:
		PlayerCheck();
		break;

	case EShowDownBetAction::Raise:
		if (TargetBet != 0)
		{
			PlayerRaiseTo(TargetBet);
		}
		else
		{
			PlayerRaise();
		}
		break;

	case EShowDownBetAction::Fold:
		PlayerFold();
		break;

	default:
		break;
	}
}

void AShowDownGameModeBase::SubmitPlayerDialogueInput(const FString& PlayerDialogue)
{
	SubmitPlayerDialogueInputFromPlayer(PlayerDialogue, TEXT("Player"));
}

void AShowDownGameModeBase::SubmitPlayerDialogueInputFromPlayer(const FString& PlayerDialogue, const FString& SenderName)
{
	const FString TrimmedDialogue = PlayerDialogue.TrimStartAndEnd().Left(240);
	if (TrimmedDialogue.IsEmpty())
	{
		return;
	}

	const FString SafeSenderName = SenderName.TrimStartAndEnd().IsEmpty()
		? TEXT("Player")
		: SenderName.TrimStartAndEnd().Left(32);
	const FString DisplaySenderName = SafeSenderName.StartsWith(TEXT("DESKTOP-"))
		? TEXT("Player")
		: SafeSenderName;

	LatestPlayerDialogueInput = TrimmedDialogue;
	AppendRecentDialogueLine(DisplaySenderName, TrimmedDialogue);
	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->BroadcastChatMessage(DisplaySenderName, TrimmedDialogue);
	}

	TryRequestBossChatReply(TrimmedDialogue);
}

void AShowDownGameModeBase::TryRequestBossChatReply(const FString& PlayerDialogue, bool bIgnoreCooldown)
{
	const FString TrimmedDialogue = PlayerDialogue.TrimStartAndEnd().Left(240);
	if (TrimmedDialogue.IsEmpty())
	{
		return;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (USDLLMSubsystem* LLMSubsystem = GameInstance->GetSubsystem<USDLLMSubsystem>())
		{
			if (!LLMSubsystem->bEnableInstantBossChatReply || !LLMSubsystem->CanMakeRequests())
			{
				return;
			}

			if (bBossChatReplyInFlight && !LLMSubsystem->bAllowOverlappingBossChatReplies)
			{
				PendingBossChatReplyDialogue = TrimmedDialogue;
				bHasPendingBossChatReply = true;
				if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
				{
					ShowDownGameState->BroadcastCollectorLLMStatus(true, TEXT("듣는 중..."));
				}
				return;
			}

			const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
			const bool bCooldownReady =
				(CurrentTime - LastBossChatReplyRequestTime) >= LLMSubsystem->BossChatReplyCooldownSeconds;

			if (!bIgnoreCooldown && !bCooldownReady)
			{
				PendingBossChatReplyDialogue = TrimmedDialogue;
				bHasPendingBossChatReply = true;

				if (UWorld* World = GetWorld())
				{
					const float RetryDelay = FMath::Max(
						0.05f,
						LLMSubsystem->BossChatReplyCooldownSeconds - (CurrentTime - LastBossChatReplyRequestTime));
					World->GetTimerManager().SetTimer(
						BossChatReplyRetryTimerHandle,
						this,
						&AShowDownGameModeBase::RequestPendingBossChatReply,
						RetryDelay,
						false);
				}

				if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
				{
					ShowDownGameState->BroadcastCollectorLLMStatus(true, TEXT("듣는 중..."));
				}
				return;
			}

			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().ClearTimer(BossChatReplyRetryTimerHandle);
			}

			bBossChatReplyInFlight = true;
			LastBossChatReplyRequestTime = CurrentTime;
			if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
			{
				ShowDownGameState->BroadcastCollectorLLMStatus(true, TEXT("생각 중..."));
			}

			const FSDLLMBossContext ChatContext = BuildLLMChatContext(TrimmedDialogue);
			LLMSubsystem->RequestBossChatReply(
				ChatContext,
				FSDLLMBossChatCallback::CreateWeakLambda(
					this,
					[this](bool bSuccess, const FString& Dialogue, const FString& Intent)
					{
						bBossChatReplyInFlight = false;
						if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
						{
							if (bSuccess)
							{
								AppendRecentDialogueLine(TEXT("Collector"), Dialogue);
								ShowDownGameState->BroadcastChatMessage(TEXT("Collector"), Dialogue);
								ShowDownGameState->BroadcastCollectorLLMStatus(true, FString());
							}
							else
							{
								ShowDownGameState->BroadcastCollectorLLMStatus(false, TEXT("대화 실패."));
							}
						}

						if (bHasPendingBossChatReply)
						{
							RequestPendingBossChatReply();
						}
					}));
		}
	}
}

void AShowDownGameModeBase::RequestPendingBossChatReply()
{
	if (!bHasPendingBossChatReply)
	{
		return;
	}

	const FString PendingDialogue = PendingBossChatReplyDialogue;
	PendingBossChatReplyDialogue.Empty();
	bHasPendingBossChatReply = false;

	TryRequestBossChatReply(PendingDialogue, true);
}

void AShowDownGameModeBase::NotifyPresentationFinished(EShowDownPhase FinishedPhase)
{
	EventEnd(FinishedPhase);
}

void AShowDownGameModeBase::EventEnd(EShowDownPhase FinishedPhase)
{
	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->EventEnd(FinishedPhase);
	}

	if (FinishedPhase != EShowDownPhase::Reveal)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(RevealDelayHandle);

	if (bHasPendingFoldReveal)
	{
		ContinueFoldAfterReveal(PendingFoldedSide, PendingFoldLoadCount);
		return;
	}

	if (bHasPendingRoundReveal)
	{
		ContinueRoundAfterReveal(PendingRoundResult);
	}
}

float AShowDownGameModeBase::EstimateCollectorWinChance() const
{
	if (!PlayerState.ForeheadCard || !CollectorState.ForeheadCard)
	{
		return 0.5f;
	}

	const int32 PlayerRank = PlayerState.ForeheadCard->Rank;
	const int32 CollectorRank = CollectorState.ForeheadCard->Rank;

	if (CollectorRank > PlayerRank)
	{
		return 0.8f;
	}

	if (CollectorRank < PlayerRank)
	{
		return 0.25f;
	}

	return 0.5f;
}

void AShowDownGameModeBase::ResolveCollectorBetResponse()
{
	if (!BettingSystem || !CollectorAISystem)
	{
		return;
	}

	if (bCollectorBetDecisionInProgress)
	{
		return;
	}
	bCollectorBetDecisionInProgress = true;

	const int32 CurrentBet = BettingSystem->GetCurrentBet();
	const int32 GivenCardRank = PlayerState.ForeheadCard ? PlayerState.ForeheadCard->Rank : 0;
	UE_LOG(LogTemp, Log, TEXT("Collector model: given card rank %d, current bet %d, committed %d, raises left %d"),
		GivenCardRank,
		CurrentBet,
		CollectorState.CurrentBet,
		BettingRaisesLeft);

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (USDLLMSubsystem* LLMSubsystem = GameInstance->GetSubsystem<USDLLMSubsystem>())
		{
			if (LLMSubsystem->CanMakeRequests())
			{
				if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
				{
					ShowDownGameState->BroadcastCollectorLLMStatus(true, TEXT("생각 중..."));
				}
				const FSDLLMBossContext LLMContext = BuildLLMBossContext(CurrentBet, GivenCardRank);
				LLMSubsystem->RequestBossResponse(
					LLMContext,
					FSDLLMBossResponseCallback::CreateWeakLambda(
						this,
						[this, GivenCardRank](bool bSuccess, const FSDLLMBossResponse& LLMResponse)
						{
							bCollectorBetDecisionInProgress = false;
							if (!bBettingPhase || !BettingSystem || !CollectorAISystem)
							{
								return;
							}

							const int32 LatestCurrentBet = BettingSystem->GetCurrentBet();
							FCollectorBetDecision CollectorDecision;
							if (bSuccess)
							{
								CollectorDecision = SanitizeCollectorDecision(LLMResponse.Decision);
								UE_LOG(LogTemp, Log, TEXT("Collector LLM decision: %d target %d / dialogue: %s / intent: %s"),
									static_cast<int32>(CollectorDecision.Action),
									CollectorDecision.TargetBet,
									*LLMResponse.Dialogue,
									*LLMResponse.Intent);
								if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
								{
									ShowDownGameState->BroadcastCollectorLLMDecision(
										LLMResponse.Dialogue,
										LLMResponse.Intent,
										CollectorDecision.Action,
										CollectorDecision.TargetBet);
								}
								AppendRecentDialogueLine(TEXT("Collector"), LLMResponse.Dialogue);
							}
							else
							{
								if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
								{
									ShowDownGameState->BroadcastCollectorLLMStatus(false, TEXT("API 실패. 기본 AI 사용."));
								}
								CollectorDecision = CollectorAISystem->ChooseBetDecisionByModel(
									CollectorState.HandCards,
									GivenCardRank,
									LatestCurrentBet,
									CollectorState.CurrentBet,
									6,
									BettingRaisesLeft,
									bHasLastRaiser && LastRaiser == EShowDownSide::Collector);
							}

							ExecuteCollectorBetDecision(CollectorDecision, GivenCardRank);
						}));
				return;
			}

			if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
			{
				ShowDownGameState->BroadcastCollectorLLMStatus(false, TEXT("API 키 없음. 기본 AI 사용."));
			}
		}
	}

	const FCollectorBetDecision CollectorDecision = CollectorAISystem->ChooseBetDecisionByModel(
		CollectorState.HandCards,
		GivenCardRank,
		CurrentBet,
		CollectorState.CurrentBet,
		6,
		BettingRaisesLeft,
		bHasLastRaiser && LastRaiser == EShowDownSide::Collector);

	bCollectorBetDecisionInProgress = false;
	ExecuteCollectorBetDecision(CollectorDecision, GivenCardRank);
}

FCollectorBetDecision AShowDownGameModeBase::SanitizeCollectorDecision(const FCollectorBetDecision& RawDecision) const
{
	FCollectorBetDecision SanitizedDecision = RawDecision;
	if (!BettingSystem)
	{
		return SanitizedDecision;
	}

	const int32 CurrentBet = BettingSystem->GetCurrentBet();
	const bool bMustRespond = CollectorState.CurrentBet < CurrentBet;

	if (SanitizedDecision.Action == EShowDownBetAction::Check && bMustRespond)
	{
		SanitizedDecision.Action = EShowDownBetAction::Call;
	}
	else if (SanitizedDecision.Action == EShowDownBetAction::Call && !bMustRespond)
	{
		SanitizedDecision.Action = EShowDownBetAction::Check;
	}
	else if (SanitizedDecision.Action == EShowDownBetAction::Fold && !bMustRespond)
	{
		SanitizedDecision.Action = EShowDownBetAction::Check;
	}

	if (SanitizedDecision.Action == EShowDownBetAction::Raise)
	{
		if (BettingRaisesLeft <= 0 || CurrentBet >= 6)
		{
			SanitizedDecision.Action = bMustRespond ? EShowDownBetAction::Call : EShowDownBetAction::Check;
			SanitizedDecision.TargetBet = CurrentBet;
		}
		else
		{
			SanitizedDecision.TargetBet = FMath::Clamp(SanitizedDecision.TargetBet, CurrentBet + 1, 6);
		}
	}

	return SanitizedDecision;
}

void AShowDownGameModeBase::ExecuteCollectorBetDecision(const FCollectorBetDecision& CollectorDecision, int32 GivenCardRank)
{
	if (!BettingSystem)
	{
		return;
	}

	const int32 CurrentBet = BettingSystem->GetCurrentBet();
	const EShowDownBetAction CollectorAction = CollectorDecision.Action;
	bCollectorHasActedInBetting = true;

	switch (CollectorAction)
	{
	case EShowDownBetAction::Check:
		BettingSystem->Check(EShowDownSide::Collector);
		CollectorState.CurrentBet = CurrentBet;
		if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
		{
			ShowDownGameState->OnBetChanged.Broadcast(EShowDownSide::Collector, CollectorState.CurrentBet);
			}
			BroadcastBetActionCommitted(EShowDownSide::Collector, EShowDownBetAction::Check, CollectorState.CurrentBet);
			RecordSingleBetBulletAction(EShowDownSide::Collector, EShowDownBetAction::Check);
			RecordCurrentRoundAction(FString::Printf(TEXT("Collector checked at %d."), CollectorState.CurrentBet));
		ShowEventDebugMessage(FString::Printf(TEXT("콜렉터 체크: %d발"), CollectorState.CurrentBet));
		UE_LOG(LogTemp, Log, TEXT("Collector Check"));
		if (bPlayerHasActedInBetting)
		{
			bBettingPhase = false;
			if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
			{
				ShowDownGameState->SetNameTagSingleRoundStatus(
					PlayerState.CurrentBet,
					CollectorState.CurrentBet,
						EShowDownSide::Collector,
						EShowDownPlayerSlot::None);
				}
				RefreshBetBulletPresentation();
				PlayCollectorActionPresentationThen([this]()
			{
				FinishBettingAndResolveRound();
			});
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("Player needs to respond."));
			if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
			{
				ShowDownGameState->SetNameTagSingleRoundStatus(
					PlayerState.CurrentBet,
					CollectorState.CurrentBet,
						EShowDownSide::Player,
						EShowDownPlayerSlot::Player1);
				}
				RefreshBetBulletPresentation();
				PlayCollectorActionPresentation();
		}
		break;

	case EShowDownBetAction::Call:
		BettingSystem->Call(EShowDownSide::Collector);
		CollectorState.CurrentBet = CurrentBet;
		bBettingPhase = false;
		if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
		{
			ShowDownGameState->OnBetChanged.Broadcast(EShowDownSide::Collector, CollectorState.CurrentBet);
			ShowDownGameState->SetNameTagSingleRoundStatus(
				PlayerState.CurrentBet,
				CollectorState.CurrentBet,
				EShowDownSide::Collector,
				EShowDownPlayerSlot::None);
			}
			BroadcastBetActionCommitted(EShowDownSide::Collector, EShowDownBetAction::Call, CollectorState.CurrentBet);
			RecordSingleBetBulletAction(EShowDownSide::Collector, EShowDownBetAction::Call);
			RefreshBetBulletPresentation();
			RecordCurrentRoundAction(FString::Printf(TEXT("Collector called to %d."), CollectorState.CurrentBet));
		ShowEventDebugMessage(FString::Printf(TEXT("콜렉터 콜: %d발"), CollectorState.CurrentBet));
		UE_LOG(LogTemp, Log, TEXT("Collector Call %d"), CurrentBet);
		PlayCollectorActionPresentationThen([this]()
		{
			FinishBettingAndResolveRound();
		});
		break;

	case EShowDownBetAction::Raise:
		{
			const int32 NewBet = FMath::Clamp(CollectorDecision.TargetBet, CurrentBet + 1, 6);
			if (BettingSystem->RaiseTo(EShowDownSide::Collector, NewBet))
			{
				CollectorState.CurrentBet = NewBet;
				BettingRaisesLeft = FMath::Max(0, BettingRaisesLeft - 1);
				bHasLastRaiser = true;
				LastRaiser = EShowDownSide::Collector;
				if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
				{
					ShowDownGameState->OnBetChanged.Broadcast(EShowDownSide::Collector, CollectorState.CurrentBet);
					ShowDownGameState->SetNameTagSingleRoundStatus(
						PlayerState.CurrentBet,
						CollectorState.CurrentBet,
						EShowDownSide::Player,
						EShowDownPlayerSlot::Player1);
					}
					BroadcastBetActionCommitted(EShowDownSide::Collector, EShowDownBetAction::Raise, CollectorState.CurrentBet);
					RecordSingleBetBulletAction(EShowDownSide::Collector, EShowDownBetAction::Raise);
					RefreshBetBulletPresentation();
					RecordCurrentRoundAction(FString::Printf(TEXT("Collector raised to %d."), CollectorState.CurrentBet));
				ShowEventDebugMessage(FString::Printf(TEXT("콜렉터 레이즈: %d발"), CollectorState.CurrentBet));
				UE_LOG(LogTemp, Log, TEXT("Collector Raise to %d"), NewBet);
				UE_LOG(LogTemp, Log, TEXT("Player needs to respond."));
				PlayCollectorActionPresentation();
			}
			break;
		}

	case EShowDownBetAction::Fold:
		BettingSystem->Fold(EShowDownSide::Collector);
		bBettingPhase = false;
		if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
		{
			ShowDownGameState->OnBetChanged.Broadcast(EShowDownSide::Collector, CollectorState.CurrentBet);
			ShowDownGameState->SetNameTagSingleRoundStatus(
				PlayerState.CurrentBet,
				CollectorState.CurrentBet,
				EShowDownSide::Collector,
				EShowDownPlayerSlot::None);
			}
			BroadcastBetActionCommitted(EShowDownSide::Collector, EShowDownBetAction::Fold, CollectorState.CurrentBet);
			RecordSingleBetBulletAction(EShowDownSide::Collector, EShowDownBetAction::Fold);
			RefreshBetBulletPresentation();
			RecordCurrentRoundAction(FString::Printf(TEXT("Collector folded at %d."), CollectorState.CurrentBet));
		ShowEventDebugMessage(FString::Printf(TEXT("콜렉터 폴드: %d발"), CollectorState.CurrentBet));
		UE_LOG(LogTemp, Log, TEXT("Collector Fold"));
		PlayCollectorActionPresentationThen([this]()
		{
			ResolveFold(EShowDownSide::Collector);
		});
		break;

	default:
		break;
	}
}

FSDLLMBossContext AShowDownGameModeBase::BuildLLMBossContext(int32 CurrentBet, int32 GivenCardRank) const
{
	FSDLLMBossContext Context;
	Context.PlayerForeheadRank = GivenCardRank;
	Context.CollectorForeheadRank = CollectorState.ForeheadCard ? CollectorState.ForeheadCard->Rank : 0;
	Context.CurrentBet = CurrentBet;
	Context.PlayerCommittedBet = PlayerState.CurrentBet;
	Context.CollectorCommittedBet = CollectorState.CurrentBet;
	Context.PlayerLives = PlayerState.Lives;
	Context.CollectorLives = CollectorState.Lives;
	Context.RaisesLeft = BettingRaisesLeft;
	Context.Stage = CurrentStageIndex + 1;
	Context.PlayerDialogue = LatestPlayerDialogueInput;
	Context.RecentDialogue = RecentDialogueHistory;
	Context.RecentRoundHistory = RecentRoundHistory;
	Context.CurrentRoundActions = CurrentRoundActionHistory;
	Context.DiscardedCardsSummary = DiscardedCardsSummary.IsEmpty()
		? TEXT("none")
		: DiscardedCardsSummary;
	if (CollectorAISystem)
	{
		Context.CollectorSettings = CollectorAISystem->Settings;
	}

	if (const AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		Context.Round = ShowDownGameState->CurrentRound;
	}

	for (const ACard* Card : CollectorState.HandCards)
	{
		if (Card)
		{
			Context.CollectorHandRanks.Add(Card->Rank);
		}
	}

	return Context;
}

FSDLLMBossContext AShowDownGameModeBase::BuildLLMChatContext(const FString& PlayerDialogue) const
{
	const int32 CurrentBet = BettingSystem ? BettingSystem->GetCurrentBet() : 1;
	const int32 GivenCardRank = PlayerState.ForeheadCard ? PlayerState.ForeheadCard->Rank : 0;
	FSDLLMBossContext Context = BuildLLMBossContext(CurrentBet, GivenCardRank);
	Context.PlayerDialogue = PlayerDialogue;
	Context.RecentDialogue = RecentDialogueHistory;
	return Context;
}

void AShowDownGameModeBase::AppendRecentDialogueLine(const FString& Speaker, const FString& Message)
{
	const FString TrimmedMessage = Message.TrimStartAndEnd();
	if (TrimmedMessage.IsEmpty())
	{
		return;
	}

	if (!RecentDialogueHistory.IsEmpty())
	{
		RecentDialogueHistory += TEXT("\n");
	}
	RecentDialogueHistory += FString::Printf(TEXT("%s: %s"), *Speaker.Left(32), *TrimmedMessage.Left(160));
	RecentDialogueHistory = RecentDialogueHistory.Right(900);
}

void AShowDownGameModeBase::ResetCurrentRoundMemory()
{
	CurrentRoundActionHistory.Reset();
	CurrentRoundPlayerGaveRank = 0;
	CurrentRoundCollectorGaveRank = 0;
	LastRoundPlayerCardRank = 0;
	LastRoundCollectorCardRank = 0;
	bCurrentRoundSummaryRecorded = false;
}

void AShowDownGameModeBase::RecordCurrentRoundAction(const FString& ActionText)
{
	const FString TrimmedAction = ActionText.TrimStartAndEnd();
	if (TrimmedAction.IsEmpty())
	{
		return;
	}

	if (!CurrentRoundActionHistory.IsEmpty())
	{
		CurrentRoundActionHistory += TEXT(" ");
	}
	CurrentRoundActionHistory += TrimmedAction.Left(180);
	CurrentRoundActionHistory = CurrentRoundActionHistory.Right(900);
}

void AShowDownGameModeBase::AppendRecentRoundSummary(EShowDownRoundResult Result, const FString& Reason)
{
	if (bCurrentRoundSummaryRecorded)
	{
		return;
	}

	bCurrentRoundSummaryRecorded = true;

	const int32 RoundNumber = GetShowDownGameState() ? GetShowDownGameState()->CurrentRound : 0;
	const FString SafeActions = CurrentRoundActionHistory.IsEmpty()
		? TEXT("none")
		: CurrentRoundActionHistory;
	const FString Summary = FString::Printf(
		TEXT("Stage %d Round %d: reason=%s, result=%s, player_gave_collector=%d, collector_gave_player=%d, revealed_player=%d, revealed_collector=%d, player_bet=%d, collector_bet=%d, player_lives=%d, collector_lives=%d, actions=[%s]."),
		CurrentStageIndex + 1,
		RoundNumber,
		*Reason.Left(64),
		*GetRoundResultText(Result),
		CurrentRoundPlayerGaveRank,
		CurrentRoundCollectorGaveRank,
		LastRoundPlayerCardRank,
		LastRoundCollectorCardRank,
		PlayerState.CurrentBet,
		CollectorState.CurrentBet,
		PlayerState.Lives,
		CollectorState.Lives,
		*SafeActions.Left(700));

	if (!RecentRoundHistory.IsEmpty())
	{
		RecentRoundHistory += TEXT("\n");
	}
	RecentRoundHistory += Summary;
	RecentRoundHistory = RecentRoundHistory.Right(1400);
}

FString AShowDownGameModeBase::GetSideText(EShowDownSide Side) const
{
	return Side == EShowDownSide::Player ? TEXT("Player") : TEXT("Collector");
}

FString AShowDownGameModeBase::GetSideDisplayText(EShowDownSide Side) const
{
	return Side == EShowDownSide::Player ? TEXT("플레이어") : TEXT("콜렉터");
}

FString AShowDownGameModeBase::GetRoundResultText(EShowDownRoundResult Result) const
{
	switch (Result)
	{
	case EShowDownRoundResult::PlayerWin:
		return TEXT("PlayerWin");

	case EShowDownRoundResult::CollectorWin:
		return TEXT("CollectorWin");

	case EShowDownRoundResult::Draw:
		return TEXT("Draw");

	default:
		return TEXT("Unknown");
	}
}

void AShowDownGameModeBase::BeginCardSelectionRound()
{
	ResetCurrentRoundMemory();
	CurrentRoundFirstSide = NextRoundFirstSide;

	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->SetPhase(EShowDownPhase::SelectCard);
		ShowDownGameState->SetNameTagSingleRoundStatus(0, 0, CurrentRoundFirstSide, EShowDownPlayerSlot::Player1);
	}

	if (CurrentRoundFirstSide == EShowDownSide::Collector)
	{
		SetPlayerHandSelectable(false);
		CollectorGiveCardToPlayer();
		return;
	}

	SetPlayerHandSelectable(true);
}

void AShowDownGameModeBase::SetNextRoundFirstSideFromResult(EShowDownRoundResult Result)
{
	switch (Result)
	{
	case EShowDownRoundResult::PlayerWin:
		NextRoundFirstSide = EShowDownSide::Collector;
		break;

	case EShowDownRoundResult::CollectorWin:
		NextRoundFirstSide = EShowDownSide::Player;
		break;

	case EShowDownRoundResult::Draw:
	default:
		NextRoundFirstSide = CurrentRoundFirstSide;
		break;
	}
}

void AShowDownGameModeBase::FinishBettingAndResolveRound()
{
	if (!RoundResolver || !PlayerState.ForeheadCard || !CollectorState.ForeheadCard)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot resolve round. RoundResolver or forehead cards are missing."));
		return;
	}

	const int32 PlayerCardRank = PlayerState.ForeheadCard->Rank;
	const int32 CollectorCardRank = CollectorState.ForeheadCard->Rank;
	LastRoundPlayerCardRank = PlayerCardRank;
	LastRoundCollectorCardRank = CollectorCardRank;
	const EShowDownRoundResult Result = RoundResolver->ResolveRevealedCards(PlayerCardRank, CollectorCardRank);
	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->SetPhase(EShowDownPhase::Reveal);
		ShowDownGameState->OnCardsRevealed.Broadcast(PlayerCardRank, CollectorCardRank);
		ShowDownGameState->OnRoundResolved.Broadcast(Result);
		ShowDownGameState->SetNameTagSingleRoundStatus(
			PlayerState.CurrentBet,
			CollectorState.CurrentBet,
			EShowDownSide::Player,
			EShowDownPlayerSlot::None);
	}
	ShowEventDebugMessage(FString::Printf(TEXT("승부 공개: 플레이어 %d / 콜렉터 %d"),
		PlayerCardRank,
		CollectorCardRank));
	UE_LOG(LogTemp, Log, TEXT("Reveal cards. Player: %d, Collector: %d"), PlayerCardRank, CollectorCardRank);
	UE_LOG(LogTemp, Log, TEXT("Reveal resolved. Waiting for presentation or auto-advance fallback."));

	const float RevealPresentationSeconds = PlaySinglePlayerCardRevealPresentation();
	bHasPendingRoundReveal = true;
	bHasPendingFoldReveal = false;
	PendingRoundResult = Result;

	PlayCollectorActionPresentationThen([this, RevealPresentationSeconds]()
	{
		ScheduleRevealAutoAdvanceIfNeeded(RevealPresentationSeconds);
	});
}

void AShowDownGameModeBase::ContinueRoundAfterReveal(EShowDownRoundResult Result)
{
	bHasPendingRoundReveal = false;
	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->EventEnd(EShowDownPhase::Reveal);
	}

	// 결과 공개 직후 보스가 승/패/무에 대한 반응 채팅을 남긴다.
	BroadcastBossResultReaction(Result);

	SetNextRoundFirstSideFromResult(Result);

	const FString ResultDisplayText = Result == EShowDownRoundResult::PlayerWin
		? TEXT("플레이어 승리")
		: Result == EShowDownRoundResult::CollectorWin
			? TEXT("콜렉터 승리")
			: TEXT("무승부");
	ShowEventDebugMessage(FString::Printf(TEXT("결과: %s / 다음 선공: %s"),
		*ResultDisplayText,
		*GetSideDisplayText(NextRoundFirstSide)));

	switch (Result)
	{
	case EShowDownRoundResult::PlayerWin:
		UE_LOG(LogTemp, Log, TEXT("Round result: Player wins. Collector roulette with %d bullet(s)."), CollectorState.CurrentBet);
		ApplyRouletteResult(EShowDownSide::Collector, CollectorState.CurrentBet, [this, Result]()
		{
			AppendRecentRoundSummary(Result, TEXT("card reveal"));
			EndRound();
		});
		break;

	case EShowDownRoundResult::CollectorWin:
		UE_LOG(LogTemp, Log, TEXT("Round result: Collector wins. Player roulette with %d bullet(s)."), PlayerState.CurrentBet);
		ApplyRouletteResult(EShowDownSide::Player, PlayerState.CurrentBet, [this, Result]()
		{
			AppendRecentRoundSummary(Result, TEXT("card reveal"));
			EndRound();
		});
		break;

	case EShowDownRoundResult::Draw:
		UE_LOG(LogTemp, Log, TEXT("Round result: Draw. Both sides roulette."));
		ApplyRouletteResult(EShowDownSide::Collector, CollectorState.CurrentBet, [this, Result]()
		{
			ApplyRouletteResult(EShowDownSide::Player, PlayerState.CurrentBet, [this, Result]()
			{
				AppendRecentRoundSummary(Result, TEXT("card reveal"));
				EndRound();
			});
		});
		break;

	default:
		break;
	}
}

void AShowDownGameModeBase::BroadcastBossResultReaction(EShowDownRoundResult Result)
{
	AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState();
	if (!ShowDownGameState)
	{
		return;
	}

	// Collector(보스) 관점의 결과와 LLM 실패 시 쓸 정적 폴백 대사
	FString Outcome;
	FString FallbackLine;
	switch (Result)
	{
	case EShowDownRoundResult::PlayerWin: // 보스 패배
		Outcome = TEXT("collector_lost");
		FallbackLine = TEXT("아쉽군… 다음엔 다르다.");
		break;
	case EShowDownRoundResult::CollectorWin: // 보스 승리
		Outcome = TEXT("collector_won");
		FallbackLine = TEXT("예상대로군.");
		break;
	case EShowDownRoundResult::Draw:
	default:
		Outcome = TEXT("draw");
		FallbackLine = TEXT("무승부라… 시시하군.");
		break;
	}

	UGameInstance* GameInstance = GetGameInstance();
	USDLLMSubsystem* LLMSubsystem = GameInstance ? GameInstance->GetSubsystem<USDLLMSubsystem>() : nullptr;
	if (!LLMSubsystem || !LLMSubsystem->CanMakeRequests())
	{
		// LLM 비활성/미설정 → 정적 대사로 대체
		AppendRecentDialogueLine(TEXT("Collector"), FallbackLine);
		ShowDownGameState->BroadcastChatMessage(TEXT("Collector"), FallbackLine);
		return;
	}

	const int32 CurrentBet = BettingSystem ? BettingSystem->GetCurrentBet() : 1;
	const int32 GivenCardRank = PlayerState.ForeheadCard ? PlayerState.ForeheadCard->Rank : 0;
	FSDLLMBossContext Context = BuildLLMBossContext(CurrentBet, GivenCardRank);
	Context.RoundOutcome = Outcome;

	LLMSubsystem->RequestBossResultReaction(
		Context,
		FSDLLMBossChatCallback::CreateWeakLambda(
			this,
			[this, FallbackLine](bool bSuccess, const FString& Dialogue, const FString& Intent)
			{
				AShowDownGameStateBase* CallbackGameState = GetShowDownGameState();
				if (!CallbackGameState)
				{
					return;
				}

				const FString Line = (bSuccess && !Dialogue.TrimStartAndEnd().IsEmpty())
					? Dialogue
					: FallbackLine;
				AppendRecentDialogueLine(TEXT("Collector"), Line);
				CallbackGameState->BroadcastChatMessage(TEXT("Collector"), Line);
			}));
}

void AShowDownGameModeBase::ResolveFold(EShowDownSide FoldedSide)
{
	if (!RoundResolver)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot resolve fold. RoundResolver is missing."));
		return;
	}

	FShowDownParticipantState& FoldedState = FoldedSide == EShowDownSide::Player ? PlayerState : CollectorState;
	const int32 FoldedCardRank = FoldedState.ForeheadCard ? FoldedState.ForeheadCard->Rank : 0;
	const FShowDownStageRule* StageRule = GetCurrentStageRule();
	const bool bSevenFoldLoadsSix = StageRule ? StageRule->bSevenFoldLoadsSix : true;
	const int32 LoadCount = RoundResolver->GetFoldLoadCount(FoldedCardRank, FoldedState.CurrentBet, bSevenFoldLoadsSix);

	const int32 PlayerCardRank = PlayerState.ForeheadCard ? PlayerState.ForeheadCard->Rank : 0;
	const int32 CollectorCardRank = CollectorState.ForeheadCard ? CollectorState.ForeheadCard->Rank : 0;
	LastRoundPlayerCardRank = PlayerCardRank;
	LastRoundCollectorCardRank = CollectorCardRank;

	const EShowDownRoundResult FoldResult = FoldedSide == EShowDownSide::Player
		? EShowDownRoundResult::CollectorWin
		: EShowDownRoundResult::PlayerWin;
	SetNextRoundFirstSideFromResult(FoldResult);

	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->SetPhase(EShowDownPhase::Reveal);
		ShowDownGameState->OnCardsRevealed.Broadcast(PlayerCardRank, CollectorCardRank);
		ShowDownGameState->OnRoundResolved.Broadcast(FoldResult);
		ShowDownGameState->SetNameTagSingleRoundStatus(
			PlayerState.CurrentBet,
			CollectorState.CurrentBet,
			FoldedSide,
			EShowDownPlayerSlot::None);
	}
	ShowEventDebugMessage(FString::Printf(TEXT("%s 폴드: 플레이어 %d / 콜렉터 %d / 다음 선공: %s"),
		*GetSideDisplayText(FoldedSide),
		PlayerCardRank,
		CollectorCardRank,
		*GetSideDisplayText(NextRoundFirstSide)));
	UE_LOG(LogTemp, Log, TEXT("%s folded. Forehead card: %d, roulette load: %d"),
		FoldedSide == EShowDownSide::Player ? TEXT("Player") : TEXT("Collector"),
		FoldedCardRank,
		LoadCount);
	UE_LOG(LogTemp, Log, TEXT("Fold reveal resolved. Waiting for presentation or auto-advance fallback."));

	const float RevealPresentationSeconds = PlaySinglePlayerCardRevealPresentation();
	bHasPendingRoundReveal = false;
	bHasPendingFoldReveal = true;
	PendingFoldedSide = FoldedSide;
	PendingFoldLoadCount = LoadCount;

	PlayCollectorActionPresentationThen([this, RevealPresentationSeconds]()
	{
		ScheduleRevealAutoAdvanceIfNeeded(RevealPresentationSeconds);
	});
}

void AShowDownGameModeBase::ContinueFoldAfterReveal(EShowDownSide FoldedSide, int32 LoadCount)
{
	bHasPendingFoldReveal = false;
	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->EventEnd(EShowDownPhase::Reveal);
	}

	// 폴드로 끝난 라운드도 보스 반응 채팅을 남긴다.
	BroadcastBossResultReaction(
		FoldedSide == EShowDownSide::Player ? EShowDownRoundResult::CollectorWin : EShowDownRoundResult::PlayerWin);

	ApplyRouletteResult(FoldedSide, LoadCount, [this, FoldedSide]()
	{
		AppendRecentRoundSummary(
			FoldedSide == EShowDownSide::Player ? EShowDownRoundResult::CollectorWin : EShowDownRoundResult::PlayerWin,
			FString::Printf(TEXT("%s folded"), *GetSideText(FoldedSide)));
		EndRound();
	});
}

ASDBetActionPanelActor* AShowDownGameModeBase::EnsureBetActionPanelActor()
{
	if (!bUseBetActionPanel || !HasAuthority())
	{
		return nullptr;
	}

	if (IsValid(BetActionPanelActor))
	{
		return BetActionPanelActor;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UClass* PanelClass = BetActionPanelClass
		? BetActionPanelClass.Get()
		: ASDBetActionPanelActor::StaticClass();
	if (!PanelClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	BetActionPanelActor = World->SpawnActor<ASDBetActionPanelActor>(
		PanelClass,
		FTransform::Identity,
		SpawnParams);
	if (BetActionPanelActor)
	{
		BetActionPanelActor->PanelVisualScale = FMath::Max(0.1f, BetActionPanelVisualScale);
	}
	return BetActionPanelActor;
}

void AShowDownGameModeBase::ClearBetBulletPresentation()
{
	ClearBetBulletTransientState();
	ClearBetBulletActionHistory();
	for (AShowDownCharacter* Character : GetShowDownCharacters())
	{
		if (IsValid(Character))
		{
			Character->ClearBetStatusPresentation();
		}
	}
	ClearBetActionPanel();
}

void AShowDownGameModeBase::RefreshBetBulletPresentation()
{
	if (!bUseBetBulletPresentation)
	{
		for (AShowDownCharacter* Character : GetShowDownCharacters())
		{
			if (IsValid(Character))
			{
				Character->ClearBetStatusPresentation();
			}
		}
		RefreshBetActionPanel();
		return;
	}

	TArray<FShowDownBetStatusLaneState> LaneStates;

	if (bMultiplayerMatchStarted)
	{
		TArray<ASDPlayerState*> OrderedPlayers;
		for (ASDPlayerState* Player : MultiplayerPlayers)
		{
			if (Player)
			{
				OrderedPlayers.Add(Player);
			}
		}
		OrderedPlayers.Sort(SortByMultiplayerTurnOrder);

		for (ASDPlayerState* Player : OrderedPlayers)
		{
			if (!Player)
			{
				continue;
			}

			FShowDownBetStatusLaneState LaneState;
			LaneState.Slot = Player->ShowDownSlot;
			LaneState.DisplayName = Player->GetPlayerName().IsEmpty()
				? FString::Printf(TEXT("Player%d"), static_cast<int32>(Player->ShowDownSlot))
				: Player->GetPlayerName();
			LaneState.BulletCount = FMath::Clamp(Player->CurrentBet, 0, 6);
			LaneState.bFolded = MultiplayerFoldedPlayers.Contains(Player);
			if (LaneState.bFolded)
			{
				// A seven-card fold replaces the regular committed bet with the
				// stage rule's full-cylinder load for the rest of this round.
				LaneState.BulletCount = ResolveMultiplayerFoldLoadCount(Player);
			}
			const EShowDownPhase CurrentPhase = GetShowDownGameState()
				? GetShowDownGameState()->CurrentPhase
				: EShowDownPhase::None;
			if (const EShowDownBetAction* LastAction = MultiplayerLastBetActions.Find(Player->ShowDownSlot))
			{
				LaneState.ActionText = BuildBetBulletActionText(*LastAction);
			}
			LaneState.bCurrentTurn = bBettingPhase
				&& !bMultiplayerRoundResolving
				&& MultiplayerCurrentBetter == Player
				&& !LaneState.bFolded
				&& !bHasBetBulletRouletteTarget;
			const int32 RequiredBet = BettingSystem
				? FMath::Clamp(BettingSystem->GetCurrentBet(), 0, 6)
				: LaneState.BulletCount;
			LaneState.bNeedsToMatchBet = CurrentPhase == EShowDownPhase::Betting
				&& !LaneState.bFolded
				&& LaneState.BulletCount < RequiredBet;

			if (CurrentPhase == EShowDownPhase::SelectCard)
			{
				LaneState.BulletCount = 0;
				LaneState.ActionText = Player->ForeheadCard ? TEXT("READY") : TEXT("SELECTING");
				LaneState.bCurrentTurn = false;
			}
			else if (CurrentPhase == EShowDownPhase::Reveal)
			{
				if (LaneState.bFolded)
				{
					LaneState.ActionText = TEXT("FOLD");
				}
				else if (LaneState.ActionText.IsEmpty())
				{
					LaneState.ActionText = TEXT("REVEAL");
				}
				LaneState.bCurrentTurn = false;
			}
			else if (CurrentPhase == EShowDownPhase::Roulette)
			{
				const bool bFiringNow = GetShowDownGameState()
					&& GetShowDownGameState()->NameTagTurnSlot == Player->ShowDownSlot;
				LaneState.ActionText = bFiringNow ? TEXT("FIRING") : TEXT("WAIT");
				LaneState.bCurrentTurn = false;
			}

			if (CurrentPhase == EShowDownPhase::Betting
				&& bHasBetBulletAction
				&& bBetBulletActionIsMultiplayer
				&& BetBulletActionSlot == Player->ShowDownSlot)
			{
				LaneState.ActionText = BuildBetBulletActionText(BetBulletAction);
			}

			if (bHasBetBulletRouletteTarget
				&& bBetBulletRouletteTargetIsMultiplayer
				&& BetBulletRouletteTargetSlot == Player->ShowDownSlot)
			{
				LaneState.bRouletteTarget = true;
				LaneState.RouletteBulletCount = BetBulletRouletteBulletCount;
				LaneState.BulletCount = LaneState.RouletteBulletCount;
			}

			LaneStates.Add(LaneState);
		}
	}
	else
	{
		const EShowDownSide Sides[] = { EShowDownSide::Player, EShowDownSide::Collector };
		for (EShowDownSide Side : Sides)
		{
			const FShowDownParticipantState& ParticipantState = Side == EShowDownSide::Player
				? PlayerState
				: CollectorState;

			FShowDownBetStatusLaneState LaneState;
			LaneState.Slot = Side == EShowDownSide::Player ? EShowDownPlayerSlot::Player1 : EShowDownPlayerSlot::None;
			LaneState.DisplayName = Side == EShowDownSide::Player ? TEXT("Player") : TEXT("Collector");
			LaneState.BulletCount = FMath::Clamp(ParticipantState.CurrentBet, 0, 6);
			if (const EShowDownBetAction* LastAction = SingleLastBetActions.Find(Side))
			{
				LaneState.ActionText = BuildBetBulletActionText(*LastAction);
				LaneState.bFolded = *LastAction == EShowDownBetAction::Fold;
			}
			LaneState.bCurrentTurn = bBettingPhase
				&& !bHasBetBulletRouletteTarget
				&& GetShowDownGameState()
				&& GetShowDownGameState()->NameTagTurnSide == Side
				&& GetShowDownGameState()->NameTagTurnSlot != EShowDownPlayerSlot::None;
			const EShowDownPhase CurrentPhase = GetShowDownGameState()
				? GetShowDownGameState()->CurrentPhase
				: EShowDownPhase::None;
			const int32 RequiredBet = BettingSystem
				? FMath::Clamp(BettingSystem->GetCurrentBet(), 0, 6)
				: LaneState.BulletCount;
			LaneState.bNeedsToMatchBet = CurrentPhase == EShowDownPhase::Betting
				&& !LaneState.bFolded
				&& LaneState.BulletCount < RequiredBet;

			if (bHasBetBulletAction
				&& !bBetBulletActionIsMultiplayer
				&& BetBulletActionSide == Side)
			{
				LaneState.ActionText = BuildBetBulletActionText(BetBulletAction);
				LaneState.bFolded = BetBulletAction == EShowDownBetAction::Fold;
			}

			if (bHasBetBulletRouletteTarget
				&& !bBetBulletRouletteTargetIsMultiplayer
				&& BetBulletRouletteTargetSide == Side)
			{
				LaneState.bRouletteTarget = true;
				LaneState.RouletteBulletCount = BetBulletRouletteBulletCount;
				LaneState.BulletCount = LaneState.RouletteBulletCount;
			}

			LaneStates.Add(LaneState);
		}
	}

	TSet<AShowDownCharacter*> UpdatedBetStatusCharacters;
	for (const FShowDownBetStatusLaneState& LaneState : LaneStates)
	{
		AShowDownCharacter* TargetCharacter = nullptr;
		if (bMultiplayerMatchStarted)
		{
			TargetCharacter = FindActiveCharacterForPlayerSlot(GetWorld(), LaneState.Slot);
		}
		else if (LaneState.Slot == EShowDownPlayerSlot::Player1)
		{
			TargetCharacter = FindSingleRouletteCharacter(EShowDownSide::Player);
		}
		else
		{
			TargetCharacter = FindSingleRouletteCharacter(EShowDownSide::Collector);
		}

		if (!IsValid(TargetCharacter))
		{
			continue;
		}

		UpdatedBetStatusCharacters.Add(TargetCharacter);
		TargetCharacter->SetBetStatusPresentation(
			true,
			LaneState.DisplayName,
			GetBetStatusLabel(LaneState),
			LaneState.BulletCount,
			6,
			LaneState.bNeedsToMatchBet,
			GetBetStatusAccentColor(LaneState));
	}

	for (AShowDownCharacter* Character : GetShowDownCharacters())
	{
		if (IsValid(Character) && !UpdatedBetStatusCharacters.Contains(Character))
		{
			Character->ClearBetStatusPresentation();
		}
	}
	RefreshBetActionPanel();
	RefreshCentralGunStatus();
}

void AShowDownGameModeBase::ClearBetActionPanel()
{
	if (!HasAuthority())
	{
		return;
	}

	++BetActionPanelRevision;
	ASDBetActionPanelActor* PanelActor = IsValid(BetActionPanelActor)
		? BetActionPanelActor.Get()
		: EnsureBetActionPanelActor();
	if (!PanelActor)
	{
		return;
	}

	FSDBetActionPanelState EmptyState;
	EmptyState.Revision = BetActionPanelRevision;
	EmptyState.ButtonAnimationDuration = FMath::Clamp(BetActionButtonAnimationDuration, 0.05f, 1.0f);
	EmptyState.ButtonAnimationStaggerDelay = FMath::Clamp(BetActionButtonAnimationStagger, 0.0f, 0.25f);
	EmptyState.ButtonBounceStrength = FMath::Clamp(BetActionButtonBounceStrength, 0.0f, 0.5f);
	EmptyState.BulletAnimationDuration = FMath::Clamp(BetActionBulletAnimationDuration, 0.05f, 1.0f);
	EmptyState.BulletRevealStaggerDelay = FMath::Clamp(BetActionBulletRevealStagger, 0.0f, 0.25f);
	EmptyState.BulletBounceStrength = FMath::Clamp(BetActionBulletBounceStrength, 0.0f, 0.5f);
	PanelActor->SetPanelState(EmptyState);
}

void AShowDownGameModeBase::SetMultiplayerRaisePreviewTarget(ASDPlayerState* SubmittingPlayer, int32 TargetBet)
{
	if (!HasAuthority()
		|| !bMultiplayerMatchStarted
		|| !bBettingPhase
		|| bMultiplayerRoundResolving
		|| !SubmittingPlayer
		|| SubmittingPlayer != MultiplayerCurrentBetter
		|| MultiplayerFoldedPlayers.Contains(SubmittingPlayer))
	{
		return;
	}

	const int32 TableBet = BettingSystem ? BettingSystem->GetCurrentBet() : SubmittingPlayer->CurrentBet;
	BetActionPanelSelectedRaiseTarget = FMath::Clamp(TargetBet, FMath::Clamp(TableBet + 1, 1, 6), 6);
	BetActionPanelPreviewTurnSlot = SubmittingPlayer->ShowDownSlot;
	RefreshBetActionPanel();
}

void AShowDownGameModeBase::RefreshBetActionPanel()
{
	if (!HasAuthority())
	{
		return;
	}

	if (!bUseBetActionPanel)
	{
		ClearBetActionPanel();
		return;
	}

	ASDBetActionPanelActor* PanelActor = EnsureBetActionPanelActor();
	if (!PanelActor)
	{
		return;
	}
	PanelActor->PanelVisualScale = FMath::Max(0.1f, BetActionPanelVisualScale);
	PanelActor->ButtonHeight = FMath::Max(1.0f, BetActionButtonHeight);
	PanelActor->PrimaryButtonWidth = FMath::Max(1.0f, BetActionPrimaryButtonWidth);
	PanelActor->RaiseButtonWidth = FMath::Max(1.0f, BetActionRaiseButtonWidth);
	PanelActor->StepButtonWidth = FMath::Max(1.0f, BetActionStepButtonWidth);
	PanelActor->FoldButtonWidth = FMath::Max(1.0f, BetActionFoldButtonWidth);
	PanelActor->TopRowHeight = BetActionRaiseRowHeight;
	PanelActor->BottomRowHeight = BetActionPrimaryRowHeight;
	PanelActor->BulletSpacing = FMath::Max(0.1f, BetActionBulletSpacing);
	PanelActor->BulletPreviewScale = FMath::Max(0.001f, BetActionBulletScale);
	PanelActor->BulletRowOffset = BetActionBulletRowOffset;

	FSDBetActionPanelState NewState;
	NewState.Revision = ++BetActionPanelRevision;
	NewState.MaxRaiseTarget = 6;
	NewState.ButtonAnimationDuration = FMath::Clamp(BetActionButtonAnimationDuration, 0.05f, 1.0f);
	NewState.ButtonAnimationStaggerDelay = FMath::Clamp(BetActionButtonAnimationStagger, 0.0f, 0.25f);
	NewState.ButtonBounceStrength = FMath::Clamp(BetActionButtonBounceStrength, 0.0f, 0.5f);
	NewState.BulletAnimationDuration = FMath::Clamp(BetActionBulletAnimationDuration, 0.05f, 1.0f);
	NewState.BulletRevealStaggerDelay = FMath::Clamp(BetActionBulletRevealStagger, 0.0f, 0.25f);
	NewState.BulletBounceStrength = FMath::Clamp(BetActionBulletBounceStrength, 0.0f, 0.5f);

	if (bMultiplayerMatchStarted)
	{
		const int32 TableBet = BettingSystem ? BettingSystem->GetCurrentBet() : 0;
		const bool bCurrentPlayerCanAct =
			bBettingPhase
			&& !bMultiplayerRoundResolving
			&& MultiplayerCurrentBetter
			&& MultiplayerCurrentBetter->Lives > 0
			&& !MultiplayerFoldedPlayers.Contains(MultiplayerCurrentBetter)
			&& CountActiveMultiplayerPlayers() > 1;

		NewState.bMultiplayer = true;
		NewState.bVisible = bCurrentPlayerCanAct;
		NewState.TurnSlot = bCurrentPlayerCanAct && MultiplayerCurrentBetter
			? MultiplayerCurrentBetter->ShowDownSlot
			: EShowDownPlayerSlot::None;
		NewState.CurrentPlayerBet = MultiplayerCurrentBetter
			? FMath::Clamp(MultiplayerCurrentBetter->CurrentBet, 0, 6)
			: 0;
		NewState.TableBet = FMath::Clamp(TableBet, 0, 6);
		NewState.LoadedBulletCount = FMath::Clamp(MultiplayerLiveRoundCount, 0, 6);
		NewState.MinRaiseTarget = FMath::Clamp(NewState.TableBet + 1, 1, NewState.MaxRaiseTarget);
		if (NewState.TurnSlot != BetActionPanelPreviewTurnSlot)
		{
			BetActionPanelPreviewTurnSlot = NewState.TurnSlot;
			BetActionPanelSelectedRaiseTarget = NewState.MinRaiseTarget;
		}
		BetActionPanelSelectedRaiseTarget = FMath::Clamp(
			BetActionPanelSelectedRaiseTarget,
			NewState.MinRaiseTarget,
			NewState.MaxRaiseTarget);
		NewState.SelectedRaiseTarget = BetActionPanelSelectedRaiseTarget;
		NewState.bCanRaise = bCurrentPlayerCanAct && BettingRaisesLeft > 0 && NewState.TableBet < NewState.MaxRaiseTarget;
		NewState.bCanFold = bCurrentPlayerCanAct;

		const FTransform PanelTransform = BuildBetActionPanelTransformForPlayer(MultiplayerCurrentBetter);
		NewState.WorldLocation = PanelTransform.GetLocation();
		NewState.WorldRotation = PanelTransform.GetRotation().Rotator();
	}
	else
	{
		const AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState();
		const EShowDownSide TurnSide = ShowDownGameState
			? ShowDownGameState->NameTagTurnSide
			: CurrentRoundFirstSide;
		const EShowDownPlayerSlot TurnSlot = ShowDownGameState
			? ShowDownGameState->NameTagTurnSlot
			: EShowDownPlayerSlot::Player1;
		const int32 TableBet = FMath::Max(PlayerState.CurrentBet, CollectorState.CurrentBet);
		const bool bPlayerCanAct =
			bBettingPhase
			&& !bCollectorActionPresentationInProgress
			&& !bCollectorBetDecisionInProgress
			&& TurnSide == EShowDownSide::Player
			&& TurnSlot != EShowDownPlayerSlot::None;

		NewState.bMultiplayer = false;
		NewState.bVisible = bPlayerCanAct;
		NewState.TurnSlot = bPlayerCanAct ? EShowDownPlayerSlot::Player1 : EShowDownPlayerSlot::None;
		NewState.CurrentPlayerBet = FMath::Clamp(PlayerState.CurrentBet, 0, 6);
		NewState.TableBet = FMath::Clamp(TableBet, 0, 6);
		NewState.LoadedBulletCount = NewState.TableBet;
		NewState.MinRaiseTarget = FMath::Clamp(NewState.TableBet + 1, 1, NewState.MaxRaiseTarget);
		NewState.SelectedRaiseTarget = NewState.MinRaiseTarget;
		NewState.bCanRaise = bPlayerCanAct && BettingRaisesLeft > 0 && NewState.TableBet < NewState.MaxRaiseTarget;
		NewState.bCanFold = bPlayerCanAct;

		const FTransform PanelTransform = BuildBetActionPanelTransformForSide(EShowDownSide::Player);
		NewState.WorldLocation = PanelTransform.GetLocation();
		NewState.WorldRotation = PanelTransform.GetRotation().Rotator();
	}

	PanelActor->SetPanelState(NewState);
}

FTransform AShowDownGameModeBase::BuildBetActionPanelTransformForSide(EShowDownSide Side) const
{
	if (const USceneComponent* HandSlot = GetHandSlotForSide(Side))
	{
		return BuildBetActionPanelTransformFromLocation(
			HandSlot->GetComponentLocation(),
			Side == EShowDownSide::Player ? 0 : 2);
	}

	if (const USceneComponent* HeadSlot = GetHeadSlotForSide(Side))
	{
		return BuildBetActionPanelTransformFromLocation(
			HeadSlot->GetComponentLocation(),
			Side == EShowDownSide::Player ? 0 : 2);
	}

	return BuildBetActionPanelTransformFromLocation(
		ResolveSingleTableCenter(GetWorld()),
		Side == EShowDownSide::Player ? 0 : 2);
}

FTransform AShowDownGameModeBase::BuildBetActionPanelTransformForPlayer(const ASDPlayerState* Player) const
{
	if (Player)
	{
		if (const USceneComponent* HandSlot = GetHandSlotForPlayerState(const_cast<ASDPlayerState*>(Player)))
		{
			return BuildBetActionPanelTransformFromLocation(
				HandSlot->GetComponentLocation(),
				GetMultiplayerTurnOrderIndex(Player->ShowDownSlot));
		}

		if (const USceneComponent* HeadSlot = GetHeadSlotForPlayerState(const_cast<ASDPlayerState*>(Player)))
		{
			return BuildBetActionPanelTransformFromLocation(
				HeadSlot->GetComponentLocation(),
				GetMultiplayerTurnOrderIndex(Player->ShowDownSlot));
		}
	}

	return BuildBetActionPanelTransformFromLocation(
		ResolveSingleTableCenter(GetWorld()),
		GetMultiplayerTurnOrderIndex(Player ? Player->ShowDownSlot : EShowDownPlayerSlot::None));
}

FTransform AShowDownGameModeBase::BuildBetActionPanelTransformFromLocation(
	const FVector& SourceLocation,
	int32 FallbackOrderIndex) const
{
	const FVector TableCenter = ResolveSingleTableCenter(GetWorld());
	FVector Direction = SourceLocation - TableCenter;
	Direction.Z = 0.0f;

	if (Direction.IsNearlyZero())
	{
		static const float FallbackYaws[] = { -90.0f, 0.0f, 90.0f, 180.0f };
		const int32 FallbackIndex = FMath::Clamp(FallbackOrderIndex, 0, UE_ARRAY_COUNT(FallbackYaws) - 1);
		Direction = FRotationMatrix(FRotator(0.0f, FallbackYaws[FallbackIndex], 0.0f)).GetUnitAxis(EAxis::X);
	}
	Direction.Normalize();

	FVector FacingDirection = Direction;

	FRotator PanelRotation = FacingDirection.Rotation();
	PanelRotation.Pitch = 0.0f;
	PanelRotation.Roll = 0.0f;
	PanelRotation = (PanelRotation.Quaternion() * BetActionPanelRotationOffset.Quaternion()).Rotator();
	const FVector RightDirection = FRotationMatrix(PanelRotation).GetUnitAxis(EAxis::Y);

	FVector PanelLocation =
		SourceLocation
		+ Direction * BetActionPanelDistanceFromCenter
		+ RightDirection * BetActionPanelRightOffset;
	PanelLocation.Z = SourceLocation.Z + BetActionPanelHeightOffset;
	return FTransform(PanelRotation, PanelLocation);
}

void AShowDownGameModeBase::RecordSingleBetBulletAction(
	EShowDownSide Side,
	EShowDownBetAction Action)
{
	SingleLastBetActions.Add(Side, Action);
	bHasBetBulletAction = true;
	bBetBulletActionIsMultiplayer = false;
	BetBulletActionSide = Side;
	BetBulletActionSlot = EShowDownPlayerSlot::None;
	BetBulletAction = Action;
	bHasBetBulletRouletteTarget = false;
}

void AShowDownGameModeBase::RecordMultiplayerBetBulletAction(
	ASDPlayerState* Player,
	EShowDownBetAction Action)
{
	if (!Player)
	{
		return;
	}

	MultiplayerLastBetActions.Add(Player->ShowDownSlot, Action);
	bHasBetBulletAction = true;
	bBetBulletActionIsMultiplayer = true;
	BetBulletActionSide = EShowDownSide::Player;
	BetBulletActionSlot = Player->ShowDownSlot;
	BetBulletAction = Action;
	bHasBetBulletRouletteTarget = false;
}

void AShowDownGameModeBase::MarkSingleBetBulletRouletteTarget(EShowDownSide TargetSide, int32 BulletCount)
{
	bHasBetBulletRouletteTarget = true;
	bBetBulletRouletteTargetIsMultiplayer = false;
	BetBulletRouletteTargetSide = TargetSide;
	BetBulletRouletteTargetSlot = EShowDownPlayerSlot::None;
	BetBulletRouletteBulletCount = FMath::Clamp(BulletCount, 0, 6);
}

void AShowDownGameModeBase::MarkMultiplayerBetBulletRouletteTarget(ASDPlayerState* TargetPlayer, int32 BulletCount)
{
	if (!TargetPlayer)
	{
		return;
	}

	bHasBetBulletRouletteTarget = true;
	bBetBulletRouletteTargetIsMultiplayer = true;
	BetBulletRouletteTargetSide = EShowDownSide::Player;
	BetBulletRouletteTargetSlot = TargetPlayer->ShowDownSlot;
	BetBulletRouletteBulletCount = FMath::Clamp(BulletCount, 0, 6);
}

void AShowDownGameModeBase::ClearBetBulletTransientState()
{
	bHasBetBulletAction = false;
	bBetBulletActionIsMultiplayer = false;
	BetBulletActionSide = EShowDownSide::Player;
	BetBulletActionSlot = EShowDownPlayerSlot::None;
	BetBulletAction = EShowDownBetAction::Check;
	bHasBetBulletRouletteTarget = false;
	bBetBulletRouletteTargetIsMultiplayer = false;
	BetBulletRouletteTargetSide = EShowDownSide::Player;
	BetBulletRouletteTargetSlot = EShowDownPlayerSlot::None;
	BetBulletRouletteBulletCount = 0;
}

void AShowDownGameModeBase::ClearBetBulletActionHistory()
{
	SingleLastBetActions.Reset();
	MultiplayerLastBetActions.Reset();
}

FString AShowDownGameModeBase::BuildBetBulletActionText(EShowDownBetAction Action) const
{
	switch (Action)
	{
	case EShowDownBetAction::Check:
		return TEXT("CHECK");
	case EShowDownBetAction::Call:
		return TEXT("CALL");
	case EShowDownBetAction::Raise:
		return TEXT("RAISE");
	case EShowDownBetAction::Fold:
		return TEXT("FOLD");
	default:
		return FString();
	}
}

float AShowDownGameModeBase::PlaySinglePlayerCardRevealPresentation()
{
	TArray<ACard*> RevealCards;
	if (PlayerState.ForeheadCard)
	{
		RevealCards.Add(PlayerState.ForeheadCard);
	}
	if (CollectorState.ForeheadCard)
	{
		RevealCards.Add(CollectorState.ForeheadCard);
	}

	return PlayCardRevealPresentation(RevealCards);
}

float AShowDownGameModeBase::PlayMultiplayerCardRevealPresentation(const TArray<ASDPlayerState*>& RevealedPlayers)
{
	TArray<ASDPlayerState*> OrderedPlayers = RevealedPlayers;
	// Folding removes a player from the showdown result, but it must not hide
	// their card from the round's reveal presentation.
	for (ASDPlayerState* FoldedPlayer : MultiplayerFoldedPlayers)
	{
		if (FoldedPlayer && FoldedPlayer->ForeheadCard)
		{
			OrderedPlayers.AddUnique(FoldedPlayer);
		}
	}
	RefreshCentralGunStatus();
	OrderedPlayers.Sort(SortByMultiplayerTurnOrder);

	TArray<ACard*> RevealCards;
	for (ASDPlayerState* Player : OrderedPlayers)
	{
		if (Player && Player->ForeheadCard)
		{
			RevealCards.Add(Player->ForeheadCard);
		}
	}

	return PlayCardRevealPresentation(RevealCards);
}

float AShowDownGameModeBase::PlayCardRevealPresentation(const TArray<ACard*>& Cards)
{
	ClearCardRevealPresentationTimers();

	TArray<ACard*> ValidCards;
	for (ACard* Card : Cards)
	{
		if (IsValid(Card))
		{
			ValidCards.Add(Card);
		}
	}

	if (ValidCards.Num() <= 0)
	{
		return 0.0f;
	}

	if (!bUseCardRevealPresentation)
	{
		for (ACard* Card : ValidCards)
		{
			Card->SetHiddenFromSlot(EShowDownPlayerSlot::None);
			Card->SetFaceUp(true);
		}
		return 0.0f;
	}

	float TotalSeconds = 0.0f;
	const int32 CardCount = ValidCards.Num();
	for (int32 CardIndex = 0; CardIndex < CardCount; ++CardIndex)
	{
		ACard* Card = ValidCards[CardIndex];
		const float RevealDelay = FMath::Max(0.0f, CardRevealLeadInSeconds + CardRevealStepSeconds * CardIndex);
		const float CardMotionSeconds = Card->GetRevealMotionTotalSeconds();
		TotalSeconds = FMath::Max(TotalSeconds, RevealDelay + CardMotionSeconds);

		TWeakObjectPtr<ACard> WeakCard(Card);
		const auto RevealCard = [this, WeakCard, CardIndex, CardCount]()
		{
			ACard* RevealCardActor = WeakCard.Get();
			if (!IsValid(RevealCardActor))
			{
				return;
			}

			const FTransform RevealTransform = BuildCardRevealPresentationTransform(
				RevealCardActor,
				CardIndex,
				CardCount);
			RevealCardActor->MoveToRevealTransform(RevealTransform, CardRevealVisualScale);
		};

		if (RevealDelay <= KINDA_SMALL_NUMBER)
		{
			RevealCard();
			continue;
		}

		FTimerHandle TimerHandle;
		GetWorldTimerManager().SetTimer(TimerHandle, FTimerDelegate::CreateWeakLambda(this, RevealCard), RevealDelay, false);
		CardRevealPresentationTimerHandles.Add(TimerHandle);
	}

	return TotalSeconds + FMath::Max(0.0f, CardRevealHoldSeconds);
}

void AShowDownGameModeBase::ClearCardRevealPresentationTimers()
{
	GetWorldTimerManager().ClearTimer(MultiplayerRevealContinuationTimerHandle);
	for (FTimerHandle& TimerHandle : CardRevealPresentationTimerHandles)
	{
		GetWorldTimerManager().ClearTimer(TimerHandle);
	}
	CardRevealPresentationTimerHandles.Reset();
}

void AShowDownGameModeBase::ClearMultiplayerRoundTimers()
{
	for (FTimerHandle& TimerHandle : MultiplayerRoundTimerHandles)
	{
		GetWorldTimerManager().ClearTimer(TimerHandle);
	}
	MultiplayerRoundTimerHandles.Reset();
	ClearPendingMultiplayerGunResult();
}

void AShowDownGameModeBase::ArmMultiplayerGunResult(
	ASDSelfShotGunActor* GunActor,
	float FallbackDelay,
	TFunction<void()>&& ResultContinuation)
{
	if (!GunActor || !ResultContinuation)
	{
		return;
	}

	if (MultiplayerGunResultContinuation)
	{
		UE_LOG(LogTemp, Warning, TEXT("A multiplayer gun result was still pending. Resolving it before arming the next shot."));
		ResolvePendingMultiplayerGunResult();
	}

	MultiplayerResultGunActor = GunActor;
	MultiplayerGunResultContinuation = MoveTemp(ResultContinuation);
	GunActor->OnGunFired.AddUniqueDynamic(this, &AShowDownGameModeBase::HandleMultiplayerGunShotResolved);
	GunActor->OnGunEmptyFired.AddUniqueDynamic(this, &AShowDownGameModeBase::HandleMultiplayerGunShotResolved);
	GunActor->OnGunPresentationFinished.AddUniqueDynamic(
		this,
		&AShowDownGameModeBase::HandleMultiplayerGunPresentationFinished);

	GetWorldTimerManager().SetTimer(
		MultiplayerGunResultFallbackTimerHandle,
		this,
		&AShowDownGameModeBase::ResolvePendingMultiplayerGunResult,
		FMath::Max(0.05f, FallbackDelay),
		false);
}

void AShowDownGameModeBase::HandleMultiplayerGunShotResolved()
{
	ResolvePendingMultiplayerGunResult();
}

void AShowDownGameModeBase::HandleMultiplayerGunPresentationFinished()
{
	// The fire/empty delegates are the normal path. This protects state if a
	// presentation is interrupted after it starts but before either delegate.
	ResolvePendingMultiplayerGunResult();
}

void AShowDownGameModeBase::ResolvePendingMultiplayerGunResult()
{
	TFunction<void()> ResultContinuation = MoveTemp(MultiplayerGunResultContinuation);
	ClearPendingMultiplayerGunResult();
	if (ResultContinuation)
	{
		ResultContinuation();
	}
}

void AShowDownGameModeBase::ClearPendingMultiplayerGunResult()
{
	GetWorldTimerManager().ClearTimer(MultiplayerGunResultFallbackTimerHandle);
	if (ASDSelfShotGunActor* GunActor = MultiplayerResultGunActor.Get())
	{
		GunActor->OnGunFired.RemoveDynamic(this, &AShowDownGameModeBase::HandleMultiplayerGunShotResolved);
		GunActor->OnGunEmptyFired.RemoveDynamic(this, &AShowDownGameModeBase::HandleMultiplayerGunShotResolved);
		GunActor->OnGunPresentationFinished.RemoveDynamic(
			this,
			&AShowDownGameModeBase::HandleMultiplayerGunPresentationFinished);
	}
	MultiplayerResultGunActor.Reset();
	MultiplayerGunResultContinuation = TFunction<void()>();
}

FTransform AShowDownGameModeBase::BuildCardRevealPresentationTransform(
	ACard* Card,
	int32 CardIndex,
	int32 CardCount) const
{
	if (!Card)
	{
		return FTransform::Identity;
	}

	const FVector TableCenter = ResolveSingleTableCenter(GetWorld());
	EShowDownPlayerSlot RevealSlot = EShowDownPlayerSlot::None;
	for (ASDPlayerState* Player : MultiplayerPlayers)
	{
		if (IsValid(Player) && Player->ForeheadCard == Card)
		{
			RevealSlot = Player->ShowDownSlot;
			break;
		}
	}
	if (RevealSlot == EShowDownPlayerSlot::None)
	{
		for (ASDPlayerState* FoldedPlayer : MultiplayerFoldedPlayers)
		{
			if (IsValid(FoldedPlayer) && FoldedPlayer->ForeheadCard == Card)
			{
				RevealSlot = FoldedPlayer->ShowDownSlot;
				break;
			}
		}
	}
	if (RevealSlot == EShowDownPlayerSlot::None)
	{
		// Before the first reveal, HiddenFromSlot is the recipient. It is cleared
		// by MoveToRevealTransform, so persistent PlayerState ownership above wins.
		RevealSlot = Card->HiddenFromSlot;
	}
	if (RevealSlot != EShowDownPlayerSlot::None)
	{
		FVector SeatLocation = FVector::ZeroVector;
		bool bHasSeatLocation = false;
		if (const AShowDownCharacter* Character = FindActiveCharacterForPlayerSlot(GetWorld(), RevealSlot))
		{
			SeatLocation = Character->GetActorLocation();
			bHasSeatLocation = true;
		}
		else if (const ASDCardPlacementAnchor* ForeheadAnchor = GetForeheadAnchorForPlayerSlot(RevealSlot))
		{
			const USceneComponent* ForeheadSlot = ForeheadAnchor->GetSlotComponent();
			SeatLocation = ForeheadSlot
				? ForeheadSlot->GetComponentLocation()
				: ForeheadAnchor->GetActorLocation();
			bHasSeatLocation = true;
		}

		FTransform RadialRevealTransform;
		if (bHasSeatLocation
			&& ShowDownCardRevealLayout::TryBuildRadialTransform(
				TableCenter,
				SeatLocation,
				CardRevealSideSpacing,
				CardRevealTableYaw,
				CardRevealHeightOffset,
				CardRevealRotationOffset,
				Card->GetActorScale3D(),
				RadialRevealTransform))
		{
			return RadialRevealTransform;
		}
	}

	// Single-player cards have no network player slot. Keep their authored linear
	// layout while multiplayer cards use CardRevealSideSpacing as a center radius.
	const FRotator TableRotation(0.0f, CardRevealTableYaw, 0.0f);
	const FVector ForwardDirection = FRotationMatrix(TableRotation).GetUnitAxis(EAxis::X);
	const float CenteredIndex =
		static_cast<float>(CardIndex) - (static_cast<float>(CardCount) - 1.0f) * 0.5f;
	const FVector RevealLocation =
		TableCenter
		+ ForwardDirection * (CardRevealForwardDistance + CardRevealSideSpacing * CenteredIndex)
		+ FVector::UpVector * CardRevealHeightOffset;
	const FQuat RevealRotation =
		(TableRotation.Quaternion() * CardRevealRotationOffset.Quaternion()).GetNormalized();

	return FTransform(RevealRotation, RevealLocation, Card->GetActorScale3D());
}

void AShowDownGameModeBase::ApplyRouletteResult(EShowDownSide TargetSide, int32 BulletCount, TFunction<void()>&& Continuation)
{
	if (!RouletteSystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot roll roulette. RouletteSystem is missing."));
		if (Continuation)
		{
			Continuation();
		}
		return;
	}

	const int32 ClampedBulletCount = FMath::Clamp(BulletCount, 1, 6);
	const float HitChance = RouletteSystem->GetHitChance(ClampedBulletCount);
	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->SetPhase(EShowDownPhase::Roulette);
		ShowDownGameState->OnRouletteStarted.Broadcast(TargetSide, ClampedBulletCount);
		ShowDownGameState->SetNameTagSingleRoundStatus(
			PlayerState.CurrentBet,
			CollectorState.CurrentBet,
			TargetSide,
			EShowDownPlayerSlot::None);
	}
	MarkSingleBetBulletRouletteTarget(TargetSide, ClampedBulletCount);
	RefreshBetBulletPresentation();
	const bool bHit = RouletteSystem->RollRoulette(ClampedBulletCount);

	UE_LOG(LogTemp, Log, TEXT("%s roulette: %d/6, %.0f%% chance, result: %s"),
		TargetSide == EShowDownSide::Player ? TEXT("Player") : TEXT("Collector"),
		ClampedBulletCount,
		HitChance * 100.0f,
		bHit ? TEXT("Hit") : TEXT("Miss"));

	if (!bHit)
	{
		auto ResolveMiss = [this, TargetSide, ClampedBulletCount]()
		{
			BroadcastSystemChatMessage(FString::Printf(
				TEXT("%s님이 %d발 룰렛을 피했습니다."),
				TargetSide == EShowDownSide::Player ? TEXT("Player") : TEXT("Collector"),
				ClampedBulletCount));
			ShowEventDebugMessage(FString::Printf(TEXT("룰렛: %s %d발 / 안 맞음"),
				*GetSideDisplayText(TargetSide),
				ClampedBulletCount));
		};
		PlaySelfShotGunPresentationThen(
			TargetSide,
			false,
			MoveTemp(ResolveMiss),
			MoveTemp(Continuation));
		return;
	}

	auto ResolveHit = [this, TargetSide, ClampedBulletCount]()
	{
		FShowDownParticipantState& TargetState = TargetSide == EShowDownSide::Player ? PlayerState : CollectorState;
		TargetState.Lives = FMath::Max(0, TargetState.Lives - 1);
		if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
		{
			ShowDownGameState->OnLifeChanged.Broadcast(TargetSide, TargetState.Lives);
		}
		BroadcastSystemChatMessage(FString::Printf(
			TEXT("%s님이 %d발 룰렛에 맞았습니다. 남은 목숨: %d"),
			TargetSide == EShowDownSide::Player ? TEXT("Player") : TEXT("Collector"),
			ClampedBulletCount,
			TargetState.Lives));
		if (TargetState.Lives <= 0)
		{
			BroadcastSystemChatMessage(FString::Printf(
				TEXT("%s님이 사망했습니다."),
				TargetSide == EShowDownSide::Player ? TEXT("Player") : TEXT("Collector")));
		}
		ShowEventDebugMessage(FString::Printf(TEXT("룰렛: %s %d발 / 총 맞음 / 목숨 %d"),
			*GetSideDisplayText(TargetSide),
			ClampedBulletCount,
			TargetState.Lives));
		UE_LOG(LogTemp, Log, TEXT("%s lives: %d"),
			TargetSide == EShowDownSide::Player ? TEXT("Player") : TEXT("Collector"),
			TargetState.Lives);
	};
	PlaySelfShotGunPresentationThen(
		TargetSide,
		true,
		MoveTemp(ResolveHit),
		MoveTemp(Continuation));
}

void AShowDownGameModeBase::EndRound()
{
	bBettingPhase = false;
	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->SetPhase(EShowDownPhase::RoundEnd);
		ShowDownGameState->SetNameTagSingleRoundStatus(0, 0, NextRoundFirstSide, EShowDownPlayerSlot::None);
	}

	ClearBetBulletPresentation();
	ClearForeheadCards();

	if (PlayerState.Lives <= 0)
	{
		if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
		{
			ShowDownGameState->SetPhase(EShowDownPhase::GameOver);
			ShowDownGameState->OnGameOver.Broadcast(EShowDownSide::Collector);
		}
		ShowEventDebugMessage(TEXT("게임 종료 - 콜렉터 승리"));
		UE_LOG(LogTemp, Log, TEXT("Game Over. Player has no lives left."));
		return;
	}

	if (CollectorState.Lives <= 0)
	{
		UE_LOG(LogTemp, Log, TEXT("Stage %d cleared. Collector has no lives left."), CurrentStageIndex + 1);
		AdvanceStage();
		return;
	}

	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->CurrentRound++;
	}

	const bool bNeedRedeal = PlayerState.HandCards.Num() <= 0 || CollectorState.HandCards.Num() <= 0;
	if (bNeedRedeal)
	{
		UE_LOG(LogTemp, Log, TEXT("Hands are empty. Dealing the remaining deck without replaying the full-card showcase."));
		StartHandRedealPresentation(false, [this]()
		{
			BeginCardSelectionRound();
		});
		return;
	}

	BeginCardSelectionRound();
	UE_LOG(LogTemp, Log, TEXT("Next round ready. Player hand: %d, Collector hand: %d"),
		PlayerState.HandCards.Num(),
		CollectorState.HandCards.Num());
}

void AShowDownGameModeBase::ClearForeheadCards()
{
	TArray<int32> DiscardRanks;

	if (PlayerState.ForeheadCard)
	{
		DiscardRanks.Add(PlayerState.ForeheadCard->Rank);
		PlayerState.ForeheadCard->Destroy();
		PlayerState.ForeheadCard = nullptr;
	}

	if (CollectorState.ForeheadCard)
	{
		DiscardRanks.Add(CollectorState.ForeheadCard->Rank);
		CollectorState.ForeheadCard->Destroy();
		CollectorState.ForeheadCard = nullptr;
	}

	if (CardSystem && DiscardRanks.Num() > 0)
	{
		CardSystem->DiscardCards(DiscardRanks);
	}
	for (int32 Rank : DiscardRanks)
	{
		if (!DiscardedCardsSummary.IsEmpty())
		{
			DiscardedCardsSummary += TEXT(", ");
		}
		DiscardedCardsSummary += FString::FromInt(Rank);
	}
	DiscardedCardsSummary = DiscardedCardsSummary.Right(240);
}

void AShowDownGameModeBase::ClearHandCards()
{
	for (ACard* Card : PlayerState.HandCards)
	{
		if (Card)
		{
			Card->Destroy();
		}
	}
	PlayerState.HandCards.Reset();

	for (ACard* Card : CollectorState.HandCards)
	{
		if (Card)
		{
			Card->Destroy();
		}
	}
	CollectorState.HandCards.Reset();
}

void AShowDownGameModeBase::SetPlayerHandSelectable(bool bSelectable)
{
	for (ACard* Card : PlayerState.HandCards)
	{
		if (Card)
		{
			Card->SetSelectable(bSelectable);
		}
	}
}

FSDCardHandLayoutSettings AShowDownGameModeBase::GetDefaultHandLayoutSettings() const
{
	FSDCardHandLayoutSettings Settings;
	Settings.CardSpacing = CardSpacing;
	Settings.ForwardOffset = ForwardOffset;
	Settings.HeightOffset = HeightOffset;
	Settings.LeanAngle = LeanAngle;
	Settings.LayerStep = LayerStep;
	return Settings;
}

FSDCardHandLayoutSettings AShowDownGameModeBase::ResolveHandLayoutSettings(EShowDownSide Side) const
{
	FSDCardHandLayoutSettings Settings = GetDefaultHandLayoutSettings();

	if (const ASDCardPlacementAnchor* HandAnchor = GetHandAnchorForSide(Side))
	{
		Settings.CardSpacing = HandAnchor->CardSpacing;
		Settings.ForwardOffset = HandAnchor->ForwardOffset;
		Settings.HeightOffset = HandAnchor->HeightOffset;
		Settings.LeanAngle = HandAnchor->LeanAngle;
		Settings.LayerStep = HandAnchor->LayerStep;
		return Settings;
	}

	if (const ASDPlayerSeat* Seat = GetSeatForSide(Side))
	{
		if (Seat->bOverrideGameModeHandLayout)
		{
			Settings.CardSpacing = Seat->HandSpacing;
			Settings.ForwardOffset = Seat->HandForwardOffset;
			Settings.HeightOffset = Seat->HandHeightOffset;
			Settings.LeanAngle = Seat->HandLeanAngle;
		}
	}

	return Settings;
}

void AShowDownGameModeBase::ApplyCardMotionForSide(EShowDownSide Side, const TArray<ACard*>& Cards) const
{
	const ASDCardPlacementAnchor* HandAnchor = GetHandAnchorForSide(Side);
	const ASDPlayerSeat* Seat = GetSeatForSide(Side);
	if (!HandAnchor && (!Seat || !Seat->bOverrideCardMotion))
	{
		return;
	}

	for (ACard* Card : Cards)
	{
		if (!Card)
		{
			continue;
		}

		Card->SelectedOffset = HandAnchor ? HandAnchor->SelectedOffset : Seat->SelectedOffset;
		Card->HoverOffset = HandAnchor ? HandAnchor->HoverOffset : Seat->HoverOffset;
		Card->MoveSpeed = HandAnchor ? HandAnchor->MoveSpeed : Seat->MoveSpeed;
	}
}

void AShowDownGameModeBase::ReflowHandCards(EShowDownSide Side)
{
	if (!CardSystem)
	{
		return;
	}

	USceneComponent* HandSlot = GetHandSlotForSide(Side);
	if (!HandSlot)
	{
		return;
	}

	const TArray<ACard*>& Cards = Side == EShowDownSide::Collector
		? CollectorState.HandCards
		: PlayerState.HandCards;

	CardSystem->LayoutHandCards(this, HandSlot, ResolveHandLayoutSettings(Side), Cards);
	ApplyCardMotionForSide(Side, Cards);
}

void AShowDownGameModeBase::RefreshNetworkPlayerSlots()
{
	AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState();
	if (!ShowDownGameState)
	{
		return;
	}

	ShowDownGameState->SetMatchMode(GetNetMode() == NM_Standalone
		? EShowDownMatchMode::SinglePlayer
		: EShowDownMatchMode::Multiplayer);

	TSet<EShowDownPlayerSlot> SeenSlots;
	TArray<ASDPlayerState*> NetworkPlayers;
	if (UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			if (APlayerController* PlayerController = Iterator->Get())
			{
				if (IsActiveNetworkPlayerController(PlayerController))
				{
					if (ASDPlayerState* ShowDownPlayerState = PlayerController->GetPlayerState<ASDPlayerState>())
					{
						NetworkPlayers.AddUnique(ShowDownPlayerState);
					}
				}
			}
		}
	}

	NetworkPlayers.Sort([](const ASDPlayerState& Left, const ASDPlayerState& Right)
	{
		return static_cast<uint8>(Left.ShowDownSlot) < static_cast<uint8>(Right.ShowDownSlot);
	});

	int32 PlayerIndex = 0;
	for (ASDPlayerState* ShowDownPlayerState : NetworkPlayers)
	{
		if (!ShowDownPlayerState)
		{
			continue;
		}

		if (!bMultiplayerMatchStarted)
		{
			ShowDownPlayerState->SetShowDownSlot(GetPlayerSlotByIndex(PlayerIndex));
			ShowDownPlayerState->SetHostPlayer(PlayerIndex == 0);
		}
		else if (ShowDownPlayerState->ShowDownSlot == EShowDownPlayerSlot::None)
		{
			ShowDownPlayerState->SetShowDownSlot(FindNextOpenPlayerSlot());
		}

		if (ShowDownPlayerState->ShowDownSlot == EShowDownPlayerSlot::None)
		{
			continue;
		}

		const FString DisplayName = GetNetworkPlayerDisplayName(ShowDownPlayerState);

		FShowDownNetworkPlayerSlot SlotState;
		SlotState.Slot = ShowDownPlayerState->ShowDownSlot;
		SlotState.PlayerId = FString::FromInt(ShowDownPlayerState->GetPlayerId());
		SlotState.DisplayName = DisplayName;
		SlotState.bConnected = true;
		SlotState.bReady = ShowDownPlayerState->bReady;

		ShowDownGameState->SetPlayerSlot(SlotState);
		SeenSlots.Add(SlotState.Slot);
		++PlayerIndex;
	}

	const EShowDownPlayerSlot AllSlots[] = {
		EShowDownPlayerSlot::Player1,
		EShowDownPlayerSlot::Player2,
		EShowDownPlayerSlot::Player3,
		EShowDownPlayerSlot::Player4
	};

	for (EShowDownPlayerSlot Slot : AllSlots)
	{
		if (!SeenSlots.Contains(Slot))
		{
			ShowDownGameState->ClearPlayerSlot(Slot);
		}
	}

	if (HasAuthority() && GetNetMode() != NM_Standalone)
	{
		TArray<ASDPlayerState*> CurrentPlayers;
		const TArray<ASDPlayerState*>& CharacterPlayers =
			bMultiplayerMatchStarted && MultiplayerPlayers.Num() > 0
				? MultiplayerPlayers
				: NetworkPlayers;
		for (ASDPlayerState* Player : CharacterPlayers)
		{
			CurrentPlayers.Add(Player);
		}
		ConfigureMultiplayerCharacters(CurrentPlayers);
	}
}

EShowDownPlayerSlot AShowDownGameModeBase::FindNextOpenPlayerSlot() const
{
	TSet<EShowDownPlayerSlot> UsedSlots;
	if (UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			const APlayerController* PlayerController = Iterator->Get();
			if (!IsActiveNetworkPlayerController(PlayerController))
			{
				continue;
			}

			const ASDPlayerState* ShowDownPlayerState = PlayerController
				? PlayerController->GetPlayerState<ASDPlayerState>()
				: nullptr;
			if (ShowDownPlayerState && ShowDownPlayerState->ShowDownSlot != EShowDownPlayerSlot::None)
			{
				UsedSlots.Add(ShowDownPlayerState->ShowDownSlot);
			}
		}
	}

	const EShowDownPlayerSlot AllSlots[] = {
		EShowDownPlayerSlot::Player1,
		EShowDownPlayerSlot::Player2,
		EShowDownPlayerSlot::Player3,
		EShowDownPlayerSlot::Player4
	};

	for (EShowDownPlayerSlot Slot : AllSlots)
	{
		if (!UsedSlots.Contains(Slot))
		{
			return Slot;
		}
	}

	return EShowDownPlayerSlot::None;
}

void AShowDownGameModeBase::TryStartMultiplayerMatch()
{
	if (bMultiplayerMatchStarted)
	{
		return;
	}

	RefreshNetworkPlayerSlots();

	TArray<ASDPlayerState*> Players = GetConnectedShowDownPlayers();
	int32 RequiredPlayerCount = 2;
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UShowDownEosSubsystem* EosSubsystem = GameInstance->GetSubsystem<UShowDownEosSubsystem>())
		{
			RequiredPlayerCount = FMath::Clamp(EosSubsystem->GetExpectedLobbyPlayerCount(), 2, 4);
		}
	}
	if (Players.Num() < RequiredPlayerCount)
	{
		int32 ControllerCount = 0;
		if (UWorld* World = GetWorld())
		{
			for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
				++ControllerCount;
			}
		}

		const int32 PlayerArrayCount = GameState ? GameState->PlayerArray.Num() : 0;
		NotifyMultiplayerStatus(FString::Printf(
			TEXT("로비 참가자 도착 대기 중... %d/%d (플레이어 상태=%d, 컨트롤러=%d)"),
			Players.Num(),
			RequiredPlayerCount,
			PlayerArrayCount,
			ControllerCount));
		GetWorldTimerManager().SetTimer(
			MultiplayerStartTimerHandle,
			this,
			&AShowDownGameModeBase::TryStartMultiplayerMatch,
			1.0f,
			false);
		return;
	}

	StartMultiplayerMatch(Players);
}

void AShowDownGameModeBase::StartMultiplayerMatch(const TArray<ASDPlayerState*>& Players)
{
	GetWorldTimerManager().ClearTimer(MultiplayerStartTimerHandle);
	ClearMultiplayerRoundTimers();
	ClearCardRevealPresentationTimers();
	ClearInitialCardDealPresentation();
	bInitialCardDealPresentationPlayed = false;
	bBettingPhase = false;
	bMultiplayerRoundResolving = false;
	MultiplayerCurrentBetter = nullptr;
	MultiplayerLastCheckedPlayer = nullptr;
	SetMultiplayerSelectableHand(nullptr);
	ClearBetActionPanel();
	ClearBetBulletPresentation();
	ClearMultiplayerForeheadCards();
	ClearMultiplayerHands();
	ClearLooseMultiplayerCards();

	bMultiplayerMatchStarted = true;
	MultiplayerPlayers.Reset();
	MultiplayerEliminationOrder.Reset();
	MultiplayerRestartVotes.Reset();
	MultiplayerFoldedPlayers.Reset();
	MultiplayerPlayersActed.Reset();
	MultiplayerRoundSpectators.Reset();
	MultiplayerRoundLeader = nullptr;
	MultiplayerDuelA = nullptr;
	MultiplayerDuelB = nullptr;
	MultiplayerNextFirstPlayer = nullptr;
	MultiplayerLiveRoundCount = 0;
	MultiplayerRemainingChamberCount = 6;
	MultiplayerSharedChamberIndex = 0;
	MultiplayerSharedChambers.Reset();

	for (ASDPlayerState* Player : Players)
	{
		if (!Player || MultiplayerPlayers.Num() >= 4)
		{
			continue;
		}

		Player->Lives = StageRules.Num() > 0 ? StageRules[0].StartingLives : 3;
		Player->CurrentBet = 0;
		Player->ForeheadCard = nullptr;
		Player->ClearHand();
		MultiplayerPlayers.Add(Player);
	}
	MultiplayerPlayers.Sort(SortByMultiplayerTurnOrder);

	TArray<ASDPlayerState*> CurrentPlayers;
	for (ASDPlayerState* Player : MultiplayerPlayers)
	{
		CurrentPlayers.Add(Player);
	}
	ConfigureMultiplayerCharacters(CurrentPlayers);
	EnsureMultiplayerSeatAnchors();

	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->SetMatchMode(EShowDownMatchMode::Multiplayer);
		ShowDownGameState->CurrentStage = 1;
		ShowDownGameState->CurrentRound = 1;
		ShowDownGameState->SetPhase(EShowDownPhase::None);
	}

	// The lobby leaves its controller in UI-only mode. This client RPC runs on
	// every local player after travel so mouse-look, card clicks, and hotkeys are
	// held in a loading state until the authoritative seat camera is applied.
	if (UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			if (AShowDownPlayerController* PlayerController = Cast<AShowDownPlayerController>(Iterator->Get()))
			{
				PlayerController->ClientEnterMultiplayerGameplay();
			}
		}
	}

	EnsureMultiplayerPawns();
	auto StartFirstDuel = [this]()
	{
		if (MultiplayerPlayers.Num() < 2)
		{
			return;
		}

		ASDPlayerState* InitialFirstPlayer = MultiplayerPlayers[0];
		for (ASDPlayerState* Player : MultiplayerPlayers)
		{
			if (Player && Player->bHostPlayer)
			{
				InitialFirstPlayer = Player;
				break;
			}
		}

		StartMultiplayerDuel(InitialFirstPlayer, FindNextAliveMultiplayerPlayer(InitialFirstPlayer));
	};

	if (bUseInitialCardDealPresentation && !bInitialCardDealPresentationPlayed)
	{
		StartInitialCardDealPresentation(MultiplayerPlayers.Num(), true, MoveTemp(StartFirstDuel));
	}
	else
	{
		DealMultiplayerHands();
		StartFirstDuel();
	}
}

TArray<ASDPlayerState*> AShowDownGameModeBase::GetConnectedShowDownPlayers() const
{
	TArray<ASDPlayerState*> Players;
	if (UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			const APlayerController* PlayerController = Iterator->Get();
			if (!IsActiveNetworkPlayerController(PlayerController))
			{
				continue;
			}

			ASDPlayerState* Player = PlayerController ? PlayerController->GetPlayerState<ASDPlayerState>() : nullptr;
			if (Player && Player->ShowDownSlot != EShowDownPlayerSlot::None)
			{
				Players.AddUnique(Player);
			}
		}
	}

	Players.Sort(SortByMultiplayerTurnOrder);

	return Players;
}

ASDPlayerState* AShowDownGameModeBase::GetPlayerStateForController(AController* Controller) const
{
	return Controller ? Controller->GetPlayerState<ASDPlayerState>() : nullptr;
}

void AShowDownGameModeBase::EnsureMultiplayerPawns()
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority())
	{
		return;
	}

	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PlayerController = Iterator->Get();
		ASDPlayerState* Player = PlayerController ? PlayerController->GetPlayerState<ASDPlayerState>() : nullptr;
		if (!Player || !MultiplayerPlayers.Contains(Player))
		{
			continue;
		}

		// A controller iterator is not guaranteed to keep the lobby join order.
		// The replicated PlayerState slot is the authoritative seat assignment and
		// must drive both the pawn spawn and the local seat camera.
		const int32 SeatIndex = GetSeatIndexFromPlayerSlot(Player->ShowDownSlot);
		if (SeatIndex == INDEX_NONE)
		{
			continue;
		}

		UE_LOG(
			LogTemp,
			Log,
			TEXT("멀티플레이 좌석 배정: Player=%s Slot=%d SeatIndex=%d Controller=%s"),
			*Player->GetPlayerName(),
			static_cast<int32>(Player->ShowDownSlot),
			SeatIndex,
			PlayerController ? *PlayerController->GetName() : TEXT("None"));

		const FTransform SpawnTransform = GetMultiplayerPawnSpawnTransform(PlayerController, SeatIndex);
		const FRotator GameplayViewRotation(-12.0f, SpawnTransform.Rotator().Yaw, 0.0f);
		if (APlayerPawn* ExistingPawn = Cast<APlayerPawn>(PlayerController->GetPawn()))
		{
			ExistingPawn->SetOwner(PlayerController);
			ExistingPawn->SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
			PlayerController->SetControlRotation(GameplayViewRotation);
			if (AShowDownPlayerController* ShowDownController = Cast<AShowDownPlayerController>(PlayerController))
			{
				ShowDownController->ClientUseMultiplayerSeatCamera(
					SeatIndex,
					GameplayCameraLookSensitivity,
					GameplayCameraMinPitch,
					GameplayCameraMaxPitch,
					GameplayCameraMinYawOffset,
					GameplayCameraMaxYawOffset,
					bInvertGameplayCameraMouseY,
					bEnableGameplayCameraBreathingSway,
					GameplayCameraBreathingSwaySpeed,
					GameplayCameraBreathingSwayRotationAmplitude,
					GameplayCameraBreathingSwayLocationAmplitude,
					GameplayCameraBreathingSwayBlendInTime);
			}
			continue;
		}

		if (APawn* ExistingPawn = PlayerController->GetPawn())
		{
			ExistingPawn->Destroy();
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = PlayerController;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		APlayerPawn* SpawnedPawn = World->SpawnActor<APlayerPawn>(
			APlayerPawn::StaticClass(),
			SpawnTransform,
			SpawnParams);

		if (SpawnedPawn)
		{
			PlayerController->Possess(SpawnedPawn);
			PlayerController->SetControlRotation(GameplayViewRotation);
			if (AShowDownPlayerController* ShowDownController = Cast<AShowDownPlayerController>(PlayerController))
			{
				ShowDownController->ClientUseMultiplayerSeatCamera(
					SeatIndex,
					GameplayCameraLookSensitivity,
					GameplayCameraMinPitch,
					GameplayCameraMaxPitch,
					GameplayCameraMinYawOffset,
					GameplayCameraMaxYawOffset,
					bInvertGameplayCameraMouseY,
					bEnableGameplayCameraBreathingSway,
					GameplayCameraBreathingSwaySpeed,
					GameplayCameraBreathingSwayRotationAmplitude,
					GameplayCameraBreathingSwayLocationAmplitude,
					GameplayCameraBreathingSwayBlendInTime);
			}
			UE_LOG(LogTemp, Log, TEXT("멀티플레이 Pawn 생성: %s / 플레이어: %s"), *SpawnedPawn->GetName(), *Player->GetPlayerName());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("멀티플레이 Pawn 생성 실패: %s"), *Player->GetPlayerName());
		}

	}
}

void AShowDownGameModeBase::EnsureMultiplayerSeatAnchors()
{
	if (!HasAuthority())
	{
		return;
	}

	for (ASDMultiplayerSeatAnchor* SeatAnchor : MultiplayerSeatAnchors)
	{
		if (SeatAnchor)
		{
			SeatAnchor->Destroy();
		}
	}
	MultiplayerSeatAnchors.Reset();

	// Multiplayer cards now use each player's pawn card slots. Keep this path as
	// cleanup for older spawned anchors, but do not create the old per-seat layout.
}

FTransform AShowDownGameModeBase::GetMultiplayerPawnSpawnTransform(AController* Controller, int32 PlayerIndex)
{
	const FVector TableCenter = ResolveSingleTableCenter(GetWorld());
	return BuildSingleTableSeatTransform(TableCenter, PlayerIndex);
}

ASDPlayerState* AShowDownGameModeBase::FindNextAliveMultiplayerPlayer(ASDPlayerState* AfterPlayer) const
{
	if (MultiplayerPlayers.Num() <= 0)
	{
		return nullptr;
	}

	int32 StartIndex = 0;
	if (AfterPlayer)
	{
		const int32 FoundIndex = MultiplayerPlayers.IndexOfByKey(AfterPlayer);
		StartIndex = FoundIndex == INDEX_NONE ? 0 : FoundIndex + 1;
	}

	for (int32 Offset = 0; Offset < MultiplayerPlayers.Num(); ++Offset)
	{
		const int32 Index = (StartIndex + Offset) % MultiplayerPlayers.Num();
		ASDPlayerState* Candidate = MultiplayerPlayers[Index];
		if (Candidate && Candidate->Lives > 0)
		{
			return Candidate;
		}
	}

	return nullptr;
}

ASDPlayerState* AShowDownGameModeBase::GetMultiplayerOpponent(ASDPlayerState* Player) const
{
	if (Player == MultiplayerDuelA)
	{
		return MultiplayerDuelB;
	}

	if (Player == MultiplayerDuelB)
	{
		return MultiplayerDuelA;
	}

	return nullptr;
}

void AShowDownGameModeBase::DealMultiplayerHands()
{
	if (!CardSystem || !CardClass)
	{
		NotifyMultiplayerStatus(TEXT("카드를 나눌 수 없습니다. 카드 시스템이 없습니다."));
		return;
	}

	ClearMultiplayerHands();
	ClearMultiplayerForeheadCards();

	int32 AlivePlayerCount = 0;
	for (const ASDPlayerState* Player : MultiplayerPlayers)
	{
		if (Player && Player->Lives > 0)
		{
			++AlivePlayerCount;
		}
	}
	AlivePlayerCount = FMath::Clamp(AlivePlayerCount, 2, 4);

	CardSystem->ResetDeck(AlivePlayerCount);
	CardSystem->ShuffleDeck();
	ActiveCardDeckCopies = AlivePlayerCount;
	InitialCardDealDeckCopies = AlivePlayerCount;
	UE_LOG(
		LogTemp,
		Log,
		TEXT("Multiplayer deck: ranks 1-7 x%d, total %d cards."),
		AlivePlayerCount,
		AlivePlayerCount * 7);

	for (ASDPlayerState* Player : MultiplayerPlayers)
	{
		if (!Player || Player->Lives <= 0)
		{
			continue;
		}

		Player->CurrentBet = 0;
		TArray<int32> Ranks;
		if (!CardSystem->DealCards(HandCount, Ranks))
		{
			NotifyMultiplayerStatus(TEXT("덱에 카드가 부족하여 모든 손패를 나눌 수 없습니다."));
			return;
		}

		USceneComponent* HandSlot = GetHandSlotForPlayerState(Player);
		if (!HandSlot)
		{
			NotifyMultiplayerStatus(FString::Printf(TEXT("%s의 손패 위치가 없습니다."), *Player->GetPlayerName()));
			continue;
		}

		const FSDCardHandLayoutSettings HandLayout = ResolveHandLayoutSettingsForPlayerState(Player);

		CardSystem->SpawnHandCards(
			this,
			CardClass,
			HandSlot,
			Ranks,
			HandLayout,
			true,
			false,
			Player->HandCards);

		ApplyCardMotionForPlayerState(Player, Player->HandCards);

		for (ACard* Card : Player->HandCards)
		{
			if (Card)
			{
				Card->SetHandOwnerSlot(Player->ShowDownSlot);
			}
		}
	}
	SetInitialDealDeckVisual(CardSystem->GetRemainingCardCount(), ActiveCardDeckCopies * 7);
}

void AShowDownGameModeBase::ClearMultiplayerHands()
{
	for (ASDPlayerState* Player : MultiplayerPlayers)
	{
		if (!Player)
		{
			continue;
		}

		for (ACard* Card : Player->HandCards)
		{
			if (Card)
			{
				Card->Destroy();
			}
		}
		Player->ClearHand();
	}
}

void AShowDownGameModeBase::ClearMultiplayerForeheadCards()
{
	TArray<int32> DiscardRanks;
	for (ASDPlayerState* Player : MultiplayerPlayers)
	{
		if (!Player || !Player->ForeheadCard)
		{
			continue;
		}

		DiscardRanks.Add(Player->ForeheadCard->Rank);
		Player->ForeheadCard->Destroy();
		Player->ForeheadCard = nullptr;
	}

	if (CardSystem && DiscardRanks.Num() > 0)
	{
		CardSystem->DiscardCards(DiscardRanks);
	}
}

void AShowDownGameModeBase::ClearLooseMultiplayerCards()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ACard> It(World); It; ++It)
	{
		ACard* Card = *It;
		if (!IsValid(Card))
		{
			continue;
		}

		const bool bLooksLikeMultiplayerCard =
			Card->HandOwnerSlot != EShowDownPlayerSlot::None
			|| Card->HiddenFromSlot != EShowDownPlayerSlot::None;
		if (bLooksLikeMultiplayerCard)
		{
			Card->Destroy();
		}
	}
}

void AShowDownGameModeBase::StartMultiplayerDuel(ASDPlayerState* FirstPlayer, ASDPlayerState* SecondPlayer)
{
	if (!FirstPlayer || !SecondPlayer || FirstPlayer == SecondPlayer)
	{
		return;
	}

	MultiplayerRoundLeader = FirstPlayer;
	MultiplayerDuelA = FirstPlayer;
	MultiplayerDuelB = SecondPlayer;
	MultiplayerLastCheckedPlayer = nullptr;
	MultiplayerFoldedPlayers.Reset();
	MultiplayerPlayersActed.Reset();
	MultiplayerRoundSpectators.Reset();
	bMultiplayerRoundResolving = false;

	if (!AreAllAliveMultiplayerPlayersReadyToReveal())
	{
		StartMultiplayerCardSelection();
	}

	NotifyMultiplayerStatus(FString::Printf(
		TEXT("%d 라운드 시작. 선행: %s, 참가자: %d명"),
		GetShowDownGameState() ? GetShowDownGameState()->CurrentRound : 1,
		*FirstPlayer->GetPlayerName(),
		MultiplayerPlayers.Num()));
}

bool AShowDownGameModeBase::AreAllAliveMultiplayerPlayersReadyToReveal() const
{
	for (const ASDPlayerState* Player : MultiplayerPlayers)
	{
		if (Player && Player->Lives > 0 && !MultiplayerFoldedPlayers.Contains(const_cast<ASDPlayerState*>(Player)) && !Player->ForeheadCard)
		{
			return false;
		}
	}

	return true;
}

bool AShowDownGameModeBase::AreAllActiveMultiplayerPlayersDoneBetting(int32 CurrentBet) const
{
	for (const ASDPlayerState* Player : MultiplayerPlayers)
	{
		if (!Player || Player->Lives <= 0 || MultiplayerFoldedPlayers.Contains(const_cast<ASDPlayerState*>(Player)))
		{
			continue;
		}

		if (Player->CurrentBet < CurrentBet || !MultiplayerPlayersActed.Contains(const_cast<ASDPlayerState*>(Player)))
		{
			return false;
		}
	}

	return true;
}

int32 AShowDownGameModeBase::CountActiveMultiplayerPlayers() const
{
	int32 ActiveCount = 0;
	for (ASDPlayerState* Player : MultiplayerPlayers)
	{
		if (IsValid(Player) && Player->Lives > 0 && !MultiplayerFoldedPlayers.Contains(Player))
		{
			++ActiveCount;
		}
	}

	return ActiveCount;
}

void AShowDownGameModeBase::HandleMultiplayerPlayerDisconnected(ASDPlayerState* LeavingPlayer)
{
	if (!HasAuthority() || !bMultiplayerMatchStarted || !IsValid(LeavingPlayer) || !MultiplayerPlayers.Contains(LeavingPlayer))
	{
		return;
	}

	const bool bRestartInitialDealAfterDisconnect = bInitialCardDealPresentationInProgress;
	if (bRestartInitialDealAfterDisconnect)
	{
		// Cancel every pending weak/raw movement callback before any owned card is
		// destroyed. The surviving players receive a fresh deck sized to the new
		// participant count below.
		ClearInitialCardDealPresentation();
	}

	const EShowDownPlayerSlot LeavingSlot = LeavingPlayer->ShowDownSlot;
	for (ACard* Card : LeavingPlayer->HandCards)
	{
		if (IsValid(Card))
		{
			Card->Destroy();
		}
	}
	LeavingPlayer->ClearHand();

	if (IsValid(LeavingPlayer->ForeheadCard))
	{
		LeavingPlayer->ForeheadCard->Destroy();
		LeavingPlayer->ForeheadCard = nullptr;
	}

	const bool bWasCurrentBetter = MultiplayerCurrentBetter == LeavingPlayer;

	MultiplayerPlayers.RemoveAll([LeavingPlayer](const TObjectPtr<ASDPlayerState>& Player)
	{
		return Player.Get() == LeavingPlayer;
	});
	for (auto FoldedIterator = MultiplayerFoldedPlayers.CreateIterator(); FoldedIterator; ++FoldedIterator)
	{
		if ((*FoldedIterator).Get() == LeavingPlayer)
		{
			FoldedIterator.RemoveCurrent();
			break;
		}
	}
	for (auto ActedIterator = MultiplayerPlayersActed.CreateIterator(); ActedIterator; ++ActedIterator)
	{
		if ((*ActedIterator).Get() == LeavingPlayer)
		{
			ActedIterator.RemoveCurrent();
			break;
		}
	}
	MultiplayerEliminationOrder.RemoveAll([LeavingPlayer](const TObjectPtr<ASDPlayerState>& Player)
	{
		return Player.Get() == LeavingPlayer;
	});
	for (auto VoteIterator = MultiplayerRestartVotes.CreateIterator(); VoteIterator; ++VoteIterator)
	{
		if ((*VoteIterator).Get() == LeavingPlayer)
		{
			VoteIterator.RemoveCurrent();
			break;
		}
	}

	if (MultiplayerRoundLeader == LeavingPlayer)
	{
		MultiplayerRoundLeader = FindNextAliveMultiplayerPlayer(nullptr);
	}
	if (MultiplayerDuelA == LeavingPlayer)
	{
		MultiplayerDuelA = MultiplayerRoundLeader;
	}
	if (MultiplayerDuelB == LeavingPlayer)
	{
		MultiplayerDuelB = FindNextAliveMultiplayerPlayer(MultiplayerDuelA);
	}
	if (MultiplayerNextFirstPlayer == LeavingPlayer)
	{
		MultiplayerNextFirstPlayer = FindNextAliveMultiplayerPlayer(nullptr);
	}
	if (bWasCurrentBetter)
	{
		MultiplayerCurrentBetter = nullptr;
	}

	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->SetNameTagPlayerLoadedBulletCount(LeavingSlot, 0);
	}

	RefreshMultiplayerCharacterVisibility();
	NotifyMultiplayerStatus(FString::Printf(TEXT("%s left the match."), *LeavingPlayer->GetPlayerName()));

	AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState();
	const EShowDownPhase CurrentPhase = ShowDownGameState
		? ShowDownGameState->CurrentPhase
		: EShowDownPhase::None;
	if (CurrentPhase == EShowDownPhase::GameOver)
	{
		return;
	}

	if (MultiplayerPlayers.Num() <= 1)
	{
		EndMultiplayerRound();
		return;
	}

	if (bRestartInitialDealAfterDisconnect)
	{
		ClearMultiplayerHands();
		ClearLooseMultiplayerCards();
		bInitialCardDealPresentationPlayed = false;
		TFunction<void()> StartRestartedFirstDuel = [this]()
		{
			ASDPlayerState* FirstPlayer = nullptr;
			for (ASDPlayerState* Player : MultiplayerPlayers)
			{
				if (Player && Player->Lives > 0 && (!FirstPlayer || Player->bHostPlayer))
				{
					FirstPlayer = Player;
					if (Player->bHostPlayer)
					{
						break;
					}
				}
			}
			if (FirstPlayer)
			{
				StartMultiplayerDuel(FirstPlayer, FindNextAliveMultiplayerPlayer(FirstPlayer));
			}
		};
		StartInitialCardDealPresentation(MultiplayerPlayers.Num(), true, MoveTemp(StartRestartedFirstDuel));
		return;
	}

	if (bMultiplayerRoundResolving
		|| CurrentPhase == EShowDownPhase::Reveal
		|| CurrentPhase == EShowDownPhase::Roulette
		|| CurrentPhase == EShowDownPhase::RoundEnd)
	{
		return;
	}

	if (CountActiveMultiplayerPlayers() <= 1)
	{
		EndMultiplayerRound();
		return;
	}

	if (!AreAllAliveMultiplayerPlayersReadyToReveal())
	{
		DealMultiplayerHands();
		ASDPlayerState* NextGiver = MultiplayerRoundLeader && MultiplayerRoundLeader->Lives > 0
			? MultiplayerRoundLeader.Get()
			: FindNextAliveMultiplayerPlayer(nullptr);
		if (NextGiver)
		{
			StartMultiplayerDuel(NextGiver, FindNextAliveMultiplayerPlayer(NextGiver));
		}
		return;
	}

	if (ShowDownGameState && ShowDownGameState->CurrentPhase == EShowDownPhase::Betting)
	{
		if (CountActiveMultiplayerPlayers() <= 1 || AreAllActiveMultiplayerPlayersDoneBetting(BettingSystem ? BettingSystem->GetCurrentBet() : 0))
		{
			FinishMultiplayerRoundByReveal();
			return;
		}

		if (!MultiplayerCurrentBetter)
		{
			const int32 LeavingTurnOrder = GetMultiplayerTurnOrderIndex(LeavingSlot);
			ASDPlayerState* FirstActivePlayer = nullptr;
			ASDPlayerState* NextActivePlayer = nullptr;
			int32 FirstActiveOrder = MAX_int32;
			int32 NextActiveOrder = MAX_int32;
			for (ASDPlayerState* Candidate : MultiplayerPlayers)
			{
				if (!IsValid(Candidate)
					|| Candidate->Lives <= 0
					|| MultiplayerFoldedPlayers.Contains(Candidate))
				{
					continue;
				}

				const int32 CandidateOrder = GetMultiplayerTurnOrderIndex(Candidate->ShowDownSlot);
				if (CandidateOrder < FirstActiveOrder)
				{
					FirstActiveOrder = CandidateOrder;
					FirstActivePlayer = Candidate;
				}
				if (CandidateOrder > LeavingTurnOrder && CandidateOrder < NextActiveOrder)
				{
					NextActiveOrder = CandidateOrder;
					NextActivePlayer = Candidate;
				}
			}
			MultiplayerCurrentBetter = NextActivePlayer ? NextActivePlayer : FirstActivePlayer;
		}

		ShowDownGameState->SetNameTagRoundStatus(
			BettingSystem ? BettingSystem->GetCurrentBet() : 0,
			EShowDownSide::Player,
			MultiplayerCurrentBetter ? MultiplayerCurrentBetter->ShowDownSlot : EShowDownPlayerSlot::None);
		ClearBetBulletTransientState();
		RefreshBetBulletPresentation();
		return;
	}

	StartMultiplayerBetting();
}

void AShowDownGameModeBase::StartMultiplayerCardSelection()
{
	bBettingPhase = false;
	ClearBetActionPanel();
	SetMultiplayerAliveHandsSelectable(true);

	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->SetPhase(EShowDownPhase::SelectCard);
		for (ASDPlayerState* Player : MultiplayerPlayers)
		{
			if (Player && Player->Lives > 0 && Player->ShowDownSlot != EShowDownPlayerSlot::None)
			{
				ShowDownGameState->SetNameTagPlayerLoadedBulletCount(Player->ShowDownSlot, 0);
			}
		}
		ShowDownGameState->SetNameTagRoundStatus(
			0,
			EShowDownSide::Player,
			EShowDownPlayerSlot::None);
	}
	MultiplayerLiveRoundCount = 0;
	MultiplayerRemainingChamberCount = 6;
	RefreshBetBulletPresentation();

	NotifyMultiplayerStatus(TEXT("모든 플레이어가 동시에 카드를 선택합니다. 전원이 고르면 베팅을 시작합니다."));
}

void AShowDownGameModeBase::HandleMultiplayerSelectedCard(ASDPlayerState* SubmittingPlayer, ACard* SelectedCard)
{
	const AShowDownGameStateBase* CurrentGameState = GetShowDownGameState();
	if (bInitialCardDealPresentationInProgress
		|| !CurrentGameState
		|| CurrentGameState->CurrentPhase != EShowDownPhase::SelectCard
		|| !SubmittingPlayer
		|| !SelectedCard
		|| !SelectedCard->IsCardSelectable()
		|| !CardSystem)
	{
		return;
	}

	if (SubmittingPlayer->Lives <= 0 || MultiplayerFoldedPlayers.Contains(SubmittingPlayer))
	{
		return;
	}

	if (!SubmittingPlayer->HandCards.Contains(SelectedCard))
	{
		return;
	}

	ASDPlayerState* Receiver = FindNextAliveMultiplayerPlayer(SubmittingPlayer);
	if (!Receiver || Receiver == SubmittingPlayer || Receiver->ForeheadCard)
	{
		return;
	}

	CardSystem->RemoveCardFromHand(SubmittingPlayer->HandCards, SelectedCard);
	ReflowMultiplayerHand(SubmittingPlayer);
	for (ACard* Card : SubmittingPlayer->HandCards)
	{
		if (Card)
		{
			Card->SetSelectable(false);
		}
	}
	SubmittingPlayer->ForceNetUpdate();
	BroadcastMultiplayerCardSelectedAction(SubmittingPlayer);
	RefreshBetBulletPresentation();

	Receiver->ForeheadCard = SelectedCard;
	Receiver->ForceNetUpdate();
	SelectedCard->SetHandOwnerSlot(EShowDownPlayerSlot::None);
	SelectedCard->SetHiddenFromSlot(Receiver->ShowDownSlot);
	SelectedCard->SetFaceUp(true);
	SelectedCard->SetSelectable(false);
	if (USceneComponent* HeadSlot = GetHeadSlotForPlayerState(Receiver))
	{
		CardSystem->MoveCardToSlotWithRotationOffset(
			SelectedCard,
			HeadSlot,
			true,
			GetForeheadCardRotationOffsetForPlayerState(Receiver, HeadSlot));
	}

	if (AreAllAliveMultiplayerPlayersReadyToReveal())
	{
		StartMultiplayerBetting();
		return;
	}

	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->SetNameTagRoundStatus(0, EShowDownSide::Player, EShowDownPlayerSlot::None);
	}
}

void AShowDownGameModeBase::StartMultiplayerBetting()
{
	bBettingPhase = true;
	const int32 MinimumBet = StageRules.Num() > 0 ? StageRules[0].MinimumBet : 1;
	MultiplayerLiveRoundCount = FMath::Clamp(MinimumBet, 1, 6);
	MultiplayerRemainingChamberCount = 6;
	MultiplayerSharedChamberIndex = 0;
	MultiplayerSharedChambers.Reset();
	if (BettingSystem)
	{
		BettingSystem->ResetBetting(MinimumBet);
	}

	for (ASDPlayerState* Player : MultiplayerPlayers)
	{
		if (Player && Player->Lives > 0 && !MultiplayerFoldedPlayers.Contains(Player))
		{
			Player->CurrentBet = MinimumBet;
		}
	}

	BettingRaisesLeft = 6;
	MultiplayerCurrentBetter = MultiplayerRoundLeader;
	MultiplayerLastCheckedPlayer = nullptr;
	MultiplayerPlayersActed.Reset();
	SetMultiplayerSelectableHand(nullptr);
	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->SetPhase(EShowDownPhase::Betting);
		for (ASDPlayerState* Player : MultiplayerPlayers)
		{
			if (Player && Player->Lives > 0 && !MultiplayerFoldedPlayers.Contains(Player))
			{
				ShowDownGameState->SetNameTagPlayerLoadedBulletCount(Player->ShowDownSlot, Player->CurrentBet);
			}
		}
		ShowDownGameState->SetNameTagRoundStatus(
			MinimumBet,
			EShowDownSide::Player,
			MultiplayerCurrentBetter ? MultiplayerCurrentBetter->ShowDownSlot : EShowDownPlayerSlot::None);
	}
	ClearBetBulletTransientState();
	ClearBetBulletActionHistory();
	RefreshBetBulletPresentation();
	RefreshCentralGunStatus();

	NotifyMultiplayerStatus(FString::Printf(
		TEXT("베팅 시작. %s부터 행동합니다. Q=체크/콜, E=레이즈, R=폴드"),
		MultiplayerCurrentBetter ? *MultiplayerCurrentBetter->GetPlayerName() : TEXT("플레이어")));
}

void AShowDownGameModeBase::HandleMultiplayerBetAction(
	ASDPlayerState* SubmittingPlayer,
	EShowDownBetAction Action,
	int32 TargetBet)
{
	if (!SubmittingPlayer
		|| !bBettingPhase
		|| SubmittingPlayer != MultiplayerCurrentBetter
		|| bMultiplayerRoundResolving
		|| SubmittingPlayer->Lives <= 0
		|| MultiplayerFoldedPlayers.Contains(SubmittingPlayer))
	{
		return;
	}

	const int32 CurrentBet = BettingSystem
		? BettingSystem->GetCurrentBet()
		: SubmittingPlayer->CurrentBet;
	auto FindNextActivePlayer = [this](ASDPlayerState* AfterPlayer)
	{
		ASDPlayerState* Candidate = FindNextAliveMultiplayerPlayer(AfterPlayer);
		for (int32 Attempt = 0; Candidate && Attempt < MultiplayerPlayers.Num(); ++Attempt)
		{
			if (!MultiplayerFoldedPlayers.Contains(Candidate))
			{
				return Candidate;
			}
			Candidate = FindNextAliveMultiplayerPlayer(Candidate);
		}
		return static_cast<ASDPlayerState*>(nullptr);
	};

	switch (Action)
	{
	case EShowDownBetAction::Check:
	case EShowDownBetAction::Call:
	{
		const EShowDownBetAction CommittedAction = SubmittingPlayer->CurrentBet < CurrentBet
			? EShowDownBetAction::Call
			: EShowDownBetAction::Check;
		if (SubmittingPlayer->CurrentBet < CurrentBet)
		{
			SubmittingPlayer->CurrentBet = CurrentBet;
			NotifyMultiplayerStatus(FString::Printf(TEXT("%s: %d 콜"), *SubmittingPlayer->GetPlayerName(), CurrentBet));
			BroadcastSystemChatMessage(FString::Printf(
				TEXT("%s님이 %d발로 콜했습니다."),
				*SubmittingPlayer->GetPlayerName(),
				CurrentBet));
		}
		else
		{
			NotifyMultiplayerStatus(FString::Printf(TEXT("%s 체크."), *SubmittingPlayer->GetPlayerName()));
			BroadcastSystemChatMessage(FString::Printf(
				TEXT("%s님이 %d발 장전 상태로 체크했습니다."),
				*SubmittingPlayer->GetPlayerName(),
				CurrentBet));
		}

		BroadcastMultiplayerBetActionCommitted(SubmittingPlayer, CommittedAction, CurrentBet);
		MultiplayerPlayersActed.Add(SubmittingPlayer);
		if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
			{
				ShowDownGameState->SetNameTagPlayerLoadedBulletCount(SubmittingPlayer->ShowDownSlot, SubmittingPlayer->CurrentBet);
			}
			RecordMultiplayerBetBulletAction(SubmittingPlayer, CommittedAction);
			if (CountActiveMultiplayerPlayers() <= 1 || AreAllActiveMultiplayerPlayersDoneBetting(CurrentBet))
			{
				RefreshBetBulletPresentation();
				FinishMultiplayerRoundByReveal();
				return;
			}

		MultiplayerCurrentBetter = FindNextActivePlayer(SubmittingPlayer);
		if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
		{
			ShowDownGameState->SetNameTagRoundStatus(
				CurrentBet,
				EShowDownSide::Player,
				MultiplayerCurrentBetter ? MultiplayerCurrentBetter->ShowDownSlot : EShowDownPlayerSlot::None);
			RefreshBetBulletPresentation();
		}
		NotifyMultiplayerStatus(FString::Printf(TEXT("다음 차례: %s"),
			MultiplayerCurrentBetter ? *MultiplayerCurrentBetter->GetPlayerName() : TEXT("없음")));
		return;
	}

	case EShowDownBetAction::Raise:
	{
		const int32 RequestedBet = TargetBet < 0
			? CurrentBet + FMath::Abs(TargetBet)
			: (TargetBet > 0 ? TargetBet : CurrentBet + 1);
		const int32 NewBet = FMath::Clamp(RequestedBet, 1, 6);
		if (NewBet <= CurrentBet || BettingRaisesLeft <= 0 || !BettingSystem || !BettingSystem->RaiseTo(EShowDownSide::Player, NewBet))
		{
			NotifyMultiplayerStatus(FString::Printf(
				TEXT("레이즈할 수 없습니다. 현재 %d / 요청 %d / 남은 레이즈 %d"),
				CurrentBet,
				NewBet,
				BettingRaisesLeft));
			return;
		}

		SubmittingPlayer->CurrentBet = NewBet;
		MultiplayerLiveRoundCount = NewBet;
		MultiplayerRemainingChamberCount = 6;
		MultiplayerSharedChambers.Reset();
		BettingRaisesLeft = FMath::Max(0, BettingRaisesLeft - 1);
		MultiplayerPlayersActed.Reset();
		MultiplayerPlayersActed.Add(SubmittingPlayer);
		BroadcastSystemChatMessage(FString::Printf(
			TEXT("%s님이 %d발 장전했습니다."),
			*SubmittingPlayer->GetPlayerName(),
			NewBet));
		BroadcastMultiplayerBetActionCommitted(SubmittingPlayer, EShowDownBetAction::Raise, NewBet);
		if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
			{
				ShowDownGameState->SetNameTagPlayerLoadedBulletCount(SubmittingPlayer->ShowDownSlot, SubmittingPlayer->CurrentBet);
			}
			RecordMultiplayerBetBulletAction(SubmittingPlayer, EShowDownBetAction::Raise);
			RefreshCentralGunStatus();
			MultiplayerCurrentBetter = FindNextActivePlayer(SubmittingPlayer);
			if (CountActiveMultiplayerPlayers() <= 1 || !MultiplayerCurrentBetter)
			{
				RefreshBetBulletPresentation();
				FinishMultiplayerRoundByReveal();
				return;
			}
		if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
		{
			ShowDownGameState->SetNameTagRoundStatus(
				NewBet,
				EShowDownSide::Player,
					MultiplayerCurrentBetter ? MultiplayerCurrentBetter->ShowDownSlot : EShowDownPlayerSlot::None);
			}
			RefreshBetBulletPresentation();
			NotifyMultiplayerStatus(FString::Printf(
			TEXT("%s: %d 레이즈. 다음 차례: %s"),
			*SubmittingPlayer->GetPlayerName(),
			NewBet,
			MultiplayerCurrentBetter ? *MultiplayerCurrentBetter->GetPlayerName() : TEXT("없음")));
		return;
	}

	case EShowDownBetAction::Fold:
	{
		// Fold reveal and roulette are one resolving transaction. Close every
		// betting/card-input gate before scheduling presentation callbacks so a
		// disconnect cannot restart betting while the shot is still in flight.
		bBettingPhase = false;
		bMultiplayerRoundResolving = true;
		MultiplayerCurrentBetter = nullptr;
		SetMultiplayerSelectableHand(nullptr);
		ClearBetActionPanel();
		NotifyMultiplayerStatus(FString::Printf(TEXT("%s 폴드."), *SubmittingPlayer->GetPlayerName()));
		BroadcastSystemChatMessage(FString::Printf(
			TEXT("%s님이 폴드했습니다."),
			*SubmittingPlayer->GetPlayerName()));
		MultiplayerFoldedPlayers.Add(SubmittingPlayer);
		BroadcastMultiplayerBetActionCommitted(SubmittingPlayer, EShowDownBetAction::Fold, SubmittingPlayer->CurrentBet);
		RecordMultiplayerBetBulletAction(SubmittingPlayer, EShowDownBetAction::Fold);
		const int32 FoldLoadCount = ResolveMultiplayerFoldLoadCount(SubmittingPlayer);
		PrepareMultiplayerFoldCylinder(FoldLoadCount);

		if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
		{
			ShowDownGameState->SetNameTagPlayerLoadedBulletCount(SubmittingPlayer->ShowDownSlot, FoldLoadCount);
			ShowDownGameState->SetNameTagRoundStatus(FoldLoadCount, EShowDownSide::Player, EShowDownPlayerSlot::None);
			ShowDownGameState->SetPhase(EShowDownPhase::Reveal);
		}
		RefreshBetBulletPresentation();
		RefreshCentralGunStatus();
		TArray<ACard*> FoldRevealCards;
		if (SubmittingPlayer->ForeheadCard)
		{
			FoldRevealCards.Add(SubmittingPlayer->ForeheadCard);
		}
		const float FoldRevealDelay = PlayCardRevealPresentation(FoldRevealCards);
		const float RouletteDelay = ApplyMultiplayerRoulette(
			SubmittingPlayer,
			FoldLoadCount,
			FoldRevealDelay,
			false);
		const TWeakObjectPtr<ASDPlayerState> WeakSubmittingPlayer(SubmittingPlayer);
		const auto ContinueAfterRoulette = [this, WeakSubmittingPlayer, CurrentBet, FindNextActivePlayer]()
		{
			ASDPlayerState* ResolvedSubmittingPlayer = WeakSubmittingPlayer.Get();
			if (!ResolvedSubmittingPlayer || !MultiplayerPlayers.Contains(ResolvedSubmittingPlayer))
			{
				// The fold target can disconnect while its reveal/roulette timers are
				// pending. Advance the survivors instead of leaving resolving latched.
				EndMultiplayerRound();
				return;
			}

			// The folded player uses a private, freshly spun cylinder. Restore the
			// table bet before either continuing betting or creating the shared
			// showdown cylinder for the remaining players.
			MultiplayerLiveRoundCount = FMath::Clamp(CurrentBet, 0, 6);
			MultiplayerRemainingChamberCount = 6;
			MultiplayerSharedChamberIndex = 0;
			MultiplayerSharedChambers.Reset();

			MultiplayerNextFirstPlayer = ResolvedSubmittingPlayer;
			MultiplayerPlayersActed.Add(ResolvedSubmittingPlayer);
			MultiplayerCurrentBetter = FindNextActivePlayer(ResolvedSubmittingPlayer);
			if (CountActiveMultiplayerPlayers() <= 1 || !MultiplayerCurrentBetter || AreAllActiveMultiplayerPlayersDoneBetting(CurrentBet))
			{
				FinishMultiplayerRoundByReveal();
				return;
			}

			bMultiplayerRoundResolving = false;
			bBettingPhase = true;
			if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
			{
				ShowDownGameState->SetPhase(EShowDownPhase::Betting);
				ShowDownGameState->SetNameTagRoundStatus(
					CurrentBet,
					EShowDownSide::Player,
					MultiplayerCurrentBetter->ShowDownSlot);
			}
			ClearBetBulletTransientState();
			RefreshBetBulletPresentation();
			RefreshCentralGunStatus();
		};

		if (RouletteDelay > KINDA_SMALL_NUMBER)
		{
			FTimerDelegate ContinueDelegate;
			ContinueDelegate.BindWeakLambda(this, ContinueAfterRoulette);
			FTimerHandle ContinueTimerHandle;
			GetWorldTimerManager().SetTimer(ContinueTimerHandle, ContinueDelegate, RouletteDelay + 0.05f, false);
			MultiplayerRoundTimerHandles.Add(ContinueTimerHandle);
		}
		else
		{
			ContinueAfterRoulette();
		}
		return;
	}

	default:
		return;
	}
}

void AShowDownGameModeBase::FinishMultiplayerRoundByReveal()
{
	bBettingPhase = false;
	bMultiplayerRoundResolving = true;
	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->SetNameTagRoundStatus(
			BettingSystem ? BettingSystem->GetCurrentBet() : 0,
			EShowDownSide::Player,
			EShowDownPlayerSlot::None);
	}
	ClearBetActionPanel();
	int32 HighestRank = 0;
	int32 LowestRank = TNumericLimits<int32>::Max();
	TArray<ASDPlayerState*> Winners;
	TArray<ASDPlayerState*> RevealedPlayers;

	for (ASDPlayerState* Player : MultiplayerPlayers)
	{
		if (!Player || Player->Lives <= 0 || MultiplayerFoldedPlayers.Contains(Player) || !Player->ForeheadCard)
		{
			continue;
		}

		RevealedPlayers.Add(Player);
		const int32 Rank = Player->ForeheadCard->Rank;
		if (Rank > HighestRank)
		{
			HighestRank = Rank;
			Winners.Reset();
			Winners.Add(Player);
		}
		else if (Rank == HighestRank)
		{
			Winners.Add(Player);
		}

		if (Rank < LowestRank)
		{
			LowestRank = Rank;
		}
	}

	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->SetPhase(EShowDownPhase::Reveal);
		ShowDownGameState->OnCardsRevealed.Broadcast(HighestRank, LowestRank == TNumericLimits<int32>::Max() ? 0 : LowestRank);
	}
	RefreshBetBulletPresentation();
	const float RevealPresentationSeconds = PlayMultiplayerCardRevealPresentation(RevealedPlayers);
	if (RevealPresentationSeconds > KINDA_SMALL_NUMBER)
	{
		TArray<TWeakObjectPtr<ASDPlayerState>> WeakRevealedPlayers;
		TArray<TWeakObjectPtr<ASDPlayerState>> WeakWinners;
		for (ASDPlayerState* RevealedPlayer : RevealedPlayers)
		{
			WeakRevealedPlayers.Add(RevealedPlayer);
		}
		for (ASDPlayerState* Winner : Winners)
		{
			WeakWinners.Add(Winner);
		}

		FTimerDelegate ContinueDelegate;
		ContinueDelegate.BindWeakLambda(this, [this, WeakRevealedPlayers, WeakWinners]()
		{
			TArray<ASDPlayerState*> ValidRevealedPlayers;
			TArray<ASDPlayerState*> ValidWinners;
			for (const TWeakObjectPtr<ASDPlayerState>& WeakPlayer : WeakRevealedPlayers)
			{
				if (ASDPlayerState* Player = WeakPlayer.Get(); Player && MultiplayerPlayers.Contains(Player))
				{
					ValidRevealedPlayers.Add(Player);
				}
			}
			for (const TWeakObjectPtr<ASDPlayerState>& WeakWinner : WeakWinners)
			{
				if (ASDPlayerState* Winner = WeakWinner.Get(); Winner && MultiplayerPlayers.Contains(Winner))
				{
					ValidWinners.Add(Winner);
				}
			}
			ContinueMultiplayerRoundAfterReveal(MoveTemp(ValidRevealedPlayers), MoveTemp(ValidWinners));
		});

		GetWorldTimerManager().SetTimer(
			MultiplayerRevealContinuationTimerHandle,
			ContinueDelegate,
			RevealPresentationSeconds,
			false);
		return;
	}

	ContinueMultiplayerRoundAfterReveal(RevealedPlayers, Winners);
}

void AShowDownGameModeBase::ContinueMultiplayerRoundAfterReveal(
	TArray<ASDPlayerState*> RevealedPlayers,
	TArray<ASDPlayerState*> Winners)
{
	RevealedPlayers.RemoveAll([this](const ASDPlayerState* Player)
	{
		return !IsValid(Player)
			|| !MultiplayerPlayers.Contains(Player)
			|| Player->Lives <= 0
			|| !Player->ForeheadCard;
	});

	// A player can leave while the reveal animation is running. Recalculate from
	// the surviving reveal set so losing the former high card does not skip the
	// remaining players' comparison and roulette entirely.
	Winners.Reset();
	int32 CurrentHighestRank = TNumericLimits<int32>::Min();
	for (ASDPlayerState* Player : RevealedPlayers)
	{
		const int32 Rank = Player->ForeheadCard->Rank;
		if (Rank > CurrentHighestRank)
		{
			CurrentHighestRank = Rank;
			Winners.Reset();
			Winners.Add(Player);
		}
		else if (Rank == CurrentHighestRank)
		{
			Winners.Add(Player);
		}
	}

	if (Winners.Num() <= 0)
	{
		NotifyMultiplayerStatus(TEXT("공개할 카드가 없습니다. 라운드를 종료합니다."));
	}
	else
	{
		FString WinnerNames;
		for (ASDPlayerState* Winner : Winners)
		{
			if (!WinnerNames.IsEmpty())
			{
				WinnerNames += TEXT(", ");
			}
			WinnerNames += Winner->GetPlayerName();
		}

		const bool bOnlyOnePlayerRemainsAfterFold = RevealedPlayers.Num() == 1 && MultiplayerFoldedPlayers.Num() > 0;
		const bool bAllRevealedPlayersTied = RevealedPlayers.Num() > 1 && Winners.Num() == RevealedPlayers.Num();
		TArray<ASDPlayerState*> RouletteTargets;
		if (bOnlyOnePlayerRemainsAfterFold)
		{
			NotifyMultiplayerStatus(FString::Printf(
				TEXT("공개 결과: %s 승리. 폴드한 플레이어만 패배 처리됩니다."),
				*WinnerNames));
		}
		else if (bAllRevealedPlayersTied)
		{
			// A full tie is safe for everyone.
			NotifyMultiplayerStatus(TEXT("공개 결과: 전원 동점. 격발 없이 라운드를 종료합니다."));
		}
		else
		{
			NotifyMultiplayerStatus(FString::Printf(TEXT("공개 결과: %s 승리."), *WinnerNames));
			TArray<ASDPlayerState*> OrderedPlayers = RevealedPlayers;
			OrderedPlayers.Sort(SortByMultiplayerTurnOrder);
			TArray<ASDPlayerState*> OrderedWinners = Winners;
			OrderedWinners.Sort(SortByMultiplayerTurnOrder);
			ASDPlayerState* RepresentativeWinner = OrderedWinners.Num() > 0 ? OrderedWinners[0] : nullptr;
			const int32 WinnerIndex = OrderedPlayers.IndexOfByKey(RepresentativeWinner);
			for (int32 Offset = 1; Offset <= OrderedPlayers.Num(); ++Offset)
			{
				ASDPlayerState* Player = OrderedPlayers[(FMath::Max(0, WinnerIndex) + Offset) % OrderedPlayers.Num()];
				if (Player && !Winners.Contains(Player))
				{
					RouletteTargets.Add(Player);
				}
			}
		}

		ASDPlayerState* NextFirstCandidate = nullptr;
		int32 NextFirstRank = TNumericLimits<int32>::Min();
		float MaxRouletteDelay = 0.0f;
		float RouletteSequenceStartDelay = 0.0f;
		if (RouletteTargets.Num() > 0)
		{
			InitializeMultiplayerSharedChambers();
		}
		for (ASDPlayerState* Player : RouletteTargets)
		{
			if (!Player || !Player->ForeheadCard)
			{
				continue;
			}

			const int32 Rank = Player->ForeheadCard->Rank;
			if (!NextFirstCandidate || Rank > NextFirstRank)
			{
				NextFirstCandidate = Player;
				NextFirstRank = Rank;
			}

			const float ShotFinishDelay = ApplyMultiplayerRoulette(
				Player,
				MultiplayerLiveRoundCount,
				RouletteSequenceStartDelay,
				true);
			MaxRouletteDelay = FMath::Max(MaxRouletteDelay, ShotFinishDelay);
			if (ShotFinishDelay > RouletteSequenceStartDelay)
			{
				RouletteSequenceStartDelay = ShotFinishDelay + 0.05f;
			}
		}

		if (NextFirstCandidate)
		{
			MultiplayerNextFirstPlayer = NextFirstCandidate;
		}

		if (MaxRouletteDelay > KINDA_SMALL_NUMBER)
		{
			FTimerDelegate EndRoundDelegate;
			EndRoundDelegate.BindWeakLambda(this, [this]()
			{
				EndMultiplayerRound();
			});
			FTimerHandle EndRoundTimerHandle;
			GetWorldTimerManager().SetTimer(EndRoundTimerHandle, EndRoundDelegate, MaxRouletteDelay + 0.05f, false);
			MultiplayerRoundTimerHandles.Add(EndRoundTimerHandle);
			return;
		}
	}

	EndMultiplayerRound();
}

void AShowDownGameModeBase::FinishMultiplayerRoundByFold(ASDPlayerState* FoldedPlayer)
{
	if (!FoldedPlayer)
	{
		return;
	}

	bBettingPhase = false;
	bMultiplayerRoundResolving = true;
	MultiplayerNextFirstPlayer = FoldedPlayer;
	ClearBetActionPanel();
	TArray<ASDPlayerState*> RevealedPlayers;
	int32 HighestRank = 0;
	int32 LowestRank = TNumericLimits<int32>::Max();
	if (MultiplayerDuelA && MultiplayerDuelA->ForeheadCard)
	{
		RevealedPlayers.Add(MultiplayerDuelA);
		const int32 Rank = MultiplayerDuelA->ForeheadCard->Rank;
		HighestRank = FMath::Max(HighestRank, Rank);
		LowestRank = FMath::Min(LowestRank, Rank);
	}
	if (MultiplayerDuelB && MultiplayerDuelB->ForeheadCard)
	{
		RevealedPlayers.Add(MultiplayerDuelB);
		const int32 Rank = MultiplayerDuelB->ForeheadCard->Rank;
		HighestRank = FMath::Max(HighestRank, Rank);
		LowestRank = FMath::Min(LowestRank, Rank);
	}

	const int32 LoadCount = ResolveMultiplayerFoldLoadCount(FoldedPlayer);
	PrepareMultiplayerFoldCylinder(LoadCount);
	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->SetNameTagPlayerLoadedBulletCount(FoldedPlayer->ShowDownSlot, LoadCount);
		ShowDownGameState->SetNameTagRoundStatus(LoadCount, EShowDownSide::Player, EShowDownPlayerSlot::None);
		ShowDownGameState->SetPhase(EShowDownPhase::Reveal);
		ShowDownGameState->OnCardsRevealed.Broadcast(
			HighestRank,
			LowestRank == TNumericLimits<int32>::Max() ? 0 : LowestRank);
	}

	const float RevealPresentationSeconds = PlayMultiplayerCardRevealPresentation(RevealedPlayers);
	if (RevealPresentationSeconds > KINDA_SMALL_NUMBER)
	{
		TWeakObjectPtr<ASDPlayerState> WeakFoldedPlayer(FoldedPlayer);
		FTimerDelegate ContinueDelegate;
		ContinueDelegate.BindWeakLambda(this, [this, WeakFoldedPlayer, LoadCount]()
		{
			ContinueMultiplayerRoundAfterFoldReveal(WeakFoldedPlayer.Get(), LoadCount);
		});

		GetWorldTimerManager().SetTimer(
			MultiplayerRevealContinuationTimerHandle,
			ContinueDelegate,
			RevealPresentationSeconds,
			false);
		return;
	}

	ContinueMultiplayerRoundAfterFoldReveal(FoldedPlayer, LoadCount);
}

int32 AShowDownGameModeBase::ResolveMultiplayerFoldLoadCount(const ASDPlayerState* FoldedPlayer) const
{
	if (!FoldedPlayer)
	{
		return 0;
	}

	const int32 FoldedRank = FoldedPlayer->ForeheadCard ? FoldedPlayer->ForeheadCard->Rank : 0;
	const FShowDownStageRule* StageRule = GetCurrentStageRule();
	const bool bSevenFoldLoadsSix = StageRule ? StageRule->bSevenFoldLoadsSix : true;
	if (RoundResolver)
	{
		return RoundResolver->GetFoldLoadCount(
			FoldedRank,
			FoldedPlayer->CurrentBet,
			bSevenFoldLoadsSix);
	}

	return bSevenFoldLoadsSix && FoldedRank == 7
		? 6
		: FMath::Clamp(FoldedPlayer->CurrentBet, 0, 6);
}

void AShowDownGameModeBase::PrepareMultiplayerFoldCylinder(int32 LoadCount)
{
	MultiplayerLiveRoundCount = FMath::Clamp(LoadCount, 0, 6);
	MultiplayerRemainingChamberCount = 6;
	MultiplayerSharedChamberIndex = 0;
	MultiplayerSharedChambers.Reset();
}

void AShowDownGameModeBase::ContinueMultiplayerRoundAfterFoldReveal(ASDPlayerState* FoldedPlayer, int32 LoadCount)
{
	if (!IsValid(FoldedPlayer) || !MultiplayerPlayers.Contains(FoldedPlayer))
	{
		EndMultiplayerRound();
		return;
	}

	const float RouletteDelay = ApplyMultiplayerRoulette(FoldedPlayer, LoadCount);
	if (RouletteDelay > KINDA_SMALL_NUMBER)
	{
		FTimerDelegate EndRoundDelegate;
		EndRoundDelegate.BindWeakLambda(this, [this]()
		{
			EndMultiplayerRound();
		});
		FTimerHandle EndRoundTimerHandle;
		GetWorldTimerManager().SetTimer(EndRoundTimerHandle, EndRoundDelegate, RouletteDelay + 0.05f, false);
		MultiplayerRoundTimerHandles.Add(EndRoundTimerHandle);
		return;
	}

	EndMultiplayerRound();
}

void AShowDownGameModeBase::InitializeMultiplayerSharedChambers()
{
	MultiplayerRemainingChamberCount = 6;
	MultiplayerSharedChamberIndex = 0;
	MultiplayerSharedChambers.Init(false, 6);

	TArray<int32> ChamberIndices = { 0, 1, 2, 3, 4, 5 };
	for (int32 Index = ChamberIndices.Num() - 1; Index > 0; --Index)
	{
		ChamberIndices.Swap(Index, FMath::RandRange(0, Index));
	}
	for (int32 LiveIndex = 0; LiveIndex < FMath::Clamp(MultiplayerLiveRoundCount, 0, 6); ++LiveIndex)
	{
		MultiplayerSharedChambers[ChamberIndices[LiveIndex]] = true;
	}
	RefreshCentralGunStatus();
}

bool AShowDownGameModeBase::ResolveNextMultiplayerSharedChamber()
{
	if (MultiplayerSharedChambers.Num() != 6 || MultiplayerSharedChamberIndex >= 6)
	{
		InitializeMultiplayerSharedChambers();
	}

	const bool bHit = MultiplayerSharedChambers.IsValidIndex(MultiplayerSharedChamberIndex)
		&& MultiplayerSharedChambers[MultiplayerSharedChamberIndex];
	++MultiplayerSharedChamberIndex;
	MultiplayerRemainingChamberCount = FMath::Max(0, 6 - MultiplayerSharedChamberIndex);
	if (bHit)
	{
		MultiplayerLiveRoundCount = FMath::Max(0, MultiplayerLiveRoundCount - 1);
	}
	return bHit;
}

void AShowDownGameModeBase::RefreshCentralGunStatus()
{
	if (ASDSelfShotGunActor* GunActor = FindSelfShotGunActor())
	{
		const AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState();
		const int32 DisplayLiveRounds = bMultiplayerMatchStarted
			? MultiplayerLiveRoundCount
			: FMath::Clamp(FMath::Max(PlayerState.CurrentBet, CollectorState.CurrentBet), 0, 6);
		const int32 DisplayRemainingChambers = bMultiplayerMatchStarted
			? MultiplayerRemainingChamberCount
			: 6;
		GunActor->SetTableStatus(
			DisplayLiveRounds,
			DisplayRemainingChambers,
			ShowDownGameState ? ShowDownGameState->CurrentPhase : EShowDownPhase::None,
			ShowDownGameState ? ShowDownGameState->NameTagTurnSlot : EShowDownPlayerSlot::None);
	}
}

float AShowDownGameModeBase::ApplyMultiplayerRoulette(
	ASDPlayerState* TargetPlayer,
	int32 BulletCount,
	float StartDelay,
	bool bUseSharedChambers)
{
	if (!TargetPlayer || !RouletteSystem)
	{
		return 0.0f;
	}

	const int32 ClampedBulletCount = FMath::Clamp(BulletCount, 0, 6);
	const float SafeStartDelay = FMath::Max(0.0f, StartDelay);
	if (bUseSharedChambers && SafeStartDelay > KINDA_SMALL_NUMBER)
	{
		// Resolve a shared chamber only when this target's presentation actually
		// begins. A queued player may disconnect before then; pre-consuming here
		// would silently skip a chamber and change every later player's outcome.
		const float ResultDelay = FMath::Max(0.0f, ResolveMultiplayerRouletteResultDelay());
		const float MaxPresentationDelay = FMath::Max(
			ResolveMultiplayerRoulettePresentationDelay(false),
			ResolveMultiplayerRoulettePresentationDelay(true));
		float MaxCharacterRecoveryDuration = 0.0f;
		if (const AShowDownCharacter* TargetCharacter = FindActiveCharacterForPlayerSlot(
			GetWorld(),
			TargetPlayer->ShowDownSlot))
		{
			MaxCharacterRecoveryDuration = TargetCharacter->GetHitRecoveryPresentationDuration();
		}
		const float ReservedFinishDelay = CalculateRoulettePresentationFinishDelay(
			ResultDelay,
			MaxPresentationDelay,
			MaxCharacterRecoveryDuration);
		const TWeakObjectPtr<ASDPlayerState> WeakQueuedTarget(TargetPlayer);
		FTimerDelegate StartDelegate;
		StartDelegate.BindWeakLambda(this, [this, WeakQueuedTarget]()
		{
			ASDPlayerState* QueuedTarget = WeakQueuedTarget.Get();
			if (!QueuedTarget
				|| !MultiplayerPlayers.Contains(QueuedTarget)
				|| QueuedTarget->Lives <= 0)
			{
				return;
			}

			ApplyMultiplayerRoulette(QueuedTarget, MultiplayerLiveRoundCount, 0.0f, true);
		});
		FTimerHandle StartTimerHandle;
		GetWorldTimerManager().SetTimer(StartTimerHandle, StartDelegate, SafeStartDelay, false);
		MultiplayerRoundTimerHandles.Add(StartTimerHandle);
		return SafeStartDelay + ReservedFinishDelay;
	}

	// Shared showdown shots consume the existing cylinder. A fold shot instead
	// loads and spins a fresh cylinder using its resolved fold load (which can be
	// six even when the table bet is one for a folded seven).
	if (!bUseSharedChambers)
	{
		MultiplayerLiveRoundCount = ClampedBulletCount;
	}
	const int32 LiveRoundsBeforeShot = MultiplayerLiveRoundCount;
	const int32 ChambersBeforeShot = bUseSharedChambers ? MultiplayerRemainingChamberCount : 6;
	const bool bHit = bUseSharedChambers
		? ResolveNextMultiplayerSharedChamber()
		: (ClampedBulletCount > 0 && RouletteSystem->RollRoulette(ClampedBulletCount));
	if (!bUseSharedChambers)
	{
		if (bHit)
		{
			MultiplayerLiveRoundCount = FMath::Max(0, MultiplayerLiveRoundCount - 1);
		}
		// A fold shot is always followed by a fresh spin.
		MultiplayerRemainingChamberCount = 6;
		MultiplayerSharedChamberIndex = 0;
		MultiplayerSharedChambers.Reset();
	}
	const int32 LiveRoundsAfterShot = MultiplayerLiveRoundCount;
	const int32 ChambersAfterShot = MultiplayerRemainingChamberCount;
	const EShowDownPlayerSlot TargetSlot = TargetPlayer->ShowDownSlot;
	const FString TargetName = TargetPlayer->GetPlayerName();
	const float ResultDelay = FMath::Max(0.0f, ResolveMultiplayerRouletteResultDelay());
	float CharacterRecoveryDuration = 0.0f;
	if (bHit)
	{
		if (const AShowDownCharacter* TargetCharacter = FindActiveCharacterForPlayerSlot(GetWorld(), TargetSlot))
		{
			CharacterRecoveryDuration = TargetCharacter->GetHitRecoveryPresentationDuration();
		}
	}
	const float FinishDelay = CalculateRoulettePresentationFinishDelay(
		ResultDelay,
		ResolveMultiplayerRoulettePresentationDelay(bHit),
		CharacterRecoveryDuration);
	const TWeakObjectPtr<ASDPlayerState> WeakTargetPlayer(TargetPlayer);

	auto BroadcastResult = [this, WeakTargetPlayer, TargetSlot, TargetName, ClampedBulletCount, bHit, LiveRoundsAfterShot, ChambersAfterShot]()
	{
		ASDPlayerState* ResolvedTargetPlayer = WeakTargetPlayer.Get();
		if (!ResolvedTargetPlayer || !MultiplayerPlayers.Contains(ResolvedTargetPlayer))
		{
			return;
		}

		const int32 PreviousLives = ResolvedTargetPlayer->Lives;
		bool bEliminated = false;
		if (bHit)
		{
			ResolvedTargetPlayer->Lives = FMath::Max(0, ResolvedTargetPlayer->Lives - 1);
			MultiplayerRoundSpectators.Add(ResolvedTargetPlayer);
			if (PreviousLives > 0 && ResolvedTargetPlayer->Lives <= 0)
			{
				bEliminated = true;
				MultiplayerEliminationOrder.AddUnique(ResolvedTargetPlayer);
			}
		}

		const int32 RemainingLives = ResolvedTargetPlayer->Lives;
		// Publish the actual shot outcome before any explanatory chat. Character
		// hit reactions and heart updates therefore begin from the result event,
		// never from a message that arrives ahead of the gun.
		if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
		{
			ShowDownGameState->BroadcastMultiplayerRouletteResult(
				TargetSlot,
				TargetName,
				ClampedBulletCount,
				bHit,
				RemainingLives);
		}
		ResolvedTargetPlayer->ForceNetUpdate();
		if (bHit)
		{
			BroadcastSystemChatMessage(FString::Printf(
				TEXT("%s님이 %d발 룰렛에 맞았습니다. 남은 목숨: %d"),
				*TargetName,
				ClampedBulletCount,
				RemainingLives));
		}
		else
		{
			BroadcastSystemChatMessage(FString::Printf(
				TEXT("%s님이 %d발 룰렛을 피했습니다."),
				*TargetName,
				ClampedBulletCount));
		}
		if (bEliminated)
		{
			BroadcastSystemChatMessage(FString::Printf(
				TEXT("%s님이 사망했습니다."),
				*TargetName));
		}
		NotifyMultiplayerStatus(FString::Printf(
			TEXT("%s roulette %d/6: %s (lives: %d)"),
			*TargetName,
			ClampedBulletCount,
			bHit ? TEXT("hit") : TEXT("miss"),
			RemainingLives));

		if (ASDSelfShotGunActor* GunActor = FindSelfShotGunActor())
		{
			GunActor->SetTableStatus(
				LiveRoundsAfterShot,
				ChambersAfterShot,
				EShowDownPhase::Roulette,
				TargetSlot);
		}
	};

	auto StartPresentation = [this, WeakTargetPlayer, TargetSlot, TargetName, ClampedBulletCount, bHit, ResultDelay, FinishDelay, BroadcastResult, LiveRoundsBeforeShot, ChambersBeforeShot]()
	{
		if (!WeakTargetPlayer.IsValid())
		{
			return;
		}

		ASDSelfShotGunActor* GunActor = FindSelfShotGunActor();
		const bool bResolveFromGunEvent = GunActor && GunActor->CanInteract_Implementation(nullptr);
		if (bResolveFromGunEvent)
		{
			TFunction<void()> GunResultContinuation = BroadcastResult;
			ArmMultiplayerGunResult(GunActor, FinishDelay, MoveTemp(GunResultContinuation));
		}
		if (ASDPlayerState* ResolvedTargetPlayer = WeakTargetPlayer.Get())
		{
			// Apply the target marker when this shot actually starts. Scheduling a
			// sequence used to leave every earlier shot pointing at the final target.
			MarkMultiplayerBetBulletRouletteTarget(ResolvedTargetPlayer, ClampedBulletCount);
			RefreshBetBulletPresentation();
		}

		if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
		{
			ShowDownGameState->SetPhase(EShowDownPhase::Roulette);
			ShowDownGameState->SetNameTagPlayerLoadedBulletCount(TargetSlot, ClampedBulletCount);
			ShowDownGameState->SetNameTagRoundStatus(
				LiveRoundsBeforeShot,
				EShowDownSide::Player,
				TargetSlot);
			ShowDownGameState->BroadcastMultiplayerRouletteStarted(TargetSlot, TargetName, ClampedBulletCount);
			ShowDownGameState->BroadcastMultiplayerRoulettePresentation(TargetSlot, TargetName, ClampedBulletCount, bHit);
		}
		if (GunActor)
		{
			GunActor->SetTableStatus(
				LiveRoundsBeforeShot,
				ChambersBeforeShot,
				EShowDownPhase::Roulette,
				TargetSlot);
		}

		if (bResolveFromGunEvent)
		{
			return;
		}

		// Missing/busy gun fallback only. Normal gameplay resolves from the gun's
		// real fire or empty-click delegate instead of predicting it with a timer.
		if (ResultDelay <= KINDA_SMALL_NUMBER)
		{
			BroadcastResult();
			return;
		}

		FTimerDelegate ResultDelegate;
		ResultDelegate.BindWeakLambda(this, BroadcastResult);
		FTimerHandle ResultTimerHandle;
		GetWorldTimerManager().SetTimer(ResultTimerHandle, ResultDelegate, ResultDelay, false);
		MultiplayerRoundTimerHandles.Add(ResultTimerHandle);
	};

	if (SafeStartDelay <= KINDA_SMALL_NUMBER)
	{
		StartPresentation();
	}
	else
	{
		FTimerDelegate StartDelegate;
		StartDelegate.BindWeakLambda(this, StartPresentation);
		FTimerHandle StartTimerHandle;
		GetWorldTimerManager().SetTimer(StartTimerHandle, StartDelegate, SafeStartDelay, false);
		MultiplayerRoundTimerHandles.Add(StartTimerHandle);
	}

	return SafeStartDelay + FinishDelay;
}

void AShowDownGameModeBase::EndMultiplayerRound()
{
	ClearMultiplayerRoundTimers();
	ClearCardRevealPresentationTimers();
	bBettingPhase = false;
	bMultiplayerRoundResolving = false;
	MultiplayerCurrentBetter = nullptr;
	MultiplayerLastCheckedPlayer = nullptr;
	MultiplayerPlayersActed.Reset();
	SetMultiplayerSelectableHand(nullptr);
	ClearBetActionPanel();
	ClearBetBulletPresentation();
	ClearMultiplayerForeheadCards();
	MultiplayerFoldedPlayers.Reset();
	MultiplayerRoundSpectators.Reset();
	MultiplayerLiveRoundCount = 0;
	MultiplayerRemainingChamberCount = 6;
	MultiplayerSharedChamberIndex = 0;
	MultiplayerSharedChambers.Reset();
	RefreshMultiplayerCharacterVisibility();

	int32 AliveCount = 0;
	ASDPlayerState* Winner = nullptr;
	for (ASDPlayerState* Player : MultiplayerPlayers)
	{
		if (Player && Player->Lives > 0)
		{
			++AliveCount;
			Winner = Player;
		}
	}

	if (AliveCount <= 1)
	{
		if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
		{
			ShowDownGameState->SetPhase(EShowDownPhase::GameOver);
			ShowDownGameState->SetNameTagRoundStatus(0, EShowDownSide::Player, EShowDownPlayerSlot::None);
			for (ASDPlayerState* Player : MultiplayerPlayers)
			{
				if (Player && Player->ShowDownSlot != EShowDownPlayerSlot::None)
				{
					ShowDownGameState->SetNameTagPlayerLoadedBulletCount(Player->ShowDownSlot, 0);
				}
			}
			ShowDownGameState->OnGameOver.Broadcast(EShowDownSide::Player);
		}
		MultiplayerRoundLeader = nullptr;
		MultiplayerDuelA = nullptr;
		MultiplayerDuelB = nullptr;
		MultiplayerNextFirstPlayer = nullptr;
		NotifyMultiplayerStatus(FString::Printf(
			TEXT("게임 종료. 승자: %s"),
			Winner ? *Winner->GetPlayerName() : TEXT("없음")));
		ShowMultiplayerFinalRanking(Winner);
		return;
	}

	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->CurrentRound++;
		ShowDownGameState->SetPhase(EShowDownPhase::RoundEnd);
		ShowDownGameState->SetNameTagRoundStatus(0, EShowDownSide::Player, EShowDownPlayerSlot::None);
		for (ASDPlayerState* Player : MultiplayerPlayers)
		{
			if (Player && Player->ShowDownSlot != EShowDownPlayerSlot::None)
			{
				ShowDownGameState->SetNameTagPlayerLoadedBulletCount(Player->ShowDownSlot, 0);
			}
		}
	}

	bool bNeedRedeal = false;
	for (ASDPlayerState* Player : MultiplayerPlayers)
	{
		if (Player && Player->Lives > 0 && Player->HandCards.Num() <= 0)
		{
			bNeedRedeal = true;
			break;
		}
	}

	auto StartNextDuel = [this]()
	{
		ASDPlayerState* NextFirst = MultiplayerNextFirstPlayer;
		MultiplayerNextFirstPlayer = nullptr;
		if (!NextFirst || NextFirst->Lives <= 0)
		{
			// The losing player can be eliminated by roulette; then the next living
			// participant takes the lead so the match can continue.
			NextFirst = FindNextAliveMultiplayerPlayer(MultiplayerDuelA);
		}

		ASDPlayerState* NextSecond = FindNextAliveMultiplayerPlayer(NextFirst);
		StartMultiplayerDuel(NextFirst, NextSecond);
	};

	if (bNeedRedeal)
	{
		StartHandRedealPresentation(true, MoveTemp(StartNextDuel));
		return;
	}

	StartNextDuel();
}

void AShowDownGameModeBase::ShowMultiplayerFinalRanking(ASDPlayerState* Winner)
{
	MultiplayerRestartVotes.Reset();

	TArray<ASDPlayerState*> FinalRanking;
	if (Winner)
	{
		FinalRanking.Add(Winner);
	}

	for (int32 Index = MultiplayerEliminationOrder.Num() - 1; Index >= 0; --Index)
	{
		ASDPlayerState* EliminatedPlayer = MultiplayerEliminationOrder[Index];
		if (EliminatedPlayer)
		{
			FinalRanking.AddUnique(EliminatedPlayer);
		}
	}

	for (ASDPlayerState* Player : MultiplayerPlayers)
	{
		if (Player)
		{
			FinalRanking.AddUnique(Player);
		}
	}

	TArray<FString> RankNames;
	for (ASDPlayerState* Player : FinalRanking)
	{
		RankNames.Add(Player ? Player->GetPlayerName() : TEXT("Unknown"));
	}

	if (UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			if (AShowDownPlayerController* PlayerController = Cast<AShowDownPlayerController>(Iterator->Get()))
			{
				PlayerController->ClientShowMultiplayerRank(RankNames);
			}
		}
	}
}

void AShowDownGameModeBase::RequestMultiplayerRestartFromController(AController* RequestingController)
{
	ASDPlayerState* RequestingPlayer = GetPlayerStateForController(RequestingController);
	if (!RequestingPlayer)
	{
		return;
	}

	AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState();
	if (!ShowDownGameState || ShowDownGameState->CurrentPhase != EShowDownPhase::GameOver)
	{
		if (AShowDownPlayerController* PlayerController = Cast<AShowDownPlayerController>(RequestingController))
		{
			PlayerController->ClientShowStatusMessage(TEXT("게임 종료 화면에서만 재시작할 수 있습니다."));
		}
		return;
	}

	TArray<ASDPlayerState*> ConnectedPlayers = GetConnectedShowDownPlayers();
	if (ConnectedPlayers.Num() <= 0)
	{
		return;
	}

	MultiplayerRestartVotes.Add(RequestingPlayer);
	TSet<TObjectPtr<ASDPlayerState>> ConnectedPlayerSet;
	for (ASDPlayerState* ConnectedPlayer : ConnectedPlayers)
	{
		ConnectedPlayerSet.Add(ConnectedPlayer);
	}

	for (auto VoteIterator = MultiplayerRestartVotes.CreateIterator(); VoteIterator; ++VoteIterator)
	{
		if (!ConnectedPlayerSet.Contains(*VoteIterator))
		{
			VoteIterator.RemoveCurrent();
		}
	}

	const int32 RequiredVotes = ConnectedPlayers.Num();
	const int32 CurrentVotes = MultiplayerRestartVotes.Num();
	const FString VoteMessage = FString::Printf(TEXT("재시작 동의 %d/%d"), CurrentVotes, RequiredVotes);
	NotifyMultiplayerStatus(VoteMessage);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		if (AShowDownPlayerController* PlayerController = Cast<AShowDownPlayerController>(Iterator->Get()))
		{
			PlayerController->ClientShowStatusMessage(VoteMessage);
		}
	}

	if (CurrentVotes < RequiredVotes)
	{
		return;
	}

	NotifyMultiplayerStatus(TEXT("전원 동의. 게임을 재시작합니다."));
	StartMultiplayerMatch(ConnectedPlayers);
}

void AShowDownGameModeBase::SetMultiplayerSelectableHand(ASDPlayerState* Player)
{
	for (ASDPlayerState* CurrentPlayer : MultiplayerPlayers)
	{
		if (!CurrentPlayer)
		{
			continue;
		}

		for (ACard* Card : CurrentPlayer->HandCards)
		{
			if (Card)
			{
				Card->SetSelectable(CurrentPlayer == Player);
			}
		}
	}
}

void AShowDownGameModeBase::SetMultiplayerAliveHandsSelectable(bool bSelectable)
{
	for (ASDPlayerState* CurrentPlayer : MultiplayerPlayers)
	{
		if (!CurrentPlayer)
		{
			continue;
		}

		const bool bCurrentPlayerSelectable =
			bSelectable
			&& CurrentPlayer->Lives > 0
			&& !MultiplayerFoldedPlayers.Contains(CurrentPlayer);

		for (ACard* Card : CurrentPlayer->HandCards)
		{
			if (Card)
			{
				Card->SetSelectable(bCurrentPlayerSelectable);
			}
		}
	}
}

void AShowDownGameModeBase::ReflowMultiplayerHand(ASDPlayerState* Player)
{
	if (!CardSystem || !Player)
	{
		return;
	}

	if (USceneComponent* HandSlot = GetHandSlotForPlayerState(Player))
	{
		const FSDCardHandLayoutSettings HandLayout = ResolveHandLayoutSettingsForPlayerState(Player);
		CardSystem->LayoutHandCards(this, HandSlot, HandLayout, Player->HandCards);
		ApplyCardMotionForPlayerState(Player, Player->HandCards);
	}
}

USceneComponent* AShowDownGameModeBase::GetHandSlotForPlayerState(ASDPlayerState* Player) const
{
	if (!Player || !GetWorld())
	{
		return nullptr;
	}

	const int32 PlayerIndex = MultiplayerPlayers.IndexOfByKey(Player);
	// Authored per-seat anchors share the same world-space table layout as the
	// character head cameras. Prefer them when present; the replicated pawn can
	// remain at its spawn transform while its local camera is attached to the
	// character mesh, which otherwise sends the final hand out of view.
	if (const ASDCardPlacementAnchor* HandAnchor = GetHandAnchorForPlayerSlot(Player->ShowDownSlot))
	{
		if (USceneComponent* HandSlot = HandAnchor->GetSlotComponent())
		{
			return HandSlot;
		}
	}

	// Maps without authored multiplayer anchors still use the pawn-local slot.
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		const APlayerController* PlayerController = Iterator->Get();
		if (!PlayerController || PlayerController->PlayerState != Player)
		{
			continue;
		}

		if (const APlayerPawn* PlayerPawn = Cast<APlayerPawn>(PlayerController->GetPawn()))
		{
			return PlayerPawn->PlayerHandCard ? PlayerPawn->PlayerHandCard : PlayerPawn->GetRootComponent();
		}
	}

	if (PlayerIndex == 0)
	{
		return GetHandSlotForSide(EShowDownSide::Player);
	}
	if (PlayerIndex == 1)
	{
		return GetHandSlotForSide(EShowDownSide::Collector);
	}

	return GetHandSlotForSide(GetMultiplayerLayoutSideForPlayerIndex(PlayerIndex));
}

USceneComponent* AShowDownGameModeBase::GetHeadSlotForPlayerState(ASDPlayerState* Player) const
{
	if (!Player || !GetWorld())
	{
		return nullptr;
	}

	const int32 PlayerIndex = MultiplayerPlayers.IndexOfByKey(Player);
	if (const AShowDownCharacter* Character = FindActiveCharacterForPlayerSlot(GetWorld(), Player->ShowDownSlot))
	{
		if (USceneComponent* HeadSlot = Character->GetForeheadCardAnchor())
		{
			return HeadSlot;
		}
	}

	if (const ASDCardPlacementAnchor* ForeheadAnchor = GetForeheadAnchorForPlayerSlot(Player->ShowDownSlot))
	{
		if (USceneComponent* HeadSlot = ForeheadAnchor->GetSlotComponent())
		{
			return HeadSlot;
		}
	}

	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		const APlayerController* PlayerController = Iterator->Get();
		if (!PlayerController || PlayerController->PlayerState != Player)
		{
			continue;
		}

		if (const APlayerPawn* PlayerPawn = Cast<APlayerPawn>(PlayerController->GetPawn()))
		{
			return PlayerPawn->PlayerHeadCard ? PlayerPawn->PlayerHeadCard : PlayerPawn->GetRootComponent();
		}
	}

	if (PlayerIndex == 0)
	{
		return GetHeadSlotForSide(EShowDownSide::Player);
	}
	if (PlayerIndex == 1)
	{
		return GetHeadSlotForSide(EShowDownSide::Collector);
	}

	return GetHeadSlotForSide(GetMultiplayerLayoutSideForPlayerIndex(PlayerIndex));
}

FRotator AShowDownGameModeBase::GetForeheadCardRotationOffsetForPlayerState(
	const ASDPlayerState* Player,
	const USceneComponent* HeadSlot) const
{
	if (!Player || !HeadSlot || !GetWorld())
	{
		return FRotator::ZeroRotator;
	}

	if (const AShowDownCharacter* Character = FindActiveCharacterForPlayerSlot(GetWorld(), Player->ShowDownSlot))
	{
		if (Character->GetForeheadCardAnchor() == HeadSlot
			&& !Character->ShouldAutoFaceForeheadCardToOpponents())
		{
			return FRotator::ZeroRotator;
		}
	}

	FVector FocusLocation = FVector::ZeroVector;
	int32 FocusCount = 0;
	for (const ASDPlayerState* OtherPlayer : MultiplayerPlayers)
	{
		if (!OtherPlayer || OtherPlayer == Player || OtherPlayer->Lives <= 0)
		{
			continue;
		}

		if (const AShowDownCharacter* Character = FindActiveCharacterForPlayerSlot(GetWorld(), OtherPlayer->ShowDownSlot))
		{
			FocusLocation += Character->GetForeheadCardAnchor()
				? Character->GetForeheadCardAnchor()->GetComponentLocation()
				: Character->GetActorLocation();
			++FocusCount;
			continue;
		}

		if (const ASDCardPlacementAnchor* ForeheadAnchor = GetForeheadAnchorForPlayerSlot(OtherPlayer->ShowDownSlot))
		{
			if (const USceneComponent* Slot = ForeheadAnchor->GetSlotComponent())
			{
				FocusLocation += Slot->GetComponentLocation();
				++FocusCount;
			}
		}
	}

	return FocusCount > 0
		? BuildForeheadCardRotationOffsetFacingLocation(HeadSlot, FocusLocation / static_cast<float>(FocusCount))
		: FRotator::ZeroRotator;
}

FRotator AShowDownGameModeBase::GetForeheadCardRotationOffsetForSide(
	EShowDownSide Side,
	const USceneComponent* HeadSlot) const
{
	if (!HeadSlot)
	{
		return FRotator::ZeroRotator;
	}

	if (const AShowDownCharacter* Character = FindSingleRouletteCharacter(Side))
	{
		if (Character->GetForeheadCardAnchor() == HeadSlot
			&& !Character->ShouldAutoFaceForeheadCardToOpponents())
		{
			return FRotator::ZeroRotator;
		}
	}

	const EShowDownSide FocusSide = Side == EShowDownSide::Collector
		? EShowDownSide::Player
		: EShowDownSide::Collector;
	if (const AShowDownCharacter* FocusCharacter = FindSingleRouletteCharacter(FocusSide))
	{
		const FVector FocusLocation = FocusCharacter->GetForeheadCardAnchor()
			? FocusCharacter->GetForeheadCardAnchor()->GetComponentLocation()
			: FocusCharacter->GetActorLocation();
		return BuildForeheadCardRotationOffsetFacingLocation(HeadSlot, FocusLocation);
	}

	if (const USceneComponent* FocusSlot = GetHeadSlotForSide(FocusSide))
	{
		return BuildForeheadCardRotationOffsetFacingLocation(HeadSlot, FocusSlot->GetComponentLocation());
	}

	return FRotator::ZeroRotator;
}

void AShowDownGameModeBase::NotifyMultiplayerStatus(const FString& Message) const
{
	UE_LOG(LogTemp, Log, TEXT("Multiplayer: %s"), *Message);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Green, Message);
	}

	if (!GetWorld())
	{
		return;
	}

	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		if (AShowDownPlayerController* PlayerController = Cast<AShowDownPlayerController>(Iterator->Get()))
		{
			PlayerController->ClientShowStatusMessage(Message);
		}
	}
}

void AShowDownGameModeBase::StartStage(int32 StageIndex)
{
	if (!StageRules.IsValidIndex(StageIndex))
	{
		if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
		{
			ShowDownGameState->SetPhase(EShowDownPhase::GameOver);
			ShowDownGameState->OnGameOver.Broadcast(EShowDownSide::Player);
		}
		ShowEventDebugMessage(TEXT("게임 종료 - 플레이어 승리"));
		UE_LOG(LogTemp, Log, TEXT("All stages cleared. Player wins the game."));
		return;
	}

	CurrentStageIndex = StageIndex;
	const FShowDownStageRule& StageRule = StageRules[CurrentStageIndex];

	bBettingPhase = false;
	GetWorldTimerManager().ClearTimer(RevealDelayHandle);
	ClearCardRevealPresentationTimers();
	ClearBetBulletPresentation();

	ClearForeheadCards();
	ClearHandCards();

	PlayerState.Lives = StageRule.StartingLives;
	CollectorState.Lives = StageRule.StartingLives;
	if (AShowDownCharacter* PlayerCharacter = FindSingleRouletteCharacter(EShowDownSide::Player))
	{
		PlayerCharacter->SetCharacterLives(PlayerState.Lives);
	}
	if (AShowDownCharacter* CollectorCharacter = FindSingleRouletteCharacter(EShowDownSide::Collector))
	{
		CollectorCharacter->SetCharacterLives(CollectorState.Lives);
	}
	PlayerState.CurrentBet = StageRule.MinimumBet;
	CollectorState.CurrentBet = StageRule.MinimumBet;
	BettingRaisesLeft = 6;
	bHasLastRaiser = false;
	CurrentRoundFirstSide = EShowDownSide::Player;
	NextRoundFirstSide = EShowDownSide::Player;
	RecentRoundHistory.Reset();
	CurrentRoundActionHistory.Reset();
	DiscardedCardsSummary.Reset();
	RecentDialogueHistory.Reset();
	ResetCurrentRoundMemory();

	if (CollectorAISystem)
	{
		CollectorAISystem->Settings.BluffRate = StageRule.CollectorBluffRate;
		CollectorAISystem->Settings.Aggression = StageRule.CollectorAggression;
	}
	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->CurrentStage = CurrentStageIndex + 1;
		ShowDownGameState->CurrentRound = 1;
		ShowDownGameState->OnStageChanged.Broadcast(CurrentStageIndex + 1);
	}

	ShowEventDebugMessage(FString::Printf(TEXT("스테이지 %d 시작"), CurrentStageIndex + 1));
	if (bUseInitialCardDealPresentation && !bInitialCardDealPresentationPlayed)
	{
		StartInitialCardDealPresentation(2, false, [this]()
		{
			BeginCardSelectionRound();
		});
	}
	else
	{
		DealInitialHand();
		BeginCardSelectionRound();
	}

	UE_LOG(LogTemp, Log, TEXT("Stage %d started. Lives: %d, MinBet: %d, Collector Bluff: %.2f, Aggression: %.2f"),
		CurrentStageIndex + 1,
		StageRule.StartingLives,
		StageRule.MinimumBet,
		StageRule.CollectorBluffRate,
		StageRule.CollectorAggression);
}

void AShowDownGameModeBase::AdvanceStage()
{
	StartStage(CurrentStageIndex + 1);
}

const FShowDownStageRule* AShowDownGameModeBase::GetCurrentStageRule() const
{
	return StageRules.IsValidIndex(CurrentStageIndex) ? &StageRules[CurrentStageIndex] : nullptr;
}

AShowDownGameStateBase* AShowDownGameModeBase::GetShowDownGameState() const
{
	return GetGameState<AShowDownGameStateBase>();
}

APlayerPawn* AShowDownGameModeBase::GetPrimaryPlayerPawn() const
{
	return Cast<APlayerPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
}

void AShowDownGameModeBase::RefreshCardPlacementAnchorCache() const
{
	CachedCardPlacementAnchors.Reset();
	bCardPlacementAnchorCacheInitialized = true;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<AActor*> AnchorActors;
	UGameplayStatics::GetAllActorsOfClass(World, ASDCardPlacementAnchor::StaticClass(), AnchorActors);

	for (AActor* AnchorActor : AnchorActors)
	{
		ASDCardPlacementAnchor* Anchor = Cast<ASDCardPlacementAnchor>(AnchorActor);
		if (Anchor && !CachedCardPlacementAnchors.Contains(Anchor->PlacementRole))
		{
			CachedCardPlacementAnchors.Add(Anchor->PlacementRole, Anchor);
		}
	}
}

void AShowDownGameModeBase::RefreshPlayerSeatCache() const
{
	CachedPlayerSeats.Reset();
	CachedFirstPlayerSeat.Reset();
	bPlayerSeatCacheInitialized = true;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<AActor*> SeatActors;
	UGameplayStatics::GetAllActorsOfClass(World, ASDPlayerSeat::StaticClass(), SeatActors);

	for (AActor* SeatActor : SeatActors)
	{
		ASDPlayerSeat* Seat = Cast<ASDPlayerSeat>(SeatActor);
		if (!Seat)
		{
			continue;
		}

		if (!CachedFirstPlayerSeat.IsValid())
		{
			CachedFirstPlayerSeat = Seat;
		}

		if (!CachedPlayerSeats.Contains(Seat->SeatSide))
		{
			CachedPlayerSeats.Add(Seat->SeatSide, Seat);
		}
	}
}

ASDCardPlacementAnchor* AShowDownGameModeBase::GetCardPlacementAnchorByRole(ESDCardPlacementRole TargetRole) const
{
	if (!bCardPlacementAnchorCacheInitialized)
	{
		RefreshCardPlacementAnchorCache();
	}

	if (const TWeakObjectPtr<ASDCardPlacementAnchor>* CachedAnchor = CachedCardPlacementAnchors.Find(TargetRole))
	{
		if (CachedAnchor->IsValid())
		{
			return CachedAnchor->Get();
		}
	}

	// Anchors can appear after the first lookup (for example after a streamed
	// level becomes visible). A cache miss must therefore be refreshed too, not
	// just an entry whose weak pointer expired.
	RefreshCardPlacementAnchorCache();
	if (const TWeakObjectPtr<ASDCardPlacementAnchor>* RefreshedAnchor = CachedCardPlacementAnchors.Find(TargetRole))
	{
		return RefreshedAnchor->Get();
	}

	return nullptr;
}

ASDCardPlacementAnchor* AShowDownGameModeBase::GetCardPlacementAnchor(EShowDownSide Side, bool bForeheadSlot) const
{
	const ESDCardPlacementRole TargetRole = Side == EShowDownSide::Collector
		? (bForeheadSlot ? ESDCardPlacementRole::OpponentForehead : ESDCardPlacementRole::OpponentHand)
		: (bForeheadSlot ? ESDCardPlacementRole::PlayerForehead : ESDCardPlacementRole::PlayerHand);

	return GetCardPlacementAnchorByRole(TargetRole);
}

ASDCardPlacementAnchor* AShowDownGameModeBase::GetHandAnchorForSide(EShowDownSide Side) const
{
	return GetCardPlacementAnchor(Side, false);
}

ASDCardPlacementAnchor* AShowDownGameModeBase::GetForeheadAnchorForSide(EShowDownSide Side) const
{
	return GetCardPlacementAnchor(Side, true);
}

ASDCardPlacementAnchor* AShowDownGameModeBase::GetHandAnchorForPlayerSlot(EShowDownPlayerSlot Slot) const
{
	ESDCardPlacementRole TargetRole = ESDCardPlacementRole::PlayerHand;
	return TryGetMultiplayerPlacementRole(Slot, false, TargetRole)
		? GetCardPlacementAnchorByRole(TargetRole)
		: nullptr;
}

ASDCardPlacementAnchor* AShowDownGameModeBase::GetForeheadAnchorForPlayerSlot(EShowDownPlayerSlot Slot) const
{
	ESDCardPlacementRole TargetRole = ESDCardPlacementRole::PlayerForehead;
	return TryGetMultiplayerPlacementRole(Slot, true, TargetRole)
		? GetCardPlacementAnchorByRole(TargetRole)
		: nullptr;
}

FSDCardHandLayoutSettings AShowDownGameModeBase::ResolveHandLayoutSettingsForPlayerState(ASDPlayerState* Player) const
{
	FSDCardHandLayoutSettings Settings = GetDefaultHandLayoutSettings();
	if (!Player)
	{
		return Settings;
	}

	if (const ASDCardPlacementAnchor* HandAnchor = GetHandAnchorForPlayerSlot(Player->ShowDownSlot))
	{
		Settings.CardSpacing = HandAnchor->CardSpacing;
		Settings.ForwardOffset = HandAnchor->ForwardOffset;
		Settings.HeightOffset = HandAnchor->HeightOffset;
		Settings.LeanAngle = HandAnchor->LeanAngle;
		Settings.LayerStep = HandAnchor->LayerStep;

		const bool bSideSeat = Player->ShowDownSlot == EShowDownPlayerSlot::Player3
			|| Player->ShowDownSlot == EShowDownPlayerSlot::Player4;
		const bool bHasLegacySideSeatLayout =
			FMath::IsNearlyEqual(Settings.CardSpacing, 70.0f)
			&& FMath::IsNearlyEqual(Settings.ForwardOffset, 180.0f)
			&& FMath::IsNearlyEqual(Settings.HeightOffset, 65.0f)
			&& FMath::IsNearlyZero(Settings.LeanAngle)
			&& FMath::IsNearlyEqual(Settings.LayerStep, 0.5f);
		if (bSideSeat && bHasLegacySideSeatLayout)
		{
			// Existing maps serialized the generic hand defaults into the Player 3/4
			// Blueprint instances. Normalize only that exact legacy preset so future
			// deliberately authored side-seat layouts remain untouched.
			Settings.CardSpacing = 9.0f;
			Settings.ForwardOffset = 0.0f;
			Settings.HeightOffset = 0.0f;
			Settings.LeanAngle = 0.0f;
			Settings.LayerStep = 0.0f;
		}
		return Settings;
	}

	return ResolveHandLayoutSettings(EShowDownSide::Player);
}

void AShowDownGameModeBase::ApplyCardMotionForPlayerState(ASDPlayerState* Player, const TArray<ACard*>& Cards) const
{
	if (!Player)
	{
		return;
	}

	if (const ASDCardPlacementAnchor* HandAnchor = GetHandAnchorForPlayerSlot(Player->ShowDownSlot))
	{
		for (ACard* Card : Cards)
		{
			if (!Card)
			{
				continue;
			}

			Card->SelectedOffset = HandAnchor->SelectedOffset;
			Card->HoverOffset = HandAnchor->HoverOffset;
			Card->MoveSpeed = HandAnchor->MoveSpeed;
		}
		return;
	}

	ApplyCardMotionForSide(EShowDownSide::Player, Cards);
}

ASDPlayerSeat* AShowDownGameModeBase::GetSeatForSide(EShowDownSide Side) const
{
	if (!bPlayerSeatCacheInitialized)
	{
		RefreshPlayerSeatCache();
	}

	if (const TWeakObjectPtr<ASDPlayerSeat>* CachedSeat = CachedPlayerSeats.Find(Side))
	{
		if (CachedSeat->IsValid())
		{
			return CachedSeat->Get();
		}
	}

	// Seats may be provided by a streamed level, so recover from both an
	// expired weak pointer and a role that was absent during the first scan.
	RefreshPlayerSeatCache();
	if (const TWeakObjectPtr<ASDPlayerSeat>* RefreshedSeat = CachedPlayerSeats.Find(Side))
	{
		return RefreshedSeat->Get();
	}

	// Old maps may have a single SDPlayerSeat without an explicit side set yet.
	return Side == EShowDownSide::Player ? CachedFirstPlayerSeat.Get() : nullptr;
}

ASDPlayerSeat* AShowDownGameModeBase::GetPrimaryPlayerSeat() const
{
	return GetSeatForSide(EShowDownSide::Player);
}

USceneComponent* AShowDownGameModeBase::GetHandSlotForSide(EShowDownSide Side) const
{
	if (const ASDCardPlacementAnchor* Anchor = GetHandAnchorForSide(Side))
	{
		if (USceneComponent* HandSlot = Anchor->GetSlotComponent())
		{
			return HandSlot;
		}
	}

	if (const ASDPlayerSeat* Seat = GetSeatForSide(Side))
	{
		if (USceneComponent* HandSlot = Seat->GetHandSlot())
		{
			return HandSlot;
		}
	}

	if (Side == EShowDownSide::Player)
	{
		if (const APlayerPawn* PlayerPawn = GetPrimaryPlayerPawn())
		{
			return PlayerPawn->PlayerHandCard;
		}
	}

	if (Side == EShowDownSide::Collector && Collector)
	{
		return Collector->c_HandCard;
	}

	return nullptr;
}

USceneComponent* AShowDownGameModeBase::GetHeadSlotForSide(EShowDownSide Side) const
{
	if (const AShowDownCharacter* Character = FindSingleRouletteCharacter(Side))
	{
		if (USceneComponent* HeadSlot = Character->GetForeheadCardAnchor())
		{
			return HeadSlot;
		}
	}

	if (const ASDCardPlacementAnchor* Anchor = GetForeheadAnchorForSide(Side))
	{
		if (USceneComponent* HeadSlot = Anchor->GetSlotComponent())
		{
			return HeadSlot;
		}
	}

	if (const ASDPlayerSeat* Seat = GetSeatForSide(Side))
	{
		if (USceneComponent* HeadSlot = Seat->GetHeadSlot())
		{
			return HeadSlot;
		}
	}

	if (Side == EShowDownSide::Player)
	{
		if (const APlayerPawn* PlayerPawn = GetPrimaryPlayerPawn())
		{
			return PlayerPawn->PlayerHeadCard;
		}
	}

	if (Side == EShowDownSide::Collector && Collector)
	{
		return Collector->c_HeadCard;
	}

	return nullptr;
}

USceneComponent* AShowDownGameModeBase::GetPlayerHandSlot() const
{
	return GetHandSlotForSide(EShowDownSide::Player);
}

USceneComponent* AShowDownGameModeBase::GetPlayerHeadSlot() const
{
	return GetHeadSlotForSide(EShowDownSide::Player);
}

void AShowDownGameModeBase::ScheduleRevealAutoAdvanceIfNeeded(float MinimumDelaySeconds)
{
	GetWorldTimerManager().ClearTimer(RevealDelayHandle);

	const AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState();
	const bool bPresentationIsHandled = ShowDownGameState && ShowDownGameState->OnPresentationStarted.IsBound();
	const bool bHasBuiltInPresentationDelay = MinimumDelaySeconds > KINDA_SMALL_NUMBER;
	if (bPresentationIsHandled || (!bHasBuiltInPresentationDelay && !bAutoAdvanceRevealWithoutPresentation))
	{
		return;
	}

	const float DelaySeconds = FMath::Max(FMath::Max(0.0f, RevealAutoAdvanceSeconds), FMath::Max(0.0f, MinimumDelaySeconds));
	if (DelaySeconds <= 0.0f)
	{
		EventEnd(EShowDownPhase::Reveal);
		return;
	}

	GetWorldTimerManager().SetTimer(
		RevealDelayHandle,
		FTimerDelegate::CreateUObject(this, &AShowDownGameModeBase::EventEnd, EShowDownPhase::Reveal),
		DelaySeconds,
		false);
}

void AShowDownGameModeBase::ShowEventDebugMessage(const FString& Message) const
{
	if (bShowGameFlowDebugMessages && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.8f, FColor::Cyan, FString::Printf(TEXT("[게임] %s"), *Message));
	}
}
