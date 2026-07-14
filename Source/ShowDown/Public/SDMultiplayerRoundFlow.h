#pragma once

#include "CoreMinimal.h"

enum class ESDMultiplayerPostBetDecision : uint8
{
	ContinueBetting,
	RevealCards,
	EndRound
};

namespace ShowDownMultiplayerRoundFlow
{
	/** Resolves the authoritative transition after a committed multiplayer action. */
	SHOWDOWN_API ESDMultiplayerPostBetDecision ResolvePostBetDecision(
		int32 ActivePlayerCount,
		bool bAllActivePlayersDoneBetting);

	/** Adds the configured settled-card hold only when at least one card was revealed. */
	SHOWDOWN_API float CalculateRevealToRouletteDelay(
		float RevealPresentationSeconds,
		float AdditionalHoldSeconds,
		bool bHasRevealedCards);
}
