#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Widgets/SWidget.h"
#include "ShowDownNameTagWidget.generated.h"

class UTextBlock;
class UBorder;
class UHorizontalBox;
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
	void SetLives(int32 NewLives);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Name Tag")
	void SetStatusText(const FText& NewStatusText);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Name Tag")
	void SetTurnActive(bool bNewTurnActive);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Name Tag")
	void ShowOverheadChatMessage(const FText& NewChatText);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Name Tag")
	void SetSpeakingIndicatorVisible(bool bVisible);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildDefaultWidget();
	void RefreshNameBackgroundColor();
	void UpdateSpeakingIndicatorAnimation(float InDeltaTime);
	void UpdateChatBubbleAnimation();
	void HideOverheadChatMessage();
	void TrimOverheadChatBubbleCount();
	void RefreshChatBubbleTimer();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LivesText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SpeakingIndicatorText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> NameBackground;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ChatStack;

	UPROPERTY(Transient)
	TArray<FShowDownOverheadChatBubbleEntry> ChatBubbles;

	FText CachedDisplayName;
	FText CachedStatusText;
	int32 CachedLives = 3;
	FTimerHandle ChatBubbleAnimationTimerHandle;
	float CurrentSpeakingIndicatorOpacity = 0.0f;
	bool bCachedTurnActive = false;
	bool bCachedSpeakingIndicatorVisible = false;
};
