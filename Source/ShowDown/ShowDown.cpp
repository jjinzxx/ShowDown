// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShowDown.h"
#include "ShowDownLoadingScreen.h"
#include "MoviePlayer.h"
#include "Modules/ModuleManager.h"

class FShowDownGameModule final : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();
		if (IsMoviePlayerEnabled())
		{
			GetMoviePlayer()->OnPrepareLoadingScreen().AddRaw(this, &FShowDownGameModule::PrepareFallbackLoadingScreen);
		}
	}

	virtual void ShutdownModule() override
	{
		if (IsMoviePlayerEnabled())
		{
			GetMoviePlayer()->OnPrepareLoadingScreen().RemoveAll(this);
		}
		FDefaultGameModuleImpl::ShutdownModule();
	}

private:
	void PrepareFallbackLoadingScreen()
	{
		ShowDownLoadingScreen::PrepareFallback();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FShowDownGameModule, ShowDown, "ShowDown");
