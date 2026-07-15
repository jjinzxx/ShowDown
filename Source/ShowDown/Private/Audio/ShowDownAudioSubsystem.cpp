#include "Audio/ShowDownAudioSubsystem.h"

#include "Audio/ShowDownAudioConfig.h"
#include "AudioDevice.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	constexpr TCHAR AudioConfigAssetPath[] =
		TEXT("/Game/Audio/DA_ShowDownAudioConfig.DA_ShowDownAudioConfig");
	constexpr TCHAR UserSettingsSection[] = TEXT("ShowDown.UserSettings");

	float ClampUserVolume(float Volume)
	{
		return FMath::Clamp(Volume, 0.0f, 1.0f);
	}
}

void UShowDownAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	AudioConfig = LoadObject<UShowDownAudioConfig>(nullptr, AudioConfigAssetPath);
	if (!AudioConfig)
	{
		UE_LOG(LogTemp, Warning, TEXT("ShowDown audio config was not found at %s."), AudioConfigAssetPath);
	}
	else
	{
		// PIE can initialize this subsystem before its world permits audio playback.
		// Keep a usable crowd target ready for the later world-ready callback or
		// lazy-start path instead of leaving the loop permanently at zero.
		CurrentCrowdConfigVolume = FMath::Max(0.0f, AudioConfig->CrowdIdleVolume);
	}

	RefreshUserVolumes();
	PostLoadMapDelegateHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this,
		&UShowDownAudioSubsystem::HandlePostLoadMap);
	WorldInitializedActorsDelegateHandle = FWorldDelegates::OnWorldInitializedActors.AddUObject(
		this,
		&UShowDownAudioSubsystem::HandleWorldInitializedActors);

	if (UWorld* World = GetWorld())
	{
		HandlePostLoadMap(World);
	}
}

void UShowDownAudioSubsystem::Deinitialize()
{
	if (PostLoadMapDelegateHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapDelegateHandle);
		PostLoadMapDelegateHandle.Reset();
	}
	if (WorldInitializedActorsDelegateHandle.IsValid())
	{
		FWorldDelegates::OnWorldInitializedActors.Remove(WorldInitializedActorsDelegateHandle);
		WorldInitializedActorsDelegateHandle.Reset();
	}

	ClearPresentationTimers();
	StopSpotlightTransitionSound();
	StopPersistentLoops();
	PlaybackWorld.Reset();
	AudioConfig = nullptr;

	Super::Deinitialize();
}

USoundWave* UShowDownAudioSubsystem::GetButtonClickSound() const
{
	return AudioConfig ? AudioConfig->ButtonClickSound.Get() : nullptr;
}

void UShowDownAudioSubsystem::NotifyGunRaised()
{
	EnsurePersistentLoops();
	if (!AudioConfig || !CanPlayInWorld(ResolvePlaybackWorld()))
	{
		return;
	}

	ClearPresentationTimers();
	SetCrowdMixVolume(AudioConfig->CrowdTensionVolume, AudioConfig->MixTransitionDuration);
	SetMusicMixMultiplier(
		AudioConfig->BackgroundMusicGunDuckMultiplier,
		AudioConfig->MixTransitionDuration);
}

void UShowDownAudioSubsystem::NotifyGunFired()
{
	EnsurePersistentLoops();
	UWorld* World = ResolvePlaybackWorld();
	if (!AudioConfig || !CanPlayInWorld(World))
	{
		return;
	}

	ClearPresentationTimers();
	SetCrowdMixVolume(AudioConfig->CrowdLiveDuckVolume, AudioConfig->MixTransitionDuration);
	SetMusicMixMultiplier(
		AudioConfig->BackgroundMusicGunDuckMultiplier,
		AudioConfig->MixTransitionDuration);

	if (AudioConfig->GunHitLayerSound)
	{
		UGameplayStatics::PlaySound2D(
			World,
			AudioConfig->GunHitLayerSound,
			FMath::Max(0.0f, AudioConfig->GunHitLayerVolume) * UserEffectVolume);
	}

	const float ShockDelay = FMath::Max(0.0f, AudioConfig->CrowdShockDelay);
	if (ShockDelay <= KINDA_SMALL_NUMBER)
	{
		HandleCrowdShockDelayElapsed();
	}
	else
	{
		World->GetTimerManager().SetTimer(
			CrowdShockTimerHandle,
			this,
			&UShowDownAudioSubsystem::HandleCrowdShockDelayElapsed,
			ShockDelay,
			false);
	}

	ScheduleMixRestore(AudioConfig->LiveCrowdRestoreDelay);
}

