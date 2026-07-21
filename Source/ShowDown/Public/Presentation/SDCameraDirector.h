#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Actor.h"
#include "Templates/SubclassOf.h"
#include "SDCameraDirector.generated.h"

class ACameraActor;
class APlayerController;
class UCameraShakeBase;

USTRUCT(BlueprintType)
struct FSDCameraShot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	FName ShotId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	TObjectPtr<ACameraActor> Camera = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend", meta = (ClampMin = "0.0"))
	float BlendTime = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend")
	TEnumAsByte<EViewTargetBlendFunction> BlendFunction = VTBlend_EaseInOut;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend", meta = (ClampMin = "0.0"))
	float BlendExponent = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bLockMoveInput = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bLockLookInput = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bShowMouseCursor = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouse Look")
	bool bEnableMouseLook = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouse Look", meta = (ClampMin = "0.0"))
	float MouseLookSensitivity = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouse Look", meta = (ClampMin = "-89.0", ClampMax = "89.0"))
	float MinPitch = -75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouse Look", meta = (ClampMin = "-89.0", ClampMax = "89.0"))
	float MaxPitch = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouse Look")
	bool bLimitYaw = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouse Look", meta = (ClampMin = "-180.0", ClampMax = "0.0"))
	float MinYawOffset = -45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouse Look", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float MaxYawOffset = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouse Look")
	bool bInvertMouseY = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
	TSubclassOf<UCameraShakeBase> CameraShakeClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects", meta = (ClampMin = "0.0"))
	float CameraShakeScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FOV")
	bool bOverrideFOV = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FOV", meta = (ClampMin = "1.0", ClampMax = "170.0"))
	float TargetFOV = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FOV", meta = (ClampMin = "0.0"))
	float FOVBlendTime = 0.25f;
};

UCLASS(Blueprintable)
class SHOWDOWN_API ASDCameraDirector : public AActor
{
	GENERATED_BODY()

public:
	ASDCameraDirector();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Camera")
	bool PlayShot(FName ShotId);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Camera")
	bool CutToShot(FName ShotId);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Camera")
	void ReturnToGameplayCamera(float BlendTime = 0.5f);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Camera")
	void ClearActiveShot();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Camera")
	void SetActiveMouseLookEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ShowDown|Camera")
	bool HasShot(FName ShotId) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ShowDown|Camera")
	FName GetActiveShotId() const { return ActiveShotId; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Camera")
	TArray<FSDCameraShot> Shots;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Camera")
	bool bPlayInitialShotOnBeginPlay = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Camera", meta = (EditCondition = "bPlayInitialShotOnBeginPlay"))
	FName InitialShotId = NAME_None;

private:
	bool ApplyShot(FName ShotId, bool bCut);
	const FSDCameraShot* FindShot(FName ShotId) const;
	void ApplyShotInputMode(const FSDCameraShot& Shot) const;
	void StartFOVBlend(const FSDCameraShot& Shot);
	void UpdateFOVBlend(float DeltaSeconds);
	void UpdateShotMouseLook();
	APlayerController* GetLocalPlayerController() const;

	FName ActiveShotId = NAME_None;

	UPROPERTY()
	TObjectPtr<ACameraActor> ActiveCamera = nullptr;

	UPROPERTY()
	TObjectPtr<AActor> GameplayViewTarget = nullptr;

	FRotator ActiveShotBaseRotation = FRotator::ZeroRotator;
	bool bForceActiveMouseLook = false;
	bool bFOVBlendActive = false;
	float FOVBlendElapsed = 0.0f;
	float FOVBlendDuration = 0.0f;
	float FOVBlendStart = 0.0f;
	float FOVBlendTarget = 0.0f;
};
