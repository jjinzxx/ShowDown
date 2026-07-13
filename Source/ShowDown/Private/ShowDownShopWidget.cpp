#include "ShowDownShopWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "ShowDownCharacterSkinCatalog.h"
#include "ShowDownMainMenuWidget.h"
#include "SupabaseSubsystem.h"

TSharedRef<SWidget> UShowDownShopWidget::RebuildWidget()
{
	// WBP 기반 위젯은 에디터가 WidgetTree를 만들어주지만,
	// 이 데모 Shop은 C++만으로 만들기 때문에 Slate 위젯 생성 전에 트리를 직접 구성합니다.
	BuildWidgetTreeIfNeeded();
	return Super::RebuildWidget();
}

void UShowDownShopWidget::SetMainMenuWidget(UShowDownMainMenuWidget* InMainMenuWidget)
{
	MainMenuWidget = InMainMenuWidget;
}

void UShowDownShopWidget::SetUseLegacyBackNavigation(bool bInUseLegacyBackNavigation)
{
	bUseLegacyBackNavigation = bInUseLegacyBackNavigation;
}

void UShowDownShopWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 방향키/Enter/Escape 단축키를 받기 위해 Shop 위젯 자체가 포커스를 가져야 합니다.
	SetIsFocusable(true);
	SetKeyboardFocus();

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (USupabaseSubsystem* SupabaseSubsystem = GameInstance->GetSubsystem<USupabaseSubsystem>())
		{
			SupabaseSubsystem->OnCosmeticDataLoaded.AddUniqueDynamic(
				this,
				&UShowDownShopWidget::HandleCosmeticDataLoaded
			);

			SupabaseSubsystem->OnSkinEquipped.AddUniqueDynamic(
				this,
				&UShowDownShopWidget::HandleSkinEquipped
			);

			SupabaseSubsystem->OnSkinSetPurchased.AddUniqueDynamic(
				this,
				&UShowDownShopWidget::HandleSkinSetPurchased
			);
		}
	}

	if (ComboBox_Skins)
	{
		ComboBox_Skins->OnSelectionChanged.AddUniqueDynamic(
			this,
			&UShowDownShopWidget::HandleSkinSelectionChanged
		);
	}

	if (Button_Equip)
	{
		Button_Equip->OnClicked.AddUniqueDynamic(this, &UShowDownShopWidget::HandleEquipClicked);
	}

	if (Button_Buy)
	{
		Button_Buy->OnClicked.AddUniqueDynamic(this, &UShowDownShopWidget::HandleBuyClicked);
	}

	if (Button_Refresh)
	{
		Button_Refresh->OnClicked.AddUniqueDynamic(this, &UShowDownShopWidget::HandleRefreshClicked);
	}

	if (Button_Back)
	{
		Button_Back->OnClicked.AddUniqueDynamic(this, &UShowDownShopWidget::HandleBackClicked);
	}

	RefreshSkinOptions();
}

void UShowDownShopWidget::NativeDestruct()
{
	if (ComboBox_Skins)
	{
		ComboBox_Skins->OnSelectionChanged.RemoveDynamic(this, &UShowDownShopWidget::HandleSkinSelectionChanged);
	}
	if (Button_Equip)
	{
		Button_Equip->OnClicked.RemoveDynamic(this, &UShowDownShopWidget::HandleEquipClicked);
	}
	if (Button_Buy)
	{
		Button_Buy->OnClicked.RemoveDynamic(this, &UShowDownShopWidget::HandleBuyClicked);
	}
	if (Button_Refresh)
	{
		Button_Refresh->OnClicked.RemoveDynamic(this, &UShowDownShopWidget::HandleRefreshClicked);
	}
	if (Button_Back)
	{
		Button_Back->OnClicked.RemoveDynamic(this, &UShowDownShopWidget::HandleBackClicked);
	}

	// Shop을 열고 닫을 때 이벤트가 중복 연결되지 않도록 제거합니다.
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (USupabaseSubsystem* SupabaseSubsystem = GameInstance->GetSubsystem<USupabaseSubsystem>())
		{
			SupabaseSubsystem->OnCosmeticDataLoaded.RemoveDynamic(
				this,
				&UShowDownShopWidget::HandleCosmeticDataLoaded
			);

			SupabaseSubsystem->OnSkinEquipped.RemoveDynamic(
				this,
				&UShowDownShopWidget::HandleSkinEquipped
			);

			SupabaseSubsystem->OnSkinSetPurchased.RemoveDynamic(
				this,
				&UShowDownShopWidget::HandleSkinSetPurchased
			);
		}
	}

	Super::NativeDestruct();
}