void UShowDownAudioSubsystem::NotifyGunEmptyFired()
{
	EnsurePersistentLoops();
	if (!AudioConfig || !CanPlayInWorld(ResolvePlaybackWorld()))
	{
		return;
	}

	ClearPresentationTimers();
	SetCrowdMixVolume(AudioConfig->CrowdEmptyBoostVolume, AudioConfig->MixTransitionDuration);
	SetMusicMixMultiplier(
		AudioConfig->BackgroundMusicGunDuckMultiplier,
		AudioConfig->MixTransitionDuration);
	ScheduleMixRestore(AudioConfig->EmptyCrowdRestoreDelay);
}

void UShowDownAudioSubsystem::NotifyGunPresentationFinished()
{
	EnsurePersistentLoops();
	if (!AudioConfig)
	{
		return;
	}

	// The crowd result envelope has its own authored duration. Let it finish,
	// but release the music as soon as the gun presentation is over.
	SetMusicMixMultiplier(1.0f, AudioConfig->MixRestoreDuration);
	UWorld* World = ResolvePlaybackWorld();
	if (!World || !World->GetTimerManager().IsTimerActive(MixRestoreTimerHandle))
	{
		SetCrowdMixVolume(AudioConfig->CrowdIdleVolume, AudioConfig->MixRestoreDuration);
	}
}

void UShowDownAudioSubsystem::NotifySpotlightChanged()
{
	EnsurePersistentLoops();
	UWorld* World = ResolvePlaybackWorld();
	if (!AudioConfig || !CanPlayInWorld(World))
	{
		return;
	}

	// Multiple characters can update their lights in one presentation step.
	// Treat that batch as one audible transition, while still allowing a later
	// frame to interrupt and restart the long one-shot from the beginning.
	if (LastSpotlightTransitionFrame == GFrameCounter)
	{
		return;
	}

	USoundBase* SpotlightSound = AudioConfig->SpotlightTransitionSound.Get();
	if (!SpotlightSound)
	{
		return;
	}
	LastSpotlightTransitionFrame = GFrameCounter;

	if (IsValid(SpotlightTransitionComponent)
		&& SpotlightTransitionComponent->GetWorld() != World)
	{
		StopSpotlightTransitionSound();
		// Preserve the duplicate guard for the current transition after cleanup.
		LastSpotlightTransitionFrame = GFrameCounter;
	}

	if (!IsValid(SpotlightTransitionComponent))
	{
		SpotlightTransitionComponent = UGameplayStatics::CreateSound2D(
			World,
			SpotlightSound,
			1.0f,
			1.0f,
			0.0f,
			nullptr,
			false,
			false);
	}

	if (!SpotlightTransitionComponent)
	{
		return;
	}

	SpotlightTransitionComponent->Stop();
	SpotlightTransitionComponent->SetSound(SpotlightSound);
	ApplySpotlightTransitionVolume();
	SpotlightTransitionComponent->Play(0.0f);
}

void UShowDownAudioSubsystem::NotifyPhaseChanged(EShowDownPhase NewPhase)
{
	EnsurePersistentLoops();
	if (!AudioConfig || NewPhase == EShowDownPhase::Roulette)
	{
		return;
	}

	ClearPresentationTimers();
	RestoreIdleMix(AudioConfig->MixRestoreDuration);
}

