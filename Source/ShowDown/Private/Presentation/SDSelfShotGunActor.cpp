#include "Presentation/SDSelfShotGunActor.h"

#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "ShowDownCharacter.h"
#include "ShowDownGameStateBase.h"
#include "ShowDownPlayerController.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	float ApplyEase(float Alpha, ESDHitSequenceEaseMode EaseMode, float Exponent)
	{
		const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
		const float ClampedExponent = FMath::Max(1.0f, Exponent);

		switch (EaseMode)
		{
		case ESDHitSequenceEaseMode::EaseIn:
			return FMath::InterpEaseIn(0.0f, 1.0f, ClampedAlpha, ClampedExponent);
		case ESDHitSequenceEaseMode::EaseOut:
			return FMath::InterpEaseOut(0.0f, 1.0f, ClampedAlpha, ClampedExponent);
		case ESDHitSequenceEaseMode::EaseInOut:
			return FMath::InterpEaseInOut(0.0f, 1.0f, ClampedAlpha, ClampedExponent);
		default:
			return ClampedAlpha;
		}
	}

	FSDArtToneSettings LerpArtToneSettings(const FSDArtToneSettings& From, const FSDArtToneSettings& To, float Alpha)
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

ASDSelfShotGunActor::ASDSelfShotGunActor()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	GunMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GunMesh"));
	GunMesh->SetupAttachment(SceneRoot);
	GunMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GunMesh->SetCollisionObjectType(ECC_WorldDynamic);
	GunMesh->SetCollisionResponseToAllChannels(ECR_Block);

	InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
	InteractionBounds->SetupAttachment(SceneRoot);
	InteractionBounds->SetBoxExtent(FVector(38.0f, 16.0f, 10.0f));
	InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBounds->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	ChamberPivot = CreateDefaultSubobject<USceneComponent>(TEXT("ChamberPivot"));
	ChamberPivot->SetupAttachment(SceneRoot);

	ChamberMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChamberMesh"));
	ChamberMesh->SetupAttachment(ChamberPivot);
	ChamberMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	BulletMesh01 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BulletMesh01"));
	BulletMesh01->SetupAttachment(ChamberPivot);
	BulletMesh01->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	BulletMesh02 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BulletMesh02"));
	BulletMesh02->SetupAttachment(ChamberPivot);
	BulletMesh02->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	BulletMesh03 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BulletMesh03"));
	BulletMesh03->SetupAttachment(ChamberPivot);
	BulletMesh03->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	BulletMesh04 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BulletMesh04"));
	BulletMesh04->SetupAttachment(ChamberPivot);
	BulletMesh04->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	BulletMesh05 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BulletMesh05"));
	BulletMesh05->SetupAttachment(ChamberPivot);
	BulletMesh05->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	BulletMesh06 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BulletMesh06"));
	BulletMesh06->SetupAttachment(ChamberPivot);
	BulletMesh06->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	TriggerPivot = CreateDefaultSubobject<USceneComponent>(TEXT("TriggerPivot"));
	TriggerPivot->SetupAttachment(SceneRoot);

	TriggerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TriggerMesh"));
	TriggerMesh->SetupAttachment(TriggerPivot);
	TriggerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	HammerPivot = CreateDefaultSubobject<USceneComponent>(TEXT("HammerPivot"));
	HammerPivot->SetupAttachment(SceneRoot);

	HammerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HammerMesh"));
	HammerMesh->SetupAttachment(HammerPivot);
	HammerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> HammerMeshAsset(
		TEXT("/Game/Fab/Revolver/Revolver/Hammer.Hammer"));
	if (HammerMeshAsset.Succeeded())
	{
		HammerMesh->SetStaticMesh(HammerMeshAsset.Object);
	}

	HandleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HandleMesh"));
	HandleMesh->SetupAttachment(SceneRoot);
	HandleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	RatchetMechanismMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RatchetMechanismMesh"));
	RatchetMechanismMesh->SetupAttachment(SceneRoot);
	RatchetMechanismMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ExtraMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExtraMesh"));
	ExtraMesh->SetupAttachment(SceneRoot);
	ExtraMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	MuzzlePoint = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzlePoint"));
	MuzzlePoint->SetupAttachment(GunMesh);

	MuzzleFlashLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MuzzleFlashLight"));
	MuzzleFlashLight->SetupAttachment(MuzzlePoint);
	MuzzleFlashLight->SetIntensity(0.0f);
	MuzzleFlashLight->SetAttenuationRadius(MuzzleFlashAttenuationRadius);
	MuzzleFlashLight->SetLightColor(MuzzleFlashColor);

	InitialHitEffectSettings.PixelCount = 95.0f;
	InitialHitEffectSettings.ColorSteps = 2.0f;
	InitialHitEffectSettings.HalftoneStrength = 0.65f;
	InitialHitEffectSettings.HalftoneCount = 120.0f;
	InitialHitEffectSettings.HalftoneRadius = 0.45f;
	InitialHitEffectSettings.HalftoneSoftness = 0.08f;
	InitialHitEffectSettings.HalftoneShape = 0.0f;
	InitialHitEffectSettings.ExposureCompensation = 1.35f;
	InitialHitEffectSettings.Saturation = 0.2f;
	InitialHitEffectSettings.Contrast = 1.45f;
	InitialHitEffectSettings.VignetteIntensity = 0.95f;
	InitialHitEffectSettings.FilmGrainIntensity = 0.9f;
	InitialHitEffectSettings.ChromaticAberrationIntensity = 1.6f;
	InitialHitEffectSettings.BloomIntensity = 1.15f;
	InitialHitEffectSettings.BloomThreshold = 0.3f;

	RecoveryHitEffectSettings.PixelCount = 165.0f;
	RecoveryHitEffectSettings.ColorSteps = 3.0f;
	RecoveryHitEffectSettings.HalftoneStrength = 0.3f;
	RecoveryHitEffectSettings.HalftoneCount = 120.0f;
	RecoveryHitEffectSettings.HalftoneRadius = 0.45f;
	RecoveryHitEffectSettings.HalftoneSoftness = 0.08f;
	RecoveryHitEffectSettings.HalftoneShape = 0.0f;
	RecoveryHitEffectSettings.ExposureCompensation = 0.7f;
	RecoveryHitEffectSettings.Saturation = 0.3f;
	RecoveryHitEffectSettings.Contrast = 1.2f;
	RecoveryHitEffectSettings.VignetteIntensity = 0.85f;
	RecoveryHitEffectSettings.FilmGrainIntensity = 0.55f;
	RecoveryHitEffectSettings.ChromaticAberrationIntensity = 0.8f;
	RecoveryHitEffectSettings.BloomIntensity = 0.35f;
	RecoveryHitEffectSettings.BloomThreshold = 0.3f;
}

#if WITH_EDITOR
bool ASDSelfShotGunActor::ShouldTickIfViewportsOnly() const
{
	return bEnableRevolverPlacementDevMode || bRevolverPlacementDevPreviewActive;
}
#endif

void ASDSelfShotGunActor::BeginPlay()
{
	Super::BeginPlay();

	RestActorTransform = GetActorTransform();
	bHasCapturedRestActorTransform = true;
	OriginalCollisionEnabled = GunMesh->GetCollisionEnabled();
	TriggerRestRotation = TriggerPivot->GetRelativeRotation();
	HammerRestRotation = HammerPivot->GetRelativeRotation();
	MechanismResetStartTriggerRotation = TriggerRestRotation;
	MechanismResetStartHammerRotation = HammerRestRotation;
	ChamberInitialRotation = ChamberPivot->GetRelativeRotation();
	ChamberCurrentRotation = ChamberInitialRotation;
	ChamberStartRotation = ChamberCurrentRotation;
	ChamberTargetRotation = ChamberCurrentRotation;
	MuzzleFlashLight->SetAttenuationRadius(FMath::Max(0.0f, MuzzleFlashAttenuationRadius));
	MuzzleFlashLight->SetLightColor(MuzzleFlashColor);

	if (AShowDownGameStateBase* ShowDownGameState = GetWorld() ? GetWorld()->GetGameState<AShowDownGameStateBase>() : nullptr)
	{
		ShowDownGameState->OnMultiplayerRoulettePresentation.AddUniqueDynamic(
			this,
			&ASDSelfShotGunActor::HandleMultiplayerRoulettePresentation);
	}
}

void ASDSelfShotGunActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AShowDownGameStateBase* ShowDownGameState = GetWorld() ? GetWorld()->GetGameState<AShowDownGameStateBase>() : nullptr)
	{
		ShowDownGameState->OnMultiplayerRoulettePresentation.RemoveDynamic(
			this,
			&ASDSelfShotGunActor::HandleMultiplayerRoulettePresentation);
	}

	Super::EndPlay(EndPlayReason);
}

void ASDSelfShotGunActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (MuzzleFlashElapsedTime > 0.0f)
	{
		MuzzleFlashElapsedTime = FMath::Max(0.0f, MuzzleFlashElapsedTime - DeltaSeconds);
		MuzzleFlashLight->SetIntensity(
			MuzzleFlashElapsedTime > 0.0f ? MuzzleFlashIntensity : 0.0f);
	}

	UpdateHitSequence(DeltaSeconds);
	UpdateTinnitusSound(DeltaSeconds);
	UpdateSelfShotCinematicCamera(DeltaSeconds);
	UpdateCinematicCameraSteppedShake(DeltaSeconds);

	if (AnimState == EGunAnimState::Idle)
	{
		if (UpdateRevolverPlacementDevPreview())
		{
			return;
		}

		if (bPreviewFirstPersonPose)
		{
			SetActorTransform(GetFirstPersonGunTransform());
		}

		return;
	}

	StateElapsedTime += DeltaSeconds;
	if (AnimState == EGunAnimState::Returning || (AnimState == EGunAnimState::Fired && !bCurrentShotWasEmpty))
	{
		MechanismResetElapsedTime += DeltaSeconds;
	}
	if (AnimState == EGunAnimState::Aiming
		|| AnimState == EGunAnimState::Cocking
		|| AnimState == EGunAnimState::HammerReleasing)
	{
		HeldGunJitterElapsedTime += DeltaSeconds;
	}

	switch (AnimState)
	{
	case EGunAnimState::Raising:
	{
		const float Alpha = FMath::Clamp(StateElapsedTime / RaiseTime, 0.0f, 1.0f);
		SetActorTransformAlpha(RaiseStartTransform, GetFirstPersonGunTransform(), FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 3.0f));
		if (Alpha >= 1.0f)
		{
			AnimState = EGunAnimState::Aiming;
			StateElapsedTime = 0.0f;
		}
		break;
	}
	case EGunAnimState::Aiming:
		SetActorTransform(ApplyHeldGunJitter(GetFirstPersonGunTransform()));
		if (StateElapsedTime >= AimHoldTime)
		{
			if (bEnableMechanismAnimation)
			{
				StartMechanismAnimation();
			}
			else
			{
				FireGun();
			}
		}
		break;
	case EGunAnimState::Cocking:
		SetActorTransform(ApplyHeldGunJitter(GetFirstPersonGunTransform()));
		UpdateMechanismCocking();
		break;
	case EGunAnimState::HammerReleasing:
		SetActorTransform(ApplyHeldGunJitter(GetFirstPersonGunTransform()));
		UpdateHammerRelease();
		break;
	case EGunAnimState::EmptyImpact:
		SetActorTransform(GetFirstPersonGunTransform());
		UpdateEmptyShotImpact();
		break;
	case EGunAnimState::Fired:
		if (!bCurrentShotWasEmpty)
		{
			UpdateMechanismReset();
		}
		if (StateElapsedTime >= ShotHoldTime)
		{
			ReturnStartTransform = GetActorTransform();
			AnimState = EGunAnimState::Returning;
			StateElapsedTime = 0.0f;

		}
		break;
	case EGunAnimState::Returning:
	{
		UpdateMechanismReset();
		const float Alpha = FMath::Clamp(StateElapsedTime / ReturnTime, 0.0f, 1.0f);
		SetActorTransformAlpha(ReturnStartTransform, RestActorTransform, FMath::InterpEaseIn(0.0f, 1.0f, Alpha, 2.0f));
		if (Alpha >= 1.0f)
		{
			FinishSequence();
		}
		break;
	}
	default:
		break;
	}
}

void ASDSelfShotGunActor::UseGun()
{
	ForcedShotTargetActor = nullptr;
	ForcedShotCamera = nullptr;
	bHasForcedShotSourceLocation = false;
	bHasForcedShotAimLocation = false;
	bHasForcedShotRotationOffset = false;
	bCurrentShotTargetsLocalPlayer = true;
	StartGunUse();
}

void ASDSelfShotGunActor::UseGunWithForcedResult(bool bLiveRound)
{
	ShotResultMode = bLiveRound
		? ESDSelfShotRoundMode::AlwaysLive
		: ESDSelfShotRoundMode::AlwaysEmpty;
	ForcedShotTargetActor = nullptr;
	ForcedShotCamera = nullptr;
	bHasForcedShotSourceLocation = false;
	bHasForcedShotAimLocation = false;
	bHasForcedShotRotationOffset = false;
	bCurrentShotTargetsLocalPlayer = true;
	StartGunUse();
}

void ASDSelfShotGunActor::UseGunWithForcedResultAtTarget(bool bLiveRound, AActor* TargetActor)
{
	ShotResultMode = bLiveRound
		? ESDSelfShotRoundMode::AlwaysLive
		: ESDSelfShotRoundMode::AlwaysEmpty;
	ForcedShotTargetActor = TargetActor;
	ForcedShotCamera = nullptr;
	bHasForcedShotSourceLocation = false;
	bHasForcedShotAimLocation = false;
	bHasForcedShotRotationOffset = false;
	bCurrentShotTargetsLocalPlayer = ShouldTreatTargetAsLocalPlayer(TargetActor);
	StartGunUse();
}

void ASDSelfShotGunActor::UseGunWithForcedResultAtTargetFromLocation(bool bLiveRound, AActor* TargetActor, FVector SourceLocation)
{
	ShotResultMode = bLiveRound
		? ESDSelfShotRoundMode::AlwaysLive
		: ESDSelfShotRoundMode::AlwaysEmpty;
	ForcedShotTargetActor = TargetActor;
	ForcedShotSourceLocation = SourceLocation;
	ForcedShotCamera = nullptr;
	bHasForcedShotSourceLocation = true;
	bHasForcedShotAimLocation = false;
	bHasForcedShotRotationOffset = false;
	bCurrentShotTargetsLocalPlayer = ShouldTreatTargetAsLocalPlayer(TargetActor);
	StartGunUse();
}

void ASDSelfShotGunActor::UseGunWithForcedResultAtTargetFromLocationAndCamera(
	bool bLiveRound,
	AActor* TargetActor,
	FVector SourceLocation,
	ACameraActor* ShotCamera)
{
	ShotResultMode = bLiveRound
		? ESDSelfShotRoundMode::AlwaysLive
		: ESDSelfShotRoundMode::AlwaysEmpty;
	ForcedShotTargetActor = TargetActor;
	ForcedShotSourceLocation = SourceLocation;
	ForcedShotCamera = ShotCamera;
	bHasForcedShotSourceLocation = true;
	bHasForcedShotAimLocation = false;
	bHasForcedShotRotationOffset = false;
	bCurrentShotTargetsLocalPlayer = ShouldTreatTargetAsLocalPlayer(TargetActor);
	StartGunUse();
}

