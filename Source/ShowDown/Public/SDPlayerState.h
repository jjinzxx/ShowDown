#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "ShowDownTypes.h"
#include "SDPlayerState.generated.h"

class ACard;
class FLifetimeProperty;

UCLASS()
class SHOWDOWN_API ASDPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	//플레이어가 들고 있는 손패 카드 목록
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "ShowDown|Card")
	TArray<ACard*> HandCards;

	//플레이어 이마에 올라간 카드
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "ShowDown|Card")
	ACard* ForeheadCard = nullptr;

	//플레이어 현재 목숨
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "ShowDown|State")
	int32 Lives = 3;

	//플레이어 현재 베팅 총알 수
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "ShowDown|Betting")
	int32 CurrentBet = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "ShowDown|Multiplayer")
	EShowDownPlayerSlot ShowDownSlot = EShowDownPlayerSlot::None;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "ShowDown|Multiplayer")
	bool bReady = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "ShowDown|Multiplayer")
	bool bHostPlayer = false;

	// Stable catalog id for the character appearance selected by this player.
	// Only the id is replicated; every client resolves the matching local assets.
	UPROPERTY(ReplicatedUsing = OnRep_EquippedCharacterSkinId, BlueprintReadOnly, Category = "ShowDown|Cosmetics")
	FString EquippedCharacterSkinId = TEXT("robot");

	//손패에 카드 추가
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Card")
	void AddHandCard(ACard* Card);

	//손패에서 카드 제거
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Card")
	void RemoveHandCard(ACard* Card);

	//손패 전체 비우기
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Card")
	void ClearHand();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Multiplayer")
	void SetShowDownSlot(EShowDownPlayerSlot NewSlot);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Multiplayer")
	void SetReady(bool bNewReady);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Multiplayer")
	void SetHostPlayer(bool bNewHostPlayer);

	// Authority-only. Unknown or empty ids fall back to the built-in robot skin.
	void SetEquippedCharacterSkinId(const FString& NewSkinId);
	const FString& GetEquippedCharacterSkinId() const { return EquippedCharacterSkinId; }

	virtual void CopyProperties(APlayerState* PlayerState) override;
	virtual void OverrideWith(APlayerState* PlayerState) override;

protected:
	UFUNCTION()
	void OnRep_EquippedCharacterSkinId();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
