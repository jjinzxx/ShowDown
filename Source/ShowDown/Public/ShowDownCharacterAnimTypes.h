#pragma once

#include "CoreMinimal.h"
#include "ShowDownCharacterAnimTypes.generated.h"

UENUM(BlueprintType)
enum class EShowDownCharacterAnimState : uint8
{
	Idle UMETA(DisplayName = "Idle"),
	Shoot UMETA(DisplayName = "Shoot"),
	Hit UMETA(DisplayName = "Hit"),
};

UENUM(BlueprintType)
enum class EShowDownCharacterRole : uint8
{
	Unassigned UMETA(DisplayName = "Unassigned"),
	Player UMETA(DisplayName = "Player"),
	Opponent UMETA(DisplayName = "Opponent"),
	Cinematic UMETA(DisplayName = "Cinematic"),
};
