#include "ShowDownShopWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "ShowDownMainMenuWidget.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"

namespace
{
	const FLinearColor ShopInk(0.96f, 0.90f, 0.76f, 1.0f);
	const FLinearColor ShopMutedInk(0.72f, 0.69f, 0.62f, 1.0f);
	const FLinearColor ShopError(0.94f, 0.2f, 0.18f, 1.0f);
	const FLinearColor ShopSuccess(0.35f, 0.88f, 0.55f, 1.0f);
	const TCHAR* ShopCurrencyIconObjectPath =
		TEXT("/Game/UI/Icons/T_UI_Currency.T_UI_Currency");

	FSlateBrush MakeFlatBrush(const FLinearColor& Color)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.TintColor = FSlateColor(Color);
		Brush.Margin = FMargin(0.08f);
		return Brush;
	}

	FButtonStyle MakeShopButtonStyle(const FLinearColor& Accent)
	{
		FButtonStyle Style;
		Style.SetNormal(MakeFlatBrush(FLinearColor(0.012f, 0.012f, 0.011f, 0.64f)));
		Style.SetHovered(MakeFlatBrush(FLinearColor(
			FMath::Max(0.08f, Accent.R * 0.26f),
			FMath::Max(0.06f, Accent.G * 0.24f),
			FMath::Max(0.025f, Accent.B * 0.18f),
			0.92f)));
		Style.SetPressed(MakeFlatBrush(FLinearColor(
			Accent.R * 0.13f,
			Accent.G * 0.12f,
			Accent.B * 0.09f,
			0.98f)));
		Style.SetDisabled(MakeFlatBrush(FLinearColor(0.018f, 0.018f, 0.017f, 0.58f)));
		Style.SetNormalPadding(FMargin(14.0f, 7.0f));
		Style.SetPressedPadding(FMargin(14.0f, 9.0f, 14.0f, 5.0f));
		return Style;
	}

	EShowDownCharacterSkinRarity ParseServerRarity(const FString& Rarity)
	{
		if (Rarity.Equals(TEXT("rare"), ESearchCase::IgnoreCase))
		{
			return EShowDownCharacterSkinRarity::Rare;
		}
		if (Rarity.Equals(TEXT("epic"), ESearchCase::IgnoreCase))
		{
			return EShowDownCharacterSkinRarity::Epic;
		}
		if (Rarity.Equals(TEXT("legendary"), ESearchCase::IgnoreCase))
		{
			return EShowDownCharacterSkinRarity::Legendary;
		}
		return EShowDownCharacterSkinRarity::Common;
	}
}

TSharedRef<SWidget> UShowDownShopWidget::RebuildWidget()
{
	BuildWidgetTreeIfNeeded();
	return Super::RebuildWidget();
}

void UShowDownShopWidget::SetMainMenuWidget(UShowDownMainMenuWidget* InMainMenuWidget)
{
	MainMenuWidget = InMainMenuWidget;
}

void UShowDownShopWidget::SetUseLegacyBackNavigation(const bool bInUseLegacyBackNavigation)
{
	bUseLegacyBackNavigation = bInUseLegacyBackNavigation;
}

void UShowDownShopWidget::SetSkinCatalog(UShowDownCharacterSkinCatalog* InSkinCatalog)
{
	SkinCatalog = InSkinCatalog;
	if (IsConstructed())
	{
		RefreshDisplayItems();
	}
}

void UShowDownShopWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);
	SetKeyboardFocus();
	bCosmeticLoadFailed = false;

	if (USupabaseSubsystem* SupabaseSubsystem = GetSupabaseSubsystem())
	{
		SupabaseSubsystem->OnCosmeticDataLoaded.AddUniqueDynamic(
			this,
			&UShowDownShopWidget::HandleCosmeticDataLoaded);
		SupabaseSubsystem->OnSkinEquipped.AddUniqueDynamic(
			this,
			&UShowDownShopWidget::HandleSkinEquipped);
		SupabaseSubsystem->OnSkinSetPurchased.AddUniqueDynamic(
			this,
			&UShowDownShopWidget::HandleSkinSetPurchased);
	}

	if (Button_Previous)
	{
		Button_Previous->OnClicked.AddUniqueDynamic(this, &UShowDownShopWidget::HandlePreviousClicked);
	}
	if (Button_Next)
	{
		Button_Next->OnClicked.AddUniqueDynamic(this, &UShowDownShopWidget::HandleNextClicked);
	}
	if (Button_PrimaryAction)
	{
		Button_PrimaryAction->OnClicked.AddUniqueDynamic(
			this,
			&UShowDownShopWidget::HandlePrimaryActionClicked);
	}
	if (Button_Back)
	{
		Button_Back->OnClicked.AddUniqueDynamic(this, &UShowDownShopWidget::HandleBackClicked);
	}

	RefreshDisplayItems();
}

