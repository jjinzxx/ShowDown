#include "ShowDownUiBuildCommandlet.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/Slider.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ShowDownCharacterSkinCatalog.h"
#include "ShowDownLobbyWidget.h"
#include "ShowDownLoginWidget.h"
#include "ShowDownMainMenuWidget.h"
#include "ShowDownMultiplayerWidget.h"
#include "ShowDownMultiRankWidget.h"
#include "ShowDownRankWidget.h"
#include "ShowDownPauseMenuWidget.h"
#include "ShowDownSettingsWidget.h"
#include "ShowDownShopWidget.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "WidgetBlueprint.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogShowDownUiBuild, Log, All);

namespace
{
const FLinearColor Ink(0.92f, 0.95f, 0.96f, 1.0f);
const FLinearColor Panel(0.0f, 0.0f, 0.0f, 0.68f);
const FLinearColor Accent(0.0f, 0.0f, 0.0f, 0.82f);
const TCHAR* MainMenuWidgetObjectPath = TEXT("/Game/UI/WBP_MainMenu.WBP_MainMenu");
const TCHAR* ShopWidgetObjectPath = TEXT("/Game/UI/WBP_Shop.WBP_Shop");
const TCHAR* CharacterSkinCatalogObjectPath =
	TEXT("/Game/Data/Characters/DA_CharacterSkinCatalog.DA_CharacterSkinCatalog");
const TCHAR* CurrencyIconObjectPath =
	TEXT("/Game/UI/Icons/T_UI_Currency.T_UI_Currency");
const TCHAR* ScoreIconObjectPath =
	TEXT("/Game/UI/Icons/T_UI_Score.T_UI_Score");

UObject* PretendardRegular()
{
	return LoadObject<UObject>(nullptr, TEXT("/Game/UI/Font/Pretendard/static/Pretendard-Regular_Font.Pretendard-Regular_Font"));
}

UObject* ShowDownDisplayFont()
{
	if (UObject* Font = LoadObject<UObject>(nullptr, TEXT("/Game/UI/Font/olds/OLDSSCH__Font.OLDSSCH__Font")))
	{
		return Font;
	}
	return PretendardRegular();
}

FSlateBrush FlatColorBrush(const FLinearColor& Color)
{
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Box;
	Brush.TintColor = FSlateColor(Color);
	Brush.Margin = FMargin(0.0f);
	return Brush;
}

FSlateBrush FlatBlackBrush(float Alpha)
{
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Box;
	Brush.TintColor = FSlateColor(FLinearColor(0.0f, 0.0f, 0.0f, Alpha));
	Brush.Margin = FMargin(0.0f);
	return Brush;
}

void ApplyFlatEditableStyle(UEditableTextBox* Field)
{
	FEditableTextBoxStyle Style = Field->WidgetStyle;
	Style.SetBackgroundImageNormal(FlatBlackBrush(0.62f));
	Style.SetBackgroundImageHovered(FlatBlackBrush(0.68f));
	Style.SetBackgroundImageFocused(FlatBlackBrush(0.72f));
	Style.SetBackgroundImageReadOnly(FlatBlackBrush(0.52f));
	Style.SetForegroundColor(Ink);
	Style.TextStyle.SetFont(FSlateFontInfo(PretendardRegular(), 17));
	Field->WidgetStyle = Style;
}

UWidgetBlueprint* GetOrCreate(const TCHAR* Path, UClass* Parent)
{
	if (UWidgetBlueprint* Existing = LoadObject<UWidgetBlueprint>(nullptr, Path)) return Existing;
	const FString ObjectPath(Path);
	const FString PackageName = FPackageName::ObjectPathToPackageName(ObjectPath);
	const FString AssetName = FPackageName::ObjectPathToObjectName(ObjectPath);
	UPackage* Package = CreatePackage(*PackageName);
	return Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
		Parent, Package, FName(*AssetName), BPTYPE_Normal,
		UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));
}

UCanvasPanelSlot* AddCanvas(UCanvasPanel* Root, UWidget* Widget, float X, float Y, float W, float H)
{
	UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Widget);
	Slot->SetPosition(FVector2D(X, Y)); Slot->SetSize(FVector2D(W, H));
	return Slot;
}

UCanvasPanelSlot* AddAnchored(UCanvasPanel* Root, UWidget* Widget, float Left, float Top, float Right, float Bottom, const FMargin& Offsets = FMargin())
{
	UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Widget);
	Slot->SetAnchors(FAnchors(Left, Top, Right, Bottom));
	Slot->SetOffsets(Offsets);
	return Slot;
}

UTextBlock* Text(UWidgetTree* Tree, const TCHAR* Name, const TCHAR* Value, int32 Size, ETextJustify::Type Justify=ETextJustify::Left)
{
	UTextBlock* T = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	T->SetText(FText::FromString(Value)); T->SetColorAndOpacity(FSlateColor(Ink));
	T->SetFont(FSlateFontInfo(PretendardRegular(), Size)); T->SetJustification(Justify);
	return T;
}

UButton* Button(UWidgetTree* Tree, const TCHAR* Name, const TCHAR* Label, const FLinearColor& Color=Accent)
{
	UButton* B = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name); B->SetBackgroundColor(FLinearColor::White);
	const FLinearColor HoveredColor(
		FMath::Min(Color.R + 0.08f, 1.0f),
		FMath::Min(Color.G + 0.08f, 1.0f),
		FMath::Min(Color.B + 0.08f, 1.0f),
		FMath::Min(Color.A + 0.06f, 1.0f));
	const FLinearColor PressedColor(
		FMath::Max(Color.R - 0.03f, 0.0f),
		FMath::Max(Color.G - 0.03f, 0.0f),
		FMath::Max(Color.B - 0.03f, 0.0f),
		FMath::Min(Color.A + 0.12f, 1.0f));
	FButtonStyle Style;
	Style.SetNormal(FlatColorBrush(Color));
	Style.SetHovered(FlatColorBrush(HoveredColor));
	Style.SetPressed(FlatColorBrush(PressedColor));
	Style.SetDisabled(FlatColorBrush(FLinearColor(Color.R, Color.G, Color.B, Color.A * 0.5f)));
	B->SetStyle(Style);
	B->SetContent(Text(Tree, *FString::Printf(TEXT("%s_Label"), Name), Label, 17, ETextJustify::Center)); return B;
}

UButton* MainMenuButton(UWidgetTree* Tree, const TCHAR* Name, const TCHAR* Label)
{
	UButton* B = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
	FButtonStyle Style;
	Style.SetNormal(FlatColorBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f)));
	Style.SetHovered(FlatColorBrush(FLinearColor(0.42f, 0.42f, 0.42f, 0.82f)));
	Style.SetPressed(FlatColorBrush(FLinearColor(0.26f, 0.26f, 0.26f, 0.92f)));
	Style.SetDisabled(FlatColorBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.18f)));
	B->SetStyle(Style);
	UTextBlock* LabelText = Text(Tree, *FString::Printf(TEXT("%s_Label"), Name), Label, 28, ETextJustify::Center);
	LabelText->SetFont(FSlateFontInfo(ShowDownDisplayFont(), 28));
	B->SetContent(LabelText);
	return B;
}

UButton* BarButton(
	UWidgetTree* Tree,
	const TCHAR* Name,
	const TCHAR* Label,
	const FLinearColor& Color = Accent)
{
	UButton* B = Button(Tree, Name, Label, Color);
	if (UTextBlock* LabelText = Cast<UTextBlock>(B->GetContent()))
	{
		LabelText->SetJustification(ETextJustify::Left);
	}
	if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(B->GetContentSlot()))
	{
		ContentSlot->SetPadding(FMargin(14.0f, 0.0f));
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
		ContentSlot->SetVerticalAlignment(VAlign_Center);
	}
	return B;
}

UButton* ShopButton(
	UWidgetTree* Tree,
	const TCHAR* ButtonName,
	const TCHAR* TextName,
	const TCHAR* Label,
	const FLinearColor& NormalColor,
	const FLinearColor& HoveredColor,
	const FLinearColor& PressedColor,
	const FLinearColor& TextColor,
	const int32 FontSize)
{
	UButton* ShopButtonWidget = Tree->ConstructWidget<UButton>(
		UButton::StaticClass(),
		ButtonName);
	ShopButtonWidget->SetBackgroundColor(FLinearColor::White);

	FButtonStyle Style;
	Style.SetNormal(FlatColorBrush(NormalColor));
	Style.SetHovered(FlatColorBrush(HoveredColor));
	Style.SetPressed(FlatColorBrush(PressedColor));
	Style.SetDisabled(FlatColorBrush(FLinearColor(0.09f, 0.09f, 0.10f, 0.74f)));
	Style.SetNormalPadding(FMargin(2.0f));
	Style.SetPressedPadding(FMargin(2.0f, 4.0f, 2.0f, 0.0f));
	ShopButtonWidget->SetStyle(Style);

	UTextBlock* LabelText = Text(
		Tree,
		TextName,
		Label,
		FontSize,
		ETextJustify::Center);
	LabelText->SetColorAndOpacity(FSlateColor(TextColor));
	LabelText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.72f));
	LabelText->SetShadowOffset(FVector2D(1.0f, 2.0f));
	ShopButtonWidget->SetContent(LabelText);

	if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(ShopButtonWidget->GetContentSlot()))
	{
		ContentSlot->SetPadding(FMargin(18.0f, 6.0f));
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
		ContentSlot->SetVerticalAlignment(VAlign_Center);
	}

	return ShopButtonWidget;
}

UBorder* FrameShopButton(
	UWidgetTree* Tree,
	UButton* ButtonWidget,
	const TCHAR* BorderName,
	const FLinearColor& BorderColor,
	const float BorderThickness = 1.25f)
{
	UBorder* Frame = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), BorderName);
	Frame->SetBrushColor(BorderColor);
	Frame->SetPadding(FMargin(BorderThickness));
	Frame->SetContent(ButtonWidget);
	return Frame;
}

void Reset(UWidgetBlueprint* BP)
{
	BP->Modify();
	TArray<FName> PreviousNames;
	BP->WidgetVariableNameToGuidMap.GetKeys(PreviousNames);
	for (const FName& PreviousName : PreviousNames)
	{
		BP->OnVariableRemoved(PreviousName);
	}
	// A widget blueprint already owns a default subobject named WidgetTree. Reusing
	// that exact object name reconstructs the tree in place and leaves its previous
	// widget objects orphaned under the new tree. Give the replacement a temporary
	// unique name; the UMG compiler moves the abandoned tree aside and restores the
	// canonical WidgetTree name during compilation.
	BP->WidgetTree = NewObject<UWidgetTree>(BP, NAME_None, RF_Transactional | RF_ArchetypeObject);
	BP->WidgetVariableNameToGuidMap.Reset();
}

