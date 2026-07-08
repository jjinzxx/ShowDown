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
#include "Kismet/GameplayStatics.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneSequencePlaybackSettings.h"
#include "PlayerPawn.h"
#include "Presentation/SDBetBulletPresentationActor.h"
#include "Presentation/SDSelfShotGunActor.h"
#include "SDPlayerSeat.h"
#include "SDPlayerState.h"
#include "ShowDownCharacter.h"
#include "ShowDownEosSubsystem.h"
#include "ShowDownGameStateBase.h"
#include "ShowDownHubFlowManager.h"
#include "ShowDownPlayerController.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerState.h"

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
		int32 ExpectedPlayerCount = 4;
		if (const UGameInstance* GameInstance = GetGameInstance())
		{
			if (const UShowDownEosSubsystem* EosSubsystem = GameInstance->GetSubsystem<UShowDownEosSubsystem>())
			{
				ExpectedPlayerCount = FMath::Clamp(EosSubsystem->GetExpectedLobbyPlayerCount(), 2, 4);
			}
		}

		if (ASDPlayerState* ShowDownPlayerState = NewPlayer ? NewPlayer->GetPlayerState<ASDPlayerState>() : nullptr)
		{
			if (ShowDownPlayerState->ShowDownSlot == EShowDownPlayerSlot::None)
			{
				ShowDownPlayerState->SetShowDownSlot(FindNextOpenPlayerSlot());
			}

			RefreshNetworkPlayerSlots();
			TArray<ASDPlayerState*> ConnectedPlayers = GetConnectedShowDownPlayers();
			if (ShowDownPlayerState->ShowDownSlot != EShowDownPlayerSlot::None
				&& ConnectedPlayers.Num() <= ExpectedPlayerCount
				&& MultiplayerPlayers.Num() < ExpectedPlayerCount)
			{
				NotifyMultiplayerStatus(FString::Printf(
					TEXT("늦게 도착한 참가자 포함. 멀티플레이어 동기화 중... %d/%d"),
					ConnectedPlayers.Num(),
					ExpectedPlayerCount));
				StartMultiplayerMatch(ConnectedPlayers);
				return;
			}
		}

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
	GetWorldTimerManager().ClearTimer(CardPlacementDelayHandle);
	GetWorldTimerManager().ClearTimer(RevealDelayHandle);
	GetWorldTimerManager().ClearTimer(CollectorActionPresentationTimerHandle);
	ClearCardRevealPresentationTimers();
	ClearBetBulletPresentation();
	if (ActiveSelfShotGunActor)
	{
		ActiveSelfShotGunActor->OnGunPresentationFinished.RemoveDynamic(
			this,
			&AShowDownGameModeBase::HandleSelfShotGunPresentationFinished);
	}

	bBettingPhase = false;
	bHasPendingRoundReveal = false;
	bHasPendingFoldReveal = false;
	bCollectorActionPresentationInProgress = false;
	bSelfShotGunPresentationInProgress = false;
	bPlayerHasActedInBetting = false;
	bCollectorHasActedInBetting = false;
	bCollectorBetDecisionInProgress = false;
	CardPlacementDelayContinuation = TFunction<void()>();
	CollectorActionPresentationContinuation = TFunction<void()>();
	SelfShotGunPresentationContinuation = TFunction<void()>();
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
	if (GetNetMode() != NM_Standalone && bMultiplayerMatchStarted)
	{
		HandleMultiplayerSelectedCard(GetPlayerStateForController(SubmittingController), SelectedCard);
		return;
	}

	if (bCollectorActionPresentationInProgress || bCollectorBetDecisionInProgress)
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

	UE_LOG(LogTemp, Log, TEXT("Single player deck: ranks 1-7 x2, total 14 cards."));
	UE_LOG(LogTemp, Log, TEXT("Player hand count: %d"), PlayerState.HandCards.Num());
	UE_LOG(LogTemp, Log, TEXT("Collector hand count: %d"), CollectorState.HandCards.Num());
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
	TFunction<void()>&& Continuation)
{
	if (GetNetMode() != NM_Standalone)
	{
		bPendingSelfShotRouletteResult = true;
		bPendingSelfShotLiveRound = bLiveRound;
		PendingSelfShotTargetSide = TargetSide;
		BroadcastPendingSelfShotRouletteResult();
		PlayCollectorActionPresentationThen(MoveTemp(Continuation));
		return;
	}

	if (bSelfShotGunPresentationInProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("Self shot gun presentation is already running. Falling back to collector presentation."));
		bPendingSelfShotRouletteResult = true;
		bPendingSelfShotLiveRound = bLiveRound;
		PendingSelfShotTargetSide = TargetSide;
		BroadcastPendingSelfShotRouletteResult();
		PlayCollectorActionPresentationThen(MoveTemp(Continuation));
		return;
	}

	ASDSelfShotGunActor* GunActor = FindSelfShotGunActor();
	if (!GunActor || !GunActor->CanInteract_Implementation(nullptr))
	{
		UE_LOG(LogTemp, Warning, TEXT("Self shot gun actor is missing or busy. Falling back to collector presentation."));
		bPendingSelfShotRouletteResult = true;
		bPendingSelfShotLiveRound = bLiveRound;
		PendingSelfShotTargetSide = TargetSide;
		BroadcastPendingSelfShotRouletteResult();
		PlayCollectorActionPresentationThen(MoveTemp(Continuation));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Playing self shot gun presentation. Target=%s Result=%s"),
		TargetSide == EShowDownSide::Player ? TEXT("Player") : TEXT("Collector"),
		bLiveRound ? TEXT("Live") : TEXT("Empty"));

	bSelfShotGunPresentationInProgress = true;
	ActiveSelfShotGunActor = GunActor;
	SelfShotGunPresentationContinuation = MoveTemp(Continuation);
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
	ACameraActor* EnemyShotCamera = nullptr;
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
			ShotTargetActor = TargetSide == EShowDownSide::Player ? nullptr : TargetCharacter;
			EnemyShotCamera = TargetSide == EShowDownSide::Collector
				? GunActor->GetEnemyShotCinematicCamera()
				: nullptr;
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
		EnemyShotCamera = GunActor->GetEnemyShotCinematicCamera();

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
			if (EnemyShotCamera)
			{
				SourcePullDirection = (ShotSourceLocation - EnemyShotCamera->GetActorLocation()).GetSafeNormal();
			}
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
				EnemyShotCamera);
		}
		else
		{
			GunActor->UseGunWithForcedResultAtTargetFromLocationAimAndCamera(
				bLiveRound,
				ShotTargetActor,
				ShotSourceLocation,
				ShotAimLocation,
				EnemyShotCamera);
		}
	}
	else if (bHasShotSourceLocation)
	{
		GunActor->UseGunWithForcedResultAtTargetFromLocationAndCamera(
			bLiveRound,
			ShotTargetActor,
			ShotSourceLocation,
			EnemyShotCamera);
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
	BroadcastPendingSelfShotRouletteResult();
}

