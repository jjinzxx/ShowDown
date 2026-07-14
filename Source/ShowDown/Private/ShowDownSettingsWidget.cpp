#include "ShowDownSettingsWidget.h"

#include "Audio/ShowDownAudioSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/GameUserSettings.h"
#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/ConfigCacheIni.h"
#include "ShowDownPlayerController.h"
#include "Styling/CoreStyle.h"
#include "SupabaseSubsystem.h"

namespace
{
const TCHAR* UserSettingsSection = TEXT("ShowDown.UserSettings");
float SensitivityFromSlider(float Value) { return FMath::Lerp(0.2f, 2.0f, FMath::Clamp(Value, 0.0f, 1.0f)); }
float SliderFromSensitivity(float Value) { return FMath::GetMappedRangeValueClamped(FVector2D(0.2f, 2.0f), FVector2D(0.0f, 1.0f), Value); }
float BrightnessFromSlider(float Value) { return FMath::Lerp(0.5f, 1.5f, FMath::Clamp(Value, 0.0f, 1.0f)); }
float SliderFromBrightness(float Value) { return FMath::GetMappedRangeValueClamped(FVector2D(0.5f, 1.5f), FVector2D(0.0f, 1.0f), Value); }
UTextBlock* ButtonLabel(UButton* Button) { return Button ? Cast<UTextBlock>(Button->GetContent()) : nullptr; }
FSlateBrush SettingsTabBrush(const FLinearColor& Color)
{
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Box;
	Brush.TintColor = FSlateColor(Color);
	Brush.Margin = FMargin(0.0f);
	return Brush;
}
void ApplySettingsTabStyle(UButton* Button, bool bActive)
{
	if (!Button)
	{
		return;
	}
	const FLinearColor NormalColor = bActive
		? FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("00000099")))
		: FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("10101099")));
	FButtonStyle Style;
	Style.SetNormal(SettingsTabBrush(NormalColor));
	Style.SetHovered(SettingsTabBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0xA6 / 255.0f)));
	Style.SetPressed(SettingsTabBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0xB3 / 255.0f)));
	Style.SetDisabled(SettingsTabBrush(FLinearColor(0.08f, 0.08f, 0.08f, 0.45f)));
	Button->SetStyle(Style);
}
UShowDownAudioSubsystem* AudioSubsystemFor(const UWidget* Widget)
{
	UGameInstance* GameInstance = Widget ? Widget->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UShowDownAudioSubsystem>() : nullptr;
}
}

TSharedRef<SWidget> UShowDownSettingsWidget::RebuildWidget()
{
	BuildLayout();
	return Super::RebuildWidget();
}

void UShowDownSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		PendingQuality = Settings->GetOverallScalabilityLevel();
		PendingWindowMode = Settings->GetFullscreenMode();
		bPendingVSync = Settings->IsVSyncEnabled();
		PendingResolution = Settings->GetScreenResolution();
		PendingPostProcess = Settings->ScalabilityQuality.PostProcessQuality;
		PendingEffects = Settings->ScalabilityQuality.EffectsQuality;
	}
	GConfig->GetString(UserSettingsSection, TEXT("CharacterName"), PendingCharacterName, GGameUserSettingsIni);
	GConfig->GetFloat(UserSettingsSection, TEXT("MouseSensitivity"), PendingMouseSensitivity, GGameUserSettingsIni);
	GConfig->GetFloat(UserSettingsSection, TEXT("Brightness"), PendingBrightness, GGameUserSettingsIni);
	GConfig->GetFloat(UserSettingsSection, TEXT("MasterVolume"), PendingMasterVolume, GGameUserSettingsIni);
	GConfig->GetFloat(UserSettingsSection, TEXT("MusicVolume"), PendingMusicVolume, GGameUserSettingsIni);
	GConfig->GetFloat(UserSettingsSection, TEXT("EffectVolume"), PendingEffectVolume, GGameUserSettingsIni);
	GConfig->GetFloat(UserSettingsSection, TEXT("DialogVolume"), PendingDialogVolume, GGameUserSettingsIni);
	if (UShowDownAudioSubsystem* AudioSubsystem = AudioSubsystemFor(this))
	{
		AudioSubsystem->SetUserMusicVolume(PendingMusicVolume);
		AudioSubsystem->SetUserEffectVolume(PendingEffectVolume);
	}
	if (EditableTextBox_CharacterName) EditableTextBox_CharacterName->SetText(FText::FromString(PendingCharacterName));
	if (Slider_MouseSensitivity) Slider_MouseSensitivity->SetValue(SliderFromSensitivity(PendingMouseSensitivity));
	if (Slider_Brightness) Slider_Brightness->SetValue(SliderFromBrightness(PendingBrightness));
	if (Slider_MasterVolume) Slider_MasterVolume->SetValue(PendingMasterVolume);
	if (Slider_MusicVolume) Slider_MusicVolume->SetValue(PendingMusicVolume);
	if (Slider_EffectVolume) Slider_EffectVolume->SetValue(PendingEffectVolume);
	if (Slider_DialogVolume) Slider_DialogVolume->SetValue(PendingDialogVolume);
	RefreshLabels();
	if (Button_ChangeNickname) Button_ChangeNickname->OnClicked.AddUniqueDynamic(this, &UShowDownSettingsWidget::HandleChangeNicknameClicked);
	if (Button_Quality) Button_Quality->OnClicked.AddUniqueDynamic(this, &UShowDownSettingsWidget::HandleQualityClicked);
	if (Button_WindowMode) Button_WindowMode->OnClicked.AddUniqueDynamic(this, &UShowDownSettingsWidget::HandleWindowModeClicked);
	if (Button_VSync) Button_VSync->OnClicked.AddUniqueDynamic(this, &UShowDownSettingsWidget::HandleVSyncClicked);
	if (Button_Resolution) Button_Resolution->OnClicked.AddUniqueDynamic(this, &UShowDownSettingsWidget::HandleResolutionClicked);
	if (Button_PostProcess) Button_PostProcess->OnClicked.AddUniqueDynamic(this, &UShowDownSettingsWidget::HandlePostProcessClicked);
	if (Button_Effects) Button_Effects->OnClicked.AddUniqueDynamic(this, &UShowDownSettingsWidget::HandleEffectsClicked);
	if (Slider_MouseSensitivity) Slider_MouseSensitivity->OnValueChanged.AddUniqueDynamic(this, &UShowDownSettingsWidget::HandleMouseSensitivityChanged);
	if (Slider_Brightness) Slider_Brightness->OnValueChanged.AddUniqueDynamic(this, &UShowDownSettingsWidget::HandleBrightnessChanged);
	if (Slider_MasterVolume) Slider_MasterVolume->OnValueChanged.AddUniqueDynamic(this, &UShowDownSettingsWidget::HandleMasterVolumeChanged);
	if (Slider_MusicVolume) Slider_MusicVolume->OnValueChanged.AddUniqueDynamic(this, &UShowDownSettingsWidget::HandleMusicVolumeChanged);
	if (Slider_EffectVolume) Slider_EffectVolume->OnValueChanged.AddUniqueDynamic(this, &UShowDownSettingsWidget::HandleEffectVolumeChanged);
	if (Slider_DialogVolume) Slider_DialogVolume->OnValueChanged.AddUniqueDynamic(this, &UShowDownSettingsWidget::HandleDialogVolumeChanged);
	if (Button_Apply) Button_Apply->OnClicked.AddUniqueDynamic(this, &UShowDownSettingsWidget::HandleApplyClicked);
	if (Button_Back) Button_Back->OnClicked.AddUniqueDynamic(this, &UShowDownSettingsWidget::HandleBackClicked);
	if (Button_Quit) Button_Quit->OnClicked.AddUniqueDynamic(this, &UShowDownSettingsWidget::HandleQuitClicked);
	if (Button_TabGeneral) Button_TabGeneral->OnClicked.AddUniqueDynamic(this, &UShowDownSettingsWidget::HandleGeneralTabClicked);
	if (Button_TabGraphics) Button_TabGraphics->OnClicked.AddUniqueDynamic(this, &UShowDownSettingsWidget::HandleGraphicsTabClicked);
	if (Button_TabSound) Button_TabSound->OnClicked.AddUniqueDynamic(this, &UShowDownSettingsWidget::HandleSoundTabClicked);
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (USupabaseSubsystem* SupabaseSubsystem = GameInstance->GetSubsystem<USupabaseSubsystem>())
		{
			SupabaseSubsystem->OnNicknameUpdated.AddUniqueDynamic(this, &UShowDownSettingsWidget::HandleNicknameUpdated);
			SupabaseSubsystem->OnPlayerDataLoaded.AddUniqueDynamic(this, &UShowDownSettingsWidget::HandlePlayerDataLoaded);
			RefreshNicknameEditor();
			SetNicknameUpdatePending(false);
			SetNicknameStatus(TEXT(""), FLinearColor::White);
		}
		else
		{
			if (EditableTextBox_Nickname) EditableTextBox_Nickname->SetIsEnabled(false);
			if (Button_ChangeNickname) Button_ChangeNickname->SetIsEnabled(false);
			SetNicknameStatus(TEXT("로그인 정보를 확인할 수 없습니다."), FLinearColor::Red);
		}
	}
	ShowSettingsPanel(Panel_General);
}

