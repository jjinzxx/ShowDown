#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShowDownPauseMenuWidget.generated.h"
class UButton;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPauseMenuAction);
UCLASS()
class SHOWDOWN_API UShowDownPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable) FOnPauseMenuAction OnResume;
	UPROPERTY(BlueprintAssignable) FOnPauseMenuAction OnMainMenu;
	UPROPERTY(BlueprintAssignable) FOnPauseMenuAction OnSettings;
	UPROPERTY(BlueprintAssignable) FOnPauseMenuAction OnQuit;
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
private:
	UPROPERTY(meta=(BindWidget)) UButton* Button_Resume=nullptr;
	UPROPERTY(meta=(BindWidget)) UButton* Button_MainMenu=nullptr;
	UPROPERTY(meta=(BindWidget)) UButton* Button_Settings=nullptr;
	UPROPERTY(meta=(BindWidget)) UButton* Button_Quit=nullptr;
	UFUNCTION() void Resume(){OnResume.Broadcast();}
	UFUNCTION() void MainMenu(){OnMainMenu.Broadcast();}
	UFUNCTION() void Settings(){OnSettings.Broadcast();}
	UFUNCTION() void Quit(){OnQuit.Broadcast();}
};
