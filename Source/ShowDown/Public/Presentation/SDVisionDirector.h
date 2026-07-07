#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SDVisionDirector.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class UPostProcessComponent;
class USceneComponent;

USTRUCT(BlueprintType)
struct FSDVisionState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision", meta = (ClampMin = "0.0"))
	float VisionRadius = 450.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision", meta = (ClampMin = "0.0"))
	float VisionFeather = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DarknessStrength = 1.0f;
};

UCLASS(Blueprintable)
class SHOWDOWN_API ASDVisionDirector : public AActor
{
	GENERATED_BODY()

public:
	ASDVisionDirector();

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Vision")
	void ApplyFocusedVision();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Vision")
	void ApplyWideVision();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Vision")
	void SetVisionAlpha(float Alpha);

	UFUNCTION(BlueprintPure, Category = "ShowDown|Vision")
	float GetVisionAlpha() const { return CurrentVisionAlpha; }

protected:
	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Vision")
	TObjectPtr<AActor> VisionCenterActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Vision")
	FSDVisionState FocusedVision;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Vision")
	FSDVisionState WideVision;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Vision|Post Process")
	TObjectPtr<UMaterialInterface> DarknessPostProcessMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Vision|Post Process")
	bool bEnableDarknessPostProcess = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Vision|Post Process")
	bool bAutoLoadDefaultDarknessMaterial = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Vision|Post Process")
	bool bUnboundPostProcess = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Vision|Post Process")
	bool bPreviewPostProcessInEditor = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Vision|Post Process")
	float PostProcessPriority = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Vision")
	bool bApplyInitialStateOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Vision")
	bool bTrackVisionCenterEveryTick = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Vision|Editor Preview", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EditorPreviewVisionAlpha = 0.0f;

private:
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(Transient)
	TObjectPtr<UPostProcessComponent> PostProcessComponent;

	void EnsureDarknessMaterialInstance();
	void ApplyCurrentState();
	void ApplyPostProcessState();
	void ApplyDarknessMaterialState();
	void UpdateTickState();
	FVector GetVisionCenterWorldLocation() const;
	bool ShouldApplyPostProcessInCurrentWorld() const;
	static FSDVisionState LerpVisionState(const FSDVisionState& From, const FSDVisionState& To, float Alpha);
	static bool IsMatchingBlendable(const UObject* BlendableObject, const UMaterialInterface* Material, const UMaterialInstanceDynamic* MID);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DarknessMID;

	FSDVisionState CurrentState;
	float CurrentVisionAlpha = 0.0f;
};
