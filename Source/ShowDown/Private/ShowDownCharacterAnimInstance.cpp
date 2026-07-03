#include "ShowDownCharacterAnimInstance.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "ShowDownCharacter.h"

void UShowDownCharacterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	CacheOwningCharacter();
}

void UShowDownCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	CacheOwningCharacter();
	if (!OwningShowDownCharacter)
	{
		GroundSpeed = 0.0f;
		bIsMoving = false;
		bIsInAir = false;
		bIsLocallyControlled = false;
		CharacterAnimState = EShowDownCharacterAnimState::Idle;
		bIsShooting = false;
		bIsHitReacting = false;
		CharacterRole = EShowDownCharacterRole::Unassigned;
		PlayerSlot = EShowDownPlayerSlot::None;
		bIsLocalPlayerCharacter = false;
		bIsOpponentCharacterForLocalPlayer = false;
		return;
	}

	CharacterAnimState = OwningShowDownCharacter->GetCharacterAnimState();
	bIsShooting = CharacterAnimState == EShowDownCharacterAnimState::Shoot;
	bIsHitReacting = CharacterAnimState == EShowDownCharacterAnimState::Hit;
	CharacterRole = OwningShowDownCharacter->GetCharacterRole();
	PlayerSlot = OwningShowDownCharacter->GetPlayerSlot();

	const FVector Velocity = OwningShowDownCharacter->GetVelocity();
	GroundSpeed = FVector(Velocity.X, Velocity.Y, 0.0f).Size();
	bIsMoving = GroundSpeed > 3.0f;
	bIsLocallyControlled = OwningShowDownCharacter->IsLocallyControlled();
	bIsLocalPlayerCharacter = OwningShowDownCharacter->IsLocalPlayerCharacter();
	bIsOpponentCharacterForLocalPlayer = OwningShowDownCharacter->IsOpponentCharacterForLocalPlayer();

	const UCharacterMovementComponent* MovementComponent = OwningShowDownCharacter->GetCharacterMovement();
	bIsInAir = MovementComponent ? MovementComponent->IsFalling() : false;
}

void UShowDownCharacterAnimInstance::SetCharacterAnimState(EShowDownCharacterAnimState NewState)
{
	CharacterAnimState = NewState;
}

void UShowDownCharacterAnimInstance::CacheOwningCharacter()
{
	if (OwningShowDownCharacter)
	{
		return;
	}

	OwningShowDownCharacter = Cast<AShowDownCharacter>(TryGetPawnOwner());
}
