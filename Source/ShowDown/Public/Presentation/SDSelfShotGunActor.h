#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/SDInteractable.h"
#include "Presentation/SDArtToneController.h"
#include "ShowDownTypes.h"
#include "SDSelfShotGunActor.generated.h"

class UBoxComponent;
class ACameraActor;
class AGameStateBase;
class AShowDownGameStateBase;
class AShowDownCharacter;
class UAudioComponent;
class UPointLightComponent;
class USceneComponent;
class USoundBase;
class UStaticMeshComponent;
class UWidgetComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSDSelfShotGunEvent);

UENUM(BlueprintType)
enum class ESDHitSequenceEaseMode : uint8
{
	Linear UMETA(DisplayName = "Linear"),
	EaseIn UMETA(DisplayName = "Ease In (Slow To Fast)"),
	EaseOut UMETA(DisplayName = "Ease Out (Fast To Slow)"),
	EaseInOut UMETA(DisplayName = "Ease In Out")
};

UENUM(BlueprintType)
enum class ESDSelfShotRoundMode : uint8
{
	AlwaysLive UMETA(DisplayName = "Always Live"),
	AlwaysEmpty UMETA(DisplayName = "Always Empty"),
	RandomChance UMETA(DisplayName = "Random Chance"),
	ChamberPattern UMETA(DisplayName = "Chamber Pattern")
};

UCLASS(Blueprintable)
class SHOWDOWN_API ASDSelfShotGunActor : public AActor, public ISDInteractable
{
	GENERATED_BODY()

public:
	ASDSelfShotGunActor();

	virtual void Tick(float DeltaSeconds) override;

#if WITH_EDITOR
	virtual bool ShouldTickIfViewportsOnly() const override;
#endif

	UFUNCTION(BlueprintCallable, Category = "Self Shot Gun")
	void UseGun();

	UFUNCTION(BlueprintCallable, Category = "Self Shot Gun")
	void UseGunWithForcedResult(bool bLiveRound);

	UFUNCTION(BlueprintCallable, Category = "Self Shot Gun")
	void UseGunWithForcedResultAtTarget(bool bLiveRound, AActor* TargetActor);

	UFUNCTION(BlueprintCallable, Category = "Self Shot Gun")
	void UseGunWithForcedResultAtTargetFromLocation(bool bLiveRound, AActor* TargetActor, FVector SourceLocation);

	UFUNCTION(BlueprintCallable, Category = "Self Shot Gun")
	void UseGunWithForcedResultAtTargetFromLocationAndCamera(
		bool bLiveRound,
		AActor* TargetActor,
		FVector SourceLocation,
		ACameraActor* ShotCamera);

	UFUNCTION(BlueprintCallable, Category = "Self Shot Gun")
	void UseGunWithForcedResultAtTargetFromLocationAimAndCamera(
		bool bLiveRound,
		AActor* TargetActor,
		FVector SourceLocation,
		FVector AimLocation,
		ACameraActor* ShotCamera);

	UFUNCTION(BlueprintCallable, Category = "Self Shot Gun")
	void UseGunWithForcedResultAtTargetFromLocationAimRotationAndCamera(
		bool bLiveRound,
		AActor* TargetActor,
		FVector SourceLocation,
		FVector AimLocation,
		FRotator RotationOffset,
		ACameraActor* ShotCamera);

	UFUNCTION(BlueprintCallable, Category = "Self Shot Gun|Target Shot")
	float GetTargetShotSourcePullDistance() const;

	UFUNCTION(BlueprintCallable, Category = "Self Shot Gun|Timing")
	float GetShotResolveDelay() const;

	UFUNCTION(BlueprintCallable, Category = "Self Shot Gun|Timing")
	float GetPresentationFinishDelay(bool bLiveRound) const;

	/** True while this local gun instance is playing a server-scripted multiplayer shot. */
	bool IsMultiplayerRoulettePresentation() const;

