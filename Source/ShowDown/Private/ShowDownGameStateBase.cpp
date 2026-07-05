#include "ShowDownGameStateBase.h"

#include "Engine/Engine.h"
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

	void ShowPresentationDebugMessage(const FString& Prefix, EShowDownPhase Phase, const FColor& Color)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				4.0f,
				Color,
				FString::Printf(TEXT("[%s] %s 연출"), *Prefix, *GetPhaseDebugName(Phase)));
		}
	}

	void ShowRawDebugMessage(const FString& Message, const FColor& Color)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 4.0f, Color, Message);
		}
	}
}

void AShowDownGameStateBase::SetPhase(EShowDownPhase NewPhase)
{
	if (CurrentPhase == NewPhase)
	{
		return;
	}

	const EShowDownPhase PhaseToFinish = CurrentPresentationPhase != EShowDownPhase::None
		? CurrentPresentationPhase
		: CurrentPhase;
	EventEnd(PhaseToFinish);

	CurrentPhase = NewPhase;
	OnPhaseChanged.Broadcast(CurrentPhase);
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

void AShowDownGameStateBase::EventStart(EShowDownPhase Phase)
{
	if (Phase == EShowDownPhase::None)
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
	OnPresentationStarted.Broadcast(Phase);

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
	if (Phase == EShowDownPhase::None || !bPresentationPlaying)
	{
		return;
	}

	const EShowDownPhase FinishedPhase = CurrentPresentationPhase != EShowDownPhase::None
		? CurrentPresentationPhase
		: Phase;

	bPresentationPlaying = false;
	OnPresentationFinished.Broadcast(FinishedPhase);
	if (bShowPresentationDebugMessages)
	{
		ShowPresentationDebugMessage(TEXT("호출종료"), FinishedPhase, FColor::Green);
	}
	CurrentPresentationPhase = EShowDownPhase::None;
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
	OnCollectorLLMStatus.Broadcast(true, TEXT("답변 완료."));
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
	if (HasAuthority())
	{
		MulticastMultiplayerRoulettePresentation(TargetSlot, TargetName, BulletCount, bHit);
		return;
	}

	OnMultiplayerRoulettePresentation.Broadcast(TargetSlot, TargetName, BulletCount, bHit);
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

void AShowDownGameStateBase::MulticastCollectorLLMDecision_Implementation(const FString& Dialogue, const FString& Intent, EShowDownBetAction Action, int32 TargetBet)
{
	OnCollectorDialogue.Broadcast(Dialogue, Intent);
	OnCollectorLLMDecision.Broadcast(Dialogue, Intent, Action, TargetBet);
	OnCollectorLLMStatus.Broadcast(true, TEXT("답변 완료."));
}

void AShowDownGameStateBase::MulticastCollectorLLMStatus_Implementation(bool bSuccess, const FString& Message)
{
	OnCollectorLLMStatus.Broadcast(bSuccess, Message);
}

void AShowDownGameStateBase::MulticastChatMessage_Implementation(const FString& SenderName, const FString& Message)
{
	OnChatMessageReceived.Broadcast(SenderName, Message);
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
	bool bHit)
{
	OnMultiplayerRoulettePresentation.Broadcast(TargetSlot, TargetName, BulletCount, bHit);
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

void AShowDownGameStateBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AShowDownGameStateBase, CurrentPhase);
	DOREPLIFETIME(AShowDownGameStateBase, CurrentPresentationPhase);
	DOREPLIFETIME(AShowDownGameStateBase, bPresentationPlaying);
	DOREPLIFETIME(AShowDownGameStateBase, CurrentStage);
	DOREPLIFETIME(AShowDownGameStateBase, CurrentRound);
	DOREPLIFETIME(AShowDownGameStateBase, MatchMode);
	DOREPLIFETIME(AShowDownGameStateBase, PlayerSlots);
}

void AShowDownGameStateBase::OnRep_CurrentPhase()
{
	OnPhaseChanged.Broadcast(CurrentPhase);
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
