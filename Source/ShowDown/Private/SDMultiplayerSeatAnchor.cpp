#include "SDMultiplayerSeatAnchor.h"

#include "Components/SceneComponent.h"

ASDMultiplayerSeatAnchor::ASDMultiplayerSeatAnchor()
{
	bReplicates = true;
	bAlwaysRelevant = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	HandSlot = CreateDefaultSubobject<USceneComponent>(TEXT("HandSlot"));
	HandSlot->SetupAttachment(SceneRoot);
	HandSlot->SetRelativeLocation(FVector(50.0f, 0.0f, -45.0f));
	HandSlot->SetRelativeRotation(FRotator(-30.0f, 0.0f, 0.0f));

	ForeheadSlot = CreateDefaultSubobject<USceneComponent>(TEXT("ForeheadSlot"));
	ForeheadSlot->SetupAttachment(SceneRoot);
	ForeheadSlot->SetRelativeLocation(FVector(20.0f, 0.0f, 14.0f));
	ForeheadSlot->SetRelativeRotation(FRotator::ZeroRotator);
}

void ASDMultiplayerSeatAnchor::ConfigureFromCameraTransform(const FTransform& CameraTransform)
{
	const FRotator CameraRotation = CameraTransform.Rotator();
	SetActorLocationAndRotation(
		CameraTransform.GetLocation(),
		FRotator(0.0f, CameraRotation.Yaw, 0.0f));
}
