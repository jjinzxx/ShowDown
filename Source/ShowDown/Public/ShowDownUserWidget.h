#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShowDownUserWidget.generated.h"

/**
 * Shared base for ShowDown screen widgets.
 *
 * Common UI behavior belongs here so authored Widget Blueprints and native
 * fallback layouts receive the same treatment.
 */
UCLASS(Abstract)
class SHOWDOWN_API UShowDownUserWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

private:
	void ApplyButtonClickSound();
};
