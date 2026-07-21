#include "ShowDownGameStateBase.h"

#include "Audio/ShowDownAudioSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

namespace
{
	FString GetPhaseDebugName(EShowDownPhase Phase)
	{
		switch (Phase)
		{
		case EShowDownPhase::SelectCard:
			return TEXT("카드 선택");
		case EShowDownPhase::Betting:
			return TEXT("베팅");
		case EShowDownPhase::Reveal:
			return TEXT("카드 공개");
		case EShowDownPhase::Roulette:
			return TEXT("룰렛");
		case EShowDownPhase::RoundEnd:
			return TEXT("라운드 종료");
		case EShowDownPhase::GameOver:
			return TEXT("게임 종료");
		case EShowDownPhase::None:
		default:
			return TEXT("없음");
		}
	}

	void ShowPresentationDebugMessage(const FString& Prefix, EShowDownPhase Phase, const FColor&)
	{
		UE_LOG(
			LogTemp,
			Verbose,
			TEXT("[%s] %s presentation"),
			*Prefix,
			*GetPhaseDebugName(Phase));
	}

	void ShowRawDebugMessage(const FString& Message, const FColor&)
	{
		UE_LOG(LogTemp, Verbose, TEXT("Presentation: %s"), *Message);
	}

	void NotifyAudioPhaseChanged(const AShowDownGameStateBase* GameState, EShowDownPhase Phase)
	{
		UGameInstance* GameInstance = GameState ? GameState->GetGameInstance() : nullptr;
		if (UShowDownAudioSubsystem* AudioSubsystem = GameInstance
			? GameInstance->GetSubsystem<UShowDownAudioSubsystem>()
			: nullptr)
		{
			AudioSubsystem->NotifyPhaseChanged(Phase);
		}
	}
}

void AShowDownGameStateBase::SetPhase(EShowDownPhase NewPhase)
{
	if (!HasAuthority() || CurrentPhase == NewPhase)
	{
		return;
	}

	const EShowDownPhase PhaseToFinish = CurrentPresentationPhase != EShowDownPhase::None
		? CurrentPresentationPhase
		: CurrentPhase;
	EventEnd(PhaseToFinish);

	CurrentPhase = NewPhase;
	OnPhaseChanged.Broadcast(CurrentPhase);
	NotifyAudioPhaseChanged(this, CurrentPhase);
	EventStart(CurrentPhase);
}

void AShowDownGameStateBase::SetMatchMode(EShowDownMatchMode NewMatchMode)
{
	if (!HasAuthority() || MatchMode == NewMatchMode)
	{
		return;
	}

	MatchMode = NewMatchMode;
	OnRep_MatchMode();
}

void AShowDownGameStateBase::SetPlayerSlot(const FShowDownNetworkPlayerSlot& SlotState)
{
	if (!HasAuthority() || SlotState.Slot == EShowDownPlayerSlot::None)
	{
		return;
	}

	for (FShowDownNetworkPlayerSlot& ExistingSlot : PlayerSlots)
	{
		if (ExistingSlot.Slot == SlotState.Slot)
		{
			ExistingSlot = SlotState;
			OnRep_PlayerSlots();
			return;
		}
	}

	PlayerSlots.Add(SlotState);
	OnRep_PlayerSlots();
}

void AShowDownGameStateBase::ClearPlayerSlot(EShowDownPlayerSlot Slot)
{
	if (!HasAuthority() || Slot == EShowDownPlayerSlot::None)
	{
		return;
	}

	PlayerSlots.RemoveAll([Slot](const FShowDownNetworkPlayerSlot& ExistingSlot)
	{
		return ExistingSlot.Slot == Slot;
	});
	OnRep_PlayerSlots();
}

bool AShowDownGameStateBase::IsMultiplayerMatch() const
{
	return MatchMode == EShowDownMatchMode::Multiplayer;
}

void AShowDownGameStateBase::SetNameTagRoundStatus(
	int32 LoadedBulletCount,
	EShowDownSide TurnSide,
	EShowDownPlayerSlot TurnSlot)
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 ClampedLoadedBulletCount = FMath::Clamp(LoadedBulletCount, 0, 6);
	if (NameTagLoadedBulletCount == ClampedLoadedBulletCount
		&& NameTagTurnSide == TurnSide
		&& NameTagTurnSlot == TurnSlot)
	{
		return;
	}

	NameTagLoadedBulletCount = ClampedLoadedBulletCount;
	NameTagTurnSide = TurnSide;
	NameTagTurnSlot = TurnSlot;
	OnNameTagRoundStatusChanged.Broadcast();
	ForceNetUpdate();
}

