#include "ShowDownShopPreviewActor.h"

#include "Animation/AnimationAsset.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "ShowDownCharacterSkinCatalog.h"

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
}

void AShowDownShopPreviewActor::SetSkinCatalog(
	UShowDownCharacterSkinCatalog* InSkinCatalog)
{
	SkinCatalog = InSkinCatalog;
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
		SetActorHiddenInGame(true);
		return false;
	}

	USkeletalMesh* LoadedMesh = Definition.SkeletalMesh.LoadSynchronous();
	if (!LoadedMesh)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Shop preview could not load mesh for skin '%s'."),
			*NewResolvedSkinId);
		SetActorHiddenInGame(true);
		return false;
	}

	PreviewMesh->SetSkeletalMesh(LoadedMesh);
	PreviewMesh->SetRelativeLocation(Definition.PreviewLocationOffset);
	PreviewMesh->SetRelativeRotation(
		BasePreviewMeshRotation + Definition.PreviewRotationOffset);
	PreviewMesh->SetRelativeScale3D(FVector(
		FMath::Max(0.01f, Definition.PreviewScale.X),
		FMath::Max(0.01f, Definition.PreviewScale.Y),
		FMath::Max(0.01f, Definition.PreviewScale.Z)));

	// Clear the previous skin's animation first so switching between an Anim BP
	// and a single-node animation cannot leave a stale instance behind.
	PreviewMesh->Stop();
	PreviewMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	PreviewMesh->SetAnimInstanceClass(nullptr);

	bool bAppliedAnimation = false;
	if (Definition.PreviewAnimationMode
		== EShowDownShopPreviewAnimationMode::AnimationBlueprint)
	{
		if (!Definition.PreviewAnimClass.IsNull())
		{
			if (UClass* LoadedAnimClass = Definition.PreviewAnimClass.LoadSynchronous())
			{
				PreviewMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
				PreviewMesh->SetAnimInstanceClass(LoadedAnimClass);
				bAppliedAnimation = true;
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
	else if (Definition.PreviewAnimationMode
		== EShowDownShopPreviewAnimationMode::SingleAnimation)
	{
		if (!Definition.PreviewAnimation.IsNull())
		{
			if (UAnimationAsset* LoadedAnimation = Definition.PreviewAnimation.LoadSynchronous())
			{
				PreviewMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
				PreviewMesh->SetAnimation(LoadedAnimation);
				PreviewMesh->SetPlayRate(FMath::Max(0.0f, Definition.PreviewAnimationPlayRate));
				PreviewMesh->Play(Definition.bLoopPreviewAnimation);
				bAppliedAnimation = true;
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
	SetActorHiddenInGame(false);
	return true;
}
