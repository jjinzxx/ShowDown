// Fill out your copyright notice in the Description page of Project Settings.

#include "Presentation/SDSmokableCigaretteActor.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundBase.h"

namespace
{
	FSDArtToneSettings LerpCigaretteArtToneSettings(const FSDArtToneSettings& From, const FSDArtToneSettings& To, float Alpha)
	{
		FSDArtToneSettings Result;
		Result.PixelCount = FMath::Lerp(From.PixelCount, To.PixelCount, Alpha);
		Result.ColorSteps = FMath::Lerp(From.ColorSteps, To.ColorSteps, Alpha);
		Result.HalftoneStrength = FMath::Lerp(From.HalftoneStrength, To.HalftoneStrength, Alpha);
		Result.HalftoneCount = FMath::Lerp(From.HalftoneCount, To.HalftoneCount, Alpha);
		Result.HalftoneRadius = FMath::Lerp(From.HalftoneRadius, To.HalftoneRadius, Alpha);
		Result.HalftoneSoftness = FMath::Lerp(From.HalftoneSoftness, To.HalftoneSoftness, Alpha);
		Result.HalftoneShape = FMath::Lerp(From.HalftoneShape, To.HalftoneShape, Alpha);
		Result.ExposureCompensation = FMath::Lerp(From.ExposureCompensation, To.ExposureCompensation, Alpha);
		Result.Saturation = FMath::Lerp(From.Saturation, To.Saturation, Alpha);
		Result.Contrast = FMath::Lerp(From.Contrast, To.Contrast, Alpha);
		Result.VignetteIntensity = FMath::Lerp(From.VignetteIntensity, To.VignetteIntensity, Alpha);
		Result.FilmGrainIntensity = FMath::Lerp(From.FilmGrainIntensity, To.FilmGrainIntensity, Alpha);
		Result.ChromaticAberrationIntensity = FMath::Lerp(From.ChromaticAberrationIntensity, To.ChromaticAberrationIntensity, Alpha);
		Result.BloomIntensity = FMath::Lerp(From.BloomIntensity, To.BloomIntensity, Alpha);
		Result.BloomThreshold = FMath::Lerp(From.BloomThreshold, To.BloomThreshold, Alpha);
		return Result;
	}
}

ASDSmokableCigaretteActor::ASDSmokableCigaretteActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CigaretteMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CigaretteMesh"));
	CigaretteMesh->SetupAttachment(SceneRoot);
	CigaretteMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CigaretteMesh->SetCollisionObjectType(ECC_WorldDynamic);
	CigaretteMesh->SetCollisionResponseToAllChannels(ECR_Block);

	EmberLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("EmberLight"));
	EmberLight->SetupAttachment(CigaretteMesh);
	EmberLight->SetIntensity(EmberIdleIntensity);
	EmberLight->SetAttenuationRadius(35.0f);
	EmberLight->SetLightColor(FLinearColor(1.0f, 0.18f, 0.05f));

	SmokeParticle = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("SmokeParticle"));
	SmokeParticle->SetupAttachment(CigaretteMesh);
	SmokeParticle->SetAutoActivate(false);

	SmokeScreenEffectSettings.PixelCount = 180.0f;
	SmokeScreenEffectSettings.ColorSteps = 4.0f;
	SmokeScreenEffectSettings.HalftoneStrength = 0.0f;
	SmokeScreenEffectSettings.HalftoneCount = 120.0f;
	SmokeScreenEffectSettings.HalftoneRadius = 0.45f;
	SmokeScreenEffectSettings.HalftoneSoftness = 0.08f;
	SmokeScreenEffectSettings.HalftoneShape = 0.0f;
	SmokeScreenEffectSettings.ExposureCompensation = 1.1f;
	SmokeScreenEffectSettings.Saturation = 0.55f;
	SmokeScreenEffectSettings.Contrast = 1.15f;
	SmokeScreenEffectSettings.VignetteIntensity = 0.72f;
	SmokeScreenEffectSettings.FilmGrainIntensity = 0.8f;
	SmokeScreenEffectSettings.ChromaticAberrationIntensity = 1.0f;
	SmokeScreenEffectSettings.BloomIntensity = 0.45f;
	SmokeScreenEffectSettings.BloomThreshold = 0.35f;
}

