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
		bIsSelectingCard = false;
		bIsBetting = false;
		bIsFolding = false;
		bIsRaising = false;
		bIsCallingOrChecking = false;
		bIsReviving = false;
		bIsReactingToRedLight = false;
		bIsHitReacting = false;
		HeadLookPitch = 0.0f;
		HeadLookYaw = 0.0f;
		bHasHeadLook = false;
		CharacterRole = EShowDownCharacterRole::Unassigned;
		PlayerSlot = EShowDownPlayerSlot::None;
		bIsLocalPlayerCharacter = false;
		bIsOpponentCharacterForLocalPlayer = false;
		return;
	}

	CharacterAnimState = OwningShowDownCharacter->GetCharacterAnimState();
	bIsShooting = CharacterAnimState == EShowDownCharacterAnimState::Shoot;
	bIsSelectingCard = CharacterAnimState == EShowDownCharacterAnimState::SelectCard;
	bIsFolding = CharacterAnimState == EShowDownCharacterAnimState::Fold;
	bIsRaising = CharacterAnimState == EShowDownCharacterAnimState::Raise;
	bIsCallingOrChecking = CharacterAnimState == EShowDownCharacterAnimState::CallCheck;
	bIsBetting = CharacterAnimState == EShowDownCharacterAnimState::Betting
		|| bIsFolding
		|| bIsRaising
		|| bIsCallingOrChecking;
	bIsReviving = CharacterAnimState == EShowDownCharacterAnimState::Revive;
	bIsReactingToRedLight = CharacterAnimState == EShowDownCharacterAnimState::RedLight;
	bIsHitReacting = CharacterAnimState == EShowDownCharacterAnimState::Hit;
	HeadLookPitch = OwningShowDownCharacter->GetHeadLookPitch();
	HeadLookYaw = OwningShowDownCharacter->GetHeadLookYaw();
	bHasHeadLook = !FMath::IsNearlyZero(HeadLookPitch, 0.1f) || !FMath::IsNearlyZero(HeadLookYaw, 0.1f);
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
	bIsShooting = CharacterAnimState == EShowDownCharacterAnimState::Shoot;
	bIsSelectingCard = CharacterAnimState == EShowDownCharacterAnimState::SelectCard;
	bIsFolding = CharacterAnimState == EShowDownCharacterAnimState::Fold;
	bIsRaising = CharacterAnimState == EShowDownCharacterAnimState::Raise;
	bIsCallingOrChecking = CharacterAnimState == EShowDownCharacterAnimState::CallCheck;
	bIsBetting = CharacterAnimState == EShowDownCharacterAnimState::Betting
		|| bIsFolding
		|| bIsRaising
		|| bIsCallingOrChecking;
	bIsReviving = CharacterAnimState == EShowDownCharacterAnimState::Revive;
	bIsReactingToRedLight = CharacterAnimState == EShowDownCharacterAnimState::RedLight;
	bIsHitReacting = CharacterAnimState == EShowDownCharacterAnimState::Hit;
}

void UShowDownCharacterAnimInstance::CacheOwningCharacter()
{
	if (OwningShowDownCharacter)
	{
		return;
	}

	OwningShowDownCharacter = Cast<AShowDownCharacter>(TryGetPawnOwner());
}
