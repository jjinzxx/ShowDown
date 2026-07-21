#include "Presentation/SDBetActionPanelActor.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "SDPlayerState.h"
#include "ShowDownPlayerController.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const ESDBetActionPanelButtonKind ButtonKinds[] =
	{
		ESDBetActionPanelButtonKind::Primary,
		ESDBetActionPanelButtonKind::RaiseDown,
		ESDBetActionPanelButtonKind::RaiseSubmit,
		ESDBetActionPanelButtonKind::RaiseUp,
		ESDBetActionPanelButtonKind::Fold
	};

	FColor ToTextColor(const FLinearColor& Color)
	{
		return Color.ToFColor(true);
	}

	FLinearColor DimColor(const FLinearColor& Color)
	{
		return FLinearColor(
			Color.R * 0.34f,
			Color.G * 0.34f,
			Color.B * 0.34f,
			1.0f);
	}

	float Smooth01(float Alpha)
	{
		return FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp(Alpha, 0.0f, 1.0f));
	}

	float EvaluatePopInScale(float Progress, float BounceStrength)
	{
		const float ClampedProgress = FMath::Clamp(Progress, 0.0f, 1.0f);
		const float Bounce = FMath::Max(0.0f, BounceStrength);
		if (Bounce <= KINDA_SMALL_NUMBER)
		{
			return Smooth01(ClampedProgress);
		}

		constexpr float OvershootPoint = 0.72f;
		if (ClampedProgress < OvershootPoint)
		{
			const float Alpha = ClampedProgress / OvershootPoint;
			const float EaseOut = 1.0f - FMath::Pow(1.0f - Alpha, 3.0f);
			return (1.0f + Bounce) * EaseOut;
		}

		const float SettleAlpha = Smooth01((ClampedProgress - OvershootPoint) / (1.0f - OvershootPoint));
		return FMath::Lerp(1.0f + Bounce, 1.0f, SettleAlpha);
	}

	float EvaluatePopOutScale(float Progress, float BounceStrength)
	{
		const float ClampedProgress = FMath::Clamp(Progress, 0.0f, 1.0f);
		const float Bounce = FMath::Max(0.0f, BounceStrength);
		if (Bounce <= KINDA_SMALL_NUMBER)
		{
			return 1.0f - Smooth01(ClampedProgress);
		}

		constexpr float AnticipationPoint = 0.22f;
		if (ClampedProgress < AnticipationPoint)
		{
			const float Alpha = ClampedProgress / AnticipationPoint;
			return 1.0f + Bounce * FMath::Sin(Alpha * PI);
		}

		const float CollapseAlpha = Smooth01((ClampedProgress - AnticipationPoint) / (1.0f - AnticipationPoint));
		return 1.0f - CollapseAlpha;
	}

	float EvaluateBulletPulseScale(float Progress, float BounceStrength)
	{
		const float ClampedProgress = FMath::Clamp(Progress, 0.0f, 1.0f);
		const float Bounce = FMath::Max(0.0f, BounceStrength);
		const float CompressedScale = 1.0f - FMath::Clamp(0.08f + Bounce * 0.35f, 0.08f, 0.2f);
		if (ClampedProgress < 0.22f)
		{
			return FMath::Lerp(1.0f, CompressedScale, Smooth01(ClampedProgress / 0.22f));
		}
		if (ClampedProgress < 0.64f)
		{
			return FMath::Lerp(
				CompressedScale,
				1.0f + Bounce,
				Smooth01((ClampedProgress - 0.22f) / 0.42f));
		}

		return FMath::Lerp(
			1.0f + Bounce,
			1.0f,
			Smooth01((ClampedProgress - 0.64f) / 0.36f));
	}
}

ASDBetActionPanelActor::ASDBetActionPanelActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.15f;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(14.0f);
	SetMinNetUpdateFrequency(4.0f);
	SetActorTickEnabled(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BulletMeshFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BulletMaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	BulletPreviewMeshAsset = BulletMeshFinder.Succeeded() ? BulletMeshFinder.Object : nullptr;
	BulletPreviewTintMaterial = BulletMaterialFinder.Succeeded() ? BulletMaterialFinder.Object : nullptr;
	for (int32 BulletIndex = 0; BulletIndex < 6; ++BulletIndex)
	{
		UStaticMeshComponent* OutlineMesh = CreateDefaultSubobject<UStaticMeshComponent>(
			*FString::Printf(TEXT("RaiseBulletPreviewOutline%d"), BulletIndex + 1));
		OutlineMesh->SetupAttachment(Root);
		OutlineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		OutlineMesh->SetCanEverAffectNavigation(false);
		OutlineMesh->SetCastShadow(false);
		OutlineMesh->SetVisibility(false, true);
		if (BulletPreviewMeshAsset)
		{
			OutlineMesh->SetStaticMesh(BulletPreviewMeshAsset);
		}
		if (BulletPreviewTintMaterial)
		{
			OutlineMesh->SetMaterial(0, BulletPreviewTintMaterial);
		}
		BulletPreviewOutlineMeshes.Add(OutlineMesh);

		UStaticMeshComponent* BulletMesh = CreateDefaultSubobject<UStaticMeshComponent>(
			*FString::Printf(TEXT("RaiseBulletPreview%d"), BulletIndex + 1));
		BulletMesh->SetupAttachment(Root);
		BulletMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BulletMesh->SetCanEverAffectNavigation(false);
		BulletMesh->SetCastShadow(false);
		BulletMesh->SetVisibility(false, true);
		if (BulletPreviewMeshAsset)
		{
			BulletMesh->SetStaticMesh(BulletPreviewMeshAsset);
		}
		if (BulletPreviewTintMaterial)
		{
			BulletMesh->SetMaterial(0, BulletPreviewTintMaterial);
		}
		BulletPreviewMeshes.Add(BulletMesh);
	}
}

void ASDBetActionPanelActor::BeginPlay()
{
	Super::BeginPlay();
	EnsureButtons();
	RefreshVisuals();
}

void ASDBetActionPanelActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateBulletAnimations(DeltaSeconds);

	if (PanelState.bVisible)
	{
		const EShowDownPlayerSlot CurrentLocalPlayerSlot = ResolveLocalPlayerSlot();
		const bool bShouldBeVisible =
			PanelState.TurnSlot != EShowDownPlayerSlot::None
			&& CurrentLocalPlayerSlot == PanelState.TurnSlot;

		if (CurrentLocalPlayerSlot != LastResolvedLocalPlayerSlot || bShouldBeVisible != bBulletTargetVisible)
		{
			RefreshVisuals();
		}
	}

	RefreshTickState();
}

void ASDBetActionPanelActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (ASDBetActionButtonActor* ButtonActor : ButtonActors)
	{
		if (IsValid(ButtonActor))
		{
			ButtonActor->Destroy();
		}
	}
	ButtonActors.Reset();

	Super::EndPlay(EndPlayReason);
}

void ASDBetActionPanelActor::SetPanelState(const FSDBetActionPanelState& NewState)
{
	if (!HasAuthority())
	{
		return;
	}

	PanelState = NewState;
	RefreshVisuals();
	ForceNetUpdate();
}

void ASDBetActionPanelActor::OnRep_PanelState()
{
	RefreshVisuals();
}