void UShowDownShopWidget::NativeDestruct()
{
	// The Hub reuses this widget instance. Its placed preview actor is reset when
	// leaving the screen, so the first selection must be broadcast again when
	// the same widget is added back to the viewport.
	LastBroadcastPreviewSkinId.Empty();

	if (Button_Previous)
	{
		Button_Previous->OnClicked.RemoveDynamic(this, &UShowDownShopWidget::HandlePreviousClicked);
	}
	if (Button_Next)
	{
		Button_Next->OnClicked.RemoveDynamic(this, &UShowDownShopWidget::HandleNextClicked);
	}
	if (Button_PrimaryAction)
	{
		Button_PrimaryAction->OnClicked.RemoveDynamic(
			this,
			&UShowDownShopWidget::HandlePrimaryActionClicked);
	}
	if (Button_Back)
	{
		Button_Back->OnClicked.RemoveDynamic(this, &UShowDownShopWidget::HandleBackClicked);
	}

	if (USupabaseSubsystem* SupabaseSubsystem = GetSupabaseSubsystem())
	{
		SupabaseSubsystem->OnCosmeticDataLoaded.RemoveDynamic(
			this,
			&UShowDownShopWidget::HandleCosmeticDataLoaded);
		SupabaseSubsystem->OnSkinEquipped.RemoveDynamic(
			this,
			&UShowDownShopWidget::HandleSkinEquipped);
		SupabaseSubsystem->OnSkinSetPurchased.RemoveDynamic(
			this,
			&UShowDownShopWidget::HandleSkinSetPurchased);
	}

	Super::NativeDestruct();
}

