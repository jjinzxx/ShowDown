#pragma once

#include "CoreMinimal.h"
#include "ShowDownTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ShowDownAudioSubsystem.generated.h"

class UAudioComponent;
class UShowDownAudioConfig;
class USoundBase;
class USoundWave;
struct FActorsInitializedParams;

enum class EShowDownMusicContext : uint8
{
	Menu,
	Match,
	Silent
};

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

	/** Restarts the non-overlapping spotlight transition one-shot. */
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Audio|Presentation")
	void NotifySpotlightChanged();

	/** Plays the dedicated warning cue for the red loser spotlight. */
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Audio|Presentation")
	void NotifyLoserSpotlightShown();

	/** Stops the warning cue on the exact trigger-pull completion frame. */
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Audio|Presentation")
	void StopLoserSpotlightWarning();

	/** Schedules the card-reveal cue against the authored reveal lead-in. */
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Audio|Presentation")
	void NotifyCardRevealStarted();

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
	friend class FShowDownAudioConfigTest;

	void HandlePostLoadMap(UWorld* LoadedWorld);
	void HandleWorldInitializedActors(const FActorsInitializedParams& Params);
	void EnsurePersistentLoops();
	void StartPersistentLoops(UWorld* World);
	void StopPersistentLoops();
	bool CanPlayInWorld(const UWorld* World) const;
	UWorld* ResolvePlaybackWorld() const;

	static float CalculateMusicTargetVolume(float ConfigVolume, float UserVolume, float MixMultiplier);
	float GetMusicTargetVolume() const;
	float GetMenuMusicTargetVolume() const;
	float GetCrowdTargetVolume() const;
	void SetCrowdMixVolume(float ConfigVolume, float FadeDuration);
	void SetMusicMixMultiplier(float Multiplier, float FadeDuration);
	void SetMusicContext(EShowDownMusicContext NewContext, float FadeDuration);
	void ApplyMusicContextVolumes(float FadeDuration);
	static void SetLoopComponentTargetVolume(UAudioComponent* Component, float TargetVolume, float FadeDuration);
	void RestoreIdleMix(float FadeDuration);
	void ScheduleMixRestore(float Delay);
	void ClearPresentationTimers(bool bClearCardRevealTimer = true);
	void ApplyButtonClickVolume();
	void ApplySpotlightTransitionVolume();
	void ApplyLoserSpotlightWarningVolume();
	void StopSpotlightTransitionSound();

	UFUNCTION()
	void HandleBackgroundMusicFinished();

	UFUNCTION()
	void HandleMenuMusicFinished();

	UFUNCTION()
	void HandleCrowdBedFinished();

	UFUNCTION()
	void HandleCrowdShockDelayElapsed();

	UFUNCTION()
	void HandleCardRevealDelayElapsed();

	UFUNCTION()
	void HandleMixRestoreDelayElapsed();

	UPROPERTY(Transient)
	TObjectPtr<UShowDownAudioConfig> AudioConfig;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> BackgroundMusicComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> MenuMusicComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> CrowdBedComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> SpotlightTransitionComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> LoserSpotlightWarningComponent;

	TWeakObjectPtr<UWorld> PlaybackWorld;
	FDelegateHandle PostLoadMapDelegateHandle;
	FDelegateHandle WorldInitializedActorsDelegateHandle;
	FTimerHandle CrowdShockTimerHandle;
	FTimerHandle CardRevealTimerHandle;
	FTimerHandle MixRestoreTimerHandle;

	float UserMusicVolume = 1.0f;
	float UserEffectVolume = 1.0f;
	float CurrentCrowdConfigVolume = 0.0f;
	float CurrentMusicMixMultiplier = 1.0f;
	uint64 LastSpotlightTransitionFrame = MAX_uint64;
	bool bCrowdBedEnabled = false;
	EShowDownMusicContext MusicContext = EShowDownMusicContext::Menu;
};
