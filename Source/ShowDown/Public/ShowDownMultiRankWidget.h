#pragma once

#include "CoreMinimal.h"
#include "ShowDownUserWidget.h"
#include "ShowDownMultiRankWidget.generated.h"

class UTextBlock;
class UVerticalBox;
class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FShowDownMultiRankRequest);

UCLASS()
class SHOWDOWN_API UShowDownMultiRankWidget : public UShowDownUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Multiplayer Rank")
	void SetRanking(const TArray<FString>& PlayerNames);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Multiplayer Rank")
	void SetRestartStatus(const FString& StatusText);

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Multiplayer Rank")
	FShowDownMultiRankRequest OnRestartRequested;

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Multiplayer Rank")
	FShowDownMultiRankRequest OnMainMenuRequested;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;

	UPROPERTY(meta = (BindWidgetOptional))
	UVerticalBox* VerticalBox_RankList = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* Text_Title = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* Text_RestartStatus = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* Button_Restart = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* Button_MainMenu = nullptr;

private:
	UFUNCTION()
	void HandleRestartClicked();

	UFUNCTION()
	void HandleMainMenuClicked();

	void BuildDefaultLayout();
	void RefreshRanking();
	void BindButtons();
	UButton* CreateMenuButton(const FName& WidgetName, const FString& LabelText);

	UPROPERTY()
	TArray<FString> CachedPlayerNames;
};