void ASDSelfShotGunActor::UseGunWithForcedResultAtTargetFromLocationAimAndCamera(
	bool bLiveRound,
	AActor* TargetActor,
	FVector SourceLocation,
	FVector AimLocation,
	ACameraActor* ShotCamera)
{
	ShotResultMode = bLiveRound
		? ESDSelfShotRoundMode::AlwaysLive
		: ESDSelfShotRoundMode::AlwaysEmpty;
	ForcedShotTargetActor = TargetActor;
	ForcedShotSourceLocation = SourceLocation;
	ForcedShotAimLocation = AimLocation;
	ForcedShotCamera = ShotCamera;
	bHasForcedShotSourceLocation = true;
	bHasForcedShotAimLocation = true;
	bHasForcedShotRotationOffset = false;
	bCurrentShotTargetsLocalPlayer = ShouldTreatTargetAsLocalPlayer(TargetActor);
	StartGunUse();
}

void ASDSelfShotGunActor::UseGunWithForcedResultAtTargetFromLocationAimRotationAndCamera(
	bool bLiveRound,
	AActor* TargetActor,
	FVector SourceLocation,
	FVector AimLocation,
	FRotator RotationOffset,
	ACameraActor* ShotCamera)
{
	ShotResultMode = bLiveRound
		? ESDSelfShotRoundMode::AlwaysLive
		: ESDSelfShotRoundMode::AlwaysEmpty;
	ForcedShotTargetActor = TargetActor;
	ForcedShotSourceLocation = SourceLocation;
	ForcedShotAimLocation = AimLocation;
	ForcedShotRotationOffset = RotationOffset;
	ForcedShotCamera = ShotCamera;
	bHasForcedShotSourceLocation = true;
	bHasForcedShotAimLocation = true;
	bHasForcedShotRotationOffset = true;
	bCurrentShotTargetsLocalPlayer = ShouldTreatTargetAsLocalPlayer(TargetActor);
	StartGunUse();
}

ACameraActor* ASDSelfShotGunActor::GetEnemyShotCinematicCamera() const
{
	return EnemyShotCinematicCamera;
}

float ASDSelfShotGunActor::GetTargetShotSourcePullDistance() const
{
	return TargetShotSourcePullDistance;
}

float ASDSelfShotGunActor::GetShotResolveDelay() const
{
	float ResolveDelay = FMath::Max(0.0f, RaiseTime) + FMath::Max(0.0f, AimHoldTime);
	if (bEnableMechanismAnimation)
	{
		ResolveDelay += FMath::Max(0.0f, MechanismCockTime);
		ResolveDelay += FMath::Max(0.0f, HammerReleaseTime);
	}

	return ResolveDelay;
}

bool ASDSelfShotGunActor::TryResolveCharacterPresentationShot(
	const AShowDownCharacter* TargetCharacter,
	FVector& OutSourceLocation,
	FVector& OutAimLocation,
	FRotator* OutRotationOffset) const
{
	if (!IsValid(TargetCharacter))
	{
		return false;
	}

	const USceneComponent* RevolverAnchor = TargetCharacter->GetRevolverPresentationAnchor();
	if (!IsValid(RevolverAnchor))
	{
		return false;
	}

	OutSourceLocation = RevolverAnchor->GetComponentLocation();
	OutAimLocation = TargetCharacter->GetActorLocation() + TargetShotAimOffset;
	if (OutRotationOffset)
	{
		*OutRotationOffset = RevolverAnchor->GetRelativeRotation();
	}

	const USkeletalMeshComponent* CharacterMesh = TargetCharacter->GetMesh();
	const FName AimAttachName = TargetCharacter->ResolvePlayerCameraAttachName();
	if (CharacterMesh
		&& AimAttachName != NAME_None
		&& (CharacterMesh->DoesSocketExist(AimAttachName)
			|| CharacterMesh->GetBoneIndex(AimAttachName) != INDEX_NONE))
	{
		OutAimLocation = CharacterMesh->GetSocketLocation(AimAttachName);
	}

	if ((OutAimLocation - OutSourceLocation).IsNearlyZero())
	{
		FVector AimDirection = TargetCharacter->GetActorForwardVector().GetSafeNormal();
		if (AimDirection.IsNearlyZero())
		{
			AimDirection = RevolverAnchor->GetForwardVector().GetSafeNormal();
		}
		if (AimDirection.IsNearlyZero())
		{
			AimDirection = FVector::ForwardVector;
		}

		OutAimLocation = OutSourceLocation + AimDirection * 100.0f;
	}

	return true;
}

