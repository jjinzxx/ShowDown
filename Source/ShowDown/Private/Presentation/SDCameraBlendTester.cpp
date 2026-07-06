// Fill out your copyright notice in the Description page of Project Settings.

#include "Presentation/SDCameraBlendTester.h"

#include "Camera/CameraActor.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Interaction/SDInteractable.h"
#include "Kismet/GameplayStatics.h"
#include "ShowDownCameraAspect.h"
#include "SlateOptMacros.h"
#include "Widgets/SLeafWidget.h"

class SSDCrosshairWidget : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SSDCrosshairWidget)
		: _CrosshairSize(18.0f)
		, _CrosshairThickness(2.0f)
		, _CrosshairColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.85f))
	{
	}
		SLATE_ARGUMENT(float, CrosshairSize)
		SLATE_ARGUMENT(float, CrosshairThickness)
		SLATE_ARGUMENT(FLinearColor, CrosshairColor)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		CrosshairSize = InArgs._CrosshairSize;
		CrosshairThickness = InArgs._CrosshairThickness;
		CrosshairColor = InArgs._CrosshairColor;
	}

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
	{
		return FVector2D::ZeroVector;
	}

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled
	) const override
	{
		const FVector2D Center = AllottedGeometry.GetLocalSize() * 0.5f;
		const float HalfSize = CrosshairSize * 0.5f;
		const float HalfThickness = CrosshairThickness * 0.5f;
		const FSlateColor SlateColor(CrosshairColor);

		const FPaintGeometry HorizontalGeometry = AllottedGeometry.ToPaintGeometry(
			FVector2f(CrosshairSize, CrosshairThickness),
			FSlateLayoutTransform(FVector2f(Center.X - HalfSize, Center.Y - HalfThickness))
		);

		const FPaintGeometry VerticalGeometry = AllottedGeometry.ToPaintGeometry(
			FVector2f(CrosshairThickness, CrosshairSize),
			FSlateLayoutTransform(FVector2f(Center.X - HalfThickness, Center.Y - HalfSize))
		);

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			HorizontalGeometry,
			FCoreStyle::Get().GetBrush("WhiteBrush"),
			ESlateDrawEffect::None,
			SlateColor.GetColor(InWidgetStyle)
		);

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			VerticalGeometry,
			FCoreStyle::Get().GetBrush("WhiteBrush"),
			ESlateDrawEffect::None,
			SlateColor.GetColor(InWidgetStyle)
		);

		return LayerId + 1;
	}

private:
	float CrosshairSize = 18.0f;
	float CrosshairThickness = 2.0f;
	FLinearColor CrosshairColor = FLinearColor::White;
};

ASDCameraBlendTester::ASDCameraBlendTester()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ASDCameraBlendTester::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerController* PlayerController = GetLocalPlayerController())
	{
		EnableInput(PlayerController);
		BindPrimaryInteractInput();
		CreateCrosshairWidget();

		if (bSetDefaultCameraOnBeginPlay && DefaultCamera)
		{
			ShowDownCameraAspect::ApplyForced16By9(DefaultCamera);
			PlayerController->SetViewTarget(DefaultCamera);
			SetViewMode(ECameraBlendTesterViewMode::DefaultCamera);
			ApplyFixedCameraInputMode();
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SDCameraBlendTester: PlayerController not found."));
	}
}

void ASDCameraBlendTester::BindPrimaryInteractInput()
{
	if (!InputComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("SDCameraBlendTester: InputComponent not found."));
		return;
	}

	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ASDCameraBlendTester::HandlePrimaryInteract);
}

void ASDCameraBlendTester::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateDefaultCameraMouseLook();
}

void ASDCameraBlendTester::BlendToDefaultCamera()
{
	BlendToCamera(DefaultCamera, TEXT("DefaultCamera"));
	SetViewMode(ECameraBlendTesterViewMode::DefaultCamera);
	ApplyFixedCameraInputMode();
}

void ASDCameraBlendTester::BlendToCloseCamera()
{
	BlendToCamera(CloseCamera, TEXT("CloseCamera"));
	SetViewMode(ECameraBlendTesterViewMode::CloseCamera);
	ApplyFixedCameraInputMode();
}

void ASDCameraBlendTester::BlendToFreeView()
{
	APlayerController* PlayerController = GetLocalPlayerController();
	if (!PlayerController)
	{
		UE_LOG(LogTemp, Warning, TEXT("SDCameraBlendTester: FreeView blend failed - PlayerController not found."));
		return;
	}

	APawn* ControlledPawn = PlayerController->GetPawn();
	if (!ControlledPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("SDCameraBlendTester: FreeView blend failed - controlled Pawn not found."));
		return;
	}

	if (bRestoreGameInputOnFreeView)
	{
		PlayerController->SetIgnoreMoveInput(false);
		PlayerController->SetIgnoreLookInput(false);
		PlayerController->SetInputMode(FInputModeGameOnly());
		PlayerController->bShowMouseCursor = false;
	}

	PlayerController->SetViewTargetWithBlend(
		ControlledPawn,
		FreeViewBlendTime,
		BlendFunction,
		BlendExponent,
		false
	);

	SetViewMode(ECameraBlendTesterViewMode::FreeView);
}