void ASDSmokableCigaretteActor::BeginPlay()
{
	Super::BeginPlay();
	SetActorTickEnabled(false);

	RestRelativeLocation = CigaretteMesh->GetRelativeLocation();
	RestRelativeRotation = CigaretteMesh->GetRelativeRotation();
	SmokeRelativeLocation = RestRelativeLocation + LocalSmokeOffset;
	SmokeRelativeRotation = RestRelativeRotation + LocalSmokeRotationOffset;
	RestActorTransform = GetActorTransform();
	OriginalCollisionEnabled = CigaretteMesh->GetCollisionEnabled();

	EmberLight->SetIntensity(EmberIdleIntensity);
	SmokeParticle->DeactivateSystem();

	if (bEnableMouseClick)
	{
		CigaretteMesh->OnClicked.AddDynamic(this, &ASDSmokableCigaretteActor::HandleCigaretteClicked);
	}

	if (bEnablePlayerClickEventsOnBeginPlay)
	{
		if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
		{
			PlayerController->bEnableClickEvents = true;
		}
	}
}

void ASDSmokableCigaretteActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (AnimState == ECigaretteAnimState::Idle)
	{
		return;
	}

	StateElapsedTime += DeltaSeconds;
	SmokeElapsedTime += DeltaSeconds;

	if (AnimState == ECigaretteAnimState::Holding)
	{
		InhaleElapsedTime += DeltaSeconds;
	}
	else if (AnimState == ECigaretteAnimState::PostSmokeMouthHold ||
		AnimState == ECigaretteAnimState::Returning ||
		AnimState == ECigaretteAnimState::CoolingDown)
	{
		PostSmokeElapsedTime += DeltaSeconds;
	}

	UpdateEmber(DeltaSeconds);

	if (AnimState == ECigaretteAnimState::Holding)
	{
		UpdateSmokeParticle();
		UpdateSmokeSound();
	}

	UpdateScreenEffect(DeltaSeconds);

	switch (AnimState)
	{
	case ECigaretteAnimState::Lifting:
	{
		const float Alpha = FMath::Clamp(StateElapsedTime / LiftTime, 0.0f, 1.0f);
		const float EasedAlpha = FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 3.0f);

		if (bUseFirstPersonSmokePose)
		{
			SetActorTransformAlpha(SmokeStartActorTransform, GetFirstPersonSmokeTransform(), EasedAlpha);
		}
		else
		{
			SetCigarettePoseAlpha(EasedAlpha);
		}

		if (Alpha >= 1.0f)
		{
			StartPreSmokeMouthHold();
		}
		break;
	}
	case ECigaretteAnimState::PreSmokeMouthHold:
		if (bUseFirstPersonSmokePose)
		{
			SetActorTransform(GetFirstPersonSmokeTransform());
		}
		else
		{
			SetCigarettePoseAlpha(1.0f);
		}

		if (StateElapsedTime >= PreSmokeMouthHoldTime)
		{
			StartSmoking();
		}
		break;
	case ECigaretteAnimState::Holding:
	{
		const float SwayStrength = GetSmokeSwayStrength();

		if (bUseFirstPersonSmokePose)
		{
			SetActorTransform(ApplySmokeSway(GetFirstPersonSmokeTransform(), SwayStrength));
		}
		else
		{
			SetCigarettePoseAlpha(1.0f, SwayStrength);
		}

		if (StateElapsedTime >= SmokeHoldTime)
		{
			StartPostSmokeMouthHold();
		}
		break;
	}
	case ECigaretteAnimState::PostSmokeMouthHold:
		if (bUseFirstPersonSmokePose)
		{
			SetActorTransform(GetFirstPersonSmokeTransform());
		}
		else
		{
			SetCigarettePoseAlpha(1.0f);
		}

		if (StateElapsedTime >= PostSmokeMouthHoldTime)
		{
			ReturnStartActorTransform = GetActorTransform();
			AnimState = ECigaretteAnimState::Returning;
			StateElapsedTime = 0.0f;
		}
		break;
	case ECigaretteAnimState::Returning:
	{
		const float Alpha = FMath::Clamp(StateElapsedTime / ReturnTime, 0.0f, 1.0f);
		const float EasedAlpha = FMath::InterpEaseIn(0.0f, 1.0f, Alpha, 2.0f);

		if (bUseFirstPersonSmokePose)
		{
			SetActorTransformAlpha(ReturnStartActorTransform, RestActorTransform, EasedAlpha);
		}
		else
		{
			SetCigarettePoseAlpha(1.0f - EasedAlpha);
		}

		if (Alpha >= 1.0f)
		{
			StartCooldown();
		}
		break;
	}
	case ECigaretteAnimState::CoolingDown:
		if (StateElapsedTime >= FMath::Max(ReinteractCooldown, FMath::Max(0.0f, EmberFadeOutTime - PostSmokeElapsedTime)))
		{
			FinishSmoke();
		}
		break;
	default:
		break;
	}
}