void ASDBetActionPanelActor::EnsureButtons()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (ButtonActors.Num() != ButtonCount)
	{
		ButtonActors.SetNum(ButtonCount);
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UClass* SpawnClass = ButtonActorClass
		? ButtonActorClass.Get()
		: ASDBetActionButtonActor::StaticClass();
	if (!SpawnClass)
	{
		return;
	}

	for (ESDBetActionPanelButtonKind ButtonKind : ButtonKinds)
	{
		const int32 ButtonIndex = GetButtonIndex(ButtonKind);
		if (!ButtonActors.IsValidIndex(ButtonIndex) || IsValid(ButtonActors[ButtonIndex]))
		{
			continue;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ASDBetActionButtonActor* ButtonActor = World->SpawnActor<ASDBetActionButtonActor>(
			SpawnClass,
			FTransform::Identity,
			SpawnParams);
		if (!ButtonActor)
		{
			continue;
		}

		ButtonActor->InitializeButton(this, ButtonKind);
		ButtonActors[ButtonIndex] = ButtonActor;
	}
}

void ASDBetActionPanelActor::EnsureBulletPreview()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	BulletPreviewMaterials.SetNum(BulletPreviewMeshes.Num());
	BulletPreviewOutlineMaterials.SetNum(BulletPreviewOutlineMeshes.Num());
	const int32 PreviousAnimationCount = BulletVisualAlphas.Num();
	BulletVisualAlphas.SetNum(BulletPreviewMeshes.Num());
	BulletVisualScales.SetNum(BulletPreviewMeshes.Num());
	BulletTransitionStartAlphas.SetNum(BulletPreviewMeshes.Num());
	BulletTransitionStartScales.SetNum(BulletPreviewMeshes.Num());
	BulletTransitionElapsedTimes.SetNum(BulletPreviewMeshes.Num());
	BulletPulseElapsedTimes.SetNum(BulletPreviewMeshes.Num());
	for (int32 BulletIndex = PreviousAnimationCount; BulletIndex < BulletPreviewMeshes.Num(); ++BulletIndex)
	{
		BulletVisualAlphas[BulletIndex] = 0.0f;
		BulletVisualScales[BulletIndex] = 0.0f;
		BulletTransitionStartAlphas[BulletIndex] = 0.0f;
		BulletTransitionStartScales[BulletIndex] = 0.0f;
		BulletTransitionElapsedTimes[BulletIndex] = 0.0f;
		BulletPulseElapsedTimes[BulletIndex] = -1.0f;
	}
	for (int32 BulletIndex = 0; BulletIndex < BulletPreviewMeshes.Num(); ++BulletIndex)
	{
		if (BulletPreviewMeshes[BulletIndex] && !BulletPreviewMaterials[BulletIndex] && BulletPreviewTintMaterial)
		{
			BulletPreviewMaterials[BulletIndex] = UMaterialInstanceDynamic::Create(BulletPreviewTintMaterial, this);
			BulletPreviewMeshes[BulletIndex]->SetMaterial(0, BulletPreviewMaterials[BulletIndex]);
		}
		if (BulletPreviewOutlineMeshes.IsValidIndex(BulletIndex)
			&& BulletPreviewOutlineMeshes[BulletIndex]
			&& !BulletPreviewOutlineMaterials[BulletIndex]
			&& BulletPreviewTintMaterial)
		{
			BulletPreviewOutlineMaterials[BulletIndex] = UMaterialInstanceDynamic::Create(BulletPreviewTintMaterial, this);
			BulletPreviewOutlineMeshes[BulletIndex]->SetMaterial(0, BulletPreviewOutlineMaterials[BulletIndex]);
		}
	}
}

void ASDBetActionPanelActor::RefreshVisuals()
{
	// AActor::bHidden is replicated. This panel has per-local-player visibility,
	// so keep its shared container visible and hide only the local visual
	// components; otherwise a listen server can hide a participant's bullets.
	SetActorHiddenInGame(false);
	EnsureButtons();
	EnsureBulletPreview();
	RefreshSelectedRaiseTarget();

	LastResolvedLocalPlayerSlot = ResolveLocalPlayerSlot();
	const bool bPanelVisible = IsPanelVisibleForLocalPlayer();
	RefreshBulletPreview(bPanelVisible);

	for (ESDBetActionPanelButtonKind ButtonKind : ButtonKinds)
	{
		const int32 ButtonIndex = GetButtonIndex(ButtonKind);
		if (!ButtonActors.IsValidIndex(ButtonIndex) || !ButtonActors[ButtonIndex])
		{
			continue;
		}

		const bool bEnabled = CanPressButton(ButtonKind);
		const int32 StaggerIndex = bPanelVisible
			? ButtonIndex
			: ButtonCount - 1 - ButtonIndex;
		const float AnimationDelay = static_cast<float>(StaggerIndex)
			* FMath::Clamp(PanelState.ButtonAnimationStaggerDelay, 0.0f, 0.25f);
		ButtonActors[ButtonIndex]->SetButtonState(
			BuildButtonLabel(ButtonKind),
			GetButtonColor(ButtonKind, bEnabled),
			GetButtonSize(ButtonKind),
			bPanelVisible,
			bEnabled,
			BuildButtonTransform(ButtonKind),
			AnimationDelay,
			FMath::Clamp(PanelState.ButtonAnimationDuration, 0.05f, 1.0f),
			FMath::Clamp(PanelState.ButtonBounceStrength, 0.0f, 0.5f));
	}

	RefreshTickState();
}

