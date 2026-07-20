#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/SDInteractable.h"
#include "ShowDownTypes.h"
#include "TimerManager.h"
#include "Card.generated.h"

class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class FLifetimeProperty;
class ASDPlayerState;

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
	float MotionDuration = 0.85f;

	UPROPERTY()
	float ArcHeight = 55.0f;

	UPROPERTY()
	float OvershootDistance = 8.0f;

	UPROPERTY()
	bool bUseSettleMotion = true;

	UPROPERTY()
	float SettleStrength = 1.0f;

	UPROPERTY()
	bool bOrientToLocalViewer = false;

	UPROPERTY()
	float VisualScaleMultiplier = 1.0f;

	UPROPERTY()
	float ServerStartTime = -1.0f;

	UPROPERTY()
	bool bRevealAtMotionStart = false;

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

	// Server-authoritative secret. Never replicate this value on the globally
	// relevant card actor. On clients it is only a compatibility mirror of the
	// currently authorized display rank (or zero while concealed), so existing
	// BP_Card reads cannot recover a hidden server value.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card", meta = (ClampMin = "1", ClampMax = "7"))
	int32 Rank = 1;

	// Zero while concealed. Set only once a card is legitimately public.
	UPROPERTY(ReplicatedUsing = OnRep_CardVisual, BlueprintReadOnly, Category = "Card")
	int32 RevealedRank = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Selectable, EditAnywhere, BlueprintReadWrite, Category = "Card")
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

	// Card reveals use the same flight shape as slot attachment, but complete the
	// travel independently so forehead placement timing remains unchanged.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Reveal Motion", meta = (ClampMin = "0.05"))
	float RevealMotionDurationMultiplier = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Slot Attach Motion", meta = (ClampMin = "0.0"))
	float SlotAttachArcHeight = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Slot Attach Motion", meta = (ClampMin = "0.0"))
	float SlotAttachOvershootDistance = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Slot Attach Motion", meta = (ClampMin = "0.1"))
	float SlotAttachTargetScale = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Slot Attach Motion", meta = (ClampMin = "0.1"))
	float ForeheadSlotAttachTargetScale = 1.0f;

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
	void RevealRankToAll();

	UFUNCTION(BlueprintCallable, Category = "Card")
	void ConcealRankFromAll();

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

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Card")
	bool IsCardSelectableForSlot(EShowDownPlayerSlot PlayerSlot) const;

	UFUNCTION(BlueprintCallable, Category = "Card")
	void MoveToSlot(USceneComponent* Slot, bool bNewFaceUp);

	UFUNCTION(BlueprintCallable, Category = "Card")
	void MoveToSlotWithRotationOffset(USceneComponent* Slot, bool bNewFaceUp, FRotator RotationOffset);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Card")
	float GetSlotAttachMotionTotalSeconds() const;

	UFUNCTION(BlueprintCallable, Category = "Card")
	void MoveToRevealTransform(const FTransform& RevealTransform, float VisualScaleMultiplier);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Card")
	float GetRevealMotionTotalSeconds() const;

	UFUNCTION(BlueprintCallable, Category = "Card")
	void MoveToHandTransform(const FTransform& NewTransform);

	// Moves the physical card without changing which side is logically visible.
	// Opening/deal presentations use this so the authored front and back are
	// revealed only by the card's world rotation.
	UFUNCTION(BlueprintCallable, Category = "Card|Presentation")
	void MoveToPresentationTransform(
		const FTransform& NewTransform,
		float VisualScaleMultiplier,
		float MotionDuration,
		float ArcHeight,
		bool bUseSettleMotion = false,
		bool bOrientToLocalViewer = false,
		float SettleStrength = 1.0f);

	/** Sends a movement target before it starts, allowing remote clients to play it on the shared server clock. */
	void MoveToPresentationTransformAtServerTime(
		const FTransform& NewTransform,
		float VisualScaleMultiplier,
		float MotionDuration,
		float ArcHeight,
		float ServerStartTime,
		bool bRevealAtMotionStart,
		bool bUseSettleMotion = false,
		bool bOrientToLocalViewer = false,
		float SettleStrength = 1.0f);

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_CardVisual();

	UFUNCTION()
	void OnRep_Selectable();

	UFUNCTION()
	void OnRep_MovementTarget();

public:
	virtual void Tick(float DeltaTime) override;

