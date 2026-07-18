#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/SDInteractable.h"
#include "ShowDownTypes.h"
#include "SDBetActionPanelActor.generated.h"

class ASDBetActionButtonActor;
class UMaterialInstanceDynamic;
class UBoxComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class ESDBetActionPanelButtonKind : uint8
{
	Primary,
	RaiseDown,
	RaiseSubmit,
	RaiseUp,
	Fold
};

USTRUCT(BlueprintType)
struct FSDBetActionPanelState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Actions")
	bool bVisible = false;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Actions")
	bool bMultiplayer = false;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Actions")
	EShowDownPlayerSlot TurnSlot = EShowDownPlayerSlot::None;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Actions")
	int32 CurrentPlayerBet = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Actions")
	int32 TableBet = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Actions")
	int32 LoadedBulletCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Actions")
	int32 SelectedRaiseTarget = 1;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Actions")
	int32 MinRaiseTarget = 1;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Actions")
	int32 MaxRaiseTarget = 6;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Actions")
	bool bCanRaise = false;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Actions")
	bool bCanFold = false;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Actions|Layout")
	float PanelVisualScale = 0.35f;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Actions|Layout")
	float ButtonHeight = 12.0f;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Actions|Layout")
	float PrimaryButtonWidth = 46.0f;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Actions|Layout")
	float RaiseButtonWidth = 58.0f;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Actions|Layout")
	float StepButtonWidth = 18.0f;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Actions|Layout")
	float FoldButtonWidth = 46.0f;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Actions|Layout")
	float TopRowHeight = 7.5f;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Actions|Layout")
	float BottomRowHeight = -7.0f;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Actions|Layout")
	float BulletSpacing = 10.0f;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Actions|Layout")
	float BulletPreviewScale = 0.08f;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Actions|Layout")
	FVector BulletRowOffset = FVector(0.0f, 0.0f, 18.0f);

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Actions|Animation")
	float ButtonAnimationDuration = 0.34f;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Actions|Animation")
	float ButtonAnimationStaggerDelay = 0.035f;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Actions|Animation")
	float ButtonBounceStrength = 0.14f;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Actions|Animation")
	float BulletAnimationDuration = 0.26f;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Actions|Animation")
	float BulletRevealStaggerDelay = 0.055f;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Actions|Animation")
	float BulletBounceStrength = 0.20f;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Actions")
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Actions")
	FRotator WorldRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Actions")
	int32 Revision = 0;
};

UCLASS()
class SHOWDOWN_API ASDBetActionPanelActor : public AActor
{
	GENERATED_BODY()

public:
	ASDBetActionPanelActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Bet Actions")
	TSubclassOf<ASDBetActionButtonActor> ButtonActorClass;

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Bet Actions")
	void SetPanelState(const FSDBetActionPanelState& NewState);

	bool CanPressButton(ESDBetActionPanelButtonKind ButtonKind) const;
	void HandleButtonClicked(ESDBetActionPanelButtonKind ButtonKind, AActor* Interactor);
	bool IsPanelVisibleForLocalPlayer() const;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_PanelState();

private:
	static constexpr int32 ButtonCount = 5;

	void EnsureButtons();
	void EnsureBulletPreview();
	void RefreshVisuals();
	void RefreshBulletPreview(bool bVisible);
	void StartBulletVisibilityAnimation(bool bVisible);
	void TriggerBulletChangeAnimation(int32 PreviousTarget, int32 NewTarget);
	void UpdateBulletAnimations(float DeltaSeconds);
	void ApplyBulletAnimatedVisuals();
	bool HasActiveBulletAnimation() const;
	void RefreshTickState();
	void RefreshSelectedRaiseTarget();
	void AdjustSelectedRaiseTarget(int32 Delta);
	int32 GetDefaultRaiseTarget() const;
	int32 ClampRaiseTarget(int32 TargetBet) const;
	EShowDownPlayerSlot ResolveLocalPlayerSlot() const;
	FTransform BuildButtonTransform(ESDBetActionPanelButtonKind ButtonKind) const;
	FVector2D GetButtonSize(ESDBetActionPanelButtonKind ButtonKind) const;
	FString BuildButtonLabel(ESDBetActionPanelButtonKind ButtonKind) const;
	FLinearColor GetButtonColor(ESDBetActionPanelButtonKind ButtonKind, bool bEnabled) const;
	int32 GetButtonIndex(ESDBetActionPanelButtonKind ButtonKind) const;

