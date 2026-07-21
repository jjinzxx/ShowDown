#include "ShowDownLoadingScreen.h"

#include "MoviePlayer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
void SetupLoadingScreen(const FString& Title, const FString& Detail)
{
	if (IsRunningDedicatedServer() || !IsMoviePlayerEnabled())
	{
		return;
	}

	const FText TitleText = FText::FromString(Title.IsEmpty() ? TEXT("LOADING SHOWDOWN") : Title);
	const FText DetailText = FText::FromString(Detail.IsEmpty() ? TEXT("잠시만 기다려 주세요.") : Detail);

	FLoadingScreenAttributes Attributes;
	Attributes.MinimumLoadingScreenDisplayTime = 0.2f;
	Attributes.bAutoCompleteWhenLoadingCompletes = true;
	Attributes.bMoviesAreSkippable = false;
	Attributes.bWaitForManualStop = false;
	Attributes.bAllowEngineTick = false;
	Attributes.WidgetLoadingScreen =
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.008f, 0.012f, 0.016f, 1.0f))
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(24.0f)
			[
				SNew(STextBlock)
				.Text(TitleText)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 30))
				.ColorAndOpacity(FLinearColor(1.0f, 0.78f, 0.34f, 1.0f))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(12.0f)
			[
				SNew(SCircularThrobber)
				.NumPieces(12)
				.Period(0.7f)
				.Radius(22.0f)
				.ColorAndOpacity(FLinearColor(0.93f, 0.96f, 1.0f, 1.0f))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(24.0f)
			[
				SNew(STextBlock)
				.Text(DetailText)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
				.ColorAndOpacity(FLinearColor(0.72f, 0.78f, 0.82f, 1.0f))
			]
		];

	GetMoviePlayer()->SetupLoadingScreen(Attributes);
}
}

void ShowDownLoadingScreen::Prepare(const FString& Title, const FString& Detail)
{
	SetupLoadingScreen(Title, Detail);
}

void ShowDownLoadingScreen::PrepareFallback()
{
	SetupLoadingScreen(
		TEXT("SHOWDOWN 불러오는 중"),
		TEXT("네트워크 월드와 게임 리소스를 준비하고 있습니다."));
}
