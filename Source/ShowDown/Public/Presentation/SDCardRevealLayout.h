#pragma once

#include "CoreMinimal.h"

namespace ShowDownCardRevealLayout
{
	/**
	 * Converts the configured reveal-card spacing into the radial distance used
	 * by multiplayer. Two opposite cards split the spacing around table center,
	 * so their final center-to-center gap matches the single-player layout.
	 */
	SHOWDOWN_API float ResolveRadialCenterDistance(float CardSpacing, int32 CardCount);

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