void ASDSelfShotGunActor::StartGunUse()
{
	if (!CanInteract_Implementation(nullptr))
	{
		return;
	}

	RestActorTransform = GetActorTransform();
	bHasCapturedRestActorTransform = true;
	RaiseStartTransform = RestActorTransform;
	StateElapsedTime = 0.0f;
	MechanismResetElapsedTime = 0.0f;
	HeldGunJitterElapsedTime = 0.0f;
	CaptureFirstPersonPoseCamera();
	StartSelfShotCinematicCamera();
	AnimState = EGunAnimState::Raising;
	bPresentationFinishPending = true;

	if (bDisableCollisionWhileUsing)
	{
		OriginalCollisionEnabled = GunMesh->GetCollisionEnabled();
		GunMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	OnGunRaised.Broadcast();
}

bool ASDSelfShotGunActor::CanInteract_Implementation(AActor* Interactor) const
{
	return AnimState == EGunAnimState::Idle
		&& HitSequenceState == EHitSequenceState::Idle
		&& !bSelfShotCinematicCameraActive;
}

void ASDSelfShotGunActor::Interact_Implementation(AActor* Interactor)
{
	UseGun();
}

void ASDSelfShotGunActor::FireGun()
{
	const bool bLiveShot = ResolveCurrentShotIsLive();
	StateElapsedTime = 0.0f;
	MechanismResetElapsedTime = 0.0f;
	if (bSelfShotCinematicCameraStartPending)
	{
		ActivateSelfShotCinematicCamera();
	}
	bSelfShotCinematicCameraHoldStarted = bSelfShotCinematicCameraActive;
	CinematicCameraElapsedTime = 0.0f;

	if (bLiveShot)
	{
		FireLiveRound();
	}
	else
	{
		FireEmptyRound();
	}

	ConsumeCurrentChamberIfNeeded(bLiveShot);
	if (ShotResultMode == ESDSelfShotRoundMode::ChamberPattern)
	{
		AdvanceCurrentChamberIndex();
	}
}

void ASDSelfShotGunActor::FireLiveRound()
{
	AnimState = EGunAnimState::Fired;
	bCurrentShotWasEmpty = false;
	MechanismResetStartTriggerRotation = TriggerRestRotation + TriggerPulledRotationOffset;
	MechanismResetStartHammerRotation = HammerRestRotation + HammerFiredRotationOffset;
	MuzzleFlashElapsedTime = MuzzleFlashDuration;
	MuzzleFlashLight->SetAttenuationRadius(FMath::Max(0.0f, MuzzleFlashAttenuationRadius));
	MuzzleFlashLight->SetLightColor(MuzzleFlashColor);
	MuzzleFlashLight->SetIntensity(MuzzleFlashIntensity);

	PlayConfiguredSound(GunshotSound, bPlayGunshotSound2D, GetActorLocation());

	if (bEnableHitSequence && bCurrentShotTargetsLocalPlayer)
	{
		StartHitSequence();
	}
	OnGunFired.Broadcast();
}

void ASDSelfShotGunActor::FireEmptyRound()
{
	AnimState = EGunAnimState::EmptyImpact;
	bCurrentShotWasEmpty = true;
	HeldGunJitterElapsedTime = 0.0f;
	SetActorTransform(GetFirstPersonGunTransform());
	MuzzleFlashElapsedTime = 0.0f;
	MuzzleFlashLight->SetIntensity(0.0f);

	PlayConfiguredSound(EmptyShotSound, bPlayEmptyShotSound2D, HammerPivot->GetComponentLocation());
	if (bEnableEmptyShotShake)
	{
		if (bSelfShotCinematicCameraActive && ActiveSelfShotCinematicCamera)
		{
			PlayCinematicCameraSteppedShake(
				EmptyShotShakeHoldTime,
				EmptyShotShakeBlendOutTime,
				EmptyShotShakeRotationAmplitude,
				EmptyShotShakeLocationAmplitude,
				EmptyShotShakeStepInterval);
		}
		else if (AShowDownPlayerController* ShowDownController = Cast<AShowDownPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
		{
			ShowDownController->PlayFixedCameraSteppedShake(
				EmptyShotShakeHoldTime,
				EmptyShotShakeBlendOutTime,
				EmptyShotShakeRotationAmplitude,
				EmptyShotShakeLocationAmplitude,
				EmptyShotShakeStepInterval);
		}
	}
	OnGunEmptyFired.Broadcast();
}

void ASDSelfShotGunActor::FinishSequence()
{
	SetActorTransform(RestActorTransform);
	ResetTriggerAndHammer();
	if (!bKeepChamberRotationAfterShot)
	{
		ChamberCurrentRotation = ChamberInitialRotation;
		ChamberPivot->SetRelativeRotation(ChamberCurrentRotation);
	}
	GunMesh->SetCollisionEnabled(OriginalCollisionEnabled);
	MuzzleFlashLight->SetIntensity(0.0f);
	MuzzleFlashElapsedTime = 0.0f;
	StateElapsedTime = 0.0f;
	MechanismResetElapsedTime = 0.0f;
	bCurrentShotWasEmpty = false;
	bHasCachedFirstPersonPoseCamera = false;
	AnimState = EGunAnimState::Idle;
	OnGunSequenceFinished.Broadcast();
	BroadcastPresentationFinishedIfIdle();
}

void ASDSelfShotGunActor::StartMechanismAnimation()
{
	ChamberStartRotation = ChamberCurrentRotation;
	ChamberTargetRotation = ChamberCurrentRotation + ChamberStepRotationOffset;
	AnimState = EGunAnimState::Cocking;
	StateElapsedTime = 0.0f;

	if (TriggerPullSound)
	{
		if (bPlayTriggerPullSound2D)
		{
			UGameplayStatics::PlaySound2D(this, TriggerPullSound);
		}
		else
		{
			UGameplayStatics::PlaySoundAtLocation(this, TriggerPullSound, TriggerPivot->GetComponentLocation());
		}
	}
}

void ASDSelfShotGunActor::UpdateMechanismCocking()
{
	const float Alpha = FMath::Clamp(StateElapsedTime / MechanismCockTime, 0.0f, 1.0f);
	const float EasedAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f);

	TriggerPivot->SetRelativeRotation(LerpRotation(
		TriggerRestRotation,
		TriggerRestRotation + TriggerPulledRotationOffset,
		EasedAlpha));
	HammerPivot->SetRelativeRotation(LerpRotation(
		HammerRestRotation,
		HammerRestRotation + HammerCockedRotationOffset,
		EasedAlpha));
	ChamberPivot->SetRelativeRotation(LerpRotation(
		ChamberStartRotation,
		ChamberTargetRotation,
		EasedAlpha));

	if (Alpha >= 1.0f)
	{
		ChamberCurrentRotation = ChamberTargetRotation;
		AnimState = EGunAnimState::HammerReleasing;
		StateElapsedTime = 0.0f;
	}
}

void ASDSelfShotGunActor::UpdateHammerRelease()
{
	const float Alpha = FMath::Clamp(StateElapsedTime / HammerReleaseTime, 0.0f, 1.0f);
	const float EasedAlpha = FMath::InterpEaseIn(0.0f, 1.0f, Alpha, 3.0f);

	TriggerPivot->SetRelativeRotation(TriggerRestRotation + TriggerPulledRotationOffset);
	ChamberPivot->SetRelativeRotation(ChamberCurrentRotation);
	HammerPivot->SetRelativeRotation(LerpRotation(
		HammerRestRotation + HammerCockedRotationOffset,
		HammerRestRotation + HammerFiredRotationOffset,
		EasedAlpha));

	if (Alpha >= 1.0f)
	{
		FireGun();
	}
}

void ASDSelfShotGunActor::UpdateEmptyShotImpact()
{
	const float ImpactAlpha = EmptyShotImpactTime > KINDA_SMALL_NUMBER
		? FMath::Clamp(StateElapsedTime / EmptyShotImpactTime, 0.0f, 1.0f)
		: 1.0f;
	const float EasedImpactAlpha = FMath::InterpEaseIn(0.0f, 1.0f, ImpactAlpha, 4.0f);

	const FRotator TriggerImpactRotation = TriggerRestRotation
		+ TriggerPulledRotationOffset
		+ EmptyShotTriggerExtraPullRotationOffset;
	const FRotator HammerImpactRotation = HammerRestRotation + EmptyShotHammerImpactRotationOffset;

	TriggerPivot->SetRelativeRotation(LerpRotation(
		TriggerRestRotation + TriggerPulledRotationOffset,
		TriggerImpactRotation,
		EasedImpactAlpha));
	HammerPivot->SetRelativeRotation(LerpRotation(
		HammerRestRotation + HammerFiredRotationOffset,
		HammerImpactRotation,
		EasedImpactAlpha));
	ChamberPivot->SetRelativeRotation(ChamberCurrentRotation);

	if (StateElapsedTime >= EmptyShotImpactTime + EmptyShotImpactHoldTime)
	{
		MechanismResetStartTriggerRotation = TriggerPivot->GetRelativeRotation();
		MechanismResetStartHammerRotation = HammerPivot->GetRelativeRotation();
		AnimState = EGunAnimState::Fired;
		StateElapsedTime = 0.0f;
		MechanismResetElapsedTime = 0.0f;
	}
}

void ASDSelfShotGunActor::UpdateMechanismReset()
{
	if (!bEnableMechanismAnimation)
	{
		return;
	}

	const float Alpha = FMath::Clamp(MechanismResetElapsedTime / TriggerResetTime, 0.0f, 1.0f);
	const float EasedAlpha = FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 2.0f);
	TriggerPivot->SetRelativeRotation(LerpRotation(
		MechanismResetStartTriggerRotation,
		TriggerRestRotation,
		EasedAlpha));
	HammerPivot->SetRelativeRotation(LerpRotation(
		MechanismResetStartHammerRotation,
		HammerRestRotation,
		EasedAlpha));
	ChamberPivot->SetRelativeRotation(ChamberCurrentRotation);
}

void ASDSelfShotGunActor::ResetTriggerAndHammer()
{
	TriggerPivot->SetRelativeRotation(TriggerRestRotation);
	HammerPivot->SetRelativeRotation(HammerRestRotation);
}

void ASDSelfShotGunActor::StartSelfShotCinematicCamera()
{
	bSelfShotCinematicCameraActive = false;
	bSelfShotCinematicCameraStartPending = false;
	bSelfShotCinematicCameraHoldStarted = false;
	CinematicCameraStartElapsedTime = 0.0f;
	CinematicCameraElapsedTime = 0.0f;
	PreviousViewTarget = nullptr;
	ActiveSelfShotCinematicCamera = ForcedShotCamera.IsValid()
		? ForcedShotCamera.Get()
		: (bCurrentShotTargetsLocalPlayer ? SelfShotCinematicCamera.Get() : nullptr);

	if (!bUseSelfShotCinematicCamera)
	{
		return;
	}

	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (PlayerController && ActiveSelfShotCinematicCamera)
	{
		bSelfShotCinematicCameraStartPending = true;
		if (CinematicCameraStartDelay <= KINDA_SMALL_NUMBER)
		{
			ActivateSelfShotCinematicCamera();
		}
	}
}

