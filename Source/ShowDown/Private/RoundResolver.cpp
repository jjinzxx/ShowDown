#include "RoundResolver.h"

URoundResolver::URoundResolver()
{
	PrimaryComponentTick.bCanEverTick = false;
}

EShowDownRoundResult URoundResolver::ResolveRevealedCards(int32 PlayerCard, int32 CollectorCard) const
{
	if (PlayerCard > CollectorCard)
	{
		return EShowDownRoundResult::PlayerWin;
	}

	if (CollectorCard > PlayerCard)
	{
		return EShowDownRoundResult::CollectorWin;
	}

	return EShowDownRoundResult::Draw;
}

int32 URoundResolver::GetFoldLoadCount(int32 FoldedForeheadCard, int32 CurrentBet, bool bSevenFoldLoadsSix) const
{
	return bSevenFoldLoadsSix && FoldedForeheadCard == 7 ? 6 : FMath::Clamp(CurrentBet, 0, 6);
}