void UShowDownSettingsWidget::NativeDestruct()
{
	if (Button_ChangeNickname) Button_ChangeNickname->OnClicked.RemoveDynamic(this, &UShowDownSettingsWidget::HandleChangeNicknameClicked);
	if (Button_Quality) Button_Quality->OnClicked.RemoveDynamic(this, &UShowDownSettingsWidget::HandleQualityClicked);
	if (Button_WindowMode) Button_WindowMode->OnClicked.RemoveDynamic(this, &UShowDownSettingsWidget::HandleWindowModeClicked);
	if (Button_VSync) Button_VSync->OnClicked.RemoveDynamic(this, &UShowDownSettingsWidget::HandleVSyncClicked);
	if (Button_Resolution) Button_Resolution->OnClicked.RemoveDynamic(this, &UShowDownSettingsWidget::HandleResolutionClicked);
	if (Button_PostProcess) Button_PostProcess->OnClicked.RemoveDynamic(this, &UShowDownSettingsWidget::HandlePostProcessClicked);
	if (Button_Effects) Button_Effects->OnClicked.RemoveDynamic(this, &UShowDownSettingsWidget::HandleEffectsClicked);
	if (Slider_MouseSensitivity) Slider_MouseSensitivity->OnValueChanged.RemoveDynamic(this, &UShowDownSettingsWidget::HandleMouseSensitivityChanged);
	if (Slider_Brightness) Slider_Brightness->OnValueChanged.RemoveDynamic(this, &UShowDownSettingsWidget::HandleBrightnessChanged);
	if (Slider_MasterVolume) Slider_MasterVolume->OnValueChanged.RemoveDynamic(this, &UShowDownSettingsWidget::HandleMasterVolumeChanged);
	if (Slider_MusicVolume) Slider_MusicVolume->OnValueChanged.RemoveDynamic(this, &UShowDownSettingsWidget::HandleMusicVolumeChanged);
	if (Slider_EffectVolume) Slider_EffectVolume->OnValueChanged.RemoveDynamic(this, &UShowDownSettingsWidget::HandleEffectVolumeChanged);
	if (Slider_DialogVolume) Slider_DialogVolume->OnValueChanged.RemoveDynamic(this, &UShowDownSettingsWidget::HandleDialogVolumeChanged);
	if (Button_Apply) Button_Apply->OnClicked.RemoveDynamic(this, &UShowDownSettingsWidget::HandleApplyClicked);
	if (Button_Back) Button_Back->OnClicked.RemoveDynamic(this, &UShowDownSettingsWidget::HandleBackClicked);
	if (Button_Quit) Button_Quit->OnClicked.RemoveDynamic(this, &UShowDownSettingsWidget::HandleQuitClicked);
	if (Button_TabGeneral) Button_TabGeneral->OnClicked.RemoveDynamic(this, &UShowDownSettingsWidget::HandleGeneralTabClicked);
	if (Button_TabGraphics) Button_TabGraphics->OnClicked.RemoveDynamic(this, &UShowDownSettingsWidget::HandleGraphicsTabClicked);
	if (Button_TabSound) Button_TabSound->OnClicked.RemoveDynamic(this, &UShowDownSettingsWidget::HandleSoundTabClicked);
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (USupabaseSubsystem* SupabaseSubsystem = GameInstance->GetSubsystem<USupabaseSubsystem>())
		{
			SupabaseSubsystem->OnNicknameUpdated.RemoveDynamic(this, &UShowDownSettingsWidget::HandleNicknameUpdated);
			SupabaseSubsystem->OnPlayerDataLoaded.RemoveDynamic(this, &UShowDownSettingsWidget::HandlePlayerDataLoaded);
		}
	}
	Super::NativeDestruct();
}

