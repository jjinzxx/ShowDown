#include "ShowDownShopPreviewActor.h"

#include "Animation/AnimationAsset.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/Skeleton.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "ShowDownCharacterSkinCatalog.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FRotator BasePreviewMeshRotation(0.0f, -90.0f, 0.0f);
}

AShowDownShopPreviewActor::AShowDownShopPreviewActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetCanBeDamaged(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PreviewMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PreviewMesh"));
	PreviewMesh->SetupAttachment(SceneRoot);
	PreviewMesh->SetRelativeRotation(BasePreviewMeshRotation);
	PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewMesh->SetGenerateOverlapEvents(false);
	PreviewMesh->SetReceivesDecals(false);
	PreviewMesh->SetUpdateAnimationInEditor(true);
	PreviewMesh->SetComponentTickEnabled(false);
	PreviewMesh->bPauseAnims = true;
	SetActorHiddenInGame(true);

	// Dragging this actor into a hub level is enough to get the project-wide
	// catalog for editor preview. The Hub still supplies the same asset again at
	// runtime, so no actor or camera is spawned or repositioned automatically.
	static ConstructorHelpers::FObjectFinder<UShowDownCharacterSkinCatalog>
		DefaultCharacterSkinCatalog(TEXT("/Game/Data/Characters/DA_CharacterSkinCatalog"));
	if (DefaultCharacterSkinCatalog.Succeeded())
	{
		SkinCatalog = DefaultCharacterSkinCatalog.Object;
	}
}

void AShowDownShopPreviewActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DeactivatePreview();
	Super::EndPlay(EndPlayReason);
}

void AShowDownShopPreviewActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

#if WITH_EDITOR
	if (const UWorld* World = GetWorld();
		World && !World->IsGameWorld() && PreviewMesh && !PreviewMesh->GetSkeletalMeshAsset())
	{
		RefreshEditorPreview();
	}
#endif
}

#if WITH_EDITOR
void AShowDownShopPreviewActor::PostEditChangeProperty(
	FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName ChangedPropertyName = PropertyChangedEvent.GetPropertyName();
	if (ChangedPropertyName == GET_MEMBER_NAME_CHECKED(
			AShowDownShopPreviewActor,
			EditorPreviewSkinId)
		|| ChangedPropertyName == GET_MEMBER_NAME_CHECKED(
			AShowDownShopPreviewActor,
			PreviewContext)
		|| ChangedPropertyName == GET_MEMBER_NAME_CHECKED(
			AShowDownShopPreviewActor,
			SkinCatalog))
	{
		RefreshEditorPreview();
	}
}
#endif

void AShowDownShopPreviewActor::SetSkinCatalog(
	UShowDownCharacterSkinCatalog* InSkinCatalog)
{
	SkinCatalog = InSkinCatalog;
	if (bPreviewActive && !ResolvedSkinId.IsEmpty())
	{
		SetPreviewSkin(ResolvedSkinId);
	}
}

void AShowDownShopPreviewActor::SetPreviewContext(
	const EShowDownCharacterPreviewContext InPreviewContext)
{
	if (PreviewContext == InPreviewContext)
	{
		return;
	}

	PreviewContext = InPreviewContext;
	if (bPreviewActive && !ResolvedSkinId.IsEmpty())
	{
		SetPreviewSkin(ResolvedSkinId);
	}
}

