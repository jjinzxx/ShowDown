// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "Interaction/SDInteractable.h"
#include "Presentation/SDArtToneController.h"
#include "SDSmokableCigaretteActor.generated.h"

class UParticleSystemComponent;
class UPointLightComponent;
class USoundBase;
class UStaticMeshComponent;
class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSDSmokableCigaretteEvent);

UCLASS(Blueprintable)
class SHOWDOWN_API ASDSmokableCigaretteActor : public AActor, public ISDInteractable
{
	GENERATED_BODY()

public:
	ASDSmokableCigaretteActor();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Smokable Cigarette")
	void Smoke();

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

	UPROPERTY(BlueprintAssignable, Category = "Smokable Cigarette")
	FSDSmokableCigaretteEvent OnSmokeStarted;

	UPROPERTY(BlueprintAssignable, Category = "Smokable Cigarette")
	FSDSmokableCigaretteEvent OnSmokeFinished;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> CigaretteMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> EmberLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UParticleSystemComponent> SmokeParticle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smokable Cigarette")
	FVector LocalSmokeOffset = FVector(0.0f, 0.0f, 10.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smokable Cigarette")
	FRotator LocalSmokeRotationOffset = FRotator(-12.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "First Person Smoke")
	bool bUseFirstPersonSmokePose = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "First Person Smoke")
	FVector FirstPersonCameraOffset = FVector(32.0f, 10.0f, -12.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "First Person Smoke")
	FRotator FirstPersonRotationOffset = FRotator(0.0f, 0.0f, -18.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "First Person Smoke")
	bool bDisableCollisionWhileSmoking = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smokable Cigarette|Timing", meta = (ClampMin = "0.01"))
	float LiftTime = 0.32f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smokable Cigarette|Timing", meta = (ClampMin = "0.0"))
	float PreSmokeMouthHoldTime = 0.28f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smokable Cigarette|Timing", meta = (ClampMin = "0.0"))
	float SmokeHoldTime = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smokable Cigarette|Timing", meta = (ClampMin = "0.0"))
	float PostSmokeMouthHoldTime = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smokable Cigarette|Timing", meta = (ClampMin = "0.01"))
	float ReturnTime = 0.42f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smokable Cigarette|Timing", meta = (ClampMin = "0.0"))
	float SmokeParticleDelay = 0.34f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smokable Cigarette|Timing", meta = (ClampMin = "0.0"))
	float SmokeSoundDelay = 0.28f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smokable Cigarette|Timing", meta = (ClampMin = "0.0"))
	float ReinteractCooldown = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smokable Cigarette|Ember", meta = (ClampMin = "0.0"))
	float EmberIdleIntensity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smokable Cigarette|Ember", meta = (ClampMin = "0.0"))
	float EmberSmokeIntensity = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smokable Cigarette|Ember", meta = (ClampMin = "0.0"))
	float EmberFlickerAmount = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smokable Cigarette|Ember", meta = (ClampMin = "0.0"))
	float EmberFlickerSpeed = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smokable Cigarette|Ember", meta = (ClampMin = "0.0"))
	float EmberWarmupTime = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smokable Cigarette|Ember", meta = (ClampMin = "0.0"))
	float EmberFadeOutTime = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smokable Cigarette|Motion")
	bool bEnableSmokeSway = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smokable Cigarette|Motion")
	FVector SmokeSwayLocationAmplitude = FVector(0.35f, 0.75f, 0.3f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smokable Cigarette|Motion")
	FRotator SmokeSwayRotationAmplitude = FRotator(0.8f, 0.45f, 1.4f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smokable Cigarette|Motion", meta = (ClampMin = "0.0"))
	float SmokeSwaySpeed = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smokable Cigarette|Motion", meta = (ClampMin = "0.0"))
	float SmokeSwayStartDelay = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smokable Cigarette|Motion", meta = (ClampMin = "0.0"))
	float SmokeSwayRampTime = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smokable Cigarette")
	TObjectPtr<USoundBase> SmokeSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smokable Cigarette|Screen Effect")
	bool bEnableSmokeScreenEffect = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smokable Cigarette|Screen Effect")
	TObjectPtr<ASDArtToneController> ArtToneController;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smokable Cigarette|Screen Effect", meta = (ClampMin = "0.0"))
	float ScreenEffectStartDelay = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smokable Cigarette|Screen Effect", meta = (ClampMin = "0.0"))
	float ScreenEffectFadeInTime = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smokable Cigarette|Screen Effect", meta = (ClampMin = "0.0"))
	float ScreenEffectFadeOutTime = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smokable Cigarette|Screen Effect", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ScreenEffectMaxStrength = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smokable Cigarette|Screen Effect")
	FSDArtToneSettings SmokeScreenEffectSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	bool bEnableMouseClick = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	bool bEnablePlayerClickEventsOnBeginPlay = true;

private:
	enum class ECigaretteAnimState : uint8
	{
		Idle,
		Lifting,
		PreSmokeMouthHold,
		Holding,
		PostSmokeMouthHold,
		Returning,
		CoolingDown
	};

	UFUNCTION()
	void HandleCigaretteClicked(UPrimitiveComponent* TouchedComponent, FKey ButtonPressed);

	void StartPreSmokeMouthHold();
	void StartSmoking();
	void StartPostSmokeMouthHold();
	void StartCooldown();
	void UpdateSmokeParticle();
	void UpdateSmokeSound();
	void StartScreenEffect();
	void UpdateScreenEffect(float DeltaSeconds);
	void RestoreScreenEffect();
	ASDArtToneController* ResolveArtToneController();
	void SetCigarettePoseAlpha(float Alpha, float SwayStrength = 0.0f);
	FTransform GetFirstPersonSmokeTransform() const;
	FTransform ApplySmokeSway(const FTransform& BaseTransform, float SwayStrength) const;
	float GetSmokeSwayStrength() const;
	float GetSmokeSwayElapsedTime() const;
	FVector GetSmokeSwayLocationOffset(float SwayStrength) const;
	FRotator GetSmokeSwayRotationOffset(float SwayStrength) const;
	void SetActorTransformAlpha(const FTransform& FromTransform, const FTransform& ToTransform, float Alpha);
	void UpdateEmber(float DeltaSeconds);
	void FinishSmoke();

	FTransform RestActorTransform;
	FTransform SmokeStartActorTransform;
	FTransform ReturnStartActorTransform;
	FVector RestRelativeLocation = FVector::ZeroVector;
	FVector SmokeRelativeLocation = FVector::ZeroVector;
	FRotator RestRelativeRotation = FRotator::ZeroRotator;
	FRotator SmokeRelativeRotation = FRotator::ZeroRotator;
	ECollisionEnabled::Type OriginalCollisionEnabled = ECollisionEnabled::QueryAndPhysics;
	ECigaretteAnimState AnimState = ECigaretteAnimState::Idle;
	float StateElapsedTime = 0.0f;
	float SmokeElapsedTime = 0.0f;
	float InhaleElapsedTime = 0.0f;
	float PostSmokeElapsedTime = 0.0f;
	float ScreenEffectRestoreElapsedTime = 0.0f;
	FSDArtToneSettings ScreenEffectBaseSettings;
	FSDArtToneSettings ScreenEffectReleaseStartSettings;
	bool bSmokeParticleActivated = false;
	bool bSmokeSoundPlayed = false;
	bool bScreenEffectActive = false;
	bool bScreenEffectRestoring = false;
};
