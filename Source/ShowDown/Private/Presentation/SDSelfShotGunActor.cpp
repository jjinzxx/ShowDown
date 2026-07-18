#include "Presentation/SDSelfShotGunActor.h"

#include "Audio/ShowDownAudioSubsystem.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "ShowDownCharacter.h"
#include "ShowDownCameraAspect.h"
#include "ShowDownAmmoStatusWidget.h"
#include "ShowDownGameStateBase.h"
#include "ShowDownPlayerController.h"
#include "SDPlayerState.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float MinimumCinematicCameraHoldTime = 1.8f;
	constexpr float GunshotVolumeMultiplier = 0.8f;
	constexpr int32 RevolverBulletSlotCount = 6;

	bool IsRaiseBulletLoadTerminalPhase(EShowDownPhase Phase)
	{
		return Phase == EShowDownPhase::Reveal
			|| Phase == EShowDownPhase::Roulette
			|| Phase == EShowDownPhase::RoundEnd
			|| Phase == EShowDownPhase::GameOver;
	}

	APlayerController* FindLocalPlayerController(const UObject* WorldContextObject)
	{
		const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
		if (!World)
		{
			return nullptr;
		}

		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			APlayerController* PlayerController = Iterator->Get();
			if (PlayerController
				&& PlayerController->IsLocalController()
				&& PlayerController->GetLocalPlayer())
			{
				return PlayerController;
			}
		}

		return nullptr;
	}

	UShowDownAudioSubsystem* FindShowDownAudioSubsystem(const UObject* WorldContextObject)
	{
		const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UShowDownAudioSubsystem>() : nullptr;
	}

	void HideCameraVisualization(ACameraActor* CameraActor)
	{
		if (!IsValid(CameraActor))
		{
			return;
		}

		// A hidden camera actor still provides its camera view, but none of its
		// renderable helper components can appear when another camera looks back
		// at it. This covers both the authored map camera and the transient local
		// copy used for seat-relative gun-shot presentation.
		CameraActor->SetActorHiddenInGame(true);
		CameraActor->SetActorEnableCollision(false);

#if WITH_EDITORONLY_DATA
		if (UCameraComponent* CameraComponent = CameraActor->GetCameraComponent())
		{
			CameraComponent->bCameraMeshHiddenInGame = true;
			CameraComponent->SetCameraMesh(nullptr);
		}

		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(CameraActor);
		for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (PrimitiveComponent && PrimitiveComponent->IsVisualizationComponent())
			{
				PrimitiveComponent->SetHiddenInGame(true, true);
				PrimitiveComponent->SetVisibility(false, true);
			}
		}
#endif
	}

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
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	// The opening card showcase owns the gun's first reveal. Hide the map-placed
	// actor at construction time so it cannot flash on screen before BeginPlay or
	// before the replicated showcase state reaches multiplayer clients.
	SetActorHiddenInGame(true);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	AmmoStatusAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("AmmoStatusAnchor"));
	AmmoStatusAnchor->SetupAttachment(SceneRoot);

	AmmoStatusWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("AmmoStatusWidget"));
	AmmoStatusWidgetComponent->SetupAttachment(AmmoStatusAnchor);
	AmmoStatusWidgetComponent->SetWidgetClass(UShowDownAmmoStatusWidget::StaticClass());
	AmmoStatusWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	AmmoStatusWidgetComponent->SetDrawAtDesiredSize(false);
	AmmoStatusWidgetComponent->SetPivot(FVector2D(0.5f, 0.5f));
	AmmoStatusWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AmmoStatusWidgetComponent->SetGenerateOverlapEvents(false);
	AmmoStatusWidgetComponent->SetVisibility(false);
	AmmoStatusWidgetComponent->SetHiddenInGame(true);

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

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BettingBulletMeshFinder(
		TEXT("/Game/Fab/Revolver/bulletBetting.bulletBetting"));
	for (int32 BulletIndex = 0; BulletIndex < RevolverBulletSlotCount; ++BulletIndex)
	{
		UStaticMeshComponent* BettingBulletMesh = CreateDefaultSubobject<UStaticMeshComponent>(
			*FString::Printf(TEXT("BettingBulletMesh%02d"), BulletIndex + 1));
		BettingBulletMesh->SetupAttachment(ChamberPivot);
		BettingBulletMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BettingBulletMesh->SetCanEverAffectNavigation(false);
		BettingBulletMesh->SetCastShadow(false);
		BettingBulletMesh->SetVisibility(false, true);
		BettingBulletMesh->SetHiddenInGame(true, true);
		if (BettingBulletMeshFinder.Succeeded())
		{
			BettingBulletMesh->SetStaticMesh(BettingBulletMeshFinder.Object);
		}
		BettingBulletMeshes.Add(BettingBulletMesh);
	}

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
	MuzzleFlashLight->SetIntensityUnits(ELightUnits::Lumens);
	MuzzleFlashLight->SetAttenuationRadius(
		FMath::Max(0.0f, MuzzleFlashAttenuationRadius));
	MuzzleFlashLight->SetLightColor(MuzzleFlashColor);
	MuzzleFlashLight->SetCastShadows(true);
	MuzzleFlashLight->SetIndirectLightingIntensity(0.0f);
	MuzzleFlashLight->SetVolumetricScatteringIntensity(0.0f);

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

void ASDSelfShotGunActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyAmmoStatusDisplaySettings();
#if WITH_EDITOR
	if (!GetWorld() || !GetWorld()->IsGameWorld())
	{
		SetActorTickEnabled(bEnableRevolverPlacementDevMode || bRevolverPlacementDevPreviewActive);
	}
#endif
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
	ApplyAmmoStatusDisplaySettings();
	CacheBulletRestRelativeTransforms();
	OnRep_TableStatus();

	RestActorTransform = GetActorTransform();
	bHasCapturedRestActorTransform = true;
	OriginalCollisionEnabled = GunMesh->GetCollisionEnabled();
	OriginalInteractionCollisionEnabled = InteractionBounds->GetCollisionEnabled();
	if (bOpeningCardShowcaseStowed)
	{
		StageOpeningCardDrop();
	}
	TriggerRestRotation = TriggerPivot->GetRelativeRotation();
	HammerRestRotation = HammerPivot->GetRelativeRotation();
	MechanismResetStartTriggerRotation = TriggerRestRotation;
	MechanismResetStartHammerRotation = HammerRestRotation;
	ChamberInitialRotation = ChamberPivot->GetRelativeRotation();
	ChamberCurrentRotation = ChamberInitialRotation;
	ChamberStartRotation = ChamberCurrentRotation;
	ChamberTargetRotation = ChamberCurrentRotation;
	MuzzleFlashLight->SetAttenuationRadius(
		FMath::Max(0.0f, MuzzleFlashAttenuationRadius));
	MuzzleFlashLight->SetIntensityUnits(ELightUnits::Lumens);
	MuzzleFlashLight->SetLightColor(MuzzleFlashColor);
	if (IsValid(SelfShotCinematicCamera))
	{
		GunShotCameraReferenceTransform = SelfShotCinematicCamera->GetActorTransform();
		bHasGunShotCameraReferenceTransform = true;
		HideCameraVisualization(SelfShotCinematicCamera);
	}

	if (UWorld* World = GetWorld())
	{
		World->GameStateSetEvent.AddUObject(this, &ASDSelfShotGunActor::HandleGameStateSet);
		HandleGameStateSet(World->GetGameState());
	}

	RefreshRuntimeTickState();
}

void ASDSelfShotGunActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bHitSequenceBlackoutActive)
	{
		SetBlackoutInstant(0.0f, false);
	}
	CancelSelfShotCinematicCamera();
	if (IsValid(LocalGunShotCamera))
	{
		LocalGunShotCamera->Destroy();
		LocalGunShotCamera = nullptr;
	}

	if (UWorld* World = GetWorld())
	{
		World->GameStateSetEvent.RemoveAll(this);
	}
	if (AShowDownGameStateBase* ShowDownGameState = BoundShowDownGameState.Get())
	{
		ShowDownGameState->OnPhaseChanged.RemoveDynamic(
			this,
			&ASDSelfShotGunActor::HandleGamePhaseChanged);
		ShowDownGameState->OnTableCinematicCue.RemoveDynamic(
			this,
			&ASDSelfShotGunActor::HandleTableCinematicCue);
		ShowDownGameState->OnMultiplayerRoulettePresentationContext.RemoveAll(this);
		ShowDownGameState->OnMultiplayerPresentationContextChanged.RemoveAll(this);
	}
	BoundShowDownGameState.Reset();

	Super::EndPlay(EndPlayReason);
}

void ASDSelfShotGunActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateAmmoStatusAnchorLocation();

	if (MuzzleFlashElapsedTime > 0.0f)
	{
		MuzzleFlashElapsedTime = FMath::Max(0.0f, MuzzleFlashElapsedTime - DeltaSeconds);
		MuzzleFlashLight->SetIntensity(
			MuzzleFlashElapsedTime > 0.0f
				? FMath::Max(0.0f, MuzzleFlashIntensity)
				: 0.0f);
	}

	UpdateHitSequence(DeltaSeconds);
	UpdateTinnitusSound(DeltaSeconds);
	UpdateSelfShotCinematicCamera(DeltaSeconds);
	UpdateCinematicCameraSteppedShake(DeltaSeconds);
	TryStartPendingRaiseBulletLoadPresentation();
	UpdateRaiseBulletLoadAnimation(DeltaSeconds);

	if (AnimState == EGunAnimState::Idle)
	{
		UpdateOpeningCardDrop(DeltaSeconds);
		UpdateRevolverPlacementDevPreview();
		RefreshRuntimeTickState();
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
		SetActorTransformAlpha(RaiseStartTransform, GetPresentationGunTransform(), FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 3.0f));
		if (Alpha >= 1.0f)
		{
			AnimState = EGunAnimState::Aiming;
			StateElapsedTime = 0.0f;
		}
		break;
	}
	case EGunAnimState::Aiming:
		SetActorTransform(ApplyHeldGunJitter(GetPresentationGunTransform()));
		if (StateElapsedTime >= AimHoldTime)
		{
			if (bEnableMechanismAnimation)
			{
				StartMechanismAnimation();
			}
			else
			{
				OnTriggerPullStarted.Broadcast();
				FireGun();
			}
		}
		break;
	case EGunAnimState::Cocking:
		SetActorTransform(ApplyHeldGunJitter(GetPresentationGunTransform()));
		UpdateMechanismCocking();
		break;
	case EGunAnimState::HammerReleasing:
		SetActorTransform(ApplyHeldGunJitter(GetPresentationGunTransform()));
		UpdateHammerRelease();
		break;
	case EGunAnimState::EmptyImpact:
		SetActorTransform(GetPresentationGunTransform());
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

	RefreshRuntimeTickState();
}

