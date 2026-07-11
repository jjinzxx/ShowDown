#include "RouletteSystem.h"

namespace
{
	constexpr int32 RouletteChamberCount = 6;
}

URouletteSystem::URouletteSystem()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool URouletteSystem::RollRoulette(int32 BulletCount) const
{
	const int32 ClampedBulletCount = FMath::Clamp(BulletCount, 0, RouletteChamberCount);
	if (ClampedBulletCount == 0)
	{
		return false;
	}

	if (ClampedBulletCount == RouletteChamberCount)
	{
		return true;
	}

	return FMath::RandRange(1, RouletteChamberCount) <= ClampedBulletCount;
}

float URouletteSystem::GetHitChance(int32 BulletCount) const
{
	return static_cast<float>(FMath::Clamp(BulletCount, 0, RouletteChamberCount))
		/ static_cast<float>(RouletteChamberCount);
}