	UPROPERTY(ReplicatedUsing = OnRep_PanelState)
	FSDBetActionPanelState PanelState;

	UPROPERTY()
	TArray<TObjectPtr<ASDBetActionButtonActor>> ButtonActors;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> BulletPreviewMeshes;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> BulletPreviewMaterials;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> BulletPreviewOutlineMeshes;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> BulletPreviewOutlineMaterials;

	TArray<float> BulletVisualAlphas;
	TArray<float> BulletVisualScales;
	TArray<float> BulletTransitionStartAlphas;
	TArray<float> BulletTransitionStartScales;
	TArray<float> BulletTransitionElapsedTimes;
	TArray<float> BulletPulseElapsedTimes;

	UPROPERTY()
	TObjectPtr<UStaticMesh> BulletPreviewMeshAsset;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BulletPreviewTintMaterial;

	int32 SelectedRaiseTarget = 1;
	int32 LastSeenRevision = INDEX_NONE;
	int32 LastSeenTableBet = INDEX_NONE;
	int32 LastSeenCurrentPlayerBet = INDEX_NONE;
	EShowDownPlayerSlot LastSeenTurnSlot = EShowDownPlayerSlot::None;
	EShowDownPlayerSlot LastResolvedLocalPlayerSlot = EShowDownPlayerSlot::None;
	FVector CachedBulletPanelLocation = FVector::ZeroVector;
	FRotator CachedBulletPanelRotation = FRotator::ZeroRotator;
	bool bHasCachedBulletPanelTransform = false;
	bool bBulletTargetVisible = false;
	bool bBulletVisibilityTransitionActive = false;
};

UCLASS()
class SHOWDOWN_API ASDBetActionButtonActor : public AActor, public ISDInteractable
{
	GENERATED_BODY()

public:
	ASDBetActionButtonActor();

	void InitializeButton(ASDBetActionPanelActor* InOwnerPanel, ESDBetActionPanelButtonKind InButtonKind);
	void BeginPointerPress();
	void CancelPointerPress();
	void ReleasePointerPress(AActor* Interactor, bool bCommit);
	void SetButtonState(
		const FString& Label,
		const FLinearColor& Color,
		const FVector2D& Size,
		bool bVisible,
		bool bEnabled,
		const FTransform& WorldTransform,
		float AnimationDelay,
		float AnimationDuration,
		float BounceStrength);

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

protected:
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> ClickBounds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BackplateMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RimMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> HighlightMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ShadowMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> LabelShadowText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> LabelText;

private:
	void ApplyBackplateColor(const FLinearColor& Color);
	void ApplyAnimatedVisuals();

	UPROPERTY()
	TObjectPtr<ASDBetActionPanelActor> OwnerPanel;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BackplateMaterialInstance;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> RimMaterialInstance;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> HighlightMaterialInstance;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> ShadowMaterialInstance;

	UPROPERTY()
	TObjectPtr<UStaticMesh> BackplateMeshAsset;

	FLinearColor CurrentBackplateColor = FLinearColor::White;
	FLinearColor CurrentRimColor = FLinearColor::White;
	FLinearColor CurrentHighlightColor = FLinearColor::White;
	FLinearColor CurrentShadowColor = FLinearColor::Black;
	FLinearColor CurrentLabelColor = FLinearColor::White;

	ESDBetActionPanelButtonKind ButtonKind = ESDBetActionPanelButtonKind::Primary;
	bool bButtonEnabled = false;
	bool bTargetVisible = false;
	bool bPointerPressed = false;
	bool bVisibilityTransitionActive = false;
	float VisualAlpha = 0.0f;
	float VisualScale = 0.0f;
	float PressVisualAlpha = 0.0f;
	float VisibilityTransitionElapsed = 0.0f;
	float VisibilityTransitionDuration = 0.34f;
	float VisibilityTransitionStartAlpha = 0.0f;
	float VisibilityTransitionStartScale = 0.0f;
	float VisibilityBounceStrength = 0.14f;
	float CurrentButtonWidth = 1.0f;
	float CurrentButtonHeight = 1.0f;
	float CurrentHighlightHeight = 0.4f;
	float CurrentPressDepth = 1.2f;
	float CurrentLabelDepth = 1.0f;
};
