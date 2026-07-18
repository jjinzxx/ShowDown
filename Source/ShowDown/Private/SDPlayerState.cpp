#include "SDPlayerState.h"

#include "Card.h"
#include "Net/UnrealNetwork.h"
#include "ShowDownCharacterSkinCatalog.h"

namespace
{
	FString NormalizeEquippedCharacterSkinId(const FString& SkinId)
	{
		return UShowDownCharacterSkinCatalog::NormalizeKnownSkinId(
			UShowDownCharacterSkinCatalog::LoadDefaultCatalog(),
			SkinId);
	}
}

void ASDPlayerState::AddHandCard(ACard* Card)
{
	if (Card)
	{
		HandCards.Add(Card);
	}
}

void ASDPlayerState::RemoveHandCard(ACard* Card)
{
	if (Card)
	{
		HandCards.Remove(Card);
	}
}

void ASDPlayerState::ClearHand()
{
	HandCards.Reset();
}

void ASDPlayerState::SetPrivateCardRank(ACard* Card, int32 Rank)
{
	if (!HasAuthority() || !IsValid(Card))
	{
		return;
	}

	const int32 ClampedRank = FMath::Clamp(Rank, 1, 7);
	FSDPrivateCardRank* ExistingEntry = PrivateCardRanks.FindByPredicate([Card](const FSDPrivateCardRank& Entry)
	{
		return Entry.Card == Card;
	});
	if (ExistingEntry)
	{
		if (ExistingEntry->Rank == ClampedRank)
		{
			return;
		}
		ExistingEntry->Rank = ClampedRank;
	}
	else
	{
		FSDPrivateCardRank& NewEntry = PrivateCardRanks.AddDefaulted_GetRef();
		NewEntry.Card = Card;
		NewEntry.Rank = ClampedRank;
	}

	RefreshPrivateCardRankVisuals();
	ForceNetUpdate();
}

void ASDPlayerState::RemovePrivateCardRank(const ACard* Card)
{
	if (!HasAuthority() || !Card)
	{
		return;
	}

	const int32 RemovedCount = PrivateCardRanks.RemoveAll([Card](const FSDPrivateCardRank& Entry)
	{
		return Entry.Card == Card;
	});
	if (RemovedCount <= 0)
	{
		return;
	}

	RefreshPrivateCardRankVisuals();
	ForceNetUpdate();
}

void ASDPlayerState::ClearPrivateCardRanks()
{
	if (!HasAuthority() || PrivateCardRanks.IsEmpty())
	{
		return;
	}

	PrivateCardRanks.Reset();
	RefreshPrivateCardRankVisuals();
	ForceNetUpdate();
}

bool ASDPlayerState::TryGetPrivateCardRank(const ACard* Card, int32& OutRank) const
{
	if (!Card)
	{
		return false;
	}

	const FSDPrivateCardRank* Entry = PrivateCardRanks.FindByPredicate([Card](const FSDPrivateCardRank& Candidate)
	{
		return Candidate.Card == Card;
	});
	if (!Entry || Entry->Rank <= 0)
	{
		return false;
	}

	OutRank = Entry->Rank;
	return true;
}

void ASDPlayerState::OnRep_PrivateCardRanks()
{
	RefreshPrivateCardRankVisuals();
}

void ASDPlayerState::RefreshPrivateCardRankVisuals()
{
	TArray<TWeakObjectPtr<ACard>> CardsToRefresh = MoveTemp(CachedPrivateRankCards);
	CachedPrivateRankCards.Reset();

	for (const FSDPrivateCardRank& Entry : PrivateCardRanks)
	{
		if (IsValid(Entry.Card))
		{
			const TWeakObjectPtr<ACard> WeakCard(Entry.Card.Get());
			CardsToRefresh.AddUnique(WeakCard);
			CachedPrivateRankCards.AddUnique(WeakCard);
		}
	}

	for (const TWeakObjectPtr<ACard>& WeakCard : CardsToRefresh)
	{
		if (ACard* Card = WeakCard.Get())
		{
			Card->RefreshVisual();
		}
	}
}

void ASDPlayerState::SetShowDownSlot(EShowDownPlayerSlot NewSlot)
{
	if (HasAuthority())
	{
		ShowDownSlot = NewSlot;
	}
}

void ASDPlayerState::SetReady(bool bNewReady)
{
	if (HasAuthority())
	{
		bReady = bNewReady;
	}
}

void ASDPlayerState::SetHostPlayer(bool bNewHostPlayer)
{
	if (HasAuthority())
	{
		bHostPlayer = bNewHostPlayer;
	}
}

void ASDPlayerState::SetEquippedCharacterSkinId(const FString& NewSkinId)
{
	if (HasAuthority())
	{
		EquippedCharacterSkinId = NormalizeEquippedCharacterSkinId(NewSkinId);
		ForceNetUpdate();
	}
}

void ASDPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);

	if (ASDPlayerState* NewPlayerState = Cast<ASDPlayerState>(PlayerState))
	{
		// Preserve lobby identity across seamless travel. The new GameMode uses
		// these fields to keep the same seat/host instead of controller iteration order.
		NewPlayerState->ShowDownSlot = ShowDownSlot;
		NewPlayerState->bReady = bReady;
		NewPlayerState->bHostPlayer = bHostPlayer;
		NewPlayerState->EquippedCharacterSkinId = NormalizeEquippedCharacterSkinId(EquippedCharacterSkinId);
	}
}

void ASDPlayerState::OverrideWith(APlayerState* PlayerState)
{
	Super::OverrideWith(PlayerState);

	if (const ASDPlayerState* PreviousPlayerState = Cast<ASDPlayerState>(PlayerState))
	{
		ShowDownSlot = PreviousPlayerState->ShowDownSlot;
		bReady = PreviousPlayerState->bReady;
		bHostPlayer = PreviousPlayerState->bHostPlayer;
		EquippedCharacterSkinId = NormalizeEquippedCharacterSkinId(
			PreviousPlayerState->EquippedCharacterSkinId);
	}
}

void ASDPlayerState::OnRep_EquippedCharacterSkinId()
{
	EquippedCharacterSkinId = NormalizeEquippedCharacterSkinId(EquippedCharacterSkinId);
}

void ASDPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASDPlayerState, HandCards);
	DOREPLIFETIME(ASDPlayerState, ForeheadCard);
	DOREPLIFETIME_CONDITION(ASDPlayerState, PrivateCardRanks, COND_OwnerOnly);
	DOREPLIFETIME(ASDPlayerState, Lives);
	DOREPLIFETIME(ASDPlayerState, CurrentBet);
	DOREPLIFETIME(ASDPlayerState, ShowDownSlot);
	DOREPLIFETIME(ASDPlayerState, bReady);
	DOREPLIFETIME(ASDPlayerState, bHostPlayer);
	DOREPLIFETIME(ASDPlayerState, EquippedCharacterSkinId);
}