bool Save(UWidgetBlueprint* BP)
{
	// Match the compiler's source-widget traversal so every generated variable gets
	// a GUID, including widgets that are not reachable from RootWidget yet.
	BP->ForEachSourceWidget([BP](UWidget* Widget)
	{
		if (Widget)
		{
			Widget->bIsVariable = true;
			BP->WidgetVariableNameToGuidMap.Add(Widget->GetFName(), FGuid::NewGuid());
		}
	});
	FKismetEditorUtilities::CompileBlueprint(BP);
	BP->MarkPackageDirty();
	const FString Filename = FPackageName::LongPackageNameToFilename(
		BP->GetOutermost()->GetName(),
		FPackageName::GetAssetPackageExtension());
	const FString Directory = FPaths::GetPath(Filename);
	if ((!IFileManager::Get().MakeDirectory(*Directory, true)
		&& !IFileManager::Get().DirectoryExists(*Directory))
		|| !UPackage::SavePackage(
			BP->GetOutermost(),
			BP,
			*Filename,
			FSavePackageArgs()))
	{
		UE_LOG(
			LogShowDownUiBuild,
			Error,
			TEXT("Failed to save Widget Blueprint '%s'."),
			*BP->GetPathName());
		return false;
	}
	return true;
}

void SaveExisting(UWidgetBlueprint* BP)
{
	FKismetEditorUtilities::CompileBlueprint(BP);
	BP->MarkPackageDirty();
	UPackage::SavePackage(
		BP->GetOutermost(),
		BP,
		*FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()),
		FSavePackageArgs());
}

void RegisterPatchedWidget(UWidgetBlueprint* Blueprint, UWidget* Widget)
{
	Widget->bIsVariable = true;
	Blueprint->WidgetVariableNameToGuidMap.Add(Widget->GetFName(), FGuid::NewGuid());
}

bool WrapCanvasMetricTextWithIcon(
	UWidgetBlueprint* Blueprint,
	const FName TextWidgetName,
	const FName RowWidgetName,
	const FName SpacerWidgetName,
	const FName ImageWidgetName,
	const TCHAR* TextureObjectPath,
	const TCHAR* InitialValue,
	const float IconSize)
{
	if (!Blueprint || !Blueprint->WidgetTree)
	{
		UE_LOG(LogShowDownUiBuild, Error, TEXT("Cannot patch a null Widget Blueprint or WidgetTree."));
		return false;
	}

	UWidgetTree* Tree = Blueprint->WidgetTree;
	UTextBlock* ValueText = Cast<UTextBlock>(Tree->FindWidget(TextWidgetName));
	if (!ValueText)
	{
		UE_LOG(
			LogShowDownUiBuild,
			Error,
			TEXT("%s is missing required metric text '%s'."),
			*Blueprint->GetPathName(),
			*TextWidgetName.ToString());
		return false;
	}

	UTexture2D* IconTexture = LoadObject<UTexture2D>(nullptr, TextureObjectPath);
	if (!IconTexture)
	{
		UE_LOG(LogShowDownUiBuild, Error, TEXT("Missing metric icon texture: %s"), TextureObjectPath);
		return false;
	}

	if (UWidget* ExistingImageWidget = Tree->FindWidget(ImageWidgetName))
	{
		UImage* ExistingImage = Cast<UImage>(ExistingImageWidget);
		if (!ExistingImage)
		{
			UE_LOG(
				LogShowDownUiBuild,
				Error,
				TEXT("%s already contains non-image widget '%s'."),
				*Blueprint->GetPathName(),
				*ImageWidgetName.ToString());
			return false;
		}

		ExistingImage->SetBrushFromTexture(IconTexture, false);
		ExistingImage->SetDesiredSizeOverride(FVector2D(IconSize, IconSize));
		ValueText->SetText(FText::FromString(InitialValue));
		ValueText->SetJustification(ETextJustify::Left);
		return true;
	}

	if (Tree->FindWidget(RowWidgetName) || Tree->FindWidget(SpacerWidgetName))
	{
		UE_LOG(
			LogShowDownUiBuild,
			Error,
			TEXT("%s contains a metric wrapper name collision for '%s'."),
			*Blueprint->GetPathName(),
			*RowWidgetName.ToString());
		return false;
	}

	UCanvasPanel* ParentCanvas = Cast<UCanvasPanel>(ValueText->GetParent());
	UCanvasPanelSlot* OriginalSlot = Cast<UCanvasPanelSlot>(ValueText->Slot);
	if (!ParentCanvas || !OriginalSlot)
	{
		UE_LOG(
			LogShowDownUiBuild,
			Error,
			TEXT("%s metric '%s' is not a direct CanvasPanel child; no changes were saved."),
			*Blueprint->GetPathName(),
			*TextWidgetName.ToString());
		return false;
	}

	const FAnchors Anchors = OriginalSlot->GetAnchors();
	const FMargin Offsets = OriginalSlot->GetOffsets();
	const FVector2D Alignment = OriginalSlot->GetAlignment();
	const bool bAutoSize = OriginalSlot->GetAutoSize();
	const int32 ZOrder = OriginalSlot->GetZOrder();
	const int32 ChildIndex = ParentCanvas->GetChildIndex(ValueText);

	Blueprint->Modify();
	Tree->Modify();
	ParentCanvas->Modify();
	ValueText->Modify();

	if (!ParentCanvas->RemoveChild(ValueText))
	{
		UE_LOG(
			LogShowDownUiBuild,
			Error,
			TEXT("Failed to detach metric '%s' from %s."),
			*TextWidgetName.ToString(),
			*Blueprint->GetPathName());
		return false;
	}

	UHorizontalBox* MetricRow = Tree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		RowWidgetName);
	RegisterPatchedWidget(Blueprint, MetricRow);
	UCanvasPanelSlot* RowSlot = Cast<UCanvasPanelSlot>(
		ParentCanvas->InsertChildAt(ChildIndex, MetricRow));
	if (!RowSlot)
	{
		UE_LOG(LogShowDownUiBuild, Error, TEXT("Failed to preserve the metric child position in %s."), *Blueprint->GetPathName());
		return false;
	}
	RowSlot->SetAnchors(Anchors);
	RowSlot->SetOffsets(Offsets);
	RowSlot->SetAlignment(Alignment);
	RowSlot->SetAutoSize(bAutoSize);
	RowSlot->SetZOrder(ZOrder);

	USpacer* LeadingSpacer = Tree->ConstructWidget<USpacer>(
		USpacer::StaticClass(),
		SpacerWidgetName);
	RegisterPatchedWidget(Blueprint, LeadingSpacer);
	if (UHorizontalBoxSlot* SpacerSlot = MetricRow->AddChildToHorizontalBox(LeadingSpacer))
	{
		SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UImage* MetricIcon = Tree->ConstructWidget<UImage>(
		UImage::StaticClass(),
		ImageWidgetName);
	RegisterPatchedWidget(Blueprint, MetricIcon);
	MetricIcon->SetBrushFromTexture(IconTexture, false);
	MetricIcon->SetDesiredSizeOverride(FVector2D(IconSize, IconSize));
	if (UHorizontalBoxSlot* IconSlot = MetricRow->AddChildToHorizontalBox(MetricIcon))
	{
		IconSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		IconSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
		IconSlot->SetVerticalAlignment(VAlign_Center);
	}

	ValueText->SetText(FText::FromString(InitialValue));
	ValueText->SetJustification(ETextJustify::Left);
	if (UHorizontalBoxSlot* ValueSlot = MetricRow->AddChildToHorizontalBox(ValueText))
	{
		ValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		ValueSlot->SetVerticalAlignment(VAlign_Center);
	}

	UE_LOG(
		LogShowDownUiBuild,
		Display,
		TEXT("Patched only metric '%s' in %s; the rest of the widget tree was preserved."),
		*TextWidgetName.ToString(),
		*Blueprint->GetPathName());
	return true;
}

bool CompileMetricIconPatch(UWidgetBlueprint* Blueprint)
{
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	if (Blueprint->Status == BS_Error)
	{
		UE_LOG(LogShowDownUiBuild, Error, TEXT("Metric icon patch failed to compile for %s."), *Blueprint->GetPathName());
		return false;
	}
	return true;
}

bool SaveMetricIconPatch(UWidgetBlueprint* Blueprint)
{
	Blueprint->MarkPackageDirty();
	const FString Filename = FPackageName::LongPackageNameToFilename(
		Blueprint->GetOutermost()->GetName(),
		FPackageName::GetAssetPackageExtension());
	const bool bSaved = UPackage::SavePackage(
		Blueprint->GetOutermost(),
		Blueprint,
		*Filename,
		FSavePackageArgs());
	if (!bSaved)
	{
		UE_LOG(LogShowDownUiBuild, Error, TEXT("Failed to save metric icon patch for %s."), *Blueprint->GetPathName());
	}
	return bSaved;
}

bool PatchPlayerMetricIcons()
{
	UWidgetBlueprint* MainMenu = LoadObject<UWidgetBlueprint>(nullptr, MainMenuWidgetObjectPath);
	UWidgetBlueprint* Shop = LoadObject<UWidgetBlueprint>(nullptr, ShopWidgetObjectPath);
	if (!MainMenu || !Shop)
	{
		UE_LOG(LogShowDownUiBuild, Error, TEXT("Main menu or shop Widget Blueprint could not be loaded."));
		return false;
	}

	const bool bMainCoinPatched = WrapCanvasMetricTextWithIcon(
		MainMenu,
		TEXT("Text_Coin"),
		TEXT("CoinMetricRow"),
		TEXT("CoinMetricSpacer"),
		TEXT("Image_Coin"),
		CurrencyIconObjectPath,
		TEXT("0$"),
		30.0f);
	const bool bMainScorePatched = WrapCanvasMetricTextWithIcon(
		MainMenu,
		TEXT("Text_Score"),
		TEXT("ScoreMetricRow"),
		TEXT("ScoreMetricSpacer"),
		TEXT("Image_Score"),
		ScoreIconObjectPath,
		TEXT("0P"),
		30.0f);
	const bool bShopCoinPatched = WrapCanvasMetricTextWithIcon(
		Shop,
		TEXT("Text_Coin"),
		TEXT("ShopCoinMetricRow"),
		TEXT("ShopCoinMetricSpacer"),
		TEXT("Image_Coin"),
		CurrencyIconObjectPath,
		TEXT("0$"),
		30.0f);

	if (!bMainCoinPatched || !bMainScorePatched || !bShopCoinPatched)
	{
		UE_LOG(LogShowDownUiBuild, Error, TEXT("Metric icon patch aborted before saving all target assets."));
		return false;
	}
	if (!CompileMetricIconPatch(MainMenu) || !CompileMetricIconPatch(Shop))
	{
		UE_LOG(LogShowDownUiBuild, Error, TEXT("Metric icon patch aborted because a target failed to compile."));
		return false;
	}

	return SaveMetricIconPatch(MainMenu) && SaveMetricIconPatch(Shop);
}