void ASDSelfShotGunActor::UseGun()
{
	ForcedShotTargetActor = nullptr;
	ForcedShotCamera = nullptr;
	bHasForcedShotSourceLocation = false;
	bHasForcedShotAimLocation = false;
	bHasForcedShotRotationOffset = false;
	bCurrentShotTargetsLocalPlayer = true;
	CurrentShotTargetSlot = EShowDownPlayerSlot::None;
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
	bCurrentShotTargetsLocalPlayer = ShouldTreatTargetAsLocalPlayer(nullptr);
	CurrentShotTargetSlot = EShowDownPlayerSlot::None;
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
	CurrentShotTargetSlot = Cast<AShowDownCharacter>(TargetActor)
		? CastChecked<AShowDownCharacter>(TargetActor)->GetPlayerSlot()
		: EShowDownPlayerSlot::None;
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
	CurrentShotTargetSlot = Cast<AShowDownCharacter>(TargetActor)
		? CastChecked<AShowDownCharacter>(TargetActor)->GetPlayerSlot()
		: EShowDownPlayerSlot::None;
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
	CurrentShotTargetSlot = Cast<AShowDownCharacter>(TargetActor)
		? CastChecked<AShowDownCharacter>(TargetActor)->GetPlayerSlot()
		: EShowDownPlayerSlot::None;
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
	CurrentShotTargetSlot = Cast<AShowDownCharacter>(TargetActor)
		? CastChecked<AShowDownCharacter>(TargetActor)->GetPlayerSlot()
		: EShowDownPlayerSlot::None;
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
	CurrentShotTargetSlot = Cast<AShowDownCharacter>(TargetActor)
		? CastChecked<AShowDownCharacter>(TargetActor)->GetPlayerSlot()
		: EShowDownPlayerSlot::None;
	StartGunUse();
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

float ASDSelfShotGunActor::GetPresentationFinishDelay(bool bLiveRound) const
{
	const float ResolveDelay = GetShotResolveDelay();
	const float GunMotionAfterResolve = bLiveRound
		? FMath::Max(0.0f, ShotHoldTime) + FMath::Max(0.0f, ReturnTime)
		: FMath::Max(0.0f, EmptyShotImpactTime)
			+ FMath::Max(0.0f, EmptyShotImpactHoldTime)
			+ FMath::Max(0.0f, ShotHoldTime)
			+ FMath::Max(0.0f, ReturnTime);

	float FinishDelay = ResolveDelay + GunMotionAfterResolve;
	// Every local target shot owns a third-person camera, including an empty
	// chamber. Reserve the camera hold for both outcomes so round progression
	// cannot cancel an empty/fold shot before its view returns.
	{
		const float CameraExitDuration = bLiveRound && bUseEliminationTableOverview
			? FMath::Max(
				FMath::Max(0.0f, CinematicCameraBlendOutTime),
				FMath::Max(0.0f, EliminationOverviewMoveTime))
			: FMath::Max(0.0f, CinematicCameraBlendOutTime);
		FinishDelay = FMath::Max(
			FinishDelay,
			ResolveDelay
				+ FMath::Max(MinimumCinematicCameraHoldTime, CinematicCameraHoldTime)
				+ CameraExitDuration);
	}

	if (bLiveRound && bEnableHitSequence)
	{
		const float RecoveryEffectDuration =
			FMath::Max(0.0f, RecoveryHitEffectHoldTime)
			+ FMath::Max(0.0f, RecoveryHitEffectBlendOutTime);
		const float RecoveryShakeDuration =
			FMath::Max(0.0f, RecoveryHitShakeHoldTime)
			+ FMath::Max(0.0f, RecoveryHitShakeBlendOutTime);
		const float HitSequenceDuration =
			FMath::Max(0.0f, InitialHitEffectDuration)
			+ FMath::Max(0.0f, HitBlackoutDelay)
			+ FMath::Max(0.0f, HitBlackoutDuration)
			+ FMath::Max(RecoveryEffectDuration, RecoveryShakeDuration);
		FinishDelay = FMath::Max(FinishDelay, ResolveDelay + HitSequenceDuration);
	}

	return FinishDelay;
}

bool ASDSelfShotGunActor::IsMultiplayerRoulettePresentation() const
{
	return bMultiplayerRoulettePresentationActive;
}

bool ASDSelfShotGunActor::ShouldUseGunShotCamera(bool bTargetsLocalPlayer)
{
	return bTargetsLocalPlayer;
}

bool ASDSelfShotGunActor::ShouldUseEliminationTableOverview(
	bool bLiveRound,
	bool bTargetsLocalPlayer,
	int32 RemainingLives)
{
	return bLiveRound && bTargetsLocalPlayer && RemainingLives <= 0;
}

bool ASDSelfShotGunActor::IsGunShotTargetLocalPlayer(
	EShowDownPlayerSlot TargetSlot,
	EShowDownPlayerSlot LocalPlayerSlot)
{
	return TargetSlot != EShowDownPlayerSlot::None
		&& LocalPlayerSlot != EShowDownPlayerSlot::None
		&& TargetSlot == LocalPlayerSlot;
}

FTransform ASDSelfShotGunActor::BuildSeatRelativeGunShotCameraTransform(
	const FTransform& PlayerOneCameraTransform,
	const FTransform& PlayerOneCharacterTransform,
	const FTransform& TargetCharacterTransform)
{
	const FVector RelativeLocation = PlayerOneCharacterTransform.InverseTransformPosition(
		PlayerOneCameraTransform.GetLocation());
	const FQuat RelativeRotation = PlayerOneCharacterTransform.GetRotation().Inverse()
		* PlayerOneCameraTransform.GetRotation();

	FTransform Result;
	Result.SetLocation(TargetCharacterTransform.TransformPosition(RelativeLocation));
	Result.SetRotation((TargetCharacterTransform.GetRotation() * RelativeRotation).GetNormalized());
	Result.SetScale3D(PlayerOneCameraTransform.GetScale3D());
	return Result;
}

FTransform ASDSelfShotGunActor::BuildFallbackGunShotCameraTransform(
	const FVector& TableCenter,
	const FTransform& TargetCharacterTransform,
	float BackDistance,
	float SideDistance,
	float Height,
	float LookAtHeight)
{
	const FVector SeatLocation = TargetCharacterTransform.GetLocation();
	FVector DirectionToTable = TableCenter - SeatLocation;
	DirectionToTable.Z = 0.0f;
	if (!DirectionToTable.Normalize())
	{
		DirectionToTable = TargetCharacterTransform.GetUnitAxis(EAxis::X).GetSafeNormal2D();
	}
	if (DirectionToTable.IsNearlyZero())
	{
		DirectionToTable = FVector::ForwardVector;
	}

	FVector ScreenRight = FVector::CrossProduct(FVector::UpVector, DirectionToTable).GetSafeNormal();
	if (ScreenRight.IsNearlyZero())
	{
		ScreenRight = FVector::RightVector;
	}

	const FVector CameraLocation = SeatLocation
		- DirectionToTable * FMath::Max(0.0f, BackDistance)
		+ ScreenRight * SideDistance
		+ FVector::UpVector * Height;
	const FVector LookAtLocation = SeatLocation
		+ DirectionToTable * 30.0f
		+ FVector::UpVector * LookAtHeight;
	return FTransform(
		(LookAtLocation - CameraLocation).Rotation(),
		CameraLocation,
		FVector::OneVector);
}

FTransform ASDSelfShotGunActor::BuildEliminationTableOverviewTransform(
	const FVector& TableCenter,
	const FTransform& TargetCharacterTransform,
	float BackDistance,
	float Height,
	float LookAtHeight)
{
	const FVector SeatLocation = TargetCharacterTransform.GetLocation();
	FVector DirectionToTable = TableCenter - SeatLocation;
	DirectionToTable.Z = 0.0f;
	if (!DirectionToTable.Normalize())
	{
		DirectionToTable = TargetCharacterTransform.GetUnitAxis(EAxis::X).GetSafeNormal2D();
	}
	if (DirectionToTable.IsNearlyZero())
	{
		DirectionToTable = FVector::ForwardVector;
	}

	const FVector CameraLocation = SeatLocation
		- DirectionToTable * FMath::Max(0.0f, BackDistance)
		+ FVector::UpVector * Height;
	const FVector LookAtLocation = TableCenter + FVector::UpVector * LookAtHeight;
	return FTransform(
		(LookAtLocation - CameraLocation).Rotation(),
		CameraLocation,
		FVector::OneVector);
}

int32 ASDSelfShotGunActor::ResolveRaiseBulletLoadStartCount(
	int32 PreviousBet,
	int32 NewBet,
	bool bReloadAllBullets)
{
	const int32 ClampedNewBet = FMath::Clamp(NewBet, 0, RevolverBulletSlotCount);
	return bReloadAllBullets
		? 0
		: FMath::Clamp(PreviousBet, 0, ClampedNewBet);
}

float ASDSelfShotGunActor::CalculateRaiseBulletLoadSequenceDuration(
	int32 BulletCount,
	float BulletDuration,
	float StaggerDelay)
{
	const int32 ClampedBulletCount = FMath::Clamp(BulletCount, 0, RevolverBulletSlotCount);
	if (ClampedBulletCount <= 0)
	{
		return 0.0f;
	}

	return FMath::Max(0.0f, BulletDuration)
		+ static_cast<float>(ClampedBulletCount - 1) * FMath::Max(0.0f, StaggerDelay);
}

float ASDSelfShotGunActor::GetRaiseBulletLoadPresentationDuration(
	int32 PreviousBet,
	int32 NewBet) const
{
	const int32 ClampedPreviousBet = FMath::Clamp(PreviousBet, 0, RevolverBulletSlotCount);
	const int32 ClampedNewBet = FMath::Clamp(NewBet, 0, RevolverBulletSlotCount);
	if (!bEnableRaiseBulletLoadAnimation || ClampedNewBet <= ClampedPreviousBet)
	{
		return 0.0f;
	}

	const int32 StartCount = ResolveRaiseBulletLoadStartCount(
		ClampedPreviousBet,
		ClampedNewBet,
		bReloadAllBulletsOnRaise);
	return CalculateRaiseBulletLoadSequenceDuration(
		ClampedNewBet - StartCount,
		RaiseBulletLoadDuration,
		RaiseBulletLoadStaggerDelay);
}

void ASDSelfShotGunActor::SetTableStatus(
	int32 LiveRounds,
	int32 RemainingChambers,
	EShowDownPhase Phase,
	EShowDownPlayerSlot TurnSlot)
{
	if (!HasAuthority())
	{
		return;
	}

	StatusLiveRounds = FMath::Clamp(LiveRounds, 0, 6);
	StatusRemainingChambers = FMath::Clamp(RemainingChambers, 0, 6);
	StatusPhase = Phase;
	StatusTurnSlot = TurnSlot;
	OnRep_TableStatus();
	ForceNetUpdate();
}

float ASDSelfShotGunActor::PlayRaiseBulletLoadPresentation(
	int32 PreviousBet,
	int32 NewBet,
	EShowDownPlayerSlot SourceSlot)
{
	if (!HasAuthority())
	{
		return 0.0f;
	}

	const int32 ClampedPreviousBet = FMath::Clamp(PreviousBet, 0, RevolverBulletSlotCount);
	const int32 ClampedNewBet = FMath::Clamp(NewBet, 0, RevolverBulletSlotCount);
	if (ClampedNewBet <= ClampedPreviousBet)
	{
		return 0.0f;
	}

	const AShowDownGameStateBase* ShowDownGameState = BoundShowDownGameState.Get();
	if (!ShowDownGameState)
	{
		const UWorld* World = GetWorld();
		ShowDownGameState = World ? World->GetGameState<AShowDownGameStateBase>() : nullptr;
	}
	const int32 PresentationRound = ShowDownGameState
		? FMath::Max(0, ShowDownGameState->CurrentRound)
		: 0;
	MulticastPlayRaiseBulletLoadPresentation(
		ClampedPreviousBet,
		ClampedNewBet,
		PresentationRound,
		SourceSlot);
	return GetRaiseBulletLoadPresentationDuration(ClampedPreviousBet, ClampedNewBet);
}

void ASDSelfShotGunActor::MulticastPlayRaiseBulletLoadPresentation_Implementation(
	int32 PreviousBet,
	int32 NewBet,
	int32 PresentationRound,
	EShowDownPlayerSlot SourceSlot)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	ReceiveRaiseBulletLoadPresentation(PreviousBet, NewBet, PresentationRound, SourceSlot);
}

void ASDSelfShotGunActor::OnRep_TableStatus()
{
	SynchronizeBulletPresentationFromStatus();
	ApplyAmmoStatusDisplaySettings();
}

void ASDSelfShotGunActor::HandleGamePhaseChanged(EShowDownPhase NewPhase)
{
	StatusPhase = NewPhase;
	if (NewPhase == EShowDownPhase::Reveal || NewPhase == EShowDownPhase::Roulette)
	{
		bAmmoStatusEmphasisLatched = true;
		bAmmoStatusClearPending = false;
	}
	else if (NewPhase == EShowDownPhase::None
		|| NewPhase == EShowDownPhase::SelectCard
		|| NewPhase == EShowDownPhase::Betting)
	{
		const bool bShotPresentationActive =
			AnimState != EGunAnimState::Idle
			|| bMultiplayerRoulettePresentationActive
			|| !PendingMultiplayerRoulettePresentations.IsEmpty();
		bAmmoStatusClearPending = bShotPresentationActive;
		if (!bShotPresentationActive)
		{
			bAmmoStatusEmphasisLatched = false;
		}
	}
	if (NewPhase != EShowDownPhase::Betting && bRaiseBulletLoadActive)
	{
		CompleteAmmoStatusRaiseDelta();
		SetBulletPresentationImmediate(StatusLiveRounds);
	}
	TryStartPendingRaiseBulletLoadPresentation();
	// Phase replication can overtake the gun actor channel. Pending work starts
	// only in betting and is discarded once its round has already progressed.
	ApplyAmmoStatusDisplaySettings();
	RefreshRuntimeTickState();
}

