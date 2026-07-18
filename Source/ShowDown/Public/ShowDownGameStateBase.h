#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "ShowDownTypes.h"
#include "TimerManager.h"
#include "ShowDownGameStateBase.generated.h"

class AStaticMeshActor;
class FLifetimeProperty;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShowDownPhaseChangedSignature, EShowDownPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FShowDownGameStateBetChangedSignature, EShowDownSide, Side, int32, BulletCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShowDownCardSelectedSignature, EShowDownSide, Side);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FShowDownBetActionCommittedSignature, EShowDownSide, Side, EShowDownBetAction, Action, int32, TargetBet);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FShowDownCardsRevealedSignature, int32, PlayerCard, int32, CollectorCard);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShowDownRoundResolvedSignature, EShowDownRoundResult, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FShowDownRouletteStartedSignature, EShowDownSide, Target, int32, BulletCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FShowDownRouletteResultSignature, EShowDownSide, Target, bool, bHit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FShowDownLifeChangedSignature, EShowDownSide, Target, int32, Life);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShowDownStageChangedSignature, int32, Stage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShowDownGameOverSignature, EShowDownSide, Winner);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShowDownPresentationSignature, EShowDownPhase, Phase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FShowDownCollectorDialogueSignature, const FString&, Dialogue, const FString&, Intent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FShowDownCollectorLLMDecisionSignature, const FString&, Dialogue, const FString&, Intent, EShowDownBetAction, Action, int32, TargetBet);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FShowDownCollectorLLMStatusSignature, bool, bSuccess, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FShowDownChatMessageSignature, const FString&, SenderName, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShowDownMultiplayerCardSelectedSignature, EShowDownPlayerSlot, PlayerSlot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FShowDownMultiplayerBetActionCommittedSignature, EShowDownPlayerSlot, PlayerSlot, EShowDownBetAction, Action, int32, TargetBet);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FShowDownMultiplayerRouletteStartedSignature, EShowDownPlayerSlot, TargetSlot, const FString&, TargetName, int32, BulletCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FShowDownMultiplayerRoulettePresentationSignature, EShowDownPlayerSlot, TargetSlot, const FString&, TargetName, int32, BulletCount, bool, bHit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FShowDownMultiplayerRouletteResultSignature, EShowDownPlayerSlot, TargetSlot, const FString&, TargetName, int32, BulletCount, bool, bHit, int32, RemainingLives);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FShowDownNameTagRoundStatusChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FShowDownTableCinematicCueSignature, ESDTableCinematicCue, Cue, uint8, PlayerSlotMask);
DECLARE_MULTICAST_DELEGATE_SixParams(
	FShowDownMultiplayerRoulettePresentationContextSignature,
	EShowDownPlayerSlot,
	const FString&,
	int32,
	bool,
	int32,
	int32);
DECLARE_MULTICAST_DELEGATE_TwoParams(
	FShowDownMultiplayerPresentationContextChangedSignature,
	int32,
	int32);

USTRUCT(BlueprintType)
struct FShowDownNameTagPlayerBetState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Name Tag")
	EShowDownPlayerSlot Slot = EShowDownPlayerSlot::None;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Name Tag")
	int32 LoadedBulletCount = 0;
};

/**
 * Last committed betting action, replicated as one value so every local HUD can
 * show the same short-lived notice without parsing localized chat text.
 */
USTRUCT(BlueprintType)
struct FShowDownBetActionNotice
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Betting")
	EShowDownSide Side = EShowDownSide::Player;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Betting")
	EShowDownPlayerSlot PlayerSlot = EShowDownPlayerSlot::None;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Betting")
	FString ActorName;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Betting")
	EShowDownBetAction Action = EShowDownBetAction::Check;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Betting")
	int32 TargetBet = 0;

	/** Number of bullets added by this action. Non-zero only for a raise. */
	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Betting")
	int32 RaiseAmount = 0;

	/** True when the server committed the action because the decision timer expired. */
	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Betting")
	bool bWasAutomatic = false;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Betting")
	float ServerWorldTimeSeconds = -1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Betting")
	int32 Revision = 0;
};

/**
 * One atomic, server-authored decision window. Clients derive the remaining
 * time from DeadlineServerWorldTimeSeconds and the synchronized GameState clock
 * instead of receiving a replicated value every second.
 */
USTRUCT(BlueprintType)
struct FShowDownDecisionTimerState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Decision Timer")
	EShowDownDecisionTimerKind Kind = EShowDownDecisionTimerKind::None;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Decision Timer")
	float DeadlineServerWorldTimeSeconds = -1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Decision Timer")
	float DurationSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Decision Timer")
	EShowDownSide TargetSide = EShowDownSide::Player;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Decision Timer")
	EShowDownPlayerSlot TargetSlot = EShowDownPlayerSlot::None;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Decision Timer")
	int32 Revision = 0;
};