bool AShowDownShopPreviewActor::SetPreviewSkin(const FString& RequestedSkinId)
{
	FShowDownCharacterSkinDefinition Definition;
	FString NewResolvedSkinId;
	if (!UShowDownCharacterSkinCatalog::ResolveSkinDefinition(
		SkinCatalog,
		RequestedSkinId,
		Definition,
		NewResolvedSkinId))
	{
		return false;
	}

	const FString CanonicalRequestedSkinId =
		UShowDownCharacterSkinCatalog::CanonicalizeSkinId(RequestedSkinId);
	bool bUsingFallbackPresentation = !RequestedSkinId.TrimStartAndEnd().IsEmpty()
		&& CanonicalRequestedSkinId != NewResolvedSkinId;
	USkeletalMesh* LoadedMesh = Definition.SkeletalMesh.LoadSynchronous();
	if (!LoadedMesh)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Shop preview could not load mesh for skin '%s'."),
			*NewResolvedSkinId);

		const FString DefaultSkinId = UShowDownCharacterSkinCatalog::GetDefaultSkinId();
		if (NewResolvedSkinId != DefaultSkinId
			&& UShowDownCharacterSkinCatalog::ResolveSkinDefinition(
				SkinCatalog,
				DefaultSkinId,
				Definition,
				NewResolvedSkinId))
		{
			LoadedMesh = Definition.SkeletalMesh.LoadSynchronous();
			bUsingFallbackPresentation = LoadedMesh != nullptr;
		}

		if (!LoadedMesh)
		{
			return false;
		}
	}

	PreviewMesh->SetVisibility(true, true);
	PreviewMesh->bPauseAnims = false;
	PreviewMesh->SetSkeletalMesh(LoadedMesh);
	PreviewMesh->SetRelativeLocation(Definition.PreviewLocationOffset);
	PreviewMesh->SetRelativeRotation(
		BasePreviewMeshRotation + Definition.PreviewRotationOffset);
	auto SanitizeScaleAxis = [](const double Value)
	{
		return FMath::IsFinite(Value) ? FMath::Max(0.01, Value) : 1.0;
	};
	PreviewMesh->SetRelativeScale3D(FVector(
		SanitizeScaleAxis(Definition.PreviewScale.X),
		SanitizeScaleAxis(Definition.PreviewScale.Y),
		SanitizeScaleAxis(Definition.PreviewScale.Z)));

	// Clear the previous skin's animation first so switching between contexts,
	// an Anim BP, and a single-node animation cannot leave stale state behind.
	ResetAnimationState();
	const FShowDownCharacterPreviewAnimationProfile& AnimationProfile =
		UShowDownCharacterSkinCatalog::GetPreviewProfile(Definition, PreviewContext);

	bool bAppliedAnimation = false;
	if (!bUsingFallbackPresentation
		&& AnimationProfile.AnimationMode
		== EShowDownShopPreviewAnimationMode::AnimationBlueprint)
	{
		if (!AnimationProfile.AnimClass.IsNull())
		{
			if (UClass* LoadedAnimClass = AnimationProfile.AnimClass.LoadSynchronous())
			{
				const IAnimClassInterface* AnimClassInterface =
					IAnimClassInterface::GetFromClass(LoadedAnimClass);
				const USkeleton* TargetSkeleton = AnimClassInterface
					? AnimClassInterface->GetTargetSkeleton()
					: nullptr;
				if (TargetSkeleton && TargetSkeleton->IsCompatibleMesh(LoadedMesh, false))
				{
					PreviewMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
					PreviewMesh->SetAnimInstanceClass(LoadedAnimClass);
					bAppliedAnimation = true;
				}
				else
				{
					UE_LOG(
						LogTemp,
						Warning,
						TEXT("Character preview animation blueprint '%s' is incompatible with skin '%s'."),
						*LoadedAnimClass->GetPathName(),
						*NewResolvedSkinId);
				}
			}
		}

		if (!bAppliedAnimation)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("Shop preview has no valid Anim BP for skin '%s'."),
				*NewResolvedSkinId);
		}
	}
	else if (!bUsingFallbackPresentation
		&& AnimationProfile.AnimationMode
		== EShowDownShopPreviewAnimationMode::SingleAnimation)
	{
		if (!AnimationProfile.Animation.IsNull())
		{
			if (UAnimationAsset* LoadedAnimation = AnimationProfile.Animation.LoadSynchronous())
			{
				const USkeleton* AnimationSkeleton = LoadedAnimation->GetSkeleton();
				if (AnimationSkeleton
					&& AnimationSkeleton->IsCompatibleMesh(LoadedMesh, false))
				{
					PreviewMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
					PreviewMesh->SetAnimation(LoadedAnimation);
					const float SafePlayRate = FMath::IsFinite(AnimationProfile.PlayRate)
						? FMath::Max(0.0f, AnimationProfile.PlayRate)
						: 1.0f;
					PreviewMesh->SetPlayRate(SafePlayRate);
					PreviewMesh->Play(AnimationProfile.bLoop);
					bAppliedAnimation = true;
				}
				else
				{
					UE_LOG(
						LogTemp,
						Warning,
						TEXT("Character preview animation '%s' is incompatible with skin '%s'."),
						*LoadedAnimation->GetPathName(),
						*NewResolvedSkinId);
				}
			}
		}

		if (!bAppliedAnimation)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("Shop preview has no valid single animation for skin '%s'."),
				*NewResolvedSkinId);
		}
	}

	if (!bAppliedAnimation)
	{
		// An intentionally empty definition is valid and displays the mesh's
		// reference pose instead of borrowing another skin's animation blueprint.
		PreviewMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		PreviewMesh->SetAnimation(nullptr);
	}

	ResolvedSkinId = NewResolvedSkinId;
	if (const UWorld* World = GetWorld(); !World || !World->IsGameWorld() || bPreviewActive)
	{
		SetActorHiddenInGame(false);
	}
	return true;
}

bool AShowDownShopPreviewActor::ActivatePreview(const FString& RequestedSkinId)
{
	bPreviewActive = true;
	PreviewMesh->SetComponentTickEnabled(true);
	PreviewMesh->bPauseAnims = false;
	PreviewMesh->SetVisibility(true, true);

	const bool bAppliedSkin = SetPreviewSkin(RequestedSkinId);
	SetActorHiddenInGame(!bAppliedSkin);
	if (!bAppliedSkin)
	{
		bPreviewActive = false;
		PreviewMesh->SetComponentTickEnabled(false);
		PreviewMesh->bPauseAnims = true;
	}
	return bAppliedSkin;
}

void AShowDownShopPreviewActor::DeactivatePreview()
{
	bPreviewActive = false;
	ResetAnimationState();
	if (PreviewMesh)
	{
		PreviewMesh->bPauseAnims = true;
		PreviewMesh->SetComponentTickEnabled(false);
		PreviewMesh->SetVisibility(false, true);
	}
	SetActorHiddenInGame(true);
}

void AShowDownShopPreviewActor::RefreshEditorPreview()
{
#if WITH_EDITOR
	const UWorld* World = GetWorld();
	if (World && World->IsGameWorld())
	{
		return;
	}

	if (PreviewMesh)
	{
		PreviewMesh->SetVisibility(true, true);
		PreviewMesh->SetComponentTickEnabled(true);
		PreviewMesh->bPauseAnims = false;
	}
	SetPreviewSkin(EditorPreviewSkinId);
#endif
}

void AShowDownShopPreviewActor::ResetAnimationState()
{
	if (!PreviewMesh)
	{
		return;
	}

	PreviewMesh->Stop();
	PreviewMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	PreviewMesh->SetAnimInstanceClass(nullptr);
}