void AShowDownGameStateBase::SetNameTagSingleRoundStatus(
	int32 PlayerLoadedBulletCount,
	int32 CollectorLoadedBulletCount,
	EShowDownSide TurnSide,
	EShowDownPlayerSlot TurnSlot)
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 ClampedPlayerLoadedBulletCount = FMath::Clamp(PlayerLoadedBulletCount, 0, 6);
	const int32 ClampedCollectorLoadedBulletCount = FMath::Clamp(CollectorLoadedBulletCount, 0, 6);
	const int32 ClampedLoadedBulletCount = FMath::Max(ClampedPlayerLoadedBulletCount, ClampedCollectorLoadedBulletCount);
	if (NameTagPlayerLoadedBulletCount == ClampedPlayerLoadedBulletCount
		&& NameTagCollectorLoadedBulletCount == ClampedCollectorLoadedBulletCount
		&& NameTagLoadedBulletCount == ClampedLoadedBulletCount
		&& NameTagTurnSide == TurnSide
		&& NameTagTurnSlot == TurnSlot)
	{
		return;
	}

	NameTagPlayerLoadedBulletCount = ClampedPlayerLoadedBulletCount;
	NameTagCollectorLoadedBulletCount = ClampedCollectorLoadedBulletCount;
	NameTagLoadedBulletCount = ClampedLoadedBulletCount;
	NameTagTurnSide = TurnSide;
	NameTagTurnSlot = TurnSlot;
	OnNameTagRoundStatusChanged.Broadcast();
	ForceNetUpdate();
}

void AShowDownGameStateBase::SetNameTagPlayerLoadedBulletCount(
	EShowDownPlayerSlot Slot,
	int32 LoadedBulletCount)
{
	if (!HasAuthority() || Slot == EShowDownPlayerSlot::None)
	{
		return;
	}

	const int32 ClampedLoadedBulletCount = FMath::Clamp(LoadedBulletCount, 0, 6);
	for (FShowDownNameTagPlayerBetState& PlayerBet : NameTagPlayerBets)
	{
		if (PlayerBet.Slot == Slot)
		{
			if (PlayerBet.LoadedBulletCount == ClampedLoadedBulletCount)
			{
				return;
			}

			PlayerBet.LoadedBulletCount = ClampedLoadedBulletCount;
			OnNameTagRoundStatusChanged.Broadcast();
			ForceNetUpdate();
			return;
		}
	}

	FShowDownNameTagPlayerBetState NewPlayerBet;
	NewPlayerBet.Slot = Slot;
	NewPlayerBet.LoadedBulletCount = ClampedLoadedBulletCount;
	NameTagPlayerBets.Add(NewPlayerBet);
	OnNameTagRoundStatusChanged.Broadcast();
	ForceNetUpdate();
}

void AShowDownGameStateBase::SetLastBetActionNotice(
	EShowDownSide Side,
	EShowDownPlayerSlot PlayerSlot,
	const FString& ActorName,
	EShowDownBetAction Action,
	int32 TargetBet,
	int32 RaiseAmount,
	bool bWasAutomatic)
{
	if (!HasAuthority())
	{
		return;
	}

	LastBetActionNotice.Side = Side;
	LastBetActionNotice.PlayerSlot = PlayerSlot;
	LastBetActionNotice.ActorName = ActorName.TrimStartAndEnd().Left(32);
	LastBetActionNotice.Action = Action;
	LastBetActionNotice.TargetBet = FMath::Clamp(TargetBet, 0, 6);
	LastBetActionNotice.RaiseAmount = Action == EShowDownBetAction::Raise
		? FMath::Clamp(RaiseAmount, 0, 6)
		: 0;
	LastBetActionNotice.bWasAutomatic = bWasAutomatic;
	LastBetActionNotice.ServerWorldTimeSeconds = GetServerWorldTimeSeconds();
	LastBetActionNotice.Revision = LastBetActionNotice.Revision >= MAX_int32
		? 1
		: LastBetActionNotice.Revision + 1;
	MulticastBetActionNotice(LastBetActionNotice);
	ForceNetUpdate();
}

