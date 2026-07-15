#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ShowDownCharacterSkinCatalog.generated.h"

class UAnimationAsset;
class UAnimInstance;
class USkeletalMesh;
class UTexture2D;

/** Selects how a skin is animated by the shop-only preview actor. */
UENUM(BlueprintType)
enum class EShowDownShopPreviewAnimationMode : uint8
{
	InheritBuiltIn UMETA(DisplayName = "Inherit Built-In"),
	ReferencePose UMETA(DisplayName = "Reference Pose"),
	SingleAnimation UMETA(DisplayName = "Single Animation"),
	AnimationBlueprint UMETA(DisplayName = "Animation Blueprint")
};

/**
 * Local presentation data for a character cosmetic. Skin ids are the stable
 * values persisted by the backend and replicated over the network; asset
 * references remain local to the game build.
 */
USTRUCT(BlueprintType)
struct SHOWDOWN_API FShowDownCharacterSkinDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Skin")
	FString SkinId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Skin")
	FText DisplayName;

	/** Optional local flavor text for the shop UI. This is not backend product data. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Skin")
	FText Description;

	/** Optional local thumbnail for future list/grid layouts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Skin")
	TSoftObjectPtr<UTexture2D> Thumbnail;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Skin")
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Skin|Shop Preview")
	EShowDownShopPreviewAnimationMode PreviewAnimationMode =
		EShowDownShopPreviewAnimationMode::InheritBuiltIn;

	/** Animation blueprint used when Preview Animation Mode is Animation Blueprint. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "ShowDown|Character Skin|Shop Preview",
		meta = (
			EditCondition = "PreviewAnimationMode == EShowDownShopPreviewAnimationMode::AnimationBlueprint",
			EditConditionHides))
	TSoftClassPtr<UAnimInstance> PreviewAnimClass;

	/** Single animation sequence, composite, or blend space used by the preview. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "ShowDown|Character Skin|Shop Preview",
		meta = (
			EditCondition = "PreviewAnimationMode == EShowDownShopPreviewAnimationMode::SingleAnimation",
			EditConditionHides))
	TSoftObjectPtr<UAnimationAsset> PreviewAnimation;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "ShowDown|Character Skin|Shop Preview",
		meta = (
			EditCondition = "PreviewAnimationMode == EShowDownShopPreviewAnimationMode::SingleAnimation",
			EditConditionHides))
	bool bLoopPreviewAnimation = true;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "ShowDown|Character Skin|Shop Preview",
		meta = (
			ClampMin = "0.0",
			UIMin = "0.0",
			UIMax = "3.0",
			EditCondition = "PreviewAnimationMode == EShowDownShopPreviewAnimationMode::SingleAnimation",
			EditConditionHides))
	float PreviewAnimationPlayRate = 1.0f;

	/** Per-skin mesh correction relative to the preview actor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Skin|Shop Preview|Transform")
	FVector PreviewLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Skin|Shop Preview|Transform")
	FRotator PreviewRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "ShowDown|Character Skin|Shop Preview|Transform",
		meta = (ClampMin = "0.01"))
	FVector PreviewScale = FVector::OneVector;
};

/**
 * Optional editor-authored skin catalog. The four shipping characters are
 * also registered in code so the game works without creating or assigning a
 * catalog asset. Entries in an assigned catalog override the supplied fields
 * for matching built-ins and can add future skins without changing C++ code.
 */
UCLASS(BlueprintType)
class SHOWDOWN_API UShowDownCharacterSkinCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "ShowDown|Character Skin")
	static FString GetDefaultSkinId();

	static FString CanonicalizeSkinId(const FString& SkinId);

	UFUNCTION(BlueprintPure, Category = "ShowDown|Character Skin")
	bool FindSkinDefinition(
		const FString& RequestedSkinId,
		FShowDownCharacterSkinDefinition& OutDefinition) const;

	static bool FindBuiltInSkinDefinition(
		const FString& RequestedSkinId,
		FShowDownCharacterSkinDefinition& OutDefinition);

	static bool ResolveSkinDefinition(
		const UShowDownCharacterSkinCatalog* Catalog,
		const FString& RequestedSkinId,
		FShowDownCharacterSkinDefinition& OutDefinition,
		FString& OutResolvedSkinId);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Skin", meta = (TitleProperty = "SkinId"))
	TArray<FShowDownCharacterSkinDefinition> Skins;
};
