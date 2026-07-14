#include "Presentation/SDVisionDirector.h"

#include "Components/PostProcessComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/SpotLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	const TCHAR* DefaultDarknessMaterialPath = TEXT("/Game/ArtTone/M_PP_TableVisionWorldRange.M_PP_TableVisionWorldRange");
	const TCHAR* DeprecatedDarknessMaterialPath = TEXT("/Game/ArtTone/M_PP_TableVisionDarkness.M_PP_TableVisionDarkness");
	constexpr float GameplayVisionRadius = 100.0f;
	constexpr float GameplayVisionFeather = 200.0f;
	constexpr float GameplayDarknessStrength = 0.4f;
	constexpr float MatchEntryVisionRadius = 5000.0f;
	const FName DarknessStrengthParameterName(TEXT("DarknessStrength"));
	const FName VisionCenterParameterName(TEXT("VisionCenter"));
	const FName VisionRadiusParameterName(TEXT("VisionRadius"));
	const FName VisionFeatherParameterName(TEXT("VisionFeather"));
}

ASDVisionDirector::ASDVisionDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PostProcessComponent = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcessComponent"));
	PostProcessComponent->SetupAttachment(SceneRoot);
	PostProcessComponent->bUnbound = bUnboundPostProcess;
	PostProcessComponent->Priority = PostProcessPriority;
	PostProcessComponent->BlendWeight = 1.0f;

	FocusedVision.VisionRadius = GameplayVisionRadius;
	FocusedVision.VisionFeather = GameplayVisionFeather;
	FocusedVision.DarknessStrength = GameplayDarknessStrength;

	WideVision.VisionRadius = 1800.0f;
	WideVision.VisionFeather = 360.0f;
	WideVision.DarknessStrength = 0.25f;

	CurrentState = FocusedVision;
	VisionBlendStartState = CurrentState;
	TargetVisionState = CurrentState;
	TargetDarknessStrength = CurrentState.DarknessStrength;
	VisionRangeBlendStartRadius = CurrentState.VisionRadius;
	VisionRangeBlendStartFeather = CurrentState.VisionFeather;
	TargetVisionRangeRadius = CurrentState.VisionRadius;
	TargetVisionRangeFeather = CurrentState.VisionFeather;
}

void ASDVisionDirector::BeginPlay()
{
	Super::BeginPlay();

	PostProcessComponent->bUnbound = bUnboundPostProcess;
	PostProcessComponent->Priority = PostProcessPriority;

	// The placed main-map actor predates the gameplay iris preset and may still
	// carry serialized editor overrides. Normalize the runtime preset while
	// leaving the user's map asset untouched.
	FocusedVision.VisionRadius = GameplayVisionRadius;
	FocusedVision.VisionFeather = GameplayVisionFeather;
	FocusedVision.DarknessStrength = GameplayDarknessStrength;
	InitialDarknessStrength = 0.0f;
	IntroWideVisionRadius = MatchEntryVisionRadius;
	IntroWideVisionFeather = GameplayVisionFeather;
	NormalizeAuthoredTableSpotLights();

	// Establish one safe state before the first rendered game frame. Applying the
	// focused preset first could briefly expose its (often fully black) darkness.
	bVisionBlendActive = false;
	bDarknessStrengthBlendActive = false;
	bVisionRangeBlendActive = false;
	bVisionCenterBlendActive = false;
	CurrentVisionAlpha = bApplyInitialStateOnBeginPlay
		? 0.0f
		: FMath::Clamp(EditorPreviewVisionAlpha, 0.0f, 1.0f);
	CurrentState = LerpVisionState(FocusedVision, WideVision, CurrentVisionAlpha);
	CurrentState.DarknessStrength = FMath::Clamp(InitialDarknessStrength, 0.0f, 1.0f);
	VisionBlendStartState = CurrentState;
	TargetVisionState = CurrentState;
	VisionBlendStartAlpha = CurrentVisionAlpha;
	TargetVisionAlpha = CurrentVisionAlpha;
	DarknessStrengthBlendStart = CurrentState.DarknessStrength;
	TargetDarknessStrength = CurrentState.DarknessStrength;
	VisionRangeBlendStartRadius = CurrentState.VisionRadius;
	VisionRangeBlendStartFeather = CurrentState.VisionFeather;
	TargetVisionRangeRadius = CurrentState.VisionRadius;
	TargetVisionRangeFeather = CurrentState.VisionFeather;
	ApplyCurrentState();
	UpdateTickState();
}