void UShowDownAudioSubsystem::SetCrowdBedEnabled(bool bEnabled, float FadeDuration)
{
	EnsurePersistentLoops();
	bCrowdBedEnabled = bEnabled;
	SetCrowdMixVolume(CurrentCrowdConfigVolume, FMath::Max(0.0f, FadeDuration));
}

void UShowDownAudioSubsystem::SetUserMusicVolume(float Volume)
{
	UserMusicVolume = ClampUserVolume(Volume);
	EnsurePersistentLoops();
	SetMusicMixMultiplier(CurrentMusicMixMultiplier, 0.0f);
}

void UShowDownAudioSubsystem::SetUserEffectVolume(float Volume)
{
	UserEffectVolume = ClampUserVolume(Volume);
	EnsurePersistentLoops();
	SetCrowdMixVolume(CurrentCrowdConfigVolume, 0.0f);
	ApplyButtonClickVolume();
	ApplySpotlightTransitionVolume();
}

void UShowDownAudioSubsystem::RefreshUserVolumes()
{
	float MasterVolume = 1.0f;
	float MusicVolume = UserMusicVolume;
	float EffectVolume = UserEffectVolume;
	if (GConfig)
	{
		GConfig->GetFloat(UserSettingsSection, TEXT("MasterVolume"), MasterVolume, GGameUserSettingsIni);
		GConfig->GetFloat(UserSettingsSection, TEXT("MusicVolume"), MusicVolume, GGameUserSettingsIni);
		GConfig->GetFloat(UserSettingsSection, TEXT("EffectVolume"), EffectVolume, GGameUserSettingsIni);
	}

	UserMusicVolume = ClampUserVolume(MusicVolume);
	UserEffectVolume = ClampUserVolume(EffectVolume);
	if (UWorld* World = ResolvePlaybackWorld())
	{
		FAudioDeviceHandle AudioDevice = World->GetAudioDevice();
		if (AudioDevice.IsValid())
		{
			AudioDevice->SetTransientPrimaryVolume(ClampUserVolume(MasterVolume));
		}
	}
	SetMusicMixMultiplier(CurrentMusicMixMultiplier, 0.0f);
	SetCrowdMixVolume(CurrentCrowdConfigVolume, 0.0f);
	ApplyButtonClickVolume();
	ApplySpotlightTransitionVolume();
}

void UShowDownAudioSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!CanPlayInWorld(LoadedWorld))
	{
		return;
	}

	PlaybackWorld = LoadedWorld;
	CrowdShockTimerHandle.Invalidate();
	MixRestoreTimerHandle.Invalidate();
	RefreshUserVolumes();
	StartPersistentLoops(LoadedWorld);
	if (AudioConfig)
	{
		RestoreIdleMix(AudioConfig->LoopFadeInDuration);
	}
}

void UShowDownAudioSubsystem::HandleWorldInitializedActors(const FActorsInitializedParams& Params)
{
	// PIE duplicates an editor world instead of loading a map, so
	// PostLoadMapWithWorld is not guaranteed to run after audio becomes valid.
	HandlePostLoadMap(Params.World);
}

void UShowDownAudioSubsystem::EnsurePersistentLoops()
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!CanPlayInWorld(World))
	{
		World = ResolvePlaybackWorld();
	}
	if (!CanPlayInWorld(World))
	{
		return;
	}

	PlaybackWorld = World;
	StartPersistentLoops(World);
}

