#pragma once

#include "CoreMinimal.h"

namespace ShowDownCardRevealLayout
{
	/**
	 * Converts the configured neighboring-card gap into a compact multiplayer
	 * radius. SeatDirections are planar directions from table center to each
	 * revealed player's seat. The nearest valid pair keeps the configured gap,
	 * independent of whether the active seats are adjacent or opposite.
	 */
	SHOWDOWN_API float ResolveRadialCenterDistance(
		float CardSpacing,
		const TArray<FVector>& SeatDirections);

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
