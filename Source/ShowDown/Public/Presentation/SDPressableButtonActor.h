// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "Interaction/SDInteractable.h"
#include "SDPressableButtonActor.generated.h"

class UStaticMeshComponent;
class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSDPressableButtonEvent);

UCLASS(Blueprintable)
class SHOWDOWN_API ASDPressableButtonActor : public AActor, public ISDInteractable
{
	GENERATED_BODY()

public:
	ASDPressableButtonActor();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Pressable Button")
	void Press();

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

	UPROPERTY(BlueprintAssignable, Category = "Pressable Button")
	FSDPressableButtonEvent OnPressed;

	UPROPERTY(BlueprintAssignable, Category = "Pressable Button")
	FSDPressableButtonEvent OnPressFinished;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ButtonMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pressable Button")
	FVector LocalPressOffset = FVector(0.0f, 0.0f, -8.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pressable Button", meta = (ClampMin = "0.01"))
	float PressDownTime = 0.07f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pressable Button", meta = (ClampMin = "0.0"))
	float HoldTime = 0.04f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pressable Button", meta = (ClampMin = "0.01"))
	float ReturnTime = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pressable Button")
	bool bUseReturnBounce = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pressable Button", meta = (ClampMin = "0.0"))
	float ReturnBounceDistance = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	bool bEnableMouseClick = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	bool bEnablePlayerClickEventsOnBeginPlay = true;

private:
	enum class EButtonAnimState : uint8
	{
		Idle,
		PressingDown,
		Holding,
		Returning
	};

	UFUNCTION()
	void HandleButtonClicked(UPrimitiveComponent* TouchedComponent, FKey ButtonPressed);

	void FinishPressAnimation();

	FVector RestRelativeLocation = FVector::ZeroVector;
	FVector PressStartRelativeLocation = FVector::ZeroVector;
	FVector PressedRelativeLocation = FVector::ZeroVector;
	EButtonAnimState AnimState = EButtonAnimState::Idle;
	float StateElapsedTime = 0.0f;
};
