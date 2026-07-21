#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Collector.generated.h"

UCLASS()
class SHOWDOWN_API ACollector : public AActor
{
	GENERATED_BODY()

public:
	ACollector();

	UPROPERTY(VisibleAnywhere, Category = Components)
	TObjectPtr<USceneComponent> rootComp;

	UPROPERTY(VisibleAnywhere, Category = CardSlot)
	TObjectPtr<USceneComponent> c_HandCard;

	UPROPERTY(VisibleAnywhere, Category = CardSlot)
	TObjectPtr<USceneComponent> c_HeadCard;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Debug Spin")
	bool bEnableActionSpin = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Debug Spin")
	TObjectPtr<USceneComponent> ActionSpinTarget = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Debug Spin")
	FName ActionSpinTargetTag = TEXT("ActionSpinTarget");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Debug Spin", meta = (ClampMin = "0.01"))
	float ActionSpinDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Debug Spin", meta = (ClampMin = "0.0"))
	float ActionSpinHoldSeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Debug Spin", meta = (ClampMin = "1", ClampMax = "8"))
	int32 ActionSpinTurns = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Debug Spin")
	FVector ActionSpinAxisLocal = FVector::UpVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Debug Spin", meta = (ClampMin = "0", ClampMax = "32"))
	int32 MaxQueuedActionSpins = 8;

	UFUNCTION(BlueprintCallable, Category = "Presentation|Debug Spin")
	void PlayActionSpin();

	UFUNCTION(BlueprintPure, Category = "Presentation|Debug Spin")
	float GetActionSpinTotalSeconds() const;

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

private:
	USceneComponent* ResolveActionSpinTarget() const;
	void StartActionSpin();

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> ActiveActionSpinTarget = nullptr;

	FQuat ActionSpinStartRotation = FQuat::Identity;
	float ActionSpinElapsed = 0.0f;
	int32 QueuedActionSpinCount = 0;
	bool bActionSpinActive = false;
};
