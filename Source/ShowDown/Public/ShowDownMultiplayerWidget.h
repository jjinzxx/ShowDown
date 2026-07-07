#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShowDownEosSubsystem.h"
#include "ShowDownMultiplayerWidget.generated.h"

class UButton;
class UEditableTextBox;
class UScrollBox;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnShowDownMultiplayerRequest);
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

	UFUNCTION()
	void HandleJoinClicked();
};

UCLASS()
class SHOWDOWN_API UShowDownMultiplayerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Multiplayer")
	FOnShowDownMultiplayerRequest OnHostRequested;

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Multiplayer")
	FOnShowDownMultiplayerRequest OnPrivateHostRequested;

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

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UPROPERTY()
	UTextBlock* Text_Status;

	UPROPERTY()
	UButton* Button_Host;

	UPROPERTY()
	UButton* Button_PrivateHost;
	
	UPROPERTY()
	UButton* Button_Join;

	UPROPERTY()
	UButton* Button_RefreshRooms;

	UPROPERTY()
	UButton* Button_Back;

	UPROPERTY()
	UEditableTextBox* EditableTextBox_RoomCode;

	UPROPERTY()
	UScrollBox* ScrollBox_PublicRooms;

	UPROPERTY()
	TArray<UShowDownPublicRoomEntryWidget*> PublicRoomEntries;

	void BuildDefaultLayout();
	UButton* CreateMenuButton(const FString& Label);
	UTextBlock* CreateTextBlock(const FString& Text, int32 FontSize, const FLinearColor& Color, ETextJustify::Type Justification = ETextJustify::Left);
	void SetButtonColor(UButton* Button, const FLinearColor& Color);

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
