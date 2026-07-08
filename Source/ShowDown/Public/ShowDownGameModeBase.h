// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SDCardLayoutTypes.h"
#include "ShowDownTypes.h"
#include "TimerManager.h"
#include "Templates/Function.h"
#include "ShowDownGameModeBase.generated.h"

class ACard;
class APlayerPawn;
class ASDCardPlacementAnchor;
class ASDPlayerSeat;
class ASDMultiplayerSeatAnchor;
class ASDSelfShotGunActor;
class UCardSystem;
class ACollector;
class UCollectorAISystem;
class UBettingSystem;
class URoundResolver;
class URouletteSystem;
struct FCollectorBetDecision;
struct FSDLLMBossContext;
class AShowDownCharacter;
class AShowDownGameStateBase;
class AController;
class APlayerController;
class ASDPlayerState;
class ALevelSequenceActor;
class ASDBetBulletPresentationActor;
class ULevelSequence;
class ULevelSequencePlayer;
class USceneComponent;
enum class ESDCardPlacementRole : uint8;

//각 플레이어(콜렉터, 플레이어, 멀티플레이어) 에 대한 값(손패, 이마의 카드, 목숨, 베팅값) 구조체로 저장
USTRUCT(BlueprintType)
struct FShowDownParticipantState
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<ACard*> HandCards;

	UPROPERTY()
	ACard* ForeheadCard = nullptr;

	UPROPERTY()
	int32 Lives = 3;

	UPROPERTY()
	int32 CurrentBet = 0;
};

//스테이지 구조체
USTRUCT(BlueprintType)
struct FShowDownStageRule
{
	GENERATED_BODY()

	//스테이지 시작 체력
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Stage")
	int32 StartingLives = 3;

	//스테이지 최소 베팅 총알 수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Stage")
	int32 MinimumBet = 1;

	//콜렉터 블러핑 확률
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Stage")
	float CollectorBluffRate = 0.15f;

	//콜렉터 공격성
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Stage")
	float CollectorAggression = 0.5f;

	//자신이 7일 때 폴드하면 6발 장전할지 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Stage")
	bool bSevenFoldLoadsSix = true;
};