	// Pure helpers kept public so the multiplayer gate and seat-relative camera
	// math can be covered without creating a PIE world.
	static bool ShouldUseGunShotCamera(bool bLiveRound, bool bTargetsLocalPlayer);
	static bool ShouldUseEliminationTableOverview(
		bool bLiveRound,
		bool bTargetsLocalPlayer,
		int32 RemainingLives);
	static bool IsGunShotTargetLocalPlayer(
		EShowDownPlayerSlot TargetSlot,
		EShowDownPlayerSlot LocalPlayerSlot);
	static FTransform BuildSeatRelativeGunShotCameraTransform(
		const FTransform& PlayerOneCameraTransform,
		const FTransform& PlayerOneCharacterTransform,
		const FTransform& TargetCharacterTransform);
	static FTransform BuildEliminationTableOverviewTransform(
		const FVector& TableCenter,
		const FTransform& TargetCharacterTransform,
		float BackDistance,
		float Height,
		float LookAtHeight);

	UFUNCTION(BlueprintCallable, Category = "Self Shot Gun|Status")
	void SetTableStatus(int32 LiveRounds, int32 RemainingChambers, EShowDownPhase Phase, EShowDownPlayerSlot TurnSlot);

	// Clears the centre of the table while the opening deck is displayed, then
	// drops the complete revolver actor back onto its authored table transform.
	UFUNCTION(BlueprintCallable, Category = "Self Shot Gun|Opening Cards")
	void SetOpeningCardShowcaseStowed(bool bStowed);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Self Shot Gun|Opening Cards", meta = (ClampMin = "0.0", DisplayName = "Drop Height"))
	float OpeningCardDropHeight = 140.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Self Shot Gun|Opening Cards", meta = (ClampMin = "1.0", DisplayName = "Drop Gravity"))
	float OpeningCardDropGravity = 980.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Self Shot Gun|Opening Cards", meta = (ClampMin = "0.0", ClampMax = "0.8", DisplayName = "Drop Bounciness"))
	float OpeningCardDropRestitution = 0.22f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Self Shot Gun|Opening Cards", meta = (ClampMin = "1.0", DisplayName = "Drop Stop Speed"))
	float OpeningCardDropStopSpeed = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Self Shot Gun|Ammo Status Display")
	FVector AmmoStatusWorldOffset = FVector(0.0f, 0.0f, 12.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Self Shot Gun|Ammo Status Display", meta = (ClampMin = "32.0"))
	FVector2D AmmoStatusDrawSize = FVector2D(260.0f, 100.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Self Shot Gun|Ammo Status Display", meta = (ClampMin = "8", ClampMax = "160"))
	int32 AmmoStatusFontSize = 48;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Self Shot Gun|Ammo Status Display")
	FLinearColor AmmoStatusTextColor = FLinearColor(1.0f, 0.82f, 0.25f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Self Shot Gun|Ammo Status Display")
	FLinearColor AmmoStatusBackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.68f);

	bool TryResolveCharacterPresentationShot(
		const AShowDownCharacter* TargetCharacter,
		FVector& OutSourceLocation,
		FVector& OutAimLocation,
		FRotator* OutRotationOffset = nullptr) const;

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

	UPROPERTY(BlueprintAssignable, Category = "Self Shot Gun")
	FSDSelfShotGunEvent OnGunRaised;

	/** Fires when the trigger starts moving, before the live/empty result. */
	UPROPERTY(BlueprintAssignable, Category = "Self Shot Gun")
	FSDSelfShotGunEvent OnTriggerPullStarted;

	UPROPERTY(BlueprintAssignable, Category = "Self Shot Gun")
	FSDSelfShotGunEvent OnGunFired;

	UPROPERTY(BlueprintAssignable, Category = "Self Shot Gun")
	FSDSelfShotGunEvent OnGunEmptyFired;

	UPROPERTY(BlueprintAssignable, Category = "Self Shot Gun")
	FSDSelfShotGunEvent OnGunSequenceFinished;

	UPROPERTY(BlueprintAssignable, Category = "Self Shot Gun")
	FSDSelfShotGunEvent OnGunPresentationFinished;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_TableStatus();

	UFUNCTION()
	void OnRep_OpeningCardShowcaseStowed();

	UFUNCTION()
	void HandleGamePhaseChanged(EShowDownPhase NewPhase);

	UFUNCTION()
	void HandleTableCinematicCue(ESDTableCinematicCue Cue, uint8 PlayerSlotMask);

	void ApplyAmmoStatusDisplaySettings();
	void UpdateAmmoStatusAnchorLocation();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> AmmoStatusAnchor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent> AmmoStatusWidgetComponent;

	UPROPERTY(ReplicatedUsing = OnRep_TableStatus, BlueprintReadOnly, Category = "Self Shot Gun|Status")
	int32 StatusLiveRounds = 0;

	UPROPERTY(ReplicatedUsing = OnRep_TableStatus, BlueprintReadOnly, Category = "Self Shot Gun|Status")
	int32 StatusRemainingChambers = 6;

	UPROPERTY(ReplicatedUsing = OnRep_TableStatus, BlueprintReadOnly, Category = "Self Shot Gun|Status")
	EShowDownPhase StatusPhase = EShowDownPhase::None;

	UPROPERTY(ReplicatedUsing = OnRep_TableStatus, BlueprintReadOnly, Category = "Self Shot Gun|Status")
	EShowDownPlayerSlot StatusTurnSlot = EShowDownPlayerSlot::None;

	UPROPERTY(ReplicatedUsing = OnRep_OpeningCardShowcaseStowed)
	bool bOpeningCardShowcaseStowed = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> GunMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> InteractionBounds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> ChamberPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ChamberMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BulletMesh01;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BulletMesh02;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BulletMesh03;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BulletMesh04;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BulletMesh05;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BulletMesh06;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> TriggerPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> TriggerMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> HammerPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> HammerMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> HandleMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RatchetMechanismMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ExtraMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> MuzzlePoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> MuzzleFlashLight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Target Shot", meta = (DisplayName = "Aim Offset"))
	FVector TargetShotAimOffset = FVector(0.0f, 0.0f, 90.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Target Shot", meta = (DisplayName = "Rotation Offset (Pitch, Yaw, Roll)", ToolTip = "Yaw 180 flips the mesh so the muzzle faces the target when the gun mesh forward axis is reversed."))
	FRotator TargetShotRotationOffset = FRotator(-22.0f, 180.0f, -58.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Target Shot", meta = (ClampMin = "0.0", DisplayName = "Source Pull Distance"))
	float TargetShotSourcePullDistance = 36.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Developer Preview", meta = (DisplayName = "Enable Revolver Placement Dev Mode"))
	bool bEnableRevolverPlacementDevMode = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Self Shot Gun|Developer Preview", meta = (EditCondition = "bEnableRevolverPlacementDevMode", DisplayName = "Preview Target Character"))
	TObjectPtr<AShowDownCharacter> DevPreviewTargetCharacter;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Developer Preview", meta = (EditCondition = "bEnableRevolverPlacementDevMode", DisplayName = "Preview Target Slot"))
	EShowDownPlayerSlot DevPreviewTargetSlot = EShowDownPlayerSlot::Player1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Developer Preview", meta = (EditCondition = "bEnableRevolverPlacementDevMode", DisplayName = "Fallback To Any Player Character"))
	bool bDevPreviewFallbackToAnyPlayerCharacter = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Developer Preview", meta = (EditCondition = "bEnableRevolverPlacementDevMode", DisplayName = "Draw Source And Aim Debug"))
	bool bDrawRevolverPlacementDevDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Developer Preview", meta = (EditCondition = "bEnableRevolverPlacementDevMode && bDrawRevolverPlacementDevDebug", ClampMin = "1.0", DisplayName = "Debug Size"))
	float RevolverPlacementDevDebugSize = 16.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Held Jitter")
	bool bEnableHeldGunJitter = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Held Jitter")
	FRotator HeldGunJitterRotationAmplitude = FRotator(0.28f, 0.38f, 0.32f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Held Jitter")
	FVector HeldGunJitterLocationAmplitude = FVector(0.18f, 0.28f, 0.16f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Held Jitter", meta = (ClampMin = "0.01"))
	float HeldGunJitterStepInterval = 0.07f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Timing", meta = (ClampMin = "0.01"))
	float RaiseTime = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Timing", meta = (ClampMin = "0.0"))
	float AimHoldTime = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Timing", meta = (ClampMin = "0.0"))
	float ShotHoldTime = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Timing", meta = (ClampMin = "0.01"))
	float ReturnTime = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Mechanism")
	bool bEnableMechanismAnimation = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Mechanism|Timing", meta = (ClampMin = "0.01"))
	float MechanismCockTime = 0.24f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Mechanism|Timing", meta = (ClampMin = "0.01"))
	float HammerReleaseTime = 0.055f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Mechanism|Timing", meta = (ClampMin = "0.01"))
	float TriggerResetTime = 0.16f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Mechanism|Rotation")
	FRotator TriggerPulledRotationOffset = FRotator(-18.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Mechanism|Rotation")
	FRotator HammerCockedRotationOffset = FRotator(-38.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Mechanism|Rotation")
	FRotator HammerFiredRotationOffset = FRotator(5.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Mechanism|Rotation")
	FRotator ChamberStepRotationOffset = FRotator(0.0f, 0.0f, 60.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Mechanism")
	bool bKeepChamberRotationAfterShot = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Cinematic Camera")
	bool bUseSelfShotCinematicCamera = true;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Self Shot Gun|Cinematic Camera", meta = (DisplayName = "Gun Shot Camera (Player 1 Reference)", ToolTip = "Author this camera for Player 1. Runtime copies the same character-relative position and rotation for Players 2-4."))
	TObjectPtr<ACameraActor> SelfShotCinematicCamera;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Cinematic Camera", meta = (ClampMin = "0.0"))
	float CinematicCameraBlendInTime = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Cinematic Camera", meta = (ClampMin = "0.0"))
	float CinematicCameraHoldTime = 2.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Cinematic Camera", meta = (ClampMin = "0.0"))
	float CinematicCameraBlendOutTime = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Cinematic Camera", meta = (ClampMin = "1.0"))
	float CinematicCameraBlendExponent = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Cinematic Camera|Elimination", meta = (DisplayName = "Use Elimination Table Overview"))
	bool bUseEliminationTableOverview = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Cinematic Camera|Elimination", meta = (ClampMin = "0.0", DisplayName = "Overview Move Time"))
	float EliminationOverviewMoveTime = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Cinematic Camera|Elimination", meta = (ClampMin = "0.0", DisplayName = "Overview Back Distance"))
	float EliminationOverviewBackDistance = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Cinematic Camera|Elimination", meta = (DisplayName = "Overview Height"))
	float EliminationOverviewHeight = 135.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Cinematic Camera|Elimination", meta = (DisplayName = "Overview Look At Height"))
	float EliminationOverviewLookAtHeight = 28.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Shot Result")
	ESDSelfShotRoundMode ShotResultMode = ESDSelfShotRoundMode::AlwaysLive;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Shot Result", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "ShotResultMode == ESDSelfShotRoundMode::RandomChance"))
	float LiveRoundChance = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Shot Result|Chamber Pattern", meta = (ClampMin = "0", ClampMax = "5", EditCondition = "ShotResultMode == ESDSelfShotRoundMode::ChamberPattern", ToolTip = "0 is chamber 1, 5 is chamber 6. This is the next chamber that will be resolved when the hammer drops."))
	int32 CurrentChamberIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Shot Result|Chamber Pattern", meta = (EditCondition = "ShotResultMode == ESDSelfShotRoundMode::ChamberPattern"))
	bool bChamber1Live = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Shot Result|Chamber Pattern", meta = (EditCondition = "ShotResultMode == ESDSelfShotRoundMode::ChamberPattern"))
	bool bChamber2Live = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Shot Result|Chamber Pattern", meta = (EditCondition = "ShotResultMode == ESDSelfShotRoundMode::ChamberPattern"))
	bool bChamber3Live = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Shot Result|Chamber Pattern", meta = (EditCondition = "ShotResultMode == ESDSelfShotRoundMode::ChamberPattern"))
	bool bChamber4Live = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Shot Result|Chamber Pattern", meta = (EditCondition = "ShotResultMode == ESDSelfShotRoundMode::ChamberPattern"))
	bool bChamber5Live = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Shot Result|Chamber Pattern", meta = (EditCondition = "ShotResultMode == ESDSelfShotRoundMode::ChamberPattern"))
	bool bChamber6Live = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Shot Result|Chamber Pattern", meta = (EditCondition = "ShotResultMode == ESDSelfShotRoundMode::ChamberPattern"))
	bool bConsumeLiveChamberAfterShot = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Effects")
	TObjectPtr<USoundBase> GunshotSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Effects")
	bool bPlayGunshotSound2D = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Effects")
	TObjectPtr<USoundBase> TriggerPullSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Effects")
	bool bPlayTriggerPullSound2D = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Effects|Empty Shot")
	TObjectPtr<USoundBase> EmptyShotSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Effects|Empty Shot")
	bool bPlayEmptyShotSound2D = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Mechanism|Empty Shot", meta = (ClampMin = "0.005"))
	float EmptyShotImpactTime = 0.035f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Mechanism|Empty Shot", meta = (ClampMin = "0.0"))
	float EmptyShotImpactHoldTime = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Mechanism|Empty Shot")
	FRotator EmptyShotTriggerExtraPullRotationOffset = FRotator(-8.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Mechanism|Empty Shot")
	FRotator EmptyShotHammerImpactRotationOffset = FRotator(14.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Effects|Tinnitus")
	TObjectPtr<USoundBase> TinnitusSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Effects|Tinnitus", meta = (ClampMin = "0.05"))
	float TinnitusPlayDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Effects|Tinnitus", meta = (ClampMin = "0.0"))
	float TinnitusFadeInDuration = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Effects|Tinnitus", meta = (ClampMin = "0.0"))
	float TinnitusFadeOutDuration = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Effects|Tinnitus", meta = (ClampMin = "0.0"))
	float TinnitusStartTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Effects|Tinnitus", meta = (ClampMin = "0.0"))
	float TinnitusVolumeMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Muzzle Flash", meta = (ClampMin = "0.0"))
	float MuzzleFlashIntensity = 8000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Muzzle Flash", meta = (ClampMin = "0.01"))
	float MuzzleFlashDuration = 0.025f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Muzzle Flash", meta = (ClampMin = "0.0"))
	float MuzzleFlashAttenuationRadius = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Muzzle Flash")
	FLinearColor MuzzleFlashColor = FLinearColor(1.0f, 0.52f, 0.16f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Hit Sequence")
	bool bEnableHitSequence = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Hit Sequence")
	TObjectPtr<ASDArtToneController> HitSequenceArtToneController;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Hit Sequence|Stage 1")
	FSDArtToneSettings InitialHitEffectSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Hit Sequence|Stage 1", meta = (ClampMin = "0.0"))
	float InitialHitEffectDuration = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Hit Sequence|Stage 1")
	FRotator InitialHitShakeRotationAmplitude = FRotator(2.4f, 1.2f, 1.8f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Hit Sequence|Stage 1")
	FVector InitialHitShakeLocationAmplitude = FVector(0.8f, 1.6f, 0.7f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Hit Sequence|Stage 1", meta = (ClampMin = "0.01"))
	float InitialHitShakeStepInterval = 0.045f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Hit Sequence|Blackout", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HitBlackoutAmount = 1.0f;

	// Lets the victim and observers see the body settle before the blackout masks
	// the shared seat-reset pulse at its peak.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Hit Sequence|Blackout", meta = (ClampMin = "0.0"))
	float HitBlackoutDelay = 0.82f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Hit Sequence|Blackout", meta = (ClampMin = "0.0"))
	float HitBlackoutDuration = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Hit Sequence|Recovery")
	FSDArtToneSettings RecoveryHitEffectSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Hit Sequence|Recovery", meta = (ClampMin = "0.0"))
	float RecoveryHitEffectHoldTime = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Hit Sequence|Recovery", meta = (ClampMin = "0.01"))
	float RecoveryHitEffectBlendOutTime = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Hit Sequence|Recovery")
	ESDHitSequenceEaseMode RecoveryHitEffectEaseMode = ESDHitSequenceEaseMode::EaseIn;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Hit Sequence|Recovery", meta = (ClampMin = "1.0"))
	float RecoveryHitEffectEaseExponent = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Hit Sequence|Recovery")
	FRotator RecoveryHitShakeRotationAmplitude = FRotator(1.2f, 1.8f, 2.5f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Hit Sequence|Recovery")
	FVector RecoveryHitShakeLocationAmplitude = FVector(0.35f, 1.3f, 0.75f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Hit Sequence|Recovery", meta = (ClampMin = "0.0"))
	float RecoveryHitShakeHoldTime = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Hit Sequence|Recovery", meta = (ClampMin = "0.01"))
	float RecoveryHitShakeBlendOutTime = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Hit Sequence|Recovery", meta = (ClampMin = "0.01"))
	float RecoveryHitShakeStepInterval = 0.075f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun")
	bool bDisableCollisionWhileUsing = true;

private:
	friend class FShowDownGunVisionSequenceTimingTest;

	enum class EGunAnimState : uint8
	{
		Idle,
		Raising,
		Aiming,
		Cocking,
		HammerReleasing,
		EmptyImpact,
		Fired,
		Returning
	};

	enum class EHitSequenceState : uint8
	{
		Idle,
		InitialHit,
		PreBlackoutHold,
		Blackout,
		RecoveryHold,
		RecoveryBlendOut
	};

	void FireGun();
	void FireLiveRound();
	void FireEmptyRound();
	void FinishSequence();
	void StartGunUse();
	void StartMechanismAnimation();
	void UpdateMechanismCocking();
	void UpdateHammerRelease();
	void UpdateEmptyShotImpact();
	void UpdateMechanismReset();
	void ResetTriggerAndHammer();
	void StartSelfShotCinematicCamera();
	void TryStartKnownLiveLocalShotCamera();
	void ActivateSelfShotCinematicCamera();
	void UpdateSelfShotCinematicCamera(float DeltaSeconds);
	bool TryStartEliminationTableOverview();
	void FinishEliminationTableOverview();
	FTransform BuildEliminationTableOverviewTransform(const AShowDownCharacter* TargetCharacter) const;
	void CancelSelfShotCinematicCamera();
	bool PrepareLocalGunShotCamera();
	ACameraActor* GetOrCreateLocalGunShotCamera();
	AShowDownCharacter* FindGunShotCameraReferenceCharacter() const;
	AShowDownCharacter* ResolveCurrentGunShotCameraTarget() const;
	FTransform ApplyHeldGunJitter(const FTransform& BaseTransform) const;
	void PlayCinematicCameraSteppedShake(
		float HoldDuration,
		float BlendOutTime,
		FRotator RotationAmplitude,
		FVector LocationAmplitude,
		float StepInterval);
	void UpdateCinematicCameraSteppedShake(float DeltaSeconds);

	UFUNCTION()
	void HandleMultiplayerRoulettePresentation(
		EShowDownPlayerSlot TargetSlot,
		const FString& TargetName,
		int32 BulletCount,
		bool bHit);
	void HandleGameStateSet(AGameStateBase* GameState);

	AActor* FindMultiplayerShotTarget(EShowDownPlayerSlot TargetSlot) const;
	void PlayMultiplayerRoulettePresentation(EShowDownPlayerSlot TargetSlot, bool bHit);
	void TryStartPendingMultiplayerRoulettePresentation();
	bool ShouldTreatSlotAsLocalPlayer(EShowDownPlayerSlot TargetSlot) const;
	bool ShouldTreatTargetAsLocalPlayer(AActor* TargetActor) const;
	bool UpdateRevolverPlacementDevPreview();
	AShowDownCharacter* FindRevolverPlacementDevPreviewTarget() const;
	FTransform MakeTargetShotTransform(
		const FVector& SourceLocation,
		const FVector& AimLocation,
		FRotator RotationOffset = FRotator::ZeroRotator) const;
	FVector GetPresentationScale3D() const;
	void DrawRevolverPlacementDevPreview(const FVector& SourceLocation, const FVector& AimLocation, const FTransform& GunTransform) const;

	static FRotator LerpRotation(const FRotator& From, const FRotator& To, float Alpha);
	void StartHitSequence();
	void UpdateHitSequence(float DeltaSeconds);
	void EnterHitSequencePreBlackoutHold();
	void EnterHitSequenceBlackout();
	void EnterHitSequenceRecovery();
	void FinishHitSequence();
	void BroadcastPresentationFinishedIfIdle();
	void StartTinnitusSound();
	void UpdateTinnitusSound(float DeltaSeconds);
	void StopTinnitusSound();
	bool IsRuntimeTickRequired() const;
	void RefreshRuntimeTickState();
	void StageOpeningCardDrop();
	void StartOpeningCardDrop();
	void UpdateOpeningCardDrop(float DeltaSeconds);
	void FinishOpeningCardDrop();
	UFUNCTION(NetMulticast, Reliable)
	void MulticastFinishOpeningCardDrop();
	void SetBlackoutInstant(float Alpha, bool bHoldWhenFinished);
	ASDArtToneController* ResolveHitSequenceArtToneController();
	bool ResolveCurrentShotIsLive() const;
	void ConsumeCurrentChamberIfNeeded(bool bLiveShot);
	void AdvanceCurrentChamberIndex();
	bool IsChamberLive(int32 ChamberIndex) const;
	void SetChamberLive(int32 ChamberIndex, bool bLive);
	void PlayConfiguredSound(USoundBase* Sound, bool bPlay2D, const FVector& Location) const;
	FTransform GetPresentationGunTransform() const;
	void SetActorTransformAlpha(const FTransform& FromTransform, const FTransform& ToTransform, float Alpha);

	FTransform RestActorTransform;
	FTransform RevolverPlacementDevPreviewRestoreTransform;
	FTransform RaiseStartTransform;
	FTransform ReturnStartTransform;
	FRotator TriggerRestRotation = FRotator::ZeroRotator;
	FRotator HammerRestRotation = FRotator::ZeroRotator;
	FRotator MechanismResetStartTriggerRotation = FRotator::ZeroRotator;
	FRotator MechanismResetStartHammerRotation = FRotator::ZeroRotator;
	FRotator ChamberInitialRotation = FRotator::ZeroRotator;
	FRotator ChamberCurrentRotation = FRotator::ZeroRotator;
	FRotator ChamberStartRotation = FRotator::ZeroRotator;
	FRotator ChamberTargetRotation = FRotator::ZeroRotator;
	TWeakObjectPtr<ACameraActor> ForcedShotCamera;
	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> ActiveSelfShotCinematicCamera = nullptr;
	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> LocalGunShotCamera = nullptr;
	ECollisionEnabled::Type OriginalCollisionEnabled = ECollisionEnabled::QueryAndPhysics;
	ECollisionEnabled::Type OriginalInteractionCollisionEnabled = ECollisionEnabled::QueryOnly;
	EGunAnimState AnimState = EGunAnimState::Idle;
	EHitSequenceState HitSequenceState = EHitSequenceState::Idle;
	float StateElapsedTime = 0.0f;
	float MechanismResetElapsedTime = 0.0f;
	float HeldGunJitterElapsedTime = 0.0f;
	float CinematicCameraElapsedTime = 0.0f;
	float CinematicCameraBlendOutElapsedTime = 0.0f;
	float EliminationOverviewElapsedTime = 0.0f;
	float CinematicCameraShakeElapsedTime = 0.0f;
	float CinematicCameraShakeHoldDuration = 0.0f;
	float CinematicCameraShakeBlendOutTime = 0.0f;
	float CinematicCameraShakeStepInterval = 0.06f;
	float CinematicCameraShakeSeed = 0.0f;
	float HitSequenceElapsedTime = 0.0f;
	float MuzzleFlashElapsedTime = 0.0f;
	float TinnitusElapsedTime = 0.0f;
	float OpeningCardDropVelocityZ = 0.0f;
	bool bSelfShotCinematicCameraActive = false;
	bool bSelfShotCinematicCameraStartPending = false;
	bool bSelfShotCinematicCameraHoldStarted = false;
	bool bSelfShotCinematicCameraBlendOutActive = false;
	bool bEliminationTableOverviewActive = false;
	bool bPresentationFinishPending = false;
	bool bHasCapturedRestActorTransform = false;
	bool bOpeningCardDropActive = false;
	bool bRevolverPlacementDevPreviewActive = false;
	bool bCurrentShotTargetsLocalPlayer = true;
	bool bCurrentShotWasEmpty = false;
	bool bHasForcedShotSourceLocation = false;
	bool bHasForcedShotAimLocation = false;
	bool bHasForcedShotRotationOffset = false;
	bool bHasGunShotCameraReferenceTransform = false;
	bool bCinematicCameraShakeActive = false;
	bool bTinnitusFadeOutStarted = false;
	bool bHitSequenceBlackoutActive = false;
	bool bMultiplayerRoulettePresentationActive = false;
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> TinnitusAudioComponent;
	TWeakObjectPtr<AShowDownGameStateBase> BoundShowDownGameState;
	TWeakObjectPtr<AActor> ForcedShotTargetActor;
	EShowDownPlayerSlot CurrentShotTargetSlot = EShowDownPlayerSlot::None;
	FVector ForcedShotSourceLocation = FVector::ZeroVector;
	FVector ForcedShotAimLocation = FVector::ZeroVector;
	FRotator ForcedShotRotationOffset = FRotator::ZeroRotator;
	FRotator CinematicCameraShakeRotationAmplitude = FRotator::ZeroRotator;
	FVector CinematicCameraShakeLocationAmplitude = FVector::ZeroVector;
	FTransform CinematicCameraShakeBaseTransform;
	FTransform GunShotCameraReferenceTransform;
	FTransform EliminationOverviewStartTransform;
	FTransform EliminationOverviewTargetTransform;
	FSDArtToneSettings HitSequenceBaseSettings;

	struct FPendingMultiplayerRoulettePresentation
	{
		EShowDownPlayerSlot TargetSlot = EShowDownPlayerSlot::None;
		bool bHit = false;
	};

	TArray<FPendingMultiplayerRoulettePresentation> PendingMultiplayerRoulettePresentations;
};
