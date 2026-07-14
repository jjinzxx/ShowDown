#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ShowDownCharacterSkinCatalog.generated.h"

class USkeletalMesh;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Skin")
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;
};

/**
 * Optional editor-authored skin catalog. The four shipping characters are
 * also registered in code so the game works without creating or assigning a
 * catalog asset. Entries in an assigned catalog override built-ins that use
 * the same id and can add future skins without changing the character class.
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Skin")
	TArray<FShowDownCharacterSkinDefinition> Skins;
};
