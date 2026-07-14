#pragma once

#include "CoreMinimal.h"
#include "ShowDownUserWidget.h"
#include "ShowDownTypes.h"
#include "ShowDownChatWidget.generated.h"

class APlayerPawn;
class AShowDownPlayerController;
class SWidget;
class UBorder;
class UButton;
class UCanvasPanel;
class UEditableTextBox;
class UHorizontalBox;
class USizeBox;
class UScrollBox;
class UTextBlock;
class UVerticalBox;
class UWidget;

UCLASS()
class SHOWDOWN_API UShowDownChatWidget : public UShowDownUserWidget
{
	GENERATED_BODY()

public:
	void SetOwningShowDownPawn(APlayerPawn* InOwningPawn);
	void SetOwningShowDownController(AShowDownPlayerController* InOwningController);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Chat")
	void FocusChatInput();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Chat")
	void AppendChatLine(const FString& Speaker, const FString& Message);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Chat")
	void SetChatInputOpen(bool bOpen);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Chat")
	bool IsChatInputFocused() const;

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Voice")
	void SetLocalSpeakingIndicatorVisible(bool bVisible);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	struct FRenderedChatLine
	{
		TWeakObjectPtr<UWidget> RowWidget;
		float SpawnTimeSeconds = 0.0f;
	};

	UPROPERTY()
	APlayerPawn* OwningShowDownPawn = nullptr;

	UPROPERTY()
	AShowDownPlayerController* OwningShowDownController = nullptr;

	TWeakObjectPtr<class AShowDownGameStateBase> BoundGameState;

	UPROPERTY(meta = (BindWidgetOptional))
	UEditableTextBox* EditableTextBox_ChatInput = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* Button_Send = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* Text_ChatHistory = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UScrollBox* ScrollBox_ChatHistory = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* Text_Status = nullptr;

	UPROPERTY()
	UTextBlock* Text_LocalSpeakingIndicator = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UBorder* Border_ChatHistoryBackground = nullptr;

	UPROPERTY()
	UBorder* Border_ChatInputBackground = nullptr;

	UPROPERTY()
	UBorder* Border_ChatInputAccent = nullptr;

	UPROPERTY()
	USizeBox* SizeBox_ChatRoot = nullptr;

	UPROPERTY()
	UVerticalBox* VerticalBox_ChatRoot = nullptr;

	UPROPERTY()
	FString ChatHistory;

	TArray<FRenderedChatLine> RenderedChatLines;

	bool bChatInputOpen = false;
	bool bChatInputClosing = false;
	bool bChatHistoryUsesDynamicRows = false;
	float ChatInputStateChangedTimeSeconds = 0.0f;
	float LastChatLineTimeSeconds = -1000.0f;
	float CurrentHistoryBackgroundAlpha = 0.0f;
	float CurrentHistoryOpacity = 1.0f;
	float CurrentLocalSpeakingIndicatorOpacity = 0.0f;
	bool bLocalSpeakingIndicatorVisible = false;

	UFUNCTION()
	void HandleSendClicked();

	UFUNCTION()
	void HandleInputCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void HandleCollectorLLMDecision(const FString& Dialogue, const FString& Intent, EShowDownBetAction Action, int32 TargetBet);

	UFUNCTION()
	void HandleCollectorLLMStatus(bool bSuccess, const FString& Message);

	UFUNCTION()
	void HandleChatMessageReceived(const FString& SenderName, const FString& Message);

	void BuildNativeChatLayout();
	void AppendDynamicChatLine(const FString& Speaker, const FString& Message);
	UHorizontalBox* CreateChatLineWidget(const FString& Speaker, const FString& Message);
	FString ResolveDisplaySpeakerName(const FString& Speaker) const;
	FString ResolveLocalSpeakerName() const;
	FLinearColor ResolveSpeakerColor(const FString& Speaker) const;
	bool IsSystemSpeaker(const FString& Speaker) const;
	void TrimRenderedChatLines();
	void ScrollChatHistoryToEnd();
	void UpdateChatVisualState(float InDeltaTime);
	void ApplyInputVisibility(ESlateVisibility NewVisibility);
	void SetStatusMessage(const FString& Message, const FLinearColor& Color);
	bool SubmitCurrentText();
};