void UShowDownAudioSubsystem::StartPersistentLoops(UWorld* World)
{
	if (!AudioConfig || !CanPlayInWorld(World))
	{
		return;
	}

	if (!IsValid(BackgroundMusicComponent) && AudioConfig->BackgroundMusicSound)
	{
		// Keep the component multiplier at full scale. FadeIn's level is
		// multiplied by this value, so spawning at zero makes the loop
		// permanently silent even after a non-zero fade target is applied.
		BackgroundMusicComponent = UGameplayStatics::CreateSound2D(
			World,
			AudioConfig->BackgroundMusicSound,
			1.0f,
			1.0f,
			0.0f,
			nullptr,
			true,
			false);
		if (BackgroundMusicComponent)
		{
			BackgroundMusicComponent->OnAudioFinished.AddUniqueDynamic(
				this,
				&UShowDownAudioSubsystem::HandleBackgroundMusicFinished);
			BackgroundMusicComponent->FadeIn(
				FMath::Max(0.0f, AudioConfig->LoopFadeInDuration),
				FMath::Max(0.0f, AudioConfig->BackgroundMusicVolume) * UserMusicVolume);
		}
	}

	if (!IsValid(CrowdBedComponent) && AudioConfig->CrowdBedSound)
	{
		CrowdBedComponent = UGameplayStatics::CreateSound2D(
			World,
			AudioConfig->CrowdBedSound,
			1.0f,
			1.0f,
			0.0f,
			nullptr,
			true,
			false);
		if (CrowdBedComponent)
		{
			CrowdBedComponent->OnAudioFinished.AddUniqueDynamic(
				this,
				&UShowDownAudioSubsystem::HandleCrowdBedFinished);
			if (bCrowdBedEnabled)
			{
				CrowdBedComponent->FadeIn(
					FMath::Max(0.0f, AudioConfig->LoopFadeInDuration),
					FMath::Max(0.0f, AudioConfig->CrowdIdleVolume) * UserEffectVolume);
			}
		}
	}
}

void UShowDownAudioSubsystem::StopPersistentLoops()
{
	if (BackgroundMusicComponent)
	{
		BackgroundMusicComponent->OnAudioFinished.RemoveDynamic(
			this,
			&UShowDownAudioSubsystem::HandleBackgroundMusicFinished);
		BackgroundMusicComponent->Stop();
		BackgroundMusicComponent->DestroyComponent();
		BackgroundMusicComponent = nullptr;
	}

	if (CrowdBedComponent)
	{
		CrowdBedComponent->OnAudioFinished.RemoveDynamic(
			this,
			&UShowDownAudioSubsystem::HandleCrowdBedFinished);
		CrowdBedComponent->Stop();
		CrowdBedComponent->DestroyComponent();
		CrowdBedComponent = nullptr;
	}
}

bool UShowDownAudioSubsystem::CanPlayInWorld(const UWorld* World) const
{
	return !IsRunningCommandlet()
		&& World
		&& World->IsGameWorld()
		&& World->bAllowAudioPlayback
		&& World->GetGameInstance() == GetGameInstance()
		&& World->GetNetMode() != NM_DedicatedServer;
}

UWorld* UShowDownAudioSubsystem::ResolvePlaybackWorld() const
{
	if (UWorld* World = PlaybackWorld.Get())
	{
		return World;
	}
	return GetWorld();
}

void UShowDownAudioSubsystem::SetCrowdMixVolume(float ConfigVolume, float FadeDuration)
{
	CurrentCrowdConfigVolume = FMath::Max(0.0f, ConfigVolume);
	if (!CrowdBedComponent)
	{
		return;
	}

	if (!bCrowdBedEnabled)
	{
		if (CrowdBedComponent->IsPlaying())
		{
			CrowdBedComponent->FadeOut(FMath::Max(0.0f, FadeDuration), 0.0f);
		}
		return;
	}

	const float TargetVolume = CurrentCrowdConfigVolume * UserEffectVolume;
	if (!CrowdBedComponent->IsPlaying())
	{
		CrowdBedComponent->FadeIn(FMath::Max(0.0f, FadeDuration), TargetVolume);
		return;
	}

	CrowdBedComponent->AdjustVolume(
		FMath::Max(0.0f, FadeDuration),
		TargetVolume);
}

void UShowDownAudioSubsystem::SetMusicMixMultiplier(float Multiplier, float FadeDuration)
{
	CurrentMusicMixMultiplier = FMath::Max(0.0f, Multiplier);
	if (!BackgroundMusicComponent || !AudioConfig)
	{
		return;
	}

	BackgroundMusicComponent->AdjustVolume(
		FMath::Max(0.0f, FadeDuration),
		FMath::Max(0.0f, AudioConfig->BackgroundMusicVolume)
			* UserMusicVolume
			* CurrentMusicMixMultiplier);
}