void ASDSelfShotGunActor::HandleTableCinematicCue(
	const ESDTableCinematicCue Cue,
	uint8 PlayerSlotMask)
{
	// Reset and result are separate reliable multicasts. Reset is allowed to
	// arrive while this peer is still presenting, so it must not discard the
	// active or queued gun shots.
	(void)PlayerSlotMask;

	if (Cue == ESDTableCinematicCue::PreRevealBlackout
		|| Cue == ESDTableCinematicCue::RevealStarted
		|| Cue == ESDTableCinematicCue::TriggerPullStarted)
	{
		bAmmoStatusEmphasisLatched = true;
		bAmmoStatusClearPending = false;
	}
	else if (Cue == ESDTableCinematicCue::Reset
		|| Cue == ESDTableCinematicCue::InitialDealStarted)
	{
		const bool bShotPresentationActive =
			AnimState != EGunAnimState::Idle
			|| bMultiplayerRoulettePresentationActive
			|| !PendingMultiplayerRoulettePresentations.IsEmpty();
		bAmmoStatusClearPending = bShotPresentationActive;
		if (!bShotPresentationActive)
		{
			bAmmoStatusEmphasisLatched = false;
		}
	}

	ApplyAmmoStatusDisplaySettings();
}

void ASDSelfShotGunActor::ApplyAmmoStatusDisplaySettings()
{
	if (AmmoStatusAnchor)
	{
		AmmoStatusAnchor->SetUsingAbsoluteLocation(true);
		AmmoStatusAnchor->SetUsingAbsoluteRotation(true);
		AmmoStatusAnchor->SetUsingAbsoluteScale(true);
		AmmoStatusAnchor->SetWorldRotation(FRotator::ZeroRotator);
		AmmoStatusAnchor->SetWorldScale3D(FVector::OneVector);
		UpdateAmmoStatusAnchorLocation();
	}
	if (AmmoStatusWidgetComponent)
	{
		const bool bShotPresentationActive =
			AnimState != EGunAnimState::Idle
			|| bMultiplayerRoulettePresentationActive
			|| !PendingMultiplayerRoulettePresentations.IsEmpty();
		const bool bEmphasizedAmmoStatus =
			bAmmoStatusEmphasisLatched
			|| StatusPhase == EShowDownPhase::Reveal
			|| StatusPhase == EShowDownPhase::Roulette
			|| bShotPresentationActive;
		const bool bShouldShowAmmoStatus =
			!bOpeningCardShowcaseStowed
			&& !bOpeningCardDropActive
			&& (StatusPhase == EShowDownPhase::Betting
				|| StatusPhase == EShowDownPhase::Reveal
				|| StatusPhase == EShowDownPhase::Roulette
				|| bEmphasizedAmmoStatus);
		const float MinimumPulseSafeWidth =
			(AmmoStatusSlotDiameter * 6.0f + AmmoStatusSlotSpacing * 5.0f + 28.0f) * 1.28f;
		const float MinimumPulseSafeHeight = (AmmoStatusSlotDiameter + 28.0f) * 1.28f;
		AmmoStatusWidgetComponent->SetDrawSize(FVector2D(
			FMath::Max(MinimumPulseSafeWidth, AmmoStatusDrawSize.X),
			FMath::Max(MinimumPulseSafeHeight, AmmoStatusDrawSize.Y)));
		AmmoStatusWidgetComponent->InitWidget();
		if (UShowDownAmmoStatusWidget* AmmoWidget =
			Cast<UShowDownAmmoStatusWidget>(AmmoStatusWidgetComponent->GetUserWidgetObject()))
		{
			AmmoWidget->SetAmmoStatus(
				DisplayedBulletCount,
				StatusRemainingChambers,
				ResolveAmmoStatusRiskColor(DisplayedBulletCount),
				AmmoStatusBackgroundColor,
				AmmoStatusSlotDiameter,
				AmmoStatusSlotSpacing,
				bEmphasizedAmmoStatus);
		}
		AmmoStatusWidgetComponent->SetVisibility(bShouldShowAmmoStatus, true);
		AmmoStatusWidgetComponent->SetHiddenInGame(!bShouldShowAmmoStatus, true);
	}
}

void ASDSelfShotGunActor::ShowAmmoStatusRaiseDelta(int32 AddedRounds)
{
	ApplyAmmoStatusDisplaySettings();
	if (AmmoStatusWidgetComponent)
	{
		if (UShowDownAmmoStatusWidget* AmmoWidget =
			Cast<UShowDownAmmoStatusWidget>(AmmoStatusWidgetComponent->GetUserWidgetObject()))
		{
			AmmoWidget->ShowRaiseDelta(AddedRounds);
		}
	}
}

void ASDSelfShotGunActor::CompleteAmmoStatusRaiseDelta()
{
	if (AmmoStatusWidgetComponent)
	{
		if (UShowDownAmmoStatusWidget* AmmoWidget =
			Cast<UShowDownAmmoStatusWidget>(AmmoStatusWidgetComponent->GetUserWidgetObject()))
		{
			AmmoWidget->CompleteRaiseDelta();
		}
	}
}

FLinearColor ASDSelfShotGunActor::ResolveAmmoStatusRiskColor(int32 LiveRounds) const
{
	if (AmmoStatusRiskColors.IsEmpty())
	{
		return FLinearColor::White;
	}

	const int32 ColorIndex = FMath::Clamp(LiveRounds - 1, 0, AmmoStatusRiskColors.Num() - 1);
	return AmmoStatusRiskColors[ColorIndex];
}

void ASDSelfShotGunActor::UpdateAmmoStatusAnchorLocation()
{
	if (!AmmoStatusAnchor)
	{
		return;
	}

	// Keep the label centered over the revolver itself. Flying betting bullets
	// are intentionally excluded so the status text does not chase them across
	// the table during a raise-loading presentation.
	FBox RevolverBounds(ForceInit);
	const UStaticMeshComponent* RevolverMeshComponents[] = {
		GunMesh,
		ChamberMesh,
		TriggerMesh,
		HammerMesh,
		HandleMesh,
		RatchetMechanismMesh,
		ExtraMesh
	};
	for (const UStaticMeshComponent* MeshComponent : RevolverMeshComponents)
	{
		if (MeshComponent
			&& MeshComponent->IsRegistered()
			&& MeshComponent->GetStaticMesh())
		{
			RevolverBounds += MeshComponent->Bounds.GetBox();
		}
	}

	if (!RevolverBounds.IsValid)
	{
		AmmoStatusAnchor->SetWorldLocation(GetActorLocation() + AmmoStatusWorldOffset);
		return;
	}

	const FVector BoundsCenter = RevolverBounds.GetCenter();
	const FVector GunTopCenter(BoundsCenter.X, BoundsCenter.Y, RevolverBounds.Max.Z);
	AmmoStatusAnchor->SetWorldLocation(GunTopCenter + AmmoStatusWorldOffset);
}

void ASDSelfShotGunActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASDSelfShotGunActor, StatusLiveRounds);
	DOREPLIFETIME(ASDSelfShotGunActor, StatusRemainingChambers);
	DOREPLIFETIME(ASDSelfShotGunActor, StatusPhase);
	DOREPLIFETIME(ASDSelfShotGunActor, StatusTurnSlot);
	DOREPLIFETIME(ASDSelfShotGunActor, bOpeningCardShowcaseStowed);
}

void ASDSelfShotGunActor::SetOpeningCardShowcaseStowed(bool bStowed)
{
	if (!HasAuthority())
	{
		return;
	}
	if (bOpeningCardShowcaseStowed == bStowed)
	{
		if (!bStowed && bOpeningCardDropActive)
		{
			MulticastFinishOpeningCardDrop();
		}
		return;
	}

	bOpeningCardShowcaseStowed = bStowed;
	OnRep_OpeningCardShowcaseStowed();
	ForceNetUpdate();
}

void ASDSelfShotGunActor::OnRep_OpeningCardShowcaseStowed()
{
	if (bHasCapturedRestActorTransform)
	{
		if (bOpeningCardShowcaseStowed)
		{
			StageOpeningCardDrop();
		}
		else
		{
			StartOpeningCardDrop();
		}
	}
	ApplyAmmoStatusDisplaySettings();
	RefreshRuntimeTickState();
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

	const FTransform RevolverPresentationTransform =
		TargetCharacter->GetRevolverPresentationTransform();
	OutSourceLocation = RevolverPresentationTransform.GetLocation();

	// Character-authored revolver placement is authoritative. Aim targets,
	// camera sockets, and head animation must not alter the final gun transform.
	FVector AimDirection = RevolverPresentationTransform.GetUnitAxis(EAxis::X).GetSafeNormal();
	if (AimDirection.IsNearlyZero())
	{
		AimDirection = TargetCharacter->GetActorForwardVector().GetSafeNormal();
	}
	if (AimDirection.IsNearlyZero())
	{
		AimDirection = FVector::ForwardVector;
	}

	OutAimLocation = OutSourceLocation + AimDirection * 100.0f;
	if (OutRotationOffset)
	{
		FRotator AnchorRotationOffset =
			RevolverPresentationTransform.Rotator()
			- AimDirection.Rotation()
			- TargetShotRotationOffset;
		AnchorRotationOffset.Normalize();
		*OutRotationOffset = AnchorRotationOffset;
	}

	return true;
}