private:
	void ConfigureInteractionComponents();
	int32 ResolveDisplayRankForLocalViewer(const ASDPlayerState* LocalPlayerState) const;
	void ScheduleVisualRefreshRetry();
	void HandleVisualRefreshRetry();
	void UpdateTargetTransform();
	void EnableMotionTick();
	void MoveToSlotComponent(USceneComponent* Slot, bool bNewFaceUp, FRotator RotationOffset);
	void MoveToSlotTransform(const FTransform& SlotTransform, bool bNewFaceUp, float VisualScaleMultiplier);
	void PublishMovementTarget(
		const FTransform& NewTransform,
		bool bPlaySlotAttachMotion,
		float MotionDuration = -1.0f,
		float ArcHeight = -1.0f,
		float OvershootDistance = -1.0f,
		bool bUseSettleMotion = true,
		bool bOrientToLocalViewer = false,
		float SettleStrength = 1.0f,
		float ServerStartTime = -1.0f,
		bool bRevealAtMotionStart = false);
	void ApplyMovementTarget(
		const FTransform& NewTransform,
		bool bPlaySlotAttachMotion,
		float MotionDuration = -1.0f,
		float ArcHeight = -1.0f,
		float OvershootDistance = -1.0f,
		bool bUseSettleMotion = true,
		bool bOrientToLocalViewer = false,
		float SettleStrength = 1.0f,
		float ServerStartTime = -1.0f,
		bool bRevealAtMotionStart = false);
	void AttachToPendingSlot();
	void ClearPendingSlotAttachment();
	void ResetTravelMotionState();
	void StartSlotAttachMotion(const FTransform& TargetTransform);
	void UpdateSlotAttachMotion(float DeltaTime);
	void UpdateSlotAttachSettle(float DeltaTime, FVector& InOutVisualWorldOffset, FRotator& OutVisualRelativeRotation);
	void SetTargetVisualScaleMultiplier(float NewTargetScaleMultiplier);
	void StartVisualScaleMotion(float NewTargetScaleMultiplier);
	void ApplySynchronizedVisualScale(float NewTargetScaleMultiplier, float ServerStartTime, float MotionDuration);
	void UpdateVisualScale(float DeltaTime);
	FQuat BuildLocalViewerVisualRotation() const;
	void UpdateLocalViewerOrientation(float DeltaTime);
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
	FQuat BaseCardTextRelativeRotation = FQuat::Identity;
	FVector SlotAttachStartLocation = FVector::ZeroVector;
	FVector SlotAttachTargetLocation = FVector::ZeroVector;
	FVector SlotAttachTravelDirection = FVector::ForwardVector;
	FQuat SlotAttachStartRotation = FQuat::Identity;
	FQuat SlotAttachTargetRotation = FQuat::Identity;
	TWeakObjectPtr<USceneComponent> PendingSlotAttachComponent;
	FRotator PendingSlotAttachRotationOffset = FRotator::ZeroRotator;
	float CurrentVisualScaleMultiplier = 1.0f;
	float VisualScaleStartMultiplier = 1.0f;
	float VisualScaleElapsedTime = 0.0f;
	float SlotAttachElapsedTime = 0.0f;
	float SlotAttachSettleElapsedTime = 0.0f;
	float ActiveSlotAttachServerStartTime = -1.0f;
	float ActiveSlotAttachDuration = 0.85f;
	float ActiveSlotAttachArcHeight = 55.0f;
	float ActiveSlotAttachOvershootDistance = 8.0f;
	float ActiveSlotAttachSettleStrength = 1.0f;
	float CurrentLocalViewerOrientationAlpha = 0.0f;
	float TargetLocalViewerOrientationAlpha = 0.0f;
	bool bActiveSlotAttachSettleMotion = true;
	bool bActiveOrientToLocalViewer = false;
	bool bActiveRevealAtMotionStart = false;
	float TargetVisualScaleMultiplier = 1.0f;
	UPROPERTY(ReplicatedUsing = OnRep_MovementTarget)
	FSDCardMovementTarget ReplicatedMovementTarget;
	int32 CachedVisualRank = INDEX_NONE;
	bool bCachedVisualVisible = false;
	bool bHasCachedVisual = false;
	FTimerHandle VisualRefreshRetryTimerHandle;
	uint8 VisualRefreshRetryAttempts = 0;

	bool bVisualScaleMotionActive = false;
	bool bSlotAttachMotionActive = false;
	bool bSlotAttachSettleActive = false;
};