FReply UShowDownShopWidget::NativeOnKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
	const FKey PressedKey = InKeyEvent.GetKey();
	if (PressedKey == EKeys::Left
		|| PressedKey == EKeys::A
		|| PressedKey == EKeys::Gamepad_DPad_Left
		|| PressedKey == EKeys::Gamepad_LeftShoulder)
	{
		SelectSkinByOffset(-1);
		return FReply::Handled();
	}

	if (PressedKey == EKeys::Right
		|| PressedKey == EKeys::D
		|| PressedKey == EKeys::Gamepad_DPad_Right
		|| PressedKey == EKeys::Gamepad_RightShoulder)
	{
		SelectSkinByOffset(1);
		return FReply::Handled();
	}

	if (PressedKey == EKeys::Enter
		|| PressedKey == EKeys::SpaceBar
		|| PressedKey == EKeys::Gamepad_FaceButton_Bottom)
	{
		HandlePrimaryActionClicked();
		return FReply::Handled();
	}

	if (PressedKey == EKeys::Escape || PressedKey == EKeys::Gamepad_FaceButton_Right)
	{
		HandleBackClicked();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

EShowDownShopPrimaryActionState UShowDownShopWidget::ResolvePrimaryActionState(
	const bool bHasCosmeticSnapshot,
	const bool bCosmeticLoadInFlight,
	const bool bCosmeticLoadFailed,
	const bool bHasServerProduct,
	const bool bOwned,
	const bool bEquipped,
	const bool bPurchaseInFlight,
	const bool bEquipInFlight)
{
	if (bPurchaseInFlight)
	{
		return EShowDownShopPrimaryActionState::Purchasing;
	}
	if (bEquipInFlight)
	{
		return EShowDownShopPrimaryActionState::Equipping;
	}
	if (bCosmeticLoadInFlight)
	{
		return EShowDownShopPrimaryActionState::Loading;
	}
	if (!bHasCosmeticSnapshot)
	{
		return bCosmeticLoadFailed
			? EShowDownShopPrimaryActionState::Unavailable
			: EShowDownShopPrimaryActionState::Loading;
	}
	if (!bHasServerProduct)
	{
		return EShowDownShopPrimaryActionState::Unavailable;
	}
	if (!bOwned)
	{
		return EShowDownShopPrimaryActionState::Purchase;
	}
	return bEquipped
		? EShowDownShopPrimaryActionState::Equipped
		: EShowDownShopPrimaryActionState::Equip;
}

int32 UShowDownShopWidget::WrapSelectionIndex(
	const int32 CurrentIndex,
	const int32 Offset,
	const int32 ItemCount)
{
	if (ItemCount <= 0)
	{
		return INDEX_NONE;
	}
	if (CurrentIndex < 0 || CurrentIndex >= ItemCount)
	{
		return Offset < 0 ? ItemCount - 1 : 0;
	}

	const int64 CandidateIndex = static_cast<int64>(CurrentIndex) + static_cast<int64>(Offset);
	const int64 WrappedIndex = ((CandidateIndex % ItemCount) + ItemCount) % ItemCount;
	return static_cast<int32>(WrappedIndex);
}

void UShowDownShopWidget::BuildWidgetTreeIfNeeded()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}
	if (WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("RootCanvas"));
	WidgetTree->RootWidget = Root;

	auto AddPointAnchored = [Root](
		UWidget* Widget,
		const FVector2D& Anchor,
		const FVector2D& Alignment,
		const FVector2D& Position,
		const FVector2D& Size)
	{
		UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Widget);
		Slot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
		Slot->SetAlignment(Alignment);
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
		return Slot;
	};

	auto MakeText = [this](
		const TCHAR* Name,
		const int32 Size,
		const ETextJustify::Type Justification)
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Text->SetColorAndOpacity(FSlateColor(ShopInk));
		Text->SetJustification(Justification);
		Text->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), Size));
		return Text;
	};

	auto MakeButton = [this, &MakeText](const TCHAR* Name, const TCHAR* Label)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Button->SetStyle(MakeShopButtonStyle(FLinearColor(0.76f, 0.56f, 0.22f, 1.0f)));
		UTextBlock* LabelText = MakeText(
			*FString::Printf(TEXT("%s_Label"), Name),
			20,
			ETextJustify::Center);
		LabelText->SetText(FText::FromString(Label));
		Button->SetContent(LabelText);
		return Button;
	};

	auto AddFramedButton = [this, &AddPointAnchored](
		UButton* Button,
		const TCHAR* FrameName,
		const FVector2D& Anchor,
		const FVector2D& Alignment,
		const FVector2D& Size,
		const FLinearColor& FrameColor)
	{
		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), FrameName);
		Frame->SetBrushColor(FrameColor);
		Frame->SetPadding(FMargin(1.25f));
		Frame->SetContent(Button);
		return AddPointAnchored(Frame, Anchor, Alignment, FVector2D::ZeroVector, Size);
	};

	Button_Back = MakeButton(TEXT("Button_Back"), TEXT("뒤로"));
	AddFramedButton(
		Button_Back,
		TEXT("Border_BackButtonFrame"),
		FVector2D(0.03f, 0.055f),
		FVector2D::ZeroVector,
		FVector2D(190.0f, 52.0f),
		FLinearColor(0.72f, 0.52f, 0.20f, 0.76f));

	UHorizontalBox* CoinRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		TEXT("ShopCoinRow"));
	UImage* CoinIcon = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(),
		TEXT("Image_Coin"));
	if (UTexture2D* CurrencyTexture = LoadObject<UTexture2D>(nullptr, ShopCurrencyIconObjectPath))
	{
		CoinIcon->SetBrushFromTexture(CurrencyTexture, false);
	}
	CoinIcon->SetDesiredSizeOverride(FVector2D(42.0f, 42.0f));
	if (UHorizontalBoxSlot* IconSlot = CoinRow->AddChildToHorizontalBox(CoinIcon))
	{
		IconSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		IconSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
		IconSlot->SetVerticalAlignment(VAlign_Center);
	}

	Text_Coin = MakeText(TEXT("Text_Coin"), 22, ETextJustify::Left);
	Text_Coin->SetText(FText::FromString(TEXT("0$")));
	if (UHorizontalBoxSlot* ValueSlot = CoinRow->AddChildToHorizontalBox(Text_Coin))
	{
		ValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		ValueSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UCanvasPanelSlot* CoinRowSlot = AddPointAnchored(
		CoinRow,
		FVector2D(0.97f, 0.06f),
		FVector2D(1.0f, 0.0f),
		FVector2D::ZeroVector,
		FVector2D::ZeroVector))
	{
		CoinRowSlot->SetAutoSize(true);
	}

	Text_SkinName = MakeText(TEXT("Text_SkinName"), 42, ETextJustify::Center);
	AddPointAnchored(Text_SkinName, FVector2D(0.5f, 0.09f), FVector2D(0.5f, 0.0f),
		FVector2D::ZeroVector, FVector2D(720.0f, 58.0f));

	Border_RarityAccent = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("Border_RarityAccent"));
	Border_RarityAccent->SetPadding(FMargin(18.0f, 4.0f));
	Border_RarityAccent->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.72f));
	Text_Rarity = MakeText(TEXT("Text_Rarity"), 18, ETextJustify::Center);
	Border_RarityAccent->SetContent(Text_Rarity);
	AddPointAnchored(Border_RarityAccent, FVector2D(0.5f, 0.155f), FVector2D(0.5f, 0.0f),
		FVector2D::ZeroVector, FVector2D(250.0f, 40.0f));

	Text_Description = MakeText(TEXT("Text_Description"), 17, ETextJustify::Center);
	Text_Description->SetColorAndOpacity(FSlateColor(ShopMutedInk));
	Text_Description->SetAutoWrapText(true);
	AddPointAnchored(Text_Description, FVector2D(0.5f, 0.205f), FVector2D(0.5f, 0.0f),
		FVector2D::ZeroVector, FVector2D(720.0f, 72.0f));

	Text_Price = MakeText(TEXT("Text_Price"), 24, ETextJustify::Center);
	AddPointAnchored(Text_Price, FVector2D(0.5f, 0.84f), FVector2D(0.5f, 0.5f),
		FVector2D::ZeroVector, FVector2D(340.0f, 44.0f));

	Button_Previous = MakeButton(TEXT("Button_Previous"), TEXT("◀ 이전"));
	AddFramedButton(
		Button_Previous,
		TEXT("Border_PreviousButtonFrame"),
		FVector2D(0.15f, 0.91f),
		FVector2D(0.5f, 0.5f),
		FVector2D(260.0f, 58.0f),
		FLinearColor(0.72f, 0.52f, 0.20f, 0.66f));

	Button_PrimaryAction = MakeButton(TEXT("Button_PrimaryAction"), TEXT("불러오는 중"));
	Text_PrimaryAction = Cast<UTextBlock>(Button_PrimaryAction->GetContent());
	AddFramedButton(
		Button_PrimaryAction,
		TEXT("Border_PrimaryActionButtonFrame"),
		FVector2D(0.5f, 0.91f),
		FVector2D(0.5f, 0.5f),
		FVector2D(380.0f, 66.0f),
		FLinearColor(0.76f, 0.56f, 0.22f, 0.82f));

	Button_Next = MakeButton(TEXT("Button_Next"), TEXT("다음 ▶"));
	AddFramedButton(
		Button_Next,
		TEXT("Border_NextButtonFrame"),
		FVector2D(0.85f, 0.91f),
		FVector2D(0.5f, 0.5f),
		FVector2D(260.0f, 58.0f),
		FLinearColor(0.72f, 0.52f, 0.20f, 0.66f));

	Text_Status = MakeText(TEXT("Text_Status"), 16, ETextJustify::Center);
	Text_Status->SetColorAndOpacity(FSlateColor(ShopMutedInk));
	AddPointAnchored(Text_Status, FVector2D(0.5f, 0.975f), FVector2D(0.5f, 0.5f),
		FVector2D::ZeroVector, FVector2D(900.0f, 42.0f));
}