void ASDBetActionPanelActor::RefreshBulletPreview(bool bVisible)
{
	const float LayoutScale = FMath::Max(0.1f, PanelState.PanelVisualScale);
	const int32 LoadedCount = FMath::Clamp(PanelState.LoadedBulletCount, 0, 6);
	const int32 RaiseTarget = FMath::Clamp(SelectedRaiseTarget, LoadedCount, 6);
	bool bHasVisibleBulletPresentation = bBulletTargetVisible;
	for (const float VisualAlpha : BulletVisualAlphas)
	{
		bHasVisibleBulletPresentation = bHasVisibleBulletPresentation || VisualAlpha > KINDA_SMALL_NUMBER;
	}
	const bool bPreserveVisiblePresentation = !bVisible && bHasVisibleBulletPresentation;
	if (bPreserveVisiblePresentation)
	{
		if (bVisible != bBulletTargetVisible)
		{
			StartBulletVisibilityAnimation(bVisible);
		}
		ApplyBulletAnimatedVisuals();
		return;
	}

	if (bVisible || !bHasCachedBulletPanelTransform)
	{
		CachedBulletPanelLocation = PanelState.WorldLocation;
		CachedBulletPanelRotation = PanelState.WorldRotation;
		bHasCachedBulletPanelTransform = true;
	}

	const FRotator PanelRotation = CachedBulletPanelRotation;
	const FVector ForwardDirection = FRotationMatrix(PanelRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(PanelRotation).GetUnitAxis(EAxis::Y);
	const FVector BulletRowWorldOffset = PanelRotation.RotateVector(PanelState.BulletRowOffset * LayoutScale);
	const FRotator DotRotation = FQuat::FindBetweenNormals(FVector::UpVector, ForwardDirection).Rotator();

	for (int32 BulletIndex = 0; BulletIndex < BulletPreviewMeshes.Num(); ++BulletIndex)
	{
		UStaticMeshComponent* BulletMesh = BulletPreviewMeshes[BulletIndex];
		if (!BulletMesh)
		{
			continue;
		}

		const bool bAlreadyLoaded = BulletIndex < LoadedCount;
		const bool bPendingRaise = BulletIndex >= LoadedCount && BulletIndex < RaiseTarget;
		const FLinearColor BulletColor = bAlreadyLoaded
			? FLinearColor(0.76f, 0.46f, 0.14f, 1.0f)
			: (bPendingRaise
				? FLinearColor(0.94f, 0.29f, 0.055f, 1.0f)
				: FLinearColor(0.085f, 0.10f, 0.135f, 1.0f));

		// The panel faces the viewer from the opposite side of its local right
		// axis, so reverse the visual slot index to fill left-to-right on screen.
		const int32 VisualSlotIndex = BulletPreviewMeshes.Num() - 1 - BulletIndex;
		const FVector DotLocation = CachedBulletPanelLocation
			+ BulletRowWorldOffset
			+ RightDirection * ((static_cast<float>(VisualSlotIndex) - 2.5f) * PanelState.BulletSpacing * LayoutScale);
		BulletMesh->SetWorldLocationAndRotation(DotLocation + ForwardDirection * 0.16f, DotRotation);
		if (BulletPreviewOutlineMeshes.IsValidIndex(BulletIndex) && BulletPreviewOutlineMeshes[BulletIndex])
		{
			BulletPreviewOutlineMeshes[BulletIndex]->SetWorldLocationAndRotation(DotLocation, DotRotation);
		}
		if (BulletPreviewMaterials.IsValidIndex(BulletIndex) && BulletPreviewMaterials[BulletIndex])
		{
			BulletMesh->SetMaterial(0, BulletPreviewMaterials[BulletIndex]);
			BulletPreviewMaterials[BulletIndex]->SetVectorParameterValue(TEXT("Color"), BulletColor);
			BulletPreviewMaterials[BulletIndex]->SetVectorParameterValue(TEXT("BaseColor"), BulletColor);
			BulletPreviewMaterials[BulletIndex]->SetScalarParameterValue(TEXT("Opacity"), BulletColor.A);
		}
		if (BulletPreviewOutlineMaterials.IsValidIndex(BulletIndex) && BulletPreviewOutlineMaterials[BulletIndex])
		{
			const FLinearColor OutlineColor(0.012f, 0.016f, 0.024f, 1.0f);
			BulletPreviewOutlineMeshes[BulletIndex]->SetMaterial(0, BulletPreviewOutlineMaterials[BulletIndex]);
			BulletPreviewOutlineMaterials[BulletIndex]->SetVectorParameterValue(TEXT("Color"), OutlineColor);
			BulletPreviewOutlineMaterials[BulletIndex]->SetVectorParameterValue(TEXT("BaseColor"), OutlineColor);
		}
	}

	if (bVisible != bBulletTargetVisible)
	{
		StartBulletVisibilityAnimation(bVisible);
	}
	ApplyBulletAnimatedVisuals();
}

void ASDBetActionPanelActor::StartBulletVisibilityAnimation(bool bVisible)
{
	bBulletTargetVisible = bVisible;
	bBulletVisibilityTransitionActive = BulletPreviewMeshes.Num() > 0;
	const float StaggerDelay = FMath::Clamp(PanelState.BulletRevealStaggerDelay, 0.0f, 0.25f);

	for (int32 BulletIndex = 0; BulletIndex < BulletPreviewMeshes.Num(); ++BulletIndex)
	{
		if (!BulletVisualAlphas.IsValidIndex(BulletIndex)
			|| !BulletVisualScales.IsValidIndex(BulletIndex)
			|| !BulletTransitionStartAlphas.IsValidIndex(BulletIndex)
			|| !BulletTransitionStartScales.IsValidIndex(BulletIndex)
			|| !BulletTransitionElapsedTimes.IsValidIndex(BulletIndex))
		{
			continue;
		}

		const int32 DelayIndex = bVisible
			? BulletIndex
			: BulletPreviewMeshes.Num() - 1 - BulletIndex;
		BulletTransitionStartAlphas[BulletIndex] = BulletVisualAlphas[BulletIndex];
		BulletTransitionStartScales[BulletIndex] = BulletVisualScales[BulletIndex];
		BulletTransitionElapsedTimes[BulletIndex] = -static_cast<float>(DelayIndex) * StaggerDelay;
	}

	RefreshTickState();
}

void ASDBetActionPanelActor::TriggerBulletChangeAnimation(int32 PreviousTarget, int32 NewTarget)
{
	const int32 FirstChangedBullet = FMath::Min(PreviousTarget, NewTarget);
	const int32 LastChangedBullet = FMath::Max(PreviousTarget, NewTarget);
	for (int32 BulletIndex = FirstChangedBullet; BulletIndex < LastChangedBullet; ++BulletIndex)
	{
		if (BulletPulseElapsedTimes.IsValidIndex(BulletIndex))
		{
			BulletPulseElapsedTimes[BulletIndex] = 0.0f;
		}
	}

	ApplyBulletAnimatedVisuals();
	RefreshTickState();
}

void ASDBetActionPanelActor::UpdateBulletAnimations(float DeltaSeconds)
{
	const float Duration = FMath::Clamp(PanelState.BulletAnimationDuration, 0.05f, 1.0f);
	const float BounceStrength = FMath::Clamp(PanelState.BulletBounceStrength, 0.0f, 0.5f);
	bool bAnyVisibilityTransitionActive = false;

	if (bBulletVisibilityTransitionActive)
	{
		for (int32 BulletIndex = 0; BulletIndex < BulletPreviewMeshes.Num(); ++BulletIndex)
		{
			if (!BulletVisualAlphas.IsValidIndex(BulletIndex)
				|| !BulletVisualScales.IsValidIndex(BulletIndex)
				|| !BulletTransitionStartAlphas.IsValidIndex(BulletIndex)
				|| !BulletTransitionStartScales.IsValidIndex(BulletIndex)
				|| !BulletTransitionElapsedTimes.IsValidIndex(BulletIndex))
			{
				continue;
			}

			BulletTransitionElapsedTimes[BulletIndex] += DeltaSeconds;
			if (BulletTransitionElapsedTimes[BulletIndex] < 0.0f)
			{
				bAnyVisibilityTransitionActive = true;
				continue;
			}

			const float Progress = FMath::Clamp(BulletTransitionElapsedTimes[BulletIndex] / Duration, 0.0f, 1.0f);
			const float TargetAlpha = bBulletTargetVisible ? 1.0f : 0.0f;
			const float TargetScale = TargetAlpha;
			const float SmoothProgress = Smooth01(Progress);
			BulletVisualAlphas[BulletIndex] = FMath::Lerp(
				BulletTransitionStartAlphas[BulletIndex],
				TargetAlpha,
				SmoothProgress);

			const bool bFullReveal = bBulletTargetVisible
				&& BulletTransitionStartAlphas[BulletIndex] <= KINDA_SMALL_NUMBER;
			const bool bFullHide = !bBulletTargetVisible
				&& BulletTransitionStartAlphas[BulletIndex] >= 1.0f - KINDA_SMALL_NUMBER;
			if (bFullReveal)
			{
				BulletVisualScales[BulletIndex] = EvaluatePopInScale(Progress, BounceStrength);
			}
			else if (bFullHide)
			{
				BulletVisualScales[BulletIndex] = EvaluatePopOutScale(Progress, BounceStrength);
			}
			else
			{
				BulletVisualScales[BulletIndex] = FMath::Lerp(
					BulletTransitionStartScales[BulletIndex],
					TargetScale,
					SmoothProgress);
			}

			if (Progress < 1.0f)
			{
				bAnyVisibilityTransitionActive = true;
			}
			else
			{
				BulletVisualAlphas[BulletIndex] = TargetAlpha;
				BulletVisualScales[BulletIndex] = TargetScale;
			}
		}
	}
	bBulletVisibilityTransitionActive = bAnyVisibilityTransitionActive;

	for (float& PulseElapsedTime : BulletPulseElapsedTimes)
	{
		if (PulseElapsedTime < 0.0f)
		{
			continue;
		}
		PulseElapsedTime += DeltaSeconds;
		if (PulseElapsedTime >= Duration)
		{
			PulseElapsedTime = -1.0f;
		}
	}

	ApplyBulletAnimatedVisuals();
}

void ASDBetActionPanelActor::ApplyBulletAnimatedVisuals()
{
	const float LayoutScale = FMath::Max(0.1f, PanelState.PanelVisualScale);
	// The old value was authored for the long bulletBetting mesh. Scale the
	// cylinder into a compact, thin disc that fits inside the existing spacing.
	const float BaseBulletScale = FMath::Max(0.001f, PanelState.BulletPreviewScale)
		* (LayoutScale / 0.35f)
		* 0.30f;
	const float Duration = FMath::Clamp(PanelState.BulletAnimationDuration, 0.05f, 1.0f);
	const float BounceStrength = FMath::Clamp(PanelState.BulletBounceStrength, 0.0f, 0.5f);

	for (int32 BulletIndex = 0; BulletIndex < BulletPreviewMeshes.Num(); ++BulletIndex)
	{
		UStaticMeshComponent* BulletMesh = BulletPreviewMeshes[BulletIndex];
		if (!BulletMesh || !BulletVisualAlphas.IsValidIndex(BulletIndex) || !BulletVisualScales.IsValidIndex(BulletIndex))
		{
			continue;
		}

		float PulseScale = 1.0f;
		if (BulletPulseElapsedTimes.IsValidIndex(BulletIndex) && BulletPulseElapsedTimes[BulletIndex] >= 0.0f)
		{
			PulseScale = EvaluateBulletPulseScale(BulletPulseElapsedTimes[BulletIndex] / Duration, BounceStrength);
		}

		const float AnimatedScale = FMath::Max(0.0f, BulletVisualScales[BulletIndex]) * PulseScale;
		const bool bDrawVisible = BulletVisualAlphas[BulletIndex] > KINDA_SMALL_NUMBER
			|| AnimatedScale > KINDA_SMALL_NUMBER;
		const float OutlineRadiusScale = BaseBulletScale * AnimatedScale;
		const float FillRadiusScale = OutlineRadiusScale * 0.72f;
		BulletMesh->SetWorldScale3D(FVector(FillRadiusScale, FillRadiusScale, FillRadiusScale * 0.11f));
		BulletMesh->SetVisibility(bDrawVisible, true);
		if (BulletPreviewOutlineMeshes.IsValidIndex(BulletIndex) && BulletPreviewOutlineMeshes[BulletIndex])
		{
			BulletPreviewOutlineMeshes[BulletIndex]->SetWorldScale3D(FVector(
				OutlineRadiusScale,
				OutlineRadiusScale,
				OutlineRadiusScale * 0.09f));
			BulletPreviewOutlineMeshes[BulletIndex]->SetVisibility(bDrawVisible, true);
		}
	}
}

bool ASDBetActionPanelActor::HasActiveBulletAnimation() const
{
	if (bBulletVisibilityTransitionActive)
	{
		return true;
	}
	for (const float PulseElapsedTime : BulletPulseElapsedTimes)
	{
		if (PulseElapsedTime >= 0.0f)
		{
			return true;
		}
	}
	return false;
}

void ASDBetActionPanelActor::RefreshTickState()
{
	const bool bNeedsSmoothTick = HasActiveBulletAnimation();
	SetActorTickInterval(bNeedsSmoothTick ? 0.0f : 0.15f);
	SetActorTickEnabled(bNeedsSmoothTick || PanelState.bVisible);
}

void ASDBetActionPanelActor::RefreshSelectedRaiseTarget()
{
	const int32 ClampedTarget = ClampRaiseTarget(SelectedRaiseTarget);
	const bool bStateChanged =
		LastSeenRevision != PanelState.Revision
		|| LastSeenTurnSlot != PanelState.TurnSlot
		|| LastSeenTableBet != PanelState.TableBet
		|| LastSeenCurrentPlayerBet != PanelState.CurrentPlayerBet;

	if (bStateChanged || ClampedTarget != SelectedRaiseTarget)
	{
		SelectedRaiseTarget = PanelState.SelectedRaiseTarget > 0
			? ClampRaiseTarget(PanelState.SelectedRaiseTarget)
			: GetDefaultRaiseTarget();
	}

	LastSeenRevision = PanelState.Revision;
	LastSeenTurnSlot = PanelState.TurnSlot;
	LastSeenTableBet = PanelState.TableBet;
	LastSeenCurrentPlayerBet = PanelState.CurrentPlayerBet;
}

void ASDBetActionPanelActor::AdjustSelectedRaiseTarget(int32 Delta)
{
	const int32 PreviousRaiseTarget = SelectedRaiseTarget;
	SelectedRaiseTarget = ClampRaiseTarget(SelectedRaiseTarget + Delta);
	RefreshVisuals();
	if (SelectedRaiseTarget != PreviousRaiseTarget)
	{
		TriggerBulletChangeAnimation(PreviousRaiseTarget, SelectedRaiseTarget);
	}
	if (PanelState.bMultiplayer)
	{
		if (AShowDownPlayerController* PlayerController = Cast<AShowDownPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
		{
			PlayerController->RequestRaisePreviewTarget(SelectedRaiseTarget);
		}
	}
}

int32 ASDBetActionPanelActor::GetDefaultRaiseTarget() const
{
	const int32 MinimumUsefulRaise = FMath::Max(PanelState.TableBet + 1, PanelState.CurrentPlayerBet + 1);
	return ClampRaiseTarget(FMath::Max(PanelState.MinRaiseTarget, MinimumUsefulRaise));
}

int32 ASDBetActionPanelActor::ClampRaiseTarget(int32 TargetBet) const
{
	const int32 MinTarget = FMath::Clamp(PanelState.MinRaiseTarget, 1, 6);
	const int32 MaxTarget = FMath::Clamp(FMath::Max(PanelState.MaxRaiseTarget, MinTarget), MinTarget, 6);
	return FMath::Clamp(TargetBet, MinTarget, MaxTarget);
}

EShowDownPlayerSlot ASDBetActionPanelActor::ResolveLocalPlayerSlot() const
{
	const APlayerController* LocalPlayerController = UGameplayStatics::GetPlayerController(this, 0);
	const ASDPlayerState* LocalPlayerState = LocalPlayerController
		? LocalPlayerController->GetPlayerState<ASDPlayerState>()
		: nullptr;

	if (PanelState.bMultiplayer)
	{
		return LocalPlayerState ? LocalPlayerState->ShowDownSlot : EShowDownPlayerSlot::None;
	}

	return EShowDownPlayerSlot::Player1;
}

bool ASDBetActionPanelActor::IsPanelVisibleForLocalPlayer() const
{
	return PanelState.bVisible
		&& PanelState.TurnSlot != EShowDownPlayerSlot::None
		&& ResolveLocalPlayerSlot() == PanelState.TurnSlot;
}

bool ASDBetActionPanelActor::CanPressButton(ESDBetActionPanelButtonKind ButtonKind) const
{
	if (!IsPanelVisibleForLocalPlayer() || ResolveLocalPlayerSlot() != PanelState.TurnSlot)
	{
		return false;
	}

	switch (ButtonKind)
	{
	case ESDBetActionPanelButtonKind::Primary:
		return true;
	case ESDBetActionPanelButtonKind::RaiseDown:
		return PanelState.bCanRaise && SelectedRaiseTarget > ClampRaiseTarget(PanelState.MinRaiseTarget);
	case ESDBetActionPanelButtonKind::RaiseSubmit:
		return PanelState.bCanRaise && SelectedRaiseTarget > PanelState.TableBet;
	case ESDBetActionPanelButtonKind::RaiseUp:
		return PanelState.bCanRaise && SelectedRaiseTarget < ClampRaiseTarget(PanelState.MaxRaiseTarget);
	case ESDBetActionPanelButtonKind::Fold:
		return PanelState.bCanFold;
	default:
		return false;
	}
}

void ASDBetActionPanelActor::HandleButtonClicked(ESDBetActionPanelButtonKind ButtonKind, AActor* Interactor)
{
	if (!CanPressButton(ButtonKind))
	{
		return;
	}

	if (ButtonKind == ESDBetActionPanelButtonKind::RaiseDown)
	{
		AdjustSelectedRaiseTarget(-1);
		return;
	}
	if (ButtonKind == ESDBetActionPanelButtonKind::RaiseUp)
	{
		AdjustSelectedRaiseTarget(1);
		return;
	}

	AShowDownPlayerController* PlayerController = Cast<AShowDownPlayerController>(Interactor);
	if (!PlayerController)
	{
		PlayerController = Cast<AShowDownPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	}
	if (!PlayerController)
	{
		return;
	}

	switch (ButtonKind)
	{
	case ESDBetActionPanelButtonKind::Primary:
		PlayerController->RequestPlayerCheck();
		break;
	case ESDBetActionPanelButtonKind::RaiseSubmit:
		PlayerController->RequestPlayerRaiseTo(SelectedRaiseTarget);
		break;
	case ESDBetActionPanelButtonKind::Fold:
		PlayerController->RequestPlayerFold();
		break;
	default:
		break;
	}
}

FTransform ASDBetActionPanelActor::BuildButtonTransform(ESDBetActionPanelButtonKind ButtonKind) const
{
	const float LayoutScale = FMath::Max(0.1f, PanelState.PanelVisualScale);
	const FRotator PanelRotation = PanelState.WorldRotation;
	const FVector RightDirection = FRotationMatrix(PanelRotation).GetUnitAxis(EAxis::Y);
	const FVector2D PrimarySize = GetButtonSize(ESDBetActionPanelButtonKind::Primary);
	const FVector2D FoldSize = GetButtonSize(ESDBetActionPanelButtonKind::Fold);
	const FVector2D StepSize = GetButtonSize(ESDBetActionPanelButtonKind::RaiseDown);
	const FVector2D RaiseSize = GetButtonSize(ESDBetActionPanelButtonKind::RaiseSubmit);
	const float RowGap = 6.0f * LayoutScale;
	const float RaiseTotalWidth = StepSize.X + RowGap + RaiseSize.X + RowGap + StepSize.X;
	const float ActionGap = FMath::Max(6.0f * LayoutScale, RaiseTotalWidth - FoldSize.X - PrimarySize.X);
	const float RaiseStepOffset = (RaiseSize.X * 0.5f) + RowGap + (StepSize.X * 0.5f);

	float RightOffset = 0.0f;
	float HeightOffset = PanelState.TopRowHeight;

	switch (ButtonKind)
	{
	case ESDBetActionPanelButtonKind::Primary:
		RightOffset = (FoldSize.X + ActionGap) * 0.5f;
		HeightOffset = PanelState.BottomRowHeight;
		break;
	case ESDBetActionPanelButtonKind::Fold:
		RightOffset = -((PrimarySize.X + ActionGap) * 0.5f);
		HeightOffset = PanelState.BottomRowHeight;
		break;
	case ESDBetActionPanelButtonKind::RaiseDown:
		RightOffset = RaiseStepOffset;
		break;
	case ESDBetActionPanelButtonKind::RaiseSubmit:
		RightOffset = 0.0f;
		break;
	case ESDBetActionPanelButtonKind::RaiseUp:
		RightOffset = -RaiseStepOffset;
		break;
	default:
		break;
	}

	const FVector ButtonLocation =
		PanelState.WorldLocation
		+ RightDirection * RightOffset
		+ FVector::UpVector * HeightOffset * LayoutScale;
	return FTransform(PanelRotation, ButtonLocation);
}

FVector2D ASDBetActionPanelActor::GetButtonSize(ESDBetActionPanelButtonKind ButtonKind) const
{
	const float LayoutScale = FMath::Max(0.1f, PanelState.PanelVisualScale);
	switch (ButtonKind)
	{
	case ESDBetActionPanelButtonKind::Primary:
		return FVector2D(PanelState.PrimaryButtonWidth, PanelState.ButtonHeight) * LayoutScale;
	case ESDBetActionPanelButtonKind::RaiseDown:
	case ESDBetActionPanelButtonKind::RaiseUp:
		return FVector2D(PanelState.StepButtonWidth, PanelState.ButtonHeight) * LayoutScale;
	case ESDBetActionPanelButtonKind::RaiseSubmit:
		return FVector2D(PanelState.RaiseButtonWidth, PanelState.ButtonHeight) * LayoutScale;
	case ESDBetActionPanelButtonKind::Fold:
		return FVector2D(PanelState.FoldButtonWidth, PanelState.ButtonHeight) * LayoutScale;
	default:
		return FVector2D(PanelState.PrimaryButtonWidth, PanelState.ButtonHeight) * LayoutScale;
	}
}

FString ASDBetActionPanelActor::BuildButtonLabel(ESDBetActionPanelButtonKind ButtonKind) const
{
	switch (ButtonKind)
	{
	case ESDBetActionPanelButtonKind::Primary:
		return PanelState.CurrentPlayerBet < PanelState.TableBet
			? FString::Printf(TEXT("CALL %d"), PanelState.TableBet)
			: FString(TEXT("CHECK"));
	case ESDBetActionPanelButtonKind::RaiseDown:
		return TEXT("-");
	case ESDBetActionPanelButtonKind::RaiseSubmit:
		return FString::Printf(TEXT("RAISE TO %d"), SelectedRaiseTarget);
	case ESDBetActionPanelButtonKind::RaiseUp:
		return TEXT("+");
	case ESDBetActionPanelButtonKind::Fold:
		return TEXT("FOLD");
	default:
		return FString();
	}
}

FLinearColor ASDBetActionPanelActor::GetButtonColor(
	ESDBetActionPanelButtonKind ButtonKind,
	bool bEnabled) const
{
	FLinearColor Color = FLinearColor::White;
	switch (ButtonKind)
	{
	case ESDBetActionPanelButtonKind::Primary:
		Color = FLinearColor(0.02f, 0.78f, 0.22f, 1.0f);
		break;
	case ESDBetActionPanelButtonKind::RaiseDown:
	case ESDBetActionPanelButtonKind::RaiseSubmit:
	case ESDBetActionPanelButtonKind::RaiseUp:
		Color = FLinearColor(0.95f, 0.38f, 0.02f, 1.0f);
		break;
	case ESDBetActionPanelButtonKind::Fold:
		Color = FLinearColor(0.86f, 0.02f, 0.04f, 1.0f);
		break;
	default:
		break;
	}

	return bEnabled ? Color : DimColor(Color);
}

int32 ASDBetActionPanelActor::GetButtonIndex(ESDBetActionPanelButtonKind ButtonKind) const
{
	switch (ButtonKind)
	{
	case ESDBetActionPanelButtonKind::Primary:
		return 0;
	case ESDBetActionPanelButtonKind::RaiseDown:
		return 1;
	case ESDBetActionPanelButtonKind::RaiseSubmit:
		return 2;
	case ESDBetActionPanelButtonKind::RaiseUp:
		return 3;
	case ESDBetActionPanelButtonKind::Fold:
		return 4;
	default:
		return INDEX_NONE;
	}
}

void ASDBetActionPanelActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASDBetActionPanelActor, PanelState);
}

