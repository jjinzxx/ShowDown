#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "ShowDownCharacterAnimTypes.h"
#include "ShowDownTypes.h"
#include "ShowDownCharacterAnimInstance.generated.h"

class AShowDownCharacter;

UCLASS(Blueprintable, BlueprintType)
class SHOWDOWN_API UShowDownCharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Character Animation")
	void SetCharacterAnimState(EShowDownCharacterAnimState NewState);

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Character Animation")
	TObjectPtr<AShowDownCharacter> OwningShowDownCharacter = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Character Animation")
	EShowDownCharacterAnimState CharacterAnimState = EShowDownCharacterAnimState::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Character Animation")
	bool bIsShooting = false;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Character Animation")
	bool bIsSelectingCard = false;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Character Animation")
	bool bIsBetting = false;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Character Animation")
	bool bIsHitReacting = false;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Character Camera")
	float HeadLookPitch = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Character Camera")
	float HeadLookYaw = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Character Camera")
	bool bHasHeadLook = false;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Character Animation")
	float GroundSpeed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Character Animation")
	bool bIsMoving = false;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Character Animation")
	bool bIsInAir = false;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Character Animation")
	bool bIsLocallyControlled = false;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Character Identity")
	EShowDownCharacterRole CharacterRole = EShowDownCharacterRole::Unassigned;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Character Identity")
	EShowDownPlayerSlot PlayerSlot = EShowDownPlayerSlot::None;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Character Identity")
	bool bIsLocalPlayerCharacter = false;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Character Identity")
	bool bIsOpponentCharacterForLocalPlayer = false;

private:
	void CacheOwningCharacter();
};
