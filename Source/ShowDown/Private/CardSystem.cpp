// Fill out your copyright notice in the Description page of Project Settings.


#include "CardSystem.h"

#include "Card.h"

// Sets default values for this component's properties
UCardSystem::UCardSystem()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCardSystem::ResetDeck(int32 CopiesPerRank)
{
	Deck.Reset();
	DiscardPile.Reset();
	BuildDeck(CopiesPerRank);
}

void UCardSystem::ShuffleDeck()
{
	for (int32 Index = Deck.Num() - 1; Index > 0; --Index)
	{
		const int32 SwapIndex = FMath::RandRange(0, Index);
		Deck.Swap(Index, SwapIndex);
	}
}

bool UCardSystem::DealCards(int32 Count, TArray<int32>& OutCards)
{
	OutCards.Reset();

	if (Count <= 0 || Deck.Num() < Count)
	{
		return false;
	}

	for (int32 Index = 0; Index < Count; ++Index)
	{
		OutCards.Add(Deck[0]);
		Deck.RemoveAt(0);
	}

	return true;
}

void UCardSystem::DiscardCard(int32 Rank)
{
	DiscardPile.Add(Rank);
}

void UCardSystem::DiscardCards(const TArray<int32>& Ranks)
{
	for (const int32 Rank : Ranks)
	{
		DiscardCard(Rank);
	}
}

int32 UCardSystem::GetRemainingCardCount() const
{
	return Deck.Num();
}

bool UCardSystem::SpawnHandCards(
	UObject* WorldContextObject,
	TSubclassOf<ACard> CardClass,
	USceneComponent* HandRoot,
	const TArray<int32>& Ranks,
	const FSDCardHandLayoutSettings& HandLayoutSettings,
	bool bFaceUp,
	bool bSelectable,
	TArray<ACard*>& OutCards)
{
	OutCards.Reset();

	if (!WorldContextObject || !CardClass || !HandRoot)
	{
		return false;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return false;
	}

	const int32 CardsToDeal = Ranks.Num();
	for (int32 Index = 0; Index < CardsToDeal; ++Index)
	{
		const int32 Rank = Ranks[Index];
		const FTransform SpawnTransform = BuildHandCardTransform(HandRoot, HandLayoutSettings, Index, CardsToDeal);

		ACard* NewCard = World->SpawnActor<ACard>(CardClass, SpawnTransform);
		if (!NewCard)
		{
			continue;
		}

		NewCard->SetCard(Rank);
		NewCard->SetFaceUp(bFaceUp);
		NewCard->SetSelectable(bSelectable);
		NewCard->MoveToHandTransform(SpawnTransform);
		OutCards.Add(NewCard);
	}

	return OutCards.Num() == CardsToDeal;
}

bool UCardSystem::LayoutHandCards(
	UObject* WorldContextObject,
	USceneComponent* HandRoot,
	const FSDCardHandLayoutSettings& HandLayoutSettings,
	const TArray<ACard*>& Cards)
{
	if (!WorldContextObject || !HandRoot)
	{
		return false;
	}

	const int32 CardCount = Cards.Num();
	for (int32 Index = 0; Index < CardCount; ++Index)
	{
		ACard* Card = Cards[Index];
		if (!IsValid(Card))
		{
			continue;
		}

		const FTransform TargetTransform = BuildHandCardTransform(HandRoot, HandLayoutSettings, Index, CardCount);
		Card->MoveToHandTransform(TargetTransform);
	}

	return true;
}

bool UCardSystem::RemoveCardFromHand(TArray<ACard*>& HandCards, ACard* Card)
{
	if (!Card)
	{
		return false;
	}

	return HandCards.Remove(Card) > 0;
}

bool UCardSystem::MoveCardToSlot(ACard* Card, USceneComponent* Slot, bool bFaceUp)
{
	if (!Card || !Slot)
	{
		return false;
	}

	Card->MoveToSlot(Slot, bFaceUp);
	return true;
}

bool UCardSystem::MoveCardToSlotWithRotationOffset(ACard* Card, USceneComponent* Slot, bool bFaceUp, FRotator RotationOffset)
{
	if (!Card || !Slot)
	{
		return false;
	}

	Card->MoveToSlotWithRotationOffset(Slot, bFaceUp, RotationOffset);
	return true;
}

void UCardSystem::BuildDeck(int32 CopiesPerRank)
{
	const int32 ClampedCopiesPerRank = FMath::Max(0, CopiesPerRank);
	for (int32 Rank = 1; Rank <= 7; ++Rank)
	{
		for (int32 CopyIndex = 0; CopyIndex < ClampedCopiesPerRank; ++CopyIndex)
		{
			Deck.Add(Rank);
		}
	}
}

FTransform UCardSystem::BuildHandCardTransform(
	USceneComponent* HandRoot,
	const FSDCardHandLayoutSettings& HandLayoutSettings,
	int32 CardIndex,
	int32 CardCount) const
{
	if (!HandRoot || CardCount <= 0)
	{
		return FTransform::Identity;
	}

	const FVector CardRight = HandRoot->GetRightVector();
	const FVector CardForward = HandRoot->GetForwardVector();
	const FVector CardUp = HandRoot->GetUpVector();

	const float LayerFromCenter = static_cast<float>(CardIndex) - static_cast<float>(CardCount - 1) * 0.5f;

	const FVector CardLocation =
		HandRoot->GetComponentLocation()
		+ CardForward * HandLayoutSettings.ForwardOffset
		+ CardUp * HandLayoutSettings.HeightOffset
		+ CardRight * (LayerFromCenter * HandLayoutSettings.CardSpacing)
		+ CardForward * (LayerFromCenter * HandLayoutSettings.LayerStep);
	const FQuat LeanRotation(CardRight, FMath::DegreesToRadians(HandLayoutSettings.LeanAngle));
	const FQuat CardRotation = LeanRotation * HandRoot->GetComponentQuat();

	return FTransform(CardRotation, CardLocation);
}

