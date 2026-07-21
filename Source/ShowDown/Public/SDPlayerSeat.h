#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShowDownTypes.h"
#include "SDPlayerSeat.generated.h"

UCLASS(Blueprintable)
class SHOWDOWN_API ASDPlayerSeat : public AActor
{
	GENERATED_BODY()

public:
	ASDPlayerSeat();

	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Seat")
	EShowDownSide SeatSide = EShowDownSide::Player;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ShowDown|Seat")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ShowDown|Seat")
	TObjectPtr<USceneComponent> PlayerHandCard;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ShowDown|Seat")
	TObjectPtr<USceneComponent> PlayerHeadCard;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Seat Setup")
	FVector HandSlotLocation = FVector(0.0f, 0.0f, 80.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Seat Setup")
	FRotator HandSlotRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Seat Setup")
	FVector HeadSlotLocation = FVector(120.0f, 0.0f, 160.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Seat Setup")
	FRotator HeadSlotRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Card Layout")
	bool bOverrideGameModeHandLayout = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Card Layout", meta = (EditCondition = "bOverrideGameModeHandLayout", ClampMin = "0.0"))
	float HandSpacing = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Card Layout", meta = (EditCondition = "bOverrideGameModeHandLayout"))
	float HandForwardOffset = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Card Layout", meta = (EditCondition = "bOverrideGameModeHandLayout", ClampMin = "0.0"))
	float HandHeightOffset = 65.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Card Layout", meta = (EditCondition = "bOverrideGameModeHandLayout", ClampMin = "-45.0", ClampMax = "45.0"))
	float HandLeanAngle = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Card Motion")
	bool bOverrideCardMotion = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Card Motion", meta = (EditCondition = "bOverrideCardMotion"))
	FVector SelectedOffset = FVector(0.0f, 0.0f, 12.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Card Motion", meta = (EditCondition = "bOverrideCardMotion"))
	FVector HoverOffset = FVector(0.0f, 0.0f, 8.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Card Motion", meta = (EditCondition = "bOverrideCardMotion", ClampMin = "0.0"))
	float MoveSpeed = 10.0f;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ShowDown|Seat")
	USceneComponent* GetHandSlot() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ShowDown|Seat")
	USceneComponent* GetHeadSlot() const;

private:
	void ApplySlotTransforms();
};