void ASDSelfShotGunActor::ActivateSelfShotCinematicCamera()
{
	if (!bSelfShotCinematicCameraStartPending)
	{
		return;
	}

	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (PlayerController && ActiveSelfShotCinematicCamera)
	{
		if (!bHasCachedFirstPersonPoseCamera)
		{
			CaptureFirstPersonPoseCamera();
		}
		PreviousViewTarget = PlayerController->GetViewTarget();
		PlayerController->SetViewTargetWithBlend(
			ActiveSelfShotCinematicCamera,
			CinematicCameraBlendInTime,
			VTBlend_EaseInOut,
			CinematicCameraBlendExponent);
		bSelfShotCinematicCameraActive = true;
	}

	bSelfShotCinematicCameraStartPending = false;
}

void ASDSelfShotGunActor::UpdateSelfShotCinematicCamera(float DeltaSeconds)
{
	if (bSelfShotCinematicCameraStartPending)
	{
		CinematicCameraStartElapsedTime += DeltaSeconds;
		if (CinematicCameraStartElapsedTime >= CinematicCameraStartDelay)
		{
			ActivateSelfShotCinematicCamera();
		}
	}

	if (!bSelfShotCinematicCameraActive || !bSelfShotCinematicCameraHoldStarted)
	{
		return;
	}

	CinematicCameraElapsedTime += DeltaSeconds;
	if (CinematicCameraElapsedTime < CinematicCameraHoldTime)
	{
		return;
	}

	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		AActor* ReturnViewTarget = PreviousViewTarget.Get();
		if (!ReturnViewTarget)
		{
			ReturnViewTarget = PlayerController->GetPawn();
		}

		if (ReturnViewTarget)
		{
			PlayerController->SetViewTargetWithBlend(
				ReturnViewTarget,
				CinematicCameraBlendOutTime,
				VTBlend_EaseInOut,
				CinematicCameraBlendExponent);
		}
	}

	PreviousViewTarget = nullptr;
	bSelfShotCinematicCameraActive = false;
	bSelfShotCinematicCameraStartPending = false;
	bSelfShotCinematicCameraHoldStarted = false;
	ActiveSelfShotCinematicCamera = nullptr;
	BroadcastPresentationFinishedIfIdle();
}

void ASDSelfShotGunActor::CaptureFirstPersonPoseCamera()
{
	bHasCachedFirstPersonPoseCamera = false;

	if (const APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (const APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager)
		{
			CachedFirstPersonCameraLocation = CameraManager->GetCameraLocation();
			CachedFirstPersonCameraRotation = CameraManager->GetCameraRotation();
			bHasCachedFirstPersonPoseCamera = true;
		}
	}
}

FTransform ASDSelfShotGunActor::ApplyHeldGunJitter(const FTransform& BaseTransform) const
{
	if (!bEnableHeldGunJitter || HeldGunJitterStepInterval <= KINDA_SMALL_NUMBER)
	{
		return BaseTransform;
	}

	const float Step = FMath::FloorToFloat(HeldGunJitterElapsedTime / HeldGunJitterStepInterval);
	const float Seed = 173.0f;
	const FRotator RotationOffset(
		FMath::PerlinNoise1D(Step * 1.61f + Seed) * HeldGunJitterRotationAmplitude.Pitch,
		FMath::PerlinNoise1D(Step * 2.09f + Seed + 31.0f) * HeldGunJitterRotationAmplitude.Yaw,
		FMath::PerlinNoise1D(Step * 2.57f + Seed + 73.0f) * HeldGunJitterRotationAmplitude.Roll);
	const FRotationMatrix BaseMatrix(BaseTransform.Rotator());
	const FVector LocationOffset =
		BaseMatrix.GetScaledAxis(EAxis::X) * FMath::PerlinNoise1D(Step * 1.79f + Seed + 101.0f) * HeldGunJitterLocationAmplitude.X +
		BaseMatrix.GetScaledAxis(EAxis::Y) * FMath::PerlinNoise1D(Step * 2.23f + Seed + 151.0f) * HeldGunJitterLocationAmplitude.Y +
		BaseMatrix.GetScaledAxis(EAxis::Z) * FMath::PerlinNoise1D(Step * 2.71f + Seed + 211.0f) * HeldGunJitterLocationAmplitude.Z;

	return FTransform(
		BaseTransform.Rotator() + RotationOffset,
		BaseTransform.GetLocation() + LocationOffset,
		BaseTransform.GetScale3D());
}

void ASDSelfShotGunActor::PlayCinematicCameraSteppedShake(
	float HoldDuration,
	float BlendOutTime,
	FRotator RotationAmplitude,
	FVector LocationAmplitude,
	float StepInterval)
{
	if (!bSelfShotCinematicCameraActive || !ActiveSelfShotCinematicCamera)
	{
		return;
	}

	CinematicCameraShakeBaseTransform = ActiveSelfShotCinematicCamera->GetActorTransform();
	CinematicCameraShakeElapsedTime = 0.0f;
	CinematicCameraShakeHoldDuration = FMath::Max(0.0f, HoldDuration);
	CinematicCameraShakeBlendOutTime = FMath::Max(0.0f, BlendOutTime);
	CinematicCameraShakeStepInterval = FMath::Max(0.01f, StepInterval);
	CinematicCameraShakeSeed = FMath::FRandRange(10.0f, 10000.0f);
	CinematicCameraShakeRotationAmplitude = RotationAmplitude;
	CinematicCameraShakeLocationAmplitude = LocationAmplitude;
	bCinematicCameraShakeActive = true;
}

void ASDSelfShotGunActor::UpdateCinematicCameraSteppedShake(float DeltaSeconds)
{
	if (!bCinematicCameraShakeActive)
	{
		return;
	}

	if (!ActiveSelfShotCinematicCamera)
	{
		bCinematicCameraShakeActive = false;
		return;
	}

	const float TotalTime = CinematicCameraShakeHoldDuration + CinematicCameraShakeBlendOutTime;
	CinematicCameraShakeElapsedTime += DeltaSeconds;
	if (TotalTime <= KINDA_SMALL_NUMBER || CinematicCameraShakeElapsedTime >= TotalTime)
	{
		ActiveSelfShotCinematicCamera->SetActorTransform(CinematicCameraShakeBaseTransform);
		bCinematicCameraShakeActive = false;
		return;
	}

	const float Strength = CinematicCameraShakeElapsedTime <= CinematicCameraShakeHoldDuration
		? 1.0f
		: (CinematicCameraShakeBlendOutTime > KINDA_SMALL_NUMBER
			? 1.0f - FMath::Clamp(
				(CinematicCameraShakeElapsedTime - CinematicCameraShakeHoldDuration) / CinematicCameraShakeBlendOutTime,
				0.0f,
				1.0f)
			: 0.0f);
	const float Step = FMath::FloorToFloat(CinematicCameraShakeElapsedTime / CinematicCameraShakeStepInterval);
	const FRotator RotationOffset(
		FMath::PerlinNoise1D(Step * 1.73f + CinematicCameraShakeSeed) * CinematicCameraShakeRotationAmplitude.Pitch,
		FMath::PerlinNoise1D(Step * 2.11f + CinematicCameraShakeSeed + 31.0f) * CinematicCameraShakeRotationAmplitude.Yaw,
		FMath::PerlinNoise1D(Step * 2.67f + CinematicCameraShakeSeed + 73.0f) * CinematicCameraShakeRotationAmplitude.Roll);
	const FRotationMatrix BaseMatrix(CinematicCameraShakeBaseTransform.Rotator());
	const FVector LocationOffset =
		BaseMatrix.GetScaledAxis(EAxis::X) * FMath::PerlinNoise1D(Step * 1.91f + CinematicCameraShakeSeed + 101.0f) * CinematicCameraShakeLocationAmplitude.X +
		BaseMatrix.GetScaledAxis(EAxis::Y) * FMath::PerlinNoise1D(Step * 2.29f + CinematicCameraShakeSeed + 151.0f) * CinematicCameraShakeLocationAmplitude.Y +
		BaseMatrix.GetScaledAxis(EAxis::Z) * FMath::PerlinNoise1D(Step * 2.83f + CinematicCameraShakeSeed + 211.0f) * CinematicCameraShakeLocationAmplitude.Z;

	ActiveSelfShotCinematicCamera->SetActorTransform(FTransform(
		CinematicCameraShakeBaseTransform.Rotator() + RotationOffset * Strength,
		CinematicCameraShakeBaseTransform.GetLocation() + LocationOffset * Strength,
		CinematicCameraShakeBaseTransform.GetScale3D()));
}

