#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShowDownTypes.h"
#include "SDBetBulletPresentationActor.generated.h"

class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

USTRUCT(BlueprintType)
struct FSDBetBulletLaneState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Bullets")
	EShowDownPlayerSlot Slot = EShowDownPlayerSlot::None;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Bullets")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Bullets")
	int32 BulletCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Bullets")
	bool bCurrentTurn = false;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Bullets")
	bool bFolded = false;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Bullets")
	bool bRouletteTarget = false;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Bullets")
	int32 RouletteBulletCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Bullets")
	FString ActionText;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Bullets")
	int32 ActionRevision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Bullets")
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Bullets")
	FRotator WorldRotation = FRotator::ZeroRotator;
};

USTRUCT(BlueprintType)
struct FSDBetBulletPresentationState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Bullets")
	TArray<FSDBetBulletLaneState> Lanes;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Bullets")
	int32 TableBet = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Bullets")
	FString StatusText;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|Bet Bullets")
	int32 Revision = 0;
};

UCLASS()
class SHOWDOWN_API ASDBetBulletPresentationActor : public AActor
{
	GENERATED_BODY()

public:
	ASDBetBulletPresentationActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Bet Bullets")
	float BulletSpacing = 4.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Bet Bullets")
	float LaneStatusHeight = 5.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Bet Bullets")
	float LaneNameHeight = -4.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Bet Bullets")
	float LaneBulletHeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Bet Bullets")
	float LaneActionHeight = -10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Bet Bullets")
	float HeaderHeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Bet Bullets")
	FVector BulletScale = FVector(0.055f, 0.055f, 0.018f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Bet Bullets")
	float CurrentTurnPulseScale = 1.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Bet Bullets")
	float ActionPulseSeconds = 1.1f;

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Bet Bullets")
	void SetPresentationState(const FSDBetBulletPresentationState& NewState);

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_PresentationState();

private:
	static constexpr int32 MaxLanes = 5;
	static constexpr int32 MaxBulletsPerLane = 6;

	void RefreshVisuals();
	void RefreshLaneVisual(int32 LaneIndex, const FSDBetBulletLaneState* LaneState);
	void SetTextComponentDefaults(UTextRenderComponent* TextComponent, float WorldSize) const;
	void ApplyBulletColor(UStaticMeshComponent* BulletMesh, const FLinearColor& Color) const;
	void SetLaneComponentsVisible(int32 LaneIndex, bool bVisible);
	FString BuildLaneStatusText(const FSDBetBulletLaneState& LaneState, bool bLocalLane) const;
	FString BuildLaneActionText(const FSDBetBulletLaneState& LaneState) const;
	bool IsLocalPlayerLane(const FSDBetBulletLaneState& LaneState) const;
	int32 GetBulletMeshIndex(int32 LaneIndex, int32 BulletIndex) const;

	UPROPERTY(ReplicatedUsing = OnRep_PresentationState)
	FSDBetBulletPresentationState PresentationState;

	UPROPERTY()
	TArray<TObjectPtr<UTextRenderComponent>> LaneStatusTexts;

	UPROPERTY()
	TArray<TObjectPtr<UTextRenderComponent>> LaneNameTexts;

	UPROPERTY()
	TArray<TObjectPtr<UTextRenderComponent>> LaneActionTexts;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> LanePlateMeshes;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> LaneAccentMeshes;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> BulletMeshes;

	UPROPERTY()
	TObjectPtr<UTextRenderComponent> HeaderText;

	UPROPERTY()
	TObjectPtr<UStaticMesh> BulletMeshAsset;

	UPROPERTY()
	TObjectPtr<UStaticMesh> PlateMeshAsset;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BulletMaterial;

	TArray<int32> LastSeenActionRevisions;
	TArray<float> LaneActionPulseRemaining;
};