FReply UShowDownShopWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey PressedKey = InKeyEvent.GetKey();

	// 좌/우 입력은 실제 3D carousel을 붙이기 전까지 선택 index만 이동시킵니다.
	if (PressedKey == EKeys::Left || PressedKey == EKeys::A)
	{
		SelectSkinByOffset(-1);
		return FReply::Handled();
	}

	if (PressedKey == EKeys::Right || PressedKey == EKeys::D)
	{
		SelectSkinByOffset(1);
		return FReply::Handled();
	}

	if (PressedKey == EKeys::Enter || PressedKey == EKeys::SpaceBar)
	{
		HandleEquipClicked();
		return FReply::Handled();
	}

	if (PressedKey == EKeys::B)
	{
		HandleBuyClicked();
		return FReply::Handled();
	}

	if (PressedKey == EKeys::Escape)
	{
		HandleBackClicked();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UShowDownShopWidget::BuildWidgetTreeIfNeeded()
{
	// RebuildWidget이 여러 번 호출될 수 있으므로 이미 만든 경우에는 다시 만들지 않습니다.
	if (!WidgetTree || Text_Title)
	{
		return;
	}

	// Keep the left side clear for the world-space character preview and place
	// controls in an opaque panel on the right.
	UHorizontalBox* RootLayout = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		TEXT("ShopRoot"));
	WidgetTree->RootWidget = RootLayout;

	USpacer* PreviewViewport = WidgetTree->ConstructWidget<USpacer>(
		USpacer::StaticClass(),
		TEXT("PreviewViewport"));
	UHorizontalBoxSlot* PreviewSlot = RootLayout->AddChildToHorizontalBox(PreviewViewport);
	FSlateChildSize PreviewSize;
	PreviewSize.SizeRule = ESlateSizeRule::Fill;
	PreviewSize.Value = 2.0f;
	PreviewSlot->SetSize(PreviewSize);

	UBorder* RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ShopPanel"));
	RootBorder->SetPadding(FMargin(32.0f));
	RootBorder->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.02f, 0.92f));
	UHorizontalBoxSlot* PanelSlot = RootLayout->AddChildToHorizontalBox(RootBorder);
	FSlateChildSize PanelSize;
	PanelSize.SizeRule = ESlateSizeRule::Fill;
	PanelSize.Value = 1.0f;
	PanelSlot->SetSize(PanelSize);

	UVerticalBox* RootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ShopContent"));
	RootBorder->SetContent(RootBox);

	Text_Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Title"));
	Text_Title->SetText(FText::FromString(TEXT("Shop")));
	Text_Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	RootBox->AddChildToVerticalBox(Text_Title);

	Text_Status = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Status"));
	Text_Status->SetText(FText::FromString(TEXT("Left/Right: select. Enter/Space: equip. B: buy. Escape: back.")));
	Text_Status->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	RootBox->AddChildToVerticalBox(Text_Status);

	ComboBox_Skins = WidgetTree->ConstructWidget<UComboBoxString>(
		UComboBoxString::StaticClass(),
		TEXT("ComboBox_Skins")
	);
	RootBox->AddChildToVerticalBox(ComboBox_Skins);

	Text_SelectedSkin = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("Text_SelectedSkin")
	);
	Text_SelectedSkin->SetText(FText::FromString(TEXT("No skin selected.")));
	Text_SelectedSkin->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	RootBox->AddChildToVerticalBox(Text_SelectedSkin);

	UHorizontalBox* ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		TEXT("ButtonRow")
	);
	RootBox->AddChildToVerticalBox(ButtonRow);

	Button_Equip = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_Equip"));
	UTextBlock* Text_Equip = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Equip"));
	Text_Equip->SetText(FText::FromString(TEXT("Equip")));
	Text_Equip->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
	Button_Equip->AddChild(Text_Equip);
	ButtonRow->AddChild(Button_Equip);

	Button_Buy = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_Buy"));
	UTextBlock* Text_Buy = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Buy"));
	Text_Buy->SetText(FText::FromString(TEXT("Buy")));
	Text_Buy->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
	Button_Buy->AddChild(Text_Buy);
	ButtonRow->AddChild(Button_Buy);

	Button_Refresh = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_Refresh"));
	UTextBlock* Text_Refresh = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Refresh"));
	Text_Refresh->SetText(FText::FromString(TEXT("Refresh")));
	Text_Refresh->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
	Button_Refresh->AddChild(Text_Refresh);
	ButtonRow->AddChild(Button_Refresh);

	Button_Back = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_Back"));
	UTextBlock* Text_Back = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Back"));
	Text_Back->SetText(FText::FromString(TEXT("Back")));
	Text_Back->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
	Button_Back->AddChild(Text_Back);
	ButtonRow->AddChild(Button_Back);
}

