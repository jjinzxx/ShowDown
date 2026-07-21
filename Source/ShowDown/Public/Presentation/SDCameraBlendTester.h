// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Actor.h"
#include "SDCameraBlendTester.generated.h"

class ACameraActor;
class APlayerController;
class SSDCrosshairWidget;

UCLASS(Blueprintable)
class SHOWDOWN_API ASDCameraBlendTester : public AActor
{
	GENERATED_BODY()

public:
	ASDCameraBlendTester();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Camera Blend Test")
	void BlendToDefaultCamera();

	UFUNCTION(BlueprintCallable, Category = "Camera Blend Test")
	void BlendToCloseCamera();

	UFUNCTION(BlueprintCallable, Category = "Camera Blend Test")
	void BlendToFreeView();

	UFUNCTION(BlueprintCallable, Category = "Center Interaction")
	void HandlePrimaryInteract();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Camera Blend Test")
	TObjectPtr<ACameraActor> DefaultCamera;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Camera Blend Test")
	TObjectPtr<ACameraActor> CloseCamera;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Blend Test", meta = (ClampMin = "0.0"))
	float BlendTime = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Blend Test", meta = (ClampMin = "0.0"))
	float FreeViewBlendTime = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Blend Test")
	TEnumAsByte<EViewTargetBlendFunction> BlendFunction = VTBlend_EaseInOut;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Blend Test", meta = (ClampMin = "0.0"))
	float BlendExponent = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Blend Test")
	bool bSetDefaultCameraOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Blend Test")
	bool bRestoreGameInputOnFreeView = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Default Camera Look")
	bool bEnableDefaultCameraMouseLook = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Default Camera Look", meta = (ClampMin = "0.0"))
	float MouseLookSensitivity = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Default Camera Look", meta = (ClampMin = "-89.0", ClampMax = "89.0"))
	float MinPitch = -75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Default Camera Look", meta = (ClampMin = "-89.0", ClampMax = "89.0"))
	float MaxPitch = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Default Camera Look")
	bool bInvertMouseY = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Center Interaction", meta = (ClampMin = "0.0"))
	float InteractTraceDistance = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Center Interaction")
	bool bDrawDebugInteractTrace = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Center Interaction")
	bool bShowCrosshair = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Center Interaction", meta = (ClampMin = "1.0"))
	float CrosshairSize = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Center Interaction", meta = (ClampMin = "1.0"))
	float CrosshairThickness = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Center Interaction")
	FLinearColor CrosshairColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.85f);

private:
	enum class ECameraBlendTesterViewMode : uint8
	{
		DefaultCamera,
		CloseCamera,
		FreeView
	};

	void BindPrimaryInteractInput();
	void BlendToCamera(ACameraActor* CameraActor, const TCHAR* CameraLabel) const;
	void BlendToViewTarget(AActor* ViewTarget, const TCHAR* ViewTargetLabel, float InBlendTime) const;
	void UpdateDefaultCameraMouseLook();
	void SetViewMode(ECameraBlendTesterViewMode NewViewMode);
	void ApplyFixedCameraInputMode() const;
	void CreateCrosshairWidget();
	void UpdateCrosshairVisibility();
	APlayerController* GetLocalPlayerController() const;

	ECameraBlendTesterViewMode ActiveViewMode = ECameraBlendTesterViewMode::FreeView;
	TSharedPtr<SSDCrosshairWidget> CrosshairWidget;
};
