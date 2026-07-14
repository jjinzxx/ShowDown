#pragma once

#include "CoreMinimal.h"

/** Loading-thread-safe Slate screen used while OpenLevel/ClientTravel blocks UMG. */
namespace ShowDownLoadingScreen
{
	SHOWDOWN_API void Prepare(const FString& Title, const FString& Detail);
	SHOWDOWN_API void PrepareFallback();
}
