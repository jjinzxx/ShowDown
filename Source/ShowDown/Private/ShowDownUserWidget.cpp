#include "ShowDownUserWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Audio/ShowDownAudioSubsystem.h"
#include "Sound/SlateSound.h"
#include "Sound/SoundWave.h"
#include "TimerManager.h"

void UShowDownUserWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyButtonClickSound();

	// A few native fallback widgets build their tree after calling Super from
	// their own NativeConstruct. Rescan on the next tick so those late-created
	// buttons receive the same sound without requiring per-widget audio code.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateWeakLambda(
				this,
				[this]()
				{
					ApplyButtonClickSound();
				}));
	}
}

void UShowDownUserWidget::ApplyButtonClickSound()
{
	UGameInstance* GameInstance = GetGameInstance();
	UShowDownAudioSubsystem* AudioSubsystem = GameInstance
		? GameInstance->GetSubsystem<UShowDownAudioSubsystem>()
		: nullptr;
	USoundWave* ButtonClickSound = AudioSubsystem
		? AudioSubsystem->GetButtonClickSound()
		: nullptr;
	if (!WidgetTree || !ButtonClickSound)
	{
		return;
	}

	FSlateSound ClickedSlateSound;
	ClickedSlateSound.SetResourceObject(ButtonClickSound);
	WidgetTree->ForEachWidgetAndDescendants(
		[&ClickedSlateSound](UWidget* Widget)
		{
			UButton* Button = Cast<UButton>(Widget);
			if (!Button)
			{
				return;
			}

			FButtonStyle Style = Button->GetStyle();
			// SButton plays Pressed and Clicked sounds at different points in
			// the same interaction. Keep only the completed-click cue so one
			// click can never produce the sound twice.
			Style.SetPressedSound(FSlateSound());
			Style.SetClickedSound(ClickedSlateSound);
			Button->SetStyle(Style);
		});
}
