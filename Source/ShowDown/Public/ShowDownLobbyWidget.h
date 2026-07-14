#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShowDownTypes.h"
#include "ShowDownLobbyWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnShowDownLobbyRequest);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShowDownLobbyKickRequest, const FString&, PlayerId);

UCLASS()
class SHOWDOWN_API UShowDownLobbyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Lobby")
	FOnShowDownLobbyRequest OnStartRequested;

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Lobby")
	FOnShowDownLobbyRequest OnLeaveRequested;

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Lobby")
	FOnShowDownLobbyKickRequest OnKickRequested;

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Lobby")
	void SetLobbyInfo(const FString& RoomName, const FString& RoomCode, bool bIsHost);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Lobby")
	void ShowStatusMessage(const FString& Message, const FLinearColor& Color);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Lobby")
	void SetInteractionPending(bool bPending);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	FString CachedRoomCode;
	FString CachedRoomName;
	FString CachedParticipantText;
	float ParticipantRefreshElapsed = 0.0f;
	bool bCachedIsHost = false;
	bool bInteractionPending = false;

	UPROPERTY(meta=(BindWidget))
	UTextBlock* Text_Code;

	UPROPERTY(meta=(BindWidget))
	UTextBlock* Text_RoomName;

	UPROPERTY(meta=(BindWidget))
	UTextBlock* Text_Status;

	UPROPERTY(meta=(BindWidgetOptional))
	UTextBlock* Text_Players;

	UPROPERTY(meta=(BindWidgetOptional))
	UTextBlock* Text_ParticipantHeader;

	UPROPERTY(meta=(BindWidgetOptional))
	UTextBlock* Text_Player1;

	UPROPERTY(meta=(BindWidgetOptional))
	UTextBlock* Text_Player2;

	UPROPERTY(meta=(BindWidgetOptional))
	UTextBlock* Text_Player3;

	UPROPERTY(meta=(BindWidgetOptional))
	UTextBlock* Text_Player4;

	UPROPERTY(meta=(BindWidget))
	UButton* Button_Start;

	UPROPERTY(meta=(BindWidget))
	UButton* Button_Leave;

	UPROPERTY(meta=(BindWidgetOptional))
	UButton* Button_KickPlayer2;

	UPROPERTY(meta=(BindWidgetOptional))
	UButton* Button_KickPlayer3;

	UPROPERTY(meta=(BindWidgetOptional))
	UButton* Button_KickPlayer4;

	void BuildDefaultLayout();
	UButton* CreateMenuButton(const FString& Label);
	void RefreshLobbyText();
	void RefreshParticipantText();
	bool HasMinimumPlayersToStart() const;
	void RequestKickForSlot(EShowDownPlayerSlot PlayerSlot);

	UFUNCTION()
	void HandleStartClicked();

	UFUNCTION()
	void HandleLeaveClicked();

	UFUNCTION()
	void HandleKickPlayer2Clicked();

	UFUNCTION()
	void HandleKickPlayer3Clicked();

	UFUNCTION()
	void HandleKickPlayer4Clicked();
};