bool ValidateMetricIconWidget(
	UWidgetBlueprint* Blueprint,
	const FName TextWidgetName,
	const FName RowWidgetName,
	const FName ImageWidgetName)
{
	if (!Blueprint || !Blueprint->WidgetTree)
	{
		return false;
	}

	UTextBlock* ValueText = Cast<UTextBlock>(Blueprint->WidgetTree->FindWidget(TextWidgetName));
	UHorizontalBox* MetricRow = Cast<UHorizontalBox>(Blueprint->WidgetTree->FindWidget(RowWidgetName));
	UImage* MetricIcon = Cast<UImage>(Blueprint->WidgetTree->FindWidget(ImageWidgetName));
	const bool bValid = ValueText
		&& MetricRow
		&& MetricIcon
		&& ValueText->GetParent() == MetricRow
		&& MetricIcon->GetParent() == MetricRow
		&& Cast<UCanvasPanel>(MetricRow->GetParent())
		&& Cast<UCanvasPanelSlot>(MetricRow->Slot);
	if (!bValid)
	{
		UE_LOG(
			LogShowDownUiBuild,
			Error,
			TEXT("Metric icon hierarchy validation failed for '%s' in %s."),
			*TextWidgetName.ToString(),
			*Blueprint->GetPathName());
	}
	return bValid;
}

bool ValidatePlayerMetricIcons()
{
	UWidgetBlueprint* MainMenu = LoadObject<UWidgetBlueprint>(nullptr, MainMenuWidgetObjectPath);
	UWidgetBlueprint* Shop = LoadObject<UWidgetBlueprint>(nullptr, ShopWidgetObjectPath);
	const bool bHierarchyValid =
		ValidateMetricIconWidget(MainMenu, TEXT("Text_Coin"), TEXT("CoinMetricRow"), TEXT("Image_Coin"))
		&& ValidateMetricIconWidget(MainMenu, TEXT("Text_Score"), TEXT("ScoreMetricRow"), TEXT("Image_Score"))
		&& ValidateMetricIconWidget(Shop, TEXT("Text_Coin"), TEXT("ShopCoinMetricRow"), TEXT("Image_Coin"));
	if (!bHierarchyValid)
	{
		return false;
	}

	FKismetEditorUtilities::CompileBlueprint(MainMenu);
	FKismetEditorUtilities::CompileBlueprint(Shop);
	if (MainMenu->Status == BS_Error || Shop->Status == BS_Error)
	{
		UE_LOG(LogShowDownUiBuild, Error, TEXT("A metric icon Widget Blueprint failed fresh-load compilation."));
		return false;
	}

	UE_LOG(
		LogShowDownUiBuild,
		Display,
		TEXT("Metric icon Widget Blueprints loaded and compiled without rebuilding or saving their widget trees."));
	return true;
}

bool BuildShop(UWidgetBlueprint* BP)
{
	Reset(BP);
	UWidgetTree* Tree = BP->WidgetTree;
	UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("RootCanvas"));
	Tree->RootWidget = Root;

	// Keep the level art visible behind the editor-placed preview actor. The
	// approved composition keeps only the existing header shade; bottom controls
	// float over the scene without full-width or side overlays.
	UBorder* TopShade = Tree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("Border_TopShade"));
	TopShade->SetBrushColor(FLinearColor(0.025f, 0.008f, 0.014f, 0.86f));
	AddAnchored(Root, TopShade, 0.0f, 0.0f, 1.0f, 0.0f, FMargin(0.0f, 0.0f, 0.0f, 214.0f));

	UBorder* HeaderLine = Tree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("Border_HeaderLine"));
	HeaderLine->SetBrushColor(FLinearColor(0.76f, 0.52f, 0.17f, 0.72f));
	AddAnchored(Root, HeaderLine, 0.0f, 0.0f, 1.0f, 0.0f, FMargin(0.0f, 210.0f, 0.0f, 2.0f));

	UButton* BackButton = ShopButton(
		Tree,
		TEXT("Button_Back"),
		TEXT("Text_BackLabel"),
		TEXT("<  뒤로"),
		FLinearColor(0.018f, 0.018f, 0.016f, 0.76f),
		FLinearColor(0.16f, 0.12f, 0.055f, 0.92f),
		FLinearColor(0.09f, 0.065f, 0.025f, 0.98f),
		FLinearColor(0.96f, 0.84f, 0.60f, 1.0f),
		18);
	UBorder* BackButtonFrame = FrameShopButton(
		Tree,
		BackButton,
		TEXT("Border_BackButtonFrame"),
		FLinearColor(0.72f, 0.52f, 0.20f, 0.76f));
	AddAnchored(Root, BackButtonFrame, 0.0f, 0.0f, 0.0f, 0.0f, FMargin(50.0f, 42.0f, 210.0f, 56.0f));

	UTextBlock* CoinText = Text(
		Tree,
		TEXT("Text_Coin"),
		TEXT("보유 코인  0"),
		20,
		ETextJustify::Right);
	CoinText->SetColorAndOpacity(FSlateColor(FLinearColor(0.96f, 0.78f, 0.35f, 1.0f)));
	CoinText->SetShadowColorAndOpacity(FLinearColor::Black);
	CoinText->SetShadowOffset(FVector2D(1.0f, 2.0f));
	AddAnchored(Root, CoinText, 1.0f, 0.0f, 1.0f, 0.0f, FMargin(-370.0f, 50.0f, 320.0f, 38.0f));

	UTextBlock* SkinName = Text(
		Tree,
		TEXT("Text_SkinName"),
		TEXT("ROBOT"),
		48,
		ETextJustify::Center);
	SkinName->SetFont(FSlateFontInfo(ShowDownDisplayFont(), 48));
	SkinName->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.95f));
	SkinName->SetShadowOffset(FVector2D(2.0f, 3.0f));
	AddAnchored(Root, SkinName, 0.5f, 0.0f, 0.5f, 0.0f, FMargin(-360.0f, 38.0f, 720.0f, 65.0f));

	UTextBlock* RarityText = Text(
		Tree,
		TEXT("Text_Rarity"),
		TEXT("COMMON"),
		18,
		ETextJustify::Center);
	RarityText->SetColorAndOpacity(FSlateColor(FLinearColor(0.82f, 0.84f, 0.86f, 1.0f)));
	AddAnchored(Root, RarityText, 0.5f, 0.0f, 0.5f, 0.0f, FMargin(-220.0f, 106.0f, 440.0f, 30.0f));

	UBorder* RarityAccent = Tree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("Border_RarityAccent"));
	RarityAccent->SetBrushColor(FLinearColor(0.82f, 0.84f, 0.86f, 1.0f));
	AddAnchored(Root, RarityAccent, 0.5f, 0.0f, 0.5f, 0.0f, FMargin(-235.0f, 139.0f, 470.0f, 4.0f));

	UTextBlock* DescriptionText = Text(
		Tree,
		TEXT("Text_Description"),
		TEXT("스킨 정보를 불러오는 중입니다."),
		16,
		ETextJustify::Center);
	DescriptionText->SetColorAndOpacity(FSlateColor(FLinearColor(0.76f, 0.76f, 0.78f, 1.0f)));
	DescriptionText->SetAutoWrapText(true);
	AddAnchored(Root, DescriptionText, 0.5f, 0.0f, 0.5f, 0.0f, FMargin(-400.0f, 154.0f, 800.0f, 46.0f));

	UTextBlock* PriceText = Text(
		Tree,
		TEXT("Text_Price"),
		TEXT("가격 불러오는 중"),
		24,
		ETextJustify::Center);
	PriceText->SetColorAndOpacity(FSlateColor(FLinearColor(0.97f, 0.81f, 0.43f, 1.0f)));
	PriceText->SetShadowColorAndOpacity(FLinearColor::Black);
	PriceText->SetShadowOffset(FVector2D(1.0f, 2.0f));
	AddAnchored(Root, PriceText, 0.5f, 1.0f, 0.5f, 1.0f, FMargin(-280.0f, -174.0f, 560.0f, 40.0f));

	UButton* PreviousButton = ShopButton(
		Tree,
		TEXT("Button_Previous"),
		TEXT("Text_PreviousLabel"),
		TEXT("<  이전"),
		FLinearColor(0.012f, 0.012f, 0.011f, 0.46f),
		FLinearColor(0.15f, 0.11f, 0.045f, 0.88f),
		FLinearColor(0.075f, 0.055f, 0.022f, 0.94f),
		FLinearColor(0.96f, 0.84f, 0.60f, 1.0f),
		20);
	UBorder* PreviousButtonFrame = FrameShopButton(
		Tree,
		PreviousButton,
		TEXT("Border_PreviousButtonFrame"),
		FLinearColor(0.72f, 0.52f, 0.20f, 0.66f));
	AddAnchored(Root, PreviousButtonFrame, 0.0f, 1.0f, 0.0f, 1.0f, FMargin(66.0f, -112.0f, 260.0f, 58.0f));

	UButton* PrimaryActionButton = ShopButton(
		Tree,
		TEXT("Button_PrimaryAction"),
		TEXT("Text_PrimaryAction"),
		TEXT("불러오는 중"),
		FLinearColor(0.018f, 0.018f, 0.016f, 0.84f),
		FLinearColor(0.20f, 0.15f, 0.065f, 0.96f),
		FLinearColor(0.10f, 0.075f, 0.028f, 1.0f),
		FLinearColor(0.96f, 0.84f, 0.60f, 1.0f),
		24);
	UBorder* PrimaryActionButtonFrame = FrameShopButton(
		Tree,
		PrimaryActionButton,
		TEXT("Border_PrimaryActionButtonFrame"),
		FLinearColor(0.76f, 0.56f, 0.22f, 0.82f),
		1.5f);
	AddAnchored(Root, PrimaryActionButtonFrame, 0.5f, 1.0f, 0.5f, 1.0f, FMargin(-190.0f, -120.0f, 380.0f, 66.0f));

	UButton* NextButton = ShopButton(
		Tree,
		TEXT("Button_Next"),
		TEXT("Text_NextLabel"),
		TEXT("다음  >"),
		FLinearColor(0.012f, 0.012f, 0.011f, 0.46f),
		FLinearColor(0.15f, 0.11f, 0.045f, 0.88f),
		FLinearColor(0.075f, 0.055f, 0.022f, 0.94f),
		FLinearColor(0.96f, 0.84f, 0.60f, 1.0f),
		20);
	UBorder* NextButtonFrame = FrameShopButton(
		Tree,
		NextButton,
		TEXT("Border_NextButtonFrame"),
		FLinearColor(0.72f, 0.52f, 0.20f, 0.66f));
	AddAnchored(Root, NextButtonFrame, 1.0f, 1.0f, 1.0f, 1.0f, FMargin(-326.0f, -112.0f, 260.0f, 58.0f));

	UTextBlock* StatusText = Text(
		Tree,
		TEXT("Text_Status"),
		TEXT(""),
		14,
		ETextJustify::Center);
	StatusText->SetAutoWrapText(true);
	AddAnchored(Root, StatusText, 0.5f, 1.0f, 0.5f, 1.0f, FMargin(-420.0f, -44.0f, 840.0f, 30.0f));

	return Save(BP);
}