void AShowDownGameStateBase::MulticastBetActionNotice_Implementation(
	const FShowDownBetActionNotice& Notice)
{
	// The replicated property keeps the latest value for late joiners. The
	// reliable RPC additionally preserves every committed action instead of
	// allowing rapid property changes to be coalesced into only the newest one.
	LastBetActionNotice = Notice;
}

int32 AShowDownGameStateBase::SetDecisionTimerState(
	EShowDownDecisionTimerKind Kind,
	float DeadlineServerWorldTimeSeconds,
	float DurationSeconds,
	EShowDownSide TargetSide,
	EShowDownPlayerSlot TargetSlot)
{
	if (!HasAuthority())
	{
		return DecisionTimerState.Revision;
	}

	if (Kind == EShowDownDecisionTimerKind::None)
	{
		ClearDecisionTimerState();
		return DecisionTimerState.Revision;
	}

	const int32 NextRevision = DecisionTimerState.Revision >= MAX_int32
		? 1
		: DecisionTimerState.Revision + 1;
	DecisionTimerState.Kind = Kind;
	DecisionTimerState.DeadlineServerWorldTimeSeconds = DeadlineServerWorldTimeSeconds;
	DecisionTimerState.DurationSeconds = FMath::Max(0.0f, DurationSeconds);
	DecisionTimerState.TargetSide = TargetSide;
	DecisionTimerState.TargetSlot = TargetSlot;
	DecisionTimerState.Revision = NextRevision;
	ForceNetUpdate();
	return NextRevision;
}

void AShowDownGameStateBase::ClearDecisionTimerState()
{
	if (!HasAuthority() || DecisionTimerState.Kind == EShowDownDecisionTimerKind::None)
	{
		return;
	}

	const int32 NextRevision = DecisionTimerState.Revision >= MAX_int32
		? 1
		: DecisionTimerState.Revision + 1;
	DecisionTimerState = FShowDownDecisionTimerState();
	DecisionTimerState.Revision = NextRevision;
	ForceNetUpdate();
}

void AShowDownGameStateBase::SetInitialDealDeckVisualState(
	FName SourceActorTag,
	int32 RemainingSteps,
	int32 TotalSteps)
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 ClampedTotalSteps = FMath::Clamp(TotalSteps, 0, static_cast<int32>(MAX_uint8));
	InitialDealDeckVisualState.TotalSteps = static_cast<uint8>(ClampedTotalSteps);
	InitialDealDeckVisualState.RemainingSteps = static_cast<uint8>(FMath::Clamp(
		RemainingSteps,
		0,
		ClampedTotalSteps));
	InitialDealDeckVisualState.SourceActorTag = SourceActorTag;
	++InitialDealDeckVisualState.Revision;
	OnRep_InitialDealDeckVisualState();
	ForceNetUpdate();
}

AStaticMeshActor* AShowDownGameStateBase::ResolveInitialDealDeckVisualActor(FName SourceActorTag)
{
	if (InitialDealDeckVisualActor.IsValid() && CachedInitialDealDeckSourceActorTag == SourceActorTag)
	{
		return InitialDealDeckVisualActor.Get();
	}

	InitialDealDeckVisualActor.Reset();
	CachedInitialDealDeckSourceActorTag = SourceActorTag;
	bInitialDealDeckAuthoredTransformCaptured = false;
	AStaticMeshActor* AuthoredMeshFallback = nullptr;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			AStaticMeshActor* MeshActor = *It;
			const UStaticMeshComponent* MeshComponent = MeshActor ? MeshActor->GetStaticMeshComponent() : nullptr;
			const UStaticMesh* StaticMesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr;
			if (!MeshActor || !StaticMesh)
			{
				continue;
			}

			if (!SourceActorTag.IsNone() && MeshActor->ActorHasTag(SourceActorTag))
			{
				InitialDealDeckVisualActor = MeshActor;
				return MeshActor;
			}
			if (!AuthoredMeshFallback
				&& StaticMesh->GetPathName() == TEXT("/Game/Fab/Card/SM_carddummyMesh.SM_carddummyMesh"))
			{
				AuthoredMeshFallback = MeshActor;
			}
		}
	}

	InitialDealDeckVisualActor = AuthoredMeshFallback;
	return AuthoredMeshFallback;
}