USTRUCT()
struct FSDInitialDealDeckVisualState
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 RemainingSteps = 0;

	UPROPERTY()
	uint8 TotalSteps = 0;

	UPROPERTY()
	FName SourceActorTag = NAME_None;

	UPROPERTY()
	uint16 Revision = 0;
};

UCLASS()
class SHOWDOWN_API AShowDownGameStateBase : public AGameStateBase
{
	GENERATED_BODY()

public:
	//게임 단계가 바뀔 때 UI/연출에 알리는 이벤트
	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Events")
	FShowDownPhaseChangedSignature OnPhaseChanged;

	//베팅 값이 바뀔 때 UI/연출에 알리는 이벤트
	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Events")
	FShowDownGameStateBetChangedSignature OnBetChanged;

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Events")
	FShowDownCardSelectedSignature OnCardSelected;

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Events")
	FShowDownBetActionCommittedSignature OnBetActionCommitted;

	//양쪽 카드가 공개될 때 UI/연출에 알리는 이벤트
	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Events")
	FShowDownCardsRevealedSignature OnCardsRevealed;

	//라운드 승패가 결정될 때 UI/연출에 알리는 이벤트
	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Events")
	FShowDownRoundResolvedSignature OnRoundResolved;

	//룰렛이 시작될 때 UI/연출에 알리는 이벤트
	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Events")
	FShowDownRouletteStartedSignature OnRouletteStarted;

	//룰렛 결과가 나왔을 때 UI/연출에 알리는 이벤트
	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Events")
	FShowDownRouletteResultSignature OnRouletteResult;

	//목숨이 변했을 때 UI/연출에 알리는 이벤트
	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Events")
	FShowDownLifeChangedSignature OnLifeChanged;

	//스테이지가 바뀔 때 UI/연출에 알리는 이벤트(
	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Events")
	FShowDownStageChangedSignature OnStageChanged;

	//게임이 끝났을 때 UI/연출에 알리는 이벤트
	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Events")
	FShowDownGameOverSignature OnGameOver;

	//현재 Phase 연출이 시작될 때 UI/연출에 알리는 이벤트
	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Events")
	FShowDownPresentationSignature OnPresentationStarted;

	//현재 Phase 연출이 끝났을 때 UI/연출에 알리는 이벤트
	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Events")
	FShowDownPresentationSignature OnPresentationFinished;

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Events")
	FShowDownCollectorDialogueSignature OnCollectorDialogue;

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Events")
	FShowDownCollectorLLMDecisionSignature OnCollectorLLMDecision;

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Events")
	FShowDownCollectorLLMStatusSignature OnCollectorLLMStatus;

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Events")
	FShowDownChatMessageSignature OnChatMessageReceived;

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Events|Multiplayer")
	FShowDownMultiplayerCardSelectedSignature OnMultiplayerCardSelected;

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Events|Multiplayer")
	FShowDownMultiplayerBetActionCommittedSignature OnMultiplayerBetActionCommitted;

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Events|Multiplayer")
	FShowDownMultiplayerRouletteStartedSignature OnMultiplayerRouletteStarted;

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Events|Multiplayer")
	FShowDownMultiplayerRoulettePresentationSignature OnMultiplayerRoulettePresentation;

	/** Native presentation event that also carries the authoritative match/round identity. */
	FShowDownMultiplayerRoulettePresentationContextSignature OnMultiplayerRoulettePresentationContext;

	/** Native boundary used by local presentation actors to expire delayed work. */
	FShowDownMultiplayerPresentationContextChangedSignature OnMultiplayerPresentationContextChanged;

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Events|Multiplayer")
	FShowDownMultiplayerRouletteResultSignature OnMultiplayerRouletteResult;

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Events|Name Tag")
	FShowDownNameTagRoundStatusChangedSignature OnNameTagRoundStatusChanged;

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Events|Presentation")
	FShowDownTableCinematicCueSignature OnTableCinematicCue;

	//현재 게임 진행 단계
	UPROPERTY(ReplicatedUsing = OnRep_CurrentPhase, BlueprintReadOnly, Category = "ShowDown|State")
	EShowDownPhase CurrentPhase = EShowDownPhase::None;