void UShowDownShopWidget::RefreshSkinOptions()
{
	if (!ComboBox_Skins)
	{
		return;
	}

	FString PreviousSelectedSkinId;
	if (const FShowDownSkin* PreviousSelectedSkin = SkinsByOption.Find(SelectedOption))
	{
		// Refresh 후에도 가능하면 같은 상품을 계속 보고 있게 유지합니다.
		PreviousSelectedSkinId = PreviousSelectedSkin->Id;
	}

	ComboBox_Skins->ClearOptions();
	SkinsByOption.Empty();
	OrderedSkinOptions.Empty();
	SelectedOption.Empty();
	SelectedSkinIndex = INDEX_NONE;

	USupabaseSubsystem* SupabaseSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<USupabaseSubsystem>()
		: nullptr;

	if (!SupabaseSubsystem)
	{
		if (Text_Status)
		{
			Text_Status->SetText(FText::FromString(TEXT("Supabase subsystem not found.")));
		}

		RefreshSelectedSkinText();
		return;
	}

	const TArray<FShowDownSkin> ShopSkins = SupabaseSubsystem->GetShopSkins();
	FString PreservedOption;
	FString EquippedOption;
	FString DefaultOption;
	FString FirstOwnedOption;

	for (const FShowDownSkin& Skin : ShopSkins)
	{
		// 현재는 모든 skin_sets 상품을 보여줍니다.
		// 나중에 개발용/숨김 상품이 생기면 ShouldShowSkinAsShopOption에서 제외하면 됩니다.
		if (!ShouldShowSkinAsShopOption(Skin))
		{
			continue;
		}

		const FString OptionText = MakeSkinOptionText(Skin);
		ComboBox_Skins->AddOption(OptionText);
		SkinsByOption.Add(OptionText, Skin);
		OrderedSkinOptions.Add(OptionText);

		if (!PreviousSelectedSkinId.IsEmpty() && Skin.Id == PreviousSelectedSkinId)
		{
			PreservedOption = OptionText;
		}

		if (EquippedOption.IsEmpty() && SupabaseSubsystem->IsShopItemEquipped(Skin.Id))
		{
			EquippedOption = OptionText;
		}

		const FString CharacterSkinId = SupabaseSubsystem->GetSkinIdForShopSet(Skin.Id, TEXT("character"));
		if (DefaultOption.IsEmpty()
			&& !CharacterSkinId.IsEmpty()
			&& UShowDownCharacterSkinCatalog::CanonicalizeSkinId(CharacterSkinId).Equals(
				UShowDownCharacterSkinCatalog::GetDefaultSkinId(),
				ESearchCase::IgnoreCase))
		{
			DefaultOption = OptionText;
		}

		if (FirstOwnedOption.IsEmpty() && SupabaseSubsystem->IsSkinOwned(Skin.Id))
		{
			FirstOwnedOption = OptionText;
		}
	}

	if (!PreservedOption.IsEmpty())
	{
		SelectedOption = PreservedOption;
	}
	else if (!EquippedOption.IsEmpty())
	{
		SelectedOption = EquippedOption;
	}
	else if (!DefaultOption.IsEmpty())
	{
		SelectedOption = DefaultOption;
	}
	else if (!FirstOwnedOption.IsEmpty())
	{
		SelectedOption = FirstOwnedOption;
	}
	else if (OrderedSkinOptions.Num() > 0)
	{
		SelectedOption = OrderedSkinOptions[0];
	}

	if (!SelectedOption.IsEmpty())
	{
		SelectSkinOption(SelectedOption);
	}

	if (Text_Status)
	{
		Text_Status->SetText(
			FText::FromString(FString::Printf(TEXT("Loaded %d shop items."), OrderedSkinOptions.Num()))
		);
	}

	RefreshSelectedSkinText();
}