ASDBetActionButtonActor::ASDBetActionButtonActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	SetReplicateMovement(false);
	SetActorTickEnabled(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	ClickBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("ClickBounds"));
	ClickBounds->SetupAttachment(Root);
	ClickBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ClickBounds->SetCollisionObjectType(ECC_WorldDynamic);
	ClickBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	ClickBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	ClickBounds->SetCanEverAffectNavigation(false);

	ShadowMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShadowMesh"));
	ShadowMesh->SetupAttachment(Root);
	ShadowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShadowMesh->SetCanEverAffectNavigation(false);
	ShadowMesh->SetCastShadow(false);

	RimMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RimMesh"));
	RimMesh->SetupAttachment(Root);
	RimMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RimMesh->SetCanEverAffectNavigation(false);
	RimMesh->SetCastShadow(false);

	BackplateMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackplateMesh"));
	BackplateMesh->SetupAttachment(Root);
	BackplateMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BackplateMesh->SetCanEverAffectNavigation(false);
	BackplateMesh->SetCastShadow(false);

	HighlightMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HighlightMesh"));
	HighlightMesh->SetupAttachment(Root);
	HighlightMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HighlightMesh->SetCanEverAffectNavigation(false);
	HighlightMesh->SetCastShadow(false);

	LabelShadowText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("LabelShadowText"));
	LabelShadowText->SetupAttachment(Root);
	LabelShadowText->SetHorizontalAlignment(EHTA_Center);
	LabelShadowText->SetVerticalAlignment(EVRTA_TextCenter);
	LabelShadowText->SetTextRenderColor(FColor::Black);
	LabelShadowText->SetWorldSize(13.0f);
	LabelShadowText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LabelShadowText->SetCanEverAffectNavigation(false);

	LabelText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("LabelText"));
	LabelText->SetupAttachment(Root);
	LabelText->SetHorizontalAlignment(EHTA_Center);
	LabelText->SetVerticalAlignment(EVRTA_TextCenter);
	LabelText->SetTextRenderColor(FColor::White);
	LabelText->SetWorldSize(13.0f);
	LabelText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LabelText->SetCanEverAffectNavigation(false);
	LabelText->SetRelativeLocation(FVector(3.5f, 0.0f, 0.0f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		BackplateMeshAsset = CubeMeshFinder.Object;
		ShadowMesh->SetStaticMesh(BackplateMeshAsset);
		RimMesh->SetStaticMesh(BackplateMeshAsset);
		BackplateMesh->SetStaticMesh(BackplateMeshAsset);
		HighlightMesh->SetStaticMesh(BackplateMeshAsset);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (MaterialFinder.Succeeded())
	{
		ShadowMesh->SetMaterial(0, MaterialFinder.Object);
		RimMesh->SetMaterial(0, MaterialFinder.Object);
		BackplateMesh->SetMaterial(0, MaterialFinder.Object);
		HighlightMesh->SetMaterial(0, MaterialFinder.Object);
	}

	SetActorHiddenInGame(true);
}

void ASDBetActionButtonActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bVisibilityTransitionActive)
	{
		VisibilityTransitionElapsed += DeltaSeconds;
		if (VisibilityTransitionElapsed >= 0.0f)
		{
			const float Progress = FMath::Clamp(
				VisibilityTransitionElapsed / FMath::Max(0.05f, VisibilityTransitionDuration),
				0.0f,
				1.0f);
			const float TargetAlpha = bTargetVisible ? 1.0f : 0.0f;
			const float SmoothProgress = Smooth01(Progress);
			VisualAlpha = FMath::Lerp(VisibilityTransitionStartAlpha, TargetAlpha, SmoothProgress);

			const bool bFullReveal = bTargetVisible && VisibilityTransitionStartAlpha <= KINDA_SMALL_NUMBER;
			const bool bFullHide = !bTargetVisible && VisibilityTransitionStartAlpha >= 1.0f - KINDA_SMALL_NUMBER;
			if (bFullReveal)
			{
				VisualScale = EvaluatePopInScale(Progress, VisibilityBounceStrength);
			}
			else if (bFullHide)
			{
				VisualScale = EvaluatePopOutScale(Progress, VisibilityBounceStrength);
			}
			else
			{
				VisualScale = FMath::Lerp(VisibilityTransitionStartScale, TargetAlpha, SmoothProgress);
			}

			if (Progress >= 1.0f)
			{
				VisualAlpha = TargetAlpha;
				VisualScale = TargetAlpha;
				bVisibilityTransitionActive = false;
			}
		}
	}

	const float TargetPressAlpha = bPointerPressed ? 1.0f : 0.0f;
	PressVisualAlpha = FMath::FInterpTo(PressVisualAlpha, TargetPressAlpha, DeltaSeconds, 18.0f);
	if (FMath::IsNearlyEqual(PressVisualAlpha, TargetPressAlpha, 0.01f))
	{
		PressVisualAlpha = TargetPressAlpha;
	}
	ApplyAnimatedVisuals();

	const bool bPressAnimationSettled = FMath::IsNearlyEqual(PressVisualAlpha, TargetPressAlpha, 0.01f);
	if (!bVisibilityTransitionActive && bPressAnimationSettled)
	{
		SetActorTickEnabled(false);
	}
}

