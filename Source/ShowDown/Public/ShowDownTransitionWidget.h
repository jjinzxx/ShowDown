#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShowDownTransitionWidget.generated.h"

class UTextBlock;

/**
 * Full-screen transition veil used while EOS operations or multiplayer startup
 * are in progress. It is deliberately native so every authored menu can share
 * the same blocking, animated feedback without requiring Blueprint changes.
 */
UCLASS()
class SHOWDOWN_API UShowDownTransitionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Transition")
	void SetTransitionText(const FString& Title, const FString& Detail);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Transition")
	void Dismiss(float FadeOutDuration = 0.18f);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	FString CachedTitle = TEXT("LOADING");
	FString CachedDetail = TEXT("잠시만 기다려 주세요.");
	float AnimationElapsed = 0.0f;
	float DismissElapsed = 0.0f;
	float DismissDuration = 0.18f;
	bool bDismissing = false;

	UPROPERTY()
	TObjectPtr<UTextBlock> Text_Title = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> Text_Detail = nullptr;

	void RefreshText();
};