bool ValidateShopWidgetBlueprint(UWidgetBlueprint* BP)
{
	if (!BP || !BP->WidgetTree || !BP->WidgetTree->RootWidget)
	{
		UE_LOG(LogShowDownUiBuild, Error, TEXT("WBP_Shop has no valid widget tree."));
		return false;
	}

	bool bIsValid = true;
	if (!BP->ParentClass || !BP->ParentClass->IsChildOf(UShowDownShopWidget::StaticClass()))
	{
		UE_LOG(
			LogShowDownUiBuild,
			Error,
			TEXT("WBP_Shop must derive from UShowDownShopWidget; existing asset was not modified."));
		bIsValid = false;
	}

	struct FRequiredWidget
	{
		const TCHAR* Name;
		UClass* Type;
	};

	const FRequiredWidget RequiredWidgets[] =
	{
		{TEXT("Border_RarityAccent"), UBorder::StaticClass()},
		{TEXT("Text_SkinName"), UTextBlock::StaticClass()},
		{TEXT("Text_Rarity"), UTextBlock::StaticClass()},
		{TEXT("Text_Description"), UTextBlock::StaticClass()},
		{TEXT("Text_Price"), UTextBlock::StaticClass()},
		{TEXT("Text_Coin"), UTextBlock::StaticClass()},
		{TEXT("Text_Status"), UTextBlock::StaticClass()},
		{TEXT("Text_PrimaryAction"), UTextBlock::StaticClass()},
		{TEXT("Button_Previous"), UButton::StaticClass()},
		{TEXT("Button_PrimaryAction"), UButton::StaticClass()},
		{TEXT("Button_Next"), UButton::StaticClass()},
		{TEXT("Button_Back"), UButton::StaticClass()}
	};

	for (const FRequiredWidget& RequiredWidget : RequiredWidgets)
	{
		UWidget* Widget = BP->WidgetTree->FindWidget(RequiredWidget.Name);
		if (!Widget || !Widget->IsA(RequiredWidget.Type))
		{
			UE_LOG(
				LogShowDownUiBuild,
				Error,
				TEXT("WBP_Shop binding '%s' is missing or has the wrong type (expected %s). Existing asset was not modified."),
				RequiredWidget.Name,
				*RequiredWidget.Type->GetName());
			bIsValid = false;
		}
	}

	if (BP->Status == BS_Error || !BP->GeneratedClass)
	{
		UE_LOG(LogShowDownUiBuild, Error, TEXT("WBP_Shop failed Blueprint compilation."));
		bIsValid = false;
	}

	return bIsValid;
}

bool BuildOrValidateShopWidget(const bool bForceRebuild)
{
	const FString ObjectPath(ShopWidgetObjectPath);
	const FString PackageName = FPackageName::ObjectPathToPackageName(ObjectPath);
	UObject* ExistingObject = LoadObject<UObject>(nullptr, ShopWidgetObjectPath);
	UWidgetBlueprint* ShopBlueprint = Cast<UWidgetBlueprint>(ExistingObject);
	const bool bCreated = ExistingObject == nullptr;
	if (!ExistingObject && FPackageName::DoesPackageExist(PackageName))
	{
		UE_LOG(
			LogShowDownUiBuild,
			Error,
			TEXT("Package '%s' exists but %s could not be loaded. The package will not be recreated or overwritten; repair or remove it explicitly in the editor."),
			*PackageName,
			ShopWidgetObjectPath);
		return false;
	}

	if (ExistingObject && !ShopBlueprint)
	{
		UE_LOG(
			LogShowDownUiBuild,
			Error,
			TEXT("%s exists but is not a Widget Blueprint. It will not be overwritten."),
			ShopWidgetObjectPath);
		return false;
	}

	if (!ShopBlueprint)
	{
		const FString AssetName = FPackageName::ObjectPathToObjectName(ObjectPath);
		UPackage* Package = CreatePackage(*PackageName);
		ShopBlueprint = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
			UShowDownShopWidget::StaticClass(),
			Package,
			FName(*AssetName),
			BPTYPE_Normal,
			UWidgetBlueprint::StaticClass(),
			UWidgetBlueprintGeneratedClass::StaticClass()));

		if (!ShopBlueprint)
		{
			UE_LOG(LogShowDownUiBuild, Error, TEXT("Failed to create %s."), ShopWidgetObjectPath);
			return false;
		}
		FAssetRegistryModule::AssetCreated(ShopBlueprint);
	}

	if (!ShopBlueprint->ParentClass
		|| !ShopBlueprint->ParentClass->IsChildOf(UShowDownShopWidget::StaticClass()))
	{
		UE_LOG(
			LogShowDownUiBuild,
			Error,
			TEXT("%s has an incompatible parent class and will not be modified."),
			ShopWidgetObjectPath);
		return false;
	}

	if (bCreated || bForceRebuild)
	{
		if (bForceRebuild && !bCreated)
		{
			UE_LOG(
				LogShowDownUiBuild,
				Warning,
				TEXT("-ForceRebuildShopAssets explicitly authorized rebuilding the existing WBP_Shop."));
		}
		if (!BuildShop(ShopBlueprint))
		{
			return false;
		}
	}
	else
	{
		// Compiling is intentionally the only operation performed on an existing
		// asset. In particular, do not call Reset, Modify, MarkPackageDirty, or
		// SavePackage here: designers may already have replaced the generated UI.
		FKismetEditorUtilities::CompileBlueprint(ShopBlueprint);
		UE_LOG(
			LogShowDownUiBuild,
			Display,
			TEXT("Existing WBP_Shop compiled and validated without being reset or saved."));
	}

	return ValidateShopWidgetBlueprint(ShopBlueprint);
}

bool ValidateCharacterSkinCatalog(const UShowDownCharacterSkinCatalog* Catalog)
{
	if (!Catalog || Catalog->Skins.IsEmpty())
	{
		UE_LOG(LogShowDownUiBuild, Error, TEXT("DA_CharacterSkinCatalog contains no skins."));
		return false;
	}

	bool bIsValid = true;
	TSet<FString> SeenSkinIds;
	for (const FShowDownCharacterSkinDefinition& Definition : Catalog->Skins)
	{
		const FString CanonicalSkinId =
			UShowDownCharacterSkinCatalog::CanonicalizeSkinId(Definition.SkinId);
		if (CanonicalSkinId.IsEmpty())
		{
			UE_LOG(LogShowDownUiBuild, Error, TEXT("DA_CharacterSkinCatalog has an entry with an empty SkinId."));
			bIsValid = false;
			continue;
		}
		if (SeenSkinIds.Contains(CanonicalSkinId))
		{
			UE_LOG(
				LogShowDownUiBuild,
				Error,
				TEXT("DA_CharacterSkinCatalog has duplicate canonical SkinId '%s'."),
				*CanonicalSkinId);
			bIsValid = false;
			continue;
		}
		SeenSkinIds.Add(CanonicalSkinId);

		if (Definition.SkeletalMesh.IsNull())
		{
			UE_LOG(
				LogShowDownUiBuild,
				Warning,
				TEXT("Skin '%s' has no SkeletalMesh and will use the runtime fallback."),
				*CanonicalSkinId);
		}

		auto ValidatePreviewProfile = [&CanonicalSkinId](
			const TCHAR* ProfileName,
			const FShowDownCharacterPreviewAnimationProfile& Profile)
		{
			if (Profile.AnimationMode == EShowDownShopPreviewAnimationMode::SingleAnimation
				&& Profile.Animation.IsNull())
			{
				UE_LOG(
					LogShowDownUiBuild,
					Warning,
					TEXT("Skin '%s' %s profile selects SingleAnimation without an asset; reference pose fallback will be used."),
					*CanonicalSkinId,
					ProfileName);
			}
			else if (Profile.AnimationMode == EShowDownShopPreviewAnimationMode::AnimationBlueprint
				&& Profile.AnimClass.IsNull())
			{
				UE_LOG(
					LogShowDownUiBuild,
					Warning,
					TEXT("Skin '%s' %s profile selects AnimationBlueprint without a class; reference pose fallback will be used."),
					*CanonicalSkinId,
					ProfileName);
			}
		};

		ValidatePreviewProfile(TEXT("ShopPreview"), Definition.ShopPreview);
		ValidatePreviewProfile(TEXT("MainMenuPreview"), Definition.MainMenuPreview);
	}

	return bIsValid;
}

