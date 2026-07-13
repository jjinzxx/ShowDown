#include "ShowDownCharacterSkinCatalog.h"

#include "Engine/SkeletalMesh.h"

namespace
{
	const FString RobotSkinId(TEXT("robot"));
	const FString HoodmanSkinId(TEXT("hoodman"));
	const FString MicuSkinId(TEXT("micu"));

	FShowDownCharacterSkinDefinition MakeBuiltInSkinDefinition(
		const FString& SkinId,
		const FText& DisplayName,
		const TCHAR* SkeletalMeshPath)
	{
		FShowDownCharacterSkinDefinition Definition;
		Definition.SkinId = SkinId;
		Definition.DisplayName = DisplayName;
		Definition.SkeletalMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(SkeletalMeshPath));
		return Definition;
	}

	const TArray<FShowDownCharacterSkinDefinition>& GetBuiltInSkinDefinitions()
	{
		static const TArray<FShowDownCharacterSkinDefinition> Definitions =
		{
			MakeBuiltInSkinDefinition(
				RobotSkinId,
				NSLOCTEXT("ShowDownCharacterSkins", "Robot", "Robot"),
				TEXT("/Game/Character/Robot/robot.robot")),
			MakeBuiltInSkinDefinition(
				HoodmanSkinId,
				NSLOCTEXT("ShowDownCharacterSkins", "Hoodman", "Hoodman"),
				TEXT("/Game/Character/hoodman_default_/hoodman.hoodman")),
			MakeBuiltInSkinDefinition(
				MicuSkinId,
				NSLOCTEXT("ShowDownCharacterSkins", "Micu", "Micu"),
				TEXT("/Game/Character/micu/Tut_Hip_Hop_Dance__1_.Tut_Hip_Hop_Dance__1_"))
		};
		return Definitions;
	}
}

FString UShowDownCharacterSkinCatalog::GetDefaultSkinId()
{
	return RobotSkinId;
}

FString UShowDownCharacterSkinCatalog::CanonicalizeSkinId(const FString& SkinId)
{
	FString CanonicalId = SkinId;
	CanonicalId.TrimStartAndEndInline();
	CanonicalId.ToLowerInline();

	// Accept the older product-style aliases while keeping the replicated and
	// persisted runtime ids short and stable.
	if (CanonicalId == TEXT("character_robot"))
	{
		return RobotSkinId;
	}
	if (CanonicalId == TEXT("character_hoodman"))
	{
		return HoodmanSkinId;
	}
	if (CanonicalId == TEXT("character_micu"))
	{
		return MicuSkinId;
	}

	return CanonicalId;
}

bool UShowDownCharacterSkinCatalog::FindSkinDefinition(
	const FString& RequestedSkinId,
	FShowDownCharacterSkinDefinition& OutDefinition) const
{
	const FString CanonicalRequestedId = CanonicalizeSkinId(RequestedSkinId);
	if (CanonicalRequestedId.IsEmpty())
	{
		return false;
	}

	for (const FShowDownCharacterSkinDefinition& Definition : Skins)
	{
		if (!Definition.SkinId.TrimStartAndEnd().IsEmpty()
			&& CanonicalizeSkinId(Definition.SkinId) == CanonicalRequestedId)
		{
			OutDefinition = Definition;
			OutDefinition.SkinId = CanonicalRequestedId;
			return true;
		}
	}

	return false;
}

bool UShowDownCharacterSkinCatalog::FindBuiltInSkinDefinition(
	const FString& RequestedSkinId,
	FShowDownCharacterSkinDefinition& OutDefinition)
{
	FString CanonicalRequestedId = CanonicalizeSkinId(RequestedSkinId);
	if (CanonicalRequestedId.IsEmpty())
	{
		CanonicalRequestedId = RobotSkinId;
	}

	for (const FShowDownCharacterSkinDefinition& Definition : GetBuiltInSkinDefinitions())
	{
		if (Definition.SkinId == CanonicalRequestedId)
		{
			OutDefinition = Definition;
			return true;
		}
	}

	return false;
}

bool UShowDownCharacterSkinCatalog::ResolveSkinDefinition(
	const UShowDownCharacterSkinCatalog* Catalog,
	const FString& RequestedSkinId,
	FShowDownCharacterSkinDefinition& OutDefinition,
	FString& OutResolvedSkinId)
{
	FString CanonicalRequestedId = CanonicalizeSkinId(RequestedSkinId);
	if (CanonicalRequestedId.IsEmpty())
	{
		CanonicalRequestedId = RobotSkinId;
	}

	if ((Catalog && Catalog->FindSkinDefinition(CanonicalRequestedId, OutDefinition))
		|| FindBuiltInSkinDefinition(CanonicalRequestedId, OutDefinition))
	{
		OutResolvedSkinId = CanonicalizeSkinId(OutDefinition.SkinId);
		return true;
	}

	// Unknown ids are deliberately non-fatal. A stale backend selection or a
	// client with older content always resolves to the robot default.
	if ((Catalog && Catalog->FindSkinDefinition(RobotSkinId, OutDefinition))
		|| FindBuiltInSkinDefinition(RobotSkinId, OutDefinition))
	{
		OutResolvedSkinId = RobotSkinId;
		return true;
	}

	OutDefinition = FShowDownCharacterSkinDefinition();
	OutResolvedSkinId = RobotSkinId;
	return false;
}
