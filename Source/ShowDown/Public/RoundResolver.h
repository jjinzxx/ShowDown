#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShowDownTypes.h"
#include "RoundResolver.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SHOWDOWN_API URoundResolver : public UActorComponent
{
	GENERATED_BODY()

public:
	URoundResolver();

	//공개된 플레이어/콜렉터 카드 숫자를 비교해서 라운드 결과 반환
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ShowDown|Round")
	EShowDownRoundResult ResolveRevealedCards(int32 PlayerCard, int32 CollectorCard) const;

	//폴드 시 룰렛 장전 수 계산. 옵션이 켜져 있고 폴드한 쪽 이마 카드가 7이면 6발
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ShowDown|Round")
	int32 GetFoldLoadCount(int32 FoldedForeheadCard, int32 CurrentBet, bool bSevenFoldLoadsSix) const;
};