bool SaveCharacterSkinCatalog(UShowDownCharacterSkinCatalog* Catalog)
{
	const FString PackageName = Catalog->GetOutermost()->GetName();
	const FString Filename = FPackageName::LongPackageNameToFilename(
		PackageName,
		FPackageName::GetAssetPackageExtension());
	const FString Directory = FPaths::GetPath(Filename);
	if (!IFileManager::Get().MakeDirectory(*Directory, true)
		&& !IFileManager::Get().DirectoryExists(*Directory))
	{
		UE_LOG(
			LogShowDownUiBuild,
			Error,
			TEXT("Failed to create data asset directory '%s'."),
			*Directory);
		return false;
	}

	Catalog->MarkPackageDirty();
	const bool bSaved = UPackage::SavePackage(
		Catalog->GetOutermost(),
		Catalog,
		*Filename,
		FSavePackageArgs());
	if (!bSaved)
	{
		UE_LOG(
			LogShowDownUiBuild,
			Error,
			TEXT("Failed to save %s."),
			CharacterSkinCatalogObjectPath);
	}
	return bSaved;
}

bool BuildOrValidateCharacterSkinCatalog(const bool bForceRebuild)
{
	const FString ObjectPath(CharacterSkinCatalogObjectPath);
	const FString PackageName = FPackageName::ObjectPathToPackageName(ObjectPath);
	UObject* ExistingObject = LoadObject<UObject>(nullptr, CharacterSkinCatalogObjectPath);
	UShowDownCharacterSkinCatalog* Catalog =
		Cast<UShowDownCharacterSkinCatalog>(ExistingObject);
	const bool bCreated = ExistingObject == nullptr;
	if (!ExistingObject && FPackageName::DoesPackageExist(PackageName))
	{
		UE_LOG(
			LogShowDownUiBuild,
			Error,
			TEXT("Package '%s' exists but %s could not be loaded. The package will not be recreated or overwritten; repair or remove it explicitly in the editor."),
			*PackageName,
			CharacterSkinCatalogObjectPath);
		return false;
	}

	if (ExistingObject && !Catalog)
	{
		UE_LOG(
			LogShowDownUiBuild,
			Error,
			TEXT("%s exists but is not a UShowDownCharacterSkinCatalog. It will not be overwritten."),
			CharacterSkinCatalogObjectPath);
		return false;
	}

	if (!Catalog)
	{
		const FString AssetName = FPackageName::ObjectPathToObjectName(ObjectPath);
		UPackage* Package = CreatePackage(*PackageName);
		Catalog = NewObject<UShowDownCharacterSkinCatalog>(
			Package,
			FName(*AssetName),
			RF_Public | RF_Standalone | RF_Transactional);
		if (!Catalog)
		{
			UE_LOG(
				LogShowDownUiBuild,
				Error,
				TEXT("Failed to create %s."),
				CharacterSkinCatalogObjectPath);
			return false;
		}
		FAssetRegistryModule::AssetCreated(Catalog);
	}

	if (bCreated || bForceRebuild)
	{
		if (bForceRebuild && !bCreated)
		{
			UE_LOG(
				LogShowDownUiBuild,
				Warning,
				TEXT("-ForceRebuildShopAssets explicitly authorized replacing the existing catalog entries."));
		}

		Catalog->Modify();
		UShowDownCharacterSkinCatalog::GetOrderedSkinDefinitions(nullptr, Catalog->Skins);
		if (!SaveCharacterSkinCatalog(Catalog))
		{
			return false;
		}
	}
	else
	{
		UE_LOG(
			LogShowDownUiBuild,
			Display,
			TEXT("Existing DA_CharacterSkinCatalog validated without being modified or saved."));
	}

	return ValidateCharacterSkinCatalog(Catalog);
}

bool BuildShopAssets(const bool bForceRebuild)
{
	// Run both operations even if one fails so a single commandlet invocation
	// reports every actionable validation error.
	const bool bCatalogValid = BuildOrValidateCharacterSkinCatalog(bForceRebuild);
	const bool bWidgetValid = BuildOrValidateShopWidget(bForceRebuild);
	return bCatalogValid && bWidgetValid;
}

void PatchSettings(UWidgetBlueprint* BP)
{
	BP->Modify();
	UWidgetTree* T = BP->WidgetTree;
	if (!T)
	{
		return;
	}

	UButton* ApplyButton = Cast<UButton>(T->FindWidget(TEXT("Button_Apply")));
	UButton* QualityButton = Cast<UButton>(T->FindWidget(TEXT("Button_Quality")));
	UButton* PostProcessButton = Cast<UButton>(T->FindWidget(TEXT("Button_PostProcess")));
	UButton* EffectsButton = Cast<UButton>(T->FindWidget(TEXT("Button_Effects")));
	UTextBlock* QualityTitle = Cast<UTextBlock>(T->FindWidget(TEXT("Button_PostProcess_LabelTitle")));
	UWidget* EffectsTitle = T->FindWidget(TEXT("Button_Effects_LabelTitle"));
	UCanvasPanel* GraphicsPanel = Cast<UCanvasPanel>(T->FindWidget(TEXT("Panel_Graphics")));

	FVector2D QualityPosition(70.0f, 513.0f);
	FVector2D QualitySize(420.0f, 46.0f);
	if (UCanvasPanelSlot* PostProcessSlot = PostProcessButton ? Cast<UCanvasPanelSlot>(PostProcessButton->Slot) : nullptr)
	{
		QualityPosition = PostProcessSlot->GetPosition();
		QualitySize = PostProcessSlot->GetSize();
	}

	if (PostProcessButton)
	{
		PostProcessButton->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (EffectsButton)
	{
		EffectsButton->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (EffectsTitle)
	{
		EffectsTitle->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (QualityTitle)
	{
		QualityTitle->SetText(FText::FromString(TEXT("그래픽 품질")));
	}
	if (QualityButton && GraphicsPanel)
	{
		QualityButton->RemoveFromParent();
		QualityButton->SetVisibility(ESlateVisibility::Visible);
		if (ApplyButton)
		{
			QualityButton->SetStyle(ApplyButton->GetStyle());
		}
		if (UTextBlock* Label = Cast<UTextBlock>(QualityButton->GetContent()))
		{
			Label->SetText(FText::FromString(TEXT("최상")));
		}
		UCanvasPanelSlot* QualitySlot = GraphicsPanel->AddChildToCanvas(QualityButton);
		QualitySlot->SetPosition(QualityPosition);
		QualitySlot->SetSize(QualitySize);
	}

	if (ApplyButton)
	{
		for (const TCHAR* ButtonName : {TEXT("Button_WindowMode"), TEXT("Button_Resolution"), TEXT("Button_Quality"), TEXT("Button_ChangeNickname")})
		{
			if (UButton* InteractiveButton = Cast<UButton>(T->FindWidget(ButtonName)))
			{
				InteractiveButton->SetStyle(ApplyButton->GetStyle());
			}
		}
	}

	SaveExisting(BP);
}

void BuildLogin(UWidgetBlueprint* BP)
{
	Reset(BP); UWidgetTree* T=BP->WidgetTree; UCanvasPanel* R=T->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("RootCanvas")); T->RootWidget=R;
	UBorder* P=T->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("LoginPanel")); P->SetBrushColor(Panel); AddCanvas(R,P,120,300,500,430);
	AddCanvas(R,Text(T,TEXT("Text_Brand"),TEXT("SHOWDOWN"),38,ETextJustify::Center),155,335,430,100);
	AddCanvas(R,Text(T,TEXT("Label_Id"),TEXT("ID"),17,ETextJustify::Right),155,465,95,40);
	UEditableTextBox* Id=T->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(),TEXT("EditableTextBox_Id")); Id->SetHintText(FText::FromString(TEXT("ID"))); ApplyFlatEditableStyle(Id); AddCanvas(R,Id,270,465,300,40);
	AddCanvas(R,Text(T,TEXT("Label_Password"),TEXT("Password"),17,ETextJustify::Right),155,520,95,40);
	UEditableTextBox* Pw=T->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(),TEXT("EditableTextBox_Password")); Pw->SetIsPassword(true); Pw->SetHintText(FText::FromString(TEXT("Password"))); ApplyFlatEditableStyle(Pw); AddCanvas(R,Pw,270,520,300,40);
	AddCanvas(R,Button(T,TEXT("Button_Login"),TEXT("LOGIN")),270,585,300,48);
	AddCanvas(R,Text(T,TEXT("Text_Status"),TEXT("Ready"),14,ETextJustify::Center),155,650,415,32); Save(BP);
}

void BuildMain(UWidgetBlueprint* BP)
{
	Reset(BP);
	UWidgetTree* T = BP->WidgetTree;
	UCanvasPanel* R = T->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	T->RootWidget = R;

	UTextBlock* Brand = Text(T, TEXT("Text_Brand"), TEXT("SHOWDOWN"), 92, ETextJustify::Center);
	Brand->SetFont(FSlateFontInfo(ShowDownDisplayFont(), 92));
	Brand->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	AddCanvas(R, Brand, 120.0f, 405.0f, 900.0f, 155.0f);

	const TCHAR* Names[] = {
		TEXT("Button_SinglePlay"),
		TEXT("Button_Multiplayer"),
		TEXT("Button_Shop"),
		TEXT("Button_Ranking"),
		TEXT("Button_Quit")};
	const TCHAR* Labels[] = {
		TEXT("SINGLEPLAY"),
		TEXT("MULTIPLAY"),
		TEXT("SHOP"),
		TEXT("RANK"),
		TEXT("OPTION")};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
	{
		AddCanvas(R, MainMenuButton(T, Names[Index], Labels[Index]), 1480.0f, 440.0f + Index * 56.0f, 330.0f, 50.0f);
	}

	UTextBlock* Coin = Text(T, TEXT("Text_Coin"), TEXT("◉ 0"), 20, ETextJustify::Right);
	UTextBlock* Score = Text(T, TEXT("Text_Score"), TEXT("▣ 0"), 20, ETextJustify::Right);
	UTextBlock* Nickname = Text(T, TEXT("Text_Nickname"), TEXT("이름 없음"), 20, ETextJustify::Right);
	AddCanvas(R, Coin, 1500.0f, 45.0f, 140.0f, 34.0f);
	AddCanvas(R, Score, 1650.0f, 45.0f, 160.0f, 34.0f);
	AddCanvas(R, Nickname, 1450.0f, 88.0f, 360.0f, 38.0f);

	UTextBlock* Status = Text(T, TEXT("Text_Status"), TEXT(""), 14, ETextJustify::Right);
	Status->SetAutoWrapText(true);
	AddCanvas(R, Status, 1370.0f, 130.0f, 440.0f, 65.0f);

	UEditableTextBox* Nick = T->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("EditableTextBox_Nickname"));
	ApplyFlatEditableStyle(Nick);
	Nick->SetVisibility(ESlateVisibility::Collapsed);
	AddCanvas(R, Nick, 0.0f, 0.0f, 1.0f, 1.0f);
	Save(BP);
}

