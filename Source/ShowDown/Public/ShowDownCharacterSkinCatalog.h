#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ShowDownCharacterSkinCatalog.generated.h"

class UAnimationAsset;
class UAnimInstance;
class USkeletalMesh;
class UTexture2D;

/** Presentation rarity authored locally for character previews and UI styling. */
UENUM(BlueprintType)
enum class EShowDownCharacterSkinRarity : uint8
{
	Unspecified UMETA(Hidden),
	Common UMETA(DisplayName = "Common"),
	Rare UMETA(DisplayName = "Rare"),
	Epic UMETA(DisplayName = "Epic"),
	Legendary UMETA(DisplayName = "Legendary")
};

/** Selects which editor-placed presentation actor is requesting animation data. */
UENUM(BlueprintType)
enum class EShowDownCharacterPreviewContext : uint8
{
	Shop UMETA(DisplayName = "Shop"),
	MainMenu UMETA(DisplayName = "Main Menu")
};

/** Selects how a skin is animated by a lightweight character preview actor. */
UENUM(BlueprintType)
enum class EShowDownShopPreviewAnimationMode : uint8
{
	InheritBuiltIn UMETA(DisplayName = "Inherit Built-In"),
	ReferencePose UMETA(DisplayName = "Reference Pose"),
	SingleAnimation UMETA(DisplayName = "Single Animation"),
	AnimationBlueprint UMETA(DisplayName = "Animation Blueprint")
};

/** A screen-specific animation profile for one character skin. */
USTRUCT(BlueprintType)
struct SHOWDOWN_API FShowDownCharacterPreviewAnimationProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Preview")
	EShowDownShopPreviewAnimationMode AnimationMode =
		EShowDownShopPreviewAnimationMode::InheritBuiltIn;

	/** Animation blueprint used when Animation Mode is Animation Blueprint. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "ShowDown|Character Preview",
		meta = (
			EditCondition = "AnimationMode == EShowDownShopPreviewAnimationMode::AnimationBlueprint",
			EditConditionHides))
	TSoftClassPtr<UAnimInstance> AnimClass;

	/** Single animation sequence, composite, or blend space used by the preview. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "ShowDown|Character Preview",
		meta = (
			EditCondition = "AnimationMode == EShowDownShopPreviewAnimationMode::SingleAnimation",
			EditConditionHides))
	TSoftObjectPtr<UAnimationAsset> Animation;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "ShowDown|Character Preview",
		meta = (
			EditCondition = "AnimationMode == EShowDownShopPreviewAnimationMode::SingleAnimation",
			EditConditionHides))
	bool bLoop = true;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "ShowDown|Character Preview",
		meta = (
			ClampMin = "0.0",
			UIMin = "0.0",
			UIMax = "3.0",
			EditCondition = "AnimationMode == EShowDownShopPreviewAnimationMode::SingleAnimation",
			EditConditionHides))
	float PlayRate = 1.0f;
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Skin")
	EShowDownCharacterSkinRarity Rarity = EShowDownCharacterSkinRarity::Unspecified;

	/** Optional local thumbnail for future list/grid layouts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Skin")
	TSoftObjectPtr<UTexture2D> Thumbnail;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Skin")
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	/** Animation shown while this skin is selected in the shop. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Skin|Preview")
	FShowDownCharacterPreviewAnimationProfile ShopPreview;

	/** Animation shown for the equipped skin on the main menu. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Skin|Preview")
	FShowDownCharacterPreviewAnimationProfile MainMenuPreview;

	/** Per-skin mesh correction relative to the preview actor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Skin|Preview|Transform")
	FVector PreviewLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Skin|Preview|Transform")
	FRotator PreviewRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "ShowDown|Character Skin|Preview|Transform",
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

	/** Loads the project-wide catalog used by gameplay replication validation. */
	static UShowDownCharacterSkinCatalog* LoadDefaultCatalog();

	/**
	 * Canonicalizes and validates an id against the supplied catalog plus the
	 * built-in skins. Unknown or unsafe replicated values resolve to robot.
	 */
	static FString NormalizeKnownSkinId(
		const UShowDownCharacterSkinCatalog* Catalog,
		const FString& SkinId);

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

	/** Returns catalog entries in editor order, then appends missing built-ins. */
	static void GetOrderedSkinDefinitions(
		const UShowDownCharacterSkinCatalog* Catalog,
		TArray<FShowDownCharacterSkinDefinition>& OutDefinitions);

	static const FShowDownCharacterPreviewAnimationProfile& GetPreviewProfile(
		const FShowDownCharacterSkinDefinition& Definition,
		EShowDownCharacterPreviewContext Context);

	UFUNCTION(BlueprintPure, Category = "ShowDown|Character Skin")
	static FText GetRarityDisplayName(EShowDownCharacterSkinRarity Rarity);

	UFUNCTION(BlueprintPure, Category = "ShowDown|Character Skin")
	static FLinearColor GetRarityColor(EShowDownCharacterSkinRarity Rarity);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Skin", meta = (TitleProperty = "SkinId"))
	TArray<FShowDownCharacterSkinDefinition> Skins;
};
