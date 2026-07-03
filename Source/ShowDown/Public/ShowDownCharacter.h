#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ShowDownCharacterAnimTypes.h"
#include "ShowDownTypes.h"
#include "TimerManager.h"
#include "ShowDownCharacter.generated.h"

class ASDPlayerState;
class UAnimationAsset;
class UAnimInstance;
class UShowDownCharacterAnimInstance;

UCLASS(Blueprintable)
class SHOWDOWN_API AShowDownCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AShowDownCharacter();

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

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Character Physics")
	void StartHitRagdoll();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Character Animation")
	void ResetCharacterAnimState();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Character Animation")
	void FinishCurrentCharacterActionAnim();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Character Animation")
	void NotifyCharacterActionAnimFinished(EShowDownCharacterAnimState FinishedState);

	UFUNCTION(BlueprintPure, Category = "ShowDown|Character Animation")
	EShowDownCharacterAnimState GetCharacterAnimState() const { return ReplicatedAnimState; }

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

	UFUNCTION(BlueprintImplementableEvent, Category = "ShowDown|Character Identity")
	void OnCharacterIdentityChanged();

	UFUNCTION(BlueprintImplementableEvent, Category = "ShowDown|Character Animation")
	void OnCharacterAnimStateChanged(EShowDownCharacterAnimState NewState);

protected:
	UFUNCTION()
	void OnRep_AnimState();

	UFUNCTION()
	void OnRep_Identity();

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
	void HandleMultiplayerRouletteStarted(EShowDownPlayerSlot TargetSlot, const FString& TargetName, int32 BulletCount);

	UFUNCTION()
	void HandleMultiplayerRouletteResult(EShowDownPlayerSlot TargetSlot, const FString& TargetName, int32 BulletCount, bool bHit, int32 RemainingLives);

	UFUNCTION(Server, Reliable)
	void ServerSetCharacterIdentity(EShowDownCharacterRole NewRole, EShowDownPlayerSlot NewPlayerSlot, const FString& NewDisplayName);

	UPROPERTY(ReplicatedUsing = OnRep_AnimState, BlueprintReadOnly, Category = "ShowDown|Character Animation")
	EShowDownCharacterAnimState ReplicatedAnimState = EShowDownCharacterAnimState::Idle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Animation")
	TObjectPtr<UAnimationAsset> ShootAnimationAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Physics")
	FName RagdollHitBoneName = TEXT("Head");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Physics")
	float RagdollHitImpulseStrength = 65000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Physics")
	FVector RagdollHitLocalImpulseDirection = FVector(0.0f, -1.0f, 0.25f);

	UPROPERTY(ReplicatedUsing = OnRep_Identity, EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Identity")
	EShowDownCharacterRole CharacterRole = EShowDownCharacterRole::Unassigned;

	UPROPERTY(ReplicatedUsing = OnRep_Identity, EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Identity")
	EShowDownPlayerSlot PlayerSlot = EShowDownPlayerSlot::None;

	UPROPERTY(ReplicatedUsing = OnRep_Identity, EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Identity")
	FString CharacterDisplayName;

private:
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
	void CacheAnimBlueprintClass();
	void RestoreAnimBlueprintClass();
	void StartActionVisual(EShowDownCharacterAnimState State);
	void CacheBaseMeshTransform();
	void StopActionVisuals();
	void PushAnimStateToAnimInstance() const;

	FTimerHandle AnimStateResetTimerHandle;
	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> CachedAnimBlueprintClass;
	FVector BaseMeshRelativeLocation = FVector::ZeroVector;
	FRotator BaseMeshRelativeRotation = FRotator::ZeroRotator;
	bool bRagdollActive = false;
};
