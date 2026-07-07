// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SDCardLayoutTypes.h"
#include "CardSystem.generated.h"

class ACard;
class USceneComponent;


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SHOWDOWN_API UCardSystem : public UActorComponent
{
	GENERATED_BODY()

public:
	UCardSystem();

private:
	//현재 덱에 남아있는 카드 숫자 목록
	UPROPERTY()
	TArray<int32> Deck;

	//버려진 카드 숫자 목록
	UPROPERTY()
	TArray<int32> DiscardPile;

public:
	//덱을 초기화하고 새 덱을 생성
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Card")
	void ResetDeck(int32 CopiesPerRank = 4);
	//덱을 무작위로 섞음
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Card")
	void ShuffleDeck();
	//덱에서 지정한 수만큼 카드 숫자를 지급
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Card")
	bool DealCards(int32 Count, TArray<int32>& OutCards);
	//카드 숫자 1장을 폐기 목록에 추가
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Card")
	void DiscardCard(int32 Rank);
	//여러 카드 숫자를 폐기 목록에 추가
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Card")
	void DiscardCards(const TArray<int32>& Ranks);
	//현재 덱에 남은 카드 수 반환
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ShowDown|Card")
	int32 GetRemainingCardCount() const;
	//카드 숫자 배열을 바탕으로 손패 카드 액터들을 스폰
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Card")
	bool SpawnHandCards(
		UObject* WorldContextObject,
		TSubclassOf<ACard> CardClass,
		USceneComponent* HandRoot,
		const TArray<int32>& Ranks,
		const FSDCardHandLayoutSettings& HandLayoutSettings,
		bool bFaceUp,
		bool bSelectable,
		TArray<ACard*>& OutCards);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Card")
	bool LayoutHandCards(
		UObject* WorldContextObject,
		USceneComponent* HandRoot,
		const FSDCardHandLayoutSettings& HandLayoutSettings,
		const TArray<ACard*>& Cards);

	//손패 배열에서 특정 카드 액터 제거
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Card")
	bool RemoveCardFromHand(TArray<ACard*>& HandCards, ACard* Card);
	//카드 액터를 지정 슬롯으로 이동
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Card")
	bool MoveCardToSlot(ACard* Card, USceneComponent* Slot, bool bFaceUp);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Card")
	bool MoveCardToSlotWithRotationOffset(ACard* Card, USceneComponent* Slot, bool bFaceUp, FRotator RotationOffset);

private:
	void BuildDeck(int32 CopiesPerRank);
	FTransform BuildHandCardTransform(
		USceneComponent* HandRoot,
		const FSDCardHandLayoutSettings& HandLayoutSettings,
		int32 CardIndex,
		int32 CardCount) const;
};
