#include "ShowDownCharacterSkinCatalog.h"

#include "Animation/AnimationAsset.h"
#include "Engine/SkeletalMesh.h"

namespace
{
	const FString RobotSkinId(TEXT("robot"));
	const FString HoodmanSkinId(TEXT("hoodman"));
	const FString GangmanSkinId(TEXT("gangman"));
	const FString MaskmanSkinId(TEXT("maskman"));
	const FString MicuSkinId(TEXT("micu"));
	const FString MikuSkinId(TEXT("miku"));
	const TCHAR* DefaultCatalogObjectPath =
		TEXT("/Game/Data/Characters/DA_CharacterSkinCatalog.DA_CharacterSkinCatalog");
	const TCHAR* MainMenuCapoeiraAnimationPath =
		TEXT("/Game/Character/Animation/Capoeira.Capoeira");
	constexpr int32 MaximumReplicatedSkinIdLength = 64;

	FShowDownCharacterPreviewAnimationProfile MakeSingleAnimationProfile(
		const TCHAR* AnimationPath,
		const bool bLoop = true,
		const float PlayRate = 1.0f)
	{
		FShowDownCharacterPreviewAnimationProfile Profile;
		Profile.AnimationMode = EShowDownShopPreviewAnimationMode::SingleAnimation;
		Profile.Animation = TSoftObjectPtr<UAnimationAsset>(FSoftObjectPath(AnimationPath));
		Profile.bLoop = bLoop;
		Profile.PlayRate = PlayRate;
		return Profile;
	}