void UShowDownSettingsWidget::BuildLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget) return;
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SettingsRoot"));
	WidgetTree->RootWidget = Root;
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SettingsPanel"));
	Panel->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.68f));
	Panel->SetPadding(FMargin(36.0f, 30.0f));
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	PanelSlot->SetSize(FVector2D(560.0f, 590.0f));

	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Stack);

	Button_Quality = CreateButton(TEXT(""), Text_Quality, FLinearColor(0.04f, 0.06f, 0.07f, 0.95f));
	Button_WindowMode = CreateButton(TEXT(""), Text_WindowMode, FLinearColor(0.04f, 0.06f, 0.07f, 0.95f));
	Button_VSync = CreateButton(TEXT(""), Text_VSync, FLinearColor(0.04f, 0.06f, 0.07f, 0.95f));
	for (UButton* Button : {Button_Quality, Button_WindowMode, Button_VSync})
	{
		Stack->AddChildToVerticalBox(Button)->SetPadding(FMargin(0, 0, 0, 12));
	}
	Button_Apply = CreateButton(TEXT("APPLY"), Text_Status, FLinearColor(0.12f, 0.68f, 0.78f, 1.0f));
	Stack->AddChildToVerticalBox(Button_Apply)->SetPadding(FMargin(0, 12, 0, 12));
	Button_Back = CreateButton(TEXT("BACK"), Text_Status, FLinearColor(0.08f, 0.10f, 0.11f, 1.0f));
	Stack->AddChildToVerticalBox(Button_Back)->SetPadding(FMargin(0, 0, 0, 12));
	Button_Quit = CreateButton(TEXT("QUIT GAME"), Text_Status, FLinearColor(0.42f, 0.05f, 0.07f, 1.0f));
	Stack->AddChildToVerticalBox(Button_Quit);
	Text_Status = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Status"));
	Text_Status->SetJustification(ETextJustify::Center);
	Text_Status->SetColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.75f, 0.77f, 1.0f)));
	Stack->AddChildToVerticalBox(Text_Status)->SetPadding(FMargin(0, 16, 0, 0));
}

