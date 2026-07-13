#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ShowDownUiBuildCommandlet.generated.h"

UCLASS()
class SHOWDOWN_API UShowDownUiBuildCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	virtual int32 Main(const FString& Params) override;
};
