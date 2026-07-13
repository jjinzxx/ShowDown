#include "ShowDownUiBuildCommandlet.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
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
	UButton* B = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name); B->SetBackgroundColor(Color);
	FButtonStyle Style;
	Style.SetNormal(FlatBlackBrush(Color.A));
	Style.SetHovered(FlatBlackBrush(FMath::Min(Color.A + 0.06f, 1.0f)));
	Style.SetPressed(FlatBlackBrush(FMath::Min(Color.A + 0.12f, 1.0f)));
	Style.SetDisabled(FlatBlackBrush(Color.A * 0.5f));
	B->SetStyle(Style);
	B->SetContent(Text(Tree, *FString::Printf(TEXT("%s_Label"), Name), Label, 17, ETextJustify::Center)); return B;
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
	BP->WidgetTree = NewObject<UWidgetTree>(BP, TEXT("WidgetTree"), RF_Transactional);
	BP->WidgetVariableNameToGuidMap.Reset();
}

void Save(UWidgetBlueprint* BP)
{
	BP->WidgetTree->ForEachWidget([BP](UWidget* Widget)
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
	Reset(BP); UWidgetTree* T=BP->WidgetTree; UCanvasPanel* R=T->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("RootCanvas")); T->RootWidget=R;
	UBorder* Bar=T->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("NavigationBar")); Bar->SetBrushColor(Panel); AddAnchored(R,Bar,0,0,1,0,FMargin(0,0,0,72));
	const TCHAR* Names[]={TEXT("Button_SinglePlay"),TEXT("Button_Multiplayer"),TEXT("Button_Shop"),TEXT("Button_Ranking"),TEXT("Button_Quit")};
	const TCHAR* Labels[]={TEXT("SINGLEPLAY"),TEXT("MULTIPLAY"),TEXT("SHOP"),TEXT("RANK"),TEXT("OPTION")};
	for(int32 I=0;I<5;++I) AddAnchored(R,Button(T,Names[I],Labels[I],FLinearColor(0.0f,0.0f,0.0f,0.0f)),I/5.0f,0,(I+1)/5.0f,0,FMargin(0,8,0,56));
	AddCanvas(R,Text(T,TEXT("Text_Nickname"),TEXT("Nickname: -"),18),24,880,300,32);
	AddCanvas(R,Text(T,TEXT("Text_Coin"),TEXT("Coin: 0"),16),24,920,300,28);
	AddCanvas(R,Text(T,TEXT("Text_Score"),TEXT("Score: 0"),16),24,950,300,28);
	AddCanvas(R,Text(T,TEXT("Text_Status"),TEXT(""),14),24,990,500,28);
	UEditableTextBox* Nick=T->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(),TEXT("EditableTextBox_Nickname")); ApplyFlatEditableStyle(Nick); Nick->SetVisibility(ESlateVisibility::Collapsed); AddCanvas(R,Nick,155,878,240,36);
	AddCanvas(R,Button(T,TEXT("Button_ChangeNickname"),TEXT("변경"),FLinearColor(0,0,0,0.55f)),410,878,100,36); Save(BP);
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
	Reset(BP); UWidgetTree* T=BP->WidgetTree; UCanvasPanel* R=T->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("RootCanvas")); T->RootWidget=R; AddTopNavigation(T,R,1);
	UBorder* Left=T->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("MultiplayerLeftPanel")); Left->SetBrushColor(FLinearColor(0,0,0,0.42f)); AddCanvas(R,Left,0,72,600,1008);
	AddCanvas(R,Text(T,TEXT("Label_Create"),TEXT("방 생성"),18),70,150,400,32);
	UEditableTextBox* RoomName=T->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(),TEXT("EditableTextBox_RoomName")); RoomName->SetHintText(FText::FromString(TEXT("방 이름 입력"))); ApplyFlatEditableStyle(RoomName); AddCanvas(R,RoomName,70,195,430,44);
	AddCanvas(R,Button(T,TEXT("Button_Host"),TEXT("공개방 만들기")),70,250,430,44);
	AddCanvas(R,Button(T,TEXT("Button_PrivateHost"),TEXT("비공개방 만들기")),70,305,430,44);
	AddCanvas(R,Text(T,TEXT("Label_Direct"),TEXT("직접 연결"),18),70,420,400,32);
	UEditableTextBox* RoomCode=T->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(),TEXT("EditableTextBox_RoomCode")); RoomCode->SetHintText(FText::FromString(TEXT("방 코드 입력"))); ApplyFlatEditableStyle(RoomCode); AddCanvas(R,RoomCode,70,465,430,44);
	AddCanvas(R,Button(T,TEXT("Button_Join"),TEXT("코드로 참가")),70,520,430,44);
	AddCanvas(R,Text(T,TEXT("Label_PublicRooms"),TEXT("공개 로비 목록"),18),650,150,820,32);
	AddCanvas(R,Button(T,TEXT("Button_RefreshRooms"),TEXT("새로고침")),1510,140,300,44);
	UBorder* ListBg=T->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("PublicRoomListBackground")); ListBg->SetBrushColor(FLinearColor(0,0,0,0.58f)); AddCanvas(R,ListBg,650,195,1160,720);
	UScrollBox* Rooms=T->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(),TEXT("ScrollBox_PublicRooms")); AddCanvas(R,Rooms,670,215,1120,680);
	AddCanvas(R,Button(T,TEXT("Button_Back"),TEXT("BACK")),70,970,430,44);
	AddCanvas(R,Text(T,TEXT("Text_Status"),TEXT(""),15,ETextJustify::Center),650,940,1160,40); Save(BP);
}

