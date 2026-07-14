#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Engine/TimerHandle.h"
#include "ShowDownEosSubsystem.h"
#include "ShowDownMultiplayerWidget.generated.h"

class UButton;
class UEditableTextBox;
class UScrollBox;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnShowDownMultiplayerRequest);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShowDownHostRoomRequest, const FString&, RoomName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShowDownJoinRoomRequest, const FString&, RoomCode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShowDownJoinPublicRoomRequest, int32, SearchResultIndex);

UCLASS()
class SHOWDOWN_API UShowDownPublicRoomEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Multiplayer")
	FOnShowDownJoinPublicRoomRequest OnJoinRequested;

	void SetRoomInfo(const FShowDownPublicRoomInfo& InRoomInfo);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	FShowDownPublicRoomInfo RoomInfo;

	UPROPERTY()
	UButton* Button_Join = nullptr;

	UPROPERTY()
	UTextBlock* Text_RoomName = nullptr;

	UPROPERTY()
	UTextBlock* Text_RoomCode = nullptr;

	UPROPERTY()
	UTextBlock* Text_PlayerCount = nullptr;

	void RefreshRoomInfo();

	UFUNCTION()
	void HandleJoinClicked();
};

UCLASS()
class SHOWDOWN_API UShowDownMultiplayerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Multiplayer")
	FOnShowDownHostRoomRequest OnHostRequested;

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Multiplayer")
	FOnShowDownHostRoomRequest OnPrivateHostRequested;

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Multiplayer")
	FOnShowDownJoinRoomRequest OnJoinRequested;

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Multiplayer")
	FOnShowDownMultiplayerRequest OnRefreshRoomsRequested;

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Multiplayer")
	FOnShowDownJoinPublicRoomRequest OnJoinPublicRoomRequested;

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Multiplayer")
	FOnShowDownMultiplayerRequest OnBackRequested;

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Multiplayer")
	void ShowStatusMessage(const FString& Message, const FLinearColor& Color);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Multiplayer")
	void SetPublicRooms(const TArray<FShowDownPublicRoomInfo>& Rooms);

	/** Blocks duplicate actions while an EOS create/join request is pending. */
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Multiplayer")
	void SetInteractionPending(bool bPending);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UPROPERTY(meta=(BindWidget))
	UTextBlock* Text_Status;

	UPROPERTY(meta=(BindWidget))
	UButton* Button_Host;

	UPROPERTY(meta=(BindWidget))
	UButton* Button_PrivateHost;
	
	UPROPERTY(meta=(BindWidget))
	UButton* Button_Join;

	UPROPERTY(meta=(BindWidget))
	UButton* Button_RefreshRooms;

	UPROPERTY(meta=(BindWidget))
	UButton* Button_Back;

	UPROPERTY(meta=(BindWidget))
	UEditableTextBox* EditableTextBox_RoomCode;

	UPROPERTY(meta=(BindWidget))
	UEditableTextBox* EditableTextBox_RoomName;

	UPROPERTY(meta=(BindWidget))
	UScrollBox* ScrollBox_PublicRooms;

	UPROPERTY()
	TArray<UShowDownPublicRoomEntryWidget*> PublicRoomEntries;
	TArray<FShowDownPublicRoomInfo> CachedPublicRooms;
	bool bHasPublicRoomSnapshot = false;

	FTimerHandle PublicRoomAutoRefreshTimerHandle;
	bool bInteractionPending = false;

	void BuildDefaultLayout();
	UButton* CreateMenuButton(const FString& Label);
	UTextBlock* CreateTextBlock(const FString& Text, int32 FontSize, const FLinearColor& Color, ETextJustify::Type Justification = ETextJustify::Left);
	void SetButtonColor(UButton* Button, const FLinearColor& Color);
	void StartPublicRoomAutoRefresh();
	void StopPublicRoomAutoRefresh();
	void HandlePublicRoomAutoRefresh();

	UFUNCTION()
	void HandleHostClicked();

	UFUNCTION()
	void HandlePrivateHostClicked();

	UFUNCTION()
	void HandleJoinClicked();

	UFUNCTION()
	void HandleRefreshRoomsClicked();

	UFUNCTION()
	void HandlePublicRoomJoinRequested(int32 SearchResultIndex);

	UFUNCTION()
	void HandleBackClicked();
};
