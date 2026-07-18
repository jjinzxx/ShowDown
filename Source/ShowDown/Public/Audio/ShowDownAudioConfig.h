#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ShowDownAudioConfig.generated.h"

class USoundBase;
class USoundWave;

/**
 * All replaceable ShowDown music and sound-effect assets live here so the
 * presentation code never needs to reference a specific imported sound.
 */
UCLASS(BlueprintType)
class SHOWDOWN_API UShowDownAudioConfig final : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundBase> CrowdBedSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundBase> CrowdShockedSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundBase> GunHitLayerSound;

	/** Gameplay loop used from match start until the game-over presentation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundBase> BackgroundMusicSound;

	/** Front-end loop used by login, main-menu and hub presentation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundBase> MenuMusicSound;

	/** One-shot synchronized with the first card beginning its reveal motion. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundBase> CardRevealSound;

	/** One-shot used whenever one or more presentation spotlights change state. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundBase> SpotlightTransitionSound;

	/** One-shot played when the red loser spotlight first appears. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundBase> LoserSpotlightWarningSound;

	// Slate button styles require a SoundWave resource rather than an arbitrary
	// SoundBase, so UI click audio is deliberately typed more narrowly.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundWave> ButtonClickSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music", meta = (ClampMin = "0.0", UIMax = "1.0"))
	float BackgroundMusicVolume = 0.28f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music", meta = (ClampMin = "0.0", UIMax = "1.0"))
	float MenuMusicVolume = 0.26f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BackgroundMusicGunDuckMultiplier = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crowd", meta = (ClampMin = "0.0", UIMax = "1.0"))
	float CrowdIdleVolume = 0.16f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crowd", meta = (ClampMin = "0.0", UIMax = "1.0"))
	float CrowdTensionVolume = 0.07f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crowd", meta = (ClampMin = "0.0", UIMax = "1.0"))
	float CrowdEmptyBoostVolume = 0.68f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crowd", meta = (ClampMin = "0.0", UIMax = "1.0"))
	float CrowdLiveDuckVolume = 0.035f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "One Shots", meta = (ClampMin = "0.0", UIMax = "2.0"))
	float CrowdShockedVolume = 1.10f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "One Shots", meta = (ClampMin = "0.0", UIMax = "2.0"))
	float GunHitLayerVolume = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "One Shots", meta = (ClampMin = "0.0", UIMax = "2.0"))
	float SpotlightTransitionVolume = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "One Shots", meta = (ClampMin = "0.0", UIMax = "2.0"))
	float LoserSpotlightWarningVolume = 0.80f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "One Shots", meta = (ClampMin = "0.0", UIMax = "2.0"))
	float CardRevealVolume = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (ClampMin = "0.0", UIMax = "2.0"))
	float ButtonClickVolume = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.0"))
	float CrowdShockDelay = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.0"))
	float EmptyCrowdRestoreDelay = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.0"))
	float LiveCrowdRestoreDelay = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.0"))
	float LoopFadeInDuration = 0.5f;

	/** Crossfade time when moving between the menu and gameplay loops. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.0"))
	float MusicTransitionDuration = 0.75f;

	/** RevealStarted precedes the first authored card movement by this amount. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.0"))
	float CardRevealSoundDelay = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.0"))
	float MixTransitionDuration = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.0"))
	float MixRestoreDuration = 0.6f;
};