void AShowDownGameModeBase::FinishSelfShotGunPresentation()
{
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

	BroadcastPendingSelfShotRouletteResult();
	bSelfShotGunPresentationInProgress = false;
	ActiveSelfShotGunActor = nullptr;

	TFunction<void()> Continuation = MoveTemp(SelfShotGunPresentationContinuation);
	SelfShotGunPresentationContinuation = TFunction<void()>();
	if (Continuation)
	{
		Continuation();
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
		PlayerCharacter->SetCharacterIdentity(
			EShowDownCharacterRole::Player,
			EShowDownPlayerSlot::Player1,
			TEXT("Player"));
		PlayerCharacter->SetCharacterSceneActive(true);
	}

	if (OpponentCharacter)
	{
		OpponentCharacter->SetCharacterIdentity(
			EShowDownCharacterRole::Opponent,
			EShowDownPlayerSlot::None,
			TEXT("김윤아"));
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
		AssignedCharacter->SetCharacterSceneActive(Player->Lives > 0);
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
			AliveBySlot.Add(Player->ShowDownSlot, Player->Lives > 0);
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
	RefreshBetBulletPresentation(FString::Printf(
		TEXT("BASE BET %d | %s TURN"),
		StageRule->MinimumBet,
		CurrentRoundFirstSide == EShowDownSide::Player ? TEXT("Player") : TEXT("Collector")));
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
	const int32 PreviousPlayerBet = PlayerState.CurrentBet;
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
		RecordSingleBetBulletAction(EShowDownSide::Player, EShowDownBetAction::Call, PreviousPlayerBet, PlayerState.CurrentBet);
		RefreshBetBulletPresentation(TEXT("Player CALL | reveal ready"));
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
	RecordSingleBetBulletAction(EShowDownSide::Player, EShowDownBetAction::Check, PreviousPlayerBet, PlayerState.CurrentBet);
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
		RefreshBetBulletPresentation(TEXT("Player CHECK | reveal ready"));
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
	RefreshBetBulletPresentation(TEXT("Player CHECK | Collector turn"));
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
	const int32 PreviousPlayerBet = PlayerState.CurrentBet;

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
		RecordSingleBetBulletAction(EShowDownSide::Player, EShowDownBetAction::Raise, PreviousPlayerBet, PlayerState.CurrentBet);
		RefreshBetBulletPresentation(TEXT("Player RAISE | Collector response"));
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

	const int32 PreviousPlayerBet = PlayerState.CurrentBet;
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
	RecordSingleBetBulletAction(EShowDownSide::Player, EShowDownBetAction::Fold, PreviousPlayerBet, PlayerState.CurrentBet);
	RefreshBetBulletPresentation(TEXT("Player FOLD | reveal"));
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
	if (GetNetMode() != NM_Standalone && bMultiplayerMatchStarted)
	{
		HandleMultiplayerBetAction(GetPlayerStateForController(SubmittingController), Action, TargetBet);
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
	const int32 PreviousCollectorBet = CollectorState.CurrentBet;
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
			RecordSingleBetBulletAction(EShowDownSide::Collector, EShowDownBetAction::Check, PreviousCollectorBet, CollectorState.CurrentBet);
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
				RefreshBetBulletPresentation(TEXT("Collector CHECK | reveal ready"));
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
				RefreshBetBulletPresentation(TEXT("Collector CHECK | Player turn"));
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
			RecordSingleBetBulletAction(EShowDownSide::Collector, EShowDownBetAction::Call, PreviousCollectorBet, CollectorState.CurrentBet);
			RefreshBetBulletPresentation(TEXT("Collector CALL | reveal ready"));
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
					RecordSingleBetBulletAction(EShowDownSide::Collector, EShowDownBetAction::Raise, PreviousCollectorBet, CollectorState.CurrentBet);
					RefreshBetBulletPresentation(TEXT("Collector RAISE | Player response"));
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
			RecordSingleBetBulletAction(EShowDownSide::Collector, EShowDownBetAction::Fold, PreviousCollectorBet, CollectorState.CurrentBet);
			RefreshBetBulletPresentation(TEXT("Collector FOLD | reveal"));
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

ASDBetBulletPresentationActor* AShowDownGameModeBase::EnsureBetBulletPresentationActor()
{
	if (!bUseBetBulletPresentation || !HasAuthority())
	{
		return nullptr;
	}

	if (IsValid(BetBulletPresentationActor))
	{
		return BetBulletPresentationActor;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UClass* PresentationClass = BetBulletPresentationClass
		? BetBulletPresentationClass.Get()
		: ASDBetBulletPresentationActor::StaticClass();
	if (!PresentationClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	BetBulletPresentationActor = World->SpawnActor<ASDBetBulletPresentationActor>(
		PresentationClass,
		FTransform::Identity,
		SpawnParams);
	return BetBulletPresentationActor;
}

void AShowDownGameModeBase::ClearBetBulletPresentation()
{
	ClearBetBulletTransientState();
	if (ASDBetBulletPresentationActor* PresentationActor = EnsureBetBulletPresentationActor())
	{
		FSDBetBulletPresentationState EmptyState;
		EmptyState.Revision = BetBulletPresentationRevision;
		PresentationActor->SetPresentationState(EmptyState);
	}
}

void AShowDownGameModeBase::RefreshBetBulletPresentation(const FString& StatusText)
{
	if (!bUseBetBulletPresentation)
	{
		return;
	}

	ASDBetBulletPresentationActor* PresentationActor = EnsureBetBulletPresentationActor();
	if (!PresentationActor)
	{
		return;
	}

	FSDBetBulletPresentationState NewState;
	NewState.TableBet = BettingSystem ? BettingSystem->GetCurrentBet() : 0;
	NewState.StatusText = StatusText;
	NewState.Revision = BetBulletPresentationRevision;

	if (bMultiplayerMatchStarted)
	{
		NewState.StatusText = NewState.StatusText.IsEmpty()
			? BuildMultiplayerBetBulletStatusText(MultiplayerCurrentBetter)
			: NewState.StatusText;

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

			FSDBetBulletLaneState LaneState;
			LaneState.Slot = Player->ShowDownSlot;
			LaneState.DisplayName = Player->GetPlayerName().IsEmpty()
				? FString::Printf(TEXT("Player%d"), static_cast<int32>(Player->ShowDownSlot))
				: Player->GetPlayerName();
			LaneState.BulletCount = FMath::Clamp(Player->CurrentBet, 0, 6);
			LaneState.bFolded = MultiplayerFoldedPlayers.Contains(Player);
			LaneState.bCurrentTurn = bBettingPhase
				&& !bMultiplayerRoundResolving
				&& MultiplayerCurrentBetter == Player
				&& !LaneState.bFolded
				&& !bHasBetBulletRouletteTarget;

			if (bHasBetBulletAction
				&& bBetBulletActionIsMultiplayer
				&& BetBulletActionSlot == Player->ShowDownSlot)
			{
				LaneState.ActionText = BuildBetBulletActionText(
					BetBulletAction,
					BetBulletActionPreviousBet,
					BetBulletActionTargetBet);
				LaneState.ActionRevision = BetBulletPresentationRevision;
			}

			if (bHasBetBulletRouletteTarget
				&& bBetBulletRouletteTargetIsMultiplayer
				&& BetBulletRouletteTargetSlot == Player->ShowDownSlot)
			{
				LaneState.bRouletteTarget = true;
				LaneState.RouletteBulletCount = BetBulletRouletteBulletCount;
				LaneState.ActionRevision = BetBulletPresentationRevision;
			}

			const FTransform LaneTransform = BuildBetBulletLaneTransformForPlayer(Player);
			LaneState.WorldLocation = LaneTransform.GetLocation();
			LaneState.WorldRotation = LaneTransform.GetRotation().Rotator();
			NewState.Lanes.Add(LaneState);
		}
	}
	else
	{
		NewState.StatusText = NewState.StatusText.IsEmpty()
			? BuildSingleBetBulletStatusText(CurrentRoundFirstSide)
			: NewState.StatusText;
		NewState.TableBet = FMath::Max(PlayerState.CurrentBet, CollectorState.CurrentBet);

		const EShowDownSide Sides[] = { EShowDownSide::Player, EShowDownSide::Collector };
		for (EShowDownSide Side : Sides)
		{
			const FShowDownParticipantState& ParticipantState = Side == EShowDownSide::Player
				? PlayerState
				: CollectorState;

			FSDBetBulletLaneState LaneState;
			LaneState.Slot = Side == EShowDownSide::Player ? EShowDownPlayerSlot::Player1 : EShowDownPlayerSlot::None;
			LaneState.DisplayName = Side == EShowDownSide::Player ? TEXT("Player") : TEXT("Collector");
			LaneState.BulletCount = FMath::Clamp(ParticipantState.CurrentBet, 0, 6);
			LaneState.bCurrentTurn = bBettingPhase
				&& !bHasBetBulletRouletteTarget
				&& GetShowDownGameState()
				&& GetShowDownGameState()->NameTagTurnSide == Side
				&& GetShowDownGameState()->NameTagTurnSlot != EShowDownPlayerSlot::None;

			if (bHasBetBulletAction
				&& !bBetBulletActionIsMultiplayer
				&& BetBulletActionSide == Side)
			{
				LaneState.ActionText = BuildBetBulletActionText(
					BetBulletAction,
					BetBulletActionPreviousBet,
					BetBulletActionTargetBet);
				LaneState.ActionRevision = BetBulletPresentationRevision;
				LaneState.bFolded = BetBulletAction == EShowDownBetAction::Fold;
			}

			if (bHasBetBulletRouletteTarget
				&& !bBetBulletRouletteTargetIsMultiplayer
				&& BetBulletRouletteTargetSide == Side)
			{
				LaneState.bRouletteTarget = true;
				LaneState.RouletteBulletCount = BetBulletRouletteBulletCount;
				LaneState.ActionRevision = BetBulletPresentationRevision;
			}

			const FTransform LaneTransform = BuildBetBulletLaneTransformForSide(Side);
			LaneState.WorldLocation = LaneTransform.GetLocation();
			LaneState.WorldRotation = LaneTransform.GetRotation().Rotator();
			NewState.Lanes.Add(LaneState);
		}
	}

	PresentationActor->SetPresentationState(NewState);
}

FTransform AShowDownGameModeBase::BuildBetBulletLaneTransformForSide(EShowDownSide Side) const
{
	if (const USceneComponent* HeadSlot = GetHeadSlotForSide(Side))
	{
		return BuildBetBulletLaneTransformFromLocation(
			HeadSlot->GetComponentLocation(),
			Side == EShowDownSide::Player ? 0 : 2);
	}

	if (const USceneComponent* HandSlot = GetHandSlotForSide(Side))
	{
		return BuildBetBulletLaneTransformFromLocation(
			HandSlot->GetComponentLocation(),
			Side == EShowDownSide::Player ? 0 : 2);
	}

	return BuildBetBulletLaneTransformFromLocation(
		ResolveSingleTableCenter(GetWorld()),
		Side == EShowDownSide::Player ? 0 : 2);
}

FTransform AShowDownGameModeBase::BuildBetBulletLaneTransformForPlayer(const ASDPlayerState* Player) const
{
	if (Player)
	{
		if (const USceneComponent* HeadSlot = GetHeadSlotForPlayerState(const_cast<ASDPlayerState*>(Player)))
		{
			return BuildBetBulletLaneTransformFromLocation(
				HeadSlot->GetComponentLocation(),
				GetMultiplayerTurnOrderIndex(Player->ShowDownSlot));
		}

		if (const USceneComponent* HandSlot = GetHandSlotForPlayerState(const_cast<ASDPlayerState*>(Player)))
		{
			return BuildBetBulletLaneTransformFromLocation(
				HandSlot->GetComponentLocation(),
				GetMultiplayerTurnOrderIndex(Player->ShowDownSlot));
		}
	}

	return BuildBetBulletLaneTransformFromLocation(
		ResolveSingleTableCenter(GetWorld()),
		GetMultiplayerTurnOrderIndex(Player ? Player->ShowDownSlot : EShowDownPlayerSlot::None));
}

FTransform AShowDownGameModeBase::BuildBetBulletLaneTransformFromLocation(
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

	FVector LaneLocation = TableCenter + Direction * FMath::Max(0.0f, BetBulletLaneDistanceFromCenter);
	LaneLocation.Z = TableCenter.Z + BetBulletLaneHeightOffset;

	FVector FacingDirection = TableCenter - LaneLocation;
	FacingDirection.Z = 0.0f;
	if (FacingDirection.IsNearlyZero())
	{
		FacingDirection = -Direction;
	}

	FRotator LaneRotation = FacingDirection.Rotation();
	LaneRotation.Pitch = 0.0f;
	LaneRotation.Roll = 0.0f;
	return FTransform(LaneRotation, LaneLocation);
}

FString AShowDownGameModeBase::BuildSingleBetBulletStatusText(EShowDownSide TurnSide) const
{
	const int32 TableBet = FMath::Max(PlayerState.CurrentBet, CollectorState.CurrentBet);
	if (!bBettingPhase)
	{
		return FString::Printf(TEXT("BET DONE | TABLE %d"), TableBet);
	}

	return FString::Printf(
		TEXT("TABLE %d | %s TURN"),
		TableBet,
		TurnSide == EShowDownSide::Player ? TEXT("Player") : TEXT("Collector"));
}

FString AShowDownGameModeBase::BuildMultiplayerBetBulletStatusText(const ASDPlayerState* TurnPlayer) const
{
	const int32 TableBet = BettingSystem ? BettingSystem->GetCurrentBet() : 0;
	if (!bBettingPhase || !TurnPlayer)
	{
		return FString::Printf(TEXT("BET DONE | TABLE %d"), TableBet);
	}

	return FString::Printf(TEXT("TABLE %d | %s TURN"), TableBet, *TurnPlayer->GetPlayerName());
}

void AShowDownGameModeBase::RecordSingleBetBulletAction(
	EShowDownSide Side,
	EShowDownBetAction Action,
	int32 PreviousBet,
	int32 TargetBet)
{
	bHasBetBulletAction = true;
	bBetBulletActionIsMultiplayer = false;
	BetBulletActionSide = Side;
	BetBulletActionSlot = EShowDownPlayerSlot::None;
	BetBulletAction = Action;
	BetBulletActionPreviousBet = PreviousBet;
	BetBulletActionTargetBet = TargetBet;
	bHasBetBulletRouletteTarget = false;
	++BetBulletPresentationRevision;
}

void AShowDownGameModeBase::RecordMultiplayerBetBulletAction(
	ASDPlayerState* Player,
	EShowDownBetAction Action,
	int32 PreviousBet,
	int32 TargetBet)
{
	if (!Player)
	{
		return;
	}

	bHasBetBulletAction = true;
	bBetBulletActionIsMultiplayer = true;
	BetBulletActionSide = EShowDownSide::Player;
	BetBulletActionSlot = Player->ShowDownSlot;
	BetBulletAction = Action;
	BetBulletActionPreviousBet = PreviousBet;
	BetBulletActionTargetBet = TargetBet;
	bHasBetBulletRouletteTarget = false;
	++BetBulletPresentationRevision;
}

void AShowDownGameModeBase::MarkSingleBetBulletRouletteTarget(EShowDownSide TargetSide, int32 BulletCount)
{
	bHasBetBulletRouletteTarget = true;
	bBetBulletRouletteTargetIsMultiplayer = false;
	BetBulletRouletteTargetSide = TargetSide;
	BetBulletRouletteTargetSlot = EShowDownPlayerSlot::None;
	BetBulletRouletteBulletCount = FMath::Clamp(BulletCount, 0, 6);
	++BetBulletPresentationRevision;
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
	++BetBulletPresentationRevision;
}

void AShowDownGameModeBase::ClearBetBulletTransientState()
{
	bHasBetBulletAction = false;
	bBetBulletActionIsMultiplayer = false;
	BetBulletActionSide = EShowDownSide::Player;
	BetBulletActionSlot = EShowDownPlayerSlot::None;
	BetBulletAction = EShowDownBetAction::Check;
	BetBulletActionPreviousBet = 0;
	BetBulletActionTargetBet = 0;
	bHasBetBulletRouletteTarget = false;
	bBetBulletRouletteTargetIsMultiplayer = false;
	BetBulletRouletteTargetSide = EShowDownSide::Player;
	BetBulletRouletteTargetSlot = EShowDownPlayerSlot::None;
	BetBulletRouletteBulletCount = 0;
	++BetBulletPresentationRevision;
}

FString AShowDownGameModeBase::BuildBetBulletActionText(
	EShowDownBetAction Action,
	int32 PreviousBet,
	int32 TargetBet) const
{
	const int32 AddedBullets = FMath::Max(0, TargetBet - PreviousBet);
	switch (Action)
	{
	case EShowDownBetAction::Check:
		return TEXT("CHECK");
	case EShowDownBetAction::Call:
		return AddedBullets > 0
			? FString::Printf(TEXT("CALL +%d  -> %d"), AddedBullets, TargetBet)
			: FString::Printf(TEXT("CALL  %d"), TargetBet);
	case EShowDownBetAction::Raise:
		return AddedBullets > 0
			? FString::Printf(TEXT("RAISE +%d  -> %d"), AddedBullets, TargetBet)
			: FString::Printf(TEXT("RAISE  %d"), TargetBet);
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

	return PlayCardRevealPresentation(RevealCards, ResolveCardRevealFocusLocationForSingle());
}

float AShowDownGameModeBase::PlayMultiplayerCardRevealPresentation(const TArray<ASDPlayerState*>& RevealedPlayers)
{
	TArray<ASDPlayerState*> OrderedPlayers = RevealedPlayers;
	OrderedPlayers.Sort(SortByMultiplayerTurnOrder);

	TArray<ACard*> RevealCards;
	for (ASDPlayerState* Player : OrderedPlayers)
	{
		if (Player && Player->ForeheadCard)
		{
			RevealCards.Add(Player->ForeheadCard);
		}
	}

	return PlayCardRevealPresentation(RevealCards, ResolveCardRevealFocusLocationForMultiplayer(OrderedPlayers));
}

float AShowDownGameModeBase::PlayCardRevealPresentation(const TArray<ACard*>& Cards, const FVector& FocusLocation)
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
		const auto RevealCard = [this, WeakCard, FocusLocation, CardIndex, CardCount]()
		{
			ACard* RevealCardActor = WeakCard.Get();
			if (!IsValid(RevealCardActor))
			{
				return;
			}

			const FTransform RevealTransform = BuildCardRevealPresentationTransform(
				RevealCardActor,
				FocusLocation,
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

FTransform AShowDownGameModeBase::BuildCardRevealPresentationTransform(
	ACard* Card,
	const FVector& FocusLocation,
	int32 CardIndex,
	int32 CardCount) const
{
	if (!Card)
	{
		return FTransform::Identity;
	}

	const FVector StartLocation = Card->GetActorLocation();
	FVector FaceDirection = FocusLocation - StartLocation;
	FaceDirection.Z = 0.0f;
	if (FaceDirection.IsNearlyZero())
	{
		FaceDirection = Card->GetActorForwardVector();
		FaceDirection.Z = 0.0f;
	}
	if (FaceDirection.IsNearlyZero())
	{
		FaceDirection = FVector::ForwardVector;
	}
	FaceDirection.Normalize();

	const FRotator FaceRotation = FaceDirection.Rotation();
	const FVector RightDirection = FRotationMatrix(FaceRotation).GetUnitAxis(EAxis::Y);
	const float CenteredIndex = static_cast<float>(CardIndex) - (static_cast<float>(CardCount) - 1.0f) * 0.5f;
	const FVector RevealLocation =
		StartLocation
		+ FaceDirection * FMath::Max(0.0f, CardRevealForwardDistance)
		+ FVector::UpVector * CardRevealHeightOffset
		+ RightDirection * CardRevealSideSpacing * CenteredIndex;
	const FQuat RevealRotation = (FaceRotation.Quaternion() * CardRevealRotationOffset.Quaternion()).GetNormalized();

	return FTransform(RevealRotation, RevealLocation, Card->GetActorScale3D());
}

FVector AShowDownGameModeBase::ResolveCardRevealFocusLocationForSingle() const
{
	return ResolveSingleTableCenter(GetWorld());
}

FVector AShowDownGameModeBase::ResolveCardRevealFocusLocationForMultiplayer(
	const TArray<ASDPlayerState*>& RevealedPlayers) const
{
	FVector FocusLocation = FVector::ZeroVector;
	int32 FocusCount = 0;
	for (const ASDPlayerState* Player : RevealedPlayers)
	{
		if (!Player)
		{
			continue;
		}

		if (Player->ForeheadCard)
		{
			FocusLocation += Player->ForeheadCard->GetActorLocation();
			++FocusCount;
			continue;
		}

		if (const USceneComponent* HeadSlot = GetHeadSlotForPlayerState(const_cast<ASDPlayerState*>(Player)))
		{
			FocusLocation += HeadSlot->GetComponentLocation();
			++FocusCount;
		}
	}

	return FocusCount > 0
		? FocusLocation / static_cast<float>(FocusCount)
		: ResolveSingleTableCenter(GetWorld());
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
	RefreshBetBulletPresentation(FString::Printf(
		TEXT("%s TARGET | LOAD %d"),
		TargetSide == EShowDownSide::Player ? TEXT("Player") : TEXT("Collector"),
		ClampedBulletCount));
	const bool bHit = RouletteSystem->RollRoulette(ClampedBulletCount);

	UE_LOG(LogTemp, Log, TEXT("%s roulette: %d/6, %.0f%% chance, result: %s"),
		TargetSide == EShowDownSide::Player ? TEXT("Player") : TEXT("Collector"),
		ClampedBulletCount,
		HitChance * 100.0f,
		bHit ? TEXT("Hit") : TEXT("Miss"));

	if (!bHit)
	{
		BroadcastSystemChatMessage(FString::Printf(
			TEXT("%s님이 %d발 룰렛을 피했습니다."),
			TargetSide == EShowDownSide::Player ? TEXT("Player") : TEXT("Collector"),
			ClampedBulletCount));
		ShowEventDebugMessage(FString::Printf(TEXT("룰렛: %s %d발 / 안 맞음"),
			*GetSideDisplayText(TargetSide),
			ClampedBulletCount));
		PlaySelfShotGunPresentationThen(TargetSide, false, MoveTemp(Continuation));
		return;
	}

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
	PlaySelfShotGunPresentationThen(TargetSide, true, MoveTemp(Continuation));

	UE_LOG(LogTemp, Log, TEXT("%s lives: %d"),
		TargetSide == EShowDownSide::Player ? TEXT("Player") : TEXT("Collector"),
		TargetState.Lives);
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
		UE_LOG(LogTemp, Log, TEXT("Hands are empty. Shuffling and dealing new 5-card hands."));
		DealInitialHand();
		BeginCardSelectionRound();
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

	if (HasAuthority()
		&& bMultiplayerMatchStarted
		&& GetNetMode() != NM_Standalone
		&& MultiplayerPlayers.Num() > 0)
	{
		TArray<ASDPlayerState*> CurrentPlayers;
		for (ASDPlayerState* Player : MultiplayerPlayers)
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
	ClearMultiplayerForeheadCards();
	ClearMultiplayerHands();
	ClearLooseMultiplayerCards();

	bMultiplayerMatchStarted = true;
	bMultiplayerRoundResolving = false;
	MultiplayerPlayers.Reset();
	MultiplayerEliminationOrder.Reset();
	MultiplayerRestartVotes.Reset();
	MultiplayerNextFirstPlayer = nullptr;

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
		ShowDownGameState->SetPhase(EShowDownPhase::SelectCard);
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
	DealMultiplayerHands();

	if (MultiplayerPlayers.Num() >= 2)
	{
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

	if (MultiplayerPlayers.Num() <= 1 || CountActiveMultiplayerPlayers() <= 1)
	{
		EndMultiplayerRound();
		return;
	}

	if (bMultiplayerRoundResolving)
	{
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

	AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState();
	if (ShowDownGameState && ShowDownGameState->CurrentPhase == EShowDownPhase::Betting)
	{
		if (CountActiveMultiplayerPlayers() <= 1 || AreAllActiveMultiplayerPlayersDoneBetting(BettingSystem ? BettingSystem->GetCurrentBet() : 0))
		{
			FinishMultiplayerRoundByReveal();
			return;
		}

		if (!MultiplayerCurrentBetter)
		{
			MultiplayerCurrentBetter = FindNextAliveMultiplayerPlayer(nullptr);
			while (MultiplayerCurrentBetter && MultiplayerFoldedPlayers.Contains(MultiplayerCurrentBetter))
			{
				MultiplayerCurrentBetter = FindNextAliveMultiplayerPlayer(MultiplayerCurrentBetter);
			}
		}

		ShowDownGameState->SetNameTagRoundStatus(
			BettingSystem ? BettingSystem->GetCurrentBet() : 0,
			EShowDownSide::Player,
			MultiplayerCurrentBetter ? MultiplayerCurrentBetter->ShowDownSlot : EShowDownPlayerSlot::None);
		return;
	}

	StartMultiplayerBetting();
}

void AShowDownGameModeBase::StartMultiplayerCardSelection()
{
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

	NotifyMultiplayerStatus(TEXT("모든 플레이어가 동시에 카드를 선택합니다. 전원이 고르면 베팅을 시작합니다."));
}

void AShowDownGameModeBase::HandleMultiplayerSelectedCard(ASDPlayerState* SubmittingPlayer, ACard* SelectedCard)
{
	if (!SubmittingPlayer || !SelectedCard || !CardSystem)
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
	const int32 MinimumBet = StageRules.Num() > 0 ? StageRules[0].MinimumBet : 1;
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
	RefreshBetBulletPresentation(FString::Printf(
		TEXT("BASE BET %d | %s TURN"),
		MinimumBet,
		MultiplayerCurrentBetter ? *MultiplayerCurrentBetter->GetPlayerName() : TEXT("Player")));

	NotifyMultiplayerStatus(FString::Printf(
		TEXT("베팅 시작. %s부터 행동합니다. Q=체크/콜, E=레이즈, R=폴드"),
		MultiplayerCurrentBetter ? *MultiplayerCurrentBetter->GetPlayerName() : TEXT("플레이어")));
}

void AShowDownGameModeBase::HandleMultiplayerBetAction(
	ASDPlayerState* SubmittingPlayer,
	EShowDownBetAction Action,
	int32 TargetBet)
{
	if (!SubmittingPlayer || SubmittingPlayer != MultiplayerCurrentBetter || bMultiplayerRoundResolving)
	{
		return;
	}

	const int32 CurrentBet = BettingSystem
		? BettingSystem->GetCurrentBet()
		: SubmittingPlayer->CurrentBet;
	const int32 PreviousSubmittingBet = SubmittingPlayer->CurrentBet;

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
			RecordMultiplayerBetBulletAction(SubmittingPlayer, CommittedAction, PreviousSubmittingBet, SubmittingPlayer->CurrentBet);
			if (CountActiveMultiplayerPlayers() <= 1 || AreAllActiveMultiplayerPlayersDoneBetting(CurrentBet))
			{
				RefreshBetBulletPresentation(FString::Printf(
					TEXT("%s %s | reveal ready"),
					*SubmittingPlayer->GetPlayerName(),
					CommittedAction == EShowDownBetAction::Call ? TEXT("CALL") : TEXT("CHECK")));
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
			RefreshBetBulletPresentation(FString::Printf(
				TEXT("%s action | next: %s"),
				*SubmittingPlayer->GetPlayerName(),
				MultiplayerCurrentBetter ? *MultiplayerCurrentBetter->GetPlayerName() : TEXT("none")));
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
			RecordMultiplayerBetBulletAction(SubmittingPlayer, EShowDownBetAction::Raise, PreviousSubmittingBet, SubmittingPlayer->CurrentBet);
			MultiplayerCurrentBetter = FindNextActivePlayer(SubmittingPlayer);
			if (CountActiveMultiplayerPlayers() <= 1 || !MultiplayerCurrentBetter)
			{
				RefreshBetBulletPresentation(FString::Printf(
					TEXT("%s raise | reveal ready"),
					*SubmittingPlayer->GetPlayerName()));
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
			RefreshBetBulletPresentation(FString::Printf(
				TEXT("%s raise | next: %s"),
				*SubmittingPlayer->GetPlayerName(),
				MultiplayerCurrentBetter ? *MultiplayerCurrentBetter->GetPlayerName() : TEXT("none")));
			NotifyMultiplayerStatus(FString::Printf(
			TEXT("%s: %d 레이즈. 다음 차례: %s"),
			*SubmittingPlayer->GetPlayerName(),
			NewBet,
			MultiplayerCurrentBetter ? *MultiplayerCurrentBetter->GetPlayerName() : TEXT("없음")));
		return;
	}

	case EShowDownBetAction::Fold:
	{
		NotifyMultiplayerStatus(FString::Printf(TEXT("%s 폴드."), *SubmittingPlayer->GetPlayerName()));
		BroadcastSystemChatMessage(FString::Printf(
			TEXT("%s님이 폴드했습니다."),
			*SubmittingPlayer->GetPlayerName()));
			MultiplayerFoldedPlayers.Add(SubmittingPlayer);
			BroadcastMultiplayerBetActionCommitted(SubmittingPlayer, EShowDownBetAction::Fold, SubmittingPlayer->CurrentBet);
			RecordMultiplayerBetBulletAction(SubmittingPlayer, EShowDownBetAction::Fold, PreviousSubmittingBet, SubmittingPlayer->CurrentBet);

			const int32 FoldedRank = SubmittingPlayer->ForeheadCard ? SubmittingPlayer->ForeheadCard->Rank : 0;
		const FShowDownStageRule* StageRule = GetCurrentStageRule();
		const bool bSevenFoldLoadsSix = StageRule ? StageRule->bSevenFoldLoadsSix : true;
		const int32 LoadCount = RoundResolver
			? RoundResolver->GetFoldLoadCount(FoldedRank, SubmittingPlayer->CurrentBet, bSevenFoldLoadsSix)
			: SubmittingPlayer->CurrentBet;
		if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
		{
				ShowDownGameState->SetNameTagPlayerLoadedBulletCount(SubmittingPlayer->ShowDownSlot, SubmittingPlayer->CurrentBet);
				ShowDownGameState->SetNameTagRoundStatus(SubmittingPlayer->CurrentBet, EShowDownSide::Player, EShowDownPlayerSlot::None);
			}
			RefreshBetBulletPresentation(FString::Printf(TEXT("%s fold"), *SubmittingPlayer->GetPlayerName()));
			const float RouletteDelay = ApplyMultiplayerRoulette(SubmittingPlayer, LoadCount);
		const auto ContinueAfterRoulette = [this, SubmittingPlayer, CurrentBet, FindNextActivePlayer]()
		{
			MultiplayerNextFirstPlayer = SubmittingPlayer;
			MultiplayerPlayersActed.Add(SubmittingPlayer);
			MultiplayerCurrentBetter = FindNextActivePlayer(SubmittingPlayer);
			if (CountActiveMultiplayerPlayers() <= 1 || !MultiplayerCurrentBetter || AreAllActiveMultiplayerPlayersDoneBetting(CurrentBet))
			{
				FinishMultiplayerRoundByReveal();
				return;
			}

			if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
			{
					ShowDownGameState->SetNameTagRoundStatus(
						CurrentBet,
						EShowDownSide::Player,
						MultiplayerCurrentBetter->ShowDownSlot);
				}
				ClearBetBulletTransientState();
				RefreshBetBulletPresentation(FString::Printf(
					TEXT("next: %s"),
					MultiplayerCurrentBetter ? *MultiplayerCurrentBetter->GetPlayerName() : TEXT("none")));
			};

		if (RouletteDelay > KINDA_SMALL_NUMBER)
		{
			FTimerDelegate ContinueDelegate;
			ContinueDelegate.BindWeakLambda(this, ContinueAfterRoulette);
			FTimerHandle ContinueTimerHandle;
			GetWorldTimerManager().SetTimer(ContinueTimerHandle, ContinueDelegate, RouletteDelay + 0.05f, false);
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
	bMultiplayerRoundResolving = true;
	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->SetNameTagRoundStatus(
			BettingSystem ? BettingSystem->GetCurrentBet() : 0,
			EShowDownSide::Player,
			EShowDownPlayerSlot::None);
	}
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

	const float RevealPresentationSeconds = PlayMultiplayerCardRevealPresentation(RevealedPlayers);
	if (RevealPresentationSeconds > KINDA_SMALL_NUMBER)
	{
		FTimerDelegate ContinueDelegate;
		ContinueDelegate.BindWeakLambda(this, [this, RevealedPlayers, Winners]()
		{
			ContinueMultiplayerRoundAfterReveal(RevealedPlayers, Winners);
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
	RevealedPlayers.RemoveAll([](const ASDPlayerState* Player)
	{
		return !Player || Player->Lives <= 0 || !Player->ForeheadCard;
	});
	Winners.RemoveAll([&RevealedPlayers](const ASDPlayerState* Player)
	{
		return !Player || !RevealedPlayers.Contains(Player);
	});

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
			RouletteTargets = RevealedPlayers;
			NotifyMultiplayerStatus(TEXT("공개 결과: 전원 동점. 모두 룰렛을 돌립니다."));
		}
		else
		{
			NotifyMultiplayerStatus(FString::Printf(TEXT("공개 결과: %s 승리."), *WinnerNames));
			for (ASDPlayerState* Player : RevealedPlayers)
			{
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
				Player->CurrentBet,
				RouletteSequenceStartDelay);
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

	bMultiplayerRoundResolving = true;
	MultiplayerNextFirstPlayer = FoldedPlayer;
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

	const bool bSevenFoldLoadsSix = StageRules.Num() > 0 ? StageRules[0].bSevenFoldLoadsSix : true;
	const int32 FoldedRank = FoldedPlayer->ForeheadCard ? FoldedPlayer->ForeheadCard->Rank : 1;
	const int32 LoadCount = RoundResolver
		? RoundResolver->GetFoldLoadCount(FoldedRank, FoldedPlayer->CurrentBet, bSevenFoldLoadsSix)
		: FoldedPlayer->CurrentBet;
	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->SetNameTagPlayerLoadedBulletCount(FoldedPlayer->ShowDownSlot, FoldedPlayer->CurrentBet);
		ShowDownGameState->SetNameTagRoundStatus(FoldedPlayer->CurrentBet, EShowDownSide::Player, EShowDownPlayerSlot::None);
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

void AShowDownGameModeBase::ContinueMultiplayerRoundAfterFoldReveal(ASDPlayerState* FoldedPlayer, int32 LoadCount)
{
	if (!FoldedPlayer)
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
		return;
	}

	EndMultiplayerRound();
}

float AShowDownGameModeBase::ApplyMultiplayerRoulette(ASDPlayerState* TargetPlayer, int32 BulletCount, float StartDelay)
{
	if (!TargetPlayer || !RouletteSystem)
	{
		return 0.0f;
	}

	const int32 ClampedBulletCount = FMath::Clamp(BulletCount, 1, 6);
	if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
	{
		ShowDownGameState->SetNameTagRoundStatus(ClampedBulletCount, EShowDownSide::Player, EShowDownPlayerSlot::None);
	}
	MarkMultiplayerBetBulletRouletteTarget(TargetPlayer, ClampedBulletCount);
	RefreshBetBulletPresentation(FString::Printf(
		TEXT("%s target | load %d"),
		*TargetPlayer->GetPlayerName(),
		ClampedBulletCount));
	const bool bHit = RouletteSystem->RollRoulette(ClampedBulletCount);
	const EShowDownPlayerSlot TargetSlot = TargetPlayer->ShowDownSlot;
	const FString TargetName = TargetPlayer->GetPlayerName();
	const float SafeStartDelay = FMath::Max(0.0f, StartDelay);
	const float ResultDelay = FMath::Max(0.0f, ResolveMultiplayerRouletteResultDelay());
	const float FinishDelay = FMath::Max(ResultDelay, ResolveMultiplayerRoulettePresentationDelay(bHit));
	const TWeakObjectPtr<ASDPlayerState> WeakTargetPlayer(TargetPlayer);

	auto BroadcastResult = [this, WeakTargetPlayer, TargetSlot, TargetName, ClampedBulletCount, bHit]()
	{
		ASDPlayerState* ResolvedTargetPlayer = WeakTargetPlayer.Get();
		if (!ResolvedTargetPlayer)
		{
			return;
		}

		const int32 PreviousLives = ResolvedTargetPlayer->Lives;
		bool bEliminated = false;
		if (bHit)
		{
			ResolvedTargetPlayer->Lives = FMath::Max(0, ResolvedTargetPlayer->Lives - 1);
			if (PreviousLives > 0 && ResolvedTargetPlayer->Lives <= 0)
			{
				bEliminated = true;
				MultiplayerEliminationOrder.AddUnique(ResolvedTargetPlayer);
			}
		}

		ResolvedTargetPlayer->ForceNetUpdate();
		RefreshMultiplayerCharacterVisibility();
		const int32 RemainingLives = ResolvedTargetPlayer->Lives;
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

		if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
		{
			ShowDownGameState->BroadcastMultiplayerRouletteResult(
				TargetSlot,
				TargetName,
				ClampedBulletCount,
				bHit,
				RemainingLives);
		}
	};

	auto StartPresentation = [this, WeakTargetPlayer, TargetSlot, TargetName, ClampedBulletCount, bHit, ResultDelay, BroadcastResult]()
	{
		if (!WeakTargetPlayer.IsValid())
		{
			return;
		}

		if (AShowDownGameStateBase* ShowDownGameState = GetShowDownGameState())
		{
			ShowDownGameState->BroadcastMultiplayerRouletteStarted(TargetSlot, TargetName, ClampedBulletCount);
			ShowDownGameState->BroadcastMultiplayerRoulettePresentation(TargetSlot, TargetName, ClampedBulletCount, bHit);
		}

		if (ResultDelay <= KINDA_SMALL_NUMBER)
		{
			BroadcastResult();
			return;
		}

		FTimerDelegate ResultDelegate;
		ResultDelegate.BindWeakLambda(this, BroadcastResult);
		FTimerHandle ResultTimerHandle;
		GetWorldTimerManager().SetTimer(ResultTimerHandle, ResultDelegate, ResultDelay, false);
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
	}

	return SafeStartDelay + FinishDelay;
}

void AShowDownGameModeBase::EndMultiplayerRound()
{
	SetMultiplayerSelectableHand(nullptr);
	ClearBetBulletPresentation();
	ClearMultiplayerForeheadCards();
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
			ShowDownGameState->OnGameOver.Broadcast(EShowDownSide::Player);
		}
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

	if (bNeedRedeal)
	{
		DealMultiplayerHands();
	}

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
	if (const ASDCardPlacementAnchor* HandAnchor = GetHandAnchorForPlayerSlot(Player->ShowDownSlot))
	{
		if (USceneComponent* HandSlot = HandAnchor->GetSlotComponent())
		{
			return HandSlot;
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

	DealInitialHand();
	ShowEventDebugMessage(FString::Printf(TEXT("스테이지 %d 시작"), CurrentStageIndex + 1));
	BeginCardSelectionRound();

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

		RefreshCardPlacementAnchorCache();
		if (const TWeakObjectPtr<ASDCardPlacementAnchor>* RefreshedAnchor = CachedCardPlacementAnchors.Find(TargetRole))
		{
			return RefreshedAnchor->Get();
		}
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

		RefreshPlayerSeatCache();
		if (const TWeakObjectPtr<ASDPlayerSeat>* RefreshedSeat = CachedPlayerSeats.Find(Side))
		{
			return RefreshedSeat->Get();
		}
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