void UShowDownShopWidget::RefreshDisplayItems(const bool bAllowPreviewBroadcast)
{
	FString PreviousSkinId;
	if (const FShopDisplayItem* PreviousItem = GetSelectedItem())
	{
		PreviousSkinId = PreviousItem->CharacterSkinId;
	}

	DisplayItems.Reset();
	SelectedItemIndex = INDEX_NONE;

	USupabaseSubsystem* SupabaseSubsystem = GetSupabaseSubsystem();
	TMap<FString, FShowDownSkin> ProductsByCharacterSkinId;
	TArray<FString> ProductSkinOrder;

	if (SupabaseSubsystem)
	{
		for (const FShowDownSkin& Product : SupabaseSubsystem->GetShopSkins())
		{
			if (!ShouldShowSkinAsShopOption(Product))
			{
				continue;
			}

			const FString CharacterSkinId = UShowDownCharacterSkinCatalog::CanonicalizeSkinId(
				SupabaseSubsystem->GetSkinIdForShopSet(Product.Id, TEXT("character")));
			if (CharacterSkinId.IsEmpty())
			{
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("Character shop set '%s' has no character skin item."),
					*Product.Id);
				continue;
			}

			if (ProductsByCharacterSkinId.Contains(CharacterSkinId))
			{
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("Multiple active shop sets target character skin '%s'; keeping the first."),
					*CharacterSkinId);
				continue;
			}

			ProductsByCharacterSkinId.Add(CharacterSkinId, Product);
			ProductSkinOrder.Add(CharacterSkinId);
		}
	}

	TSet<FString> ConsumedProductSkinIds;
	TArray<FShowDownCharacterSkinDefinition> OrderedDefinitions;
	UShowDownCharacterSkinCatalog::GetOrderedSkinDefinitions(SkinCatalog, OrderedDefinitions);
	for (const FShowDownCharacterSkinDefinition& Definition : OrderedDefinitions)
	{
		const FString CharacterSkinId = UShowDownCharacterSkinCatalog::CanonicalizeSkinId(
			Definition.SkinId);
		if (CharacterSkinId.IsEmpty())
		{
			continue;
		}

		FShopDisplayItem& Item = DisplayItems.AddDefaulted_GetRef();
		Item.CharacterSkinId = CharacterSkinId;
		Item.Presentation = Definition;
		if (const FShowDownSkin* Product = ProductsByCharacterSkinId.Find(CharacterSkinId))
		{
			Item.Product = *Product;
			Item.SetId = Product->Id;
			Item.bHasServerProduct = true;
			ConsumedProductSkinIds.Add(CharacterSkinId);
		}
	}

	for (const FString& CharacterSkinId : ProductSkinOrder)
	{
		if (ConsumedProductSkinIds.Contains(CharacterSkinId))
		{
			continue;
		}

		const FShowDownSkin* Product = ProductsByCharacterSkinId.Find(CharacterSkinId);
		if (!Product)
		{
			continue;
		}

		FShopDisplayItem& Item = DisplayItems.AddDefaulted_GetRef();
		Item.CharacterSkinId = CharacterSkinId;
		Item.Product = *Product;
		Item.SetId = Product->Id;
		Item.bHasServerProduct = true;

		FString ResolvedSkinId;
		UShowDownCharacterSkinCatalog::ResolveSkinDefinition(
			SkinCatalog,
			CharacterSkinId,
			Item.Presentation,
			ResolvedSkinId);
		if (!UShowDownCharacterSkinCatalog::CanonicalizeSkinId(ResolvedSkinId).Equals(CharacterSkinId))
		{
			// The backend may be deployed ahead of a client content build. Keep the
			// item visible as unavailable for diagnostics, but never charge for or
			// equip a skin that this build can only render as the robot fallback.
			Item.bHasServerProduct = false;
			Item.SetId.Empty();
			Item.Presentation.SkinId = CharacterSkinId;
			Item.Presentation.DisplayName = FText::FromString(Product->Name);
			Item.Presentation.Description = FText::GetEmpty();
			Item.Presentation.Rarity = ParseServerRarity(Product->Rarity);
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("Character shop product '%s' targets skin '%s', which is missing from this build's catalog. Purchase and equip are disabled."),
				*Product->Id,
				*CharacterSkinId);
		}
	}

	int32 NewSelectionIndex = INDEX_NONE;
	if (!PreviousSkinId.IsEmpty())
	{
		NewSelectionIndex = DisplayItems.IndexOfByPredicate(
			[&PreviousSkinId](const FShopDisplayItem& Item)
			{
				return Item.CharacterSkinId.Equals(PreviousSkinId, ESearchCase::IgnoreCase);
			});
	}

	if (NewSelectionIndex == INDEX_NONE && SupabaseSubsystem)
	{
		const FString EquippedSkinId = UShowDownCharacterSkinCatalog::CanonicalizeSkinId(
			SupabaseSubsystem->GetEquippedSkinId(TEXT("character")));
		NewSelectionIndex = DisplayItems.IndexOfByPredicate(
			[&EquippedSkinId](const FShopDisplayItem& Item)
			{
				return Item.CharacterSkinId.Equals(EquippedSkinId, ESearchCase::IgnoreCase);
			});
	}

	if (NewSelectionIndex == INDEX_NONE)
	{
		const FString DefaultSkinId = UShowDownCharacterSkinCatalog::GetDefaultSkinId();
		NewSelectionIndex = DisplayItems.IndexOfByPredicate(
			[&DefaultSkinId](const FShopDisplayItem& Item)
			{
				return Item.CharacterSkinId.Equals(DefaultSkinId, ESearchCase::IgnoreCase);
			});
	}
	if (NewSelectionIndex == INDEX_NONE && DisplayItems.Num() > 0)
	{
		NewSelectionIndex = 0;
	}

	SelectSkinIndex(NewSelectionIndex, bAllowPreviewBroadcast);
}