void ASDSmokableCigaretteActor::Smoke()
{
	if (!CanInteract_Implementation(nullptr))
	{
		return;
	}

	SmokeRelativeLocation = RestRelativeLocation + LocalSmokeOffset;
	SmokeRelativeRotation = RestRelativeRotation + LocalSmokeRotationOffset;
	RestActorTransform = GetActorTransform();
	SmokeStartActorTransform = RestActorTransform;
	ReturnStartActorTransform = RestActorTransform;
	AnimState = ECigaretteAnimState::Lifting;
	StateElapsedTime = 0.0f;
	SmokeElapsedTime = 0.0f;
	InhaleElapsedTime = 0.0f;
	PostSmokeElapsedTime = 0.0f;
	bSmokeParticleActivated = false;
	bSmokeSoundPlayed = false;
	SetActorTickEnabled(true);

	if (bDisableCollisionWhileSmoking)
	{
		OriginalCollisionEnabled = CigaretteMesh->GetCollisionEnabled();
		CigaretteMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	OnSmokeStarted.Broadcast();
}

bool ASDSmokableCigaretteActor::CanInteract_Implementation(AActor* Interactor) const
{
	return AnimState == ECigaretteAnimState::Idle;
}

void ASDSmokableCigaretteActor::Interact_Implementation(AActor* Interactor)
{
	Smoke();
}

void ASDSmokableCigaretteActor::HandleCigaretteClicked(UPrimitiveComponent* TouchedComponent, FKey ButtonPressed)
{
	Smoke();
}

void ASDSmokableCigaretteActor::StartPreSmokeMouthHold()
{
	AnimState = ECigaretteAnimState::PreSmokeMouthHold;
	StateElapsedTime = 0.0f;

	if (PreSmokeMouthHoldTime <= KINDA_SMALL_NUMBER)
	{
		StartSmoking();
	}
}

void ASDSmokableCigaretteActor::StartSmoking()
{
	SmokeStartActorTransform = GetActorTransform();
	ReturnStartActorTransform = SmokeStartActorTransform;
	AnimState = ECigaretteAnimState::Holding;
	StateElapsedTime = 0.0f;
	InhaleElapsedTime = 0.0f;
	StartScreenEffect();
	UpdateSmokeParticle();
	UpdateSmokeSound();
}

void ASDSmokableCigaretteActor::StartPostSmokeMouthHold()
{
	AnimState = ECigaretteAnimState::PostSmokeMouthHold;
	StateElapsedTime = 0.0f;
	PostSmokeElapsedTime = 0.0f;
	SmokeParticle->DeactivateSystem();
	bSmokeParticleActivated = false;
	RestoreScreenEffect();

	if (PostSmokeMouthHoldTime <= KINDA_SMALL_NUMBER)
	{
		ReturnStartActorTransform = GetActorTransform();
		AnimState = ECigaretteAnimState::Returning;
		StateElapsedTime = 0.0f;
	}
}

void ASDSmokableCigaretteActor::StartCooldown()
{
	if (bUseFirstPersonSmokePose)
	{
		SetActorTransform(RestActorTransform);
	}
	else
	{
		SetCigarettePoseAlpha(0.0f);
	}

	if (bDisableCollisionWhileSmoking)
	{
		CigaretteMesh->SetCollisionEnabled(OriginalCollisionEnabled);
	}

	AnimState = ECigaretteAnimState::CoolingDown;
	StateElapsedTime = 0.0f;

	SmokeParticle->DeactivateSystem();
	bSmokeParticleActivated = false;

	if (FMath::Max(ReinteractCooldown, FMath::Max(0.0f, EmberFadeOutTime - PostSmokeElapsedTime)) <= KINDA_SMALL_NUMBER)
	{
		FinishSmoke();
	}
}

void ASDSmokableCigaretteActor::UpdateSmokeParticle()
{
	if (!bSmokeParticleActivated && InhaleElapsedTime >= SmokeParticleDelay)
	{
		SmokeParticle->ActivateSystem(true);
		bSmokeParticleActivated = true;
	}
}

void ASDSmokableCigaretteActor::UpdateSmokeSound()
{
	if (!bSmokeSoundPlayed && SmokeSound && InhaleElapsedTime >= SmokeSoundDelay)
	{
		UGameplayStatics::PlaySoundAtLocation(this, SmokeSound, GetActorLocation());
		bSmokeSoundPlayed = true;
	}
}

void ASDSmokableCigaretteActor::StartScreenEffect()
{
	if (!bEnableSmokeScreenEffect)
	{
		return;
	}

	ASDArtToneController* Controller = ResolveArtToneController();
	if (!Controller)
	{
		return;
	}

	ScreenEffectBaseSettings = Controller->GetCurrentSettings();
	ScreenEffectReleaseStartSettings = ScreenEffectBaseSettings;
	ScreenEffectRestoreElapsedTime = 0.0f;
	bScreenEffectActive = true;
	bScreenEffectRestoring = false;
	UpdateScreenEffect(0.0f);
}

void ASDSmokableCigaretteActor::UpdateScreenEffect(float DeltaSeconds)
{
	ASDArtToneController* Controller = ResolveArtToneController();
	if (!Controller)
	{
		bScreenEffectActive = false;
		bScreenEffectRestoring = false;
		return;
	}

	if (bScreenEffectActive)
	{
		const float EffectElapsedTime = FMath::Max(0.0f, InhaleElapsedTime - ScreenEffectStartDelay);
		const float FadeAlpha = ScreenEffectFadeInTime > KINDA_SMALL_NUMBER
			? FMath::Clamp(EffectElapsedTime / ScreenEffectFadeInTime, 0.0f, 1.0f)
			: 1.0f;
		const float EffectStrength = FMath::InterpEaseInOut(0.0f, FMath::Clamp(ScreenEffectMaxStrength, 0.0f, 1.0f), FadeAlpha, 2.0f);
		Controller->ApplyArtTone(LerpCigaretteArtToneSettings(ScreenEffectBaseSettings, SmokeScreenEffectSettings, EffectStrength));
		return;
	}

	if (bScreenEffectRestoring)
	{
		ScreenEffectRestoreElapsedTime += DeltaSeconds;
		const float FadeAlpha = ScreenEffectFadeOutTime > KINDA_SMALL_NUMBER
			? FMath::Clamp(ScreenEffectRestoreElapsedTime / ScreenEffectFadeOutTime, 0.0f, 1.0f)
			: 1.0f;
		const float EasedAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, FadeAlpha, 2.0f);
		Controller->ApplyArtTone(LerpCigaretteArtToneSettings(ScreenEffectReleaseStartSettings, ScreenEffectBaseSettings, EasedAlpha));

		if (FadeAlpha >= 1.0f)
		{
			bScreenEffectRestoring = false;
			ScreenEffectRestoreElapsedTime = 0.0f;
		}
	}
}