void ASDSelfShotGunActor::StartGunUse()
{
	if (!CanStartPresentation())
	{
		return;
	}

	RestActorTransform = GetActorTransform();
	bHasCapturedRestActorTransform = true;
	RaiseStartTransform = RestActorTransform;
	StateElapsedTime = 0.0f;
	MechanismResetElapsedTime = 0.0f;
	HeldGunJitterElapsedTime = 0.0f;
	AnimState = EGunAnimState::Raising;
	bPresentationFinishPending = true;
	SetActorTickEnabled(true);

	if (bDisableCollisionWhileUsing)
	{
		OriginalCollisionEnabled = GunMesh->GetCollisionEnabled();
		GunMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Forced multiplayer results are known before the first raising tick. Move
	// the victim to the third-person shot on the same frame the gun starts,
	// instead of waiting through the authored (BP-overridden) raise duration.
	TryStartLocalTargetShotCamera();

	OnGunRaised.Broadcast();
	if (UShowDownAudioSubsystem* AudioSubsystem = FindShowDownAudioSubsystem(this))
	{
		AudioSubsystem->NotifyGunRaised();
	}
}

bool ASDSelfShotGunActor::CanInteract_Implementation(AActor* Interactor) const
{
	// Direct clicking was a development-only preview path. Runtime shots are
	// authored by GameMode and use CanStartPresentation instead.
	return false;
}

bool ASDSelfShotGunActor::CanStartPresentation() const
{
	return AnimState == EGunAnimState::Idle
		&& HitSequenceState == EHitSequenceState::Idle
		&& !bSelfShotCinematicCameraActive
		&& !bOpeningCardShowcaseStowed
		&& !bOpeningCardDropActive;
}

void ASDSelfShotGunActor::Interact_Implementation(AActor* Interactor)
{
	// Player interaction is intentionally disabled. Server-authored gameplay
	// invokes the explicit presentation methods directly.
}

void ASDSelfShotGunActor::FireGun()
{
	const bool bLiveShot = ResolveCurrentShotIsLive();
	if (CurrentShotTargetSlot != EShowDownPlayerSlot::None)
	{
		// Seat assignment and the seat camera can finish replicating during the
		// raise animation. Re-evaluate on the actual fire frame before choosing the
		// local victim camera.
		bCurrentShotTargetsLocalPlayer = ShouldTreatSlotAsLocalPlayer(CurrentShotTargetSlot);
	}
	// This is the exact local frame where the trigger reaches full travel. Each
	// peer runs the authored gun presentation locally, so publish a local-only
	// cue instead of adding another replicated timer that could drift from it.
	UWorld* World = GetWorld();
	if (AShowDownGameStateBase* ShowDownGameState =
		World ? World->GetGameState<AShowDownGameStateBase>() : nullptr)
	{
		ShowDownGameState->OnTableCinematicCue.Broadcast(
			ESDTableCinematicCue::TriggerPullCompleted,
			0);
	}
	StateElapsedTime = 0.0f;
	MechanismResetElapsedTime = 0.0f;
	if (ShouldUseGunShotCamera(bCurrentShotTargetsLocalPlayer)
		&& !bSelfShotCinematicCameraActive
		&& !bSelfShotCinematicCameraStartPending)
	{
		StartSelfShotCinematicCamera();
	}
	else if (!bCurrentShotTargetsLocalPlayer)
	{
		CancelSelfShotCinematicCamera();
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
	MuzzleFlashElapsedTime = FMath::Max(0.01f, MuzzleFlashDuration);
	MuzzleFlashLight->SetAttenuationRadius(
		FMath::Max(0.0f, MuzzleFlashAttenuationRadius));
	MuzzleFlashLight->SetIntensityUnits(ELightUnits::Lumens);
	MuzzleFlashLight->SetLightColor(MuzzleFlashColor);
	MuzzleFlashLight->SetCastShadows(true);
	MuzzleFlashLight->SetIndirectLightingIntensity(0.0f);
	MuzzleFlashLight->SetVolumetricScatteringIntensity(0.0f);
	MuzzleFlashLight->SetIntensity(
		FMath::Max(0.0f, MuzzleFlashIntensity));

	PlayConfiguredSound(
		GunshotSound,
		bPlayGunshotSound2D,
		GetActorLocation(),
		GunshotVolumeMultiplier);
	if (UShowDownAudioSubsystem* AudioSubsystem = FindShowDownAudioSubsystem(this))
	{
		AudioSubsystem->NotifyGunFired();
	}

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
	SetActorTransform(GetPresentationGunTransform());
	MuzzleFlashElapsedTime = 0.0f;
	MuzzleFlashLight->SetIntensity(0.0f);

	PlayConfiguredSound(EmptyShotSound, bPlayEmptyShotSound2D, HammerPivot->GetComponentLocation());
	if (UShowDownAudioSubsystem* AudioSubsystem = FindShowDownAudioSubsystem(this))
	{
		AudioSubsystem->NotifyGunEmptyFired();
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
	AnimState = EGunAnimState::Idle;
	OnGunSequenceFinished.Broadcast();
	BroadcastPresentationFinishedIfIdle();
}

void ASDSelfShotGunActor::StartMechanismAnimation()
{
	OnTriggerPullStarted.Broadcast();
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
	CancelSelfShotCinematicCamera();
	CinematicCameraElapsedTime = 0.0f;
	CinematicCameraBlendOutElapsedTime = 0.0f;
	EliminationOverviewElapsedTime = 0.0f;

	if (!bCurrentShotTargetsLocalPlayer
		|| !PrepareLocalGunShotCamera())
	{
		return;
	}

	bSelfShotCinematicCameraStartPending = true;
	ActivateSelfShotCinematicCamera();
}

void ASDSelfShotGunActor::TryStartLocalTargetShotCamera()
{
	if (bSelfShotCinematicCameraActive || bSelfShotCinematicCameraStartPending)
	{
		return;
	}

	if (CurrentShotTargetSlot != EShowDownPlayerSlot::None)
	{
		bCurrentShotTargetsLocalPlayer = ShouldTreatSlotAsLocalPlayer(CurrentShotTargetSlot);
	}
	if (ShouldUseGunShotCamera(bCurrentShotTargetsLocalPlayer))
	{
		StartSelfShotCinematicCamera();
	}
}

void ASDSelfShotGunActor::ActivateSelfShotCinematicCamera()
{
	if (!bSelfShotCinematicCameraStartPending)
	{
		return;
	}

	AShowDownPlayerController* PlayerController = Cast<AShowDownPlayerController>(
		FindLocalPlayerController(this));
	if (PlayerController
		&& ActiveSelfShotCinematicCamera
		&& PlayerController->BeginGunShotCameraOverride(
			ActiveSelfShotCinematicCamera,
			AnimState == EGunAnimState::Raising
				? FMath::Min(FMath::Max(0.0f, CinematicCameraBlendInTime), 0.25f)
				: CinematicCameraBlendInTime,
			CinematicCameraBlendExponent))
	{
		ShowDownCameraAspect::ApplyForced16By9(ActiveSelfShotCinematicCamera);
		bSelfShotCinematicCameraActive = true;
	}

	bSelfShotCinematicCameraStartPending = false;
}

void ASDSelfShotGunActor::UpdateSelfShotCinematicCamera(float DeltaSeconds)
{
	if (!bSelfShotCinematicCameraActive)
	{
		return;
	}
	if (bEliminationTableOverviewActive)
	{
		EliminationOverviewElapsedTime += FMath::Max(0.0f, DeltaSeconds);
		const float SafeMoveTime = FMath::Max(0.0f, EliminationOverviewMoveTime);
		const float Alpha = SafeMoveTime > KINDA_SMALL_NUMBER
			? FMath::Clamp(EliminationOverviewElapsedTime / SafeMoveTime, 0.0f, 1.0f)
			: 1.0f;
		const float EasedAlpha = FMath::InterpEaseInOut(
			0.0f,
			1.0f,
			Alpha,
			FMath::Max(1.0f, CinematicCameraBlendExponent));
		FTransform BlendedTransform;
		BlendedTransform.Blend(
			EliminationOverviewStartTransform,
			EliminationOverviewTargetTransform,
			EasedAlpha);
		if (ActiveSelfShotCinematicCamera)
		{
			ActiveSelfShotCinematicCamera->SetActorTransform(BlendedTransform);
		}

		if (Alpha >= 1.0f)
		{
			FinishEliminationTableOverview();
		}
		return;
	}
	if (bSelfShotCinematicCameraBlendOutActive)
	{
		CinematicCameraBlendOutElapsedTime += FMath::Max(0.0f, DeltaSeconds);
		if (CinematicCameraBlendOutElapsedTime < FMath::Max(0.0f, CinematicCameraBlendOutTime))
		{
			return;
		}

		bSelfShotCinematicCameraActive = false;
		bSelfShotCinematicCameraStartPending = false;
		bSelfShotCinematicCameraHoldStarted = false;
		bSelfShotCinematicCameraBlendOutActive = false;
		ActiveSelfShotCinematicCamera = nullptr;
		BroadcastPresentationFinishedIfIdle();
		return;
	}
	if (!bSelfShotCinematicCameraHoldStarted)
	{
		return;
	}

	CinematicCameraElapsedTime += DeltaSeconds;
	if (CinematicCameraElapsedTime
		< FMath::Max(MinimumCinematicCameraHoldTime, CinematicCameraHoldTime))
	{
		return;
	}

	if (bCinematicCameraShakeActive && ActiveSelfShotCinematicCamera)
	{
		ActiveSelfShotCinematicCamera->SetActorTransform(CinematicCameraShakeBaseTransform);
		bCinematicCameraShakeActive = false;
	}
	if (TryStartEliminationTableOverview())
	{
		return;
	}
	if (AShowDownPlayerController* PlayerController = Cast<AShowDownPlayerController>(
		FindLocalPlayerController(this)))
	{
		PlayerController->EndGunShotCameraOverride(
			ActiveSelfShotCinematicCamera,
			CinematicCameraBlendOutTime,
			CinematicCameraBlendExponent);
	}

	bSelfShotCinematicCameraHoldStarted = false;
	bSelfShotCinematicCameraBlendOutActive = true;
	CinematicCameraBlendOutElapsedTime = 0.0f;
	if (CinematicCameraBlendOutTime <= KINDA_SMALL_NUMBER)
	{
		bSelfShotCinematicCameraActive = false;
		bSelfShotCinematicCameraBlendOutActive = false;
		ActiveSelfShotCinematicCamera = nullptr;
		BroadcastPresentationFinishedIfIdle();
	}
}

bool ASDSelfShotGunActor::TryStartEliminationTableOverview()
{
	AShowDownCharacter* TargetCharacter = ResolveCurrentGunShotCameraTarget();
	if (!bUseEliminationTableOverview
		|| !IsValid(ActiveSelfShotCinematicCamera)
		|| !IsValid(TargetCharacter)
		|| !ShouldUseEliminationTableOverview(
			true,
			bCurrentShotTargetsLocalPlayer,
			TargetCharacter->GetCharacterLives()))
	{
		return false;
	}

	EliminationOverviewStartTransform = ActiveSelfShotCinematicCamera->GetActorTransform();
	EliminationOverviewTargetTransform = BuildEliminationTableOverviewTransform(TargetCharacter);
	EliminationOverviewElapsedTime = 0.0f;
	bSelfShotCinematicCameraHoldStarted = false;
	bSelfShotCinematicCameraBlendOutActive = false;
	bEliminationTableOverviewActive = true;

	if (EliminationOverviewMoveTime <= KINDA_SMALL_NUMBER)
	{
		ActiveSelfShotCinematicCamera->SetActorTransform(EliminationOverviewTargetTransform);
		FinishEliminationTableOverview();
	}
	return true;
}

void ASDSelfShotGunActor::FinishEliminationTableOverview()
{
	ACameraActor* SpectatorCamera = ActiveSelfShotCinematicCamera;
	if (AShowDownPlayerController* PlayerController = Cast<AShowDownPlayerController>(
		FindLocalPlayerController(this)))
	{
		PlayerController->ReleaseGunShotCameraOverrideForElimination(SpectatorCamera);
	}

	bEliminationTableOverviewActive = false;
	EliminationOverviewElapsedTime = 0.0f;
	bSelfShotCinematicCameraActive = false;
	bSelfShotCinematicCameraStartPending = false;
	bSelfShotCinematicCameraHoldStarted = false;
	bSelfShotCinematicCameraBlendOutActive = false;
	ActiveSelfShotCinematicCamera = nullptr;
	BroadcastPresentationFinishedIfIdle();
}

FTransform ASDSelfShotGunActor::BuildEliminationTableOverviewTransform(
	const AShowDownCharacter* TargetCharacter) const
{
	if (!IsValid(TargetCharacter))
	{
		return ActiveSelfShotCinematicCamera
			? ActiveSelfShotCinematicCamera->GetActorTransform()
			: FTransform::Identity;
	}

	return BuildEliminationTableOverviewTransform(
		GetActorLocation(),
		TargetCharacter->GetActorTransform(),
		EliminationOverviewBackDistance,
		EliminationOverviewHeight,
		EliminationOverviewLookAtHeight);
}

void ASDSelfShotGunActor::CancelSelfShotCinematicCamera()
{
	if (bCinematicCameraShakeActive && ActiveSelfShotCinematicCamera)
	{
		ActiveSelfShotCinematicCamera->SetActorTransform(CinematicCameraShakeBaseTransform);
	}
	bCinematicCameraShakeActive = false;

	if (AShowDownPlayerController* PlayerController = Cast<AShowDownPlayerController>(
		FindLocalPlayerController(this)))
	{
		PlayerController->CancelGunShotCameraOverride(ActiveSelfShotCinematicCamera);
	}

	bSelfShotCinematicCameraActive = false;
	bSelfShotCinematicCameraStartPending = false;
	bSelfShotCinematicCameraHoldStarted = false;
	bSelfShotCinematicCameraBlendOutActive = false;
	bEliminationTableOverviewActive = false;
	CinematicCameraBlendOutElapsedTime = 0.0f;
	EliminationOverviewElapsedTime = 0.0f;
	ActiveSelfShotCinematicCamera = nullptr;
}

bool ASDSelfShotGunActor::PrepareLocalGunShotCamera()
{
	ACameraActor* CameraTemplate = ForcedShotCamera.IsValid()
		? ForcedShotCamera.Get()
		: SelfShotCinematicCamera.Get();
	HideCameraVisualization(CameraTemplate);
	AShowDownCharacter* ReferenceCharacter = FindGunShotCameraReferenceCharacter();
	AShowDownCharacter* TargetCharacter = ResolveCurrentGunShotCameraTarget();
	if (!IsValid(TargetCharacter))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Skipping local gun-shot camera because the target is missing. Template=%s ReferenceP1=%s Slot=%d"),
			*GetNameSafe(CameraTemplate),
			*GetNameSafe(ReferenceCharacter),
			static_cast<int32>(CurrentShotTargetSlot));
		return false;
	}

	ACameraActor* LocalCamera = GetOrCreateLocalGunShotCamera();
	if (!IsValid(LocalCamera))
	{
		return false;
	}

	if (IsValid(CameraTemplate))
	{
		if (UCameraComponent* SourceCameraComponent = CameraTemplate->GetCameraComponent())
		{
			if (UCameraComponent* LocalCameraComponent = LocalCamera->GetCameraComponent())
			{
				FMinimalViewInfo ViewInfo;
				SourceCameraComponent->GetCameraView(0.0f, ViewInfo);
				LocalCameraComponent->SetProjectionMode(ViewInfo.ProjectionMode);
				LocalCameraComponent->SetFieldOfView(ViewInfo.FOV);
				LocalCameraComponent->SetOrthoWidth(ViewInfo.OrthoWidth);
				LocalCameraComponent->SetAspectRatio(ViewInfo.AspectRatio);
				LocalCameraComponent->SetConstraintAspectRatio(SourceCameraComponent->bConstrainAspectRatio);
				LocalCameraComponent->PostProcessSettings = ViewInfo.PostProcessSettings;
				LocalCameraComponent->PostProcessBlendWeight = ViewInfo.PostProcessBlendWeight;
			}
		}
	}

	if (IsValid(CameraTemplate) && IsValid(ReferenceCharacter))
	{
		FTransform CameraReferenceTransform = CameraTemplate->GetActorTransform();
		if (CameraTemplate == SelfShotCinematicCamera.Get())
		{
			if (!bHasGunShotCameraReferenceTransform)
			{
				GunShotCameraReferenceTransform = CameraReferenceTransform;
				bHasGunShotCameraReferenceTransform = true;
			}
			CameraReferenceTransform = GunShotCameraReferenceTransform;
		}

		LocalCamera->SetActorTransform(BuildSeatRelativeGunShotCameraTransform(
			CameraReferenceTransform,
			ReferenceCharacter->GetActorTransform(),
			TargetCharacter->GetActorTransform()));
	}
	else
	{
		LocalCamera->SetActorTransform(BuildFallbackGunShotCameraTransform(
			GetActorLocation(),
			TargetCharacter->GetActorTransform(),
			FallbackGunShotCameraBackDistance,
			FallbackGunShotCameraSideDistance,
			FallbackGunShotCameraHeight,
			FallbackGunShotCameraLookAtHeight));
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Using fallback local third-person gun-shot camera. Template=%s ReferenceP1=%s Target=%s Slot=%d"),
			*GetNameSafe(CameraTemplate),
			*GetNameSafe(ReferenceCharacter),
			*GetNameSafe(TargetCharacter),
			static_cast<int32>(CurrentShotTargetSlot));
	}
	ActiveSelfShotCinematicCamera = LocalCamera;
	return true;
}

