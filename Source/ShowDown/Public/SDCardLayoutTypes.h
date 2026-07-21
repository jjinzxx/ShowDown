#pragma once

#include "CoreMinimal.h"
#include "SDCardLayoutTypes.generated.h"

USTRUCT(BlueprintType)
struct FSDCardHandLayoutSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Hand Layout", meta = (ClampMin = "0.0"))
	float CardSpacing = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Hand Layout", meta = (ClampMin = "0.0"))
	float ForwardOffset = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Hand Layout", meta = (ClampMin = "0.0"))
	float HeightOffset = 65.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Hand Layout", meta = (ClampMin = "-45.0", ClampMax = "45.0"))
	float LeanAngle = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Hand Layout", meta = (ClampMin = "0.0"))
	float LayerStep = 0.5f;
};