void ASDSmokableCigaretteActor::RestoreScreenEffect()
{
	if (!bScreenEffectActive && !bScreenEffectRestoring)
	{
		return;
	}

	ASDArtToneController* Controller = ResolveArtToneController();
	if (!Controller)
	{
		bScreenEffectActive = false;
		bScreenEffectRestoring = false;
		return;
	}

	ScreenEffectReleaseStartSettings = Controller->GetCurrentSettings();
	ScreenEffectRestoreElapsedTime = 0.0f;
	bScreenEffectActive = false;
	bScreenEffectRestoring = true;

	if (ScreenEffectFadeOutTime <= KINDA_SMALL_NUMBER)
	{
		Controller->ApplyArtTone(ScreenEffectBaseSettings);
		bScreenEffectRestoring = false;
	}
}

ASDArtToneController* ASDSmokableCigaretteActor::ResolveArtToneController()
{
	if (ArtToneController)
	{
		return ArtToneController;
	}

	ArtToneController = Cast<ASDArtToneController>(UGameplayStatics::GetActorOfClass(this, ASDArtToneController::StaticClass()));
	return ArtToneController;
}

void ASDSmokableCigaretteActor::SetCigarettePoseAlpha(float Alpha, float SwayStrength)
{
	const FQuat RestQuat = RestRelativeRotation.Quaternion();
	const FQuat SmokeQuat = SmokeRelativeRotation.Quaternion();
	FQuat NewRotation = FQuat::Slerp(RestQuat, SmokeQuat, Alpha);
	FVector NewLocation = FMath::Lerp(RestRelativeLocation, SmokeRelativeLocation, Alpha);

	if (bEnableSmokeSway && SwayStrength > 0.0f)
	{
		NewLocation += NewRotation.RotateVector(GetSmokeSwayLocationOffset(SwayStrength));
		NewRotation = NewRotation * GetSmokeSwayRotationOffset(SwayStrength).Quaternion();
	}

	CigaretteMesh->SetRelativeLocation(NewLocation);
	CigaretteMesh->SetRelativeRotation(NewRotation);
}

