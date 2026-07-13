#include "Presentation/SDCardRevealLayout.h"

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
