#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShowDownTypes.h"
#include "CollectorAISystem.generated.h"

class ACard;

UENUM(BlueprintType)
enum class ECollectorGiveStrategy : uint8
{
	Lowest,
	Highest,
	Random,
	Weighted,
	Mixed
};

USTRUCT(BlueprintType)
struct FCollectorAISettings
{
	GENERATED_BODY()

	//카드 제공 전략
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|CollectorAI")
	ECollectorGiveStrategy GiveStrategy = ECollectorGiveStrategy::Mixed;

	//낮은 카드 선택 가중치
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|CollectorAI")
	float LowBias = 0.7f;

	//불리해도 레이즈할 확률
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|CollectorAI")
	float BluffRate = 0.15f;

	//유리할 때 레이즈 크기를 키우는 정도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|CollectorAI")
	float Aggression = 0.5f;

	//콜보다 폴드를 선호하는 정도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|CollectorAI")
	float Timidity = 0.0f;

	//판단 흔들림 정도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|CollectorAI")
	float Noise = 0.05f;
};

USTRUCT(BlueprintType)
struct FCollectorBetDecision
{
	GENERATED_BODY()

	//콜렉터가 선택한 베팅 행동
	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|CollectorAI")
	EShowDownBetAction Action = EShowDownBetAction::Call;

	//레이즈일 때 목표 최종 장전 수
	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|CollectorAI")
	int32 TargetBet = 0;
};

USTRUCT(BlueprintType)
struct FCollectorLLMInfluence
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "ShowDown|CollectorAI")
	float AggressionDelta = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "ShowDown|CollectorAI")
	float BluffRateDelta = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "ShowDown|CollectorAI")
	float TimidityDelta = 0.0f;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))



class SHOWDOWN_API UCollectorAISystem : public UActorComponent
{
	GENERATED_BODY()

public:
	UCollectorAISystem();

	//콜렉터 AI 파라미터
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|CollectorAI")
	FCollectorAISettings Settings;

	//숫자 손패에서 콜렉터가 줄 카드 숫자 선택
	UFUNCTION(BlueprintCallable, Category = "ShowDown|CollectorAI")
	int32 ChooseCardToGive(const TArray<int32>& HandRanks) const;

	//실제 카드 액터 손패에서 콜렉터가 줄 카드 선택
	UFUNCTION(BlueprintCallable, Category = "ShowDown|CollectorAI")
	ACard* ChooseCardActorToGive(const TArray<ACard*>& HandCards) const;

	//추정 승률 기준으로 콜렉터 베팅 행동 선택
	UFUNCTION(BlueprintCallable, Category = "ShowDown|CollectorAI")
	EShowDownBetAction ChooseBetAction(float EstimatedWinChance, int32 CurrentBet) const;

	//콜렉터가 플레이어에게 준 카드 숫자를 기준으로 베팅 행동 선택
	UFUNCTION(BlueprintCallable, Category = "ShowDown|CollectorAI")
	EShowDownBetAction ChooseBetActionByGivenCard(int32 GivenCardRank, int32 CurrentBet) const;

	//HTML 밸런스 테스트 모델 기반 베팅 결정
	UFUNCTION(BlueprintCallable, Category = "ShowDown|CollectorAI")
	FCollectorBetDecision ChooseBetDecisionByModel(
		const TArray<ACard*>& OwnHandCards,
		int32 OpponentForeheadCard,
		int32 CurrentBet,
		int32 OwnCommittedBet,
		int32 MaxRaise,
		int32 RaisesLeft,
		bool bLastRaiserWasCollector) const;

	FCollectorBetDecision ChooseBetDecisionByModelWithInfluence(
		const TArray<ACard*>& OwnHandCards,
		int32 OpponentForeheadCard,
		int32 CurrentBet,
		int32 OwnCommittedBet,
		int32 MaxRaise,
		int32 RaisesLeft,
		bool bLastRaiserWasCollector,
		const FCollectorLLMInfluence& LLMInfluence) const;
};