void ASDCameraBlendTester::HandlePrimaryInteract()
{
	if (ActiveViewMode != ECameraBlendTesterViewMode::DefaultCamera || !DefaultCamera)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector TraceStart = DefaultCamera->GetActorLocation();
	const FVector TraceEnd = TraceStart + (DefaultCamera->GetActorForwardVector() * InteractTraceDistance);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SDCameraBlendTesterInteract), true, this);
	if (APlayerController* PlayerController = GetLocalPlayerController())
	{
		if (APawn* ControlledPawn = PlayerController->GetPawn())
		{
			QueryParams.AddIgnoredActor(ControlledPawn);
		}
	}

	FHitResult HitResult;
	const bool bHit = World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams);

	if (bDrawDebugInteractTrace)
	{
		DrawDebugLine(
			World,
			TraceStart,
			bHit ? HitResult.ImpactPoint : TraceEnd,
			bHit ? FColor::Green : FColor::Red,
			false,
			1.0f,
			0,
			1.5f
		);
	}

	if (!bHit)
	{
		return;
	}

	AActor* HitActor = HitResult.GetActor();
	if (HitActor && HitActor->GetClass()->ImplementsInterface(USDInteractable::StaticClass()))
	{
		if (ISDInteractable::Execute_CanInteract(HitActor, this))
		{
			ISDInteractable::Execute_Interact(HitActor, this);
		}
	}
}

void ASDCameraBlendTester::BlendToCamera(ACameraActor* CameraActor, const TCHAR* CameraLabel) const
{
	BlendToViewTarget(CameraActor, CameraLabel, BlendTime);
}

void ASDCameraBlendTester::BlendToViewTarget(AActor* ViewTarget, const TCHAR* ViewTargetLabel, float InBlendTime) const
{
	APlayerController* PlayerController = GetLocalPlayerController();
	if (!PlayerController)
	{
		UE_LOG(LogTemp, Warning, TEXT("SDCameraBlendTester: %s blend failed - PlayerController not found."), ViewTargetLabel);
		return;
	}

	if (!ViewTarget)
	{
		UE_LOG(LogTemp, Warning, TEXT("SDCameraBlendTester: %s is not assigned."), ViewTargetLabel);
		return;
	}

	if (ACameraActor* CameraActor = Cast<ACameraActor>(ViewTarget))
	{
		ShowDownCameraAspect::ApplyForced16By9(CameraActor);
	}

	PlayerController->SetViewTargetWithBlend(
		ViewTarget,
		InBlendTime,
		BlendFunction,
		BlendExponent,
		false
	);
}

void ASDCameraBlendTester::UpdateDefaultCameraMouseLook()
{
	if (ActiveViewMode != ECameraBlendTesterViewMode::DefaultCamera || !bEnableDefaultCameraMouseLook || !DefaultCamera)
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

	FRotator NewRotation = DefaultCamera->GetActorRotation();
	NewRotation.Yaw += MouseDeltaX * MouseLookSensitivity;

	const float PitchInputSign = bInvertMouseY ? 1.0f : -1.0f;
	const float CurrentPitch = FRotator::NormalizeAxis(NewRotation.Pitch);
	NewRotation.Pitch = FMath::Clamp(CurrentPitch + MouseDeltaY * MouseLookSensitivity * PitchInputSign, MinPitch, MaxPitch);
	NewRotation.Roll = 0.0f;

	DefaultCamera->SetActorRotation(NewRotation);
}

void ASDCameraBlendTester::SetViewMode(ECameraBlendTesterViewMode NewViewMode)
{
	ActiveViewMode = NewViewMode;
	UpdateCrosshairVisibility();
}

void ASDCameraBlendTester::ApplyFixedCameraInputMode() const
{
	APlayerController* PlayerController = GetLocalPlayerController();
	if (!PlayerController)
	{
		return;
	}

	PlayerController->SetIgnoreMoveInput(false);
	PlayerController->SetIgnoreLookInput(false);
	PlayerController->SetInputMode(FInputModeGameOnly());
	PlayerController->bShowMouseCursor = false;
}

void ASDCameraBlendTester::CreateCrosshairWidget()
{
	if (CrosshairWidget.IsValid())
	{
		return;
	}

	APlayerController* PlayerController = GetLocalPlayerController();
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	CrosshairWidget = SNew(SSDCrosshairWidget)
		.CrosshairSize(CrosshairSize)
		.CrosshairThickness(CrosshairThickness)
		.CrosshairColor(CrosshairColor);

	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->AddViewportWidgetContent(CrosshairWidget.ToSharedRef(), 10);
		UpdateCrosshairVisibility();
	}
}

void ASDCameraBlendTester::UpdateCrosshairVisibility()
{
	if (!CrosshairWidget.IsValid())
	{
		return;
	}

	const bool bShouldShowCrosshair = bShowCrosshair && ActiveViewMode == ECameraBlendTesterViewMode::DefaultCamera;
	CrosshairWidget->SetVisibility(bShouldShowCrosshair ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
}

APlayerController* ASDCameraBlendTester::GetLocalPlayerController() const
{
	return UGameplayStatics::GetPlayerController(this, 0);
}