void ASDBetActionButtonActor::InitializeButton(
	ASDBetActionPanelActor* InOwnerPanel,
	ESDBetActionPanelButtonKind InButtonKind)
{
	OwnerPanel = InOwnerPanel;
	ButtonKind = InButtonKind;
}

void ASDBetActionButtonActor::BeginPointerPress()
{
	if (!bButtonEnabled)
	{
		return;
	}

	bPointerPressed = true;
	SetActorTickEnabled(true);
	ApplyAnimatedVisuals();
}

void ASDBetActionButtonActor::CancelPointerPress()
{
	bPointerPressed = false;
	SetActorTickEnabled(true);
	ApplyAnimatedVisuals();
}

void ASDBetActionButtonActor::ReleasePointerPress(AActor* Interactor, bool bCommit)
{
	bPointerPressed = false;
	SetActorTickEnabled(true);
	ApplyAnimatedVisuals();

	if (bCommit && OwnerPanel)
	{
		OwnerPanel->HandleButtonClicked(ButtonKind, Interactor);
	}
}

void ASDBetActionButtonActor::SetButtonState(
	const FString& Label,
	const FLinearColor& Color,
	const FVector2D& Size,
	bool bVisible,
	bool bEnabled,
	const FTransform& WorldTransform,
	float AnimationDelay,
	float AnimationDuration,
	float BounceStrength)
{
	const bool bPreserveVisiblePresentation = !bVisible
		&& (bTargetVisible || VisualAlpha > KINDA_SMALL_NUMBER || VisualScale > KINDA_SMALL_NUMBER);
	if (bVisible || VisualAlpha <= KINDA_SMALL_NUMBER)
	{
		SetActorTransform(WorldTransform);
	}

	const bool bVisibilityChanged = bVisible != bTargetVisible;
	if (bVisible && bVisibilityChanged)
	{
		SetActorHiddenInGame(false);
	}

	VisibilityTransitionDuration = FMath::Clamp(AnimationDuration, 0.05f, 1.0f);
	VisibilityBounceStrength = FMath::Clamp(BounceStrength, 0.0f, 0.5f);
	if (bVisibilityChanged)
	{
		VisibilityTransitionStartAlpha = VisualAlpha;
		VisibilityTransitionStartScale = VisualScale;
		VisibilityTransitionElapsed = -FMath::Clamp(AnimationDelay, 0.0f, 1.0f);
		bVisibilityTransitionActive = true;
	}
	bTargetVisible = bVisible;
	bButtonEnabled = bVisible && bEnabled;
	if (!bButtonEnabled)
	{
		bPointerPressed = false;
	}
	if (bVisibilityTransitionActive)
	{
		SetActorTickEnabled(true);
	}
	if (bPreserveVisiblePresentation)
	{
		ApplyAnimatedVisuals();
		return;
	}

	if (ClickBounds)
	{
		ClickBounds->SetBoxExtent(FVector(
			FMath::Max(0.75f, Size.Y * 0.25f),
			FMath::Max(1.0f, Size.X * 0.5f),
			FMath::Max(1.0f, Size.Y * 0.5f)));
	}

	CurrentButtonWidth = FMath::Max(1.0f, Size.X);
	CurrentButtonHeight = FMath::Max(1.0f, Size.Y);
	CurrentHighlightHeight = FMath::Clamp(CurrentButtonHeight * 0.16f, 0.32f, 0.75f);
	const float RimThickness = FMath::Clamp(CurrentButtonHeight * 0.16f, 0.34f, 0.82f);
	const float InnerWidth = FMath::Max(1.0f, CurrentButtonWidth - RimThickness * 2.0f);
	const float InnerHeight = FMath::Max(1.0f, CurrentButtonHeight - RimThickness * 2.0f);
	const float PlateDepth = FMath::Clamp(CurrentButtonHeight * 0.20f, 0.62f, 1.15f);
	const float RimDepth = PlateDepth + 0.18f;
	const float ShadowDepth = FMath::Clamp(CurrentButtonHeight * 0.11f, 0.35f, 0.7f);

	if (ShadowMesh)
	{
		ShadowMesh->SetRelativeScale3D(FVector(
			ShadowDepth / 100.0f,
			(CurrentButtonWidth + CurrentButtonHeight * 0.35f) / 100.0f,
			(CurrentButtonHeight + CurrentButtonHeight * 0.25f) / 100.0f));
	}

	if (RimMesh)
	{
		RimMesh->SetRelativeScale3D(FVector(
			RimDepth / 100.0f,
			CurrentButtonWidth / 100.0f,
			CurrentButtonHeight / 100.0f));
	}

	if (BackplateMesh)
	{
		CurrentPressDepth = FMath::Clamp(CurrentButtonHeight * 0.28f, 0.58f, 1.55f);
		CurrentLabelDepth = FMath::Max(0.78f, CurrentPressDepth + 0.45f);
		BackplateMesh->SetRelativeScale3D(FVector(
			PlateDepth / 100.0f,
			InnerWidth / 100.0f,
			InnerHeight / 100.0f));
	}

	if (HighlightMesh)
	{
		HighlightMesh->SetRelativeScale3D(FVector(
			FMath::Max(0.08f, PlateDepth * 0.16f) / 100.0f,
			InnerWidth * 0.84f / 100.0f,
			CurrentHighlightHeight / 100.0f));
	}

	ApplyBackplateColor(Color);

	if (LabelText)
	{
		LabelText->SetVisibility(bVisible);
		LabelText->SetText(FText::FromString(Label));
		CurrentLabelColor = bEnabled
			? FLinearColor(1.0f, 0.96f, 0.84f, 1.0f)
			: FLinearColor(0.62f, 0.58f, 0.5f, 1.0f);
		LabelText->SetWorldSize(Label.Len() <= 2
			? FMath::Clamp(CurrentButtonHeight * 0.78f, 1.8f, 8.0f)
			: FMath::Clamp(CurrentButtonHeight * 0.50f, 1.7f, 5.8f));
	}

	if (LabelShadowText)
	{
		LabelShadowText->SetVisibility(bVisible);
		LabelShadowText->SetText(FText::FromString(Label));
		LabelShadowText->SetWorldSize(Label.Len() <= 2
			? FMath::Clamp(CurrentButtonHeight * 0.78f, 1.8f, 8.0f)
			: FMath::Clamp(CurrentButtonHeight * 0.50f, 1.7f, 5.8f));
	}

	ApplyAnimatedVisuals();
}

