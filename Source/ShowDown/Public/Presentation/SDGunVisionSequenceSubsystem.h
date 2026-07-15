#pragma once

#include "CoreMinimal.h"
#include "Presentation/SDVisionDirector.h"
#include "ShowDownTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "SDGunVisionSequenceSubsystem.generated.h"

class AActor;
class ASpotLight;
class ASDSelfShotGunActor;
class AShowDownGameStateBase;
class USDGunVisionSequenceSubsystem;

/**
 * Per-gun delegate proxy. The gun delegates intentionally have no sender
 * parameter, so a small proxy preserves which gun started each presentation.
 */
UCLASS()
class SHOWDOWN_API USDGunVisionGunBinding final : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(USDGunVisionSequenceSubsystem* InOwner, ASDSelfShotGunActor* InGun);
	void Shutdown();
	ASDSelfShotGunActor* GetGun() const { return Gun.Get(); }

private:
	UFUNCTION()
	void HandleGunRaised();

	UFUNCTION()
	void HandleGunFired();

	UFUNCTION()
	void HandleGunEmptyFired();

	UFUNCTION()
	void HandleGunPresentationFinished();

	UFUNCTION()
	void HandleGunDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void HandleGunEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason);

	TWeakObjectPtr<USDGunVisionSequenceSubsystem> Owner;
	TWeakObjectPtr<ASDSelfShotGunActor> Gun;
};

/**
 * Automatically connects every local gameplay world's gun presentation to its
 * VisionDirector. It never binds OnPresentationStarted because listeners on
 * that delegate change the phase presentation ownership contract.
 */
UCLASS()
class SHOWDOWN_API USDGunVisionSequenceSubsystem final : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Starts at the authored table range and stays bright until the initial deal finishes. */
	void QueueMatchEntryPresentation();

	/** Restores a clear hub view so another match can enter cleanly in the same world. */
	void ResetMatchPresentationForHub();

private:
	friend class USDGunVisionGunBinding;

	enum class ESequenceState : uint8
	{
		Idle,
		RaiseToTension,
		RampToPeak,
		LivePeakHold,
		LiveAftermath,
		EmptyPeakHold,
		AwaitingFinish
	};

	enum class EIntroSequenceState : uint8
	{
		Idle,
		WaitingForBeat,
		Collapsing
	};

	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

	void RefreshExistingBindings();
	void HandleActorSpawned(AActor* SpawnedActor);
	void RegisterVisionDirector(ASDVisionDirector* VisionDirector);
	void BindGun(ASDSelfShotGunActor* GunActor);
	void BindGameState(AShowDownGameStateBase* GameState);
	bool HasLocalPresentationView() const;
	void SynchronizePendingVisionDirectors();
	void StartMatchIntro();
	void AdvanceIntroSequence(float DeltaTime);
	void SetVisionRangeImmediateToTable();
	void SetVisionRangeImmediateToIntroWide();
	void BlendVisionRangeToTable(float Duration, ESDVisionBlendEase EaseMode);
	void SetDarknessImmediate(float Strength);
	void BlendDarkness(
		float TargetStrength,
		float Duration,
		ESDVisionBlendEase EaseMode,
		float EaseExponent = 2.0f);
	ASpotLight* ResolveTableSpotlight();
	ASpotLight* ResolveZeroDarknessSpotlight();
	bool SetTableSpotlightEnabled(bool bEnabled);
	bool SetZeroDarknessSpotlightEnabled(bool bEnabled);
	void PlaySpotlightTransitionSound() const;
	void RefreshTurnSpotlightSoundState();
	void ApplyPhasePresentationPolicy(EShowDownPhase Phase);
	bool IsRoulettePhase() const;
	void ResetToIdle(bool bImmediate);

	void HandleGunRaised(ASDSelfShotGunActor* GunActor);
	void HandleGunFired(ASDSelfShotGunActor* GunActor);
	void HandleGunEmptyFired(ASDSelfShotGunActor* GunActor);
	void HandleGunPresentationFinished(ASDSelfShotGunActor* GunActor);
	void HandleGunUnavailable(ASDSelfShotGunActor* GunActor);

	UFUNCTION()
	void HandlePhaseChanged(EShowDownPhase NewPhase);

	UFUNCTION()
	void HandleTableCinematicCue(ESDTableCinematicCue Cue, uint8 PlayerSlotMask);

	UFUNCTION()
	void HandleNameTagRoundStatusChanged();

	UPROPERTY(Transient)
	TArray<TObjectPtr<USDGunVisionGunBinding>> GunBindings;

	TArray<TWeakObjectPtr<ASDVisionDirector>> VisionDirectors;
	TArray<TWeakObjectPtr<ASDVisionDirector>> PendingVisionDirectorSync;
	TWeakObjectPtr<AShowDownGameStateBase> BoundGameState;
	TWeakObjectPtr<ASDSelfShotGunActor> ActiveGun;
	TWeakObjectPtr<ASpotLight> TableSpotlight;
	TWeakObjectPtr<ASpotLight> ZeroDarknessSpotlight;
	FDelegateHandle ActorSpawnedDelegateHandle;

	ESequenceState SequenceState = ESequenceState::Idle;
	EIntroSequenceState IntroSequenceState = EIntroSequenceState::Idle;
	float IntroSequenceElapsedTime = 0.0f;
	float SequenceElapsedTime = 0.0f;
	float SequenceStageDuration = 0.0f;
	float ShotResolveDelay = 0.0f;
	float PostShotBrightHoldElapsedTime = 0.0f;
	float DesiredDarknessStrength = 0.0f;
	uint8 ActiveTargetSpotlightMask = 0;
	EShowDownSide LastTurnSpotlightSide = EShowDownSide::Player;
	EShowDownPlayerSlot LastTurnSpotlightSlot = EShowDownPlayerSlot::None;
	bool bMatchPresentationActivated = false;
	bool bWorldHasBegunPlay = false;
	bool bTableSpotlightEnabled = false;
	bool bZeroDarknessSpotlightEnabled = false;
	bool bTurnSpotlightStateInitialized = false;
	bool bLastTurnSpotlightVisible = false;
	bool bPostShotBrightHoldActive = false;
	bool bInitialDealPresentationActive = false;
};