void ASDVisionDirector::NormalizeAuthoredTableSpotLights()
{
	UWorld* World = GetWorld();
	if (!bNormalizeAuthoredTableSpotLights
		|| !World
		|| World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	constexpr float MinimumLegacyIntensity = 50000.0f;
	// Includes the table, both seats, and the tight gameplay staging around it.
	// The authored spotlights aim at different points in this area, so testing
	// only the exact vision-center point misses every real map light.
	constexpr float TableGameplayAreaRadius = 800.0f;
	const FVector TableCenter = GetVisionCenterWorldLocation();
	const float SafeScale = FMath::Clamp(AuthoredTableSpotLightIntensityScale, 0.0f, 1.0f);
	const float SafeMaximum = FMath::Max(0.0f, MaximumAuthoredTableSpotLightIntensity);

	for (TActorIterator<ASpotLight> It(World); It; ++It)
	{
		USpotLightComponent* Light = Cast<USpotLightComponent>(It->GetLightComponent());
		if (!Light
			|| !Light->IsRegistered()
			|| !Light->IsVisible()
			|| !Light->bAffectsWorld
			|| Light->GetLightUnits() != ELightUnits::Unitless
			|| Light->Intensity < MinimumLegacyIntensity)
		{
			continue;
		}

		const FVector ToTable = TableCenter - Light->GetComponentLocation();
		const float Distance = ToTable.Size();
		if (Distance <= KINDA_SMALL_NUMBER
			|| Distance > Light->AttenuationRadius + TableGameplayAreaRadius)
		{
			continue;
		}

		const float AreaAngularRadius = FMath::RadiansToDegrees(FMath::Asin(
			FMath::Clamp(TableGameplayAreaRadius / Distance, 0.0f, 1.0f)));
		const float MaximumAreaAngle = FMath::Clamp(
			Light->OuterConeAngle + AreaAngularRadius,
			0.0f,
			180.0f);
		const float DirectionDot = FVector::DotProduct(
			Light->GetForwardVector(),
			ToTable / Distance);
		if (DirectionDot < FMath::Cos(FMath::DegreesToRadians(MaximumAreaAngle)))
		{
			continue;
		}

		// Static lighting is disabled for this project, but several legacy actors
		// are still authored Static. Promote only those; Stationary lights already
		// accept dynamic intensity changes and should keep their cheaper mobility.
		if (Light->Mobility == EComponentMobility::Static)
		{
			Light->SetMobility(EComponentMobility::Movable);
		}
		Light->SetIntensity(FMath::Min(Light->Intensity * SafeScale, SafeMaximum));
	}
}

void ASDVisionDirector::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	PostProcessComponent->bUnbound = bUnboundPostProcess;
	PostProcessComponent->Priority = PostProcessPriority;

	CurrentVisionAlpha = EditorPreviewVisionAlpha;
	CurrentState = LerpVisionState(FocusedVision, WideVision, CurrentVisionAlpha);
	bVisionBlendActive = false;
	bDarknessStrengthBlendActive = false;
	bVisionRangeBlendActive = false;
	bVisionCenterBlendActive = false;
	VisionBlendStartState = CurrentState;
	TargetVisionState = CurrentState;
	TargetVisionAlpha = CurrentVisionAlpha;
	TargetDarknessStrength = CurrentState.DarknessStrength;
	VisionRangeBlendStartRadius = CurrentState.VisionRadius;
	VisionRangeBlendStartFeather = CurrentState.VisionFeather;
	TargetVisionRangeRadius = CurrentState.VisionRadius;
	TargetVisionRangeFeather = CurrentState.VisionFeather;
	ApplyCurrentState();
}

void ASDVisionDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const bool bWasVisionBlending = bVisionBlendActive;
	const bool bWasDarknessBlending = bDarknessStrengthBlendActive;
	const bool bWasRangeBlending = bVisionRangeBlendActive;
	const bool bWasCenterBlending = bVisionCenterBlendActive;

	AdvanceVisionBlend(DeltaSeconds);
	AdvanceDarknessStrengthBlend(DeltaSeconds);
	AdvanceVisionRangeBlend(DeltaSeconds);
	AdvanceVisionCenterBlend(DeltaSeconds);

	if (bWasVisionBlending || bWasDarknessBlending || bWasRangeBlending)
	{
		ApplyCurrentState();
	}
	else if (bTrackVisionCenterEveryTick || bWasCenterBlending)
	{
		ApplyDarknessMaterialState();
	}

	UpdateTickState();
}

#if WITH_EDITOR
void ASDVisionDirector::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(ASDVisionDirector, DarknessPostProcessMaterial))
	{
		DarknessMID = nullptr;
	}

	PostProcessComponent->bUnbound = bUnboundPostProcess;
	PostProcessComponent->Priority = PostProcessPriority;
	CurrentVisionAlpha = EditorPreviewVisionAlpha;
	CurrentState = LerpVisionState(FocusedVision, WideVision, CurrentVisionAlpha);
	bVisionBlendActive = false;
	bDarknessStrengthBlendActive = false;
	bVisionRangeBlendActive = false;
	bVisionCenterBlendActive = false;
	VisionBlendStartState = CurrentState;
	TargetVisionState = CurrentState;
	TargetVisionAlpha = CurrentVisionAlpha;
	TargetDarknessStrength = CurrentState.DarknessStrength;
	VisionRangeBlendStartRadius = CurrentState.VisionRadius;
	VisionRangeBlendStartFeather = CurrentState.VisionFeather;
	TargetVisionRangeRadius = CurrentState.VisionRadius;
	TargetVisionRangeFeather = CurrentState.VisionFeather;
	ApplyCurrentState();
}
#endif

void ASDVisionDirector::ApplyFocusedVision()
{
	SetVisionAlpha(0.0f);
}

void ASDVisionDirector::ApplyWideVision()
{
	SetVisionAlpha(1.0f);
}

void ASDVisionDirector::SetVisionAlpha(float Alpha)
{
	bVisionBlendActive = false;
	bDarknessStrengthBlendActive = false;
	bVisionRangeBlendActive = false;
	CurrentVisionAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	CurrentState = LerpVisionState(FocusedVision, WideVision, CurrentVisionAlpha);
	VisionBlendStartAlpha = CurrentVisionAlpha;
	TargetVisionAlpha = CurrentVisionAlpha;
	VisionBlendStartState = CurrentState;
	TargetVisionState = CurrentState;
	DarknessStrengthBlendStart = CurrentState.DarknessStrength;
	TargetDarknessStrength = CurrentState.DarknessStrength;
	ApplyCurrentState();
	UpdateTickState();
}

void ASDVisionDirector::BlendToVisionAlpha(
	float TargetAlpha,
	float Duration,
	ESDVisionBlendEase EaseMode,
	float EaseExponent)
{
	const float ClampedTargetAlpha = FMath::Clamp(TargetAlpha, 0.0f, 1.0f);
	const FSDVisionState NewTargetState = LerpVisionState(FocusedVision, WideVision, ClampedTargetAlpha);

	if (Duration <= KINDA_SMALL_NUMBER)
	{
		SetVisionAlpha(ClampedTargetAlpha);
		return;
	}

	// Alpha and direct-darkness blends both own DarknessStrength, so they are
	// deliberately mutually exclusive. CurrentState is already the exact value
	// displayed, making this handoff and any retarget continuous.
	bDarknessStrengthBlendActive = false;
	bVisionRangeBlendActive = false;
	VisionBlendStartAlpha = CurrentVisionAlpha;
	TargetVisionAlpha = ClampedTargetAlpha;
	VisionBlendStartState = CurrentState;
	TargetVisionState = NewTargetState;
	VisionBlendDuration = Duration;
	VisionBlendElapsed = 0.0f;
	VisionBlendEaseMode = EaseMode;
	VisionBlendEaseExponent = FMath::Max(1.0f, EaseExponent);
	bVisionBlendActive = true;

	UpdateTickState();
}