UButton* UShowDownSettingsWidget::CreateButton(const FString& Label, UTextBlock*& OutLabel, const FLinearColor& Color)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>();
	FSlateBrush Normal; Normal.DrawAs = ESlateBrushDrawType::Box; Normal.TintColor = FSlateColor(FLinearColor(0,0,0,Color.A)); Normal.Margin = FMargin(0);
	FSlateBrush Hovered = Normal; Hovered.TintColor = FSlateColor(FLinearColor(0,0,0,FMath::Min(Color.A + 0.06f, 1.0f)));
	FSlateBrush Pressed = Normal; Pressed.TintColor = FSlateColor(FLinearColor(0,0,0,FMath::Min(Color.A + 0.12f, 1.0f)));
	FButtonStyle Style; Style.SetNormal(Normal).SetHovered(Hovered).SetPressed(Pressed).SetDisabled(Normal); Button->SetStyle(Style);
	OutLabel = WidgetTree->ConstructWidget<UTextBlock>();
	OutLabel->SetText(FText::FromString(Label));
	OutLabel->SetJustification(ETextJustify::Center);
	OutLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	OutLabel->SetFont(FSlateFontInfo(LoadObject<UObject>(nullptr, TEXT("/Game/UI/Font/Pretendard/static/Pretendard-Regular_Font.Pretendard-Regular_Font")), 18));
	Button->SetContent(OutLabel);
	return Button;
}

void UShowDownSettingsWidget::RefreshLabels()
{
	static const TCHAR* OverallQualityNames[] = { TEXT("낮음"), TEXT("중간"), TEXT("높음"), TEXT("최상") };
	const FText QualityLabel = FText::FromString(OverallQualityNames[FMath::Clamp(PendingQuality, 0, 3)]);
	const TCHAR* Mode = PendingWindowMode == EWindowMode::Fullscreen
		? TEXT("전체 화면")
		: PendingWindowMode == EWindowMode::Windowed
			? TEXT("창 모드")
			: TEXT("테두리 없는 창");
	const FText WindowModeLabel = FText::FromString(Mode);
	const FText VSyncLabel = FText::FromString(bPendingVSync ? TEXT("켜짐") : TEXT("꺼짐"));
	static const TCHAR* QualityNames[] = { TEXT("하"), TEXT("중"), TEXT("상"), TEXT("최상") };
	const FText ResolutionLabel = FText::FromString(FString::Printf(TEXT("%dx%d @60Hz"), PendingResolution.X, PendingResolution.Y));
	const FText BrightnessLabel = FText::FromString(FString::Printf(TEXT("%.1f"), PendingBrightness));
	const FText PostProcessLabel = FText::FromString(QualityNames[FMath::Clamp(PendingPostProcess,0,3)]);
	const FText EffectsLabel = FText::FromString(QualityNames[FMath::Clamp(PendingEffects,0,3)]);
	if (Text_Quality) Text_Quality->SetText(QualityLabel);
	if (Text_WindowMode) Text_WindowMode->SetText(WindowModeLabel);
	if (Text_VSync) Text_VSync->SetText(VSyncLabel);
	if (Text_Resolution) Text_Resolution->SetText(ResolutionLabel);
	if (Text_Brightness) Text_Brightness->SetText(BrightnessLabel);
	if (Text_PostProcess) Text_PostProcess->SetText(PostProcessLabel);
	if (Text_Effects) Text_Effects->SetText(EffectsLabel);
	if (Text_MouseSensitivity) Text_MouseSensitivity->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), PendingMouseSensitivity)));
	if (Text_MasterVolume) Text_MasterVolume->SetText(FText::AsNumber(FMath::RoundToInt(PendingMasterVolume*100.0f)));
	if (Text_MusicVolume) Text_MusicVolume->SetText(FText::AsNumber(FMath::RoundToInt(PendingMusicVolume*100.0f)));
	if (Text_EffectVolume) Text_EffectVolume->SetText(FText::AsNumber(FMath::RoundToInt(PendingEffectVolume*100.0f)));
	if (Text_DialogVolume) Text_DialogVolume->SetText(FText::AsNumber(FMath::RoundToInt(PendingDialogVolume*100.0f)));
	if (Button_Quality) if (UTextBlock* Label = Cast<UTextBlock>(Button_Quality->GetContent())) Label->SetText(QualityLabel);
	if (Button_WindowMode) if (UTextBlock* Label = Cast<UTextBlock>(Button_WindowMode->GetContent())) Label->SetText(WindowModeLabel);
	if (Button_VSync) if (UTextBlock* Label = Cast<UTextBlock>(Button_VSync->GetContent())) Label->SetText(VSyncLabel);
	if (UTextBlock* Label=ButtonLabel(Button_Resolution)) Label->SetText(ResolutionLabel);
	if (UTextBlock* Label=ButtonLabel(Button_Brightness)) Label->SetText(BrightnessLabel);
	if (UTextBlock* Label=ButtonLabel(Button_PostProcess)) Label->SetText(PostProcessLabel);
	if (UTextBlock* Label=ButtonLabel(Button_Effects)) Label->SetText(EffectsLabel);
}

