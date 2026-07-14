#include "ShowDownShopPreviewActor.h"

#include "Animation/AnimInstance.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "ShowDownCharacterSkinCatalog.h"
#include "UObject/ConstructorHelpers.h"

AShowDownShopPreviewActor::AShowDownShopPreviewActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetCanBeDamaged(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PreviewMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PreviewMesh"));
	PreviewMesh->SetupAttachment(SceneRoot);
	PreviewMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewMesh->SetGenerateOverlapEvents(false);
	PreviewMesh->SetReceivesDecals(false);

	static ConstructorHelpers::FClassFinder<UAnimInstance> PreviewAnimBlueprint(
		TEXT("/Game/Character/hoodman_default_/ABP_Hoodman"));
	if (PreviewAnimBlueprint.Succeeded())
	{
		DefaultPreviewAnimClass = PreviewAnimBlueprint.Class;
	}
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
	if (DefaultPreviewAnimClass)
	{
		PreviewMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		PreviewMesh->SetAnimInstanceClass(DefaultPreviewAnimClass);
	}

	ResolvedSkinId = NewResolvedSkinId;
	SetActorHiddenInGame(false);
	return true;
}