FRotator ASDSelfShotGunActor::LerpRotation(const FRotator& From, const FRotator& To, float Alpha)
{
	return FQuat::Slerp(From.Quaternion(), To.Quaternion(), FMath::Clamp(Alpha, 0.0f, 1.0f)).Rotator();
}

void ASDSelfShotGunActor::StartHitSequence()
{
	ASDArtToneController* Controller = ResolveHitSequenceArtToneController();
	if (Controller)
	{
		HitSequenceBaseSettings = Controller->GetCurrentSettings();
		Controller->ApplyArtTone(InitialHitEffectSettings);
	}

	if (bSelfShotCinematicCameraActive && ActiveSelfShotCinematicCamera)
	{
		PlayCinematicCameraSteppedShake(
			InitialHitEffectDuration,
			0.0f,
			InitialHitShakeRotationAmplitude,
			InitialHitShakeLocationAmplitude,
			InitialHitShakeStepInterval);
	}
	else if (AShowDownPlayerController* ShowDownController = Cast<AShowDownPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
	{
		ShowDownController->PlayFixedCameraSteppedShake(
			InitialHitEffectDuration,
			0.0f,
			InitialHitShakeRotationAmplitude,
			InitialHitShakeLocationAmplitude,
			InitialHitShakeStepInterval);
	}

	HitSequenceState = EHitSequenceState::InitialHit;
	HitSequenceElapsedTime = 0.0f;
	if (InitialHitEffectDuration <= KINDA_SMALL_NUMBER)
	{
		EnterHitSequenceBlackout();
	}
}

void ASDSelfShotGunActor::UpdateHitSequence(float DeltaSeconds)
{
	if (HitSequenceState == EHitSequenceState::Idle)
	{
		return;
	}

	HitSequenceElapsedTime += DeltaSeconds;

	switch (HitSequenceState)
	{
	case EHitSequenceState::InitialHit:
		if (HitSequenceElapsedTime >= InitialHitEffectDuration)
		{
			EnterHitSequenceBlackout();
		}
		break;
	case EHitSequenceState::Blackout:
		if (HitSequenceElapsedTime >= HitBlackoutDuration)
		{
			EnterHitSequenceRecovery();
		}
		break;
	case EHitSequenceState::RecoveryHold:
		if (HitSequenceElapsedTime >= RecoveryHitEffectHoldTime)
		{
			HitSequenceState = EHitSequenceState::RecoveryBlendOut;
		}
		break;
	case EHitSequenceState::RecoveryBlendOut:
	{
		ASDArtToneController* Controller = ResolveHitSequenceArtToneController();
		const float EffectBlendElapsed = FMath::Max(0.0f, HitSequenceElapsedTime - RecoveryHitEffectHoldTime);
		const float EffectBlendAlpha = RecoveryHitEffectBlendOutTime > KINDA_SMALL_NUMBER
			? FMath::Clamp(EffectBlendElapsed / RecoveryHitEffectBlendOutTime, 0.0f, 1.0f)
			: 1.0f;
		const float EasedAlpha = ApplyEase(
			EffectBlendAlpha,
			RecoveryHitEffectEaseMode,
			RecoveryHitEffectEaseExponent);
		if (Controller)
		{
			Controller->ApplyArtTone(LerpArtToneSettings(RecoveryHitEffectSettings, HitSequenceBaseSettings, EasedAlpha));
		}

		const float EffectTotalTime = RecoveryHitEffectHoldTime + RecoveryHitEffectBlendOutTime;
		const float ShakeTotalTime = RecoveryHitShakeHoldTime + RecoveryHitShakeBlendOutTime;
		if (HitSequenceElapsedTime >= FMath::Max(EffectTotalTime, ShakeTotalTime))
		{
			FinishHitSequence();
		}
		break;
	}
	default:
		break;
	}
}

void ASDSelfShotGunActor::EnterHitSequenceBlackout()
{
	SetBlackoutInstant(FMath::Clamp(HitBlackoutAmount, 0.0f, 1.0f), true);
	HitSequenceState = EHitSequenceState::Blackout;
	HitSequenceElapsedTime = 0.0f;
}

void ASDSelfShotGunActor::EnterHitSequenceRecovery()
{
	ASDArtToneController* Controller = ResolveHitSequenceArtToneController();
	if (Controller)
	{
		Controller->ApplyArtTone(RecoveryHitEffectSettings);
	}

	SetBlackoutInstant(0.0f, false);
	StartTinnitusSound();
	if (bSelfShotCinematicCameraActive && ActiveSelfShotCinematicCamera)
	{
		PlayCinematicCameraSteppedShake(
			RecoveryHitShakeHoldTime,
			RecoveryHitShakeBlendOutTime,
			RecoveryHitShakeRotationAmplitude,
			RecoveryHitShakeLocationAmplitude,
			RecoveryHitShakeStepInterval);
	}
	else if (AShowDownPlayerController* ShowDownController = Cast<AShowDownPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
	{
		ShowDownController->PlayFixedCameraSteppedShake(
			RecoveryHitShakeHoldTime,
			RecoveryHitShakeBlendOutTime,
			RecoveryHitShakeRotationAmplitude,
			RecoveryHitShakeLocationAmplitude,
			RecoveryHitShakeStepInterval);
	}

	HitSequenceState = RecoveryHitEffectHoldTime > KINDA_SMALL_NUMBER
		? EHitSequenceState::RecoveryHold
		: EHitSequenceState::RecoveryBlendOut;
	HitSequenceElapsedTime = 0.0f;
}

void ASDSelfShotGunActor::FinishHitSequence()
{
	if (ASDArtToneController* Controller = ResolveHitSequenceArtToneController())
	{
		Controller->ApplyArtTone(HitSequenceBaseSettings);
	}

	SetBlackoutInstant(0.0f, false);
	HitSequenceState = EHitSequenceState::Idle;
	HitSequenceElapsedTime = 0.0f;
	BroadcastPresentationFinishedIfIdle();
}

void ASDSelfShotGunActor::BroadcastPresentationFinishedIfIdle()
{
	if (!bPresentationFinishPending)
	{
		return;
	}

	if (AnimState != EGunAnimState::Idle
		|| HitSequenceState != EHitSequenceState::Idle
		|| bSelfShotCinematicCameraActive
		|| bSelfShotCinematicCameraStartPending)
	{
		return;
	}

	bPresentationFinishPending = false;
	OnGunPresentationFinished.Broadcast();
}

void ASDSelfShotGunActor::StartTinnitusSound()
{
	StopTinnitusSound();

	if (!TinnitusSound || TinnitusPlayDuration <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	TinnitusAudioComponent = UGameplayStatics::SpawnSound2D(
		this,
		TinnitusSound,
		TinnitusVolumeMultiplier,
		1.0f,
		TinnitusStartTime,
		nullptr,
		false,
		false);

	if (TinnitusAudioComponent)
	{
		TinnitusElapsedTime = 0.0f;
		bTinnitusFadeOutStarted = false;
		TinnitusAudioComponent->FadeIn(
			FMath::Min(TinnitusFadeInDuration, TinnitusPlayDuration),
			TinnitusVolumeMultiplier,
			TinnitusStartTime);
	}
}

void ASDSelfShotGunActor::UpdateTinnitusSound(float DeltaSeconds)
{
	if (!TinnitusAudioComponent)
	{
		return;
	}

	TinnitusElapsedTime += DeltaSeconds;
	const float FadeOutDuration = FMath::Min(TinnitusFadeOutDuration, TinnitusPlayDuration);
	const float FadeOutStartTime = FMath::Max(0.0f, TinnitusPlayDuration - FadeOutDuration);

	if (!bTinnitusFadeOutStarted && TinnitusElapsedTime >= FadeOutStartTime)
	{
		bTinnitusFadeOutStarted = true;
		if (FadeOutDuration > KINDA_SMALL_NUMBER)
		{
			TinnitusAudioComponent->FadeOut(FadeOutDuration, 0.0f);
		}
		else
		{
			TinnitusAudioComponent->Stop();
		}
	}

	if (TinnitusElapsedTime >= TinnitusPlayDuration)
	{
		StopTinnitusSound();
	}
}

void ASDSelfShotGunActor::StopTinnitusSound()
{
	if (TinnitusAudioComponent)
	{
		TinnitusAudioComponent->Stop();
		TinnitusAudioComponent->DestroyComponent();
		TinnitusAudioComponent = nullptr;
	}

	TinnitusElapsedTime = 0.0f;
	bTinnitusFadeOutStarted = false;
}

void ASDSelfShotGunActor::SetBlackoutInstant(float Alpha, bool bHoldWhenFinished)
{
	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (PlayerController->PlayerCameraManager)
		{
			PlayerController->PlayerCameraManager->StartCameraFade(
				Alpha > 0.0f ? 0.0f : FMath::Clamp(HitBlackoutAmount, 0.0f, 1.0f),
				Alpha,
				0.001f,
				FLinearColor::Black,
				false,
				bHoldWhenFinished);
		}
	}
}

ASDArtToneController* ASDSelfShotGunActor::ResolveHitSequenceArtToneController()
{
	if (HitSequenceArtToneController)
	{
		return HitSequenceArtToneController;
	}

	HitSequenceArtToneController = Cast<ASDArtToneController>(
		UGameplayStatics::GetActorOfClass(this, ASDArtToneController::StaticClass()));
	return HitSequenceArtToneController;
}

bool ASDSelfShotGunActor::ResolveCurrentShotIsLive() const
{
	switch (ShotResultMode)
	{
	case ESDSelfShotRoundMode::AlwaysEmpty:
		return false;
	case ESDSelfShotRoundMode::RandomChance:
		return FMath::FRand() <= FMath::Clamp(LiveRoundChance, 0.0f, 1.0f);
	case ESDSelfShotRoundMode::ChamberPattern:
		return IsChamberLive(CurrentChamberIndex);
	case ESDSelfShotRoundMode::AlwaysLive:
	default:
		return true;
	}
}

void ASDSelfShotGunActor::ConsumeCurrentChamberIfNeeded(bool bLiveShot)
{
	if (ShotResultMode == ESDSelfShotRoundMode::ChamberPattern
		&& bConsumeLiveChamberAfterShot
		&& bLiveShot)
	{
		SetChamberLive(CurrentChamberIndex, false);
	}
}

void ASDSelfShotGunActor::AdvanceCurrentChamberIndex()
{
	CurrentChamberIndex = (FMath::Clamp(CurrentChamberIndex, 0, 5) + 1) % 6;
}

bool ASDSelfShotGunActor::IsChamberLive(int32 ChamberIndex) const
{
	switch (FMath::Clamp(ChamberIndex, 0, 5))
	{
	case 0:
		return bChamber1Live;
	case 1:
		return bChamber2Live;
	case 2:
		return bChamber3Live;
	case 3:
		return bChamber4Live;
	case 4:
		return bChamber5Live;
	case 5:
		return bChamber6Live;
	default:
		return false;
	}
}

void ASDSelfShotGunActor::SetChamberLive(int32 ChamberIndex, bool bLive)
{
	switch (FMath::Clamp(ChamberIndex, 0, 5))
	{
	case 0:
		bChamber1Live = bLive;
		break;
	case 1:
		bChamber2Live = bLive;
		break;
	case 2:
		bChamber3Live = bLive;
		break;
	case 3:
		bChamber4Live = bLive;
		break;
	case 4:
		bChamber5Live = bLive;
		break;
	case 5:
		bChamber6Live = bLive;
		break;
	default:
		break;
	}
}

void ASDSelfShotGunActor::PlayConfiguredSound(USoundBase* Sound, bool bPlay2D, const FVector& Location) const
{
	if (!Sound)
	{
		return;
	}

	if (bPlay2D)
	{
		UGameplayStatics::PlaySound2D(this, Sound);
	}
	else
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Location);
	}
}