void UShowDownSettingsWidget::RefreshNicknameEditor()
{
	if (!EditableTextBox_Nickname)
	{
		return;
	}

	USupabaseSubsystem* SupabaseSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<USupabaseSubsystem>()
		: nullptr;
	const FString CurrentNickname = SupabaseSubsystem ? SupabaseSubsystem->GetNickname() : FString();
	EditableTextBox_Nickname->SetText(FText::FromString(CurrentNickname));
	EditableTextBox_Nickname->SetHintText(FText::FromString(
		CurrentNickname.IsEmpty() ? TEXT("2~16자 닉네임") : CurrentNickname));
}

void UShowDownSettingsWidget::SetNicknameUpdatePending(bool bPending)
{
	bNicknameUpdatePending = bPending;
	if (EditableTextBox_Nickname)
	{
		EditableTextBox_Nickname->SetIsEnabled(!bPending);
	}
	if (Button_ChangeNickname)
	{
		Button_ChangeNickname->SetIsEnabled(!bPending);
		if (UTextBlock* Label = Cast<UTextBlock>(Button_ChangeNickname->GetContent()))
		{
			Label->SetText(FText::FromString(bPending ? TEXT("변경 중") : TEXT("변경")));
		}
	}
}

void UShowDownSettingsWidget::SetNicknameStatus(const FString& Message, const FLinearColor& Color)
{
	if (Text_NicknameStatus)
	{
		Text_NicknameStatus->SetText(FText::FromString(Message));
		Text_NicknameStatus->SetColorAndOpacity(FSlateColor(Color));
	}
}

