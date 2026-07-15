#pragma once

#include "CoreMinimal.h"
#include "ShowDownTypes.generated.h"

UENUM(BlueprintType)
enum class EShowDownPhase : uint8
{
	None,
	SelectCard,
	Betting,
	Reveal,
	Roulette,
	RoundEnd,
	GameOver
};

UENUM(BlueprintType)
enum class EShowDownSide : uint8
{
	Player,
	Collector
};

UENUM(BlueprintType)
enum class EShowDownRoundResult : uint8
{
	PlayerWin,
	CollectorWin,
	Draw
};

UENUM(BlueprintType)
enum class EShowDownBetAction : uint8
{
	Check,
	Call,
	Raise,
	Fold
};

UENUM(BlueprintType)
enum class EShowDownMatchMode : uint8
{
	SinglePlayer,
	Multiplayer
};

UENUM(BlueprintType)
enum class EShowDownPlayerSlot : uint8
{
	None,
	Player1,
	Player2,
	Player3,
	Player4
};

/** Server-authored table presentation beats shared by every local view. */
UENUM(BlueprintType)
enum class ESDTableCinematicCue : uint8
{
	Reset,
	MatchIntro,
	PreRevealBlackout,
	RevealStarted,
	LoserSpotlight,
	TableSpotlightOn,
	TableSpotlightOff,
	BetFocusStarted,
	BetFocusEnded,
	TriggerPullStarted,
	InitialDealStarted,
	InitialDealFinished
};

namespace ShowDownTableCinematics
{
	FORCEINLINE uint8 PlayerSlotToMask(EShowDownPlayerSlot Slot)
	{
		const int32 SlotIndex = static_cast<int32>(Slot) - 1;
		return SlotIndex >= 0 && SlotIndex < 4
			? static_cast<uint8>(1u << SlotIndex)
			: 0;
	}

	FORCEINLINE bool IsPlayerSlotInMask(uint8 Mask, EShowDownPlayerSlot Slot)
	{
		const uint8 SlotMask = PlayerSlotToMask(Slot);
		return SlotMask != 0 && (Mask & SlotMask) != 0;
	}

	// Bits 0-3 remain multiplayer seats. The two upper bits let the same
	// cinematic cue target the standalone player/opponent characters without
	// pretending that the Collector owns a multiplayer seat.
	FORCEINLINE uint8 SingleSideToMask(EShowDownSide Side)
	{
		return Side == EShowDownSide::Player
			? static_cast<uint8>(1u << 4)
			: static_cast<uint8>(1u << 5);
	}

	FORCEINLINE bool IsSingleSideInMask(uint8 Mask, EShowDownSide Side)
	{
		return (Mask & SingleSideToMask(Side)) != 0;
	}
}

USTRUCT(BlueprintType)
struct FShowDownNetworkPlayerSlot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Multiplayer")
	EShowDownPlayerSlot Slot = EShowDownPlayerSlot::None;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Multiplayer")
	FString PlayerId;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Multiplayer")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Multiplayer")
	bool bConnected = false;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Multiplayer")
	bool bReady = false;
};