	FShowDownCharacterSkinDefinition MakeBuiltInSkinDefinition(
		const FString& SkinId,
		const FText& DisplayName,
		const EShowDownCharacterSkinRarity Rarity,
		const TCHAR* SkeletalMeshPath,
		const TCHAR* PreviewAnimationPath,
		const bool bLoopPreviewAnimation = true)
	{
		FShowDownCharacterSkinDefinition Definition;
		Definition.SkinId = SkinId;
		Definition.DisplayName = DisplayName;
		Definition.Rarity = Rarity;
		Definition.SkeletalMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(SkeletalMeshPath));
		Definition.ShopPreview = MakeSingleAnimationProfile(
			PreviewAnimationPath,
			bLoopPreviewAnimation);
		Definition.MainMenuPreview = MakeSingleAnimationProfile(
			MainMenuCapoeiraAnimationPath);
		return Definition;
	}

	const TArray<FShowDownCharacterSkinDefinition>& GetBuiltInSkinDefinitions()
	{
		static const TArray<FShowDownCharacterSkinDefinition> Definitions =
		{
			MakeBuiltInSkinDefinition(
				RobotSkinId,
				NSLOCTEXT("ShowDownCharacterSkins", "Robot", "Robot"),
				EShowDownCharacterSkinRarity::Common,
				TEXT("/Game/Character/Robot/robot.robot"),
				TEXT("/Game/Data/Characters/Robot_Hip_Hop_Dance.Robot_Hip_Hop_Dance")),
			MakeBuiltInSkinDefinition(
				HoodmanSkinId,
				NSLOCTEXT("ShowDownCharacterSkins", "Hoodman", "Hoodman"),
				EShowDownCharacterSkinRarity::Rare,
				TEXT("/Game/Character/hoodman_default_/hoodman.hoodman"),
				TEXT("/Game/Data/Characters/Wave_Hip_Hop_Dance.Wave_Hip_Hop_Dance")),
			MakeBuiltInSkinDefinition(
				GangmanSkinId,
				NSLOCTEXT("ShowDownCharacterSkins", "Gangman", "Gangman"),
				EShowDownCharacterSkinRarity::Epic,
				TEXT("/Game/Character/gangman/gangman.gangman"),
				TEXT("/Game/Data/Characters/Gangnam_Style__1_.Gangnam_Style__1_")),
			MakeBuiltInSkinDefinition(
				MaskmanSkinId,
				NSLOCTEXT("ShowDownCharacterSkins", "Maskman", "Maskman"),
				EShowDownCharacterSkinRarity::Rare,
				TEXT("/Game/Character/maskman/maskman.maskman"),
				TEXT("/Game/Data/Characters/Locking_Hip_Hop_Dance.Locking_Hip_Hop_Dance")),
			MakeBuiltInSkinDefinition(
				MicuSkinId,
				NSLOCTEXT("ShowDownCharacterSkins", "Micu", "Micu"),
				EShowDownCharacterSkinRarity::Epic,
				TEXT("/Game/Character/micu/Tut_Hip_Hop_Dance__1_.Tut_Hip_Hop_Dance__1_"),
				TEXT("/Game/Character/micu/Tut_Hip_Hop_Dance__1__Anim.Tut_Hip_Hop_Dance__1__Anim")),
			MakeBuiltInSkinDefinition(
				MikuSkinId,
				NSLOCTEXT("ShowDownCharacterSkins", "Miku", "Miku"),
				EShowDownCharacterSkinRarity::Legendary,
				TEXT("/Game/Character/miku/miku.miku"),
				TEXT("/Game/Data/Characters/Tut_Hip_Hop_Dance.Tut_Hip_Hop_Dance"))
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
	if (CanonicalId.Len() > MaximumReplicatedSkinIdLength)
	{
		return FString();
	}
	for (const TCHAR Character : CanonicalId)
	{
		if (!FChar::IsAlnum(Character)
			&& Character != TEXT('_')
			&& Character != TEXT('-'))
		{
			return FString();
		}
	}

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
	if (CanonicalId == TEXT("character_gangman"))
	{
		return GangmanSkinId;
	}
	if (CanonicalId == TEXT("character_maskman"))
	{
		return MaskmanSkinId;
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

UShowDownCharacterSkinCatalog* UShowDownCharacterSkinCatalog::LoadDefaultCatalog()
{
	return LoadObject<UShowDownCharacterSkinCatalog>(nullptr, DefaultCatalogObjectPath);
}

FString UShowDownCharacterSkinCatalog::NormalizeKnownSkinId(
	const UShowDownCharacterSkinCatalog* Catalog,
	const FString& SkinId)
{
	const FString CanonicalSkinId = CanonicalizeSkinId(SkinId);
	if (CanonicalSkinId.IsEmpty())
	{
		return RobotSkinId;
	}

	FShowDownCharacterSkinDefinition Definition;
	if (FindBuiltInSkinDefinition(CanonicalSkinId, Definition)
		|| (Catalog && Catalog->FindSkinDefinition(CanonicalSkinId, Definition)))
	{
		return CanonicalSkinId;
	}

	return RobotSkinId;
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
			if (CatalogDefinition.Rarity != EShowDownCharacterSkinRarity::Unspecified)
			{
				OutDefinition.Rarity = CatalogDefinition.Rarity;
			}
			if (!CatalogDefinition.Thumbnail.IsNull())
			{
				OutDefinition.Thumbnail = CatalogDefinition.Thumbnail;
			}
			if (!CatalogDefinition.SkeletalMesh.IsNull())
			{
				OutDefinition.SkeletalMesh = CatalogDefinition.SkeletalMesh;
			}

			if (CatalogDefinition.ShopPreview.AnimationMode
				!= EShowDownShopPreviewAnimationMode::InheritBuiltIn)
			{
				OutDefinition.ShopPreview = CatalogDefinition.ShopPreview;
			}

			if (CatalogDefinition.MainMenuPreview.AnimationMode
				!= EShowDownShopPreviewAnimationMode::InheritBuiltIn)
			{
				OutDefinition.MainMenuPreview = CatalogDefinition.MainMenuPreview;
			}

			OutDefinition.PreviewLocationOffset = CatalogDefinition.PreviewLocationOffset;
			OutDefinition.PreviewRotationOffset = CatalogDefinition.PreviewRotationOffset;
			OutDefinition.PreviewScale = CatalogDefinition.PreviewScale;
		}
		else
		{
			OutDefinition = CatalogDefinition;
			if (OutDefinition.Rarity == EShowDownCharacterSkinRarity::Unspecified)
			{
				OutDefinition.Rarity = EShowDownCharacterSkinRarity::Common;
			}
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

void UShowDownCharacterSkinCatalog::GetOrderedSkinDefinitions(
	const UShowDownCharacterSkinCatalog* Catalog,
	TArray<FShowDownCharacterSkinDefinition>& OutDefinitions)
{
	OutDefinitions.Reset();
	TSet<FString> AddedSkinIds;

	auto AddResolvedDefinition = [&](const FString& RequestedSkinId)
	{
		const FString CanonicalSkinId = CanonicalizeSkinId(RequestedSkinId);
		if (CanonicalSkinId.IsEmpty() || AddedSkinIds.Contains(CanonicalSkinId))
		{
			return;
		}
		// Micu remains resolvable for existing owners and replicated legacy
		// loadouts, but is intentionally retired from the character shop.
		if (CanonicalSkinId == MicuSkinId)
		{
			return;
		}

		FShowDownCharacterSkinDefinition Definition;
		FString ResolvedSkinId;
		if (ResolveSkinDefinition(Catalog, CanonicalSkinId, Definition, ResolvedSkinId)
			&& CanonicalizeSkinId(ResolvedSkinId) == CanonicalSkinId)
		{
			AddedSkinIds.Add(CanonicalSkinId);
			OutDefinitions.Add(MoveTemp(Definition));
		}
	};

	if (Catalog)
	{
		for (const FShowDownCharacterSkinDefinition& Definition : Catalog->Skins)
		{
			AddResolvedDefinition(Definition.SkinId);
		}
	}

	for (const FShowDownCharacterSkinDefinition& BuiltInDefinition : GetBuiltInSkinDefinitions())
	{
		AddResolvedDefinition(BuiltInDefinition.SkinId);
	}
}

const FShowDownCharacterPreviewAnimationProfile&
UShowDownCharacterSkinCatalog::GetPreviewProfile(
	const FShowDownCharacterSkinDefinition& Definition,
	const EShowDownCharacterPreviewContext Context)
{
	return Context == EShowDownCharacterPreviewContext::MainMenu
		? Definition.MainMenuPreview
		: Definition.ShopPreview;
}

FText UShowDownCharacterSkinCatalog::GetRarityDisplayName(
	const EShowDownCharacterSkinRarity Rarity)
{
	switch (Rarity)
	{
	case EShowDownCharacterSkinRarity::Rare:
		return NSLOCTEXT("ShowDownCharacterSkins", "Rare", "RARE");
	case EShowDownCharacterSkinRarity::Epic:
		return NSLOCTEXT("ShowDownCharacterSkins", "Epic", "EPIC");
	case EShowDownCharacterSkinRarity::Legendary:
		return NSLOCTEXT("ShowDownCharacterSkins", "Legendary", "LEGENDARY");
	case EShowDownCharacterSkinRarity::Unspecified:
	case EShowDownCharacterSkinRarity::Common:
	default:
		return NSLOCTEXT("ShowDownCharacterSkins", "Common", "COMMON");
	}
}

FLinearColor UShowDownCharacterSkinCatalog::GetRarityColor(
	const EShowDownCharacterSkinRarity Rarity)
{
	switch (Rarity)
	{
	case EShowDownCharacterSkinRarity::Rare:
		return FLinearColor(0.16f, 0.48f, 1.0f, 1.0f);
	case EShowDownCharacterSkinRarity::Epic:
		return FLinearColor(0.65f, 0.24f, 0.94f, 1.0f);
	case EShowDownCharacterSkinRarity::Legendary:
		return FLinearColor(0.95f, 0.69f, 0.16f, 1.0f);
	case EShowDownCharacterSkinRarity::Unspecified:
	case EShowDownCharacterSkinRarity::Common:
	default:
		return FLinearColor(0.82f, 0.84f, 0.86f, 1.0f);
	}
}
