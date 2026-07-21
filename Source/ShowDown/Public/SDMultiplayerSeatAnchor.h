#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SDMultiplayerSeatAnchor.generated.h"

class USceneComponent;

// Runtime seat layout derived from a level-placed multiplayer camera.
UCLASS()
class SHOWDOWN_API ASDMultiplayerSeatAnchor : public AActor
{
	GENERATED_BODY()

public:
	ASDMultiplayerSeatAnchor();
	void ConfigureFromCameraTransform(const FTransform& CameraTransform);

	USceneComponent* GetHandSlot() const { return HandSlot; }
	USceneComponent* GetForeheadSlot() const { return ForeheadSlot; }

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> HandSlot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> ForeheadSlot;
};