void ASDVisionDirector::BlendToFocusedVision(
	float Duration,
	ESDVisionBlendEase EaseMode,
	float EaseExponent)
{
	BlendToVisionAlpha(0.0f, Duration, EaseMode, EaseExponent);
}

void ASDVisionDirector::BlendToWideVision(
	float Duration,
	ESDVisionBlendEase EaseMode,
	float EaseExponent)
{
	BlendToVisionAlpha(1.0f, Duration, EaseMode, EaseExponent);
}

void ASDVisionDirector::CancelVisionBlend()
{
	if (!bVisionBlendActive)
	{
		return;
	}

	bVisionBlendActive = false;
	VisionBlendStartAlpha = CurrentVisionAlpha;
	TargetVisionAlpha = CurrentVisionAlpha;
	VisionBlendStartState = CurrentState;
	TargetVisionState = CurrentState;
	UpdateTickState();
}

void ASDVisionDirector::CompleteVisionBlend()
{
	if (!bVisionBlendActive)
	{
		return;
	}

	SetVisionAlpha(TargetVisionAlpha);
}

float ASDVisionDirector::EvaluateVisionBlendEase(
	float NormalizedAlpha,
	ESDVisionBlendEase EaseMode,
	float EaseExponent)
{
	const float ClampedAlpha = FMath::Clamp(NormalizedAlpha, 0.0f, 1.0f);
	const float ClampedExponent = FMath::Max(1.0f, EaseExponent);

	switch (EaseMode)
	{
	case ESDVisionBlendEase::EaseIn:
		return FMath::InterpEaseIn(0.0f, 1.0f, ClampedAlpha, ClampedExponent);
	case ESDVisionBlendEase::EaseOut:
		return FMath::InterpEaseOut(0.0f, 1.0f, ClampedAlpha, ClampedExponent);
	case ESDVisionBlendEase::EaseInOut:
		return FMath::InterpEaseInOut(0.0f, 1.0f, ClampedAlpha, ClampedExponent);
	case ESDVisionBlendEase::Linear:
	default:
		return ClampedAlpha;
	}
}

void ASDVisionDirector::SetDarknessStrength(float Strength)
{
	bVisionBlendActive = false;
	bDarknessStrengthBlendActive = false;
	CurrentState.DarknessStrength = FMath::Clamp(Strength, 0.0f, 1.0f);
	DarknessStrengthBlendStart = CurrentState.DarknessStrength;
	TargetDarknessStrength = CurrentState.DarknessStrength;
	ApplyCurrentState();
	UpdateTickState();
}

void ASDVisionDirector::BlendToDarknessStrength(
	float TargetStrength,
	float Duration,
	ESDVisionBlendEase EaseMode,
	float EaseExponent)
{
	const float ClampedTargetStrength = FMath::Clamp(TargetStrength, 0.0f, 1.0f);
	if (Duration <= KINDA_SMALL_NUMBER)
	{
		SetDarknessStrength(ClampedTargetStrength);
		return;
	}

	bVisionBlendActive = false;
	DarknessStrengthBlendStart = CurrentState.DarknessStrength;
	TargetDarknessStrength = ClampedTargetStrength;
	DarknessStrengthBlendDuration = Duration;
	DarknessStrengthBlendElapsed = 0.0f;
	DarknessStrengthBlendEaseMode = EaseMode;
	DarknessStrengthBlendEaseExponent = FMath::Max(1.0f, EaseExponent);
	bDarknessStrengthBlendActive = true;

	UpdateTickState();
}

void ASDVisionDirector::CancelDarknessStrengthBlend()
{
	if (!bDarknessStrengthBlendActive)
	{
		return;
	}

	bDarknessStrengthBlendActive = false;
	DarknessStrengthBlendStart = CurrentState.DarknessStrength;
	TargetDarknessStrength = CurrentState.DarknessStrength;
	UpdateTickState();
}

void ASDVisionDirector::CompleteDarknessStrengthBlend()
{
	if (!bDarknessStrengthBlendActive)
	{
		return;
	}

	SetDarknessStrength(TargetDarknessStrength);
}