FTransform ASDSmokableCigaretteActor::GetFirstPersonSmokeTransform() const
{
	if (const APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (const APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager)
		{
			const FVector CameraLocation = CameraManager->GetCameraLocation();
			const FRotator CameraRotation = CameraManager->GetCameraRotation();
			const FRotationMatrix CameraMatrix(CameraRotation);

			const FVector TargetLocation =
				CameraLocation +
				CameraMatrix.GetScaledAxis(EAxis::X) * FirstPersonCameraOffset.X +
				CameraMatrix.GetScaledAxis(EAxis::Y) * FirstPersonCameraOffset.Y +
				CameraMatrix.GetScaledAxis(EAxis::Z) * FirstPersonCameraOffset.Z;

			const FRotator TargetRotation = CameraRotation + FirstPersonRotationOffset;
			return FTransform(TargetRotation, TargetLocation, RestActorTransform.GetScale3D());
		}
	}

	return RestActorTransform;
}

FTransform ASDSmokableCigaretteActor::ApplySmokeSway(const FTransform& BaseTransform, float SwayStrength) const
{
	if (!bEnableSmokeSway || SwayStrength <= 0.0f)
	{
		return BaseTransform;
	}

	FTransform SwayedTransform = BaseTransform;
	const FQuat BaseRotation = BaseTransform.GetRotation();
	SwayedTransform.AddToTranslation(BaseRotation.RotateVector(GetSmokeSwayLocationOffset(SwayStrength)));
	SwayedTransform.SetRotation((BaseRotation * GetSmokeSwayRotationOffset(SwayStrength).Quaternion()).GetNormalized());
	return SwayedTransform;
}

float ASDSmokableCigaretteActor::GetSmokeSwayStrength() const
{
	if (!bEnableSmokeSway)
	{
		return 0.0f;
	}

	const float SwayElapsedTime = GetSmokeSwayElapsedTime();
	if (SwayElapsedTime <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	if (SmokeSwayRampTime <= KINDA_SMALL_NUMBER)
	{
		return 1.0f;
	}

	const float Alpha = FMath::Clamp(SwayElapsedTime / SmokeSwayRampTime, 0.0f, 1.0f);
	return FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f);
}

float ASDSmokableCigaretteActor::GetSmokeSwayElapsedTime() const
{
	return FMath::Max(0.0f, InhaleElapsedTime - SmokeSwayStartDelay);
}

FVector ASDSmokableCigaretteActor::GetSmokeSwayLocationOffset(float SwayStrength) const
{
	const float ClampedStrength = FMath::Clamp(SwayStrength, 0.0f, 1.0f);
	const float SwayTime = GetSmokeSwayElapsedTime() * SmokeSwaySpeed;

	return FVector(
		FMath::Sin(SwayTime * 0.74f + 0.3f) * SmokeSwayLocationAmplitude.X,
		FMath::Sin(SwayTime * 1.13f + 1.7f) * SmokeSwayLocationAmplitude.Y,
		FMath::Sin(SwayTime * 0.91f + 2.4f) * SmokeSwayLocationAmplitude.Z) * ClampedStrength;
}

