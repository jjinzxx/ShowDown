#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Widgets/SWidget.h"
#include "ShowDownNameTagWidget.generated.h"

class UTextBlock;
class UBorder;
class UVerticalBox;

USTRUCT()
struct FShowDownOverheadChatBubbleEntry
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Background;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text;

	double StartTimeSeconds = 0.0;
	double PushStartTimeSeconds = 0.0;
	float PushDistance = 0.0f;
};

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

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Name Tag")
	void ShowOverheadChatMessage(const FText& NewChatText);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildDefaultWidget();
	void RefreshNameBackgroundColor();
	void UpdateChatBubbleAnimation();
	void HideOverheadChatMessage();
	void TrimOverheadChatBubbleCount();
	void RefreshChatBubbleTimer();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> NameBackground;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ChatStack;

	UPROPERTY(Transient)
	TArray<FShowDownOverheadChatBubbleEntry> ChatBubbles;

	FText CachedDisplayName;
	FText CachedStatusText;
	FTimerHandle ChatBubbleAnimationTimerHandle;
	bool bCachedTurnActive = false;
};