void ASDVisionDirector::SetVisionRange(float Radius, float Feather)
{
	bVisionBlendActive = false;
	bVisionRangeBlendActive = false;
	CurrentState.VisionRadius = FMath::Max(0.0f, Radius);
	CurrentState.VisionFeather = FMath::Max(1.0f, Feather);
	VisionRangeBlendStartRadius = CurrentState.VisionRadius;
	VisionRangeBlendStartFeather = CurrentState.VisionFeather;
	TargetVisionRangeRadius = CurrentState.VisionRadius;
	TargetVisionRangeFeather = CurrentState.VisionFeather;
	ApplyCurrentState();
	UpdateTickState();
}

void ASDVisionDirector::BlendToVisionRange(
	float TargetRadius,
	float TargetFeather,
	float Duration,
	ESDVisionBlendEase EaseMode,
	float EaseExponent)
{
	const float SafeTargetRadius = FMath::Max(0.0f, TargetRadius);
	const float SafeTargetFeather = FMath::Max(1.0f, TargetFeather);
	if (Duration <= KINDA_SMALL_NUMBER)
	{
		SetVisionRange(SafeTargetRadius, SafeTargetFeather);
		return;
	}

	// Alpha blends also own radius and feather. Direct range animation takes
	// over those two fields while leaving the independent darkness track intact.
	bVisionBlendActive = false;
	VisionRangeBlendStartRadius = CurrentState.VisionRadius;
	VisionRangeBlendStartFeather = CurrentState.VisionFeather;
	TargetVisionRangeRadius = SafeTargetRadius;
	TargetVisionRangeFeather = SafeTargetFeather;
	VisionRangeBlendDuration = Duration;
	VisionRangeBlendElapsed = 0.0f;
	VisionRangeBlendEaseMode = EaseMode;
	VisionRangeBlendEaseExponent = FMath::Max(1.0f, EaseExponent);
	bVisionRangeBlendActive = true;
	UpdateTickState();
}

void ASDVisionDirector::CancelVisionRangeBlend()
{
	if (!bVisionRangeBlendActive)
	{
		return;
	}

	bVisionRangeBlendActive = false;
	VisionRangeBlendStartRadius = CurrentState.VisionRadius;
	VisionRangeBlendStartFeather = CurrentState.VisionFeather;
	TargetVisionRangeRadius = CurrentState.VisionRadius;
	TargetVisionRangeFeather = CurrentState.VisionFeather;
	UpdateTickState();
}

void ASDVisionDirector::CompleteVisionRangeBlend()
{
	if (!bVisionRangeBlendActive)
	{
		return;
	}

	SetVisionRange(TargetVisionRangeRadius, TargetVisionRangeFeather);
}

void ASDVisionDirector::SetVisionCenterActor(AActor* NewVisionCenterActor)
{
	bVisionCenterBlendActive = false;
	TargetVisionCenterActor = nullptr;
	VisionCenterActor = NewVisionCenterActor;
	bUseExplicitVisionCenterLocation = false;
	ApplyDarknessMaterialState();
	UpdateTickState();
}

void ASDVisionDirector::SetVisionCenterWorldLocation(FVector NewWorldLocation)
{
	bVisionCenterBlendActive = false;
	TargetVisionCenterActor = nullptr;
	VisionCenterActor = nullptr;
	bUseExplicitVisionCenterLocation = true;
	ExplicitVisionCenterWorldLocation = NewWorldLocation;
	ApplyDarknessMaterialState();
	UpdateTickState();
}

void ASDVisionDirector::BlendVisionCenterToActor(
	AActor* TargetActor,
	float Duration,
	ESDVisionBlendEase EaseMode,
	float EaseExponent)
{
	if (Duration <= KINDA_SMALL_NUMBER)
	{
		SetVisionCenterActor(TargetActor);
		return;
	}

	VisionCenterBlendStartLocation = GetVisionCenterWorldLocation();
	BlendedVisionCenterWorldLocation = VisionCenterBlendStartLocation;
	TargetVisionCenterActor = TargetActor;
	bVisionCenterBlendTargetsActor = true;
	VisionCenterBlendDuration = Duration;
	VisionCenterBlendElapsed = 0.0f;
	VisionCenterBlendEaseMode = EaseMode;
	VisionCenterBlendEaseExponent = FMath::Max(1.0f, EaseExponent);
	bVisionCenterBlendActive = true;
	UpdateTickState();
}

