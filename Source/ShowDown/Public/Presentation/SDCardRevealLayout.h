#pragma once

#include "CoreMinimal.h"

namespace ShowDownCardRevealLayout
{
	/**
	 * Builds a flat reveal transform on the radial line from the table center to a seat.
	 * CenterDistance is the shared distance from the table center, not spacing between cards.
	 */
	SHOWDOWN_API bool TryBuildRadialTransform(
		const FVector& TableCenter,
		const FVector& SeatLocation,
		float CenterDistance,
		float TableYawOffset,
		float HeightOffset,
		const FRotator& RotationOffset,
		const FVector& Scale,
		FTransform& OutTransform);
}
