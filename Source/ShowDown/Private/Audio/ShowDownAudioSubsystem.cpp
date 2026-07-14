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

	RefreshUserVolumes();
	PostLoadMapDelegateHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this,
		&UShowDownAudioSubsystem::HandlePostLoadMap);

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

	ClearPresentationTimers();
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

void UShowDownAudioSubsystem::NotifyPhaseChanged(EShowDownPhase NewPhase)
{
	if (!AudioConfig || NewPhase == EShowDownPhase::Roulette)
	{
		return;
	}

	ClearPresentationTimers();
	RestoreIdleMix(AudioConfig->MixRestoreDuration);
}

void UShowDownAudioSubsystem::SetUserMusicVolume(float Volume)
{
	UserMusicVolume = ClampUserVolume(Volume);
	SetMusicMixMultiplier(CurrentMusicMixMultiplier, 0.0f);
}

void UShowDownAudioSubsystem::SetUserEffectVolume(float Volume)
{
	UserEffectVolume = ClampUserVolume(Volume);
	SetCrowdMixVolume(CurrentCrowdConfigVolume, 0.0f);
	ApplyButtonClickVolume();
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
			CrowdBedComponent->FadeIn(
				FMath::Max(0.0f, AudioConfig->LoopFadeInDuration),
				FMath::Max(0.0f, AudioConfig->CrowdIdleVolume) * UserEffectVolume);
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

	CrowdBedComponent->AdjustVolume(
		FMath::Max(0.0f, FadeDuration),
		CurrentCrowdConfigVolume * UserEffectVolume);
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

void UShowDownAudioSubsystem::HandleBackgroundMusicFinished()
{
	if (BackgroundMusicComponent && AudioConfig && AudioConfig->BackgroundMusicSound)
	{
		BackgroundMusicComponent->Play(0.0f);
	}
}

void UShowDownAudioSubsystem::HandleCrowdBedFinished()
{
	if (CrowdBedComponent && AudioConfig && AudioConfig->CrowdBedSound)
	{
		CrowdBedComponent->Play(0.0f);
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
