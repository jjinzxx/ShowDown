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
#include "Components/ScrollBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "ShowDownLobbyWidget.h"
#include "ShowDownLoginWidget.h"
#include "ShowDownMainMenuWidget.h"
#include "ShowDownMultiplayerWidget.h"
#include "ShowDownMultiRankWidget.h"
#include "ShowDownRankWidget.h"
#include "ShowDownPauseMenuWidget.h"
#include "ShowDownSettingsWidget.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "WidgetBlueprint.h"
#include "UObject/SavePackage.h"

namespace
{
const FLinearColor Ink(0.92f, 0.95f, 0.96f, 1.0f);
const FLinearColor Panel(0.0f, 0.0f, 0.0f, 0.68f);
const FLinearColor Accent(0.0f, 0.0f, 0.0f, 0.82f);

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

void Save(UWidgetBlueprint* BP)
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
	UPackage::SavePackage(BP->GetOutermost(), BP, *FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()), FSavePackageArgs());
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
		UButton* ValueButton = BarButton(T, Name, Value, FLinearColor(0.08f, 0.08f, 0.08f, 0.94f));
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
	AddCanvas(General, Text(T, TEXT("Text_LlmModel"), TEXT("gpt-5.4-mini"), 18), 85.0f, 168.0f, 390.0f, 32.0f);
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
	AddCanvas(Graphics, Text(T, TEXT("Button_PostProcess_LabelTitle"), TEXT("포스트 이펙트"), 18), 70.0f, 475.0f, 420.0f, 34.0f);
	AddValueButton(Graphics, TEXT("Button_PostProcess"), TEXT("하"), 513.0f);
	AddCanvas(Graphics, Text(T, TEXT("Button_Effects_LabelTitle"), TEXT("특수효과 품질"), 18), 70.0f, 580.0f, 420.0f, 34.0f);
	AddValueButton(Graphics, TEXT("Button_Effects"), TEXT("상"), 618.0f);

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

	UButton* LegacyQuality = Button(T, TEXT("Button_Quality"), TEXT("")); LegacyQuality->SetVisibility(ESlateVisibility::Collapsed); AddCanvas(R, LegacyQuality, 0.0f, 0.0f, 1.0f, 1.0f);
	UButton* LegacyVSync = Button(T, TEXT("Button_VSync"), TEXT("")); LegacyVSync->SetVisibility(ESlateVisibility::Collapsed); AddCanvas(R, LegacyVSync, 0.0f, 0.0f, 1.0f, 1.0f);
	UButton* LegacyBrightness = Button(T, TEXT("Button_Brightness"), TEXT("")); LegacyBrightness->SetVisibility(ESlateVisibility::Collapsed); AddCanvas(R, LegacyBrightness, 0.0f, 0.0f, 1.0f, 1.0f);
	for (const TCHAR* StateName : {TEXT("Text_Quality"), TEXT("Text_WindowMode"), TEXT("Text_VSync"), TEXT("Text_Resolution"), TEXT("Text_PostProcess"), TEXT("Text_Effects")})
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
	BuildSettings(GetOrCreate(TEXT("/Game/UI/WBP_Settings.WBP_Settings"),UShowDownSettingsWidget::StaticClass()));
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