void ASDVisionDirector::BlendVisionCenterToWorldLocation(
	FVector TargetWorldLocation,
	float Duration,
	ESDVisionBlendEase EaseMode,
	float EaseExponent)
{
	if (Duration <= KINDA_SMALL_NUMBER)
	{
		SetVisionCenterWorldLocation(TargetWorldLocation);
		return;
	}

	VisionCenterBlendStartLocation = GetVisionCenterWorldLocation();
	BlendedVisionCenterWorldLocation = VisionCenterBlendStartLocation;
	TargetVisionCenterWorldLocation = TargetWorldLocation;
	TargetVisionCenterActor = nullptr;
	bVisionCenterBlendTargetsActor = false;
	VisionCenterBlendDuration = Duration;
	VisionCenterBlendElapsed = 0.0f;
	VisionCenterBlendEaseMode = EaseMode;
	VisionCenterBlendEaseExponent = FMath::Max(1.0f, EaseExponent);
	bVisionCenterBlendActive = true;
	UpdateTickState();
}

void ASDVisionDirector::CancelVisionCenterBlend()
{
	if (!bVisionCenterBlendActive)
	{
		return;
	}

	const FVector CurrentLocation = GetVisionCenterWorldLocation();
	bVisionCenterBlendActive = false;
	TargetVisionCenterActor = nullptr;
	VisionCenterActor = nullptr;
	bUseExplicitVisionCenterLocation = true;
	ExplicitVisionCenterWorldLocation = CurrentLocation;
	BlendedVisionCenterWorldLocation = CurrentLocation;
	ApplyDarknessMaterialState();
	UpdateTickState();
}

void ASDVisionDirector::CompleteVisionCenterBlend()
{
	if (!bVisionCenterBlendActive)
	{
		return;
	}

	if (bVisionCenterBlendTargetsActor)
	{
		SetVisionCenterActor(TargetVisionCenterActor);
	}
	else
	{
		SetVisionCenterWorldLocation(TargetVisionCenterWorldLocation);
	}
}

void ASDVisionDirector::EnsureDarknessMaterialInstance()
{
	if (DarknessPostProcessMaterial && DarknessPostProcessMaterial->GetPathName() == DeprecatedDarknessMaterialPath)
	{
		DarknessPostProcessMaterial = nullptr;
		DarknessMID = nullptr;
	}

	if (!DarknessPostProcessMaterial && bAutoLoadDefaultDarknessMaterial)
	{
		DarknessPostProcessMaterial = LoadObject<UMaterialInterface>(nullptr, DefaultDarknessMaterialPath);
	}

	if (!DarknessPostProcessMaterial || !PostProcessComponent)
	{
		return;
	}

	if (!DarknessMID)
	{
		DarknessMID = UMaterialInstanceDynamic::Create(DarknessPostProcessMaterial, this);
	}

	if (!DarknessMID)
	{
		return;
	}

	FWeightedBlendables& WeightedBlendables = PostProcessComponent->Settings.WeightedBlendables;
	for (int32 Index = WeightedBlendables.Array.Num() - 1; Index >= 0; --Index)
	{
		const UObject* BlendableObject = WeightedBlendables.Array[Index].Object.Get();
		if (IsMatchingBlendable(BlendableObject, DarknessPostProcessMaterial, DarknessMID))
		{
			WeightedBlendables.Array.RemoveAt(Index);
		}
	}

	WeightedBlendables.Array.Add(FWeightedBlendable(1.0f, DarknessMID));
}

void ASDVisionDirector::ApplyCurrentState()
{
	ApplyPostProcessState();
	ApplyDarknessMaterialState();
}

void ASDVisionDirector::ApplyPostProcessState()
{
	if (!PostProcessComponent)
	{
		return;
	}

	PostProcessComponent->BlendWeight = ShouldApplyPostProcessInCurrentWorld() ? 1.0f : 0.0f;
}

