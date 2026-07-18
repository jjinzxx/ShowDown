#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Widgets/SWidget.h"
#include "ShowDownAmmoStatusWidget.generated.h"

class UBorder;
class UHorizontalBox;
class UOverlay;
class USizeBox;
class UTextBlock;

enum class EShowDownAmmoSlotState : uint8
{
	Live,
	Empty,
	Spent
};

UCLASS()
class SHOWDOWN_API UShowDownAmmoStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetAmmoStatus(
		int32 LiveRounds,
		int32 RemainingChambers,
		const FLinearColor& LiveRoundColor,
		const FLinearColor& BackgroundColor,
		float SlotDiameter,
		float SlotSpacing,
		bool bEmphasized);
	void ShowRaiseDelta(int32 AddedRounds);
	void CompleteRaiseDelta();
	static EShowDownAmmoSlotState ResolveSlotStateForCounts(
		int32 SlotIndex,
		int32 LiveRounds,
		int32 RemainingChambers);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	static constexpr int32 ChamberSlotCount = 6;
	static constexpr float RiskColorTransitionDuration = 0.20f;
	static constexpr float SlotPulseDuration = 0.28f;
	static constexpr float EmphasisPulseDuration = 0.34f;
	static constexpr float ShotStatusHoldDuration = 0.12f;
	static constexpr float RaiseDeltaAppearDuration = 0.16f;
	static constexpr float RaiseDeltaCompleteDuration = 0.46f;

	void BuildDefaultWidget();
	void RefreshVisuals();
	void CommitAmmoStatus(int32 LiveRounds, int32 RemainingChambers, const FLinearColor& LiveRoundColor, bool bAnimateSlotChanges);
	EShowDownAmmoSlotState ResolveSlotState(int32 SlotIndex) const;
	float ResolveSlotPulseScale(int32 SlotIndex) const;
	float ResolvePanelScale() const;

	UPROPERTY(Transient)
	TObjectPtr<UOverlay> RootOverlay;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Background;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> StatusRow;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> RaiseDeltaBadge;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RaiseDeltaText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USizeBox>> SlotContainers;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> SlotCircles;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> SpentMarks;

	int32 CachedLiveRounds = 0;
	int32 CachedRemainingChambers = ChamberSlotCount;
	float CachedSlotDiameter = 28.0f;
	float CachedSlotSpacing = 8.0f;
	bool bCachedEmphasized = false;
	bool bHasCachedStatus = false;
	FLinearColor CurrentLiveRoundColor = FLinearColor::White;
	FLinearColor LiveRoundColorTransitionStart = FLinearColor::White;
	FLinearColor TargetLiveRoundColor = FLinearColor::White;
	FLinearColor CachedBackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.68f);
	float RiskColorTransitionElapsed = RiskColorTransitionDuration;
	float EmphasisPulseElapsed = EmphasisPulseDuration;
	TArray<float> SlotPulseElapsed;
	bool bShotStatusCommitPending = false;
	int32 PendingLiveRounds = 0;
	int32 PendingRemainingChambers = ChamberSlotCount;
	FLinearColor PendingLiveRoundColor = FLinearColor::White;
	float ShotStatusHoldElapsed = 0.0f;
	int32 CachedRaiseDelta = 0;
	bool bRaiseDeltaActive = false;
	bool bRaiseDeltaCompleting = false;
	float RaiseDeltaElapsed = 0.0f;
};
