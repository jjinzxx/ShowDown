// Fill out your copyright notice in the Description page of Project Settings.

#include "Presentation/SDPressableButtonActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"

ASDPressableButtonActor::ASDPressableButtonActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ButtonMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ButtonMesh"));
	ButtonMesh->SetupAttachment(SceneRoot);
	ButtonMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ButtonMesh->SetCollisionObjectType(ECC_WorldDynamic);
	ButtonMesh->SetCollisionResponseToAllChannels(ECR_Block);
}

void ASDPressableButtonActor::BeginPlay()
{
	Super::BeginPlay();
	SetActorTickEnabled(false);

	RestRelativeLocation = ButtonMesh->GetRelativeLocation();
	PressedRelativeLocation = RestRelativeLocation + LocalPressOffset;

	if (bEnableMouseClick)
	{
		ButtonMesh->OnClicked.AddDynamic(this, &ASDPressableButtonActor::HandleButtonClicked);
	}

	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (bEnablePlayerClickEventsOnBeginPlay)
		{
			PlayerController->bEnableClickEvents = true;
		}
	}
}

void ASDPressableButtonActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (AnimState == EButtonAnimState::Idle)
	{
		return;
	}

	StateElapsedTime += DeltaSeconds;

	switch (AnimState)
	{
	case EButtonAnimState::PressingDown:
	{
		const float Alpha = FMath::Clamp(StateElapsedTime / PressDownTime, 0.0f, 1.0f);
		const float EasedAlpha = FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 3.0f);
		ButtonMesh->SetRelativeLocation(FMath::Lerp(PressStartRelativeLocation, PressedRelativeLocation, EasedAlpha));

		if (Alpha >= 1.0f)
		{
			AnimState = EButtonAnimState::Holding;
			StateElapsedTime = 0.0f;
		}
		break;
	}
	case EButtonAnimState::Holding:
		if (StateElapsedTime >= HoldTime)
		{
			AnimState = EButtonAnimState::Returning;
			StateElapsedTime = 0.0f;
		}
		break;
	case EButtonAnimState::Returning:
	{
		const float Alpha = FMath::Clamp(StateElapsedTime / ReturnTime, 0.0f, 1.0f);
		const float EasedAlpha = FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 2.0f);
		FVector NewLocation = FMath::Lerp(PressedRelativeLocation, RestRelativeLocation, EasedAlpha);

		if (bUseReturnBounce && ReturnBounceDistance > 0.0f && !LocalPressOffset.IsNearlyZero())
		{
			const FVector BounceDirection = -LocalPressOffset.GetSafeNormal();
			const float BounceAmount = FMath::Sin(Alpha * PI) * ReturnBounceDistance;
			NewLocation += BounceDirection * BounceAmount;
		}

		ButtonMesh->SetRelativeLocation(NewLocation);

		if (Alpha >= 1.0f)
		{
			FinishPressAnimation();
		}
		break;
	}
	default:
		break;
	}
}

void ASDPressableButtonActor::Press()
{
	PressedRelativeLocation = RestRelativeLocation + LocalPressOffset;
	PressStartRelativeLocation = ButtonMesh->GetRelativeLocation();
	AnimState = EButtonAnimState::PressingDown;
	StateElapsedTime = 0.0f;
	SetActorTickEnabled(true);
	OnPressed.Broadcast();
}

bool ASDPressableButtonActor::CanInteract_Implementation(AActor* Interactor) const
{
	return true;
}

void ASDPressableButtonActor::Interact_Implementation(AActor* Interactor)
{
	Press();
}

void ASDPressableButtonActor::HandleButtonClicked(UPrimitiveComponent* TouchedComponent, FKey ButtonPressed)
{
	Press();
}

void ASDPressableButtonActor::FinishPressAnimation()
{
	ButtonMesh->SetRelativeLocation(RestRelativeLocation);
	AnimState = EButtonAnimState::Idle;
	StateElapsedTime = 0.0f;
	SetActorTickEnabled(false);
	OnPressFinished.Broadcast();
}