	//현재 연출 중인 Phase
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "ShowDown|State")
	EShowDownPhase CurrentPresentationPhase = EShowDownPhase::None;

	//연출 진행 중인지 여부
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "ShowDown|State")
	bool bPresentationPlaying = false;

	//현재 스테이지 번호
	UPROPERTY(ReplicatedUsing = OnRep_CurrentStage, BlueprintReadOnly, Category = "ShowDown|State")
	int32 CurrentStage = 1;

	//현재 라운드 번호
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "ShowDown|State")
	int32 CurrentRound = 1;

	/** Monotonic identity for the currently active multiplayer match. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "ShowDown|Multiplayer|Presentation")
	int32 MultiplayerMatchSequence = 0;

	/** Monotonic identity for the currently active multiplayer round/duel. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "ShowDown|Multiplayer|Presentation")
	int32 MultiplayerRoundSequence = 0;

	UPROPERTY(ReplicatedUsing = OnRep_MatchMode, BlueprintReadOnly, Category = "ShowDown|Multiplayer")
	EShowDownMatchMode MatchMode = EShowDownMatchMode::SinglePlayer;

	UPROPERTY(ReplicatedUsing = OnRep_PlayerSlots, BlueprintReadOnly, Category = "ShowDown|Multiplayer")
	TArray<FShowDownNetworkPlayerSlot> PlayerSlots;

	UPROPERTY(ReplicatedUsing = OnRep_NameTagRoundStatus, BlueprintReadOnly, Category = "ShowDown|Name Tag")
	int32 NameTagLoadedBulletCount = 0;

	UPROPERTY(ReplicatedUsing = OnRep_NameTagRoundStatus, BlueprintReadOnly, Category = "ShowDown|Name Tag")
	int32 NameTagPlayerLoadedBulletCount = 0;

	UPROPERTY(ReplicatedUsing = OnRep_NameTagRoundStatus, BlueprintReadOnly, Category = "ShowDown|Name Tag")
	int32 NameTagCollectorLoadedBulletCount = 0;

	UPROPERTY(ReplicatedUsing = OnRep_NameTagRoundStatus, BlueprintReadOnly, Category = "ShowDown|Name Tag")
	TArray<FShowDownNameTagPlayerBetState> NameTagPlayerBets;

	UPROPERTY(ReplicatedUsing = OnRep_NameTagRoundStatus, BlueprintReadOnly, Category = "ShowDown|Name Tag")
	EShowDownSide NameTagTurnSide = EShowDownSide::Player;

	UPROPERTY(ReplicatedUsing = OnRep_NameTagRoundStatus, BlueprintReadOnly, Category = "ShowDown|Name Tag")
	EShowDownPlayerSlot NameTagTurnSlot = EShowDownPlayerSlot::None;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "ShowDown|Betting")
	FShowDownBetActionNotice LastBetActionNotice;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "ShowDown|Decision Timer")
	FShowDownDecisionTimerState DecisionTimerState;

	UPROPERTY(ReplicatedUsing = OnRep_InitialDealDeckVisualState)
	FSDInitialDealDeckVisualState InitialDealDeckVisualState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Debug")
	bool bShowPresentationDebugMessages = false;

	//현재 게임 진행 단계 변경
	UFUNCTION(BlueprintCallable, Category = "ShowDown|State")
	void SetPhase(EShowDownPhase NewPhase);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Multiplayer")
	void SetMatchMode(EShowDownMatchMode NewMatchMode);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Multiplayer")
	void SetPlayerSlot(const FShowDownNetworkPlayerSlot& SlotState);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Multiplayer")
	void ClearPlayerSlot(EShowDownPlayerSlot Slot);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ShowDown|Multiplayer")
	bool IsMultiplayerMatch() const;

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Name Tag")
	void SetNameTagRoundStatus(int32 LoadedBulletCount, EShowDownSide TurnSide, EShowDownPlayerSlot TurnSlot);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Name Tag")
	void SetNameTagSingleRoundStatus(int32 PlayerLoadedBulletCount, int32 CollectorLoadedBulletCount, EShowDownSide TurnSide, EShowDownPlayerSlot TurnSlot);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Name Tag")
	void SetNameTagPlayerLoadedBulletCount(EShowDownPlayerSlot Slot, int32 LoadedBulletCount);

	void SetLastBetActionNotice(
		EShowDownSide Side,
		EShowDownPlayerSlot PlayerSlot,
		const FString& ActorName,
		EShowDownBetAction Action,
		int32 TargetBet,
		int32 RaiseAmount = 0,
		bool bWasAutomatic = false);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastBetActionNotice(const FShowDownBetActionNotice& Notice);

	int32 SetDecisionTimerState(
		EShowDownDecisionTimerKind Kind,
		float DeadlineServerWorldTimeSeconds,
		float DurationSeconds,
		EShowDownSide TargetSide,
		EShowDownPlayerSlot TargetSlot);

	void ClearDecisionTimerState();

	void SetInitialDealDeckVisualState(FName SourceActorTag, int32 RemainingSteps, int32 TotalSteps);

	//연출 시작 알림
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Presentation", meta = (DisplayName = "eventStart"))
	void EventStart(EShowDownPhase Phase);

	//연출 종료 알림
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Presentation", meta = (DisplayName = "eventEnd"))
	void EventEnd(EShowDownPhase Phase);

	//Phase 연출 시작 알림
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Presentation")
	void StartPresentation(EShowDownPhase Phase);

	//Phase 연출 종료 알림
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Presentation")
	void FinishPresentation(EShowDownPhase Phase);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Presentation")
	void BroadcastCollectorDialogue(const FString& Dialogue, const FString& Intent);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Presentation")
	void BroadcastCollectorLLMDecision(const FString& Dialogue, const FString& Intent, EShowDownBetAction Action, int32 TargetBet);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Presentation")
	void BroadcastCollectorLLMStatus(bool bSuccess, const FString& Message);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Chat")
	void BroadcastChatMessage(const FString& SenderName, const FString& Message);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Presentation")
	void BroadcastCardsRevealed(int32 PlayerCard, int32 CollectorCard);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Presentation")
	void BroadcastRoundResolved(EShowDownRoundResult Result);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Presentation")
	void BroadcastGameOver(EShowDownSide Winner);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Multiplayer|Presentation")
	void BroadcastMultiplayerRouletteStarted(EShowDownPlayerSlot TargetSlot, const FString& TargetName, int32 BulletCount);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Multiplayer|Presentation")
	void BroadcastMultiplayerRoulettePresentation(EShowDownPlayerSlot TargetSlot, const FString& TargetName, int32 BulletCount, bool bHit);

	void BroadcastMultiplayerRoulettePresentationWithContext(
		EShowDownPlayerSlot TargetSlot,
		const FString& TargetName,
		int32 BulletCount,
		bool bHit,
		int32 MatchSequence,
		int32 RoundSequence);

	/** Advances the reliable presentation boundary. Server only. */
	void SetMultiplayerPresentationContext(int32 MatchSequence, int32 RoundSequence);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Multiplayer|Presentation")
	void BroadcastMultiplayerRouletteResult(EShowDownPlayerSlot TargetSlot, const FString& TargetName, int32 BulletCount, bool bHit, int32 RemainingLives);

	/** PlayerSlotMask uses bits 0-3 for Player1-Player4. */
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Presentation")
	void BroadcastTableCinematicCue(ESDTableCinematicCue Cue, uint8 PlayerSlotMask);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastCollectorLLMDecision(const FString& Dialogue, const FString& Intent, EShowDownBetAction Action, int32 TargetBet);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastCollectorLLMStatus(bool bSuccess, const FString& Message);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastChatMessage(const FString& SenderName, const FString& Message);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastCardsRevealed(int32 PlayerCard, int32 CollectorCard);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastRoundResolved(EShowDownRoundResult Result);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastGameOver(EShowDownSide Winner);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastMultiplayerRouletteStarted(EShowDownPlayerSlot TargetSlot, const FString& TargetName, int32 BulletCount);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastMultiplayerRoulettePresentation(
		EShowDownPlayerSlot TargetSlot,
		const FString& TargetName,
		int32 BulletCount,
		bool bHit,
		int32 MatchSequence,
		int32 RoundSequence);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastMultiplayerPresentationContext(int32 MatchSequence, int32 RoundSequence);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPresentationStarted(EShowDownPhase Phase);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPresentationFinished(EShowDownPhase Phase);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastMultiplayerRouletteResult(EShowDownPlayerSlot TargetSlot, const FString& TargetName, int32 BulletCount, bool bHit, int32 RemainingLives);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastTableCinematicCue(ESDTableCinematicCue Cue, uint8 PlayerSlotMask);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_CurrentPhase();

	UFUNCTION()
	void OnRep_CurrentStage();

	UFUNCTION()
	void OnRep_MatchMode();

	UFUNCTION()
	void OnRep_PlayerSlots();

	UFUNCTION()
	void OnRep_NameTagRoundStatus();

	UFUNCTION()
	void OnRep_InitialDealDeckVisualState();

	AStaticMeshActor* ResolveInitialDealDeckVisualActor(FName SourceActorTag);
	void ApplyInitialDealDeckVisualState();

	TWeakObjectPtr<AStaticMeshActor> InitialDealDeckVisualActor;
	FName CachedInitialDealDeckSourceActorTag = NAME_None;
	FVector InitialDealDeckAuthoredLocation = FVector::ZeroVector;
	FVector InitialDealDeckAuthoredScale = FVector::OneVector;
	float InitialDealDeckAuthoredBottomZ = 0.0f;
	bool bInitialDealDeckAuthoredTransformCaptured = false;
	FTimerHandle InitialDealDeckVisualRetryTimerHandle;
	uint8 InitialDealDeckVisualRetryAttempts = 0;
	
};
