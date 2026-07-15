#pragma once

#include "CoreMinimal.h"
#include "ShowDownTypes.h"

enum class ESDMultiplayerPostBetDecision : uint8
{
	ContinueBetting,
	RevealCards,
	EndRound
};

enum class ESDMultiplayerRestartDecision : uint8
{
	NotEnoughPlayers,
	WaitingForVotes,
	RestartMatch
};

namespace ShowDownMultiplayerRoundFlow
{
	/** Resolves the authoritative transition after a committed multiplayer action. */
	SHOWDOWN_API ESDMultiplayerPostBetDecision ResolvePostBetDecision(
		int32 ActivePlayerCount,
		bool bAllActivePlayersDoneBetting);

	/** Uses the complete card presentation when available, otherwise falls back to a fixed cinematic beat. */
	SHOWDOWN_API float ResolveRevealCompletionDelay(
		float RevealPresentationSeconds,
		float FallbackCinematicSeconds);

	/** Shared table order used by selection, betting, and next-round leadership. */
	SHOWDOWN_API int32 GetMultiplayerTurnOrderIndex(EShowDownPlayerSlot Slot);

	/** Keeps a living preferred leader, otherwise advances from its seat anchor. */
	SHOWDOWN_API EShowDownPlayerSlot ResolveNextRoundLeaderSlot(
		EShowDownPlayerSlot PreferredLeaderSlot,
		EShowDownPlayerSlot CurrentLeaderSlot,
		const TArray<EShowDownPlayerSlot>& AliveSlots);

	/** Requires at least two eligible players and one vote from every player. */
	SHOWDOWN_API ESDMultiplayerRestartDecision ResolveRestartDecision(
		int32 EligiblePlayerCount,
		int32 VoteCount);

	/** A new controller may not join once either transition or gameplay is locked. */
	SHOWDOWN_API bool ShouldRejectNewPlayerJoin(
		bool bMatchStarted,
		bool bHostedGameRosterLocked);
}
