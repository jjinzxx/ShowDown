#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Widgets/SWidget.h"
#include "ShowDownBetStatusWidget.generated.h"

class UBorder;
class UHorizontalBox;
class UTextBlock;

enum class EShowDownBetStatusPulseMode : uint8
{
	Generic,
	Call,
	Raise,
	Fold
};

UCLASS()
class SHOWDOWN_API UShowDownBetStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Bet Status")
	void SetBetStatus(
		const FText& DisplayName,
		const FText& StatusText,
		int32 FilledBulletCount,
		int32 MaxBulletCount,
		FLinearColor AccentColor);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildDefaultWidget();
	void RefreshVisuals();
	void RefreshBulletDots();
	EShowDownBetStatusPulseMode ResolvePulseMode(const FString& StatusText) const;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> BackgroundBorder;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> AccentBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> BulletRow;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> BulletDotTexts;

	FText CachedDisplayName;
	FText CachedStatusText;
	int32 CachedFilledBulletCount = 0;
	int32 CachedMaxBulletCount = 6;
	FLinearColor CachedAccentColor = FLinearColor(1.0f, 0.72f, 0.18f, 1.0f);
	float PulseRemainingSeconds = 0.0f;
	EShowDownBetStatusPulseMode PulseMode = EShowDownBetStatusPulseMode::Generic;
};
