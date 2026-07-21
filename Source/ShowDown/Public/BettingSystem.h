#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShowDownTypes.h"
#include "BettingSystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FShowDownBetChangedSignature, EShowDownSide, Side, int32, BulletCount);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SHOWDOWN_API UBettingSystem : public UActorComponent
{
	GENERATED_BODY()

public:
	UBettingSystem();

private:
	//현재 판의 베팅 총알 수
	UPROPERTY()
	int32 CurrentBet = 0;

public:
	//베팅 값이 바뀌었을 때 UI/연출에 알리는 이벤트
	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Betting")
	FShowDownBetChangedSignature OnBetChanged;

	//베팅 상태 초기화
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "ShowDown|Betting")
	void ResetBetting(int32 MinimumBet = 0);

	//체크 처리
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "ShowDown|Betting")
	void Check(EShowDownSide Side);

	//콜 처리
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "ShowDown|Betting")
	void Call(EShowDownSide Side);

	//지정한 총알 수로 레이즈
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "ShowDown|Betting")
	bool RaiseTo(EShowDownSide Side, int32 BulletCount);

	//폴드 처리
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "ShowDown|Betting")
	void Fold(EShowDownSide Side);

	//현재 베팅 총알 수 반환
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ShowDown|Betting")
	int32 GetCurrentBet() const;
};