ACameraActor* ASDSelfShotGunActor::GetOrCreateLocalGunShotCamera()
{
	if (IsValid(LocalGunShotCamera))
	{
		HideCameraVisualization(LocalGunShotCamera);
		return LocalGunShotCamera;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.ObjectFlags |= RF_Transient;
	LocalGunShotCamera = World->SpawnActor<ACameraActor>(
		ACameraActor::StaticClass(),
		GunShotCameraReferenceTransform,
		SpawnParameters);
	if (LocalGunShotCamera)
	{
		LocalGunShotCamera->SetReplicates(false);
		LocalGunShotCamera->SetActorEnableCollision(false);
		HideCameraVisualization(LocalGunShotCamera);
	}
	return LocalGunShotCamera;
}

AShowDownCharacter* ASDSelfShotGunActor::FindGunShotCameraReferenceCharacter() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AShowDownCharacter> It(World); It; ++It)
	{
		AShowDownCharacter* Character = *It;
		if (IsValid(Character) && Character->IsAssignedToSlot(EShowDownPlayerSlot::Player1))
		{
			return Character;
		}
	}
	return nullptr;
}

AShowDownCharacter* ASDSelfShotGunActor::ResolveCurrentGunShotCameraTarget() const
{
	if (AShowDownCharacter* ForcedTargetCharacter = Cast<AShowDownCharacter>(ForcedShotTargetActor.Get()))
	{
		return ForcedTargetCharacter;
	}

	if (CurrentShotTargetSlot != EShowDownPlayerSlot::None)
	{
		return Cast<AShowDownCharacter>(FindMultiplayerShotTarget(CurrentShotTargetSlot));
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AShowDownCharacter> It(World); It; ++It)
	{
		AShowDownCharacter* Character = *It;
		if (IsValid(Character) && Character->IsLocalPlayerCharacter())
		{
			return Character;
		}
	}
	return nullptr;
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
	else if (AShowDownPlayerController* ShowDownController = Cast<AShowDownPlayerController>(FindLocalPlayerController(this)))
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
		EnterHitSequencePreBlackoutHold();
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
			EnterHitSequencePreBlackoutHold();
		}
		break;
	case EHitSequenceState::PreBlackoutHold:
		if (HitSequenceElapsedTime >= HitBlackoutDelay)
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