void ASDVisionDirector::ApplyDarknessMaterialState()
{
	if (!PostProcessComponent)
	{
		return;
	}

	if (!bEnableDarknessPostProcess || !ShouldApplyPostProcessInCurrentWorld())
	{
		PostProcessComponent->Settings.WeightedBlendables.Array.Empty();
		return;
	}

	EnsureDarknessMaterialInstance();
	if (!DarknessMID)
	{
		return;
	}

	const FVector VisionCenter = GetVisionCenterWorldLocation();
	DarknessMID->SetVectorParameterValue(
		VisionCenterParameterName,
		FLinearColor(VisionCenter.X, VisionCenter.Y, VisionCenter.Z, 1.0f));
	DarknessMID->SetScalarParameterValue(DarknessStrengthParameterName, FMath::Clamp(CurrentState.DarknessStrength, 0.0f, 1.0f));
	DarknessMID->SetScalarParameterValue(VisionRadiusParameterName, FMath::Max(0.0f, CurrentState.VisionRadius));
	DarknessMID->SetScalarParameterValue(VisionFeatherParameterName, FMath::Max(1.0f, CurrentState.VisionFeather));
}

void ASDVisionDirector::UpdateTickState()
{
	SetActorTickEnabled(
		bTrackVisionCenterEveryTick
		|| bVisionBlendActive
		|| bDarknessStrengthBlendActive
		|| bVisionRangeBlendActive
		|| bVisionCenterBlendActive);
}

FVector ASDVisionDirector::GetVisionCenterWorldLocation() const
{
	if (bVisionCenterBlendActive)
	{
		return BlendedVisionCenterWorldLocation;
	}

	if (IsValid(VisionCenterActor))
	{
		return VisionCenterActor->GetActorLocation();
	}

	return bUseExplicitVisionCenterLocation
		? ExplicitVisionCenterWorldLocation
		: GetActorLocation();
}

void ASDVisionDirector::AdvanceVisionBlend(float DeltaSeconds)
{
	if (!bVisionBlendActive)
	{
		return;
	}

	VisionBlendElapsed += FMath::Max(0.0f, DeltaSeconds);
	const float NormalizedAlpha = VisionBlendDuration <= KINDA_SMALL_NUMBER
		? 1.0f
		: FMath::Clamp(VisionBlendElapsed / VisionBlendDuration, 0.0f, 1.0f);
	const float EasedAlpha = EvaluateVisionBlendEase(
		NormalizedAlpha,
		VisionBlendEaseMode,
		VisionBlendEaseExponent);

	CurrentVisionAlpha = FMath::Lerp(VisionBlendStartAlpha, TargetVisionAlpha, EasedAlpha);
	CurrentState = LerpVisionState(VisionBlendStartState, TargetVisionState, EasedAlpha);

	if (NormalizedAlpha >= 1.0f)
	{
		CurrentVisionAlpha = TargetVisionAlpha;
		CurrentState = TargetVisionState;
		bVisionBlendActive = false;
		VisionBlendStartAlpha = CurrentVisionAlpha;
		VisionBlendStartState = CurrentState;
	}
}

void ASDVisionDirector::AdvanceDarknessStrengthBlend(float DeltaSeconds)
{
	if (!bDarknessStrengthBlendActive)
	{
		return;
	}

	DarknessStrengthBlendElapsed += FMath::Max(0.0f, DeltaSeconds);
	const float NormalizedAlpha = DarknessStrengthBlendDuration <= KINDA_SMALL_NUMBER
		? 1.0f
		: FMath::Clamp(DarknessStrengthBlendElapsed / DarknessStrengthBlendDuration, 0.0f, 1.0f);
	const float EasedAlpha = EvaluateVisionBlendEase(
		NormalizedAlpha,
		DarknessStrengthBlendEaseMode,
		DarknessStrengthBlendEaseExponent);

	CurrentState.DarknessStrength = FMath::Lerp(
		DarknessStrengthBlendStart,
		TargetDarknessStrength,
		EasedAlpha);

	if (NormalizedAlpha >= 1.0f)
	{
		CurrentState.DarknessStrength = TargetDarknessStrength;
		bDarknessStrengthBlendActive = false;
		DarknessStrengthBlendStart = CurrentState.DarknessStrength;
	}
}

