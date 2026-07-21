#include "Presentation/SDCameraDirector.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "ShowDownCameraAspect.h"

ASDCameraDirector::ASDCameraDirector()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ASDCameraDirector::BeginPlay()
{
	Super::BeginPlay();

	if (bPlayInitialShotOnBeginPlay && !InitialShotId.IsNone())
	{
		CutToShot(InitialShotId);
	}
}

void ASDCameraDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateFOVBlend(DeltaSeconds);
	UpdateShotMouseLook();
}

bool ASDCameraDirector::PlayShot(FName ShotId)
{
	return ApplyShot(ShotId, false);
}

bool ASDCameraDirector::CutToShot(FName ShotId)
{
	return ApplyShot(ShotId, true);
}

bool ASDCameraDirector::ApplyShot(FName ShotId, bool bCut)
{
	const FSDCameraShot* Shot = FindShot(ShotId);
	if (!Shot || !Shot->Camera)
	{
		UE_LOG(LogTemp, Warning, TEXT("SDCameraDirector: Shot '%s' is missing or has no camera."), *ShotId.ToString());
		return false;
	}

	APlayerController* PlayerController = GetLocalPlayerController();
	if (!PlayerController)
	{
		UE_LOG(LogTemp, Warning, TEXT("SDCameraDirector: PlayerController not found."));
		return false;
	}

	if (!GameplayViewTarget)
	{
		GameplayViewTarget = PlayerController->GetViewTarget();
	}

	ActiveShotId = ShotId;
	ActiveCamera = Shot->Camera;
	ShowDownCameraAspect::ApplyForced16By9(ActiveCamera);
	ActiveShotBaseRotation = ActiveCamera->GetActorRotation();
	bForceActiveMouseLook = Shot->bEnableMouseLook;

	if (bCut || Shot->BlendTime <= 0.0f)
	{
		PlayerController->SetViewTarget(ActiveCamera);
	}
	else
	{
		PlayerController->SetViewTargetWithBlend(
			ActiveCamera,
			Shot->BlendTime,
			Shot->BlendFunction,
			Shot->BlendExponent,
			false);
	}

	ApplyShotInputMode(*Shot);
	StartFOVBlend(*Shot);

	if (Shot->CameraShakeClass && PlayerController->PlayerCameraManager)
	{
		PlayerController->PlayerCameraManager->StartCameraShake(Shot->CameraShakeClass, Shot->CameraShakeScale);
	}

	return true;
}

void ASDCameraDirector::ReturnToGameplayCamera(float BlendTime)
{
	APlayerController* PlayerController = GetLocalPlayerController();
	if (!PlayerController || !GameplayViewTarget)
	{
		ClearActiveShot();
		return;
	}

	if (BlendTime <= 0.0f)
	{
		if (ACameraActor* CameraActor = Cast<ACameraActor>(GameplayViewTarget))
		{
			ShowDownCameraAspect::ApplyForced16By9(CameraActor);
		}
		PlayerController->SetViewTarget(GameplayViewTarget);
	}
	else
	{
		if (ACameraActor* CameraActor = Cast<ACameraActor>(GameplayViewTarget))
		{
			ShowDownCameraAspect::ApplyForced16By9(CameraActor);
		}
		PlayerController->SetViewTargetWithBlend(GameplayViewTarget, BlendTime);
	}

	ClearActiveShot();
}

void ASDCameraDirector::ClearActiveShot()
{
	ActiveShotId = NAME_None;
	ActiveCamera = nullptr;
	GameplayViewTarget = nullptr;
	bForceActiveMouseLook = false;
	bFOVBlendActive = false;
}

void ASDCameraDirector::SetActiveMouseLookEnabled(bool bEnabled)
{
	bForceActiveMouseLook = bEnabled;
}

bool ASDCameraDirector::HasShot(FName ShotId) const
{
	return FindShot(ShotId) != nullptr;
}

const FSDCameraShot* ASDCameraDirector::FindShot(FName ShotId) const
{
	for (const FSDCameraShot& Shot : Shots)
	{
		if (Shot.ShotId == ShotId)
		{
			return &Shot;
		}
	}

	return nullptr;
}

