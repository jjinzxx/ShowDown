#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SDVisionDirector.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class UPostProcessComponent;
class USceneComponent;

UENUM(BlueprintType)
enum class ESDVisionBlendEase : uint8
{
	Linear UMETA(DisplayName = "Linear"),
	EaseIn UMETA(DisplayName = "Ease In"),
	EaseOut UMETA(DisplayName = "Ease Out"),
	EaseInOut UMETA(DisplayName = "Ease In Out")
};

USTRUCT(BlueprintType)
struct FSDVisionState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision", meta = (ClampMin = "0.0"))
	float VisionRadius = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision", meta = (ClampMin = "0.0"))
	float VisionFeather = 100.0f;

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

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Vision", meta = (ClampMin = "0.0", ClampMax = "1.0", AdvancedDisplay = "EaseMode,EaseExponent"))
	void BlendToVisionAlpha(
		float TargetAlpha,
		float Duration = 0.35f,
		ESDVisionBlendEase EaseMode = ESDVisionBlendEase::EaseInOut,
		float EaseExponent = 2.0f);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Vision", meta = (AdvancedDisplay = "EaseMode,EaseExponent"))
	void BlendToFocusedVision(
		float Duration = 0.35f,
		ESDVisionBlendEase EaseMode = ESDVisionBlendEase::EaseInOut,
		float EaseExponent = 2.0f);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Vision", meta = (AdvancedDisplay = "EaseMode,EaseExponent"))
	void BlendToWideVision(
		float Duration = 0.35f,
		ESDVisionBlendEase EaseMode = ESDVisionBlendEase::EaseInOut,
		float EaseExponent = 2.0f);

	/** Stops the active alpha blend at the value currently on screen. */
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Vision")
	void CancelVisionBlend();

	/** Immediately applies the active blend's destination, if one exists. */
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Vision")
	void CompleteVisionBlend();

	UFUNCTION(BlueprintPure, Category = "ShowDown|Vision")
	float GetVisionAlpha() const { return CurrentVisionAlpha; }

	UFUNCTION(BlueprintPure, Category = "ShowDown|Vision")
	float GetVisionBlendTargetAlpha() const { return bVisionBlendActive ? TargetVisionAlpha : CurrentVisionAlpha; }

	UFUNCTION(BlueprintPure, Category = "ShowDown|Vision")
	bool IsVisionBlending() const { return bVisionBlendActive; }

	UFUNCTION(BlueprintPure, Category = "ShowDown|Vision")
	static float EvaluateVisionBlendEase(
		float NormalizedAlpha,
		ESDVisionBlendEase EaseMode = ESDVisionBlendEase::EaseInOut,
		float EaseExponent = 2.0f);

	/** Immediately changes only DarknessStrength; radius and feather remain unchanged. */
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Vision|Darkness")
	void SetDarknessStrength(float Strength);

	/** Blends only DarknessStrength; radius and feather remain unchanged. */
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Vision|Darkness", meta = (ClampMin = "0.0", ClampMax = "1.0", AdvancedDisplay = "EaseMode,EaseExponent"))
	void BlendToDarknessStrength(
		float TargetStrength,
		float Duration = 0.35f,
		ESDVisionBlendEase EaseMode = ESDVisionBlendEase::EaseInOut,
		float EaseExponent = 2.0f);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Vision|Darkness")
	void CancelDarknessStrengthBlend();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Vision|Darkness")
	void CompleteDarknessStrengthBlend();

	UFUNCTION(BlueprintPure, Category = "ShowDown|Vision|Darkness")
	bool IsDarknessStrengthBlending() const { return bDarknessStrengthBlendActive; }

	UFUNCTION(BlueprintPure, Category = "ShowDown|Vision|Darkness")
	float GetDarknessStrength() const { return CurrentState.DarknessStrength; }

	UFUNCTION(BlueprintPure, Category = "ShowDown|Vision|Darkness")
	float GetDarknessStrengthBlendTarget() const
	{
		return bDarknessStrengthBlendActive ? TargetDarknessStrength : CurrentState.DarknessStrength;
	}

	/** Immediately changes only the visible radius and feather. */
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Vision|Range")
	void SetVisionRange(float Radius, float Feather);

	/** Blends radius and feather independently from DarknessStrength. */
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Vision|Range", meta = (AdvancedDisplay = "EaseMode,EaseExponent"))
	void BlendToVisionRange(
		float TargetRadius,
		float TargetFeather,
		float Duration = 0.35f,
		ESDVisionBlendEase EaseMode = ESDVisionBlendEase::EaseInOut,
		float EaseExponent = 2.0f);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Vision|Range")
	void CancelVisionRangeBlend();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Vision|Range")
	void CompleteVisionRangeBlend();

	UFUNCTION(BlueprintPure, Category = "ShowDown|Vision|Range")
	bool IsVisionRangeBlending() const { return bVisionRangeBlendActive; }

	UFUNCTION(BlueprintPure, Category = "ShowDown|Vision|Range")
	float GetVisionRadius() const { return CurrentState.VisionRadius; }

	UFUNCTION(BlueprintPure, Category = "ShowDown|Vision|Range")
	float GetVisionFeather() const { return CurrentState.VisionFeather; }

	UFUNCTION(BlueprintPure, Category = "ShowDown|Vision|Range")
	float GetTableVisionRadius() const { return FMath::Max(0.0f, FocusedVision.VisionRadius); }

	UFUNCTION(BlueprintPure, Category = "ShowDown|Vision|Range")
	float GetTableVisionFeather() const { return FMath::Max(1.0f, FocusedVision.VisionFeather); }

	UFUNCTION(BlueprintPure, Category = "ShowDown|Vision|Range")
	float GetIntroWideVisionRadius() const { return FMath::Max(0.0f, IntroWideVisionRadius); }

	UFUNCTION(BlueprintPure, Category = "ShowDown|Vision|Range")
	float GetIntroWideVisionFeather() const { return FMath::Max(1.0f, IntroWideVisionFeather); }

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Vision|Center")
	void SetVisionCenterActor(AActor* NewVisionCenterActor);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Vision|Center")
	void SetVisionCenterWorldLocation(FVector NewWorldLocation);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Vision|Center", meta = (AdvancedDisplay = "EaseMode,EaseExponent"))
	void BlendVisionCenterToActor(
		AActor* TargetActor,
		float Duration = 0.35f,
		ESDVisionBlendEase EaseMode = ESDVisionBlendEase::EaseInOut,
		float EaseExponent = 2.0f);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Vision|Center", meta = (AdvancedDisplay = "EaseMode,EaseExponent"))
	void BlendVisionCenterToWorldLocation(
		FVector TargetWorldLocation,
		float Duration = 0.35f,
		ESDVisionBlendEase EaseMode = ESDVisionBlendEase::EaseInOut,
		float EaseExponent = 2.0f);

	/** Stops the active center blend at its currently displayed world location. */
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Vision|Center")
	void CancelVisionCenterBlend();

	/** Immediately applies the active center blend's destination, if one exists. */
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Vision|Center")
	void CompleteVisionCenterBlend();

	UFUNCTION(BlueprintPure, Category = "ShowDown|Vision|Center")
	bool IsVisionCenterBlending() const { return bVisionCenterBlendActive; }

	UFUNCTION(BlueprintPure, Category = "ShowDown|Vision|Center")
	FVector GetVisionCenterWorldLocation() const;

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

	/** Hub/loading stays clear. Match-entry presentation activates gameplay darkness. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Vision", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InitialDarknessStrength = 0.0f;

	/** Match-entry iris starts wide, then closes to the focused table preset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Vision|Intro", meta = (ClampMin = "0.0"))
	float IntroWideVisionRadius = 5000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Vision|Intro", meta = (ClampMin = "1.0"))
	float IntroWideVisionFeather = 100.0f;

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
	void AdvanceVisionBlend(float DeltaSeconds);
	void AdvanceDarknessStrengthBlend(float DeltaSeconds);
	void AdvanceVisionRangeBlend(float DeltaSeconds);
	void AdvanceVisionCenterBlend(float DeltaSeconds);
	FVector GetVisionCenterBlendTargetLocation() const;
	bool ShouldApplyPostProcessInCurrentWorld() const;
	static FSDVisionState LerpVisionState(const FSDVisionState& From, const FSDVisionState& To, float Alpha);
	static bool IsMatchingBlendable(const UObject* BlendableObject, const UMaterialInterface* Material, const UMaterialInstanceDynamic* MID);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DarknessMID;

	FSDVisionState CurrentState;
	FSDVisionState VisionBlendStartState;
	FSDVisionState TargetVisionState;
	float CurrentVisionAlpha = 0.0f;
	float VisionBlendStartAlpha = 0.0f;
	float TargetVisionAlpha = 0.0f;
	float VisionBlendDuration = 0.0f;
	float VisionBlendElapsed = 0.0f;
	float VisionBlendEaseExponent = 2.0f;
	ESDVisionBlendEase VisionBlendEaseMode = ESDVisionBlendEase::EaseInOut;
	bool bVisionBlendActive = false;
	float DarknessStrengthBlendStart = 0.0f;
	float TargetDarknessStrength = 0.0f;
	float DarknessStrengthBlendDuration = 0.0f;
	float DarknessStrengthBlendElapsed = 0.0f;
	float DarknessStrengthBlendEaseExponent = 2.0f;
	ESDVisionBlendEase DarknessStrengthBlendEaseMode = ESDVisionBlendEase::EaseInOut;
	bool bDarknessStrengthBlendActive = false;
	float VisionRangeBlendStartRadius = 0.0f;
	float VisionRangeBlendStartFeather = 1.0f;
	float TargetVisionRangeRadius = 0.0f;
	float TargetVisionRangeFeather = 1.0f;
	float VisionRangeBlendDuration = 0.0f;
	float VisionRangeBlendElapsed = 0.0f;
	float VisionRangeBlendEaseExponent = 2.0f;
	ESDVisionBlendEase VisionRangeBlendEaseMode = ESDVisionBlendEase::EaseInOut;
	bool bVisionRangeBlendActive = false;

	FVector ExplicitVisionCenterWorldLocation = FVector::ZeroVector;
	FVector VisionCenterBlendStartLocation = FVector::ZeroVector;
	FVector TargetVisionCenterWorldLocation = FVector::ZeroVector;
	FVector BlendedVisionCenterWorldLocation = FVector::ZeroVector;
	float VisionCenterBlendDuration = 0.0f;
	float VisionCenterBlendElapsed = 0.0f;
	float VisionCenterBlendEaseExponent = 2.0f;
	ESDVisionBlendEase VisionCenterBlendEaseMode = ESDVisionBlendEase::EaseInOut;
	bool bUseExplicitVisionCenterLocation = false;
	bool bVisionCenterBlendTargetsActor = false;
	bool bVisionCenterBlendActive = false;

	UPROPERTY(Transient)
	TObjectPtr<AActor> TargetVisionCenterActor;
};