void AShowDownGameStateBase::ApplyInitialDealDeckVisualState()
{
	AStaticMeshActor* DeckActor = ResolveInitialDealDeckVisualActor(InitialDealDeckVisualState.SourceActorTag);
	UStaticMeshComponent* DeckMesh = DeckActor ? DeckActor->GetStaticMeshComponent() : nullptr;
	if (!DeckActor || !DeckMesh)
	{
		if (GetWorld()
			&& InitialDealDeckVisualRetryAttempts < 20
			&& !GetWorldTimerManager().IsTimerActive(InitialDealDeckVisualRetryTimerHandle))
		{
			++InitialDealDeckVisualRetryAttempts;
			GetWorldTimerManager().SetTimer(
				InitialDealDeckVisualRetryTimerHandle,
				this,
				&AShowDownGameStateBase::ApplyInitialDealDeckVisualState,
				0.25f,
				false);
		}
		return;
	}
	GetWorldTimerManager().ClearTimer(InitialDealDeckVisualRetryTimerHandle);
	InitialDealDeckVisualRetryAttempts = 0;

	if (!bInitialDealDeckAuthoredTransformCaptured)
	{
		InitialDealDeckAuthoredLocation = DeckActor->GetActorLocation();
		InitialDealDeckAuthoredScale = DeckActor->GetActorScale3D();
		DeckMesh->UpdateBounds();
		InitialDealDeckAuthoredBottomZ = DeckMesh->Bounds.Origin.Z - DeckMesh->Bounds.BoxExtent.Z;
		bInitialDealDeckAuthoredTransformCaptured = true;
	}

	DeckMesh->SetMobility(EComponentMobility::Movable);
	DeckActor->SetActorLocation(InitialDealDeckAuthoredLocation, false, nullptr, ETeleportType::TeleportPhysics);
	DeckActor->SetActorScale3D(InitialDealDeckAuthoredScale);
	if (InitialDealDeckVisualState.TotalSteps == 0)
	{
		DeckActor->SetActorHiddenInGame(false);
		return;
	}
	if (InitialDealDeckVisualState.RemainingSteps == 0)
	{
		DeckActor->SetActorHiddenInGame(true);
		return;
	}

	const float HeightAlpha = static_cast<float>(InitialDealDeckVisualState.RemainingSteps)
		/ static_cast<float>(InitialDealDeckVisualState.TotalSteps);
	FVector ScaledDeck = InitialDealDeckAuthoredScale;
	ScaledDeck.Z *= HeightAlpha;
	DeckActor->SetActorScale3D(ScaledDeck);
	DeckMesh->UpdateBounds();
	const float ScaledBottomZ = DeckMesh->Bounds.Origin.Z - DeckMesh->Bounds.BoxExtent.Z;
	FVector BottomAnchoredLocation = DeckActor->GetActorLocation();
	BottomAnchoredLocation.Z += InitialDealDeckAuthoredBottomZ - ScaledBottomZ;
	DeckActor->SetActorLocation(BottomAnchoredLocation, false, nullptr, ETeleportType::TeleportPhysics);
	DeckActor->SetActorHiddenInGame(false);
}

void AShowDownGameStateBase::EventStart(EShowDownPhase Phase)
{
	if (!HasAuthority() || Phase == EShowDownPhase::None)
	{
		return;
	}

	CurrentPresentationPhase = Phase;
	bPresentationPlaying = true;
	if (bShowPresentationDebugMessages)
	{
		ShowPresentationDebugMessage(TEXT("호출"), Phase, FColor::Orange);
	}

	const bool bHasPresentationEvent = OnPresentationStarted.IsBound();
	MulticastPresentationStarted(Phase);

	if (!bHasPresentationEvent)
	{
		if (bShowPresentationDebugMessages)
		{
			ShowRawDebugMessage(TEXT("*"), FColor::White);
		}
		EventEnd(Phase);
	}
}