void BuildSimple(UWidgetBlueprint* BP, const TCHAR* Title, const TArray<TPair<FName,FString>>& Buttons, const TArray<FName>& Texts)
{
	Reset(BP); UWidgetTree* T=BP->WidgetTree; UCanvasPanel* R=T->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("RootCanvas")); T->RootWidget=R;
	UBorder* P=T->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("MainPanel")); P->SetBrushColor(Panel); AddCanvas(R,P,300,160,1320,760);
	AddCanvas(R,Text(T,TEXT("Text_Title"),Title,32,ETextJustify::Center),420,205,1080,60);
	int32 Y=300; for(const TPair<FName,FString>& Item:Buttons){ AddCanvas(R,Button(T,*Item.Key.ToString(),*Item.Value),660,Y,600,52); Y+=68; }
	for(const FName& Name:Texts){ AddCanvas(R,Text(T,*Name.ToString(),TEXT(""),16,ETextJustify::Center),520,Y,880,38); Y+=45; }
	Save(BP);
}

void AddTopNavigation(UWidgetTree* T, UCanvasPanel* R, int32 ActiveIndex)
{
	const TCHAR* Labels[]={TEXT("SINGLEPLAY"),TEXT("MULTIPLAY"),TEXT("SHOP"),TEXT("RANK"),TEXT("OPTION")};
	for(int32 I=0;I<5;++I)
	{
		const float Alpha = I == ActiveIndex ? 0.88f : 0.0f;
		AddAnchored(R,Button(T,*FString::Printf(TEXT("Nav_%d"),I),Labels[I],FLinearColor(0,0,0,Alpha)),I/5.0f,0,(I+1)/5.0f,0,FMargin(0,0,0,72));
	}
}

void BuildMultiplayer(UWidgetBlueprint* BP)
{
	Reset(BP);
	UWidgetTree* T = BP->WidgetTree;
	UCanvasPanel* R = T->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	T->RootWidget = R;

	UBorder* Left = T->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MultiplayerLeftPanel"));
	Left->SetBrushColor(FLinearColor(0.28f, 0.28f, 0.28f, 0.78f));
	AddCanvas(R, Left, 60.0f, 100.0f, 1230.0f, 880.0f);
	UBorder* Right = T->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MultiplayerRightPanel"));
	Right->SetBrushColor(FLinearColor(0.055f, 0.055f, 0.055f, 0.92f));
	AddCanvas(R, Right, 1290.0f, 100.0f, 570.0f, 880.0f);

	AddCanvas(R, Text(T, TEXT("Label_PublicRooms"), TEXT("공개 로비 목록"), 20), 170.0f, 205.0f, 620.0f, 40.0f);
	AddCanvas(R, Button(T, TEXT("Button_RefreshRooms"), TEXT("새로고침"), FLinearColor(0.10f, 0.10f, 0.10f, 0.94f)), 1040.0f, 198.0f, 160.0f, 44.0f);
	UBorder* ListBg = T->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PublicRoomListBackground"));
	ListBg->SetBrushColor(FLinearColor(0.10f, 0.10f, 0.10f, 0.28f));
	AddCanvas(R, ListBg, 170.0f, 260.0f, 1030.0f, 555.0f);
	UScrollBox* Rooms = T->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ScrollBox_PublicRooms"));
	Rooms->SetScrollBarVisibility(ESlateVisibility::Visible);
	AddCanvas(R, Rooms, 185.0f, 275.0f, 1000.0f, 525.0f);
	UTextBlock* Status = Text(T, TEXT("Text_Status"), TEXT(""), 15, ETextJustify::Center);
	Status->SetAutoWrapText(true);
	AddCanvas(R, Status, 170.0f, 835.0f, 1030.0f, 68.0f);

	AddCanvas(R, Text(T, TEXT("Label_Create"), TEXT("방 생성"), 20), 1360.0f, 210.0f, 410.0f, 36.0f);
	UEditableTextBox* RoomName = T->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("EditableTextBox_RoomName"));
	RoomName->SetHintText(FText::FromString(TEXT("방 이름 입력")));
	ApplyFlatEditableStyle(RoomName);
	AddCanvas(R, RoomName, 1360.0f, 255.0f, 430.0f, 46.0f);
	AddCanvas(R, BarButton(T, TEXT("Button_Host"), TEXT("공개방 만들기")), 1360.0f, 315.0f, 430.0f, 46.0f);
	AddCanvas(R, BarButton(T, TEXT("Button_PrivateHost"), TEXT("비공개방 만들기")), 1360.0f, 375.0f, 430.0f, 46.0f);

	AddCanvas(R, Text(T, TEXT("Label_Direct"), TEXT("방 코드 입력"), 20), 1360.0f, 520.0f, 410.0f, 36.0f);
	UEditableTextBox* RoomCode = T->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("EditableTextBox_RoomCode"));
	RoomCode->SetHintText(FText::FromString(TEXT("6자리 코드")));
	ApplyFlatEditableStyle(RoomCode);
	AddCanvas(R, RoomCode, 1360.0f, 565.0f, 430.0f, 46.0f);
	AddCanvas(R, BarButton(T, TEXT("Button_Join"), TEXT("코드로 참가")), 1360.0f, 625.0f, 430.0f, 46.0f);
	AddCanvas(R, BarButton(T, TEXT("Button_Back"), TEXT("뒤로")), 1360.0f, 890.0f, 430.0f, 46.0f);
	Save(BP);
}

void BuildLobby(UWidgetBlueprint* BP)
{
	Reset(BP);
	UWidgetTree* T = BP->WidgetTree;
	UCanvasPanel* R = T->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	T->RootWidget = R;

	UBorder* Left = T->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("LobbyLeftPanel"));
	Left->SetBrushColor(FLinearColor(0.30f, 0.30f, 0.30f, 0.70f));
	AddCanvas(R, Left, 95.0f, 100.0f, 1220.0f, 880.0f);
	UBorder* Right = T->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("LobbyRightPanel"));
	Right->SetBrushColor(FLinearColor(0.055f, 0.055f, 0.055f, 0.92f));
	AddCanvas(R, Right, 1315.0f, 100.0f, 510.0f, 880.0f);

	UTextBlock* RoomName = Text(T, TEXT("Text_RoomName"), TEXT("방 이름"), 34, ETextJustify::Center);
	RoomName->SetAutoWrapText(true);
	AddCanvas(R, RoomName, 1355.0f, 165.0f, 430.0f, 72.0f);
	AddCanvas(R, Text(T, TEXT("Text_Code"), TEXT("방 코드: ------"), 18, ETextJustify::Center), 1355.0f, 238.0f, 430.0f, 40.0f);
	AddCanvas(R, Text(T, TEXT("Text_ParticipantHeader"), TEXT("참가자 목록 (0/4)"), 19, ETextJustify::Center), 1355.0f, 320.0f, 430.0f, 40.0f);

	auto AddPlayerRow = [&](int32 PlayerIndex, float Y)
	{
		const FString RowName = FString::Printf(TEXT("Border_Player%d"), PlayerIndex);
		UBorder* Row = T->ConstructWidget<UBorder>(UBorder::StaticClass(), *RowName);
		Row->SetBrushColor(FLinearColor(0.12f, 0.12f, 0.12f, 0.95f));
		AddCanvas(R, Row, 1360.0f, Y, 420.0f, 46.0f);

		const FString TextName = FString::Printf(TEXT("Text_Player%d"), PlayerIndex);
		AddCanvas(R, Text(T, *TextName, TEXT("빈 자리"), 17), 1375.0f, Y + 6.0f, 350.0f, 34.0f);
		if (PlayerIndex > 1)
		{
			const FString ButtonName = FString::Printf(TEXT("Button_KickPlayer%d"), PlayerIndex);
			UButton* KickButton = Button(T, *ButtonName, TEXT("X"), FLinearColor(0.34f, 0.12f, 0.13f, 0.98f));
			// The host-only state is applied once replicated lobby data arrives.
			// Starting collapsed prevents a non-host flash while that data loads.
			KickButton->SetVisibility(ESlateVisibility::Collapsed);
			AddCanvas(R, KickButton, 1735.0f, Y + 5.0f, 36.0f, 36.0f);
		}
	};
	AddPlayerRow(1, 375.0f);
	AddPlayerRow(2, 431.0f);
	AddPlayerRow(3, 487.0f);
	AddPlayerRow(4, 543.0f);

	UTextBlock* LegacyPlayers = Text(T, TEXT("Text_Players"), TEXT(""), 1);
	LegacyPlayers->SetVisibility(ESlateVisibility::Collapsed);
	AddCanvas(R, LegacyPlayers, 0.0f, 0.0f, 1.0f, 1.0f);
	UTextBlock* Status = Text(T, TEXT("Text_Status"), TEXT("참가자를 기다리는 중입니다."), 15, ETextJustify::Center);
	Status->SetAutoWrapText(true);
	AddCanvas(R, Status, 1350.0f, 625.0f, 440.0f, 100.0f);
	AddCanvas(R, BarButton(T, TEXT("Button_Start"), TEXT("게임 시작")), 1360.0f, 850.0f, 420.0f, 46.0f);
	AddCanvas(R, BarButton(T, TEXT("Button_Leave"), TEXT("나가기")), 1360.0f, 910.0f, 420.0f, 46.0f);
	Save(BP);
}

