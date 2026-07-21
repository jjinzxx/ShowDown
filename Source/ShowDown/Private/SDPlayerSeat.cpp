#include "SDPlayerSeat.h"

ASDPlayerSeat::ASDPlayerSeat()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PlayerHandCard = CreateDefaultSubobject<USceneComponent>(TEXT("PlayerHandRoot"));
	PlayerHandCard->SetupAttachment(SceneRoot);

	PlayerHeadCard = CreateDefaultSubobject<USceneComponent>(TEXT("PlayerHeadCardSlot"));
	PlayerHeadCard->SetupAttachment(SceneRoot);

	ApplySlotTransforms();
}

void ASDPlayerSeat::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplySlotTransforms();
}

void ASDPlayerSeat::ApplySlotTransforms()
{
	if (PlayerHandCard)
	{
		PlayerHandCard->SetRelativeLocationAndRotation(HandSlotLocation, HandSlotRotation);
	}

	if (PlayerHeadCard)
	{
		PlayerHeadCard->SetRelativeLocationAndRotation(HeadSlotLocation, HeadSlotRotation);
	}
}

USceneComponent* ASDPlayerSeat::GetHandSlot() const
{
	return PlayerHandCard;
}

USceneComponent* ASDPlayerSeat::GetHeadSlot() const
{
	return PlayerHeadCard;
}
