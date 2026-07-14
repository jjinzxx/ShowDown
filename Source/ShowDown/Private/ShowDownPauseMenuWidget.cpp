#include "ShowDownPauseMenuWidget.h"
#include "Components/Button.h"
void UShowDownPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if(Button_Resume) Button_Resume->OnClicked.AddUniqueDynamic(this,&UShowDownPauseMenuWidget::Resume);
	if(Button_MainMenu) Button_MainMenu->OnClicked.AddUniqueDynamic(this,&UShowDownPauseMenuWidget::MainMenu);
	if(Button_Settings) Button_Settings->OnClicked.AddUniqueDynamic(this,&UShowDownPauseMenuWidget::Settings);
	if(Button_Quit) Button_Quit->OnClicked.AddUniqueDynamic(this,&UShowDownPauseMenuWidget::Quit);
}
void UShowDownPauseMenuWidget::NativeDestruct(){if(Button_Resume)Button_Resume->OnClicked.RemoveDynamic(this,&UShowDownPauseMenuWidget::Resume);if(Button_MainMenu)Button_MainMenu->OnClicked.RemoveDynamic(this,&UShowDownPauseMenuWidget::MainMenu);if(Button_Settings)Button_Settings->OnClicked.RemoveDynamic(this,&UShowDownPauseMenuWidget::Settings);if(Button_Quit)Button_Quit->OnClicked.RemoveDynamic(this,&UShowDownPauseMenuWidget::Quit);Super::NativeDestruct();}
