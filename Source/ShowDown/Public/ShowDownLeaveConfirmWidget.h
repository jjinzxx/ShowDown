#pragma once

#include "CoreMinimal.h"
#include "ShowDownUserWidget.h"
#include "ShowDownLeaveConfirmWidget.generated.h"

class AShowDownPlayerController;
class UBorder;
class UButton;
class UTextBlock;

// C++-only confirmation dialog used before a multiplayer player leaves a match.
UCLASS()
class SHOWDOWN_API UShowDownLeaveConfirmWidget : public UShowDownUserWidget
{
	GENERATED_BODY()

public:
	void SetOwningShowDownController(AShowDownPlayerController* InController);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UPROPERTY()
	AShowDownPlayerController* OwningShowDownController = nullptr;

	UPROPERTY()
	UButton* Button_Confirm = nullptr;

	UPROPERTY()
	UButton* Button_Cancel = nullptr;

	void BuildDefaultLayout();
	UButton* CreateDialogButton(const FString& Label, const FLinearColor& Color);

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleCancelClicked();
};
