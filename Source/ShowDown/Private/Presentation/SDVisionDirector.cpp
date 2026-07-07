#include "Presentation/SDVisionDirector.h"

#include "Components/PostProcessComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	const TCHAR* DefaultDarknessMaterialPath = TEXT("/Game/ArtTone/M_PP_TableVisionWorldRange.M_PP_TableVisionWorldRange");
	const TCHAR* DeprecatedDarknessMaterialPath = TEXT("/Game/ArtTone/M_PP_TableVisionDarkness.M_PP_TableVisionDarkness");
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

	FocusedVision.VisionRadius = 450.0f;
	FocusedVision.VisionFeather = 120.0f;
	FocusedVision.DarknessStrength = 1.0f;

	WideVision.VisionRadius = 1800.0f;
	WideVision.VisionFeather = 360.0f;
	WideVision.DarknessStrength = 0.25f;

	CurrentState = FocusedVision;
}

void ASDVisionDirector::BeginPlay()
{
	Super::BeginPlay();

	PostProcessComponent->bUnbound = bUnboundPostProcess;
	PostProcessComponent->Priority = PostProcessPriority;

	if (bApplyInitialStateOnBeginPlay)
	{
		ApplyFocusedVision();
	}
	else
	{
		UpdateTickState();
	}
}

void ASDVisionDirector::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	PostProcessComponent->bUnbound = bUnboundPostProcess;
	PostProcessComponent->Priority = PostProcessPriority;

	CurrentVisionAlpha = EditorPreviewVisionAlpha;
	CurrentState = LerpVisionState(FocusedVision, WideVision, CurrentVisionAlpha);
	ApplyCurrentState();
}

void ASDVisionDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bTrackVisionCenterEveryTick)
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
	CurrentVisionAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	CurrentState = LerpVisionState(FocusedVision, WideVision, CurrentVisionAlpha);
	ApplyCurrentState();
	UpdateTickState();
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
	SetActorTickEnabled(bTrackVisionCenterEveryTick);
}

FVector ASDVisionDirector::GetVisionCenterWorldLocation() const
{
	return VisionCenterActor ? VisionCenterActor->GetActorLocation() : GetActorLocation();
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
