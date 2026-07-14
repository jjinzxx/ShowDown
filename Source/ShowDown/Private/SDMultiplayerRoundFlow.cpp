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

float ShowDownMultiplayerRoundFlow::CalculateRevealToRouletteDelay(
	float RevealPresentationSeconds,
	float AdditionalHoldSeconds,
	bool bHasRevealedCards)
{
	const float SafePresentationSeconds = FMath::Max(0.0f, RevealPresentationSeconds);
	return bHasRevealedCards
		? SafePresentationSeconds + FMath::Max(0.0f, AdditionalHoldSeconds)
		: SafePresentationSeconds;
}
