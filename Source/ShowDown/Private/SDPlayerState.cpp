#include "SDPlayerState.h"

#include "Card.h"
#include "Net/UnrealNetwork.h"
#include "ShowDownCharacterSkinCatalog.h"

namespace
{
	FString NormalizeEquippedCharacterSkinId(const FString& SkinId)
	{
		FShowDownCharacterSkinDefinition Definition;
		FString ResolvedSkinId;
		UShowDownCharacterSkinCatalog::ResolveSkinDefinition(
			nullptr,
			SkinId,
			Definition,
			ResolvedSkinId);
		return ResolvedSkinId;
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
	DOREPLIFETIME(ASDPlayerState, Lives);
	DOREPLIFETIME(ASDPlayerState, CurrentBet);
	DOREPLIFETIME(ASDPlayerState, ShowDownSlot);
	DOREPLIFETIME(ASDPlayerState, bReady);
	DOREPLIFETIME(ASDPlayerState, bHostPlayer);
	DOREPLIFETIME(ASDPlayerState, EquippedCharacterSkinId);
}
