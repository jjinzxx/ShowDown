#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "ShowDownTypes.h"
#include "ShowDownGameStateBase.generated.h"

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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FShowDownMultiplayerRouletteResultSignature, EShowDownPlayerSlot, TargetSlot, const FString&, TargetName, int32, BulletCount, bool, bHit, int32, RemainingLives);

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
	FShowDownMultiplayerRouletteResultSignature OnMultiplayerRouletteResult;

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

	UPROPERTY(ReplicatedUsing = OnRep_MatchMode, BlueprintReadOnly, Category = "ShowDown|Multiplayer")
	EShowDownMatchMode MatchMode = EShowDownMatchMode::SinglePlayer;

	UPROPERTY(ReplicatedUsing = OnRep_PlayerSlots, BlueprintReadOnly, Category = "ShowDown|Multiplayer")
	TArray<FShowDownNetworkPlayerSlot> PlayerSlots;

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

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Multiplayer|Presentation")
	void BroadcastMultiplayerRouletteStarted(EShowDownPlayerSlot TargetSlot, const FString& TargetName, int32 BulletCount);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Multiplayer|Presentation")
	void BroadcastMultiplayerRouletteResult(EShowDownPlayerSlot TargetSlot, const FString& TargetName, int32 BulletCount, bool bHit, int32 RemainingLives);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastCollectorLLMDecision(const FString& Dialogue, const FString& Intent, EShowDownBetAction Action, int32 TargetBet);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastCollectorLLMStatus(bool bSuccess, const FString& Message);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastChatMessage(const FString& SenderName, const FString& Message);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastMultiplayerRouletteStarted(EShowDownPlayerSlot TargetSlot, const FString& TargetName, int32 BulletCount);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastMultiplayerRouletteResult(EShowDownPlayerSlot TargetSlot, const FString& TargetName, int32 BulletCount, bool bHit, int32 RemainingLives);

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
	
};