FRotator ASDSmokableCigaretteActor::GetSmokeSwayRotationOffset(float SwayStrength) const
{
	const float ClampedStrength = FMath::Clamp(SwayStrength, 0.0f, 1.0f);
	const float SwayTime = GetSmokeSwayElapsedTime() * SmokeSwaySpeed;

	return FRotator(
		FMath::Sin(SwayTime * 1.21f + 0.8f) * SmokeSwayRotationAmplitude.Pitch * ClampedStrength,
		FMath::Sin(SwayTime * 0.83f + 2.2f) * SmokeSwayRotationAmplitude.Yaw * ClampedStrength,
		FMath::Sin(SwayTime * 1.37f + 1.1f) * SmokeSwayRotationAmplitude.Roll * ClampedStrength);
}

void ASDSmokableCigaretteActor::SetActorTransformAlpha(const FTransform& FromTransform, const FTransform& ToTransform, float Alpha)
{
	const FVector NewLocation = FMath::Lerp(FromTransform.GetLocation(), ToTransform.GetLocation(), Alpha);
	const FQuat NewRotation = FQuat::Slerp(FromTransform.GetRotation(), ToTransform.GetRotation(), Alpha);
	const FVector NewScale = FMath::Lerp(FromTransform.GetScale3D(), ToTransform.GetScale3D(), Alpha);

	SetActorTransform(FTransform(NewRotation, NewLocation, NewScale));
}

void ASDSmokableCigaretteActor::UpdateEmber(float DeltaSeconds)
{
	const float WarmupAlpha = EmberWarmupTime > KINDA_SMALL_NUMBER
		? FMath::Clamp(InhaleElapsedTime / EmberWarmupTime, 0.0f, 1.0f)
		: 1.0f;

	float TargetIntensity = EmberIdleIntensity;
	if (AnimState == ECigaretteAnimState::Holding)
	{
		const float Flicker = FMath::Sin(InhaleElapsedTime * EmberFlickerSpeed) * EmberFlickerAmount * WarmupAlpha;
		TargetIntensity = FMath::Lerp(EmberIdleIntensity, EmberSmokeIntensity, WarmupAlpha) + Flicker;
	}
	else if (AnimState == ECigaretteAnimState::PostSmokeMouthHold ||
		AnimState == ECigaretteAnimState::Returning ||
		AnimState == ECigaretteAnimState::CoolingDown)
	{
		const float FadeAlpha = EmberFadeOutTime > KINDA_SMALL_NUMBER
			? FMath::Clamp(PostSmokeElapsedTime / EmberFadeOutTime, 0.0f, 1.0f)
			: 1.0f;
		TargetIntensity = FMath::Lerp(EmberSmokeIntensity, EmberIdleIntensity, FadeAlpha);
	}

	TargetIntensity = FMath::Max(0.0f, TargetIntensity);
	EmberLight->SetIntensity(TargetIntensity);
}

void ASDSmokableCigaretteActor::FinishSmoke()
{
	if (bUseFirstPersonSmokePose)
	{
		SetActorTransform(RestActorTransform);
	}
	else
	{
		SetCigarettePoseAlpha(0.0f);
	}

	if (bDisableCollisionWhileSmoking)
	{
		CigaretteMesh->SetCollisionEnabled(OriginalCollisionEnabled);
	}

	EmberLight->SetIntensity(EmberIdleIntensity);
	SmokeParticle->DeactivateSystem();
	if (ASDArtToneController* Controller = ResolveArtToneController())
	{
		if (bScreenEffectActive || bScreenEffectRestoring)
		{
			Controller->ApplyArtTone(ScreenEffectBaseSettings);
		}
	}
	AnimState = ECigaretteAnimState::Idle;
	StateElapsedTime = 0.0f;
	SmokeElapsedTime = 0.0f;
	InhaleElapsedTime = 0.0f;
	PostSmokeElapsedTime = 0.0f;
	ScreenEffectRestoreElapsedTime = 0.0f;
	bSmokeParticleActivated = false;
	bSmokeSoundPlayed = false;
	bScreenEffectActive = false;
	bScreenEffectRestoring = false;
	SetActorTickEnabled(false);
	OnSmokeFinished.Broadcast();
}
