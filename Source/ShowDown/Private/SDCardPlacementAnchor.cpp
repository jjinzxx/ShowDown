#include "SDCardPlacementAnchor.h"

#include "Components/SceneComponent.h"
#include "UObject/UnrealType.h"

ASDCardPlacementAnchor::ASDCardPlacementAnchor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CardSlot = CreateDefaultSubobject<USceneComponent>(TEXT("CardSlot"));
	CardSlot->SetupAttachment(SceneRoot);
}

USceneComponent* ASDCardPlacementAnchor::GetSlotComponent() const
{
	return CardSlot ? CardSlot.Get() : GetRootComponent();
}

bool ASDCardPlacementAnchor::IsHandAnchor() const
{
	return PlacementRole == ESDCardPlacementRole::PlayerHand
		|| PlacementRole == ESDCardPlacementRole::OpponentHand
		|| PlacementRole == ESDCardPlacementRole::Player3Hand
		|| PlacementRole == ESDCardPlacementRole::Player4Hand;
}

bool ASDCardPlacementAnchor::IsForeheadAnchor() const
{
	return PlacementRole == ESDCardPlacementRole::PlayerForehead
		|| PlacementRole == ESDCardPlacementRole::OpponentForehead
		|| PlacementRole == ESDCardPlacementRole::Player3Forehead
		|| PlacementRole == ESDCardPlacementRole::Player4Forehead;
}

#if WITH_EDITOR
bool ASDCardPlacementAnchor::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	const FName PropertyName = InProperty ? InProperty->GetFName() : NAME_None;
	const bool bHandOnlyProperty =
		PropertyName == GET_MEMBER_NAME_CHECKED(ASDCardPlacementAnchor, CardSpacing)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(ASDCardPlacementAnchor, ForwardOffset)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(ASDCardPlacementAnchor, HeightOffset)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(ASDCardPlacementAnchor, LeanAngle)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(ASDCardPlacementAnchor, LayerStep)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(ASDCardPlacementAnchor, SelectedOffset)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(ASDCardPlacementAnchor, HoverOffset)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(ASDCardPlacementAnchor, MoveSpeed);

	return !bHandOnlyProperty || IsHandAnchor();
}
#endif

ASDPlayerHandAnchor::ASDPlayerHandAnchor()
{
	PlacementRole = ESDCardPlacementRole::PlayerHand;
}

ASDPlayerForeheadAnchor::ASDPlayerForeheadAnchor()
{
	PlacementRole = ESDCardPlacementRole::PlayerForehead;
}

ASDOpponentHandAnchor::ASDOpponentHandAnchor()
{
	PlacementRole = ESDCardPlacementRole::OpponentHand;
}

ASDOpponentForeheadAnchor::ASDOpponentForeheadAnchor()
{
	PlacementRole = ESDCardPlacementRole::OpponentForehead;
}

ASDPlayer3HandAnchor::ASDPlayer3HandAnchor()
{
	PlacementRole = ESDCardPlacementRole::Player3Hand;
	CardSpacing = 9.0f;
	ForwardOffset = 0.0f;
	HeightOffset = 0.0f;
	LeanAngle = 0.0f;
	LayerStep = 0.0f;
}

ASDPlayer3ForeheadAnchor::ASDPlayer3ForeheadAnchor()
{
	PlacementRole = ESDCardPlacementRole::Player3Forehead;
}

ASDPlayer4HandAnchor::ASDPlayer4HandAnchor()
{
	PlacementRole = ESDCardPlacementRole::Player4Hand;
	CardSpacing = 9.0f;
	ForwardOffset = 0.0f;
	HeightOffset = 0.0f;
	LeanAngle = 0.0f;
	LayerStep = 0.0f;
}

ASDPlayer4ForeheadAnchor::ASDPlayer4ForeheadAnchor()
{
	PlacementRole = ESDCardPlacementRole::Player4Forehead;
}
