#include "ShowDownCharacterSkinCatalog.h"

#include "Animation/AnimationAsset.h"
#include "Engine/SkeletalMesh.h"

namespace
{
	const FString RobotSkinId(TEXT("robot"));
	const FString HoodmanSkinId(TEXT("hoodman"));
	const FString MicuSkinId(TEXT("micu"));
	const FString MikuSkinId(TEXT("miku"));

	FShowDownCharacterSkinDefinition MakeBuiltInSkinDefinition(
		const FString& SkinId,
		const FText& DisplayName,
		const TCHAR* SkeletalMeshPath,
		const TCHAR* PreviewAnimationPath,
		const bool bLoopPreviewAnimation = true)
	{
		FShowDownCharacterSkinDefinition Definition;
		Definition.SkinId = SkinId;
		Definition.DisplayName = DisplayName;
		Definition.SkeletalMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(SkeletalMeshPath));
		Definition.PreviewAnimationMode = EShowDownShopPreviewAnimationMode::SingleAnimation;
		Definition.PreviewAnimation = TSoftObjectPtr<UAnimationAsset>(FSoftObjectPath(PreviewAnimationPath));
		Definition.bLoopPreviewAnimation = bLoopPreviewAnimation;
		return Definition;
	}

	const TArray<FShowDownCharacterSkinDefinition>& GetBuiltInSkinDefinitions()
	{
		static const TArray<FShowDownCharacterSkinDefinition> Definitions =
		{
			MakeBuiltInSkinDefinition(
				RobotSkinId,
				NSLOCTEXT("ShowDownCharacterSkins", "Robot", "Robot"),
				TEXT("/Game/Character/Robot/robot.robot"),
				TEXT("/Game/Character/Animation/Idle_default_.Idle_default_")),
			MakeBuiltInSkinDefinition(
				HoodmanSkinId,
				NSLOCTEXT("ShowDownCharacterSkins", "Hoodman", "Hoodman"),
				TEXT("/Game/Character/hoodman_default_/hoodman.hoodman"),
				TEXT("/Game/Character/Animation/Dismissing_Gesture.Dismissing_Gesture"),
				false),
			MakeBuiltInSkinDefinition(
				MicuSkinId,
				NSLOCTEXT("ShowDownCharacterSkins", "Micu", "Micu"),
				TEXT("/Game/Character/micu/Tut_Hip_Hop_Dance__1_.Tut_Hip_Hop_Dance__1_"),
				TEXT("/Game/Character/micu/Tut_Hip_Hop_Dance__1__Anim.Tut_Hip_Hop_Dance__1__Anim")),
			MakeBuiltInSkinDefinition(
				MikuSkinId,
				NSLOCTEXT("ShowDownCharacterSkins", "Miku", "Miku"),
				TEXT("/Game/Character/miku/miku.miku"),
				TEXT("/Game/Character/Animation/Reacting.Reacting"),
				false)
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
	if (CanonicalId == TEXT("character_miku"))
	{
		return MikuSkinId;
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

	FShowDownCharacterSkinDefinition BuiltInDefinition;
	const bool bHasBuiltInDefinition = FindBuiltInSkinDefinition(
		CanonicalRequestedId,
		BuiltInDefinition);

	FShowDownCharacterSkinDefinition CatalogDefinition;
	if (Catalog && Catalog->FindSkinDefinition(CanonicalRequestedId, CatalogDefinition))
	{
		if (bHasBuiltInDefinition)
		{
			// Existing skins can override just their shop animation in an editor
			// catalog without having to repeat the built-in mesh and display name.
			OutDefinition = BuiltInDefinition;
			if (!CatalogDefinition.DisplayName.IsEmpty())
			{
				OutDefinition.DisplayName = CatalogDefinition.DisplayName;
			}
			if (!CatalogDefinition.Description.IsEmpty())
			{
				OutDefinition.Description = CatalogDefinition.Description;
			}
			if (!CatalogDefinition.Thumbnail.IsNull())
			{
				OutDefinition.Thumbnail = CatalogDefinition.Thumbnail;
			}
			if (!CatalogDefinition.SkeletalMesh.IsNull())
			{
				OutDefinition.SkeletalMesh = CatalogDefinition.SkeletalMesh;
			}

			if (CatalogDefinition.PreviewAnimationMode
				!= EShowDownShopPreviewAnimationMode::InheritBuiltIn)
			{
				OutDefinition.PreviewAnimationMode = CatalogDefinition.PreviewAnimationMode;
				OutDefinition.PreviewAnimClass = CatalogDefinition.PreviewAnimClass;
				OutDefinition.PreviewAnimation = CatalogDefinition.PreviewAnimation;
				OutDefinition.bLoopPreviewAnimation = CatalogDefinition.bLoopPreviewAnimation;
				OutDefinition.PreviewAnimationPlayRate =
					CatalogDefinition.PreviewAnimationPlayRate;
			}

			OutDefinition.PreviewLocationOffset = CatalogDefinition.PreviewLocationOffset;
			OutDefinition.PreviewRotationOffset = CatalogDefinition.PreviewRotationOffset;
			OutDefinition.PreviewScale = CatalogDefinition.PreviewScale;
		}
		else
		{
			OutDefinition = CatalogDefinition;
		}

		OutDefinition.SkinId = CanonicalRequestedId;
		OutResolvedSkinId = CanonicalRequestedId;
		return true;
	}

	if (bHasBuiltInDefinition)
	{
		OutDefinition = BuiltInDefinition;
		OutResolvedSkinId = CanonicalRequestedId;
		return true;
	}

	// Unknown ids are deliberately non-fatal. A stale backend selection or a
	// client with older content always resolves to the robot default.
	if (CanonicalRequestedId != RobotSkinId)
	{
		FString DefaultResolvedSkinId;
		if (ResolveSkinDefinition(
			Catalog,
			RobotSkinId,
			OutDefinition,
			DefaultResolvedSkinId))
		{
			OutResolvedSkinId = RobotSkinId;
			return true;
		}
	}

	OutDefinition = FShowDownCharacterSkinDefinition();
	OutResolvedSkinId = RobotSkinId;
	return false;
}
