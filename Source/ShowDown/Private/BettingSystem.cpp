#include "BettingSystem.h"

UBettingSystem::UBettingSystem()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBettingSystem::ResetBetting(int32 MinimumBet)
{
	CurrentBet = FMath::Clamp(MinimumBet, 0, 6);
}

void UBettingSystem::Check(EShowDownSide Side)
{
	OnBetChanged.Broadcast(Side, CurrentBet);
}

void UBettingSystem::Call(EShowDownSide Side)
{
	OnBetChanged.Broadcast(Side, CurrentBet);
}

bool UBettingSystem::RaiseTo(EShowDownSide Side, int32 BulletCount)
{
	if (BulletCount <= CurrentBet || BulletCount < 1 || BulletCount > 6)
	{
		return false;
	}

	CurrentBet = BulletCount;
	OnBetChanged.Broadcast(Side, CurrentBet);
	return true;
}

void UBettingSystem::Fold(EShowDownSide Side)
{
	OnBetChanged.Broadcast(Side, CurrentBet);
}

int32 UBettingSystem::GetCurrentBet() const
{
	return CurrentBet;
}