void UShowDownShopWidget::RefreshSelectedPresentation()
{
	const FShopDisplayItem* Item = GetSelectedItem();
	USupabaseSubsystem* SupabaseSubsystem = GetSupabaseSubsystem();
	if (Text_Coin)
	{
		Text_Coin->SetText(FText::FromString(FString::Printf(
			TEXT("%s$"),
			*FText::AsNumber(
				SupabaseSubsystem ? SupabaseSubsystem->GetCoin() : 0).ToString())));
	}

	if (!Item)
	{
		if (Text_SkinName) Text_SkinName->SetText(FText::FromString(TEXT("스킨 없음")));
		if (Text_Rarity) Text_Rarity->SetText(FText::GetEmpty());
		if (Text_Description) Text_Description->SetText(FText::GetEmpty());
		if (Text_Price) Text_Price->SetText(FText::FromString(TEXT("-")));
		if (Text_PrimaryAction) Text_PrimaryAction->SetText(FText::FromString(TEXT("사용 불가")));
		if (Button_PrimaryAction) Button_PrimaryAction->SetIsEnabled(false);
		if (Button_Previous) Button_Previous->SetIsEnabled(false);
		if (Button_Next) Button_Next->SetIsEnabled(false);
		return;
	}

	const EShowDownShopPrimaryActionState ActionState = GetCurrentPrimaryActionState();
	const EShowDownCharacterSkinRarity Rarity =
		Item->Presentation.Rarity == EShowDownCharacterSkinRarity::Unspecified
			? ParseServerRarity(Item->Product.Rarity)
			: Item->Presentation.Rarity;
	const FLinearColor RarityColor = UShowDownCharacterSkinCatalog::GetRarityColor(Rarity);

	FText DisplayName = Item->Presentation.DisplayName;
	if (DisplayName.IsEmpty())
	{
		DisplayName = !Item->Product.Name.IsEmpty()
			? FText::FromString(Item->Product.Name)
			: FText::FromString(Item->CharacterSkinId);
	}

	if (Text_SkinName)
	{
		Text_SkinName->SetText(DisplayName);
		Text_SkinName->SetColorAndOpacity(FSlateColor(RarityColor));
	}
	if (Text_Rarity)
	{
		Text_Rarity->SetText(UShowDownCharacterSkinCatalog::GetRarityDisplayName(Rarity));
		Text_Rarity->SetColorAndOpacity(FSlateColor(RarityColor));
	}
	if (Border_RarityAccent)
	{
		Border_RarityAccent->SetBrushColor(FLinearColor(
			RarityColor.R * 0.18f,
			RarityColor.G * 0.18f,
			RarityColor.B * 0.18f,
			0.88f));
	}
	if (Text_Description)
	{
		Text_Description->SetText(Item->Presentation.Description);
	}
	if (Text_Price)
	{
		Text_Price->SetText(Item->bHasServerProduct
			? FText::FromString(FString::Printf(TEXT("%d COIN"), Item->Product.Price))
			: FText::FromString(TEXT("-")));
		Text_Price->SetColorAndOpacity(FSlateColor(RarityColor));
	}
	FString ActionLabel;
	switch (ActionState)
	{
	case EShowDownShopPrimaryActionState::Purchase:
		ActionLabel = TEXT("구매");
		break;
	case EShowDownShopPrimaryActionState::Equip:
		ActionLabel = TEXT("적용");
		break;
	case EShowDownShopPrimaryActionState::Equipped:
		ActionLabel = TEXT("적용됨");
		break;
	case EShowDownShopPrimaryActionState::Purchasing:
		ActionLabel = TEXT("처리 중");
		break;
	case EShowDownShopPrimaryActionState::Equipping:
		ActionLabel = TEXT("적용 중");
		break;
	case EShowDownShopPrimaryActionState::Loading:
		ActionLabel = TEXT("불러오는 중");
		break;
	case EShowDownShopPrimaryActionState::Unavailable:
	default:
		ActionLabel = TEXT("판매 준비 중");
		break;
	}

	if (Text_PrimaryAction)
	{
		Text_PrimaryAction->SetText(FText::FromString(ActionLabel));
	}
	if (Button_PrimaryAction)
	{
		Button_PrimaryAction->SetIsEnabled(
			ActionState == EShowDownShopPrimaryActionState::Purchase
			|| ActionState == EShowDownShopPrimaryActionState::Equip);
	}

	const bool bOperationInFlight = SupabaseSubsystem && SupabaseSubsystem->IsShopOperationInFlight();
	const bool bCanNavigate = DisplayItems.Num() > 1 && !bOperationInFlight;
	if (Button_Previous) Button_Previous->SetIsEnabled(bCanNavigate);
	if (Button_Next) Button_Next->SetIsEnabled(bCanNavigate);

	if (ActionState == EShowDownShopPrimaryActionState::Purchasing)
	{
		SetStatusMessage(TEXT("구매를 처리하고 있습니다..."), ShopMutedInk);
	}
	else if (ActionState == EShowDownShopPrimaryActionState::Equipping)
	{
		SetStatusMessage(TEXT("스킨을 적용하고 있습니다..."), ShopMutedInk);
	}
	else if (ActionState == EShowDownShopPrimaryActionState::Loading)
	{
		SetStatusMessage(TEXT("상점 데이터를 불러오는 중입니다..."), ShopMutedInk);
	}
}