void ASDVisionDirector::AdvanceVisionRangeBlend(float DeltaSeconds)
{
	if (!bVisionRangeBlendActive)
	{
		return;
	}

	VisionRangeBlendElapsed += FMath::Max(0.0f, DeltaSeconds);
	const float NormalizedAlpha = VisionRangeBlendDuration <= KINDA_SMALL_NUMBER
		? 1.0f
		: FMath::Clamp(VisionRangeBlendElapsed / VisionRangeBlendDuration, 0.0f, 1.0f);
	const float EasedAlpha = EvaluateVisionBlendEase(
		NormalizedAlpha,
		VisionRangeBlendEaseMode,
		VisionRangeBlendEaseExponent);

	CurrentState.VisionRadius = FMath::Lerp(
		VisionRangeBlendStartRadius,
		TargetVisionRangeRadius,
		EasedAlpha);
	CurrentState.VisionFeather = FMath::Lerp(
		VisionRangeBlendStartFeather,
		TargetVisionRangeFeather,
		EasedAlpha);

	if (NormalizedAlpha >= 1.0f)
	{
		CurrentState.VisionRadius = TargetVisionRangeRadius;
		CurrentState.VisionFeather = TargetVisionRangeFeather;
		bVisionRangeBlendActive = false;
		VisionRangeBlendStartRadius = CurrentState.VisionRadius;
		VisionRangeBlendStartFeather = CurrentState.VisionFeather;
	}
}

void ASDVisionDirector::AdvanceVisionCenterBlend(float DeltaSeconds)
{
	if (!bVisionCenterBlendActive)
	{
		return;
	}

	VisionCenterBlendElapsed += FMath::Max(0.0f, DeltaSeconds);
	const float NormalizedAlpha = VisionCenterBlendDuration <= KINDA_SMALL_NUMBER
		? 1.0f
		: FMath::Clamp(VisionCenterBlendElapsed / VisionCenterBlendDuration, 0.0f, 1.0f);
	const float EasedAlpha = EvaluateVisionBlendEase(
		NormalizedAlpha,
		VisionCenterBlendEaseMode,
		VisionCenterBlendEaseExponent);
	BlendedVisionCenterWorldLocation = FMath::Lerp(
		VisionCenterBlendStartLocation,
		GetVisionCenterBlendTargetLocation(),
		EasedAlpha);

	if (NormalizedAlpha < 1.0f)
	{
		return;
	}

	if (bVisionCenterBlendTargetsActor)
	{
		VisionCenterActor = TargetVisionCenterActor;
		bUseExplicitVisionCenterLocation = false;
	}
	else
	{
		VisionCenterActor = nullptr;
		bUseExplicitVisionCenterLocation = true;
		ExplicitVisionCenterWorldLocation = TargetVisionCenterWorldLocation;
	}

	bVisionCenterBlendActive = false;
	TargetVisionCenterActor = nullptr;
}

FVector ASDVisionDirector::GetVisionCenterBlendTargetLocation() const
{
	if (bVisionCenterBlendTargetsActor)
	{
		return IsValid(TargetVisionCenterActor)
			? TargetVisionCenterActor->GetActorLocation()
			: GetActorLocation();
	}

	return TargetVisionCenterWorldLocation;
}

bool ASDVisionDirector::ShouldApplyPostProcessInCurrentWorld() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	return World->IsGameWorld() || bPreviewPostProcessInEditor;
}

FSDVisionState ASDVisionDirector::LerpVisionState(const FSDVisionState& From, const FSDVisionState& To, float Alpha)
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);

	FSDVisionState Result;
	Result.VisionRadius = FMath::Lerp(From.VisionRadius, To.VisionRadius, ClampedAlpha);
	Result.VisionFeather = FMath::Lerp(From.VisionFeather, To.VisionFeather, ClampedAlpha);
	Result.DarknessStrength = FMath::Lerp(From.DarknessStrength, To.DarknessStrength, ClampedAlpha);
	return Result;
}

bool ASDVisionDirector::IsMatchingBlendable(
	const UObject* BlendableObject,
	const UMaterialInterface* Material,
	const UMaterialInstanceDynamic* MID)
{
	if (!BlendableObject)
	{
		return false;
	}

	if (BlendableObject == Material || BlendableObject == MID)
	{
		return true;
	}

	const UMaterialInterface* BlendableMaterial = Cast<UMaterialInterface>(BlendableObject);
	return BlendableMaterial && Material && BlendableMaterial->GetMaterial() == Material->GetMaterial();
}