bool ASDBetActionButtonActor::CanInteract_Implementation(AActor* Interactor) const
{
	return bButtonEnabled
		&& OwnerPanel
		&& OwnerPanel->CanPressButton(ButtonKind);
}

void ASDBetActionButtonActor::Interact_Implementation(AActor* Interactor)
{
	if (OwnerPanel)
	{
		ReleasePointerPress(Interactor, true);
	}
}

void ASDBetActionButtonActor::ApplyAnimatedVisuals()
{
	const bool bDrawVisible = bTargetVisible
		|| VisualAlpha > KINDA_SMALL_NUMBER
		|| VisualScale > KINDA_SMALL_NUMBER;
	SetActorHiddenInGame(!bDrawVisible);

	const float Alpha = FMath::Clamp(VisualAlpha, 0.0f, 1.0f);
	const float PressAlpha = FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp(PressVisualAlpha, 0.0f, 1.0f));
	const float PressOffsetX = -CurrentPressDepth * PressAlpha;
	const float RevealOffsetZ = 0.0f;
	const float PressScale = FMath::Lerp(1.0f, 0.95f, PressAlpha);
	SetActorScale3D(FVector(FMath::Max(0.0f, VisualScale) * PressScale));

	if (ClickBounds)
	{
		ClickBounds->SetCollisionEnabled(
			bButtonEnabled && VisualAlpha > 0.85f
				? ECollisionEnabled::QueryOnly
				: ECollisionEnabled::NoCollision);
	}

	auto ApplyMaterialColor = [](UMaterialInstanceDynamic* MaterialInstance, const FLinearColor& Color)
	{
		if (!MaterialInstance)
		{
			return;
		}

		FLinearColor OpaqueColor = Color;
		OpaqueColor.A = 1.0f;
		MaterialInstance->SetVectorParameterValue(TEXT("Color"), OpaqueColor);
		MaterialInstance->SetVectorParameterValue(TEXT("BaseColor"), OpaqueColor);
	};

	if (ShadowMesh)
	{
		ShadowMesh->SetVisibility(bDrawVisible);
		ShadowMesh->SetRelativeLocation(FVector(-0.48f + PressOffsetX * 0.16f, 0.18f, RevealOffsetZ - 0.18f));
		const FLinearColor ShadowFadeColor(0.0f, 0.0f, 0.0f, 1.0f);
		const FLinearColor ShadowColor = FMath::Lerp(ShadowFadeColor, CurrentShadowColor, Alpha);
		ApplyMaterialColor(ShadowMaterialInstance, ShadowColor);
	}

	if (RimMesh)
	{
		RimMesh->SetVisibility(bDrawVisible);
		RimMesh->SetRelativeLocation(FVector(PressOffsetX * 0.82f - 0.04f, 0.0f, RevealOffsetZ));
		const FLinearColor FadeColor(
			CurrentRimColor.R * 0.08f,
			CurrentRimColor.G * 0.08f,
			CurrentRimColor.B * 0.08f,
			1.0f);
		FLinearColor RimColor = FMath::Lerp(FadeColor, CurrentRimColor, Alpha);
		const FLinearColor PressedRimColor = FMath::Lerp(CurrentRimColor, FLinearColor::White, 0.18f);
		RimColor = FMath::Lerp(RimColor, PressedRimColor, PressAlpha);
		ApplyMaterialColor(RimMaterialInstance, RimColor);
	}

	if (BackplateMesh)
	{
		BackplateMesh->SetVisibility(bDrawVisible);
		BackplateMesh->SetRelativeLocation(FVector(PressOffsetX, 0.0f, RevealOffsetZ));
		if (BackplateMaterialInstance)
		{
			const FLinearColor FadeColor(
				CurrentBackplateColor.R * 0.08f,
				CurrentBackplateColor.G * 0.08f,
				CurrentBackplateColor.B * 0.08f,
				1.0f);
			FLinearColor AnimatedColor = FMath::Lerp(FadeColor, CurrentBackplateColor, Alpha);
			const FLinearColor PressedColor(
				CurrentBackplateColor.R * 0.58f,
				CurrentBackplateColor.G * 0.58f,
				CurrentBackplateColor.B * 0.58f,
				1.0f);
			AnimatedColor = FMath::Lerp(AnimatedColor, PressedColor, PressAlpha);
			AnimatedColor.A = 1.0f;
			BackplateMaterialInstance->SetVectorParameterValue(TEXT("Color"), AnimatedColor);
			BackplateMaterialInstance->SetVectorParameterValue(TEXT("BaseColor"), AnimatedColor);
		}
	}
	if (HighlightMesh)
	{
		HighlightMesh->SetVisibility(bDrawVisible);
		HighlightMesh->SetRelativeLocation(FVector(
			PressOffsetX + 0.48f,
			0.0f,
			RevealOffsetZ + CurrentButtonHeight * 0.5f - CurrentHighlightHeight * 0.78f));
		const FLinearColor FadeColor(
			CurrentHighlightColor.R * 0.08f,
			CurrentHighlightColor.G * 0.08f,
			CurrentHighlightColor.B * 0.08f,
			1.0f);
		FLinearColor HighlightColor = FMath::Lerp(FadeColor, CurrentHighlightColor, Alpha);
		HighlightColor = FMath::Lerp(HighlightColor, CurrentHighlightColor * 0.72f, PressAlpha);
		HighlightColor.A = 1.0f;
		ApplyMaterialColor(HighlightMaterialInstance, HighlightColor);
	}
	if (LabelShadowText)
	{
		LabelShadowText->SetVisibility(bDrawVisible);
		LabelShadowText->SetRelativeLocation(FVector(CurrentLabelDepth - 0.08f + PressOffsetX, 0.16f, RevealOffsetZ - 0.16f));
		const FLinearColor ShadowTextColor = FMath::Lerp(
			FLinearColor(0.0f, 0.0f, 0.0f, 1.0f),
			FLinearColor(0.03f, 0.025f, 0.018f, 1.0f),
			Alpha);
		LabelShadowText->SetTextRenderColor(ToTextColor(ShadowTextColor));
	}
	if (LabelText)
	{
		LabelText->SetVisibility(bDrawVisible);
		LabelText->SetRelativeLocation(FVector(CurrentLabelDepth + PressOffsetX, 0.0f, RevealOffsetZ));
		const FLinearColor FadeTextColor(
			CurrentLabelColor.R * 0.12f,
			CurrentLabelColor.G * 0.12f,
			CurrentLabelColor.B * 0.12f,
			1.0f);
		FLinearColor TextColor = FMath::Lerp(FadeTextColor, CurrentLabelColor, Alpha);
		TextColor = FMath::Lerp(TextColor, CurrentLabelColor * 0.82f, PressAlpha);
		TextColor.A = 1.0f;
		LabelText->SetTextRenderColor(ToTextColor(TextColor));
	}
}