void AShowDownGameStateBase::EventEnd(EShowDownPhase Phase)
{
	if (!HasAuthority() || Phase == EShowDownPhase::None || !bPresentationPlaying)
	{
		return;
	}

	const EShowDownPhase FinishedPhase = CurrentPresentationPhase != EShowDownPhase::None
		? CurrentPresentationPhase
		: Phase;

	bPresentationPlaying = false;
	MulticastPresentationFinished(FinishedPhase);
	CurrentPresentationPhase = EShowDownPhase::None;
	if (bShowPresentationDebugMessages)
	{
		ShowPresentationDebugMessage(TEXT("호출종료"), FinishedPhase, FColor::Green);
	}
}

void AShowDownGameStateBase::StartPresentation(EShowDownPhase Phase)
{
	EventStart(Phase);
}

void AShowDownGameStateBase::FinishPresentation(EShowDownPhase Phase)
{
	EventEnd(Phase);
}

void AShowDownGameStateBase::BroadcastCollectorDialogue(const FString& Dialogue, const FString& Intent)
{
	OnCollectorDialogue.Broadcast(Dialogue, Intent);
}

void AShowDownGameStateBase::BroadcastCollectorLLMDecision(const FString& Dialogue, const FString& Intent, EShowDownBetAction Action, int32 TargetBet)
{
	if (HasAuthority())
	{
		MulticastCollectorLLMDecision(Dialogue, Intent, Action, TargetBet);
		return;
	}

	OnCollectorDialogue.Broadcast(Dialogue, Intent);
	OnCollectorLLMDecision.Broadcast(Dialogue, Intent, Action, TargetBet);
	OnCollectorLLMStatus.Broadcast(true, FString());
}

void AShowDownGameStateBase::BroadcastCollectorLLMStatus(bool bSuccess, const FString& Message)
{
	if (HasAuthority())
	{
		MulticastCollectorLLMStatus(bSuccess, Message);
		return;
	}

	OnCollectorLLMStatus.Broadcast(bSuccess, Message);
}

void AShowDownGameStateBase::BroadcastChatMessage(const FString& SenderName, const FString& Message)
{
	if (HasAuthority())
	{
		MulticastChatMessage(SenderName, Message);
		return;
	}

	OnChatMessageReceived.Broadcast(SenderName, Message);
}

void AShowDownGameStateBase::BroadcastCardsRevealed(int32 PlayerCard, int32 CollectorCard)
{
	if (HasAuthority())
	{
		MulticastCardsRevealed(PlayerCard, CollectorCard);
		return;
	}

	OnCardsRevealed.Broadcast(PlayerCard, CollectorCard);
}

void AShowDownGameStateBase::BroadcastRoundResolved(EShowDownRoundResult Result)
{
	if (HasAuthority())
	{
		MulticastRoundResolved(Result);
		return;
	}

	OnRoundResolved.Broadcast(Result);
}

void AShowDownGameStateBase::BroadcastGameOver(EShowDownSide Winner)
{
	if (HasAuthority())
	{
		MulticastGameOver(Winner);
		return;
	}

	OnGameOver.Broadcast(Winner);
}

void AShowDownGameStateBase::BroadcastMultiplayerRouletteStarted(
	EShowDownPlayerSlot TargetSlot,
	const FString& TargetName,
	int32 BulletCount)
{
	if (HasAuthority())
	{
		MulticastMultiplayerRouletteStarted(TargetSlot, TargetName, BulletCount);
		return;
	}

	OnMultiplayerRouletteStarted.Broadcast(TargetSlot, TargetName, BulletCount);
}

void AShowDownGameStateBase::BroadcastMultiplayerRoulettePresentation(
	EShowDownPlayerSlot TargetSlot,
	const FString& TargetName,
	int32 BulletCount,
	bool bHit)
{
	BroadcastMultiplayerRoulettePresentationWithContext(
		TargetSlot,
		TargetName,
		BulletCount,
		bHit,
		MultiplayerMatchSequence,
		MultiplayerRoundSequence);
}

void AShowDownGameStateBase::BroadcastMultiplayerRoulettePresentationWithContext(
	EShowDownPlayerSlot TargetSlot,
	const FString& TargetName,
	int32 BulletCount,
	bool bHit,
	int32 MatchSequence,
	int32 RoundSequence)
{
	if (HasAuthority())
	{
		MulticastMultiplayerRoulettePresentation(
			TargetSlot,
			TargetName,
			BulletCount,
			bHit,
			MatchSequence,
			RoundSequence);
		return;
	}

	OnMultiplayerRoulettePresentation.Broadcast(TargetSlot, TargetName, BulletCount, bHit);
	OnMultiplayerRoulettePresentationContext.Broadcast(
		TargetSlot,
		TargetName,
		BulletCount,
		bHit,
		MatchSequence,
		RoundSequence);
}

