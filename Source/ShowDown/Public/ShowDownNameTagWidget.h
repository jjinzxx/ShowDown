#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Widgets/SWidget.h"
#include "ShowDownNameTagWidget.generated.h"

class UTextBlock;
class UBorder;

UCLASS()
class SHOWDOWN_API UShowDownNameTagWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Name Tag")
	void SetDisplayName(const FText& NewDisplayName);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Name Tag")
	void SetStatusText(const FText& NewStatusText);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Name Tag")
	void SetTurnActive(bool bNewTurnActive);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	void BuildDefaultWidget();
	void RefreshNameBackgroundColor();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> NameBackground;

	FText CachedDisplayName;
	FText CachedStatusText;
	bool bCachedTurnActive = false;
};
