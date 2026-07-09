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
	int32 MinRaiseTarget = 1;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Actions")
	int32 MaxRaiseTarget = 6;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Actions")
	bool bCanRaise = false;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Actions")
	bool bCanFold = false;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Bet Actions", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float PanelVisualScale = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Bet Actions", meta = (ClampMin = "1.0"))
	float ButtonHeight = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Bet Actions", meta = (ClampMin = "1.0"))
	float PrimaryButtonWidth = 44.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Bet Actions", meta = (ClampMin = "1.0"))
	float RaiseButtonWidth = 51.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Bet Actions", meta = (ClampMin = "1.0"))
	float StepButtonWidth = 19.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Bet Actions", meta = (ClampMin = "1.0"))
	float FoldButtonWidth = 34.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Bet Actions")
	float TopRowHeight = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Bet Actions")
	float BottomRowHeight = -9.0f;

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
	void RefreshVisuals();
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

	int32 SelectedRaiseTarget = 1;
	int32 LastSeenRevision = INDEX_NONE;
	int32 LastSeenTableBet = INDEX_NONE;
	int32 LastSeenCurrentPlayerBet = INDEX_NONE;
	EShowDownPlayerSlot LastSeenTurnSlot = EShowDownPlayerSlot::None;
	EShowDownPlayerSlot LastResolvedLocalPlayerSlot = EShowDownPlayerSlot::None;
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
		const FTransform& WorldTransform);

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
	TObjectPtr<UTextRenderComponent> LabelText;

private:
	void ApplyBackplateColor(const FLinearColor& Color);
	void ApplyAnimatedVisuals();

	UPROPERTY()
	TObjectPtr<ASDBetActionPanelActor> OwnerPanel;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BackplateMaterialInstance;

	UPROPERTY()
	TObjectPtr<UStaticMesh> BackplateMeshAsset;

	FLinearColor CurrentBackplateColor = FLinearColor::White;
	FLinearColor CurrentLabelColor = FLinearColor::White;

	ESDBetActionPanelButtonKind ButtonKind = ESDBetActionPanelButtonKind::Primary;
	bool bButtonEnabled = false;
	bool bTargetVisible = false;
	bool bPointerPressed = false;
	float VisualAlpha = 0.0f;
	float PressVisualAlpha = 0.0f;
	float CurrentPressDepth = 1.2f;
};
