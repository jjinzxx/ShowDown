#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShowDownLobbyWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnShowDownLobbyRequest);

UCLASS()
class SHOWDOWN_API UShowDownLobbyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Lobby")
	FOnShowDownLobbyRequest OnStartRequested;

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Lobby")
	FOnShowDownLobbyRequest OnLeaveRequested;

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Lobby")
	void SetLobbyInfo(const FString& RoomName, const FString& RoomCode, bool bIsHost);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Lobby")
	void ShowStatusMessage(const FString& Message, const FLinearColor& Color);

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

	UPROPERTY(meta=(BindWidget))
	UTextBlock* Text_Title;

	UPROPERTY(meta=(BindWidget))
	UTextBlock* Text_Code;

	UPROPERTY(meta=(BindWidget))
	UTextBlock* Text_RoomName;

	UPROPERTY(meta=(BindWidget))
	UTextBlock* Text_Status;

	UPROPERTY(meta=(BindWidget))
	UTextBlock* Text_Players;

	UPROPERTY(meta=(BindWidget))
	UButton* Button_Start;

	UPROPERTY(meta=(BindWidget))
	UButton* Button_Leave;

	void BuildDefaultLayout();
	UButton* CreateMenuButton(const FString& Label);
	void RefreshLobbyText();
	void RefreshParticipantText();

	UFUNCTION()
	void HandleStartClicked();

	UFUNCTION()
	void HandleLeaveClicked();
};
