#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RouletteSystem.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SHOWDOWN_API URouletteSystem : public UActorComponent
{
	GENERATED_BODY()

public:
	URouletteSystem();

	//총알 수를 기준으로 룰렛 명중 여부를 판정
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "ShowDown|Roulette")
	bool RollRoulette(int32 BulletCount) const;

	//총알 수를 0~1 사이 명중 확률로 반환
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ShowDown|Roulette")
	float GetHitChance(int32 BulletCount) const;
};
