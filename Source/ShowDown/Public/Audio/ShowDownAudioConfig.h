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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundBase> BackgroundMusicSound;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BackgroundMusicGunDuckMultiplier = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crowd", meta = (ClampMin = "0.0", UIMax = "1.0"))
	float CrowdIdleVolume = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crowd", meta = (ClampMin = "0.0", UIMax = "1.0"))
	float CrowdTensionVolume = 0.04f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crowd", meta = (ClampMin = "0.0", UIMax = "1.0"))
	float CrowdEmptyBoostVolume = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crowd", meta = (ClampMin = "0.0", UIMax = "1.0"))
	float CrowdLiveDuckVolume = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "One Shots", meta = (ClampMin = "0.0", UIMax = "2.0"))
	float CrowdShockedVolume = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "One Shots", meta = (ClampMin = "0.0", UIMax = "2.0"))
	float GunHitLayerVolume = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "One Shots", meta = (ClampMin = "0.0", UIMax = "2.0"))
	float SpotlightTransitionVolume = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "One Shots", meta = (ClampMin = "0.0", UIMax = "2.0"))
	float LoserSpotlightWarningVolume = 1.0f;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.0"))
	float MixTransitionDuration = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.0"))
	float MixRestoreDuration = 0.6f;
};
