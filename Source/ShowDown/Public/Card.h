#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/SDInteractable.h"
#include "ShowDownTypes.h"
#include "Card.generated.h"

class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class FLifetimeProperty;

USTRUCT()
struct FSDCardMovementTarget
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY()
	bool bUseSlotAttachMotion = false;

	UPROPERTY()
	uint8 Revision = 0;
};

UCLASS()
class SHOWDOWN_API ACard : public AActor, public ISDInteractable
{
	GENERATED_BODY()

public:
	ACard();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* RootComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* VisualRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* CardMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UTextRenderComponent* CardText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* InteractionBounds;

	UPROPERTY(ReplicatedUsing = OnRep_CardVisual, EditAnywhere, BlueprintReadWrite, Category = "Card", meta = (ClampMin = "1", ClampMax = "7"))
	int32 Rank = 1;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Card")
	bool bSelectable = true;

	UPROPERTY(ReplicatedUsing = OnRep_CardVisual, EditAnywhere, BlueprintReadWrite, Category = "Card")
	bool bFaceUp = false;

	UPROPERTY(ReplicatedUsing = OnRep_CardVisual, BlueprintReadOnly, Category = "Card")
	EShowDownPlayerSlot HiddenFromSlot = EShowDownPlayerSlot::None;

	UPROPERTY(ReplicatedUsing = OnRep_CardVisual, BlueprintReadOnly, Category = "Card")
	EShowDownPlayerSlot HandOwnerSlot = EShowDownPlayerSlot::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Select")
	FVector SelectedOffset = FVector(0.0f, 0.0f, 12.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Hover")
	FVector HoverOffset = FVector(0.0f, 0.0f, 8.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Select")
	float MoveSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Slot Attach Motion")
	bool bUseSlotAttachMotion = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Slot Attach Motion", meta = (ClampMin = "0.05"))
	float SlotAttachDuration = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Slot Attach Motion", meta = (ClampMin = "0.0"))
	float SlotAttachArcHeight = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Slot Attach Motion", meta = (ClampMin = "0.0"))
	float SlotAttachOvershootDistance = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Slot Attach Motion", meta = (ClampMin = "0.1"))
	float SlotAttachTargetScale = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Slot Attach Motion")
	FRotator SlotAttachFlightRotationAmplitude = FRotator(8.0f, 0.0f, 16.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Slot Attach Motion", meta = (ClampMin = "0.0"))
	float SlotAttachSettleDuration = 0.24f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Slot Attach Motion", meta = (ClampMin = "0.0"))
	float SlotAttachSettleLocationAmplitude = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Slot Attach Motion")
	FRotator SlotAttachSettleRotationAmplitude = FRotator(0.0f, 0.0f, 5.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Slot Attach Motion", meta = (ClampMin = "0.0"))
	float SlotAttachSettleOscillations = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Interaction")
	FVector InteractionBoundsExtent = FVector(45.0f, 65.0f, 8.0f);

	UFUNCTION(BlueprintCallable, Category = "Card")
	void SetCard(int32 NewRank);

	UFUNCTION(BlueprintCallable, Category = "Card")
	void SetFaceUp(bool bNewFaceUp);

	UFUNCTION(BlueprintCallable, Category = "Card")
	void SetHiddenFromSlot(EShowDownPlayerSlot NewHiddenFromSlot);

	UFUNCTION(BlueprintCallable, Category = "Card")
	void SetHandOwnerSlot(EShowDownPlayerSlot NewHandOwnerSlot);

	UFUNCTION(BlueprintCallable, Category = "Card")
	void RefreshVisual();

	UFUNCTION(BlueprintCallable, Category = "Card")
	void SelectCard(bool bNewSelected);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Card")
	bool IsSelected() const;

	UFUNCTION(BlueprintCallable, Category = "Card")
	void SetHovered(bool bNewHovered);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Card")
	bool IsHovered() const;

	UFUNCTION(BlueprintCallable, Category = "Card")
	void SetSelectable(bool bNewSelectable);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Card")
	bool IsCardSelectable() const;

	UFUNCTION(BlueprintCallable, Category = "Card")
	void MoveToSlot(USceneComponent* Slot, bool bNewFaceUp);

	UFUNCTION(BlueprintCallable, Category = "Card")
	void MoveToSlotWithRotationOffset(USceneComponent* Slot, bool bNewFaceUp, FRotator RotationOffset);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Card")
	float GetSlotAttachMotionTotalSeconds() const;

	UFUNCTION(BlueprintCallable, Category = "Card")
	void MoveToHandTransform(const FTransform& NewTransform);

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_CardVisual();

	UFUNCTION()
	void OnRep_TargetVisualScaleMultiplier();

	UFUNCTION()
	void OnRep_MovementTarget();

public:
	virtual void Tick(float DeltaTime) override;

private:
	void ConfigureInteractionComponents();
	void UpdateTargetTransform();
	void EnableMotionTick();
	void MoveToSlotTransform(const FTransform& SlotTransform, bool bNewFaceUp);
	void PublishMovementTarget(const FTransform& NewTransform, bool bPlaySlotAttachMotion);
	void ApplyMovementTarget(const FTransform& NewTransform, bool bPlaySlotAttachMotion);
	void ResetTravelMotionState();
	void StartSlotAttachMotion(const FTransform& TargetTransform);
	void UpdateSlotAttachMotion(float DeltaTime);
	void UpdateSlotAttachSettle(float DeltaTime, FVector& InOutVisualWorldOffset, FRotator& OutVisualRelativeRotation);
	void SetTargetVisualScaleMultiplier(float NewTargetScaleMultiplier);
	void StartVisualScaleMotion(float NewTargetScaleMultiplier);
	void UpdateVisualScale(float DeltaTime);
	FRotator ScaleRotator(const FRotator& Rotator, float Scale) const;

	UPROPERTY(VisibleAnywhere, Category = "Card")
	bool bSelected = false;

	UPROPERTY(VisibleAnywhere, Category = "Card")
	bool bHovered = false;

	FVector DefaultLocation = FVector::ZeroVector;
	FVector TargetLocation = FVector::ZeroVector;
	FVector CurrentVisualWorldOffset = FVector::ZeroVector;
	FVector TargetVisualWorldOffset = FVector::ZeroVector;
	FRotator DefaultRotation = FRotator::ZeroRotator;
	FRotator TargetRotation = FRotator::ZeroRotator;
	FVector BaseVisualRootScale = FVector::OneVector;
	FVector SlotAttachStartLocation = FVector::ZeroVector;
	FVector SlotAttachTargetLocation = FVector::ZeroVector;
	FVector SlotAttachTravelDirection = FVector::ForwardVector;
	FQuat SlotAttachStartRotation = FQuat::Identity;
	FQuat SlotAttachTargetRotation = FQuat::Identity;
	float CurrentVisualScaleMultiplier = 1.0f;
	float VisualScaleStartMultiplier = 1.0f;
	float VisualScaleElapsedTime = 0.0f;
	float SlotAttachElapsedTime = 0.0f;
	float SlotAttachSettleElapsedTime = 0.0f;
	UPROPERTY(ReplicatedUsing = OnRep_TargetVisualScaleMultiplier)
	float TargetVisualScaleMultiplier = 1.0f;
	UPROPERTY(ReplicatedUsing = OnRep_MovementTarget)
	FSDCardMovementTarget ReplicatedMovementTarget;
	int32 CachedVisualRank = INDEX_NONE;
	bool bCachedVisualVisible = false;
	bool bHasCachedVisual = false;

	bool bVisualScaleMotionActive = false;
	bool bSlotAttachMotionActive = false;
	bool bSlotAttachSettleActive = false;
};