void BuildLobby(UWidgetBlueprint* BP)
{
	Reset(BP); UWidgetTree* T=BP->WidgetTree; UCanvasPanel* R=T->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("RootCanvas")); T->RootWidget=R; AddTopNavigation(T,R,1);
	UBorder* Left=T->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("LobbyLeftPanel")); Left->SetBrushColor(FLinearColor(0,0,0,0.42f)); AddCanvas(R,Left,0,72,600,1008);
	AddCanvas(R,Text(T,TEXT("Text_Title"),TEXT("WAITING ROOM"),22),70,145,430,42);
	AddCanvas(R,Text(T,TEXT("Label_Room"),TEXT("방 정보"),18),70,220,430,32);
	AddCanvas(R,Text(T,TEXT("Text_RoomName"),TEXT("이름 없음"),18),70,265,430,44);
	AddCanvas(R,Text(T,TEXT("Text_Code"),TEXT("Code: ------"),18),70,315,430,44);
	AddCanvas(R,Button(T,TEXT("Button_Start"),TEXT("게임 시작")),70,400,430,44);
	AddCanvas(R,Button(T,TEXT("Button_Leave"),TEXT("나가기")),70,455,430,44);
	AddCanvas(R,Text(T,TEXT("Label_Players"),TEXT("참가자 목록"),18),650,150,900,32);
	UBorder* ListBg=T->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("ParticipantListBackground")); ListBg->SetBrushColor(FLinearColor(0,0,0,0.58f)); AddCanvas(R,ListBg,650,195,1160,720);
	AddCanvas(R,Text(T,TEXT("Text_Players"),TEXT("참가자 목록이 없습니다."),17),690,235,1080,620);
	AddCanvas(R,Text(T,TEXT("Text_Status"),TEXT("참가자를 기다리는 중입니다."),15,ETextJustify::Center),650,940,1160,40); Save(BP);
}