void UShowDownShopWidget::RefreshSelectedSkinText()
{
	if (!Text_SelectedSkin)
	{
		return;
	}

	const FShowDownSkin* SelectedSkin = SkinsByOption.Find(SelectedOption);
	USupabaseSubsystem* SupabaseSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<USupabaseSubsystem>()
		: nullptr;

	if (!SelectedSkin || !SupabaseSubsystem)
	{
		Text_SelectedSkin->SetText(FText::FromString(TEXT("No skin selected.")));
		if (Button_Equip) Button_Equip->SetIsEnabled(false);
		if (Button_Buy) Button_Buy->SetIsEnabled(false);
		return;
	}

	const bool bOwned = SupabaseSubsystem->IsSkinOwned(SelectedSkin->Id);
	const bool bEquipped = SupabaseSubsystem->IsShopItemEquipped(SelectedSkin->Id);
	if (Button_Equip) Button_Equip->SetIsEnabled(bOwned && !bEquipped);
	if (Button_Buy) Button_Buy->SetIsEnabled(!bOwned);

	// SelectedSkin은 skin_sets의 상품 정보입니다.
	// Equipped는 세트 안의 실제 스킨들이 모두 player_equipment와 일치하는지로 판단합니다.
	Text_SelectedSkin->SetText(
		FText::FromString(
			FString::Printf(
				TEXT("%d / %d\n%s\nItem Type: %s\nRarity: %s\nPrice: %d\nCoin: %d\nOwned: %s\nEquipped: %s"),
				SelectedSkinIndex + 1,
				OrderedSkinOptions.Num(),
				*SelectedSkin->Name,
				*SelectedSkin->Type,
				*SelectedSkin->Rarity,
				SelectedSkin->Price,
				SupabaseSubsystem->GetCoin(),
				bOwned ? TEXT("true") : TEXT("false"),
				bEquipped ? TEXT("true") : TEXT("false")
			)
		)
	);
}

void UShowDownShopWidget::SelectSkinByOffset(int32 Offset)
{
	if (OrderedSkinOptions.Num() == 0)
	{
		return;
	}

	int32 NextIndex = SelectedSkinIndex;

	if (NextIndex == INDEX_NONE)
	{
		NextIndex = 0;
	}
	else
	{
		// 배열 끝에서 한 번 더 이동하면 반대쪽 끝으로 순환합니다.
		NextIndex = (NextIndex + Offset + OrderedSkinOptions.Num()) % OrderedSkinOptions.Num();
	}

	SelectSkinOption(OrderedSkinOptions[NextIndex]);
}

void UShowDownShopWidget::SelectSkinOption(const FString& OptionText)
{
	if (bUpdatingSelection || !SkinsByOption.Contains(OptionText))
	{
		return;
	}

	TGuardValue<bool> SelectionGuard(bUpdatingSelection, true);

	// ComboBox 선택, 내부 선택 문자열, carousel index를 한 곳에서 동기화합니다.
	SelectedOption = OptionText;
	SelectedSkinIndex = OrderedSkinOptions.IndexOfByKey(SelectedOption);

	if (ComboBox_Skins && ComboBox_Skins->GetSelectedOption() != SelectedOption)
	{
		ComboBox_Skins->SetSelectedOption(SelectedOption);
	}

	RefreshSelectedSkinText();
	BroadcastSelectedPreviewSkin();
}

FString UShowDownShopWidget::MakeSkinOptionText(const FShowDownSkin& Skin) const
{
	USupabaseSubsystem* SupabaseSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<USupabaseSubsystem>()
		: nullptr;

	const bool bOwned = SupabaseSubsystem && SupabaseSubsystem->IsSkinOwned(Skin.Id);
	const bool bEquipped = SupabaseSubsystem && SupabaseSubsystem->IsShopItemEquipped(Skin.Id);

	return FString::Printf(
		TEXT("%s%s [%s] %s - %d (%s)"),
		bEquipped ? TEXT("* ") : TEXT(""),
		*Skin.Name,
		*Skin.Type,
		bOwned ? TEXT("Owned") : TEXT("Locked"),
		Skin.Price,
		*Skin.Id
	);
}

bool UShowDownShopWidget::ShouldShowSkinAsShopOption(const FShowDownSkin& Skin) const
{
	if (!Skin.bIsActive)
	{
		return false;
	}

	const USupabaseSubsystem* SupabaseSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<USupabaseSubsystem>()
		: nullptr;

	// This first shop release intentionally exposes character products only.
	// Set contents cover legacy rows whose skin_sets.type is missing.
	return Skin.Type.Equals(TEXT("character"), ESearchCase::IgnoreCase)
		|| (SupabaseSubsystem
			&& !SupabaseSubsystem->GetSkinIdForShopSet(Skin.Id, TEXT("character")).IsEmpty());
}