void UShowDownShopWidget::SelectSkinByOffset(const int32 Offset)
{
	if (USupabaseSubsystem* SupabaseSubsystem = GetSupabaseSubsystem();
		SupabaseSubsystem && SupabaseSubsystem->IsShopOperationInFlight())
	{
		return;
	}

	const int32 NextIndex = WrapSelectionIndex(SelectedItemIndex, Offset, DisplayItems.Num());
	if (NextIndex != INDEX_NONE)
	{
		SetStatusMessage(TEXT(""), ShopMutedInk);
		SelectSkinIndex(NextIndex);
	}
}

void UShowDownShopWidget::SelectSkinIndex(
	const int32 NewIndex,
	const bool bAllowPreviewBroadcast)
{
	SelectedItemIndex = DisplayItems.IsValidIndex(NewIndex) ? NewIndex : INDEX_NONE;
	RefreshSelectedPresentation();
	if (bAllowPreviewBroadcast)
	{
		BroadcastSelectedPreviewSkin();
	}
}

void UShowDownShopWidget::BroadcastSelectedPreviewSkin()
{
	const FShopDisplayItem* Item = GetSelectedItem();
	const FString PreviewSkinId = Item
		? Item->CharacterSkinId
		: UShowDownCharacterSkinCatalog::GetDefaultSkinId();
	if (!PreviewSkinId.IsEmpty() && PreviewSkinId != LastBroadcastPreviewSkinId)
	{
		LastBroadcastPreviewSkinId = PreviewSkinId;
		OnPreviewSkinChanged.Broadcast(PreviewSkinId);
	}
}

