#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ShowDownCharacterAnimTypes.h"
#include "ShowDownTypes.h"
#include "TimerManager.h"
#include "ShowDownCharacter.generated.h"

class ASDPlayerState;
class UAnimationAsset;
class UAnimMontage;
class UAnimInstance;
class USceneComponent;
class UShowDownCharacterAnimInstance;
class UShowDownCharacterSkinCatalog;
class USkeletalMesh;
class USpotLightComponent;
class UTextRenderComponent;
class UWidgetComponent;

USTRUCT()
struct FShowDownCharacterBetStatusPresentation
{
	GENERATED_BODY()

	UPROPERTY()
	bool bVisible = false;

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	FString StatusText;

	UPROPERTY()
	int32 BulletCount = 0;

	UPROPERTY()
	int32 MaxBulletCount = 6;

	UPROPERTY()
	bool bNeedsToMatchBet = false;

	UPROPERTY()
	FLinearColor AccentColor = FLinearColor(1.0f, 0.72f, 0.18f, 1.0f);
};

/**
 * One server-authored hit presentation. Clients derive its visual phase from
 * the replicated server start time instead of running independent recovery
 * timers, so a late packet cannot leave one seat snapped upright early.
 */
USTRUCT()
struct FShowDownHitRecoveryPresentationState
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Sequence = 0;

	UPROPERTY()
	float ServerStartTimeSeconds = 0.0f;

	UPROPERTY()
	bool bActive = false;

	UPROPERTY()
	bool bFinalElimination = false;
};