void BuildSettings(UWidgetBlueprint* BP)
{
	Reset(BP);
	UWidgetTree* T = BP->WidgetTree;
	UCanvasPanel* R = T->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	T->RootWidget = R;

	UBorder* Background = T->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SettingsBackground"));
	Background->SetBrushColor(FLinearColor(0.30f, 0.30f, 0.30f, 0.78f));
	AddCanvas(R, Background, 85.0f, 100.0f, 1750.0f, 880.0f);

	AddCanvas(R, Text(T, TEXT("Label_Settings"), TEXT("설정"), 20), 180.0f, 195.0f, 410.0f, 40.0f);
	auto SettingsTabButton = [&](const TCHAR* Name, const TCHAR* Label, const FLinearColor& PreviousBaseColor, bool bActive)
	{
		UButton* TabButton = BarButton(T, Name, Label, PreviousBaseColor);
		FButtonStyle Style = TabButton->GetStyle();
		const FLinearColor NormalColor = bActive
			? FLinearColor(0.0f, 0.0f, 0.0f, 0x99 / 255.0f)
			: FLinearColor(0x19 / 255.0f, 0x19 / 255.0f, 0x19 / 255.0f, 0xFF / 255.0f);
		Style.SetNormal(FlatColorBrush(NormalColor));
		Style.SetHovered(FlatColorBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0xA6 / 255.0f)));
		Style.SetPressed(FlatColorBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0xB3 / 255.0f)));
		TabButton->SetStyle(Style);
		return TabButton;
	};
	AddCanvas(R, SettingsTabButton(TEXT("Button_TabGeneral"), TEXT("일반"), FLinearColor(0.04f, 0.04f, 0.04f, 0.96f), true), 180.0f, 240.0f, 410.0f, 46.0f);
	AddCanvas(R, SettingsTabButton(TEXT("Button_TabGraphics"), TEXT("그래픽"), FLinearColor(0.16f, 0.16f, 0.16f, 0.92f), false), 180.0f, 298.0f, 410.0f, 46.0f);
	AddCanvas(R, SettingsTabButton(TEXT("Button_TabSound"), TEXT("소리"), FLinearColor(0.16f, 0.16f, 0.16f, 0.92f), false), 180.0f, 356.0f, 410.0f, 46.0f);
	AddCanvas(R, BarButton(T, TEXT("Button_Apply"), TEXT("적용")), 180.0f, 777.0f, 410.0f, 46.0f);
	AddCanvas(R, BarButton(T, TEXT("Button_Back"), TEXT("뒤로")), 180.0f, 835.0f, 410.0f, 46.0f);
	AddCanvas(R, BarButton(T, TEXT("Button_Quit"), TEXT("게임 종료")), 180.0f, 893.0f, 410.0f, 46.0f);

	auto AddValueBackground = [&](UCanvasPanel* Parent, const TCHAR* Name, float X, float Y, float W, float H)
	{
		UBorder* ValueBackground = T->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		ValueBackground->SetBrushColor(FLinearColor(0.08f, 0.08f, 0.08f, 0.90f));
		AddCanvas(Parent, ValueBackground, X, Y, W, H);
	};
	auto AddSlider = [&](UCanvasPanel* Parent, const TCHAR* SliderName, const TCHAR* ValueName, const TCHAR* Label, float Value, float Y)
	{
		AddCanvas(Parent, Text(T, *FString::Printf(TEXT("%s_Label"), SliderName), Label, 18), 70.0f, Y, 300.0f, 34.0f);
		USlider* Slider = T->ConstructWidget<USlider>(USlider::StaticClass(), SliderName);
		Slider->SetValue(Value);
		Slider->SetSliderBarColor(FLinearColor(0.93f, 0.93f, 0.93f, 1.0f));
		Slider->SetSliderHandleColor(FLinearColor::White);
		AddCanvas(Parent, Slider, 70.0f, Y + 36.0f, 430.0f, 30.0f);
		AddCanvas(Parent, Text(T, ValueName, *FString::Printf(TEXT("%d"), FMath::RoundToInt(Value * 100.0f)), 16, ETextJustify::Right), 515.0f, Y + 33.0f, 80.0f, 34.0f);
	};
	auto AddValueButton = [&](UCanvasPanel* Parent, const TCHAR* Name, const TCHAR* Value, float Y)
	{
		UButton* ValueButton = BarButton(T, Name, Value);
		AddCanvas(Parent, ValueButton, 70.0f, Y, 420.0f, 46.0f);
		return ValueButton;
	};

	UCanvasPanel* General = T->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Panel_General"));
	AddCanvas(R, General, 650.0f, 100.0f, 1185.0f, 880.0f);
	AddCanvas(General, Text(T, TEXT("EditableTextBox_Nickname_Label"), TEXT("내 닉네임"), 18), 620.0f, 120.0f, 420.0f, 34.0f);
	UEditableTextBox* Nickname = T->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("EditableTextBox_Nickname"));
	Nickname->SetHintText(FText::FromString(TEXT("2~16자 닉네임")));
	ApplyFlatEditableStyle(Nickname);
	AddCanvas(General, Nickname, 620.0f, 160.0f, 330.0f, 46.0f);
	AddCanvas(General, BarButton(T, TEXT("Button_ChangeNickname"), TEXT("변경")), 965.0f, 160.0f, 140.0f, 46.0f);
	UTextBlock* NicknameStatus = Text(T, TEXT("Text_NicknameStatus"), TEXT(""), 15);
	NicknameStatus->SetAutoWrapText(true);
	AddCanvas(General, NicknameStatus, 620.0f, 218.0f, 485.0f, 64.0f);
	AddCanvas(General, Text(T, TEXT("Text_LlmModel_Label"), TEXT("LLM 모델 선택"), 18), 70.0f, 120.0f, 420.0f, 34.0f);
	AddValueBackground(General, TEXT("LlmModelBackground"), 70.0f, 160.0f, 420.0f, 46.0f);
	AddCanvas(General, Text(T, TEXT("Text_LlmModel"), TEXT("gpt-5.6-terra"), 18), 85.0f, 168.0f, 390.0f, 32.0f);
	AddCanvas(General, Text(T, TEXT("EditableTextBox_CharacterName_Label"), TEXT("싱글플레이 적 이름"), 18), 70.0f, 235.0f, 420.0f, 34.0f);
	UEditableTextBox* CharacterName = T->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("EditableTextBox_CharacterName"));
	CharacterName->SetHintText(FText::FromString(TEXT("상대 이름")));
	ApplyFlatEditableStyle(CharacterName);
	AddCanvas(General, CharacterName, 70.0f, 275.0f, 420.0f, 46.0f);
	AddCanvas(General, Text(T, TEXT("Slider_MouseSensitivity_Label"), TEXT("마우스 감도"), 18), 70.0f, 350.0f, 300.0f, 34.0f);
	USlider* MouseSensitivity = T->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("Slider_MouseSensitivity"));
	MouseSensitivity->SetValue(0.444f);
	MouseSensitivity->SetSliderBarColor(FLinearColor::White);
	MouseSensitivity->SetSliderHandleColor(FLinearColor::White);
	AddCanvas(General, MouseSensitivity, 70.0f, 388.0f, 360.0f, 30.0f);
	AddCanvas(General, Text(T, TEXT("Text_MouseSensitivity"), TEXT("1.0"), 16, ETextJustify::Right), 445.0f, 385.0f, 70.0f, 34.0f);

	UCanvasPanel* Graphics = T->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Panel_Graphics"));
	AddCanvas(R, Graphics, 650.0f, 100.0f, 1185.0f, 880.0f);
	Graphics->SetVisibility(ESlateVisibility::Collapsed);
	AddCanvas(Graphics, Text(T, TEXT("Label_DisplaySettings"), TEXT("화면 설정"), 20), 70.0f, 110.0f, 420.0f, 38.0f);
	AddCanvas(Graphics, Text(T, TEXT("Button_WindowMode_LabelTitle"), TEXT("화면 모드"), 18), 70.0f, 165.0f, 420.0f, 34.0f);
	AddValueButton(Graphics, TEXT("Button_WindowMode"), TEXT("테두리 없는 창"), 203.0f);
	AddCanvas(Graphics, Text(T, TEXT("Button_Resolution_LabelTitle"), TEXT("화면 비율"), 18), 70.0f, 270.0f, 420.0f, 34.0f);
	AddValueButton(Graphics, TEXT("Button_Resolution"), TEXT("1920x1080 @60Hz"), 308.0f);
	AddCanvas(Graphics, Text(T, TEXT("Slider_Brightness_Label"), TEXT("밝기"), 18), 70.0f, 375.0f, 300.0f, 34.0f);
	USlider* Brightness = T->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("Slider_Brightness"));
	Brightness->SetValue(0.5f);
	Brightness->SetSliderBarColor(FLinearColor::White);
	Brightness->SetSliderHandleColor(FLinearColor::White);
	AddCanvas(Graphics, Brightness, 70.0f, 413.0f, 360.0f, 30.0f);
	AddCanvas(Graphics, Text(T, TEXT("Text_Brightness"), TEXT("1.0"), 16, ETextJustify::Right), 445.0f, 410.0f, 70.0f, 34.0f);
	AddCanvas(Graphics, Text(T, TEXT("Button_Quality_LabelTitle"), TEXT("그래픽 품질"), 18), 70.0f, 475.0f, 420.0f, 34.0f);
	AddValueButton(Graphics, TEXT("Button_Quality"), TEXT("최상"), 513.0f);

	UCanvasPanel* Sound = T->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Panel_Sound"));
	AddCanvas(R, Sound, 650.0f, 100.0f, 1185.0f, 880.0f);
	Sound->SetVisibility(ESlateVisibility::Collapsed);
	AddSlider(Sound, TEXT("Slider_MasterVolume"), TEXT("Text_MasterVolume"), TEXT("마스터 볼륨"), 1.0f, 95.0f);
	AddSlider(Sound, TEXT("Slider_MusicVolume"), TEXT("Text_MusicVolume"), TEXT("배경음악"), 1.0f, 185.0f);
	AddSlider(Sound, TEXT("Slider_EffectVolume"), TEXT("Text_EffectVolume"), TEXT("효과음"), 1.0f, 275.0f);
	AddSlider(Sound, TEXT("Slider_DialogVolume"), TEXT("Text_DialogVolume"), TEXT("대사"), 1.0f, 365.0f);
	AddCanvas(Sound, Text(T, TEXT("Label_Microphone"), TEXT("마이크 설정"), 20), 70.0f, 475.0f, 420.0f, 38.0f);
	AddCanvas(Sound, Text(T, TEXT("Label_MicInput"), TEXT("입력"), 17), 70.0f, 520.0f, 180.0f, 32.0f);
	AddValueBackground(Sound, TEXT("MicInputBackground"), 70.0f, 553.0f, 420.0f, 42.0f);
	AddCanvas(Sound, Text(T, TEXT("Text_MicInputDevice"), TEXT("기본 입력 장치"), 16), 84.0f, 559.0f, 390.0f, 30.0f);
	USlider* MicInput = T->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("Slider_MicInputVolume"));
	MicInput->SetValue(1.0f); MicInput->SetSliderBarColor(FLinearColor::White); MicInput->SetSliderHandleColor(FLinearColor::White); MicInput->SetIsEnabled(false);
	AddCanvas(Sound, MicInput, 525.0f, 559.0f, 360.0f, 30.0f);
	AddCanvas(Sound, Text(T, TEXT("Text_MicInputVolume"), TEXT("100"), 16, ETextJustify::Right), 900.0f, 556.0f, 70.0f, 34.0f);
	AddCanvas(Sound, Text(T, TEXT("Label_MicOutput"), TEXT("출력"), 17), 70.0f, 625.0f, 180.0f, 32.0f);
	AddValueBackground(Sound, TEXT("MicOutputBackground"), 70.0f, 658.0f, 420.0f, 42.0f);
	AddCanvas(Sound, Text(T, TEXT("Text_MicOutputDevice"), TEXT("기본 출력 장치"), 16), 84.0f, 664.0f, 390.0f, 30.0f);
	USlider* MicOutput = T->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("Slider_MicOutputVolume"));
	MicOutput->SetValue(1.0f); MicOutput->SetSliderBarColor(FLinearColor::White); MicOutput->SetSliderHandleColor(FLinearColor::White); MicOutput->SetIsEnabled(false);
	AddCanvas(Sound, MicOutput, 525.0f, 664.0f, 360.0f, 30.0f);
	AddCanvas(Sound, Text(T, TEXT("Text_MicOutputVolume"), TEXT("100"), 16, ETextJustify::Right), 900.0f, 661.0f, 70.0f, 34.0f);

	UButton* LegacyVSync = Button(T, TEXT("Button_VSync"), TEXT("")); LegacyVSync->SetVisibility(ESlateVisibility::Collapsed); AddCanvas(R, LegacyVSync, 0.0f, 0.0f, 1.0f, 1.0f);
	UButton* LegacyBrightness = Button(T, TEXT("Button_Brightness"), TEXT("")); LegacyBrightness->SetVisibility(ESlateVisibility::Collapsed); AddCanvas(R, LegacyBrightness, 0.0f, 0.0f, 1.0f, 1.0f);
	for (const TCHAR* StateName : {TEXT("Text_Quality"), TEXT("Text_WindowMode"), TEXT("Text_VSync"), TEXT("Text_Resolution")})
	{
		UTextBlock* State = Text(T, StateName, TEXT(""), 1);
		State->SetVisibility(ESlateVisibility::Collapsed);
		AddCanvas(R, State, 0.0f, 0.0f, 1.0f, 1.0f);
	}
	AddCanvas(R, Text(T, TEXT("Text_Status"), TEXT(""), 14, ETextJustify::Center), 700.0f, 930.0f, 1030.0f, 34.0f);
	Save(BP);
}

