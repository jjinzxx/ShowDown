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

	/** Remaining delay from the first gun-motion frame to the trigger result. */
	float GetGunMotionShotResolveDelay() const;

	UFUNCTION(BlueprintCallable, Category = "Self Shot Gun|Timing")
	float GetPresentationFinishDelay(bool bLiveRound) const;

	/** True while this local gun instance is playing a server-scripted multiplayer shot. */
	bool IsMultiplayerRoulettePresentation() const;

	/** Internal presentation availability. Player click interaction is intentionally disabled. */
	bool CanStartPresentation() const;

	// Pure helpers kept public so the multiplayer gate and seat-relative camera
	// math can be covered without creating a PIE world.
	static bool ShouldUseGunShotCamera(bool bTargetsLocalPlayer);
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
	static FTransform BuildFallbackGunShotCameraTransform(
		const FVector& TableCenter,
		const FTransform& TargetCharacterTransform,
		float BackDistance,
		float SideDistance,
		float Height,
		float LookAtHeight);
	static FTransform BuildEliminationTableOverviewTransform(
		const FVector& TableCenter,
		const FTransform& TargetCharacterTransform,
		float BackDistance,
		float Height,
		float LookAtHeight);
	static float CalculateShotRecoilWeight(
		float ElapsedTime,
		float KickDuration,
		float RecoveryDuration);
	static float CalculateEffectiveShotHoldTime(
		float ShotHoldDuration,
		float KickDuration,
		float RecoveryDuration,
		bool bRecoilEnabled);
	static int32 ResolveRaiseBulletLoadStartCount(
		int32 PreviousBet,
		int32 NewBet,
		bool bReloadAllBullets);
	static float CalculateRaiseBulletLoadSequenceDuration(
		int32 BulletCount,
		float BulletDuration,
		float StaggerDelay);
	static float CalculateRaiseBulletTravelAlpha(float NormalizedTime);
	float GetRaiseBulletLoadPresentationDuration(int32 PreviousBet, int32 NewBet) const;

	UFUNCTION(BlueprintCallable, Category = "Self Shot Gun|Status")
	void SetTableStatus(int32 LiveRounds, int32 RemainingChambers, EShowDownPhase Phase, EShowDownPlayerSlot TurnSlot);

	/** Plays the reliable table presentation for a successfully committed raise. */
	float PlayRaiseBulletLoadPresentation(
		int32 PreviousBet,
		int32 NewBet,
		EShowDownPlayerSlot SourceSlot);

	/** Replays a full, fast load only after a seven-card fold has been revealed. */
	float PlayFoldRevealBulletLoadPresentation(
		int32 BulletCount,
		EShowDownPlayerSlot SourceSlot);

	float GetFoldRevealBulletLoadPresentationDuration(int32 BulletCount) const;

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
	FVector2D AmmoStatusDrawSize = FVector2D(360.0f, 140.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Self Shot Gun|Ammo Status Display", meta = (ClampMin = "12.0", ClampMax = "48.0"))
	float AmmoStatusSlotDiameter = 28.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Self Shot Gun|Ammo Status Display", meta = (ClampMin = "0.0", ClampMax = "24.0"))
	float AmmoStatusSlotSpacing = 8.0f;

	/** One color per loaded-round count. Index 0 is one live round; index 5 is six. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Self Shot Gun|Ammo Status Display")
	TArray<FLinearColor> AmmoStatusRiskColors = {
		FLinearColor(0.95f, 0.95f, 0.93f, 1.0f),
		FLinearColor(1.00f, 0.72f, 0.32f, 1.0f),
		FLinearColor(1.00f, 0.47f, 0.10f, 1.0f),
		FLinearColor(1.00f, 0.25f, 0.04f, 1.0f),
		FLinearColor(1.00f, 0.08f, 0.025f, 1.0f),
		FLinearColor(1.00f, 0.01f, 0.02f, 1.0f)
	};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Self Shot Gun|Ammo Status Display")
	FLinearColor AmmoStatusBackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.68f);

	bool TryResolveCharacterPresentationShot(
		const AShowDownCharacter* TargetCharacter,
		FVector& OutSourceLocation,
		FVector& OutAimLocation,
		FRotator* OutRotationOffset = nullptr) const;

	void RefreshRecordingUiVisibility();

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
	void ShowAmmoStatusRaiseDelta(int32 AddedRounds);
	void CompleteAmmoStatusRaiseDelta();
	void UpdateAmmoStatusAnchorLocation();
	FLinearColor ResolveAmmoStatusRiskColor(int32 LiveRounds) const;

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
	bool bOpeningCardShowcaseStowed = true;

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

	/** Pooled bulletBetting meshes used for the temporary hand-to-gun travel effect. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<TObjectPtr<UStaticMeshComponent>> BettingBulletMeshes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Raise Bullet Loading")
	bool bEnableRaiseBulletLoadAnimation = true;

	/** Optional full replay. Disabled by default so a 1 -> 4 raise flies exactly three new bullets. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Raise Bullet Loading")
	bool bReloadAllBulletsOnRaise = false;

	/** Fallback start offset in ChamberPivot space when the raising character cannot be resolved. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Raise Bullet Loading")
	FVector RaiseBulletLoadStartOffset = FVector(86.0f, 0.0f, 12.0f);

	/** Socket/bone used as the start of the direct hand-to-gun path. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Raise Bullet Loading")
	FName RaiseBulletSourceHandName = TEXT("RightHand");

	/** Local offset from the raising character's hand bone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Raise Bullet Loading")
	FVector RaiseBulletSourceHandOffset = FVector::ZeroVector;

	/** Character-local fallback when the active skin has no matching hand bone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Raise Bullet Loading")
	FVector RaiseBulletFallbackCharacterOffset = FVector(32.0f, 26.0f, 42.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Raise Bullet Loading", meta = (ClampMin = "0.05"))
	float RaiseBulletLoadDuration = 0.95f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Raise Bullet Loading", meta = (ClampMin = "0.0"))
	float RaiseBulletLoadStaggerDelay = 0.18f;

	/** Fast full-cylinder travel used after the hidden seven-card fold is revealed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Fold Reveal Bullet Loading", meta = (ClampMin = "0.05"))
	float FoldRevealBulletLoadDuration = 0.22f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Fold Reveal Bullet Loading", meta = (ClampMin = "0.0"))
	float FoldRevealBulletLoadStaggerDelay = 0.055f;

	/** Stops the temporary bullet just short of the gun before it is hidden. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Raise Bullet Loading", meta = (ClampMin = "0.0"))
	float RaiseBulletVanishDistance = 12.0f;

	/** World scale for /Game/Fab/Revolver/bulletBetting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Raise Bullet Loading", meta = (ClampMin = "0.001"))
	float RaiseBulletBettingScale = 0.10f;

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
	float AimHoldTime = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Timing", meta = (ClampMin = "0.0"))
	float ShotHoldTime = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Timing", meta = (ClampMin = "0.01"))
	float ReturnTime = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Timing", meta = (ClampMin = "0.01", DisplayName = "Multiplayer Target Transition Time"))
	float MultiplayerTargetTransitionTime = 0.24f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Recoil")
	bool bEnableShotRecoil = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Recoil", meta = (ClampMin = "0.0", DisplayName = "Kick Time"))
	float ShotRecoilKickTime = 0.055f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Recoil", meta = (ClampMin = "0.0", DisplayName = "Recovery Time"))
	float ShotRecoilRecoveryTime = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Recoil", meta = (ClampMin = "0.0", DisplayName = "Backward Distance"))
	float ShotRecoilBackwardDistance = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Recoil", meta = (ClampMin = "0.0", DisplayName = "Upward Distance"))
	float ShotRecoilUpwardDistance = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Recoil", meta = (ClampMin = "0.0", ClampMax = "45.0", DisplayName = "Muzzle Rise Degrees"))
	float ShotRecoilMuzzleRiseDegrees = 9.0f;

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

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Self Shot Gun|Cinematic Camera", meta = (DisplayName = "Gun Shot Camera (Player 1 Reference)", ToolTip = "Author this camera for Player 1. Runtime copies the same character-relative position and rotation for Players 2-4."))
	TObjectPtr<ACameraActor> SelfShotCinematicCamera;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Cinematic Camera|Fallback", meta = (ClampMin = "0.0"))
	float FallbackGunShotCameraBackDistance = 165.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Cinematic Camera|Fallback")
	float FallbackGunShotCameraSideDistance = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Cinematic Camera|Fallback")
	float FallbackGunShotCameraHeight = 115.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Cinematic Camera|Fallback")
	float FallbackGunShotCameraLookAtHeight = 75.0f;

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

	// Briefly exposes the hit reaction before the blackout masks the shared
	// seat-reset pulse. Keep this short so ArtTone flows directly into blackout.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Self Shot Gun|Hit Sequence|Blackout", meta = (ClampMin = "0.0"))
	float HitBlackoutDelay = 0.25f;

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
	friend class FShowDownGunShotCameraTest;
	friend class FShowDownGunVisionSequenceTimingTest;
	friend class FShowDownRaiseBulletLoadingTest;

	enum class EGunAnimState : uint8
	{
		Idle,
		CameraLeadIn,
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
	void StartGunUse(bool bContinueFromCurrentTransform = false);
	void BeginGunRaiseMotion();
	static bool ShouldDelayGunRaiseForCamera(bool bCameraActive, float BlendInTime);
	static bool HasGunShotCameraArrived(bool bCameraActive, float ElapsedTime, float BlendInTime);
	void StartMechanismAnimation();
	void UpdateMechanismCocking();
	void UpdateHammerRelease();
	void UpdateEmptyShotImpact();
	void UpdateMechanismReset();
	void UpdateShotRecoil();
	void ResetTriggerAndHammer();
	void StartSelfShotCinematicCamera();
	void TryStartLocalTargetShotCamera();
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

	void HandleMultiplayerRoulettePresentation(
		EShowDownPlayerSlot TargetSlot,
		const FString& TargetName,
		int32 BulletCount,
		bool bHit,
		int32 MatchSequence,
		int32 RoundSequence);
	void HandleMultiplayerPresentationContextChanged(int32 MatchSequence, int32 RoundSequence);
	void HandleGameStateSet(AGameStateBase* GameState);

	AActor* FindMultiplayerShotTarget(EShowDownPlayerSlot TargetSlot) const;
	void PlayMultiplayerRoulettePresentation(
		EShowDownPlayerSlot TargetSlot,
		bool bHit,
		int32 MatchSequence,
		int32 RoundSequence,
		bool bContinueFromCurrentTransform = false);
	bool TryStartPendingMultiplayerRoulettePresentation(bool bContinueFromCurrentTransform = false);

	enum class EMultiplayerPresentationContextRelation : uint8
	{
		Past,
		Current,
		Future
	};

	EMultiplayerPresentationContextRelation CompareMultiplayerPresentationContext(
		int32 MatchSequence,
		int32 RoundSequence) const;
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
	void CacheBulletRestRelativeTransforms();
	FTransform ResolveBettingBulletRestRelativeTransform(int32 BulletIndex) const;
	UStaticMeshComponent* GetBulletMeshComponent(int32 BulletIndex) const;
	UStaticMeshComponent* GetBettingBulletMeshComponent(int32 BulletIndex) const;
	void SynchronizeBulletPresentationFromStatus();
	void SetBulletPresentationImmediate(int32 BulletCount);
	FVector ResolveRaiseBulletSourceWorldLocation(
		EShowDownPlayerSlot SourceSlot,
		int32 StartCount) const;
	void StartRaiseBulletLoadAnimation(
		int32 PreviousBet,
		int32 NewBet,
		EShowDownPlayerSlot SourceSlot,
		bool bForceReloadAllBullets = false,
		bool bAllowDuringReveal = false);
	void UpdateRaiseBulletLoadAnimation(float DeltaSeconds);
	void ReceiveRaiseBulletLoadPresentation(
		int32 PreviousBet,
		int32 NewBet,
		int32 PresentationRound,
		EShowDownPlayerSlot SourceSlot);
	void TryStartPendingRaiseBulletLoadPresentation();
	void ClearPendingRaiseBulletLoadPresentation();
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayRaiseBulletLoadPresentation(
		int32 PreviousBet,
		int32 NewBet,
		int32 PresentationRound,
		EShowDownPlayerSlot SourceSlot);
	void ReceiveFoldRevealBulletLoadPresentation(
		int32 BulletCount,
		int32 PresentationRound,
		EShowDownPlayerSlot SourceSlot);
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayFoldRevealBulletLoadPresentation(
		int32 BulletCount,
		int32 PresentationRound,
		EShowDownPlayerSlot SourceSlot);
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
	void PlayConfiguredSound(
		USoundBase* Sound,
		bool bPlay2D,
		const FVector& Location,
		float VolumeMultiplier = 1.0f) const;
	FTransform GetPresentationGunTransform() const;
	void SetActorTransformAlpha(const FTransform& FromTransform, const FTransform& ToTransform, float Alpha);

	FTransform RestActorTransform;
	FTransform RevolverPlacementDevPreviewRestoreTransform;
	FTransform RaiseStartTransform;
	FTransform ReturnStartTransform;
	FTransform ShotRecoilBaseTransform;
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
	float ActiveRaiseTime = 0.45f;
	float ActiveCameraLeadInTime = 0.0f;
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
	float RaiseBulletLoadElapsedTime = 0.0f;
	float ActiveBulletLoadDuration = 0.95f;
	float ActiveBulletLoadStaggerDelay = 0.18f;
	FVector ActiveRaiseBulletSourceWorldLocation = FVector::ZeroVector;
	FVector ShotRecoilDirection = FVector::ForwardVector;
	int32 DisplayedBulletCount = 0;
	int32 RaiseBulletLoadPreviousCount = 0;
	int32 RaiseBulletLoadStartCount = 0;
	int32 RaiseBulletLoadTargetCount = 0;
	int32 PendingRaiseBulletLoadPreviousCount = 0;
	int32 PendingRaiseBulletLoadTargetCount = 0;
	int32 PendingRaiseBulletLoadRound = 0;
	EShowDownPlayerSlot ActiveRaiseBulletSourceSlot = EShowDownPlayerSlot::None;
	EShowDownPlayerSlot PendingRaiseBulletLoadSourceSlot = EShowDownPlayerSlot::None;
	TArray<FTransform> BulletRestRelativeTransforms;
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
	bool bAmmoStatusEmphasisLatched = false;
	bool bAmmoStatusClearPending = false;
	bool bRaiseBulletLoadActive = false;
	bool bRaiseBulletLoadPending = false;
	bool bRaiseBulletLoadAllowedDuringReveal = false;
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
		int32 MatchSequence = 0;
		int32 RoundSequence = 0;
	};

	TArray<FPendingMultiplayerRoulettePresentation> PendingMultiplayerRoulettePresentations;
};
