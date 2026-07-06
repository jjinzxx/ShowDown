#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"

namespace ShowDownCameraAspect
{
	inline constexpr float Forced16By9AspectRatio = 16.0f / 9.0f;

	inline bool NeedsForced16By9(const UCameraComponent* CameraComponent)
	{
		return CameraComponent
			&& (!CameraComponent->bConstrainAspectRatio
				|| !FMath::IsNearlyEqual(CameraComponent->AspectRatio, Forced16By9AspectRatio, 0.001f));
	}

	inline void ApplyForced16By9(UCameraComponent* CameraComponent)
	{
		if (!CameraComponent)
		{
			return;
		}

		CameraComponent->AspectRatio = Forced16By9AspectRatio;
		CameraComponent->bConstrainAspectRatio = true;
	}

	inline void ApplyForced16By9(ACameraActor* CameraActor)
	{
		ApplyForced16By9(CameraActor ? CameraActor->GetCameraComponent() : nullptr);
	}
}