void ASDSelfShotGunActor::EnterHitSequencePreBlackoutHold()
{
	if (HitBlackoutDelay <= KINDA_SMALL_NUMBER)
	{
		EnterHitSequenceBlackout();
		return;
	}

	HitSequenceState = EHitSequenceState::PreBlackoutHold;
	HitSequenceElapsedTime = 0.0f;
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
	else if (AShowDownPlayerController* ShowDownController = Cast<AShowDownPlayerController>(FindLocalPlayerController(this)))
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
	if (AnimState != EGunAnimState::Idle
		|| HitSequenceState != EHitSequenceState::Idle
		|| bSelfShotCinematicCameraActive
		|| bSelfShotCinematicCameraStartPending)
	{
		return;
	}

	if (bPresentationFinishPending)
	{
		bPresentationFinishPending = false;
		bMultiplayerRoulettePresentationActive = false;
		OnGunPresentationFinished.Broadcast();
		if (UShowDownAudioSubsystem* AudioSubsystem = FindShowDownAudioSubsystem(this))
		{
			AudioSubsystem->NotifyGunPresentationFinished();
		}
	}

	TryStartPendingMultiplayerRoulettePresentation();
	if (bAmmoStatusClearPending
		&& AnimState == EGunAnimState::Idle
		&& !bMultiplayerRoulettePresentationActive
		&& PendingMultiplayerRoulettePresentations.IsEmpty())
	{
		bAmmoStatusClearPending = false;
		bAmmoStatusEmphasisLatched = false;
		ApplyAmmoStatusDisplaySettings();
	}
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

void ASDSelfShotGunActor::CacheBulletRestRelativeTransforms()
{
	if (BulletRestRelativeTransforms.Num() == RevolverBulletSlotCount)
	{
		return;
	}

	BulletRestRelativeTransforms.SetNum(RevolverBulletSlotCount);
	for (int32 BulletIndex = 0; BulletIndex < RevolverBulletSlotCount; ++BulletIndex)
	{
		if (const UStaticMeshComponent* BulletMesh = GetBulletMeshComponent(BulletIndex))
		{
			BulletRestRelativeTransforms[BulletIndex] = BulletMesh->GetRelativeTransform();
		}
		else
		{
			BulletRestRelativeTransforms[BulletIndex] = FTransform::Identity;
		}
	}
}

FTransform ASDSelfShotGunActor::ResolveBettingBulletRestRelativeTransform(int32 BulletIndex) const
{
	if (!BulletRestRelativeTransforms.IsValidIndex(BulletIndex))
	{
		return FTransform::Identity;
	}

	FTransform RestTransform = BulletRestRelativeTransforms[BulletIndex];
	RestTransform.SetLocation(
		RestTransform.GetLocation()
		+ RestTransform.GetRotation().RotateVector(RaiseBulletLoadedLocalOffset));
	return RestTransform;
}

UStaticMeshComponent* ASDSelfShotGunActor::GetBulletMeshComponent(int32 BulletIndex) const
{
	switch (BulletIndex)
	{
	case 0:
		return BulletMesh01;
	case 1:
		return BulletMesh02;
	case 2:
		return BulletMesh03;
	case 3:
		return BulletMesh04;
	case 4:
		return BulletMesh05;
	case 5:
		return BulletMesh06;
	default:
		return nullptr;
	}
}

UStaticMeshComponent* ASDSelfShotGunActor::GetBettingBulletMeshComponent(int32 BulletIndex) const
{
	return BettingBulletMeshes.IsValidIndex(BulletIndex)
		? BettingBulletMeshes[BulletIndex]
		: nullptr;
}

void ASDSelfShotGunActor::SynchronizeBulletPresentationFromStatus()
{
	CacheBulletRestRelativeTransforms();
	const int32 TargetBulletCount = FMath::Clamp(StatusLiveRounds, 0, RevolverBulletSlotCount);
	if (bRaiseBulletLoadActive && StatusPhase == EShowDownPhase::Betting)
	{
		// A raise RPC is reliable while status replication is state based. During
		// betting, an older coalesced count must not cancel a newer cascade.
		return;
	}

	SetBulletPresentationImmediate(TargetBulletCount);
	TryStartPendingRaiseBulletLoadPresentation();
	RefreshRuntimeTickState();
}

void ASDSelfShotGunActor::SetBulletPresentationImmediate(int32 BulletCount)
{
	CacheBulletRestRelativeTransforms();
	const int32 ClampedBulletCount = FMath::Clamp(BulletCount, 0, RevolverBulletSlotCount);
	for (int32 BulletIndex = 0; BulletIndex < RevolverBulletSlotCount; ++BulletIndex)
	{
		// The old slot meshes are retained only as authored transform markers.
		// All visible rounds use the dedicated bulletBetting asset.
		if (UStaticMeshComponent* LegacyBulletMesh = GetBulletMeshComponent(BulletIndex))
		{
			LegacyBulletMesh->SetVisibility(false, true);
			LegacyBulletMesh->SetHiddenInGame(true, true);
		}

		UStaticMeshComponent* BettingBulletMesh = GetBettingBulletMeshComponent(BulletIndex);
		if (!BettingBulletMesh || !BulletRestRelativeTransforms.IsValidIndex(BulletIndex))
		{
			continue;
		}

		const FTransform SlotTransform = ResolveBettingBulletRestRelativeTransform(BulletIndex);
		BettingBulletMesh->SetRelativeLocationAndRotation(
			SlotTransform.GetLocation(),
			SlotTransform.GetRotation());
		BettingBulletMesh->SetRelativeScale3D(FVector(FMath::Max(0.001f, RaiseBulletBettingScale)));
		const bool bVisible = BulletIndex < ClampedBulletCount;
		BettingBulletMesh->SetVisibility(bVisible, true);
		BettingBulletMesh->SetHiddenInGame(!bVisible, true);
	}

	DisplayedBulletCount = ClampedBulletCount;
	RaiseBulletLoadPreviousCount = ClampedBulletCount;
	RaiseBulletLoadStartCount = ClampedBulletCount;
	RaiseBulletLoadTargetCount = ClampedBulletCount;
	RaiseBulletLoadElapsedTime = 0.0f;
	bRaiseBulletLoadActive = false;
	ActiveRaiseBulletSourceSlot = EShowDownPlayerSlot::None;
}

FVector ASDSelfShotGunActor::ResolveRaiseBulletSourceWorldLocation(
	EShowDownPlayerSlot SourceSlot,
	int32 StartCount) const
{
	const AShowDownCharacter* SourceCharacter = Cast<AShowDownCharacter>(
		FindMultiplayerShotTarget(SourceSlot));
	if (!SourceCharacter && (SourceSlot == EShowDownPlayerSlot::Player1
		|| SourceSlot == EShowDownPlayerSlot::Player2))
	{
		// Single player assigns the AI opponent to the Opponent role with no network
		// slot. Preserve Player1/Player2 as the wire format and fall back to roles.
		const EShowDownCharacterRole ExpectedRole = SourceSlot == EShowDownPlayerSlot::Player1
			? EShowDownCharacterRole::Player
			: EShowDownCharacterRole::Opponent;
		if (UWorld* World = GetWorld())
		{
			for (TActorIterator<AShowDownCharacter> It(World); It; ++It)
			{
				const AShowDownCharacter* Candidate = *It;
				if (IsValid(Candidate)
					&& Candidate->IsCharacterSceneActive()
					&& Candidate->GetCharacterRole() == ExpectedRole)
				{
					SourceCharacter = Candidate;
					break;
				}
			}
		}
	}

	if (SourceCharacter)
	{
		if (const USkeletalMeshComponent* CharacterMesh = SourceCharacter->GetMesh())
		{
			static const FName HandCandidates[] = {
				TEXT("RightHand"),
				TEXT("hand_r"),
				TEXT("mixamorig:RightHand"),
				TEXT("R_Hand")
			};

			FName ResolvedHandName = RaiseBulletSourceHandName;
			if (ResolvedHandName == NAME_None
				|| (CharacterMesh->GetBoneIndex(ResolvedHandName) == INDEX_NONE
					&& !CharacterMesh->DoesSocketExist(ResolvedHandName)))
			{
				ResolvedHandName = NAME_None;
				for (const FName Candidate : HandCandidates)
				{
					if (CharacterMesh->GetBoneIndex(Candidate) != INDEX_NONE
						|| CharacterMesh->DoesSocketExist(Candidate))
					{
						ResolvedHandName = Candidate;
						break;
					}
				}
			}

			if (ResolvedHandName != NAME_None)
			{
				return CharacterMesh
					->GetSocketTransform(ResolvedHandName, RTS_World)
					.TransformPosition(RaiseBulletSourceHandOffset);
			}
		}

		return SourceCharacter->GetActorTransform().TransformPosition(
			RaiseBulletFallbackCharacterOffset);
	}

	if (ChamberPivot && BulletRestRelativeTransforms.IsValidIndex(StartCount))
	{
		const FTransform RestTransform = ResolveBettingBulletRestRelativeTransform(StartCount);
		return ChamberPivot->GetComponentTransform().TransformPosition(
			RestTransform.GetLocation()
				+ RaiseBulletLoadStartOffset);
	}

	return GetActorLocation();
}

void ASDSelfShotGunActor::ReceiveRaiseBulletLoadPresentation(
	int32 PreviousBet,
	int32 NewBet,
	int32 PresentationRound,
	EShowDownPlayerSlot SourceSlot)
{
	const int32 ClampedPreviousBet = FMath::Clamp(PreviousBet, 0, RevolverBulletSlotCount);
	const int32 ClampedNewBet = FMath::Clamp(NewBet, 0, RevolverBulletSlotCount);
	if (ClampedNewBet <= ClampedPreviousBet)
	{
		return;
	}

	const AShowDownGameStateBase* ShowDownGameState = BoundShowDownGameState.Get();
	if (!ShowDownGameState)
	{
		const UWorld* World = GetWorld();
		ShowDownGameState = World ? World->GetGameState<AShowDownGameStateBase>() : nullptr;
	}

	const int32 LocalRound = ShowDownGameState
		? FMath::Max(0, ShowDownGameState->CurrentRound)
		: 0;
	const EShowDownPhase LocalPhase = ShowDownGameState
		? ShowDownGameState->CurrentPhase
		: StatusPhase;
	const int32 SafePresentationRound = FMath::Max(0, PresentationRound);
	if (SafePresentationRound > 0 && LocalRound > SafePresentationRound)
	{
		ClearPendingRaiseBulletLoadPresentation();
		SetBulletPresentationImmediate(StatusLiveRounds);
		RefreshRuntimeTickState();
		return;
	}

	if (SafePresentationRound > 0 && LocalRound > 0 && LocalRound < SafePresentationRound)
	{
		PendingRaiseBulletLoadPreviousCount = ClampedPreviousBet;
		PendingRaiseBulletLoadTargetCount = ClampedNewBet;
		PendingRaiseBulletLoadRound = SafePresentationRound;
		PendingRaiseBulletLoadSourceSlot = SourceSlot;
		bRaiseBulletLoadPending = true;
		RefreshRuntimeTickState();
		return;
	}

	if (IsRaiseBulletLoadTerminalPhase(LocalPhase))
	{
		// GameState and the gun replicate on different actor channels. A phase may
		// legitimately overtake this reliable RPC; never replay a stale load during
		// reveal or roulette.
		ClearPendingRaiseBulletLoadPresentation();
		SetBulletPresentationImmediate(StatusLiveRounds);
		RefreshRuntimeTickState();
		return;
	}

	if (LocalPhase != EShowDownPhase::Betting)
	{
		PendingRaiseBulletLoadPreviousCount = ClampedPreviousBet;
		PendingRaiseBulletLoadTargetCount = ClampedNewBet;
		PendingRaiseBulletLoadRound = SafePresentationRound;
		PendingRaiseBulletLoadSourceSlot = SourceSlot;
		bRaiseBulletLoadPending = true;
		RefreshRuntimeTickState();
		return;
	}

	ClearPendingRaiseBulletLoadPresentation();
	StartRaiseBulletLoadAnimation(ClampedPreviousBet, ClampedNewBet, SourceSlot);
}

void ASDSelfShotGunActor::TryStartPendingRaiseBulletLoadPresentation()
{
	if (!bRaiseBulletLoadPending)
	{
		return;
	}

	const AShowDownGameStateBase* ShowDownGameState = BoundShowDownGameState.Get();
	if (!ShowDownGameState)
	{
		const UWorld* World = GetWorld();
		ShowDownGameState = World ? World->GetGameState<AShowDownGameStateBase>() : nullptr;
	}

	const int32 LocalRound = ShowDownGameState
		? FMath::Max(0, ShowDownGameState->CurrentRound)
		: 0;
	const EShowDownPhase LocalPhase = ShowDownGameState
		? ShowDownGameState->CurrentPhase
		: StatusPhase;
	if (PendingRaiseBulletLoadRound > 0 && LocalRound > PendingRaiseBulletLoadRound)
	{
		ClearPendingRaiseBulletLoadPresentation();
		SetBulletPresentationImmediate(StatusLiveRounds);
		return;
	}
	if (PendingRaiseBulletLoadRound > 0
		&& LocalRound > 0
		&& LocalRound < PendingRaiseBulletLoadRound)
	{
		return;
	}
	if (IsRaiseBulletLoadTerminalPhase(LocalPhase))
	{
		ClearPendingRaiseBulletLoadPresentation();
		SetBulletPresentationImmediate(StatusLiveRounds);
		return;
	}
	if (LocalPhase != EShowDownPhase::Betting)
	{
		return;
	}

	const int32 PreviousBet = PendingRaiseBulletLoadPreviousCount;
	const int32 NewBet = PendingRaiseBulletLoadTargetCount;
	const EShowDownPlayerSlot SourceSlot = PendingRaiseBulletLoadSourceSlot;
	ClearPendingRaiseBulletLoadPresentation();
	StartRaiseBulletLoadAnimation(PreviousBet, NewBet, SourceSlot);
}

void ASDSelfShotGunActor::ClearPendingRaiseBulletLoadPresentation()
{
	bRaiseBulletLoadPending = false;
	PendingRaiseBulletLoadPreviousCount = 0;
	PendingRaiseBulletLoadTargetCount = 0;
	PendingRaiseBulletLoadRound = 0;
	PendingRaiseBulletLoadSourceSlot = EShowDownPlayerSlot::None;
}

void ASDSelfShotGunActor::StartRaiseBulletLoadAnimation(
	int32 PreviousBet,
	int32 NewBet,
	EShowDownPlayerSlot SourceSlot)
{
	const int32 ClampedPreviousBet = FMath::Clamp(PreviousBet, 0, RevolverBulletSlotCount);
	const int32 ClampedNewBet = FMath::Clamp(NewBet, 0, RevolverBulletSlotCount);
	if (ClampedNewBet <= ClampedPreviousBet)
	{
		return;
	}
	ShowAmmoStatusRaiseDelta(ClampedNewBet - ClampedPreviousBet);

	if (!bEnableRaiseBulletLoadAnimation)
	{
		SetBulletPresentationImmediate(ClampedNewBet);
		ApplyAmmoStatusDisplaySettings();
		CompleteAmmoStatusRaiseDelta();
		RefreshRuntimeTickState();
		return;
	}

	const int32 StartCount = ResolveRaiseBulletLoadStartCount(
		ClampedPreviousBet,
		ClampedNewBet,
		bReloadAllBulletsOnRaise);
	SetBulletPresentationImmediate(StartCount);
	RaiseBulletLoadPreviousCount = ClampedPreviousBet;
	RaiseBulletLoadStartCount = StartCount;
	RaiseBulletLoadTargetCount = ClampedNewBet;
	RaiseBulletLoadElapsedTime = 0.0f;
	ActiveRaiseBulletSourceSlot = SourceSlot;
	ActiveRaiseBulletSourceWorldLocation = ResolveRaiseBulletSourceWorldLocation(SourceSlot, StartCount);
	bRaiseBulletLoadActive = true;
	UpdateRaiseBulletLoadAnimation(0.0f);
	ApplyAmmoStatusDisplaySettings();
	RefreshRuntimeTickState();
}

void ASDSelfShotGunActor::UpdateRaiseBulletLoadAnimation(float DeltaSeconds)
{
	if (!bRaiseBulletLoadActive
		|| !ChamberPivot
		|| BulletRestRelativeTransforms.Num() != RevolverBulletSlotCount)
	{
		return;
	}

	RaiseBulletLoadElapsedTime += FMath::Max(0.0f, DeltaSeconds);
	const float BulletDuration = FMath::Max(0.05f, RaiseBulletLoadDuration);
	const float StaggerDelay = FMath::Max(0.0f, RaiseBulletLoadStaggerDelay);
	const float StartHoldTime = FMath::Clamp(
		RaiseBulletLoadStartHoldTime,
		0.0f,
		FMath::Max(0.0f, BulletDuration - 0.05f));
	const float TravelDuration = FMath::Max(0.05f, BulletDuration - StartHoldTime);
	const int32 StartCount = FMath::Clamp(
		RaiseBulletLoadStartCount,
		0,
		RaiseBulletLoadTargetCount);
	const int32 TargetCount = FMath::Clamp(
		RaiseBulletLoadTargetCount,
		StartCount,
		RevolverBulletSlotCount);

	for (int32 BulletIndex = 0; BulletIndex < RevolverBulletSlotCount; ++BulletIndex)
	{
		UStaticMeshComponent* BettingBulletMesh = GetBettingBulletMeshComponent(BulletIndex);
		if (!BettingBulletMesh)
		{
			continue;
		}

		const FTransform RestTransform = ResolveBettingBulletRestRelativeTransform(BulletIndex);
		if (BulletIndex < StartCount)
		{
			BettingBulletMesh->SetRelativeLocationAndRotation(
				RestTransform.GetLocation(),
				RestTransform.GetRotation());
			BettingBulletMesh->SetRelativeScale3D(FVector(FMath::Max(0.001f, RaiseBulletBettingScale)));
			BettingBulletMesh->SetVisibility(true, true);
			BettingBulletMesh->SetHiddenInGame(false, true);
			continue;
		}
		if (BulletIndex >= TargetCount)
		{
			BettingBulletMesh->SetRelativeLocationAndRotation(
				RestTransform.GetLocation(),
				RestTransform.GetRotation());
			BettingBulletMesh->SetRelativeScale3D(FVector(FMath::Max(0.001f, RaiseBulletBettingScale)));
			BettingBulletMesh->SetVisibility(false, true);
			BettingBulletMesh->SetHiddenInGame(true, true);
			continue;
		}

		const int32 SequenceIndex = BulletIndex - StartCount;
		const float LocalElapsedTime = RaiseBulletLoadElapsedTime
			- static_cast<float>(SequenceIndex) * StaggerDelay;
		if (LocalElapsedTime < 0.0f)
		{
			BettingBulletMesh->SetVisibility(false, true);
			BettingBulletMesh->SetHiddenInGame(true, true);
			continue;
		}

		BettingBulletMesh->SetVisibility(true, true);
		BettingBulletMesh->SetHiddenInGame(false, true);
		const FTransform ChamberWorldTransform = ChamberPivot->GetComponentTransform();
		const FVector TargetWorldLocation = ChamberWorldTransform.TransformPosition(
			RestTransform.GetLocation());
		const FQuat TargetWorldRotation = ChamberWorldTransform.TransformRotation(
			RestTransform.GetRotation());
		if (LocalElapsedTime >= BulletDuration)
		{
			BettingBulletMesh->SetRelativeLocationAndRotation(
				RestTransform.GetLocation(),
				RestTransform.GetRotation());
			BettingBulletMesh->SetRelativeScale3D(FVector(FMath::Max(0.001f, RaiseBulletBettingScale)));
			continue;
		}

		const float TravelElapsedTime = FMath::Max(0.0f, LocalElapsedTime - StartHoldTime);
		const float Alpha = FMath::Clamp(TravelElapsedTime / TravelDuration, 0.0f, 1.0f);
		FVector EntryAxis = TargetWorldRotation.GetAxisZ().GetSafeNormal();
		const FVector TargetToSource = (
			ActiveRaiseBulletSourceWorldLocation - TargetWorldLocation).GetSafeNormal();
		if (EntryAxis.IsNearlyZero())
		{
			EntryAxis = TargetToSource.IsNearlyZero() ? FVector::UpVector : TargetToSource;
		}
		else if (!TargetToSource.IsNearlyZero() && FVector::DotProduct(EntryAxis, TargetToSource) < 0.0f)
		{
			EntryAxis *= -1.0f;
		}

		const FVector EntryWorldLocation = TargetWorldLocation
			+ EntryAxis * FMath::Max(0.0f, RaiseBulletLoadEntryDistance);
		const float InsertionFraction = FMath::Clamp(
			RaiseBulletLoadInsertionFraction,
			0.1f,
			0.75f);
		const float ApproachFraction = 1.0f - InsertionFraction;
		FVector CurrentWorldLocation = TargetWorldLocation;
		FQuat CurrentWorldRotation = TargetWorldRotation;
		if (Alpha < ApproachFraction)
		{
			const float ApproachAlpha = FMath::Clamp(Alpha / ApproachFraction, 0.0f, 1.0f);
			const float EasedApproachAlpha = FMath::InterpEaseInOut(
				0.0f,
				1.0f,
				ApproachAlpha,
				2.0f);
			CurrentWorldLocation = FMath::Lerp(
				ActiveRaiseBulletSourceWorldLocation,
				EntryWorldLocation,
				EasedApproachAlpha);
			CurrentWorldLocation += FVector::UpVector
				* FMath::Sin(EasedApproachAlpha * PI)
				* FMath::Max(0.0f, RaiseBulletLoadArcHeight);

			const FVector ApproachDirection = (
				EntryWorldLocation - ActiveRaiseBulletSourceWorldLocation).GetSafeNormal();
			const FQuat ApproachWorldRotation = ApproachDirection.IsNearlyZero()
				? TargetWorldRotation
				: FRotationMatrix::MakeFromZ(ApproachDirection).ToQuat();
			CurrentWorldRotation = FQuat::Slerp(
				ApproachWorldRotation,
				TargetWorldRotation,
				EasedApproachAlpha).GetNormalized();
		}
		else
		{
			const float InsertionAlpha = FMath::Clamp(
				(Alpha - ApproachFraction) / InsertionFraction,
				0.0f,
				1.0f);
			const float EasedInsertionAlpha = FMath::InterpEaseInOut(
				0.0f,
				1.0f,
				InsertionAlpha,
				2.0f);
			CurrentWorldLocation = FMath::Lerp(
				EntryWorldLocation,
				TargetWorldLocation,
				EasedInsertionAlpha);
		}
		BettingBulletMesh->SetWorldLocationAndRotation(
			CurrentWorldLocation,
			CurrentWorldRotation);
		BettingBulletMesh->SetWorldScale3D(FVector(FMath::Max(0.001f, RaiseBulletBettingScale)));
	}

	const int32 AnimatedBulletCount = TargetCount - StartCount;
	int32 SettledBulletCount = StartCount;
	for (int32 SequenceIndex = 0; SequenceIndex < AnimatedBulletCount; ++SequenceIndex)
	{
		const float BulletSettleTime = BulletDuration
			+ static_cast<float>(SequenceIndex) * StaggerDelay;
		if (RaiseBulletLoadElapsedTime + KINDA_SMALL_NUMBER >= BulletSettleTime)
		{
			SettledBulletCount = StartCount + SequenceIndex + 1;
		}
	}
	if (DisplayedBulletCount != SettledBulletCount)
	{
		DisplayedBulletCount = SettledBulletCount;
		ApplyAmmoStatusDisplaySettings();
	}

	const float SequenceDuration = CalculateRaiseBulletLoadSequenceDuration(
		AnimatedBulletCount,
		BulletDuration,
		StaggerDelay);
	if (RaiseBulletLoadElapsedTime + KINDA_SMALL_NUMBER >= SequenceDuration)
	{
		SetBulletPresentationImmediate(TargetCount);
		ApplyAmmoStatusDisplaySettings();
		CompleteAmmoStatusRaiseDelta();
		RefreshRuntimeTickState();
	}
}

bool ASDSelfShotGunActor::IsRuntimeTickRequired() const
{
	const bool bPresentationActive = AnimState != EGunAnimState::Idle
		|| HitSequenceState != EHitSequenceState::Idle
		|| MuzzleFlashElapsedTime > 0.0f
		|| bSelfShotCinematicCameraActive
		|| bSelfShotCinematicCameraStartPending
		|| bCinematicCameraShakeActive
		|| TinnitusAudioComponent != nullptr
		|| bOpeningCardDropActive
		|| bRaiseBulletLoadActive
		|| bRaiseBulletLoadPending;

#if WITH_EDITOR
	return bPresentationActive || bEnableRevolverPlacementDevMode || bRevolverPlacementDevPreviewActive;
#else
	return bPresentationActive;
#endif
}

void ASDSelfShotGunActor::RefreshRuntimeTickState()
{
	SetActorTickEnabled(IsRuntimeTickRequired());
}

void ASDSelfShotGunActor::StageOpeningCardDrop()
{
	if (!bHasCapturedRestActorTransform)
	{
		return;
	}

	bOpeningCardDropActive = false;
	OpeningCardDropVelocityZ = 0.0f;
	FTransform StagedTransform = RestActorTransform;
	StagedTransform.AddToTranslation(FVector::UpVector * FMath::Max(0.0f, OpeningCardDropHeight));
	SetActorTransform(StagedTransform);
	SetActorHiddenInGame(true);
	if (GunMesh)
	{
		GunMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (InteractionBounds)
	{
		InteractionBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ASDSelfShotGunActor::StartOpeningCardDrop()
{
	if (!bHasCapturedRestActorTransform)
	{
		return;
	}

	FTransform DropTransform = RestActorTransform;
	DropTransform.AddToTranslation(FVector::UpVector * FMath::Max(0.0f, OpeningCardDropHeight));
	SetActorTransform(DropTransform);
	SetActorHiddenInGame(false);
	OpeningCardDropVelocityZ = 0.0f;
	bOpeningCardDropActive = OpeningCardDropHeight > KINDA_SMALL_NUMBER;
	if (GunMesh)
	{
		GunMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (InteractionBounds)
	{
		InteractionBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (!bOpeningCardDropActive)
	{
		FinishOpeningCardDrop();
	}
}

void ASDSelfShotGunActor::UpdateOpeningCardDrop(float DeltaSeconds)
{
	if (!bOpeningCardDropActive || !bHasCapturedRestActorTransform)
	{
		return;
	}

	const float RestZ = RestActorTransform.GetLocation().Z;
	const float Gravity = FMath::Max(1.0f, OpeningCardDropGravity);
	const float Restitution = FMath::Clamp(OpeningCardDropRestitution, 0.0f, 0.8f);
	const float StopSpeed = FMath::Max(1.0f, OpeningCardDropStopSpeed);
	float RemainingTime = FMath::Clamp(DeltaSeconds, 0.0f, 0.10f);
	while (RemainingTime > KINDA_SMALL_NUMBER && bOpeningCardDropActive)
	{
		const float Step = FMath::Min(RemainingTime, 1.0f / 60.0f);
		RemainingTime -= Step;
		OpeningCardDropVelocityZ -= Gravity * Step;

		FVector Location = GetActorLocation();
		Location.Z += OpeningCardDropVelocityZ * Step;
		if (Location.Z <= RestZ)
		{
			Location.Z = RestZ;
			const float ReboundSpeed = FMath::Abs(OpeningCardDropVelocityZ) * Restitution;
			if (ReboundSpeed < StopSpeed)
			{
				SetActorLocation(Location);
				FinishOpeningCardDrop();
				break;
			}
			OpeningCardDropVelocityZ = ReboundSpeed;
		}
		SetActorLocation(Location);
	}
}

void ASDSelfShotGunActor::FinishOpeningCardDrop()
{
	bOpeningCardDropActive = false;
	OpeningCardDropVelocityZ = 0.0f;
	SetActorTransform(RestActorTransform);
	SetActorHiddenInGame(false);
	if (GunMesh)
	{
		GunMesh->SetCollisionEnabled(OriginalCollisionEnabled);
	}
	if (InteractionBounds)
	{
		InteractionBounds->SetCollisionEnabled(OriginalInteractionCollisionEnabled);
	}
	ApplyAmmoStatusDisplaySettings();
	TryStartPendingMultiplayerRoulettePresentation();
	RefreshRuntimeTickState();
}

void ASDSelfShotGunActor::MulticastFinishOpeningCardDrop_Implementation()
{
	if (bHasCapturedRestActorTransform)
	{
		FinishOpeningCardDrop();
	}
}

void ASDSelfShotGunActor::SetBlackoutInstant(float Alpha, bool bHoldWhenFinished)
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	if (ClampedAlpha <= 0.0f && !bHitSequenceBlackoutActive)
	{
		return;
	}

	if (APlayerController* PlayerController = FindLocalPlayerController(this))
	{
		if (AShowDownPlayerController* ShowDownController = Cast<AShowDownPlayerController>(PlayerController))
		{
			ShowDownController->SetHitBlackoutUiOpacity(ClampedAlpha);
		}

		if (PlayerController->PlayerCameraManager)
		{
			APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager;
			CameraManager->StopCameraFade();
			CameraManager->SetManualCameraFade(ClampedAlpha, FLinearColor::Black, false);

			if (!bHoldWhenFinished && ClampedAlpha <= KINDA_SMALL_NUMBER)
			{
				// Manual fade applies this frame. A zero-length automatic fade then
				// releases the camera manager on its next update without a black frame.
				CameraManager->StartCameraFade(
					0.0f,
					0.0f,
					0.0f,
					FLinearColor::Black,
					false,
					false);
			}

		}
	}

	bHitSequenceBlackoutActive = ClampedAlpha > 0.0f;
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

void ASDSelfShotGunActor::PlayConfiguredSound(
	USoundBase* Sound,
	bool bPlay2D,
	const FVector& Location,
	float VolumeMultiplier) const
{
	if (!Sound)
	{
		return;
	}

	if (bPlay2D)
	{
		UGameplayStatics::PlaySound2D(this, Sound, FMath::Max(0.0f, VolumeMultiplier));
	}
	else
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			Sound,
			Location,
			FMath::Max(0.0f, VolumeMultiplier));
	}
}

void ASDSelfShotGunActor::HandleMultiplayerRoulettePresentation(
	EShowDownPlayerSlot TargetSlot,
	const FString& TargetName,
	int32 BulletCount,
	bool bHit,
	int32 MatchSequence,
	int32 RoundSequence)
{
	PlayMultiplayerRoulettePresentation(
		TargetSlot,
		bHit,
		MatchSequence,
		RoundSequence);
}

void ASDSelfShotGunActor::HandleMultiplayerPresentationContextChanged(
	int32 MatchSequence,
	int32 RoundSequence)
{
	PendingMultiplayerRoulettePresentations.RemoveAll(
		[MatchSequence, RoundSequence](const FPendingMultiplayerRoulettePresentation& PendingPresentation)
		{
			return PendingPresentation.MatchSequence < MatchSequence
				|| (PendingPresentation.MatchSequence == MatchSequence
					&& PendingPresentation.RoundSequence < RoundSequence);
		});
	TryStartPendingMultiplayerRoulettePresentation();
	ApplyAmmoStatusDisplaySettings();
}

void ASDSelfShotGunActor::HandleGameStateSet(AGameStateBase* GameState)
{
	AShowDownGameStateBase* ShowDownGameState = Cast<AShowDownGameStateBase>(GameState);
	if (BoundShowDownGameState.Get() == ShowDownGameState)
	{
		return;
	}

	if (AShowDownGameStateBase* PreviousGameState = BoundShowDownGameState.Get())
	{
		PreviousGameState->OnPhaseChanged.RemoveDynamic(
			this,
			&ASDSelfShotGunActor::HandleGamePhaseChanged);
		PreviousGameState->OnTableCinematicCue.RemoveDynamic(
			this,
			&ASDSelfShotGunActor::HandleTableCinematicCue);
		PreviousGameState->OnMultiplayerRoulettePresentationContext.RemoveAll(this);
		PreviousGameState->OnMultiplayerPresentationContextChanged.RemoveAll(this);
	}

	BoundShowDownGameState = ShowDownGameState;
	if (ShowDownGameState)
	{
		ShowDownGameState->OnPhaseChanged.AddUniqueDynamic(
			this,
			&ASDSelfShotGunActor::HandleGamePhaseChanged);
		ShowDownGameState->OnTableCinematicCue.AddUniqueDynamic(
			this,
			&ASDSelfShotGunActor::HandleTableCinematicCue);
		ShowDownGameState->OnMultiplayerRoulettePresentationContext.AddUObject(
			this,
			&ASDSelfShotGunActor::HandleMultiplayerRoulettePresentation);
		ShowDownGameState->OnMultiplayerPresentationContextChanged.AddUObject(
			this,
			&ASDSelfShotGunActor::HandleMultiplayerPresentationContextChanged);
		HandleMultiplayerPresentationContextChanged(
			ShowDownGameState->MultiplayerMatchSequence,
			ShowDownGameState->MultiplayerRoundSequence);
		HandleGamePhaseChanged(ShowDownGameState->CurrentPhase);
	}
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

void ASDSelfShotGunActor::PlayMultiplayerRoulettePresentation(
	EShowDownPlayerSlot TargetSlot,
	bool bHit,
	int32 MatchSequence,
	int32 RoundSequence)
{
	const EMultiplayerPresentationContextRelation ContextRelation =
		CompareMultiplayerPresentationContext(MatchSequence, RoundSequence);
	if (ContextRelation == EMultiplayerPresentationContextRelation::Past)
	{
		UE_LOG(
			LogTemp,
			Verbose,
			TEXT("Discarded stale multiplayer roulette presentation. Match=%d Round=%d"),
			MatchSequence,
			RoundSequence);
		return;
	}

	if (ContextRelation == EMultiplayerPresentationContextRelation::Future
		|| !CanStartPresentation())
	{
		if (GetNetMode() != NM_DedicatedServer)
		{
			PendingMultiplayerRoulettePresentations.Add(
				{ TargetSlot, bHit, MatchSequence, RoundSequence });
			UE_LOG(
				LogTemp,
				Verbose,
				TEXT("Queued multiplayer roulette presentation. Slot=%d Hit=%s Match=%d Round=%d Pending=%d"),
				static_cast<int32>(TargetSlot),
				bHit ? TEXT("true") : TEXT("false"),
				MatchSequence,
				RoundSequence,
				PendingMultiplayerRoulettePresentations.Num());
		}
		return;
	}

	AActor* TargetActor = FindMultiplayerShotTarget(TargetSlot);
	ShotResultMode = bHit
		? ESDSelfShotRoundMode::AlwaysLive
		: ESDSelfShotRoundMode::AlwaysEmpty;
	ForcedShotTargetActor = TargetActor;
	ForcedShotCamera = nullptr;
	bHasForcedShotSourceLocation = false;
	bHasForcedShotAimLocation = false;
	bHasForcedShotRotationOffset = false;
	bCurrentShotTargetsLocalPlayer = ShouldTreatSlotAsLocalPlayer(TargetSlot);
	CurrentShotTargetSlot = TargetSlot;
	bMultiplayerRoulettePresentationActive = true;
	UE_LOG(
		LogTemp,
		Log,
		TEXT("Playing multiplayer roulette presentation. Slot=%d Hit=%s LocalVictim=%s Target=%s"),
		static_cast<int32>(TargetSlot),
		bHit ? TEXT("true") : TEXT("false"),
		bCurrentShotTargetsLocalPlayer ? TEXT("true") : TEXT("false"),
		*GetNameSafe(TargetActor));

	FVector SourceLocation = FVector::ZeroVector;
	FVector AimLocation = FVector::ZeroVector;
	FRotator RotationOffset = FRotator::ZeroRotator;
	if (IsValid(TargetActor)
		&& TryResolveCharacterPresentationShot(
			Cast<AShowDownCharacter>(TargetActor),
			SourceLocation,
			AimLocation,
			&RotationOffset))
	{
		ForcedShotSourceLocation = SourceLocation;
		ForcedShotAimLocation = AimLocation;
		ForcedShotRotationOffset = RotationOffset;
		bHasForcedShotSourceLocation = true;
		bHasForcedShotAimLocation = true;
		bHasForcedShotRotationOffset = true;
	}

	StartGunUse();
}

void ASDSelfShotGunActor::TryStartPendingMultiplayerRoulettePresentation()
{
	if (GetNetMode() == NM_DedicatedServer
		|| PendingMultiplayerRoulettePresentations.IsEmpty())
	{
		return;
	}

	while (!PendingMultiplayerRoulettePresentations.IsEmpty())
	{
		const FPendingMultiplayerRoulettePresentation& PendingPresentation =
			PendingMultiplayerRoulettePresentations[0];
		const EMultiplayerPresentationContextRelation ContextRelation =
			CompareMultiplayerPresentationContext(
				PendingPresentation.MatchSequence,
				PendingPresentation.RoundSequence);
		if (ContextRelation == EMultiplayerPresentationContextRelation::Past)
		{
			PendingMultiplayerRoulettePresentations.RemoveAt(0);
			continue;
		}
		if (ContextRelation == EMultiplayerPresentationContextRelation::Future
			|| !CanStartPresentation())
		{
			return;
		}

		const FPendingMultiplayerRoulettePresentation PresentationToStart =
			PendingPresentation;
		PendingMultiplayerRoulettePresentations.RemoveAt(0);
		PlayMultiplayerRoulettePresentation(
			PresentationToStart.TargetSlot,
			PresentationToStart.bHit,
			PresentationToStart.MatchSequence,
			PresentationToStart.RoundSequence);
		return;
	}
}

ASDSelfShotGunActor::EMultiplayerPresentationContextRelation
ASDSelfShotGunActor::CompareMultiplayerPresentationContext(
	int32 MatchSequence,
	int32 RoundSequence) const
{
	const AShowDownGameStateBase* ShowDownGameState = BoundShowDownGameState.Get();
	if (!ShowDownGameState)
	{
		const UWorld* World = GetWorld();
		ShowDownGameState = World ? World->GetGameState<AShowDownGameStateBase>() : nullptr;
	}
	if (!ShowDownGameState)
	{
		return EMultiplayerPresentationContextRelation::Future;
	}

	const int32 CurrentMatchSequence = ShowDownGameState->MultiplayerMatchSequence;
	const int32 CurrentRoundSequence = ShowDownGameState->MultiplayerRoundSequence;
	if (MatchSequence == CurrentMatchSequence
		&& RoundSequence == CurrentRoundSequence)
	{
		return EMultiplayerPresentationContextRelation::Current;
	}
	if (MatchSequence < CurrentMatchSequence
		|| (MatchSequence == CurrentMatchSequence
			&& RoundSequence < CurrentRoundSequence))
	{
		return EMultiplayerPresentationContextRelation::Past;
	}
	return EMultiplayerPresentationContextRelation::Future;
}

bool ASDSelfShotGunActor::ShouldTreatSlotAsLocalPlayer(EShowDownPlayerSlot TargetSlot) const
{
	if (TargetSlot == EShowDownPlayerSlot::None)
	{
		return false;
	}

	const AShowDownPlayerController* LocalPlayerController = Cast<AShowDownPlayerController>(
		FindLocalPlayerController(this));
	if (!LocalPlayerController
		|| !LocalPlayerController->IsLocalController()
		|| !LocalPlayerController->GetLocalPlayer())
	{
		return false;
	}
	if (IsGunShotTargetLocalPlayer(
		TargetSlot,
		LocalPlayerController->ResolveLocalShowDownPlayerSlot()))
	{
		return true;
	}

	// Seat assignment may arrive one frame after the reliable shot presentation.
	// The character's local marker gives the victim a deterministic fallback,
	// while observers still fail this check and keep their current first-person view.
	const AShowDownCharacter* TargetCharacter = Cast<AShowDownCharacter>(
		FindMultiplayerShotTarget(TargetSlot));
	return IsValid(TargetCharacter) && TargetCharacter->IsLocalPlayerCharacter();
}

bool ASDSelfShotGunActor::ShouldTreatTargetAsLocalPlayer(AActor* TargetActor) const
{
	const UWorld* World = GetWorld();
	if (!IsValid(TargetActor))
	{
		const APlayerController* LocalPlayerController = FindLocalPlayerController(this);
		return World
			&& World->GetNetMode() == NM_Standalone
			&& LocalPlayerController
			&& LocalPlayerController->IsLocalController()
			&& LocalPlayerController->GetLocalPlayer();
	}

	const AShowDownCharacter* TargetCharacter = Cast<AShowDownCharacter>(TargetActor);
	if (!TargetCharacter)
	{
		return false;
	}

	if (World && World->GetNetMode() == NM_Standalone)
	{
		return TargetCharacter->GetCharacterRole() == EShowDownCharacterRole::Player;
	}

	return ShouldTreatSlotAsLocalPlayer(TargetCharacter->GetPlayerSlot())
		|| TargetCharacter->IsLocalPlayerCharacter();
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

FTransform ASDSelfShotGunActor::GetPresentationGunTransform() const
{
	const FTransform BaseTransform = bHasCapturedRestActorTransform
		? RestActorTransform
		: GetActorTransform();
	const FVector SourceLocation = bHasForcedShotSourceLocation
		? ForcedShotSourceLocation
		: BaseTransform.GetLocation();

	if (bHasForcedShotAimLocation || ForcedShotTargetActor.IsValid())
	{
		const FVector AimLocation = bHasForcedShotAimLocation
			? ForcedShotAimLocation
			: ForcedShotTargetActor->GetActorLocation() + TargetShotAimOffset;
		return MakeTargetShotTransform(
			SourceLocation,
			AimLocation,
			bHasForcedShotRotationOffset ? ForcedShotRotationOffset : FRotator::ZeroRotator);
	}

	return BaseTransform;
}

void ASDSelfShotGunActor::SetActorTransformAlpha(const FTransform& FromTransform, const FTransform& ToTransform, float Alpha)
{
	const FVector NewLocation = FMath::Lerp(FromTransform.GetLocation(), ToTransform.GetLocation(), Alpha);
	const FQuat NewRotation = FQuat::Slerp(FromTransform.GetRotation(), ToTransform.GetRotation(), Alpha);
	const FVector NewScale = FMath::Lerp(FromTransform.GetScale3D(), ToTransform.GetScale3D(), Alpha);
	SetActorTransform(FTransform(NewRotation, NewLocation, NewScale));
}