void ASDBetActionButtonActor::ApplyBackplateColor(const FLinearColor& Color)
{
	if (!ShadowMaterialInstance && ShadowMesh)
	{
		ShadowMaterialInstance = ShadowMesh->CreateAndSetMaterialInstanceDynamic(0);
	}
	if (!RimMaterialInstance && RimMesh)
	{
		RimMaterialInstance = RimMesh->CreateAndSetMaterialInstanceDynamic(0);
	}
	if (!BackplateMaterialInstance && BackplateMesh)
	{
		BackplateMaterialInstance = BackplateMesh->CreateAndSetMaterialInstanceDynamic(0);
	}
	if (!HighlightMaterialInstance && HighlightMesh)
	{
		HighlightMaterialInstance = HighlightMesh->CreateAndSetMaterialInstanceDynamic(0);
	}

	const FLinearColor PlateColor = FMath::Lerp(Color, FLinearColor::Black, 0.12f);
	CurrentBackplateColor = FLinearColor(PlateColor.R, PlateColor.G, PlateColor.B, 1.0f);
	const FLinearColor RimColor = FMath::Lerp(Color, FLinearColor::White, 0.24f);
	CurrentRimColor = FLinearColor(RimColor.R, RimColor.G, RimColor.B, 1.0f);
	const FLinearColor HighlightColor = FMath::Lerp(Color, FLinearColor::White, 0.42f);
	CurrentHighlightColor = FLinearColor(HighlightColor.R, HighlightColor.G, HighlightColor.B, 1.0f);
	CurrentShadowColor = FLinearColor(
		FMath::Clamp(Color.R * 0.045f, 0.0f, 0.08f),
		FMath::Clamp(Color.G * 0.045f, 0.0f, 0.08f),
		FMath::Clamp(Color.B * 0.045f, 0.0f, 0.08f),
		1.0f);

	const FLinearColor FadeColor(
		CurrentBackplateColor.R * 0.08f,
		CurrentBackplateColor.G * 0.08f,
		CurrentBackplateColor.B * 0.08f,
		1.0f);
	FLinearColor AnimatedColor = FMath::Lerp(FadeColor, CurrentBackplateColor, FMath::Clamp(VisualAlpha, 0.0f, 1.0f));
	AnimatedColor.A = 1.0f;

	auto ApplyMaterialColor = [](UMaterialInstanceDynamic* MaterialInstance, const FLinearColor& MaterialColor)
	{
		if (!MaterialInstance)
		{
			return;
		}

		FLinearColor OpaqueColor = MaterialColor;
		OpaqueColor.A = 1.0f;
		MaterialInstance->SetVectorParameterValue(TEXT("Color"), OpaqueColor);
		MaterialInstance->SetVectorParameterValue(TEXT("BaseColor"), OpaqueColor);
	};

	ApplyMaterialColor(ShadowMaterialInstance, CurrentShadowColor);
	ApplyMaterialColor(RimMaterialInstance, CurrentRimColor);
	ApplyMaterialColor(BackplateMaterialInstance, AnimatedColor);
	ApplyMaterialColor(HighlightMaterialInstance, CurrentHighlightColor);
}
