#pragma once

#include "CoreMinimal.h"
#include "ShowDownUserWidget.h"
#include "ShowDownSettingsWidget.generated.h"

class UButton;
class UTextBlock;
class UCanvasPanel;
class UEditableTextBox;
class USlider;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnShowDownSettingsRequest);

UCLASS()
class SHOWDOWN_API UShowDownSettingsWidget : public UShowDownUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category="ShowDown|Settings")
	FOnShowDownSettingsRequest OnBackRequested;

	UPROPERTY(BlueprintAssignable, Category="ShowDown|Settings")
	FOnShowDownSettingsRequest OnQuitRequested;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UPROPERTY(meta=(BindWidget)) UTextBlock* Text_Quality = nullptr;
	UPROPERTY(meta=(BindWidget)) UTextBlock* Text_WindowMode = nullptr;
	UPROPERTY(meta=(BindWidget)) UTextBlock* Text_VSync = nullptr;
	UPROPERTY(meta=(BindWidget)) UTextBlock* Text_Status = nullptr;
	UPROPERTY(meta=(BindWidget)) UTextBlock* Text_Resolution = nullptr;
	UPROPERTY(meta=(BindWidget)) UTextBlock* Text_Brightness = nullptr;
	UPROPERTY(meta=(BindWidget)) UTextBlock* Text_PostProcess = nullptr;
	UPROPERTY(meta=(BindWidget)) UTextBlock* Text_Effects = nullptr;
	UPROPERTY(meta=(BindWidget)) UTextBlock* Text_MouseSensitivity = nullptr;
	UPROPERTY(meta=(BindWidget)) UTextBlock* Text_MasterVolume = nullptr;
	UPROPERTY(meta=(BindWidget)) UTextBlock* Text_MusicVolume = nullptr;
	UPROPERTY(meta=(BindWidget)) UTextBlock* Text_EffectVolume = nullptr;
	UPROPERTY(meta=(BindWidget)) UTextBlock* Text_DialogVolume = nullptr;
	UPROPERTY(meta=(BindWidget)) UButton* Button_Quality = nullptr;
	UPROPERTY(meta=(BindWidget)) UButton* Button_WindowMode = nullptr;
	UPROPERTY(meta=(BindWidget)) UButton* Button_VSync = nullptr;
	UPROPERTY(meta=(BindWidget)) UButton* Button_Resolution = nullptr;
	UPROPERTY(meta=(BindWidget)) UButton* Button_Brightness = nullptr;
	UPROPERTY(meta=(BindWidget)) UButton* Button_PostProcess = nullptr;
	UPROPERTY(meta=(BindWidget)) UButton* Button_Effects = nullptr;
	UPROPERTY(meta=(BindWidget)) UButton* Button_Apply = nullptr;
	UPROPERTY(meta=(BindWidget)) UButton* Button_Back = nullptr;
	UPROPERTY(meta=(BindWidget)) UButton* Button_Quit = nullptr;
	UPROPERTY(meta=(BindWidget)) UButton* Button_TabGeneral = nullptr;
	UPROPERTY(meta=(BindWidget)) UButton* Button_TabGraphics = nullptr;
	UPROPERTY(meta=(BindWidget)) UButton* Button_TabSound = nullptr;
	UPROPERTY(meta=(BindWidget)) UCanvasPanel* Panel_General = nullptr;
	UPROPERTY(meta=(BindWidget)) UCanvasPanel* Panel_Graphics = nullptr;
	UPROPERTY(meta=(BindWidget)) UCanvasPanel* Panel_Sound = nullptr;
	UPROPERTY(meta=(BindWidget)) UEditableTextBox* EditableTextBox_CharacterName = nullptr;
	UPROPERTY(meta=(BindWidget)) USlider* Slider_MouseSensitivity = nullptr;
	UPROPERTY(meta=(BindWidget)) USlider* Slider_Brightness = nullptr;
	UPROPERTY(meta=(BindWidget)) USlider* Slider_MasterVolume = nullptr;
	UPROPERTY(meta=(BindWidget)) USlider* Slider_MusicVolume = nullptr;
	UPROPERTY(meta=(BindWidget)) USlider* Slider_EffectVolume = nullptr;
	UPROPERTY(meta=(BindWidget)) USlider* Slider_DialogVolume = nullptr;

	int32 PendingQuality = 3;
	EWindowMode::Type PendingWindowMode = EWindowMode::Windowed;
	bool bPendingVSync = true;
	FIntPoint PendingResolution = FIntPoint(1920, 1080);
	float PendingBrightness = 1.0f;
	int32 PendingPostProcess = 0;
	int32 PendingEffects = 2;
	float PendingMouseSensitivity = 1.0f;
	float PendingMasterVolume = 1.0f;
	float PendingMusicVolume = 1.0f;
	float PendingEffectVolume = 1.0f;
	float PendingDialogVolume = 1.0f;
	FString PendingCharacterName = TEXT("상대");

	void BuildLayout();
	void RefreshLabels();
	UButton* CreateButton(const FString& Label, UTextBlock*& OutLabel, const FLinearColor& Color);

	UFUNCTION() void HandleQualityClicked();
	UFUNCTION() void HandleWindowModeClicked();
	UFUNCTION() void HandleVSyncClicked();
	UFUNCTION() void HandleResolutionClicked();
	UFUNCTION() void HandleBrightnessChanged(float Value);
	UFUNCTION() void HandlePostProcessClicked();
	UFUNCTION() void HandleEffectsClicked();
	UFUNCTION() void HandleMouseSensitivityChanged(float Value);
	UFUNCTION() void HandleMasterVolumeChanged(float Value);
	UFUNCTION() void HandleMusicVolumeChanged(float Value);
	UFUNCTION() void HandleEffectVolumeChanged(float Value);
	UFUNCTION() void HandleDialogVolumeChanged(float Value);
	UFUNCTION() void HandleApplyClicked();
	UFUNCTION() void HandleBackClicked();
	UFUNCTION() void HandleQuitClicked();
	UFUNCTION() void HandleGeneralTabClicked();
	UFUNCTION() void HandleGraphicsTabClicked();
	UFUNCTION() void HandleSoundTabClicked();
	void ShowSettingsPanel(UCanvasPanel* PanelToShow);
};
