#pragma once

#include "CoreMinimal.h"
#include "ShowDownCharacterSkinCatalog.h"
#include "ShowDownUserWidget.h"
#include "SupabaseSubsystem.h"
#include "Types/SlateEnums.h"
#include "ShowDownShopWidget.generated.h"

class UBorder;
class UButton;
class UShowDownMainMenuWidget;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnShowDownShopBackRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnShowDownShopPreviewSkinChanged,
	const FString&,
	SkinId);

UENUM(BlueprintType)
enum class EShowDownShopPrimaryActionState : uint8
{
	Loading,
	Unavailable,
	Purchase,
	Equip,
	Equipped,
	Purchasing,
	Equipping
};

/** Character-only carousel shop backed by the existing Supabase cosmetic flow. */
UCLASS()
class SHOWDOWN_API UShowDownShopWidget : public UShowDownUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Flow")
	FOnShowDownShopBackRequested OnBackRequested;

	/** Concrete character skins.id selected by the carousel. */
	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Shop")
	FOnShowDownShopPreviewSkinChanged OnPreviewSkinChanged;

	void SetMainMenuWidget(UShowDownMainMenuWidget* InMainMenuWidget);
	void SetUseLegacyBackNavigation(bool bInUseLegacyBackNavigation);
	void SetSkinCatalog(UShowDownCharacterSkinCatalog* InSkinCatalog);

	static EShowDownShopPrimaryActionState ResolvePrimaryActionState(
		bool bHasCosmeticSnapshot,
		bool bCosmeticLoadInFlight,
		bool bCosmeticLoadFailed,
		bool bHasServerProduct,
		bool bOwned,
		bool bEquipped,
		bool bPurchaseInFlight,
		bool bEquipInFlight);

	static int32 WrapSelectionIndex(int32 CurrentIndex, int32 Offset, int32 ItemCount);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(
		const FGeometry& InGeometry,
		const FKeyEvent& InKeyEvent) override;

private:
	struct FShopDisplayItem
	{
		FString CharacterSkinId;
		FString SetId;
		FShowDownSkin Product;
		FShowDownCharacterSkinDefinition Presentation;
		bool bHasServerProduct = false;
	};

	UPROPERTY()
	TObjectPtr<UShowDownMainMenuWidget> MainMenuWidget;

	UPROPERTY()
	TObjectPtr<UShowDownCharacterSkinCatalog> SkinCatalog;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_RarityAccent;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_SkinName;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Rarity;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Description;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Price;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Coin;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Status;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_PrimaryAction;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Previous;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_PrimaryAction;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Next;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Back;

	TArray<FShopDisplayItem> DisplayItems;
	int32 SelectedItemIndex = INDEX_NONE;
	FString LastBroadcastPreviewSkinId;
	bool bUseLegacyBackNavigation = true;
	bool bCosmeticLoadFailed = false;

	void BuildWidgetTreeIfNeeded();
	void RefreshDisplayItems(bool bAllowPreviewBroadcast = true);
	void RefreshSelectedPresentation();
	void SelectSkinByOffset(int32 Offset);
	void SelectSkinIndex(int32 NewIndex, bool bAllowPreviewBroadcast = true);
	void BroadcastSelectedPreviewSkin();
	void SetStatusMessage(const FString& Message, const FLinearColor& Color);
	bool ShouldShowSkinAsShopOption(const FShowDownSkin& Skin) const;
	EShowDownShopPrimaryActionState GetCurrentPrimaryActionState() const;
	USupabaseSubsystem* GetSupabaseSubsystem() const;
	const FShopDisplayItem* GetSelectedItem() const;

	UFUNCTION()
	void HandlePreviousClicked();

	UFUNCTION()
	void HandleNextClicked();

	UFUNCTION()
	void HandlePrimaryActionClicked();

	UFUNCTION()
	void HandleBackClicked();

	UFUNCTION()
	void HandleCosmeticDataLoaded(bool bSuccess, const FString& Message);

	UFUNCTION()
	void HandleSkinEquipped(bool bSuccess, const FString& Message);

	UFUNCTION()
	void HandleSkinSetPurchased(bool bSuccess, const FString& Message);
};
