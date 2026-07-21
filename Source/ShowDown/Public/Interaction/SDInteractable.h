// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SDInteractable.generated.h"

UINTERFACE(Blueprintable)
class SHOWDOWN_API USDInteractable : public UInterface
{
	GENERATED_BODY()
};

class SHOWDOWN_API ISDInteractable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Interaction")
	bool CanInteract(AActor* Interactor) const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Interaction")
	void Interact(AActor* Interactor);
};