void ASDSelfShotGunActor::HandleMultiplayerRoulettePresentation(
	EShowDownPlayerSlot TargetSlot,
	const FString& TargetName,
	int32 BulletCount,
	bool bHit)
{
	PlayMultiplayerRoulettePresentation(TargetSlot, bHit);
}

AActor* ASDSelfShotGunActor::FindMultiplayerShotTarget(EShowDownPlayerSlot TargetSlot) const
{
	if (TargetSlot == EShowDownPlayerSlot::None)
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AShowDownCharacter> It(World); It; ++It)
	{
		AShowDownCharacter* CandidateCharacter = *It;
		if (IsValid(CandidateCharacter)
			&& CandidateCharacter->IsCharacterSceneActive()
			&& CandidateCharacter->IsAssignedToSlot(TargetSlot))
		{
			return CandidateCharacter;
		}
	}

	return nullptr;
}

void ASDSelfShotGunActor::PlayMultiplayerRoulettePresentation(EShowDownPlayerSlot TargetSlot, bool bHit)
{
	AActor* TargetActor = FindMultiplayerShotTarget(TargetSlot);
	if (!IsValid(TargetActor))
	{
		UseGunWithForcedResult(bHit);
		return;
	}

	FVector SourceLocation = FVector::ZeroVector;
	FVector AimLocation = FVector::ZeroVector;
	FRotator RotationOffset = FRotator::ZeroRotator;
	if (TryResolveCharacterPresentationShot(Cast<AShowDownCharacter>(TargetActor), SourceLocation, AimLocation, &RotationOffset))
	{
		UseGunWithForcedResultAtTargetFromLocationAimRotationAndCamera(
			bHit,
			TargetActor,
			SourceLocation,
			AimLocation,
			RotationOffset,
			nullptr);
		return;
	}

	UseGunWithForcedResultAtTarget(bHit, TargetActor);
}

bool ASDSelfShotGunActor::ShouldTreatTargetAsLocalPlayer(AActor* TargetActor) const
{
	if (!IsValid(TargetActor))
	{
		return true;
	}

	const AShowDownCharacter* TargetCharacter = Cast<AShowDownCharacter>(TargetActor);
	return TargetCharacter && TargetCharacter->IsLocalPlayerCharacter();
}