void UShowDownSettingsWidget::HandleQualityClicked() { PendingQuality = (PendingQuality + 1) % 4; RefreshLabels(); }
void UShowDownSettingsWidget::HandleWindowModeClicked() { PendingWindowMode = PendingWindowMode == EWindowMode::WindowedFullscreen ? EWindowMode::Fullscreen : PendingWindowMode == EWindowMode::Fullscreen ? EWindowMode::Windowed : EWindowMode::WindowedFullscreen; RefreshLabels(); }
void UShowDownSettingsWidget::HandleVSyncClicked() { bPendingVSync = !bPendingVSync; RefreshLabels(); }
void UShowDownSettingsWidget::HandleResolutionClicked()
{
	static const FIntPoint Options[] = { {1280,720}, {1600,900}, {1920,1080}, {2560,1440}, {3840,2160} };
	int32 Index=0; for(int32 I=0;I<UE_ARRAY_COUNT(Options);++I) if(Options[I]==PendingResolution){Index=I;break;}
	PendingResolution=Options[(Index+1)%UE_ARRAY_COUNT(Options)]; RefreshLabels();
}
void UShowDownSettingsWidget::HandleBrightnessChanged(float Value)
{
	PendingBrightness = BrightnessFromSlider(Value);
	RefreshLabels();
}
void UShowDownSettingsWidget::HandlePostProcessClicked() { PendingPostProcess=(PendingPostProcess+1)%4; RefreshLabels(); }
void UShowDownSettingsWidget::HandleEffectsClicked() { PendingEffects=(PendingEffects+1)%4; RefreshLabels(); }
void UShowDownSettingsWidget::HandleMouseSensitivityChanged(float Value) { PendingMouseSensitivity=SensitivityFromSlider(Value); RefreshLabels(); }
void UShowDownSettingsWidget::HandleMasterVolumeChanged(float Value) { PendingMasterVolume=Value; RefreshLabels(); if(UWorld* World=GetWorld()){FAudioDeviceHandle Device=World->GetAudioDevice(); if(Device.IsValid()) Device->SetTransientPrimaryVolume(Value);} }
void UShowDownSettingsWidget::HandleMusicVolumeChanged(float Value) { PendingMusicVolume=Value; RefreshLabels(); if(UShowDownAudioSubsystem* AudioSubsystem=AudioSubsystemFor(this)) AudioSubsystem->SetUserMusicVolume(Value); }
void UShowDownSettingsWidget::HandleEffectVolumeChanged(float Value) { PendingEffectVolume=Value; RefreshLabels(); if(UShowDownAudioSubsystem* AudioSubsystem=AudioSubsystemFor(this)) AudioSubsystem->SetUserEffectVolume(Value); }
void UShowDownSettingsWidget::HandleDialogVolumeChanged(float Value) { PendingDialogVolume=Value; RefreshLabels(); }
void UShowDownSettingsWidget::HandleApplyClicked()
{
	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		Settings->SetOverallScalabilityLevel(PendingQuality);
		Settings->SetFullscreenMode(PendingWindowMode);
		Settings->SetVSyncEnabled(bPendingVSync);
		Settings->SetScreenResolution(PendingResolution);
		Settings->ScalabilityQuality.SetPostProcessQuality(PendingPostProcess);
		Settings->ScalabilityQuality.SetEffectsQuality(PendingEffects);
		Settings->ApplySettings(false);
		Settings->SaveSettings();
		PendingCharacterName = EditableTextBox_CharacterName ? EditableTextBox_CharacterName->GetText().ToString().TrimStartAndEnd() : PendingCharacterName;
		if(PendingCharacterName.IsEmpty()) PendingCharacterName=TEXT("상대");
		GConfig->SetString(UserSettingsSection,TEXT("CharacterName"),*PendingCharacterName,GGameUserSettingsIni);
		GConfig->SetFloat(UserSettingsSection,TEXT("MouseSensitivity"),PendingMouseSensitivity,GGameUserSettingsIni);
		GConfig->SetFloat(UserSettingsSection,TEXT("Brightness"),PendingBrightness,GGameUserSettingsIni);
		GConfig->SetFloat(UserSettingsSection,TEXT("MasterVolume"),PendingMasterVolume,GGameUserSettingsIni);
		GConfig->SetFloat(UserSettingsSection,TEXT("MusicVolume"),PendingMusicVolume,GGameUserSettingsIni);
		GConfig->SetFloat(UserSettingsSection,TEXT("EffectVolume"),PendingEffectVolume,GGameUserSettingsIni);
		GConfig->SetFloat(UserSettingsSection,TEXT("DialogVolume"),PendingDialogVolume,GGameUserSettingsIni);
		GConfig->Flush(false,GGameUserSettingsIni);
		if(AShowDownPlayerController* Controller=Cast<AShowDownPlayerController>(GetOwningPlayer()))
		{
			Controller->SetUserBrightness(PendingBrightness);
			Controller->SetUserMouseSensitivity(PendingMouseSensitivity);
		}
		if (Text_Status) Text_Status->SetText(FText::FromString(TEXT("Settings applied")));
	}
}
void UShowDownSettingsWidget::HandleBackClicked() { HandleApplyClicked(); OnBackRequested.Broadcast(); }
void UShowDownSettingsWidget::HandleQuitClicked() { HandleApplyClicked(); OnQuitRequested.Broadcast(); }
void UShowDownSettingsWidget::HandleGeneralTabClicked() { ShowSettingsPanel(Panel_General); }
void UShowDownSettingsWidget::HandleGraphicsTabClicked() { ShowSettingsPanel(Panel_Graphics); }
void UShowDownSettingsWidget::HandleSoundTabClicked() { ShowSettingsPanel(Panel_Sound); }