void UShowDownShopWidget::BroadcastSelectedPreviewSkin()
{
	const FShowDownSkin* SelectedSkin = SkinsByOption.Find(SelectedOption);
	const USupabaseSubsystem* SupabaseSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<USupabaseSubsystem>()
		: nullptr;

	FString PreviewSkinId;
	if (SelectedSkin && SupabaseSubsystem)
	{
		PreviewSkinId = SupabaseSubsystem->GetSkinIdForShopSet(SelectedSkin->Id, TEXT("character"));
	}

	if (PreviewSkinId.IsEmpty())
	{
		PreviewSkinId = UShowDownCharacterSkinCatalog::GetDefaultSkinId();
	}

	OnPreviewSkinChanged.Broadcast(PreviewSkinId);
}

void UShowDownShopWidget::HandleSkinSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	SelectSkinOption(SelectedItem);
}

void UShowDownShopWidget::HandleEquipClicked()
{
	const FShowDownSkin* SelectedSkin = SkinsByOption.Find(SelectedOption);

	if (!SelectedSkin)
	{
		if (Text_Status)
		{
			Text_Status->SetText(FText::FromString(TEXT("No skin selected.")));
		}

		return;
	}

	USupabaseSubsystem* SupabaseSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<USupabaseSubsystem>()
		: nullptr;
	if (SupabaseSubsystem)
	{
		if (!SupabaseSubsystem->IsSkinOwned(SelectedSkin->Id))
		{
			if (Text_Status)
			{
				Text_Status->SetText(FText::FromString(TEXT("Locked skin cannot be equipped.")));
			}

			return;
		}

		// SelectedSkin->Id는 skin_sets.id입니다.
		// SupabaseSubsystem이 세트 안의 실제 skins를 찾아 slot별로 장착 저장합니다.
		SupabaseSubsystem->EquipSkin(SelectedSkin->Id);
	}
}

void UShowDownShopWidget::HandleBuyClicked()
{
	const FShowDownSkin* SelectedSkin = SkinsByOption.Find(SelectedOption);

	if (!SelectedSkin)
	{
		if (Text_Status)
		{
			Text_Status->SetText(FText::FromString(TEXT("No shop item selected.")));
		}

		return;
	}

	USupabaseSubsystem* SupabaseSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<USupabaseSubsystem>()
		: nullptr;
	if (SupabaseSubsystem)
	{
		if (SupabaseSubsystem->IsSkinOwned(SelectedSkin->Id))
		{
			if (Text_Status)
			{
				Text_Status->SetText(FText::FromString(TEXT("Shop item is already owned.")));
			}

			return;
		}

		SupabaseSubsystem->PurchaseSkinSet(SelectedSkin->Id);
	}
}

void UShowDownShopWidget::HandleRefreshClicked()
{
	USupabaseSubsystem* SupabaseSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<USupabaseSubsystem>()
		: nullptr;
	if (SupabaseSubsystem)
	{
		SupabaseSubsystem->LoadCosmeticData();
	}
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

void UShowDownShopWidget::HandleCosmeticDataLoaded(bool bSuccess, const FString& Message)
{
	if (Text_Status)
	{
		Text_Status->SetText(FText::FromString(Message));
	}

	if (bSuccess)
	{
		// skin_sets / player_skin_sets / skin_set_items / equipment 중 하나라도 갱신되면
		// 화면의 owned/equipped 상태가 바뀔 수 있으므로 목록을 다시 구성합니다.
		RefreshSkinOptions();
	}
}

void UShowDownShopWidget::HandleSkinEquipped(bool bSuccess, const FString& Message)
{
	if (Text_Status)
	{
		Text_Status->SetText(FText::FromString(Message));
	}

	if (bSuccess)
	{
		// 장착 요청은 세트 안의 slot 수만큼 여러 PATCH가 나갈 수 있습니다.
		// 각 응답이 올 때마다 최신 장착 상태를 화면에 반영합니다.
		RefreshSkinOptions();
	}
}

void UShowDownShopWidget::HandleSkinSetPurchased(bool bSuccess, const FString& Message)
{
	if (Text_Status)
	{
		Text_Status->SetText(FText::FromString(Message));
	}

	if (bSuccess)
	{
		RefreshSkinOptions();
	}
}
