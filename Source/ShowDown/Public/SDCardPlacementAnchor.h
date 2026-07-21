#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SDCardLayoutTypes.h"
#include "SDCardPlacementAnchor.generated.h"

class USceneComponent;

UENUM(BlueprintType)
enum class ESDCardPlacementRole : uint8
{
	PlayerHand UMETA(DisplayName = "Player Hand"),
	PlayerForehead UMETA(DisplayName = "Player Forehead"),
	OpponentHand UMETA(DisplayName = "Opponent Hand"),
	OpponentForehead UMETA(DisplayName = "Opponent Forehead"),
	Player3Hand UMETA(DisplayName = "Player 3 Hand"),
	Player3Forehead UMETA(DisplayName = "Player 3 Forehead"),
	Player4Hand UMETA(DisplayName = "Player 4 Hand"),
	Player4Forehead UMETA(DisplayName = "Player 4 Forehead")
};

UCLASS(Blueprintable)
class SHOWDOWN_API ASDCardPlacementAnchor : public AActor
{
	GENERATED_BODY()

public:
	ASDCardPlacementAnchor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ShowDown|Placement")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ShowDown|Placement")
	TObjectPtr<USceneComponent> CardSlot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Placement", meta = (DisplayName = "Placement Role"))
	ESDCardPlacementRole PlacementRole = ESDCardPlacementRole::PlayerHand;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Hand Layout", meta = (ClampMin = "0.0"))
	float CardSpacing = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Hand Layout", meta = (ClampMin = "0.0"))
	float ForwardOffset = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Hand Layout", meta = (ClampMin = "0.0"))
	float HeightOffset = 65.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Hand Layout", meta = (ClampMin = "-45.0", ClampMax = "45.0"))
	float LeanAngle = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Hand Layout", meta = (ClampMin = "0.0"))
	float LayerStep = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Card Motion")
	FVector SelectedOffset = FVector(0.0f, 0.0f, 12.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Card Motion")
	FVector HoverOffset = FVector(0.0f, 0.0f, 8.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Card Motion", meta = (ClampMin = "0.0"))
	float MoveSpeed = 10.0f;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ShowDown|Placement")
	USceneComponent* GetSlotComponent() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ShowDown|Placement")
	bool IsHandAnchor() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ShowDown|Placement")
	bool IsForeheadAnchor() const;

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif
};

UCLASS(Blueprintable)
class SHOWDOWN_API ASDPlayerHandAnchor : public ASDCardPlacementAnchor
{
	GENERATED_BODY()

public:
	ASDPlayerHandAnchor();
};

UCLASS(Blueprintable)
class SHOWDOWN_API ASDPlayerForeheadAnchor : public ASDCardPlacementAnchor
{
	GENERATED_BODY()

public:
	ASDPlayerForeheadAnchor();
};

UCLASS(Blueprintable)
class SHOWDOWN_API ASDOpponentHandAnchor : public ASDCardPlacementAnchor
{
	GENERATED_BODY()

public:
	ASDOpponentHandAnchor();
};

UCLASS(Blueprintable)
class SHOWDOWN_API ASDOpponentForeheadAnchor : public ASDCardPlacementAnchor
{
	GENERATED_BODY()

public:
	ASDOpponentForeheadAnchor();
};

UCLASS(Blueprintable)
class SHOWDOWN_API ASDPlayer3HandAnchor : public ASDCardPlacementAnchor
{
	GENERATED_BODY()

public:
	ASDPlayer3HandAnchor();
};

UCLASS(Blueprintable)
class SHOWDOWN_API ASDPlayer3ForeheadAnchor : public ASDCardPlacementAnchor
{
	GENERATED_BODY()

public:
	ASDPlayer3ForeheadAnchor();
};

UCLASS(Blueprintable)
class SHOWDOWN_API ASDPlayer4HandAnchor : public ASDCardPlacementAnchor
{
	GENERATED_BODY()

public:
	ASDPlayer4HandAnchor();
};

UCLASS(Blueprintable)
class SHOWDOWN_API ASDPlayer4ForeheadAnchor : public ASDCardPlacementAnchor
{
	GENERATED_BODY()

public:
	ASDPlayer4ForeheadAnchor();
};