void UShowDownSettingsWidget::HandleChangeNicknameClicked()
{
	if (bNicknameUpdatePending)
	{
		return;
	}

	const FString NewNickname = EditableTextBox_Nickname
		? EditableTextBox_Nickname->GetText().ToString().TrimStartAndEnd()
		: FString();
	if (NewNickname.Len() < 2 || NewNickname.Len() > 16)
	{
		SetNicknameStatus(TEXT("닉네임은 2~16자로 입력해 주세요."), FLinearColor::Red);
		return;
	}

	USupabaseSubsystem* SupabaseSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<USupabaseSubsystem>()
		: nullptr;
	if (!SupabaseSubsystem)
	{
		SetNicknameStatus(TEXT("로그인 정보를 확인할 수 없습니다."), FLinearColor::Red);
		return;
	}
	if (NewNickname == SupabaseSubsystem->GetNickname())
	{
		SetNicknameStatus(TEXT("현재 닉네임과 같습니다."), FLinearColor::Yellow);
		return;
	}

	SetNicknameUpdatePending(true);
	SetNicknameStatus(TEXT("닉네임 변경 중..."), FLinearColor::Yellow);
	SupabaseSubsystem->UpdateNickname(NewNickname);
}

void UShowDownSettingsWidget::HandleNicknameUpdated(bool bSuccess, const FString& Message)
{
	if (!bSuccess && Message == TEXT("Updating nickname..."))
	{
		SetNicknameUpdatePending(true);
		SetNicknameStatus(TEXT("닉네임 변경 중..."), FLinearColor::Yellow);
		return;
	}

	SetNicknameUpdatePending(false);
	if (bSuccess)
	{
		RefreshNicknameEditor();
		SetNicknameStatus(TEXT("닉네임이 변경되었습니다."), FLinearColor::Green);
		return;
	}

	const FString ErrorMessage = Message == TEXT("Nickname must be 2-16 characters.")
		? TEXT("닉네임은 2~16자로 입력해 주세요.")
		: Message == TEXT("Access token is empty.") || Message == TEXT("User id is empty.")
			? TEXT("로그인 정보를 확인할 수 없습니다.")
			: TEXT("닉네임 변경에 실패했습니다.");
	SetNicknameStatus(ErrorMessage, FLinearColor::Red);
}

void UShowDownSettingsWidget::HandlePlayerDataLoaded(bool bSuccess, const FString& Message)
{
	(void)Message;
	if (bSuccess && !bNicknameUpdatePending && EditableTextBox_Nickname && !EditableTextBox_Nickname->HasKeyboardFocus())
	{
		RefreshNicknameEditor();
	}
}

void UShowDownSettingsWidget::ShowSettingsPanel(UCanvasPanel* PanelToShow)
{
	if (Panel_General) Panel_General->SetVisibility(PanelToShow == Panel_General ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (Panel_Graphics) Panel_Graphics->SetVisibility(PanelToShow == Panel_Graphics ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (Panel_Sound) Panel_Sound->SetVisibility(PanelToShow == Panel_Sound ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	ApplySettingsTabStyle(Button_TabGeneral, PanelToShow == Panel_General);
	ApplySettingsTabStyle(Button_TabGraphics, PanelToShow == Panel_Graphics);
	ApplySettingsTabStyle(Button_TabSound, PanelToShow == Panel_Sound);
}
