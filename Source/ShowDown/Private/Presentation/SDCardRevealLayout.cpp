#include "Presentation/SDCardRevealLayout.h"

float ShowDownCardRevealLayout::ResolveRadialCenterDistance(
	float CardSpacing,
	const TArray<FVector>& SeatDirections)
{
	const float ClampedSpacing = FMath::Max(0.0f, CardSpacing);
	if (ClampedSpacing <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	TArray<FVector> UniqueDirections;
	for (FVector Direction : SeatDirections)
	{
		Direction.Z = 0.0f;
		Direction = Direction.GetSafeNormal();
		if (Direction.IsNearlyZero())
		{
			continue;
		}

		const bool bAlreadyAdded = UniqueDirections.ContainsByPredicate([Direction](const FVector& Existing)
		{
			return Existing.Equals(Direction, 0.01f);
		});
		if (!bAlreadyAdded)
		{
			UniqueDirections.Add(Direction);
		}
	}

	if (UniqueDirections.Num() <= 1)
	{
		return 0.0f;
	}

	float MinimumChord = TNumericLimits<float>::Max();
	for (int32 LeftIndex = 0; LeftIndex < UniqueDirections.Num(); ++LeftIndex)
	{
		for (int32 RightIndex = LeftIndex + 1; RightIndex < UniqueDirections.Num(); ++RightIndex)
		{
			MinimumChord = FMath::Min(
				MinimumChord,
				FVector::Distance(UniqueDirections[LeftIndex], UniqueDirections[RightIndex]));
		}
	}

	if (!FMath::IsFinite(MinimumChord) || MinimumChord <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	// Pathological near-duplicate seat directions must not push cards arbitrarily
	// far from the table. Authored 2-4 player seats have chords >= sqrt(2), so
	// this compactness guard does not alter the intended layout.
	const float SafeChord = FMath::Max(1.0f, MinimumChord);
	return ClampedSpacing / SafeChord;
}

bool ShowDownCardRevealLayout::TryBuildRadialTransform(
	const FVector& TableCenter,
	const FVector& SeatLocation,
	float CenterDistance,
	float TableYawOffset,
	float HeightOffset,
	const FRotator& RotationOffset,
	const FVector& Scale,
	FTransform& OutTransform)
{
	FVector CenterToSeat = SeatLocation - TableCenter;
	CenterToSeat.Z = 0.0f;
	const FVector RadialDirection = CenterToSeat.GetSafeNormal();
	if (RadialDirection.IsNearlyZero())
	{
		return false;
	}

	const float RadialDistance = FMath::Max(0.0f, CenterDistance);
	const FVector RevealLocation =
		TableCenter
		+ RadialDirection * RadialDistance
		+ FVector::UpVector * HeightOffset;
	const FRotator SeatRotation(
		0.0f,
		RadialDirection.Rotation().Yaw + TableYawOffset,
		0.0f);
	const FQuat RevealRotation =
		(SeatRotation.Quaternion() * RotationOffset.Quaternion()).GetNormalized();

	OutTransform = FTransform(RevealRotation, RevealLocation, Scale);
	return true;
}