bool ASDSelfShotGunActor::UpdateRevolverPlacementDevPreview()
{
#if WITH_EDITOR
	if (!bEnableRevolverPlacementDevMode)
	{
		if (bRevolverPlacementDevPreviewActive)
		{
			SetActorTransform(RevolverPlacementDevPreviewRestoreTransform);
			bRevolverPlacementDevPreviewActive = false;
		}

		return false;
	}

	if (!bRevolverPlacementDevPreviewActive)
	{
		RevolverPlacementDevPreviewRestoreTransform = GetActorTransform();
		bRevolverPlacementDevPreviewActive = true;
	}

	const AShowDownCharacter* PreviewTarget = FindRevolverPlacementDevPreviewTarget();
	if (!PreviewTarget)
	{
		return true;
	}

	FVector SourceLocation = FVector::ZeroVector;
	FVector AimLocation = FVector::ZeroVector;
	FRotator RotationOffset = FRotator::ZeroRotator;
	if (!TryResolveCharacterPresentationShot(PreviewTarget, SourceLocation, AimLocation, &RotationOffset))
	{
		return true;
	}

	const FTransform PreviewTransform = MakeTargetShotTransform(SourceLocation, AimLocation, RotationOffset);
	SetActorTransform(PreviewTransform);
	DrawRevolverPlacementDevPreview(SourceLocation, AimLocation, PreviewTransform);
	return true;
#else
	return false;
#endif
}

AShowDownCharacter* ASDSelfShotGunActor::FindRevolverPlacementDevPreviewTarget() const
{
	if (IsValid(DevPreviewTargetCharacter))
	{
		return DevPreviewTargetCharacter;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	AShowDownCharacter* PlayerOneFallback = nullptr;
	AShowDownCharacter* PlayerRoleFallback = nullptr;
	AShowDownCharacter* AnyCharacterFallback = nullptr;
	for (TActorIterator<AShowDownCharacter> It(World); It; ++It)
	{
		AShowDownCharacter* CandidateCharacter = *It;
		if (!IsValid(CandidateCharacter))
		{
			continue;
		}

		if (CandidateCharacter->IsAssignedToSlot(DevPreviewTargetSlot))
		{
			return CandidateCharacter;
		}

		if (!AnyCharacterFallback)
		{
			AnyCharacterFallback = CandidateCharacter;
		}

		if (!PlayerRoleFallback && CandidateCharacter->GetCharacterRole() == EShowDownCharacterRole::Player)
		{
			PlayerRoleFallback = CandidateCharacter;
		}

		if (!PlayerOneFallback && CandidateCharacter->IsAssignedToSlot(EShowDownPlayerSlot::Player1))
		{
			PlayerOneFallback = CandidateCharacter;
		}
	}

	if (!bDevPreviewFallbackToAnyPlayerCharacter)
	{
		return nullptr;
	}

	if (PlayerOneFallback)
	{
		return PlayerOneFallback;
	}

	return PlayerRoleFallback ? PlayerRoleFallback : AnyCharacterFallback;
}

FTransform ASDSelfShotGunActor::MakeTargetShotTransform(
	const FVector& SourceLocation,
	const FVector& AimLocation,
	FRotator RotationOffset) const
{
	FVector AimDirection = (AimLocation - SourceLocation).GetSafeNormal();
	if (AimDirection.IsNearlyZero())
	{
		AimDirection = GetActorForwardVector().GetSafeNormal();
	}
	if (AimDirection.IsNearlyZero())
	{
		AimDirection = FVector::ForwardVector;
	}

	return FTransform(
		AimDirection.Rotation() + TargetShotRotationOffset + RotationOffset,
		SourceLocation,
		GetPresentationScale3D());
}

FVector ASDSelfShotGunActor::GetPresentationScale3D() const
{
	return bHasCapturedRestActorTransform
		? RestActorTransform.GetScale3D()
		: GetActorScale3D();
}

void ASDSelfShotGunActor::DrawRevolverPlacementDevPreview(
	const FVector& SourceLocation,
	const FVector& AimLocation,
	const FTransform& GunTransform) const
{
	if (!bDrawRevolverPlacementDevDebug)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float DebugSize = FMath::Max(1.0f, RevolverPlacementDevDebugSize);
	DrawDebugSphere(World, SourceLocation, DebugSize * 0.35f, 12, FColor::Cyan, false, 0.0f, 0, 1.5f);
	DrawDebugSphere(World, AimLocation, DebugSize * 0.25f, 12, FColor::Red, false, 0.0f, 0, 1.5f);
	DrawDebugLine(World, SourceLocation, AimLocation, FColor::Yellow, false, 0.0f, 0, 1.0f);
	DrawDebugCoordinateSystem(World, SourceLocation, GunTransform.Rotator(), DebugSize, false, 0.0f, 0, 0.75f);
}

FTransform ASDSelfShotGunActor::GetFirstPersonGunTransform() const
{
	if (!bUseFirstPersonPose)
	{
		return RestActorTransform;
	}

	if (bHasCachedFirstPersonPoseCamera)
	{
		const FRotationMatrix CameraMatrix(CachedFirstPersonCameraRotation);
		const FVector TargetLocation = bHasForcedShotSourceLocation
			? ForcedShotSourceLocation
			: CachedFirstPersonCameraLocation
				+ CameraMatrix.GetScaledAxis(EAxis::X) * FirstPersonCameraOffset.X
				+ CameraMatrix.GetScaledAxis(EAxis::Y) * FirstPersonCameraOffset.Y
				+ CameraMatrix.GetScaledAxis(EAxis::Z) * FirstPersonCameraOffset.Z;

		if (bHasForcedShotAimLocation || ForcedShotTargetActor.IsValid())
		{
			const FVector AimLocation = bHasForcedShotAimLocation
				? ForcedShotAimLocation
				: ForcedShotTargetActor->GetActorLocation() + TargetShotAimOffset;
			return MakeTargetShotTransform(
				TargetLocation,
				AimLocation,
				bHasForcedShotRotationOffset ? ForcedShotRotationOffset : FRotator::ZeroRotator);
		}

		return FTransform(
			CachedFirstPersonCameraRotation + FirstPersonRotationOffset,
			TargetLocation,
			GetPresentationScale3D());
	}

	if (const APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (const APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager)
		{
			const FRotator CameraRotation = CameraManager->GetCameraRotation();
			const FRotationMatrix CameraMatrix(CameraRotation);
			const FVector CameraLocation = CameraManager->GetCameraLocation();
			const FVector TargetLocation = bHasForcedShotSourceLocation
				? ForcedShotSourceLocation
				: CameraLocation
					+ CameraMatrix.GetScaledAxis(EAxis::X) * FirstPersonCameraOffset.X
					+ CameraMatrix.GetScaledAxis(EAxis::Y) * FirstPersonCameraOffset.Y
					+ CameraMatrix.GetScaledAxis(EAxis::Z) * FirstPersonCameraOffset.Z;

			if (bHasForcedShotAimLocation || ForcedShotTargetActor.IsValid())
			{
				const FVector AimLocation = bHasForcedShotAimLocation
					? ForcedShotAimLocation
					: ForcedShotTargetActor->GetActorLocation() + TargetShotAimOffset;
				return MakeTargetShotTransform(
					TargetLocation,
					AimLocation,
					bHasForcedShotRotationOffset ? ForcedShotRotationOffset : FRotator::ZeroRotator);
			}

			return FTransform(CameraRotation + FirstPersonRotationOffset, TargetLocation, GetPresentationScale3D());
		}
	}

	return RestActorTransform;
}

void ASDSelfShotGunActor::SetActorTransformAlpha(const FTransform& FromTransform, const FTransform& ToTransform, float Alpha)
{
	const FVector NewLocation = FMath::Lerp(FromTransform.GetLocation(), ToTransform.GetLocation(), Alpha);
	const FQuat NewRotation = FQuat::Slerp(FromTransform.GetRotation(), ToTransform.GetRotation(), Alpha);
	const FVector NewScale = FMath::Lerp(FromTransform.GetScale3D(), ToTransform.GetScale3D(), Alpha);
	SetActorTransform(FTransform(NewRotation, NewLocation, NewScale));
}
