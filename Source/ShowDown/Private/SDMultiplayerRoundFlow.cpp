#include "SDMultiplayerRoundFlow.h"

ESDMultiplayerPostBetDecision ShowDownMultiplayerRoundFlow::ResolvePostBetDecision(
	int32 ActivePlayerCount,
	bool bAllActivePlayersDoneBetting)
{
	if (ActivePlayerCount <= 1)
	{
		return ESDMultiplayerPostBetDecision::EndRound;
	}

	return bAllActivePlayersDoneBetting
		? ESDMultiplayerPostBetDecision::RevealCards
		: ESDMultiplayerPostBetDecision::ContinueBetting;
}

float ShowDownMultiplayerRoundFlow::CalculateRevealCompletionDelay(
	float RevealPresentationSeconds,
	float MinimumCinematicSeconds)
{
	return FMath::Max(
		FMath::Max(0.0f, RevealPresentationSeconds),
		FMath::Max(0.0f, MinimumCinematicSeconds));
}

int32 ShowDownMultiplayerRoundFlow::GetMultiplayerTurnOrderIndex(EShowDownPlayerSlot Slot)
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

EShowDownPlayerSlot ShowDownMultiplayerRoundFlow::ResolveNextRoundLeaderSlot(
	EShowDownPlayerSlot PreferredLeaderSlot,
	EShowDownPlayerSlot CurrentLeaderSlot,
	const TArray<EShowDownPlayerSlot>& AliveSlots)
{
	TArray<EShowDownPlayerSlot> OrderedAliveSlots;
	for (const EShowDownPlayerSlot Slot : AliveSlots)
	{
		if (GetMultiplayerTurnOrderIndex(Slot) != MAX_int32)
		{
			OrderedAliveSlots.AddUnique(Slot);
		}
	}
	OrderedAliveSlots.Sort([](const EShowDownPlayerSlot Left, const EShowDownPlayerSlot Right)
	{
		return GetMultiplayerTurnOrderIndex(Left) < GetMultiplayerTurnOrderIndex(Right);
	});

	if (OrderedAliveSlots.IsEmpty())
	{
		return EShowDownPlayerSlot::None;
	}

	const EShowDownPlayerSlot AnchorSlot = PreferredLeaderSlot != EShowDownPlayerSlot::None
		? PreferredLeaderSlot
		: CurrentLeaderSlot;
	if (OrderedAliveSlots.Contains(AnchorSlot))
	{
		return AnchorSlot;
	}

	const int32 AnchorOrder = GetMultiplayerTurnOrderIndex(AnchorSlot);
	for (const EShowDownPlayerSlot Slot : OrderedAliveSlots)
	{
		if (GetMultiplayerTurnOrderIndex(Slot) > AnchorOrder)
		{
			return Slot;
		}
	}

	return OrderedAliveSlots[0];
}

ESDMultiplayerRestartDecision ShowDownMultiplayerRoundFlow::ResolveRestartDecision(
	int32 EligiblePlayerCount,
	int32 VoteCount)
{
	if (EligiblePlayerCount < 2)
	{
		return ESDMultiplayerRestartDecision::NotEnoughPlayers;
	}

	return VoteCount >= EligiblePlayerCount
		? ESDMultiplayerRestartDecision::RestartMatch
		: ESDMultiplayerRestartDecision::WaitingForVotes;
}

bool ShowDownMultiplayerRoundFlow::ShouldRejectNewPlayerJoin(
	bool bMatchStarted,
	bool bHostedGameRosterLocked)
{
	return bMatchStarted || bHostedGameRosterLocked;
}