void UShowDownShopWidget::SetStatusMessage(
	const FString& Message,
	const FLinearColor& Color)
{
	if (Text_Status)
	{
		Text_Status->SetText(FText::FromString(Message));
		Text_Status->SetColorAndOpacity(FSlateColor(Color));
	}
}

bool UShowDownShopWidget::ShouldShowSkinAsShopOption(const FShowDownSkin& Skin) const
{
	if (!Skin.bIsActive)
	{
		return false;
	}

	const USupabaseSubsystem* SupabaseSubsystem = GetSupabaseSubsystem();
	return Skin.Type.Equals(TEXT("character"), ESearchCase::IgnoreCase)
		|| (SupabaseSubsystem
			&& !SupabaseSubsystem->GetSkinIdForShopSet(Skin.Id, TEXT("character")).IsEmpty());
}

EShowDownShopPrimaryActionState UShowDownShopWidget::GetCurrentPrimaryActionState() const
{
	const FShopDisplayItem* Item = GetSelectedItem();
	const USupabaseSubsystem* SupabaseSubsystem = GetSupabaseSubsystem();
	const bool bHasProduct = Item && Item->bHasServerProduct;
	const bool bOwned = bHasProduct && SupabaseSubsystem
		&& SupabaseSubsystem->IsSkinOwned(Item->SetId);
	const bool bEquipped = bHasProduct && SupabaseSubsystem
		&& SupabaseSubsystem->IsShopItemEquipped(Item->SetId);

	return ResolvePrimaryActionState(
		SupabaseSubsystem && SupabaseSubsystem->HasCosmeticDataSnapshot(),
		SupabaseSubsystem && SupabaseSubsystem->IsCosmeticDataLoadInFlight(),
		bCosmeticLoadFailed,
		bHasProduct,
		bOwned,
		bEquipped,
		SupabaseSubsystem && SupabaseSubsystem->IsSkinPurchaseInFlight(),
		SupabaseSubsystem && SupabaseSubsystem->IsSkinEquipInFlight());
}