UCLASS()
class SHOWDOWN_API AShowDownGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	AShowDownGameModeBase();

	// 덱 관리 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ShowDown|Deck")
	UCardSystem* CardSystem;

	//콜렉터 AI 시스템
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ShowDown|AI")
	UCollectorAISystem* CollectorAISystem;
	
	// 스폰할 카드 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ShowDown|Card")
	TSubclassOf<ACard> CardClass;

	// 플레이어에게 줄 카드 수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Card")
	int32 HandCount = 5;

	// 카드 사이 간격
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Card")
	float CardSpacing = 70.0f;

	// 카드 위치
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Card")
	float ForwardOffset = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Card")
	float HeightOffset = 65.0f;

	// 카드 각도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Card", meta = (ClampMin = "-45.0", ClampMax = "45.0"))
	float LeanAngle = 0.0f;

	// 같은 줄에서 살짝 앞뒤로 겹치는 정도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Card", meta = (ClampMin = "0.0"))
	float LayerStep = 0.5f;
	
	
	
	// 베팅 시스템
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ShowDown|Betting")
	UBettingSystem* BettingSystem;

	// 라운드 판정 시스템
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ShowDown|Round")
	URoundResolver* RoundResolver;

	// 룰렛 판정 시스템
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ShowDown|Roulette")
	URouletteSystem* RouletteSystem;

	//스테이지별 규칙 목록
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Stage")
	TArray<FShowDownStageRule> StageRules;

	//현재 스테이지 인덱스
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ShowDown|Stage")
	int32 CurrentStageIndex = 0;

	// 플레이어가 선택한 카드를 게임모드에 전달
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Card")
	void PlayerSelectedCard(ACard* SelectedCard);

	void PlayerSelectedCardFromController(AController* SubmittingController, ACard* SelectedCard);

	// 베팅 단계 시작
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Betting")
	void StartBettingPhase();

	// 플레이어 체크 또는 콜
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Betting")
	void PlayerCheck();

	// 플레이어 레이즈
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Betting")
	void PlayerRaise();

	// 플레이어가 지정한 최종 총알 수로 레이즈
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Betting")
	void PlayerRaiseTo(int32 BulletCount);

	// 플레이어 폴드
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Betting")
	void PlayerFold();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|LLM")
	void SubmitPlayerDialogueInput(const FString& PlayerDialogue);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Chat")
	void SubmitPlayerDialogueInputFromPlayer(const FString& PlayerDialogue, const FString& SenderName);

	//연출팀이 Phase 연출 종료를 코어에 알릴 때 호출
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Presentation")
	void NotifyPresentationFinished(EShowDownPhase FinishedPhase);

	//연출팀이 Phase 연출 종료를 코어에 알릴 때 호출
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Presentation", meta = (DisplayName = "eventEnd"))
	void EventEnd(EShowDownPhase FinishedPhase);

	// 싱글플레이 한 판을 시작합니다. 콜렉터를 찾고 1스테이지부터 진행합니다.
	// 메인 레벨에서는 싱글플레이 버튼을 눌렀을 때 HubFlowManager가 이 함수를 호출합니다.
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Flow")
	void StartSinglePlayer();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Flow")
	void StartMultiplayerGame();

	void RefreshMultiplayerLobbyPlayers();
	void SetMultiplayerVoiceTalking(AController* RequestingController, bool bIsTalking);

	void RequestMultiplayerRestartFromController(AController* RequestingController);

	// 게임 종료 후 허브(메인메뉴)로 돌아갈 때 게임판을 정리합니다.
	// 진행 중인 타이머/베팅 상태를 끄고 테이블의 카드를 모두 제거합니다.
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Flow")
	void ResetForHubReturn();

	// true면 BeginPlay에서 곧장 게임을 시작합니다(테스트 레벨용 기본값).
	// 단, 레벨에 HubFlowManager가 있으면 자동 시작을 미루고 허브 흐름이 시작을 제어합니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Flow")
	bool bAutoStartOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Camera", meta = (ClampMin = "0.0"))
	float GameplayCameraLookSensitivity = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Camera")
	float GameplayCameraMinPitch = -35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Camera")
	float GameplayCameraMaxPitch = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Camera")
	float GameplayCameraMinYawOffset = -45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Camera")
	float GameplayCameraMaxYawOffset = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Camera")
	bool bInvertGameplayCameraMouseY = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Camera|Breathing")
	bool bEnableGameplayCameraBreathingSway = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Camera|Breathing", meta = (ClampMin = "0.0"))
	float GameplayCameraBreathingSwaySpeed = 0.38f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Camera|Breathing")
	FRotator GameplayCameraBreathingSwayRotationAmplitude = FRotator(0.12f, 0.05f, 0.08f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Camera|Breathing")
	FVector GameplayCameraBreathingSwayLocationAmplitude = FVector(0.0f, 0.0f, 0.8f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Camera|Breathing", meta = (ClampMin = "0.0"))
	float GameplayCameraBreathingSwayBlendInTime = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Presentation")
	bool bAutoAdvanceRevealWithoutPresentation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Presentation", meta = (ClampMin = "0.0"))
	float RevealAutoAdvanceSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Presentation|Bet Bullets")
	bool bUseBetBulletPresentation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Presentation|Bet Bullets")
	TSubclassOf<ASDBetBulletPresentationActor> BetBulletPresentationClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Presentation|Bet Bullets", meta = (ClampMin = "0.0"))
	float BetBulletLaneDistanceFromCenter = 168.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Presentation|Bet Bullets")
	float BetBulletLaneHeightOffset = 58.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Presentation|Card Reveal")
	bool bUseCardRevealPresentation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Presentation|Card Reveal", meta = (ClampMin = "0.0"))
	float CardRevealLeadInSeconds = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Presentation|Card Reveal", meta = (ClampMin = "0.0"))
	float CardRevealStepSeconds = 0.24f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Presentation|Card Reveal", meta = (ClampMin = "0.0"))
	float CardRevealHoldSeconds = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Presentation|Card Reveal", meta = (ClampMin = "0.0"))
	float CardRevealForwardDistance = 42.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Presentation|Card Reveal")
	float CardRevealHeightOffset = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Presentation|Card Reveal", meta = (ClampMin = "0.0"))
	float CardRevealSideSpacing = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Presentation|Card Reveal", meta = (ClampMin = "0.1"))
	float CardRevealVisualScale = 1.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Presentation|Card Reveal")
	FRotator CardRevealRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Presentation|Single Player Intro")
	bool bPlaySinglePlayerIntro = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Presentation|Single Player Intro", meta = (EditCondition = "bPlaySinglePlayerIntro"))
	TObjectPtr<ULevelSequence> SinglePlayerIntroSequence = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Presentation|Single Player Intro", meta = (EditCondition = "bPlaySinglePlayerIntro"))
	FName SinglePlayerIntroPlayerBindingTag = TEXT("Player");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Presentation|Single Player Intro", meta = (EditCondition = "bPlaySinglePlayerIntro"))
	FName SinglePlayerIntroCollectorBindingTag = TEXT("Collector");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Presentation|Single Player Intro", meta = (EditCondition = "bPlaySinglePlayerIntro"))
	bool bUseSinglePlayerIntroFallbackWhenNoSequence = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Presentation|Single Player Intro", meta = (EditCondition = "bPlaySinglePlayerIntro && bUseSinglePlayerIntroFallbackWhenNoSequence", ClampMin = "0.0"))
	float SinglePlayerIntroFallbackDuration = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Presentation|Single Player Intro", meta = (EditCondition = "bPlaySinglePlayerIntro && bUseSinglePlayerIntroFallbackWhenNoSequence", ClampMin = "0.0"))
	float SinglePlayerIntroFallbackStartDistance = 280.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Debug")
	bool bShowGameFlowDebugMessages = true;

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Betting")
	void RequestPlayerBetAction(EShowDownBetAction Action, int32 TargetBet);

	void RequestPlayerBetActionFromController(AController* SubmittingController, EShowDownBetAction Action, int32 TargetBet);


protected:
	virtual void BeginPlay() override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

private:
	//플레이어 스탯
	UPROPERTY()
	FShowDownParticipantState PlayerState;

	//콜렉터 스탯
	UPROPERTY()
	FShowDownParticipantState CollectorState;
	
	bool bBettingPhase = false;
	FTimerHandle RevealDelayHandle;
	FTimerHandle MultiplayerRevealContinuationTimerHandle;
	TArray<FTimerHandle> CardRevealPresentationTimerHandles;
	int32 BettingRaisesLeft = 6;
	bool bHasLastRaiser = false;
	EShowDownSide LastRaiser = EShowDownSide::Player;
	EShowDownSide CurrentRoundFirstSide = EShowDownSide::Player;
	EShowDownSide NextRoundFirstSide = EShowDownSide::Player;
	bool bPlayerHasActedInBetting = false;
	bool bCollectorHasActedInBetting = false;
	bool bCollectorBetDecisionInProgress = false;
	bool bHasPendingRoundReveal = false;
	bool bHasPendingFoldReveal = false;
	bool bCollectorActionPresentationInProgress = false;
	EShowDownRoundResult PendingRoundResult = EShowDownRoundResult::Draw;
	EShowDownSide PendingFoldedSide = EShowDownSide::Player;
	int32 PendingFoldLoadCount = 1;
	FTimerHandle CardPlacementDelayHandle;
	FTimerHandle CollectorActionPresentationTimerHandle;
	TFunction<void()> CardPlacementDelayContinuation;
	TFunction<void()> CollectorActionPresentationContinuation;
	TFunction<void()> SelfShotGunPresentationContinuation;
	TArray<TFunction<void()>> QueuedCollectorActionPresentationContinuations;
	FString LatestPlayerDialogueInput;
	FString RecentDialogueHistory;
	FString RecentRoundHistory;
	FString CurrentRoundActionHistory;
	FString DiscardedCardsSummary;
	FString PendingBossChatReplyDialogue;
	int32 CurrentRoundPlayerGaveRank = 0;
	int32 CurrentRoundCollectorGaveRank = 0;
	int32 LastRoundPlayerCardRank = 0;
	int32 LastRoundCollectorCardRank = 0;
	bool bCurrentRoundSummaryRecorded = false;
	bool bBossChatReplyInFlight = false;
	bool bHasPendingBossChatReply = false;
	float LastBossChatReplyRequestTime = -1000.0f;
	FTimerHandle BossChatReplyRetryTimerHandle;

	UPROPERTY()
	TArray<TObjectPtr<ASDPlayerState>> MultiplayerPlayers;

	UPROPERTY()
	TArray<TObjectPtr<ASDPlayerState>> MultiplayerEliminationOrder;

	UPROPERTY()
	TObjectPtr<ASDPlayerState> MultiplayerDuelA = nullptr;

	UPROPERTY()
	TObjectPtr<ASDPlayerState> MultiplayerDuelB = nullptr;

	UPROPERTY()
	TObjectPtr<ASDPlayerState> MultiplayerCurrentBetter = nullptr;

	UPROPERTY()
	TObjectPtr<ASDPlayerState> MultiplayerLastCheckedPlayer = nullptr;

	// The player who lost the previous round leads the next multiplayer round.
	// A draw keeps the current lead; an eliminated loser falls back to the next survivor.
	UPROPERTY()
	TObjectPtr<ASDPlayerState> MultiplayerNextFirstPlayer = nullptr;

	UPROPERTY()
	TArray<TObjectPtr<ASDMultiplayerSeatAnchor>> MultiplayerSeatAnchors;

	UPROPERTY()
	TObjectPtr<ASDPlayerState> MultiplayerRoundLeader = nullptr;

	UPROPERTY()
	TSet<TObjectPtr<ASDPlayerState>> MultiplayerFoldedPlayers;

	UPROPERTY()
	TSet<TObjectPtr<ASDPlayerState>> MultiplayerPlayersActed;

	UPROPERTY()
	TSet<TObjectPtr<ASDPlayerState>> MultiplayerRestartVotes;

	FTimerHandle MultiplayerStartTimerHandle;
	bool bMultiplayerMatchStarted = false;
	bool bMultiplayerRoundResolving = false;
	
	//콜렉터 추적
	UPROPERTY()
	ACollector* Collector = nullptr;

	UPROPERTY()
	TObjectPtr<ASDSelfShotGunActor> ActiveSelfShotGunActor = nullptr;
	UPROPERTY()
	TObjectPtr<ASDBetBulletPresentationActor> BetBulletPresentationActor = nullptr;

	bool bSelfShotGunPresentationInProgress = false;
	bool bPendingSelfShotRouletteResult = false;
	bool bPendingSelfShotLiveRound = false;
	EShowDownSide PendingSelfShotTargetSide = EShowDownSide::Player;
	int32 BetBulletPresentationRevision = 0;
	bool bHasBetBulletAction = false;
	bool bBetBulletActionIsMultiplayer = false;
	EShowDownSide BetBulletActionSide = EShowDownSide::Player;
	EShowDownPlayerSlot BetBulletActionSlot = EShowDownPlayerSlot::None;
	EShowDownBetAction BetBulletAction = EShowDownBetAction::Check;
	int32 BetBulletActionPreviousBet = 0;
	int32 BetBulletActionTargetBet = 0;
	bool bHasBetBulletRouletteTarget = false;
	bool bBetBulletRouletteTargetIsMultiplayer = false;
	EShowDownSide BetBulletRouletteTargetSide = EShowDownSide::Player;
	EShowDownPlayerSlot BetBulletRouletteTargetSlot = EShowDownPlayerSlot::None;
	int32 BetBulletRouletteBulletCount = 0;

	UPROPERTY()
	TObjectPtr<ULevelSequencePlayer> ActiveSinglePlayerIntroSequencePlayer = nullptr;

	UPROPERTY()
	TObjectPtr<ALevelSequenceActor> ActiveSinglePlayerIntroSequenceActor = nullptr;

	UPROPERTY()
	TObjectPtr<AShowDownCharacter> SinglePlayerIntroPlayerCharacter = nullptr;

	UPROPERTY()
	TObjectPtr<AShowDownCharacter> SinglePlayerIntroCollectorCharacter = nullptr;

	FTimerHandle SinglePlayerIntroFallbackTimerHandle;
	FTransform SinglePlayerIntroPlayerStartTransform;
	FTransform SinglePlayerIntroPlayerTargetTransform;
	FTransform SinglePlayerIntroCollectorStartTransform;
	FTransform SinglePlayerIntroCollectorTargetTransform;
	float SinglePlayerIntroFallbackStartTime = 0.0f;
	bool bSinglePlayerIntroFallbackActive = false;

	mutable bool bCardPlacementAnchorCacheInitialized = false;
	mutable TMap<ESDCardPlacementRole, TWeakObjectPtr<ASDCardPlacementAnchor>> CachedCardPlacementAnchors;
	mutable bool bPlayerSeatCacheInitialized = false;
	mutable TMap<EShowDownSide, TWeakObjectPtr<ASDPlayerSeat>> CachedPlayerSeats;
	mutable TWeakObjectPtr<ASDPlayerSeat> CachedFirstPlayerSeat;

	UFUNCTION()
	void HandleSelfShotGunPresentationFinished();

	UFUNCTION()
	void HandleSelfShotGunShotResolved();

	UFUNCTION()
	void HandleSinglePlayerIntroSequenceFinished();

	// 덱을 만들고 섞은 뒤에 플레이어와 콜렉터에게 5장 스폰
	void DealInitialHand();
	void FindCollector();
	void CollectorGiveCardToPlayer();
	float EstimateCollectorWinChance() const;
	void ResolveCollectorBetResponse();
	FCollectorBetDecision SanitizeCollectorDecision(const FCollectorBetDecision& RawDecision) const;
	void ExecuteCollectorBetDecision(const FCollectorBetDecision& CollectorDecision, int32 GivenCardRank);
	FSDLLMBossContext BuildLLMBossContext(int32 CurrentBet, int32 GivenCardRank) const;
	FSDLLMBossContext BuildLLMChatContext(const FString& PlayerDialogue) const;
	void TryRequestBossChatReply(const FString& PlayerDialogue, bool bIgnoreCooldown = false);
	void RequestPendingBossChatReply();
	void AppendRecentDialogueLine(const FString& Speaker, const FString& Message);
	void ResetCurrentRoundMemory();
	void RecordCurrentRoundAction(const FString& ActionText);
	void AppendRecentRoundSummary(EShowDownRoundResult Result, const FString& Reason);
	FString GetSideText(EShowDownSide Side) const;
	FString GetSideDisplayText(EShowDownSide Side) const;
	FString GetRoundResultText(EShowDownRoundResult Result) const;
	void BeginCardSelectionRound();
	void SetNextRoundFirstSideFromResult(EShowDownRoundResult Result);
	void FinishBettingAndResolveRound();
	void BroadcastBossResultReaction(EShowDownRoundResult Result);
	void ResolveFold(EShowDownSide FoldedSide);
	void ContinueRoundAfterReveal(EShowDownRoundResult Result);
	void ContinueFoldAfterReveal(EShowDownSide FoldedSide, int32 LoadCount);
	void ClearBetBulletPresentation();
	void RefreshBetBulletPresentation(const FString& StatusText);
	ASDBetBulletPresentationActor* EnsureBetBulletPresentationActor();
	FTransform BuildBetBulletLaneTransformForSide(EShowDownSide Side) const;
	FTransform BuildBetBulletLaneTransformForPlayer(const ASDPlayerState* Player) const;
	FTransform BuildBetBulletLaneTransformFromLocation(const FVector& SourceLocation, int32 FallbackOrderIndex) const;
	FString BuildSingleBetBulletStatusText(EShowDownSide TurnSide) const;
	FString BuildMultiplayerBetBulletStatusText(const ASDPlayerState* TurnPlayer) const;
	void RecordSingleBetBulletAction(EShowDownSide Side, EShowDownBetAction Action, int32 PreviousBet, int32 TargetBet);
	void RecordMultiplayerBetBulletAction(ASDPlayerState* Player, EShowDownBetAction Action, int32 PreviousBet, int32 TargetBet);
	void MarkSingleBetBulletRouletteTarget(EShowDownSide TargetSide, int32 BulletCount);
	void MarkMultiplayerBetBulletRouletteTarget(ASDPlayerState* TargetPlayer, int32 BulletCount);
	void ClearBetBulletTransientState();
	FString BuildBetBulletActionText(EShowDownBetAction Action, int32 PreviousBet, int32 TargetBet) const;
	float PlaySinglePlayerCardRevealPresentation();
	float PlayMultiplayerCardRevealPresentation(const TArray<ASDPlayerState*>& RevealedPlayers);
	float PlayCardRevealPresentation(const TArray<ACard*>& Cards, const FVector& FocusLocation);
	void ClearCardRevealPresentationTimers();
	FTransform BuildCardRevealPresentationTransform(ACard* Card, const FVector& FocusLocation, int32 CardIndex, int32 CardCount) const;
	FVector ResolveCardRevealFocusLocationForSingle() const;
	FVector ResolveCardRevealFocusLocationForMultiplayer(const TArray<ASDPlayerState*>& RevealedPlayers) const;
	void ApplyRouletteResult(EShowDownSide TargetSide, int32 BulletCount, TFunction<void()>&& Continuation);
	void EndRound();
	void ClearForeheadCards();
	void ClearHandCards();
	void SetPlayerHandSelectable(bool bSelectable);
	void WaitForCardPlacementThen(ACard* Card, TFunction<void()>&& Continuation);
	void FinishCardPlacementWait();
	void PlayCollectorActionPresentation();
	void PlayCollectorActionPresentationThen(TFunction<void()>&& Continuation);
	void FinishCollectorActionPresentation();
	void BroadcastCardSelectedAction(EShowDownSide Side) const;
	void BroadcastBetActionCommitted(EShowDownSide Side, EShowDownBetAction Action, int32 TargetBet) const;
	void BroadcastMultiplayerCardSelectedAction(ASDPlayerState* Player) const;
	void BroadcastMultiplayerBetActionCommitted(ASDPlayerState* Player, EShowDownBetAction Action, int32 TargetBet) const;
	void BroadcastSystemChatMessage(const FString& Message) const;
	void PlaySelfShotGunPresentationThen(EShowDownSide TargetSide, bool bLiveRound, TFunction<void()>&& Continuation);
	void FinishSelfShotGunPresentation();
	void BroadcastPendingSelfShotRouletteResult();
	ASDSelfShotGunActor* FindSelfShotGunActor() const;
	AShowDownCharacter* FindSingleRouletteCharacter(EShowDownSide TargetSide) const;
	void PlaySinglePlayerIntroThenStartStage();
	bool PlaySinglePlayerIntroSequence(AShowDownCharacter* PlayerCharacter, AShowDownCharacter* CollectorCharacter);
	bool PlaySinglePlayerIntroFallback(AShowDownCharacter* PlayerCharacter, AShowDownCharacter* CollectorCharacter);
	void UpdateSinglePlayerIntroFallback();
	void FinishSinglePlayerIntro();
	TArray<AShowDownCharacter*> GetShowDownCharacters() const;
	void ConfigureSinglePlayerCharacters();
	void ConfigureMultiplayerCharacters(const TArray<ASDPlayerState*>& Players);
	void RefreshMultiplayerCharacterVisibility();
	float ResolveMultiplayerRouletteResultDelay() const;
	float ResolveMultiplayerRoulettePresentationDelay(bool bLiveRound) const;
	FSDCardHandLayoutSettings GetDefaultHandLayoutSettings() const;
	FSDCardHandLayoutSettings ResolveHandLayoutSettings(EShowDownSide Side) const;
	void ApplyCardMotionForSide(EShowDownSide Side, const TArray<ACard*>& Cards) const;
	void ReflowHandCards(EShowDownSide Side);
	void RefreshNetworkPlayerSlots();
	EShowDownPlayerSlot FindNextOpenPlayerSlot() const;
	void TryStartMultiplayerMatch();
	void StartMultiplayerMatch(const TArray<ASDPlayerState*>& Players);
	TArray<ASDPlayerState*> GetConnectedShowDownPlayers() const;
	ASDPlayerState* GetPlayerStateForController(AController* Controller) const;
	void EnsureMultiplayerSeatAnchors();
	void EnsureMultiplayerPawns();
	FTransform GetMultiplayerPawnSpawnTransform(AController* Controller, int32 PlayerIndex);
	ASDPlayerState* FindNextAliveMultiplayerPlayer(ASDPlayerState* AfterPlayer) const;
	ASDPlayerState* GetMultiplayerOpponent(ASDPlayerState* Player) const;
	void DealMultiplayerHands();
	void ClearMultiplayerHands();
	void ClearMultiplayerForeheadCards();
	void ClearLooseMultiplayerCards();
	void StartMultiplayerDuel(ASDPlayerState* FirstPlayer, ASDPlayerState* SecondPlayer);
	bool AreAllAliveMultiplayerPlayersReadyToReveal() const;
	bool AreAllActiveMultiplayerPlayersDoneBetting(int32 CurrentBet) const;
	int32 CountActiveMultiplayerPlayers() const;
	void HandleMultiplayerPlayerDisconnected(ASDPlayerState* LeavingPlayer);
	void StartMultiplayerCardSelection();
	void HandleMultiplayerSelectedCard(ASDPlayerState* SubmittingPlayer, ACard* SelectedCard);
	void StartMultiplayerBetting();
	void HandleMultiplayerBetAction(ASDPlayerState* SubmittingPlayer, EShowDownBetAction Action, int32 TargetBet);
	void FinishMultiplayerRoundByReveal();
	void ContinueMultiplayerRoundAfterReveal(TArray<ASDPlayerState*> RevealedPlayers, TArray<ASDPlayerState*> Winners);
	void FinishMultiplayerRoundByFold(ASDPlayerState* FoldedPlayer);
	void ContinueMultiplayerRoundAfterFoldReveal(ASDPlayerState* FoldedPlayer, int32 LoadCount);
	float ApplyMultiplayerRoulette(ASDPlayerState* TargetPlayer, int32 BulletCount, float StartDelay = 0.0f);
	void EndMultiplayerRound();
	void ShowMultiplayerFinalRanking(ASDPlayerState* Winner);
	void SetMultiplayerSelectableHand(ASDPlayerState* Player);
	void SetMultiplayerAliveHandsSelectable(bool bSelectable);
	void ReflowMultiplayerHand(ASDPlayerState* Player);
	USceneComponent* GetHandSlotForPlayerState(ASDPlayerState* Player) const;
	USceneComponent* GetHeadSlotForPlayerState(ASDPlayerState* Player) const;
	FRotator GetForeheadCardRotationOffsetForPlayerState(const ASDPlayerState* Player, const USceneComponent* HeadSlot) const;
	FRotator GetForeheadCardRotationOffsetForSide(EShowDownSide Side, const USceneComponent* HeadSlot) const;
	void NotifyMultiplayerStatus(const FString& Message) const;
	void StartStage(int32 StageIndex);
	void AdvanceStage();
	const FShowDownStageRule* GetCurrentStageRule() const;
	AShowDownGameStateBase* GetShowDownGameState() const;
	APlayerPawn* GetPrimaryPlayerPawn() const;
	void RefreshCardPlacementAnchorCache() const;
	void RefreshPlayerSeatCache() const;
	ASDCardPlacementAnchor* GetCardPlacementAnchorByRole(ESDCardPlacementRole TargetRole) const;
	ASDCardPlacementAnchor* GetCardPlacementAnchor(EShowDownSide Side, bool bForeheadSlot) const;
	ASDCardPlacementAnchor* GetHandAnchorForSide(EShowDownSide Side) const;
	ASDCardPlacementAnchor* GetForeheadAnchorForSide(EShowDownSide Side) const;
	ASDCardPlacementAnchor* GetHandAnchorForPlayerSlot(EShowDownPlayerSlot Slot) const;
	ASDCardPlacementAnchor* GetForeheadAnchorForPlayerSlot(EShowDownPlayerSlot Slot) const;
	ASDPlayerSeat* GetSeatForSide(EShowDownSide Side) const;
	ASDPlayerSeat* GetPrimaryPlayerSeat() const;
	USceneComponent* GetHandSlotForSide(EShowDownSide Side) const;
	USceneComponent* GetHeadSlotForSide(EShowDownSide Side) const;
	USceneComponent* GetPlayerHandSlot() const;
	USceneComponent* GetPlayerHeadSlot() const;
	FSDCardHandLayoutSettings ResolveHandLayoutSettingsForPlayerState(ASDPlayerState* Player) const;
	void ApplyCardMotionForPlayerState(ASDPlayerState* Player, const TArray<ACard*>& Cards) const;
	void ScheduleRevealAutoAdvanceIfNeeded(float MinimumDelaySeconds = 0.0f);
	void ShowEventDebugMessage(const FString& Message) const;
	
};
