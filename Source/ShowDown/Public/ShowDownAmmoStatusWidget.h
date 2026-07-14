#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Widgets/SWidget.h"
#include "ShowDownAmmoStatusWidget.generated.h"

class UBorder;
class UTextBlock;

UCLASS()
class SHOWDOWN_API UShowDownAmmoStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetAmmoStatus(const FText& NewText, int32 FontSize, const FLinearColor& TextColor, const FLinearColor& BackgroundColor);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	void BuildDefaultWidget();
	void RefreshVisuals();

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Background;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> AmmoText;

	FText CachedText;
	int32 CachedFontSize = 42;
	FLinearColor CachedTextColor = FLinearColor(1.0f, 0.82f, 0.25f, 1.0f);
	FLinearColor CachedBackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.68f);
};