UCLASS(Blueprintable)
class SHOWDOWN_API AShowDownCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AShowDownCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PostInitializeComponents() override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Character Animation")
	void SetCharacterAnimState(EShowDownCharacterAnimState NewState);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Character Animation")
	void PlayCharacterActionAnim(EShowDownCharacterAnimState NewState, float Duration = -1.0f, bool bReturnToIdle = true);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Character Animation")
	void PlayShootAnimation(float Duration = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Character Animation")
	void PlayHitAnimation(float Duration = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Character Animation")
	void PlaySelectCardAnimation(float Duration = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Character Animation")
	void PlayBettingAnimation(float Duration = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Character Physics")
	void StartHitRagdoll();

	/** Starts the synchronized downed -> reset-pulse -> recovery presentation. */
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Hit Recovery")
	void StartHitRecoveryPresentation(bool bFinalElimination);

	/** Clears an interrupted presentation, for example when a new match resets the table. */
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Hit Recovery")
	void CancelHitRecoveryPresentation(bool bRevealCharacter = true);

	UFUNCTION(BlueprintPure, Category = "ShowDown|Hit Recovery")
	bool IsHitRecoveryPresentationActive() const { return HitRecoveryPresentationState.bActive; }

	UFUNCTION(BlueprintPure, Category = "ShowDown|Hit Recovery")
	float GetHitRecoveryPresentationDuration() const;

	UFUNCTION(BlueprintPure, Category = "ShowDown|Hit Recovery")
	float GetHitRecoveryPresentationRemainingTime() const;

	static float CalculateHitRecoveryPresentationDuration(
		float DownedHoldDuration,
		float ResetPulseDuration,
		float RecoveryRevealDuration);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Character Animation")
	void ResetCharacterAnimState();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Character Animation")
	void FinishCurrentCharacterActionAnim();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Character Animation")
	void NotifyCharacterActionAnimFinished(EShowDownCharacterAnimState FinishedState);

	UFUNCTION(BlueprintPure, Category = "ShowDown|Character Animation")
	EShowDownCharacterAnimState GetCharacterAnimState() const { return ReplicatedAnimState; }

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Character Skin")
	void SetCharacterSkinId(const FString& NewSkinId);

	UFUNCTION(BlueprintPure, Category = "ShowDown|Character Skin")
	FString GetCharacterSkinId() const { return CharacterSkinId; }

	UFUNCTION(BlueprintPure, Category = "ShowDown|Character Skin")
	static FString GetDefaultCharacterSkinId();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Player Camera")
	void SetPlayerViewRotation(FRotator ViewRotation);

	UFUNCTION(BlueprintPure, Category = "ShowDown|Player Camera")
	FName ResolvePlayerCameraAttachName() const;

	UFUNCTION(BlueprintPure, Category = "ShowDown|Player Camera")
	FVector GetPlayerCameraRelativeLocation() const { return PlayerCameraRelativeLocation; }

	UFUNCTION(BlueprintPure, Category = "ShowDown|Player Camera")
	FVector GetPlayerCameraStableLocationOffset() const { return PlayerCameraStableLocationOffset; }

	UFUNCTION(BlueprintPure, Category = "ShowDown|Player Camera")
	FRotator GetPlayerCameraRotationOffset() const { return PlayerCameraRotationOffset; }

	UFUNCTION(BlueprintPure, Category = "ShowDown|Player Camera")
	float GetPlayerCameraFOV() const { return PlayerCameraFOV; }

	UFUNCTION(BlueprintPure, Category = "ShowDown|Presentation")
	USceneComponent* GetRevolverPresentationAnchor() const { return RevolverPresentationAnchor; }

	UFUNCTION(BlueprintPure, Category = "ShowDown|Presentation")
	USceneComponent* GetForeheadCardAnchor() const { return ForeheadCardAnchor; }

	UFUNCTION(BlueprintPure, Category = "ShowDown|Presentation")
	FTransform GetRevolverPresentationTransform() const;

	UFUNCTION(BlueprintPure, Category = "ShowDown|Presentation")
	bool ShouldAutoAimRevolverPresentationAtTarget() const { return bAutoAimRevolverPresentationAtTarget; }

	UFUNCTION(BlueprintPure, Category = "ShowDown|Presentation")
	bool ShouldAutoFaceForeheadCardToOpponents() const { return bAutoFaceForeheadCardToOpponents; }

	UFUNCTION(BlueprintPure, Category = "ShowDown|Character Camera")
	float GetHeadLookPitch() const { return HeadLookPitch; }

	UFUNCTION(BlueprintPure, Category = "ShowDown|Character Camera")
	float GetHeadLookYaw() const { return HeadLookYaw; }

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Character Identity")
	void SetCharacterIdentity(EShowDownCharacterRole NewRole, EShowDownPlayerSlot NewPlayerSlot, const FString& NewDisplayName);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Character Identity")
	void ApplyIdentityFromPlayerState(ASDPlayerState* InPlayerState, EShowDownCharacterRole NewRole);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Character Identity")
	void SetCharacterRole(EShowDownCharacterRole NewRole);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Character Identity")
	void SetPlayerSlot(EShowDownPlayerSlot NewPlayerSlot);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Character Identity")
	void SetCharacterDisplayName(const FString& NewDisplayName);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Name Tag")
	void SetCharacterLives(int32 NewLives);

	UFUNCTION(BlueprintPure, Category = "ShowDown|Name Tag")
	int32 GetCharacterLives() const { return CharacterLives; }

	/** Re-registers the screen-space name tag after viewport UI layers change. */
	void RestoreNameTagScreenRegistration();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Voice")
	void SetVoiceTalking(bool bNewVoiceTalking);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Bet Status")
	void SetBetStatusPresentation(
		bool bVisible,
		const FString& DisplayName,
		const FString& StatusText,
		int32 BulletCount,
		int32 MaxBulletCount,
		bool bNeedsToMatchBet,
		const FLinearColor& AccentColor);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Bet Status")
	void ClearBetStatusPresentation();

	UFUNCTION(BlueprintPure, Category = "ShowDown|Character Identity")
	EShowDownCharacterRole GetCharacterRole() const { return CharacterRole; }

	UFUNCTION(BlueprintPure, Category = "ShowDown|Character Identity")
	EShowDownPlayerSlot GetPlayerSlot() const { return PlayerSlot; }

	UFUNCTION(BlueprintPure, Category = "ShowDown|Character Identity")
	FString GetCharacterDisplayName() const { return CharacterDisplayName; }

	UFUNCTION(BlueprintPure, Category = "ShowDown|Character Identity")
	bool IsAssignedToSlot(EShowDownPlayerSlot Slot) const { return PlayerSlot == Slot; }

	UFUNCTION(BlueprintPure, Category = "ShowDown|Character Identity")
	EShowDownPlayerSlot GetLocalPlayerSlot() const;

	UFUNCTION(BlueprintPure, Category = "ShowDown|Character Identity")
	bool IsLocalPlayerCharacter() const;

	UFUNCTION(BlueprintPure, Category = "ShowDown|Character Identity")
	bool IsOpponentCharacterForLocalPlayer() const;

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Character Visibility")
	void SetCharacterSceneActive(bool bNewActive);

	UFUNCTION(BlueprintPure, Category = "ShowDown|Character Visibility")
	bool IsCharacterSceneActive() const { return bCharacterSceneActive; }

	UFUNCTION(BlueprintImplementableEvent, Category = "ShowDown|Character Identity")
	void OnCharacterIdentityChanged();

	UFUNCTION(BlueprintImplementableEvent, Category = "ShowDown|Character Animation")
	void OnCharacterAnimStateChanged(EShowDownCharacterAnimState NewState);

	UFUNCTION(BlueprintImplementableEvent, Category = "ShowDown|Character Skin")
	void OnCharacterSkinChanged(const FString& NewSkinId);

protected:
	UFUNCTION()
	void OnRep_AnimState();

	UFUNCTION()
	void OnRep_CharacterSkinId();

	UFUNCTION()
	void OnRep_Identity();

	UFUNCTION()
	void OnRep_CharacterLives(int32 PreviousLives);

	UFUNCTION()
	void OnRep_ViewRotation();

	UFUNCTION()
	void OnRep_SceneActive();

	UFUNCTION()
	void OnRep_BetStatusPresentation();

	UFUNCTION()
	void OnRep_HitRecoveryPresentationState();

	UFUNCTION(Server, Reliable)
	void ServerSetCharacterAnimState(EShowDownCharacterAnimState NewState);

	UFUNCTION(Server, Reliable)
	void ServerPlayCharacterActionAnim(EShowDownCharacterAnimState NewState, float Duration, bool bReturnToIdle);

	UFUNCTION(Server, Reliable)
	void ServerNotifyCharacterActionAnimFinished(EShowDownCharacterAnimState FinishedState);

	UFUNCTION()
	void HandleRouletteStarted(EShowDownSide Target, int32 BulletCount);

	UFUNCTION()
	void HandleRouletteResult(EShowDownSide Target, bool bHit);

	UFUNCTION()
	void HandleLifeChanged(EShowDownSide Target, int32 Life);

	UFUNCTION()
	void HandleMultiplayerRouletteStarted(EShowDownPlayerSlot TargetSlot, const FString& TargetName, int32 BulletCount);

	UFUNCTION()
	void HandleMultiplayerRouletteResult(EShowDownPlayerSlot TargetSlot, const FString& TargetName, int32 BulletCount, bool bHit, int32 RemainingLives);

	UFUNCTION()
	void HandleTablePhaseChanged(EShowDownPhase NewPhase);

	UFUNCTION()
	void HandleTableCinematicCue(ESDTableCinematicCue Cue, uint8 PlayerSlotMask);

	UFUNCTION()
	void HandleCardSelected(EShowDownSide Side);

	UFUNCTION()
	void HandleBetActionCommitted(EShowDownSide Side, EShowDownBetAction Action, int32 TargetBet);

	UFUNCTION()
	void HandleMultiplayerCardSelected(EShowDownPlayerSlot Slot);

	UFUNCTION()
	void HandleMultiplayerBetActionCommitted(EShowDownPlayerSlot Slot, EShowDownBetAction Action, int32 TargetBet);

	UFUNCTION()
	void HandleNameTagRoundStatusChanged();

	UFUNCTION()
	void HandleChatMessageReceived(const FString& SenderName, const FString& Message);

	UFUNCTION(Server, Reliable)
	void ServerSetCharacterIdentity(EShowDownCharacterRole NewRole, EShowDownPlayerSlot NewPlayerSlot, const FString& NewDisplayName);

	UPROPERTY(ReplicatedUsing = OnRep_AnimState, BlueprintReadOnly, Category = "ShowDown|Character Animation")
	EShowDownCharacterAnimState ReplicatedAnimState = EShowDownCharacterAnimState::Idle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ShowDown|Character Skin")
	TObjectPtr<UShowDownCharacterSkinCatalog> CharacterSkinCatalog;

	UPROPERTY(ReplicatedUsing = OnRep_CharacterSkinId, EditDefaultsOnly, BlueprintReadOnly, Category = "ShowDown|Character Skin")
	FString CharacterSkinId = TEXT("robot");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ShowDown|Presentation")
	TObjectPtr<USceneComponent> RevolverPresentationAnchor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ShowDown|Presentation")
	TObjectPtr<USceneComponent> ForeheadCardAnchor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Presentation", meta = (DisplayName = "Auto Aim Revolver Presentation At Target"))
	bool bAutoAimRevolverPresentationAtTarget = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Presentation", meta = (DisplayName = "Auto Face Forehead Card To Opponents"))
	bool bAutoFaceForeheadCardToOpponents = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ShowDown|Name Tag")
	TObjectPtr<UWidgetComponent> NameTagWidgetComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ShowDown|World Lives")
	TObjectPtr<USceneComponent> WorldLivesAnchor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ShowDown|World Lives")
	TObjectPtr<UTextRenderComponent> WorldLivesShadowText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ShowDown|World Lives")
	TObjectPtr<UTextRenderComponent> WorldLivesText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|World Lives", meta = (ClampMin = "4.0"))
	float WorldLivesTextSize = 16.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|World Lives", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float WorldLifeLostPulseDuration = 0.28f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|World Lives", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WorldLifeLostPulseScale = 0.35f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ShowDown|World Bet Status")
	TObjectPtr<USceneComponent> BetStatusAnchorComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ShowDown|World Bet Status")
	TObjectPtr<UTextRenderComponent> BetStatusValueText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ShowDown|World Bet Status")
	TObjectPtr<UTextRenderComponent> BetStatusActionText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|World Bet Status", meta = (ClampMin = "4.0"))
	float BetStatusValueTextSize = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|World Bet Status", meta = (ClampMin = "4.0"))
	float BetStatusActionTextSize = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Player Camera")
	FName PlayerCameraAttachName = TEXT("Head");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Player Camera")
	FVector PlayerCameraRelativeLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Player Camera")
	FVector PlayerCameraStableLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Player Camera")
	FRotator PlayerCameraRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Player Camera", meta = (ClampMin = "30.0", ClampMax = "140.0"))
	float PlayerCameraFOV = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Player Camera", meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float MaxHeadLookPitch = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Player Camera", meta = (ClampMin = "0.0", ClampMax = "120.0"))
	float MaxHeadLookYaw = 65.0f;

	UPROPERTY(ReplicatedUsing = OnRep_ViewRotation, BlueprintReadOnly, Category = "ShowDown|Player Camera")
	FRotator ReplicatedPlayerViewRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Player Camera")
	float HeadLookPitch = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Player Camera")
	float HeadLookYaw = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Animation")
	TObjectPtr<UAnimationAsset> ShootAnimationAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Animation")
	TObjectPtr<UAnimationAsset> SelectCardAnimationAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Animation")
	TObjectPtr<UAnimationAsset> BettingAnimationAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Animation")
	FName ActionMontageSlotName = TEXT("DefaultSlot");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Animation", meta = (ClampMin = "0.0"))
	float ActionAnimationBlendInTime = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Animation", meta = (ClampMin = "0.0"))
	float ActionAnimationBlendOutTime = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Physics")
	FName RagdollHitBoneName = TEXT("Head");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Physics")
	float RagdollHitImpulseStrength = 65000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Physics")
	FVector RagdollHitLocalImpulseDirection = FVector(0.0f, -1.0f, 0.25f);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ShowDown|Hit Recovery")
	TObjectPtr<USpotLightComponent> HitResetPulseLight;

	/**
	 * Shared turn/loser key light. Its transform and lighting properties are
	 * authored on the component; gameplay code only toggles it on and off.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ShowDown|Cinematic Light")
	TObjectPtr<USpotLightComponent> RoundStatusSpotLight;

	/** Runtime override used only while this character is a roulette target. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Cinematic Light")
	FLinearColor RouletteTargetSpotLightColor = FLinearColor(1.0f, 0.015f, 0.015f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Hit Recovery", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "3.0"))
	float HitDownedHoldDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Hit Recovery", meta = (ClampMin = "0.1", UIMin = "0.1", UIMax = "2.0"))
	float HitResetPulseDuration = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Hit Recovery", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "2.0"))
	float HitRecoveryRevealDuration = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Hit Recovery")
	FLinearColor HitResetPulseColor = FLinearColor(0.32f, 0.85f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Hit Recovery", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "8000.0"))
	float HitResetPulsePeakIntensity = 8000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Hit Recovery", meta = (ClampMin = "50.0", UIMin = "50.0", UIMax = "1200.0"))
	float HitResetPulseRadius = 240.0f;

	UPROPERTY(ReplicatedUsing = OnRep_Identity, EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Identity")
	EShowDownCharacterRole CharacterRole = EShowDownCharacterRole::Unassigned;

	UPROPERTY(ReplicatedUsing = OnRep_Identity, EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Identity")
	EShowDownPlayerSlot PlayerSlot = EShowDownPlayerSlot::None;

	UPROPERTY(ReplicatedUsing = OnRep_Identity, EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Identity")
	FString CharacterDisplayName;

	UPROPERTY(ReplicatedUsing = OnRep_CharacterLives, BlueprintReadOnly, Category = "ShowDown|Name Tag")
	int32 CharacterLives = 3;

	UPROPERTY(ReplicatedUsing = OnRep_Identity, BlueprintReadOnly, Category = "ShowDown|Voice")
	bool bVoiceTalking = false;

	UPROPERTY(ReplicatedUsing = OnRep_SceneActive, BlueprintReadOnly, Category = "ShowDown|Character Visibility")
	bool bCharacterSceneActive = false;

	UPROPERTY(ReplicatedUsing = OnRep_BetStatusPresentation)
	FShowDownCharacterBetStatusPresentation ReplicatedBetStatusPresentation;

	UPROPERTY(ReplicatedUsing = OnRep_HitRecoveryPresentationState)
	FShowDownHitRecoveryPresentationState HitRecoveryPresentationState;

private:
	void ApplyCharacterSkin();
	void ApplyCharacterAnimState(EShowDownCharacterAnimState NewState);
	void FinishCharacterActionAnimIfCurrent(EShowDownCharacterAnimState FinishedState);
	void BindToRouletteEvents();
	void UnbindFromRouletteEvents();
	void ScheduleAnimStateReset(float Duration);
	float GetDefaultAnimDuration(EShowDownCharacterAnimState State) const;
	bool ShouldReactToSingleRouletteTarget(EShowDownSide Target) const;
	bool ShouldReactToMultiplayerRouletteTarget(EShowDownPlayerSlot TargetSlot) const;
	UAnimationAsset* GetAssignedActionAnimation(EShowDownCharacterAnimState State) const;
	bool CanPlayAssignedActionAnimation(const UAnimationAsset* AnimationAsset) const;
	bool TryPlayAssignedActionAnimation(EShowDownCharacterAnimState State);
	float GetAssignedActionAnimationDuration(EShowDownCharacterAnimState State) const;
	void LogAssignedActionAnimationFailure(EShowDownCharacterAnimState State, const UAnimationAsset* AnimationAsset) const;
	FName ResolveRagdollHitBoneName() const;
	FVector GetRagdollHitImpulseDirection() const;
	void StopRagdoll();
	void BeginLocalHitRecoveryPresentation();
	void UpdateHitRecoveryPresentation();
	void CompleteHitRecoveryPresentationAuthority();
	void SetHitRecoveryVisualConcealed(bool bConcealed);
	void SetHitRecoveryStatusConcealed(bool bConcealed);
	void SetHitResetPulseStrength(float Strength);
	float GetSynchronizedServerTimeSeconds() const;
	void StartWorldLifeLostPulse(int32 PreviousLives);
	void UpdateWorldLifeLostPulse(float DeltaSeconds);
	void ResetWorldLifeLostPulseVisual();
	void HandleCharacterLivesChanged(int32 PreviousLives);
	void CacheAnimBlueprintClass();
	void RestoreAnimBlueprintClass();
	void StartActionVisual(EShowDownCharacterAnimState State);
	void CacheBaseMeshTransform();
	void StopActionVisuals();
	void PushAnimStateToAnimInstance() const;
	void ApplyPlayerViewRotation(FRotator ViewRotation);
	void ApplyCharacterSceneActive();
	void ApplyPresentationCollisionSettings();
	void RefreshRoundStatusSpotlight();
	void RefreshNameTag();
	void SyncNameTagVisibility();
	void BindNameTagToLocalPlayer();
	void ApplyNameTagWidgetContent();
	void RefreshWorldLives();
	void RefreshWorldBetStatus();
	FString ResolveNameTagDisplayName() const;
	FString ResolveNameTagStatusText() const;
	bool IsNameTagTurnActive() const;
	bool ShouldShowNameTag() const;
	bool ShouldShowOverheadChatMessage(const FString& SenderName) const;

	EShowDownPlayerSlot LastPresentationLocalPlayerSlot = EShowDownPlayerSlot::None;
	bool bNameTagVisibilityInitialized = false;
	bool bLastNameTagVisible = false;
	bool bLoserSpotlightActive = false;
	bool bRoundStatusSpotLightTurnColorCached = false;
	FLinearColor RoundStatusSpotLightTurnColor = FLinearColor::White;

	FTimerHandle AnimStateResetTimerHandle;
	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> CachedAnimBlueprintClass;
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveActionMontage = nullptr;
	// Hard CDO references guarantee that all built-in skins are included in a
	// packaged build even when no optional catalog asset has been created.
	UPROPERTY()
	TObjectPtr<USkeletalMesh> BuiltInRobotMesh = nullptr;
	UPROPERTY()
	TObjectPtr<USkeletalMesh> BuiltInHoodmanMesh = nullptr;
	UPROPERTY()
	TObjectPtr<USkeletalMesh> BuiltInMicuMesh = nullptr;
	FVector BaseMeshRelativeLocation = FVector::ZeroVector;
	FRotator BaseMeshRelativeRotation = FRotator::ZeroRotator;
	FString AppliedCharacterSkinId;
	int32 LocalHitRecoverySequence = INDEX_NONE;
	bool bHitRecoveryVisualConcealed = false;
	bool bHitRecoveryStatusConcealed = false;
	bool bHitRecoveryRagdollReset = false;
	bool bHitRecoverySurvivorRevealed = false;
	bool bPendingSceneDeactivateAfterHitRecovery = false;
	bool bWorldLifeLostPulseActive = false;
	int32 WorldLifeLostDisplayedLives = INDEX_NONE;
	float WorldLifeLostPulseElapsedTime = 0.0f;
	FVector WorldLivesBaseRelativeScale = FVector::OneVector;
	bool bRagdollActive = false;
};