void BuildSettings(UWidgetBlueprint* BP)
{
	Reset(BP); UWidgetTree* T=BP->WidgetTree; UCanvasPanel* R=T->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("RootCanvas")); T->RootWidget=R; AddTopNavigation(T,R,4);
	UBorder* Left=T->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("SettingsLeftPanel")); Left->SetBrushColor(FLinearColor(0,0,0,0.42f)); AddCanvas(R,Left,0,72,600,1008);
	AddCanvas(R,Text(T,TEXT("Label_Settings"),TEXT("설정"),18),70,150,430,32);
	AddCanvas(R,Button(T,TEXT("Button_TabGeneral"),TEXT("일반")),70,195,430,44);
	AddCanvas(R,Button(T,TEXT("Button_TabGraphics"),TEXT("그래픽")),70,250,430,44);
	AddCanvas(R,Button(T,TEXT("Button_TabSound"),TEXT("소리")),70,305,430,44);
	AddCanvas(R,Button(T,TEXT("Button_Quit"),TEXT("게임 종료")),70,970,430,44);

	auto AddValue=[&](UCanvasPanel* P,const TCHAR* N,const TCHAR* Label,const TCHAR* Value,int32 Y){AddCanvas(P,Text(T,*FString::Printf(TEXT("%s_Label"),N),Label,16),35,Y,260,34);AddCanvas(P,Text(T,N,Value,16),310,Y,430,34);};
	auto AddSlider=[&](UCanvasPanel* P,const TCHAR* SliderName,const TCHAR* ValueName,const TCHAR* Label,float Value,int32 Y)
	{
		AddCanvas(P,Text(T,*FString::Printf(TEXT("%s_Label"),SliderName),Label,16),35,Y,260,34);
		USlider* Slider=T->ConstructWidget<USlider>(USlider::StaticClass(),SliderName); Slider->SetValue(Value); Slider->SetSliderBarColor(FLinearColor(0,0,0,0.72f)); Slider->SetSliderHandleColor(Ink); AddCanvas(P,Slider,310,Y,430,34);
		AddCanvas(P,Text(T,ValueName,*FString::Printf(TEXT("%d"),FMath::RoundToInt(Value*100.0f)),15,ETextJustify::Right),750,Y,90,34);
	};
	UCanvasPanel* General=T->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("Panel_General")); AddCanvas(R,General,600,72,1320,1008);
	AddValue(General,TEXT("Text_LlmModel"),TEXT("LLM 모델 선택"),TEXT("gpt-5.4-mini"),90);
	AddCanvas(General,Text(T,TEXT("EditableTextBox_CharacterName_Label"),TEXT("싱글플레이 적 이름"),16),35,145,260,34);
	UEditableTextBox* CharacterName=T->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(),TEXT("EditableTextBox_CharacterName")); CharacterName->SetHintText(FText::FromString(TEXT("상대 이름"))); ApplyFlatEditableStyle(CharacterName); AddCanvas(General,CharacterName,310,145,430,38);
	AddCanvas(General,Text(T,TEXT("Slider_MouseSensitivity_Label"),TEXT("마우스 감도"),16),35,200,260,34);
	USlider* MouseSensitivity=T->ConstructWidget<USlider>(USlider::StaticClass(),TEXT("Slider_MouseSensitivity")); MouseSensitivity->SetValue(0.444f); MouseSensitivity->SetSliderBarColor(FLinearColor(0,0,0,0.72f)); MouseSensitivity->SetSliderHandleColor(Ink); AddCanvas(General,MouseSensitivity,310,200,430,34);
	AddCanvas(General,Text(T,TEXT("Text_MouseSensitivity"),TEXT("1.0"),15,ETextJustify::Right),750,200,90,34);
	UCanvasPanel* Graphics=T->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("Panel_Graphics")); AddCanvas(R,Graphics,600,72,1320,1008); Graphics->SetVisibility(ESlateVisibility::Collapsed);
	AddCanvas(Graphics,Button(T,TEXT("Button_WindowMode"),TEXT("화면 모드     테두리 없는 창")),35,90,740,44);
	AddCanvas(Graphics,Button(T,TEXT("Button_Resolution"),TEXT("화면 비율     1920x1080 @60Hz")),35,145,740,44);
	AddCanvas(Graphics,Text(T,TEXT("Slider_Brightness_Label"),TEXT("밝기"),16),35,200,260,34);
	USlider* Brightness=T->ConstructWidget<USlider>(USlider::StaticClass(),TEXT("Slider_Brightness")); Brightness->SetValue(0.5f); Brightness->SetSliderBarColor(FLinearColor(0,0,0,0.72f)); Brightness->SetSliderHandleColor(Ink); AddCanvas(Graphics,Brightness,310,200,430,34);
	UButton* LegacyBrightness=Button(T,TEXT("Button_Brightness"),TEXT("")); LegacyBrightness->SetVisibility(ESlateVisibility::Collapsed); AddCanvas(Graphics,LegacyBrightness,0,0,1,1);
	AddCanvas(Graphics,Button(T,TEXT("Button_PostProcess"),TEXT("포스트이펙트     하")),35,255,740,44);
	AddCanvas(Graphics,Button(T,TEXT("Button_Effects"),TEXT("특수효과 품질     상")),35,310,740,44);
	UButton* LegacyQuality=Button(T,TEXT("Button_Quality"),TEXT("")); LegacyQuality->SetVisibility(ESlateVisibility::Collapsed); AddCanvas(Graphics,LegacyQuality,0,0,1,1);
	UButton* LegacyVSync=Button(T,TEXT("Button_VSync"),TEXT("")); LegacyVSync->SetVisibility(ESlateVisibility::Collapsed); AddCanvas(Graphics,LegacyVSync,0,0,1,1);
	UCanvasPanel* Sound=T->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("Panel_Sound")); AddCanvas(R,Sound,600,72,1320,1008); Sound->SetVisibility(ESlateVisibility::Collapsed);
	AddSlider(Sound,TEXT("Slider_MasterVolume"),TEXT("Text_MasterVolume"),TEXT("마스터 볼륨"),1.0f,90);
	AddSlider(Sound,TEXT("Slider_MusicVolume"),TEXT("Text_MusicVolume"),TEXT("배경음악"),1.0f,145);
	AddSlider(Sound,TEXT("Slider_EffectVolume"),TEXT("Text_EffectVolume"),TEXT("효과음"),1.0f,200);
	AddSlider(Sound,TEXT("Slider_DialogVolume"),TEXT("Text_DialogVolume"),TEXT("대사"),1.0f,255);
	AddCanvas(R,Button(T,TEXT("Button_Apply"),TEXT("APPLY")),1450,960,300,44); AddCanvas(R,Button(T,TEXT("Button_Back"),TEXT("BACK")),1100,960,300,44);
	UTextBlock* QualityState=Text(T,TEXT("Text_Quality"),TEXT("EPIC"),1); QualityState->SetVisibility(ESlateVisibility::Collapsed); AddCanvas(R,QualityState,0,0,1,1);
	UTextBlock* WindowModeState=Text(T,TEXT("Text_WindowMode"),TEXT("BORDERLESS"),1); WindowModeState->SetVisibility(ESlateVisibility::Collapsed); AddCanvas(R,WindowModeState,0,0,1,1);
	UTextBlock* VSyncState=Text(T,TEXT("Text_VSync"),TEXT("ON"),1); VSyncState->SetVisibility(ESlateVisibility::Collapsed); AddCanvas(R,VSyncState,0,0,1,1);
	UTextBlock* ResolutionState=Text(T,TEXT("Text_Resolution"),TEXT("1920x1080"),1); ResolutionState->SetVisibility(ESlateVisibility::Collapsed); AddCanvas(R,ResolutionState,0,0,1,1);
	AddCanvas(Graphics,Text(T,TEXT("Text_Brightness"),TEXT("1.0"),15,ETextJustify::Right),750,200,90,34);
	UTextBlock* PostProcessState=Text(T,TEXT("Text_PostProcess"),TEXT("LOW"),1); PostProcessState->SetVisibility(ESlateVisibility::Collapsed); AddCanvas(R,PostProcessState,0,0,1,1);
	UTextBlock* EffectsState=Text(T,TEXT("Text_Effects"),TEXT("HIGH"),1); EffectsState->SetVisibility(ESlateVisibility::Collapsed); AddCanvas(R,EffectsState,0,0,1,1);
	AddCanvas(R,Text(T,TEXT("Text_Status"),TEXT(""),14,ETextJustify::Center),650,910,1100,35); Save(BP);
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
	BuildLogin(GetOrCreate(TEXT("/Game/UI/WBP_Login.WBP_Login"),UShowDownLoginWidget::StaticClass()));
	BuildMain(GetOrCreate(TEXT("/Game/UI/WBP_MainMenu.WBP_MainMenu"),UShowDownMainMenuWidget::StaticClass()));
	BuildMultiplayer(GetOrCreate(TEXT("/Game/UI/WBP_Multiplayer.WBP_Multiplayer"),UShowDownMultiplayerWidget::StaticClass()));
	BuildLobby(GetOrCreate(TEXT("/Game/UI/WBP_Lobby.WBP_Lobby"),UShowDownLobbyWidget::StaticClass()));
	BuildSettings(GetOrCreate(TEXT("/Game/UI/WBP_Settings.WBP_Settings"),UShowDownSettingsWidget::StaticClass()));
	BuildRank(GetOrCreate(TEXT("/Game/UI/WBP_Rank.WBP_Rank"),UShowDownRankWidget::StaticClass()));
	BuildPause(GetOrCreate(TEXT("/Game/UI/WBP_PauseMenu.WBP_PauseMenu"),UShowDownPauseMenuWidget::StaticClass()));
	BuildMultiResult(GetOrCreate(TEXT("/Game/UI/WBP_MultiResult.WBP_MultiResult"),UShowDownMultiRankWidget::StaticClass()));
	return 0;
}
#else
int32 UShowDownUiBuildCommandlet::Main(const FString& Params){ return 1; }
#endif