void ASDCameraDirector::ApplyShotInputMode(const FSDCameraShot& Shot) const
{
	APlayerController* PlayerController = GetLocalPlayerController();
	if (!PlayerController)
	{
		return;
	}

	PlayerController->SetIgnoreMoveInput(Shot.bLockMoveInput);
	PlayerController->SetIgnoreLookInput(Shot.bLockLookInput);
	PlayerController->bShowMouseCursor = Shot.bShowMouseCursor;

	if (Shot.bShowMouseCursor)
	{
		FInputModeGameAndUI InputMode;
		PlayerController->SetInputMode(InputMode);
	}
	else
	{
		FInputModeGameOnly InputMode;
		PlayerController->SetInputMode(InputMode);
	}
}

void ASDCameraDirector::StartFOVBlend(const FSDCameraShot& Shot)
{
	if (!Shot.bOverrideFOV)
	{
		bFOVBlendActive = false;
		return;
	}

	const APlayerController* PlayerController = GetLocalPlayerController();
	if (!PlayerController || !PlayerController->PlayerCameraManager)
	{
		return;
	}

	FOVBlendElapsed = 0.0f;
	FOVBlendDuration = FMath::Max(Shot.FOVBlendTime, 0.0f);
	FOVBlendStart = PlayerController->PlayerCameraManager->GetFOVAngle();
	FOVBlendTarget = Shot.TargetFOV;
	bFOVBlendActive = true;

	if (FOVBlendDuration <= 0.0f)
	{
		PlayerController->PlayerCameraManager->SetFOV(FOVBlendTarget);
		bFOVBlendActive = false;
	}
}

void ASDCameraDirector::UpdateFOVBlend(float DeltaSeconds)
{
	if (!bFOVBlendActive)
	{
		return;
	}

	const APlayerController* PlayerController = GetLocalPlayerController();
	if (!PlayerController || !PlayerController->PlayerCameraManager)
	{
		bFOVBlendActive = false;
		return;
	}

	FOVBlendElapsed += DeltaSeconds;
	const float Alpha = FOVBlendDuration <= 0.0f ? 1.0f : FMath::Clamp(FOVBlendElapsed / FOVBlendDuration, 0.0f, 1.0f);
	PlayerController->PlayerCameraManager->SetFOV(FMath::Lerp(FOVBlendStart, FOVBlendTarget, Alpha));

	if (Alpha >= 1.0f)
	{
		bFOVBlendActive = false;
	}
}

void ASDCameraDirector::UpdateShotMouseLook()
{
	if (!ActiveCamera || !bForceActiveMouseLook)
	{
		return;
	}

	const FSDCameraShot* Shot = FindShot(ActiveShotId);
	if (!Shot)
	{
		return;
	}

	APlayerController* PlayerController = GetLocalPlayerController();
	if (!PlayerController)
	{
		return;
	}

	float MouseDeltaX = 0.0f;
	float MouseDeltaY = 0.0f;
	PlayerController->GetInputMouseDelta(MouseDeltaX, MouseDeltaY);
	if (FMath::IsNearlyZero(MouseDeltaX) && FMath::IsNearlyZero(MouseDeltaY))
	{
		return;
	}

	FRotator NewRotation = ActiveCamera->GetActorRotation();
	NewRotation.Yaw += MouseDeltaX * Shot->MouseLookSensitivity;

	if (Shot->bLimitYaw)
	{
		const float RelativeYaw = FRotator::NormalizeAxis(NewRotation.Yaw - ActiveShotBaseRotation.Yaw);
		const float ClampedYaw = FMath::Clamp(RelativeYaw, Shot->MinYawOffset, Shot->MaxYawOffset);
		NewRotation.Yaw = ActiveShotBaseRotation.Yaw + ClampedYaw;
	}

	const float PitchSign = Shot->bInvertMouseY ? 1.0f : -1.0f;
	const float CurrentPitch = FRotator::NormalizeAxis(NewRotation.Pitch);
	NewRotation.Pitch = FMath::Clamp(
		CurrentPitch + MouseDeltaY * Shot->MouseLookSensitivity * PitchSign,
		Shot->MinPitch,
		Shot->MaxPitch);
	NewRotation.Roll = 0.0f;

	ActiveCamera->SetActorRotation(NewRotation);
}

APlayerController* ASDCameraDirector::GetLocalPlayerController() const
{
	return UGameplayStatics::GetPlayerController(this, 0);
}