void AShowDownGameStateBase::SetMultiplayerPresentationContext(
	int32 MatchSequence,
	int32 RoundSequence)
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 SafeMatchSequence = FMath::Max(0, MatchSequence);
	const int32 SafeRoundSequence = FMath::Max(0, RoundSequence);
	if (MultiplayerMatchSequence == SafeMatchSequence
		&& MultiplayerRoundSequence == SafeRoundSequence)
	{
		return;
	}

	MultiplayerMatchSequence = SafeMatchSequence;
	MultiplayerRoundSequence = SafeRoundSequence;
	ForceNetUpdate();
	MulticastMultiplayerPresentationContext(
		MultiplayerMatchSequence,
		MultiplayerRoundSequence);
}

void AShowDownGameStateBase::BroadcastMultiplayerRouletteResult(
	EShowDownPlayerSlot TargetSlot,
	const FString& TargetName,
	int32 BulletCount,
	bool bHit,
	int32 RemainingLives)
{
	if (HasAuthority())
	{
		MulticastMultiplayerRouletteResult(TargetSlot, TargetName, BulletCount, bHit, RemainingLives);
		return;
	}

	OnMultiplayerRouletteResult.Broadcast(TargetSlot, TargetName, BulletCount, bHit, RemainingLives);
}

void AShowDownGameStateBase::BroadcastTableCinematicCue(
	ESDTableCinematicCue Cue,
	uint8 PlayerSlotMask)
{
	if (HasAuthority())
	{
		MulticastTableCinematicCue(Cue, PlayerSlotMask);
		return;
	}

	OnTableCinematicCue.Broadcast(Cue, PlayerSlotMask);
}

void AShowDownGameStateBase::MulticastCollectorLLMDecision_Implementation(const FString& Dialogue, const FString& Intent, EShowDownBetAction Action, int32 TargetBet)
{
	OnCollectorDialogue.Broadcast(Dialogue, Intent);
	OnCollectorLLMDecision.Broadcast(Dialogue, Intent, Action, TargetBet);
	OnCollectorLLMStatus.Broadcast(true, FString());
}

void AShowDownGameStateBase::MulticastCollectorLLMStatus_Implementation(bool bSuccess, const FString& Message)
{
	OnCollectorLLMStatus.Broadcast(bSuccess, Message);
}

void AShowDownGameStateBase::MulticastChatMessage_Implementation(const FString& SenderName, const FString& Message)
{
	OnChatMessageReceived.Broadcast(SenderName, Message);
}

void AShowDownGameStateBase::MulticastCardsRevealed_Implementation(int32 PlayerCard, int32 CollectorCard)
{
	OnCardsRevealed.Broadcast(PlayerCard, CollectorCard);
}

void AShowDownGameStateBase::MulticastRoundResolved_Implementation(EShowDownRoundResult Result)
{
	OnRoundResolved.Broadcast(Result);
}

void AShowDownGameStateBase::MulticastGameOver_Implementation(EShowDownSide Winner)
{
	OnGameOver.Broadcast(Winner);
}

void AShowDownGameStateBase::MulticastMultiplayerRouletteStarted_Implementation(
	EShowDownPlayerSlot TargetSlot,
	const FString& TargetName,
	int32 BulletCount)
{
	OnMultiplayerRouletteStarted.Broadcast(TargetSlot, TargetName, BulletCount);
}

void AShowDownGameStateBase::MulticastMultiplayerRoulettePresentation_Implementation(
	EShowDownPlayerSlot TargetSlot,
	const FString& TargetName,
	int32 BulletCount,
	bool bHit,
	int32 MatchSequence,
	int32 RoundSequence)
{
	OnMultiplayerRoulettePresentation.Broadcast(TargetSlot, TargetName, BulletCount, bHit);
	OnMultiplayerRoulettePresentationContext.Broadcast(
		TargetSlot,
		TargetName,
		BulletCount,
		bHit,
		MatchSequence,
		RoundSequence);
}