void BuildRank(UWidgetBlueprint* BP)
{
	Reset(BP); UWidgetTree* T=BP->WidgetTree; UCanvasPanel* R=T->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("RootCanvas")); T->RootWidget=R; AddTopNavigation(T,R,3);
	AddCanvas(R,Text(T,TEXT("Label_RankTitle"),TEXT("RANK"),20),70,115,400,38);
	AddCanvas(R,Text(T,TEXT("Label_Columns"),TEXT("순위          닉네임                         점수"),16),220,175,900,36);
	UScrollBox* RankScroll=T->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(),TEXT("ScrollBox_RankEntries")); AddCanvas(R,RankScroll,220,220,1050,650);
	UVerticalBox* Entries=T->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("Box_Entries")); RankScroll->AddChild(Entries);
	AddCanvas(R,Text(T,TEXT("Text_Status"),TEXT(""),14),220,900,1050,32);
	UTextBlock* RankNicknameState=Text(T,TEXT("Text_RankNickname"),TEXT(""),1); RankNicknameState->SetVisibility(ESlateVisibility::Collapsed); AddCanvas(R,RankNicknameState,0,0,1,1);
	UTextBlock* RankScoreState=Text(T,TEXT("Text_RankScore"),TEXT(""),1); RankScoreState->SetVisibility(ESlateVisibility::Collapsed); AddCanvas(R,RankScoreState,0,0,1,1);
	UButton* Back=Button(T,TEXT("Button_Back"),TEXT("뒤로가기")); AddCanvas(R,Back,70,970,300,44);
	Save(BP);
}

void BuildPause(UWidgetBlueprint* BP)
{
	Reset(BP); UWidgetTree* T=BP->WidgetTree; UCanvasPanel* R=T->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("RootCanvas")); T->RootWidget=R;
	UBorder* Dim=T->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("ScreenDim")); Dim->SetBrushColor(FLinearColor(0,0,0,0.28f)); AddCanvas(R,Dim,0,0,1920,1080);
	UBorder* PausePanel=T->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("PausePanel")); PausePanel->SetBrushColor(FLinearColor(0,0,0,0.68f)); AddCanvas(R,PausePanel,760,300,400,480);
	AddCanvas(R,Text(T,TEXT("Text_PauseTitle"),TEXT("PAUSE"),24,ETextJustify::Center),800,340,320,42);
	AddCanvas(R,Button(T,TEXT("Button_Resume"),TEXT("계속하기")),800,420,320,52);
	AddCanvas(R,Button(T,TEXT("Button_MainMenu"),TEXT("메인메뉴")),800,485,320,52);
	AddCanvas(R,Button(T,TEXT("Button_Settings"),TEXT("설정")),800,550,320,52);
	AddCanvas(R,Button(T,TEXT("Button_Quit"),TEXT("게임종료")),800,615,320,52);
	Save(BP);
}

void BuildMultiResult(UWidgetBlueprint* BP)
{
	Reset(BP); UWidgetTree* T=BP->WidgetTree; UCanvasPanel* R=T->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("RootCanvas")); T->RootWidget=R;
	UBorder* Dim=T->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("ScreenDim")); Dim->SetBrushColor(FLinearColor(0,0,0,0.18f)); AddCanvas(R,Dim,0,0,1920,1080);
	UBorder* ResultPanel=T->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("ResultPanel")); ResultPanel->SetBrushColor(FLinearColor(0,0,0,0.56f)); AddCanvas(R,ResultPanel,640,300,760,570);
	AddCanvas(R,Text(T,TEXT("Text_Title"),TEXT("결과 발표"),28,ETextJustify::Center),700,335,640,55);
	UVerticalBox* RankList=T->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("VerticalBox_RankList")); AddCanvas(R,RankList,735,435,550,300);
	AddCanvas(R,Button(T,TEXT("Button_MainMenu"),TEXT("메인메뉴"),FLinearColor(0,0,0,0.62f)),1480,925,280,52);
	UButton* Restart=Button(T,TEXT("Button_Restart"),TEXT("재시작")); Restart->SetVisibility(ESlateVisibility::Collapsed); AddCanvas(R,Restart,0,0,1,1);
	UTextBlock* Status=Text(T,TEXT("Text_RestartStatus"),TEXT(""),14,ETextJustify::Center); Status->SetVisibility(ESlateVisibility::Collapsed); AddCanvas(R,Status,0,0,1,1);
	Save(BP);
}
}

int32 UShowDownUiBuildCommandlet::Main(const FString& Params)
{
	if (FParse::Param(*Params, TEXT("ValidateMetricIconsOnly")))
	{
		return ValidatePlayerMetricIcons() ? 0 : 1;
	}

	if (FParse::Param(*Params, TEXT("MetricIconsOnly")))
	{
		return PatchPlayerMetricIcons() ? 0 : 1;
	}

	const bool bShopOnly = FParse::Param(*Params, TEXT("ShopOnly"));
	const bool bForceRebuildShopAssets =
		FParse::Param(*Params, TEXT("ForceRebuildShopAssets"));

	if (bForceRebuildShopAssets && !bShopOnly)
	{
		UE_LOG(
			LogShowDownUiBuild,
			Error,
			TEXT("-ForceRebuildShopAssets is only accepted together with -ShopOnly."));
		return 1;
	}

	if (bShopOnly)
	{
		return BuildShopAssets(bForceRebuildShopAssets) ? 0 : 1;
	}

	if (Params.Contains(TEXT("SettingsOnly"), ESearchCase::IgnoreCase))
	{
		PatchSettings(GetOrCreate(TEXT("/Game/UI/WBP_Settings.WBP_Settings"), UShowDownSettingsWidget::StaticClass()));
		return 0;
	}

	if (Params.Contains(TEXT("MainMenuOnly"), ESearchCase::IgnoreCase))
	{
		BuildMain(GetOrCreate(TEXT("/Game/UI/WBP_MainMenu.WBP_MainMenu"),UShowDownMainMenuWidget::StaticClass()));
		return 0;
	}

	const bool bRequestedScreensOnly = Params.Contains(TEXT("RequestedScreens"), ESearchCase::IgnoreCase);
	if (!bRequestedScreensOnly)
	{
		BuildLogin(GetOrCreate(TEXT("/Game/UI/WBP_Login.WBP_Login"),UShowDownLoginWidget::StaticClass()));
	}
	BuildMain(GetOrCreate(TEXT("/Game/UI/WBP_MainMenu.WBP_MainMenu"),UShowDownMainMenuWidget::StaticClass()));
	BuildMultiplayer(GetOrCreate(TEXT("/Game/UI/WBP_Multiplayer.WBP_Multiplayer"),UShowDownMultiplayerWidget::StaticClass()));
	BuildLobby(GetOrCreate(TEXT("/Game/UI/WBP_Lobby.WBP_Lobby"),UShowDownLobbyWidget::StaticClass()));
	PatchSettings(GetOrCreate(TEXT("/Game/UI/WBP_Settings.WBP_Settings"), UShowDownSettingsWidget::StaticClass()));
	if (!bRequestedScreensOnly)
	{
		BuildRank(GetOrCreate(TEXT("/Game/UI/WBP_Rank.WBP_Rank"),UShowDownRankWidget::StaticClass()));
		BuildPause(GetOrCreate(TEXT("/Game/UI/WBP_PauseMenu.WBP_PauseMenu"),UShowDownPauseMenuWidget::StaticClass()));
		BuildMultiResult(GetOrCreate(TEXT("/Game/UI/WBP_MultiResult.WBP_MultiResult"),UShowDownMultiRankWidget::StaticClass()));
	}
	return 0;
}
#else
int32 UShowDownUiBuildCommandlet::Main(const FString& Params){ return 1; }
#endif
