#pragma once

#include "CoreMinimal.h"
#include "ShowDownTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ShowDownAudioSubsystem.generated.h"

class UAudioComponent;
class UShowDownAudioConfig;
class USoundBase;
class USoundWave;

/**
 * Persistent local audio director for music, the crowd bed and presentation
 * one-shots. Gameplay actors only report semantic events; all sound choices
 * and mix values remain in DA_ShowDownAudioConfig.
 */
UCLASS()
class SHOWDOWN_API UShowDownAudioSubsystem final : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "ShowDown|Audio")
	USoundWave* GetButtonClickSound() const;

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Audio|Presentation")
	void NotifyGunRaised();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Audio|Presentation")
	void NotifyGunFired();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Audio|Presentation")
	void NotifyGunEmptyFired();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Audio|Presentation")
	void NotifyGunPresentationFinished();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Audio|Presentation")
	void NotifyPhaseChanged(EShowDownPhase NewPhase);

	/** Enables the ambient crowd bed only while the local player is in a match. */
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Audio|Crowd")
	void SetCrowdBedEnabled(bool bEnabled, float FadeDuration = 0.35f);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Audio|Settings")
	void SetUserMusicVolume(float Volume);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Audio|Settings")
	void SetUserEffectVolume(float Volume);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Audio|Settings")
	void RefreshUserVolumes();

	UFUNCTION(BlueprintPure, Category = "ShowDown|Audio")
	const UShowDownAudioConfig* GetAudioConfig() const { return AudioConfig; }

private:
	void HandlePostLoadMap(UWorld* LoadedWorld);
	void StartPersistentLoops(UWorld* World);
	void StopPersistentLoops();
	bool CanPlayInWorld(const UWorld* World) const;
	UWorld* ResolvePlaybackWorld() const;

	void SetCrowdMixVolume(float ConfigVolume, float FadeDuration);
	void SetMusicMixMultiplier(float Multiplier, float FadeDuration);
	void RestoreIdleMix(float FadeDuration);
	void ScheduleMixRestore(float Delay);
	void ClearPresentationTimers();
	void ApplyButtonClickVolume();

	UFUNCTION()
	void HandleBackgroundMusicFinished();

	UFUNCTION()
	void HandleCrowdBedFinished();

	UFUNCTION()
	void HandleCrowdShockDelayElapsed();

	UFUNCTION()
	void HandleMixRestoreDelayElapsed();

	UPROPERTY(Transient)
	TObjectPtr<UShowDownAudioConfig> AudioConfig;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> BackgroundMusicComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> CrowdBedComponent;

	TWeakObjectPtr<UWorld> PlaybackWorld;
	FDelegateHandle PostLoadMapDelegateHandle;
	FTimerHandle CrowdShockTimerHandle;
	FTimerHandle MixRestoreTimerHandle;

	float UserMusicVolume = 1.0f;
	float UserEffectVolume = 1.0f;
	float CurrentCrowdConfigVolume = 0.0f;
	float CurrentMusicMixMultiplier = 1.0f;
	bool bCrowdBedEnabled = false;
};