void UShowDownAudioSubsystem::RestoreIdleMix(float FadeDuration)
{
	if (!AudioConfig)
	{
		return;
	}

	SetCrowdMixVolume(AudioConfig->CrowdIdleVolume, FadeDuration);
	SetMusicMixMultiplier(1.0f, FadeDuration);
}

void UShowDownAudioSubsystem::ScheduleMixRestore(float Delay)
{
	UWorld* World = ResolvePlaybackWorld();
	if (!World)
	{
		return;
	}

	const float SafeDelay = FMath::Max(0.0f, Delay);
	if (SafeDelay <= KINDA_SMALL_NUMBER)
	{
		HandleMixRestoreDelayElapsed();
		return;
	}

	World->GetTimerManager().SetTimer(
		MixRestoreTimerHandle,
		this,
		&UShowDownAudioSubsystem::HandleMixRestoreDelayElapsed,
		SafeDelay,
		false);
}

void UShowDownAudioSubsystem::ClearPresentationTimers()
{
	if (UWorld* World = ResolvePlaybackWorld())
	{
		World->GetTimerManager().ClearTimer(CrowdShockTimerHandle);
		World->GetTimerManager().ClearTimer(MixRestoreTimerHandle);
	}
	CrowdShockTimerHandle.Invalidate();
	MixRestoreTimerHandle.Invalidate();
}

void UShowDownAudioSubsystem::ApplyButtonClickVolume()
{
	if (AudioConfig && AudioConfig->ButtonClickSound)
	{
		AudioConfig->ButtonClickSound->Volume =
			FMath::Max(0.0f, AudioConfig->ButtonClickVolume) * UserEffectVolume;
	}
}

void UShowDownAudioSubsystem::ApplySpotlightTransitionVolume()
{
	if (SpotlightTransitionComponent && AudioConfig)
	{
		SpotlightTransitionComponent->SetVolumeMultiplier(
			FMath::Max(0.0f, AudioConfig->SpotlightTransitionVolume) * UserEffectVolume);
	}
}

void UShowDownAudioSubsystem::StopSpotlightTransitionSound()
{
	if (IsValid(SpotlightTransitionComponent))
	{
		SpotlightTransitionComponent->Stop();
		SpotlightTransitionComponent->DestroyComponent();
	}
	SpotlightTransitionComponent = nullptr;
	LastSpotlightTransitionFrame = MAX_uint64;
}

void UShowDownAudioSubsystem::HandleBackgroundMusicFinished()
{
	if (BackgroundMusicComponent && AudioConfig && AudioConfig->BackgroundMusicSound)
	{
		BackgroundMusicComponent->Play(0.0f);
	}
}

void UShowDownAudioSubsystem::HandleCrowdBedFinished()
{
	if (bCrowdBedEnabled && CrowdBedComponent && AudioConfig && AudioConfig->CrowdBedSound)
	{
		CrowdBedComponent->FadeIn(
			0.0f,
			CurrentCrowdConfigVolume * UserEffectVolume,
			0.0f);
	}
}

void UShowDownAudioSubsystem::HandleCrowdShockDelayElapsed()
{
	CrowdShockTimerHandle.Invalidate();
	UWorld* World = ResolvePlaybackWorld();
	if (!AudioConfig || !CanPlayInWorld(World) || !AudioConfig->CrowdShockedSound)
	{
		return;
	}

	UGameplayStatics::PlaySound2D(
		World,
		AudioConfig->CrowdShockedSound,
		FMath::Max(0.0f, AudioConfig->CrowdShockedVolume) * UserEffectVolume);
}

void UShowDownAudioSubsystem::HandleMixRestoreDelayElapsed()
{
	MixRestoreTimerHandle.Invalidate();
	if (AudioConfig)
	{
		RestoreIdleMix(AudioConfig->MixRestoreDuration);
	}
}