void AShowDownGameStateBase::MulticastMultiplayerPresentationContext_Implementation(
	int32 MatchSequence,
	int32 RoundSequence)
{
	MultiplayerMatchSequence = FMath::Max(0, MatchSequence);
	MultiplayerRoundSequence = FMath::Max(0, RoundSequence);
	OnMultiplayerPresentationContextChanged.Broadcast(
		MultiplayerMatchSequence,
		MultiplayerRoundSequence);
}

void AShowDownGameStateBase::MulticastPresentationStarted_Implementation(EShowDownPhase Phase)
{
	if (!HasAuthority())
	{
		CurrentPresentationPhase = Phase;
		bPresentationPlaying = true;
	}
	OnPresentationStarted.Broadcast(Phase);
}

void AShowDownGameStateBase::MulticastPresentationFinished_Implementation(EShowDownPhase Phase)
{
	if (!HasAuthority())
	{
		bPresentationPlaying = false;
		CurrentPresentationPhase = Phase;
	}
	OnPresentationFinished.Broadcast(Phase);
	if (!HasAuthority())
	{
		CurrentPresentationPhase = EShowDownPhase::None;
	}
}

void AShowDownGameStateBase::MulticastMultiplayerRouletteResult_Implementation(
	EShowDownPlayerSlot TargetSlot,
	const FString& TargetName,
	int32 BulletCount,
	bool bHit,
	int32 RemainingLives)
{
	OnMultiplayerRouletteResult.Broadcast(TargetSlot, TargetName, BulletCount, bHit, RemainingLives);
}

void AShowDownGameStateBase::MulticastTableCinematicCue_Implementation(
	ESDTableCinematicCue Cue,
	uint8 PlayerSlotMask)
{
	OnTableCinematicCue.Broadcast(Cue, PlayerSlotMask);
}

void AShowDownGameStateBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AShowDownGameStateBase, CurrentPhase);
	DOREPLIFETIME(AShowDownGameStateBase, CurrentPresentationPhase);
	DOREPLIFETIME(AShowDownGameStateBase, bPresentationPlaying);
	DOREPLIFETIME(AShowDownGameStateBase, CurrentStage);
	DOREPLIFETIME(AShowDownGameStateBase, CurrentRound);
	DOREPLIFETIME(AShowDownGameStateBase, MultiplayerMatchSequence);
	DOREPLIFETIME(AShowDownGameStateBase, MultiplayerRoundSequence);
	DOREPLIFETIME(AShowDownGameStateBase, MatchMode);
	DOREPLIFETIME(AShowDownGameStateBase, PlayerSlots);
	DOREPLIFETIME(AShowDownGameStateBase, NameTagLoadedBulletCount);
	DOREPLIFETIME(AShowDownGameStateBase, NameTagPlayerLoadedBulletCount);
	DOREPLIFETIME(AShowDownGameStateBase, NameTagCollectorLoadedBulletCount);
	DOREPLIFETIME(AShowDownGameStateBase, NameTagPlayerBets);
	DOREPLIFETIME(AShowDownGameStateBase, NameTagTurnSide);
	DOREPLIFETIME(AShowDownGameStateBase, NameTagTurnSlot);
	DOREPLIFETIME(AShowDownGameStateBase, LastBetActionNotice);
	DOREPLIFETIME(AShowDownGameStateBase, DecisionTimerState);
	DOREPLIFETIME(AShowDownGameStateBase, InitialDealDeckVisualState);
}

void AShowDownGameStateBase::OnRep_CurrentPhase()
{
	OnPhaseChanged.Broadcast(CurrentPhase);
	NotifyAudioPhaseChanged(this, CurrentPhase);
}

void AShowDownGameStateBase::OnRep_CurrentStage()
{
	OnStageChanged.Broadcast(CurrentStage);
}

void AShowDownGameStateBase::OnRep_MatchMode()
{
}

void AShowDownGameStateBase::OnRep_PlayerSlots()
{
}

void AShowDownGameStateBase::OnRep_NameTagRoundStatus()
{
	OnNameTagRoundStatusChanged.Broadcast();
}

void AShowDownGameStateBase::OnRep_InitialDealDeckVisualState()
{
	ApplyInitialDealDeckVisualState();
}