USupabaseSubsystem* UShowDownShopWidget::GetSupabaseSubsystem() const
{
	UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<USupabaseSubsystem>() : nullptr;
}

const UShowDownShopWidget::FShopDisplayItem* UShowDownShopWidget::GetSelectedItem() const
{
	return DisplayItems.IsValidIndex(SelectedItemIndex)
		? &DisplayItems[SelectedItemIndex]
		: nullptr;
}

void UShowDownShopWidget::HandlePreviousClicked()
{
	SelectSkinByOffset(-1);
}

void UShowDownShopWidget::HandleNextClicked()
{
	SelectSkinByOffset(1);
}

void UShowDownShopWidget::HandlePrimaryActionClicked()
{
	FShopDisplayItem const* Item = GetSelectedItem();
	USupabaseSubsystem* SupabaseSubsystem = GetSupabaseSubsystem();
	if (!Item || !SupabaseSubsystem)
	{
		SetStatusMessage(TEXT("상점 데이터를 사용할 수 없습니다."), ShopError);
		return;
	}

	switch (GetCurrentPrimaryActionState())
	{
	case EShowDownShopPrimaryActionState::Purchase:
		SetStatusMessage(TEXT("구매를 요청했습니다..."), ShopMutedInk);
		SupabaseSubsystem->PurchaseSkinSet(Item->SetId);
		break;
	case EShowDownShopPrimaryActionState::Equip:
		SetStatusMessage(TEXT("스킨 적용을 요청했습니다..."), ShopMutedInk);
		SupabaseSubsystem->EquipSkin(Item->SetId);
		break;
	default:
		return;
	}

	RefreshSelectedPresentation();
}

void UShowDownShopWidget::HandleBackClicked()
{
	OnBackRequested.Broadcast();
	if (!bUseLegacyBackNavigation)
	{
		return;
	}

	if (MainMenuWidget)
	{
		MainMenuWidget->SetVisibility(ESlateVisibility::Visible);
	}
	RemoveFromParent();
}

void UShowDownShopWidget::HandleCosmeticDataLoaded(
	const bool bSuccess,
	const FString& Message)
{
	const USupabaseSubsystem* SupabaseSubsystem = GetSupabaseSubsystem();
	bCosmeticLoadFailed = !bSuccess
		&& (!SupabaseSubsystem || !SupabaseSubsystem->HasCosmeticDataSnapshot());
	RefreshDisplayItems();
	if (bSuccess)
	{
		SetStatusMessage(TEXT(""), ShopMutedInk);
	}
	else
	{
		SetStatusMessage(Message, ShopError);
	}
}

void UShowDownShopWidget::HandleSkinEquipped(
	const bool bSuccess,
	const FString& Message)
{
	RefreshDisplayItems(false);
	SetStatusMessage(
		bSuccess ? TEXT("스킨을 적용했습니다.") : Message,
		bSuccess ? ShopSuccess : ShopError);
}

void UShowDownShopWidget::HandleSkinSetPurchased(
	const bool bSuccess,
	const FString& Message)
{
	RefreshDisplayItems(false);
	SetStatusMessage(
		bSuccess ? TEXT("구매가 완료되었습니다. 이제 스킨을 적용할 수 있습니다.") : Message,
		bSuccess ? ShopSuccess : ShopError);
}
