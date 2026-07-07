#include "ShowDownChatWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Layout/Clipping.h"
#include "PlayerPawn.h"
#include "ShowDownPlayerController.h"
#include "ShowDownGameStateBase.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "TimerManager.h"

namespace
{
	constexpr int32 MaxRenderedChatLines = 60;
	constexpr float ChatLineIntroSeconds = 0.13f;
	constexpr float ChatInputAnimationSeconds = 0.22f;
	constexpr float ChatRecentVisibleSeconds = 6.0f;
	constexpr float ChatVisualInterpSpeed = 9.0f;
	constexpr float ChatRootWidth = 420.0f;
	constexpr float ChatHistoryHeight = 166.0f;
	constexpr float ChatInputHeight = 34.0f;
	constexpr float ChatInputGap = 6.0f;
	constexpr float ChatClosedRootOffsetY = ChatInputHeight + ChatInputGap;
	constexpr float SpeakingIndicatorInterpSpeed = 11.0f;

	const FLinearColor LocalSpeakerColor(0.20f, 0.78f, 1.00f, 1.0f);
	const FLinearColor OpponentSpeakerColor(1.00f, 0.55f, 0.14f, 1.0f);
	const FLinearColor CollectorSpeakerColor(1.00f, 0.34f, 0.18f, 1.0f);
	const FLinearColor SystemSpeakerColor(0.88f, 0.78f, 0.46f, 1.0f);
	const FLinearColor ChatMessageColor(1.0f, 1.0f, 1.0f, 1.0f);
	const FLinearColor ChatPanelColor(0.0f, 0.0f, 0.0f, 1.0f);
	const FLinearColor InputPanelColor(0.0f, 0.0f, 0.0f, 0.90f);
	const FLinearColor InputAccentColor(0.20f, 0.78f, 1.0f, 0.95f);
	const TCHAR* PretendardRegularFontPath = TEXT("/Script/Engine.Font'/Game/UI/Font/Pretendard/static/alternative/Pretendard-Regular_Font.Pretendard-Regular_Font'");

	float EaseOut(float Alpha)
	{
		return FMath::InterpEaseOut(0.0f, 1.0f, FMath::Clamp(Alpha, 0.0f, 1.0f), 2.4f);
	}

	const UObject* ResolvePretendardRegularFont()
	{
		return StaticLoadObject(UObject::StaticClass(), nullptr, PretendardRegularFontPath);
	}

	FSlateFontInfo MakePretendardFont(float Size)
	{
		if (const UObject* FontObject = ResolvePretendardRegularFont())
		{
			return FSlateFontInfo(FontObject, Size);
		}

		FSlateFontInfo FallbackFont;
		FallbackFont.Size = Size;
		return FallbackFont;
	}
}

void UShowDownChatWidget::SetOwningShowDownPawn(APlayerPawn* InOwningPawn)
{
	OwningShowDownPawn = InOwningPawn;
}

void UShowDownChatWidget::SetOwningShowDownController(AShowDownPlayerController* InOwningController)
{
	OwningShowDownController = InOwningController;
}

void UShowDownChatWidget::FocusChatInput()
{
	if (EditableTextBox_ChatInput)
	{
		if (APlayerController* PlayerController = GetOwningPlayer())
		{
			EditableTextBox_ChatInput->SetUserFocus(PlayerController);
		}
		EditableTextBox_ChatInput->SetKeyboardFocus();
	}
}

void UShowDownChatWidget::AppendChatLine(const FString& Speaker, const FString& Message)
{
	const FString TrimmedMessage = Message.TrimStartAndEnd();
	if (TrimmedMessage.IsEmpty())
	{
		return;
	}

	LastChatLineTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	if (bChatHistoryUsesDynamicRows && ScrollBox_ChatHistory)
	{
		AppendDynamicChatLine(Speaker, TrimmedMessage);
		return;
	}

	if (!ChatHistory.IsEmpty())
	{
		ChatHistory += TEXT("\n");
	}

	ChatHistory += FString::Printf(TEXT("%s: %s"), *Speaker, *TrimmedMessage);
	if (Text_ChatHistory)
	{
		Text_ChatHistory->SetText(FText::FromString(ChatHistory));
	}
	if (ScrollBox_ChatHistory)
	{
		ScrollChatHistoryToEnd();
	}
}

void UShowDownChatWidget::SetChatInputOpen(bool bOpen)
{
	if (bChatInputOpen == bOpen && !bChatInputClosing)
	{
		if (!bOpen)
		{
			ApplyInputVisibility(ESlateVisibility::Hidden);
		}
		return;
	}

	bChatInputOpen = bOpen;
	bChatInputClosing = !bOpen;
	ChatInputStateChangedTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	if (bOpen)
	{
		ApplyInputVisibility(ESlateVisibility::Visible);
		if (EditableTextBox_ChatInput)
		{
			EditableTextBox_ChatInput->SetText(FText::GetEmpty());
		}
		SetStatusMessage(FString(), FLinearColor::White);
	}
	else
	{
		SetStatusMessage(FString(), FLinearColor::White);
	}

}

bool UShowDownChatWidget::IsChatInputFocused() const
{
	return EditableTextBox_ChatInput && EditableTextBox_ChatInput->HasKeyboardFocus();
}

void UShowDownChatWidget::SetLocalSpeakingIndicatorVisible(bool bVisible)
{
	bLocalSpeakingIndicatorVisible = bVisible;
	if (bVisible && Text_LocalSpeakingIndicator)
	{
		Text_LocalSpeakingIndicator->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

TSharedRef<SWidget> UShowDownChatWidget::RebuildWidget()
{
	BuildNativeChatLayout();
	return Super::RebuildWidget();
}

void UShowDownChatWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BuildNativeChatLayout();

	bChatHistoryUsesDynamicRows = ScrollBox_ChatHistory != nullptr;
	if (bChatHistoryUsesDynamicRows)
	{
		ScrollBox_ChatHistory->ClearChildren();
	}
	if (Text_ChatHistory)
	{
		Text_ChatHistory->SetVisibility(bChatHistoryUsesDynamicRows ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	if (Button_Send)
	{
		Button_Send->OnClicked.AddDynamic(this, &UShowDownChatWidget::HandleSendClicked);
	}

	if (EditableTextBox_ChatInput)
	{
		EditableTextBox_ChatInput->SetHintText(FText::FromString(TEXT("메시지 입력...")));
		EditableTextBox_ChatInput->SetForegroundColor(ChatMessageColor);
		EditableTextBox_ChatInput->WidgetStyle.SetFont(MakePretendardFont(13.0f));
		EditableTextBox_ChatInput->OnTextCommitted.AddDynamic(this, &UShowDownChatWidget::HandleInputCommitted);
	}

	if (AShowDownGameStateBase* ShowDownGameState = GetWorld() ? GetWorld()->GetGameState<AShowDownGameStateBase>() : nullptr)
	{
		ShowDownGameState->OnCollectorLLMDecision.AddDynamic(this, &UShowDownChatWidget::HandleCollectorLLMDecision);
		ShowDownGameState->OnCollectorLLMStatus.AddDynamic(this, &UShowDownChatWidget::HandleCollectorLLMStatus);
		ShowDownGameState->OnChatMessageReceived.AddDynamic(this, &UShowDownChatWidget::HandleChatMessageReceived);
	}

	if (Border_ChatHistoryBackground)
	{
		Border_ChatHistoryBackground->SetBrushColor(FLinearColor(ChatPanelColor.R, ChatPanelColor.G, ChatPanelColor.B, 0.40f));
	}

	CurrentHistoryBackgroundAlpha = 0.40f;
	CurrentHistoryOpacity = 1.0f;
	LastChatLineTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	SetChatInputOpen(false);
	SetStatusMessage(FString(), FLinearColor::White);
}

void UShowDownChatWidget::NativeDestruct()
{
	if (Button_Send)
	{
		Button_Send->OnClicked.RemoveDynamic(this, &UShowDownChatWidget::HandleSendClicked);
	}

	if (EditableTextBox_ChatInput)
	{
		EditableTextBox_ChatInput->OnTextCommitted.RemoveDynamic(this, &UShowDownChatWidget::HandleInputCommitted);
	}

	if (AShowDownGameStateBase* ShowDownGameState = GetWorld() ? GetWorld()->GetGameState<AShowDownGameStateBase>() : nullptr)
	{
		ShowDownGameState->OnCollectorLLMDecision.RemoveDynamic(this, &UShowDownChatWidget::HandleCollectorLLMDecision);
		ShowDownGameState->OnCollectorLLMStatus.RemoveDynamic(this, &UShowDownChatWidget::HandleCollectorLLMStatus);
		ShowDownGameState->OnChatMessageReceived.RemoveDynamic(this, &UShowDownChatWidget::HandleChatMessageReceived);
	}

	Super::NativeDestruct();
}

void UShowDownChatWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UpdateChatVisualState(InDeltaTime);
}

void UShowDownChatWidget::HandleSendClicked()
{
	SubmitCurrentText();
}

void UShowDownChatWidget::HandleInputCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		SubmitCurrentText();
	}
}

void UShowDownChatWidget::HandleCollectorLLMDecision(const FString& Dialogue, const FString& Intent, EShowDownBetAction Action, int32 TargetBet)
{
	AppendChatLine(TEXT("Collector"), Dialogue);
}

void UShowDownChatWidget::HandleCollectorLLMStatus(bool bSuccess, const FString& Message)
{
	if (bSuccess)
	{
		SetStatusMessage(FString(), FLinearColor::White);
		return;
	}

	SetStatusMessage(Message, FLinearColor::Red);
}

void UShowDownChatWidget::HandleChatMessageReceived(const FString& SenderName, const FString& Message)
{
	AppendChatLine(SenderName, Message);
}

void UShowDownChatWidget::BuildNativeChatLayout()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas_ChatRoot"));
	WidgetTree->RootWidget = RootCanvas;
	if (!RootCanvas)
	{
		return;
	}

	SizeBox_ChatRoot = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SizeBox_ChatRoot"));
	SizeBox_ChatRoot->SetWidthOverride(ChatRootWidth);
	SizeBox_ChatRoot->SetRenderTranslation(FVector2D(0.0f, ChatClosedRootOffsetY));
	if (UCanvasPanelSlot* RootSlot = RootCanvas->AddChildToCanvas(SizeBox_ChatRoot))
	{
		RootSlot->SetAnchors(FAnchors(0.0f, 1.0f, 0.0f, 1.0f));
		RootSlot->SetAlignment(FVector2D(0.0f, 1.0f));
		RootSlot->SetPosition(FVector2D(44.0f, -48.0f));
		RootSlot->SetAutoSize(true);
	}

	Text_LocalSpeakingIndicator = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("Text_LocalSpeakingIndicator"));
	Text_LocalSpeakingIndicator->SetText(FText::FromString(TEXT("말 하는 중...")));
	Text_LocalSpeakingIndicator->SetFont(MakePretendardFont(12.0f));
	Text_LocalSpeakingIndicator->SetColorAndOpacity(FSlateColor(FLinearColor(0.70f, 0.92f, 1.0f, 1.0f)));
	Text_LocalSpeakingIndicator->SetShadowOffset(FVector2D(0.0f, 1.0f));
	Text_LocalSpeakingIndicator->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.65f));
	Text_LocalSpeakingIndicator->SetVisibility(ESlateVisibility::Collapsed);
	Text_LocalSpeakingIndicator->SetRenderOpacity(0.0f);
	if (UCanvasPanelSlot* SpeakingSlot = RootCanvas->AddChildToCanvas(Text_LocalSpeakingIndicator))
	{
		SpeakingSlot->SetAnchors(FAnchors(0.0f, 1.0f, 0.0f, 1.0f));
		SpeakingSlot->SetAlignment(FVector2D(0.0f, 0.5f));
		SpeakingSlot->SetPosition(FVector2D(
			44.0f + ChatRootWidth + 12.0f,
			-48.0f - ChatInputHeight - ChatInputGap - ChatHistoryHeight * 0.5f));
		SpeakingSlot->SetAutoSize(true);
	}

	VerticalBox_ChatRoot = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VerticalBox_ChatRoot"));
	SizeBox_ChatRoot->SetContent(VerticalBox_ChatRoot);

	Border_ChatHistoryBackground = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("Border_ChatHistoryBackground"));
	Border_ChatHistoryBackground->SetBrush(FSlateRoundedBoxBrush(
		FLinearColor(ChatPanelColor.R, ChatPanelColor.G, ChatPanelColor.B, 0.54f),
		2.0f,
		FLinearColor::Transparent,
		0.0f));
	Border_ChatHistoryBackground->SetPadding(FMargin(16.0f, 11.0f, 16.0f, 11.0f));
	Border_ChatHistoryBackground->SetClipping(EWidgetClipping::ClipToBoundsAlways);

	ScrollBox_ChatHistory = WidgetTree->ConstructWidget<UScrollBox>(
		UScrollBox::StaticClass(),
		TEXT("ScrollBox_ChatHistory"));
	FSlateBrush NoDrawBrush;
	NoDrawBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	FScrollBoxStyle ChatScrollBoxStyle = FScrollBoxStyle::GetDefault();
	ChatScrollBoxStyle.SetTopShadowBrush(NoDrawBrush);
	ChatScrollBoxStyle.SetBottomShadowBrush(NoDrawBrush);
	ChatScrollBoxStyle.SetLeftShadowBrush(NoDrawBrush);
	ChatScrollBoxStyle.SetRightShadowBrush(NoDrawBrush);
	ChatScrollBoxStyle.SetVerticalScrolledContentPadding(FMargin(0.0f));
	ScrollBox_ChatHistory->SetWidgetStyle(ChatScrollBoxStyle);
	ScrollBox_ChatHistory->SetAnimateWheelScrolling(true);
	ScrollBox_ChatHistory->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
	ScrollBox_ChatHistory->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	ScrollBox_ChatHistory->SetAlwaysShowScrollbar(false);
	ScrollBox_ChatHistory->SetAlwaysShowScrollbarTrack(false);
	ScrollBox_ChatHistory->SetAllowOverscroll(false);
	ScrollBox_ChatHistory->SetScrollbarThickness(FVector2D::ZeroVector);
	ScrollBox_ChatHistory->SetScrollbarPadding(FMargin(0.0f));
	ScrollBox_ChatHistory->SetClipping(EWidgetClipping::ClipToBoundsAlways);

	USizeBox* HistorySizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(),
		TEXT("SizeBox_ChatHistory"));
	HistorySizeBox->SetHeightOverride(ChatHistoryHeight);
	HistorySizeBox->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	HistorySizeBox->SetContent(ScrollBox_ChatHistory);
	Border_ChatHistoryBackground->SetContent(HistorySizeBox);

	if (UVerticalBoxSlot* HistorySlot = VerticalBox_ChatRoot->AddChildToVerticalBox(Border_ChatHistoryBackground))
	{
		HistorySlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, ChatInputGap));
		HistorySlot->SetHorizontalAlignment(HAlign_Fill);
		HistorySlot->SetVerticalAlignment(VAlign_Bottom);
	}

	Border_ChatInputBackground = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("Border_ChatInputBackground"));
	Border_ChatInputBackground->SetBrush(FSlateRoundedBoxBrush(
		InputPanelColor,
		2.5f,
		FLinearColor::Transparent,
		0.0f));
	Border_ChatInputBackground->SetPadding(FMargin(0.0f));
	Border_ChatInputBackground->SetBrushColor(InputPanelColor);

	EditableTextBox_ChatInput = WidgetTree->ConstructWidget<UEditableTextBox>(
		UEditableTextBox::StaticClass(),
		TEXT("EditableTextBox_ChatInput"));
	EditableTextBox_ChatInput->SetClearKeyboardFocusOnCommit(false);
	EditableTextBox_ChatInput->SetSelectAllTextWhenFocused(false);
	EditableTextBox_ChatInput->SetRevertTextOnEscape(false);
	EditableTextBox_ChatInput->SetHintText(FText::FromString(TEXT("메시지 입력...")));
	EditableTextBox_ChatInput->SetForegroundColor(ChatMessageColor);

	FSlateBrush TransparentInputBrush;
	TransparentInputBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	FEditableTextBoxStyle InputStyle = FEditableTextBoxStyle::GetDefault();
	FSlateFontInfo InputFont = MakePretendardFont(13.0f);
	InputStyle.SetFont(InputFont);
	InputStyle.SetBackgroundImageNormal(TransparentInputBrush);
	InputStyle.SetBackgroundImageHovered(TransparentInputBrush);
	InputStyle.SetBackgroundImageFocused(TransparentInputBrush);
	InputStyle.SetBackgroundImageReadOnly(TransparentInputBrush);
	InputStyle.SetForegroundColor(FSlateColor(ChatMessageColor));
	InputStyle.SetFocusedForegroundColor(FSlateColor(ChatMessageColor));
	InputStyle.SetReadOnlyForegroundColor(FSlateColor(ChatMessageColor));
	InputStyle.SetBackgroundColor(FSlateColor(FLinearColor::Transparent));
	InputStyle.SetPadding(FMargin(12.0f, 0.0f, 10.0f, 0.0f));
	InputStyle.TextStyle.SetColorAndOpacity(FSlateColor(ChatMessageColor));
	EditableTextBox_ChatInput->WidgetStyle = InputStyle;

	UHorizontalBox* InputContentBox = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		TEXT("HorizontalBox_ChatInput"));

	Border_ChatInputAccent = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("Border_ChatInputAccent"));
	Border_ChatInputAccent->SetBrushColor(InputAccentColor);
	USizeBox* AccentSizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(),
		TEXT("SizeBox_ChatInputAccent"));
	AccentSizeBox->SetWidthOverride(4.0f);
	AccentSizeBox->SetContent(Border_ChatInputAccent);
	if (UHorizontalBoxSlot* AccentSlot = InputContentBox->AddChildToHorizontalBox(AccentSizeBox))
	{
		AccentSlot->SetVerticalAlignment(VAlign_Fill);
	}

	if (UHorizontalBoxSlot* EditableSlot = InputContentBox->AddChildToHorizontalBox(EditableTextBox_ChatInput))
	{
		EditableSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		EditableSlot->SetVerticalAlignment(VAlign_Center);
	}

	USizeBox* InputSizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(),
		TEXT("SizeBox_ChatInput"));
	InputSizeBox->SetHeightOverride(ChatInputHeight);
	InputSizeBox->SetContent(InputContentBox);
	Border_ChatInputBackground->SetContent(InputSizeBox);

	if (UVerticalBoxSlot* InputSlot = VerticalBox_ChatRoot->AddChildToVerticalBox(Border_ChatInputBackground))
	{
		InputSlot->SetHorizontalAlignment(HAlign_Fill);
		InputSlot->SetVerticalAlignment(VAlign_Bottom);
	}

	Text_Status = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Status"));
	Text_Status->SetFont(MakePretendardFont(11.0f));
	Text_Status->SetText(FText::GetEmpty());
	Text_Status->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.72f)));
	Text_Status->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* StatusSlot = VerticalBox_ChatRoot->AddChildToVerticalBox(Text_Status))
	{
		StatusSlot->SetPadding(FMargin(2.0f, 4.0f, 0.0f, 0.0f));
		StatusSlot->SetHorizontalAlignment(HAlign_Left);
	}
}

void UShowDownChatWidget::AppendDynamicChatLine(const FString& Speaker, const FString& Message)
{
	if (!ScrollBox_ChatHistory)
	{
		return;
	}

	UHorizontalBox* LineWidget = CreateChatLineWidget(Speaker, Message);
	if (!LineWidget)
	{
		return;
	}

	LineWidget->SetRenderOpacity(1.0f);
	LineWidget->SetRenderTranslation(FVector2D::ZeroVector);
	if (UScrollBoxSlot* LineSlot = Cast<UScrollBoxSlot>(ScrollBox_ChatHistory->AddChild(LineWidget)))
	{
		LineSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 2.0f));
	}

	FRenderedChatLine RenderedLine;
	RenderedLine.RowWidget = LineWidget;
	RenderedLine.SpawnTimeSeconds = LastChatLineTimeSeconds;
	RenderedChatLines.Add(RenderedLine);
	TrimRenderedChatLines();

	ScrollChatHistoryToEnd();
}

UHorizontalBox* UShowDownChatWidget::CreateChatLineWidget(const FString& Speaker, const FString& Message)
{
	UHorizontalBox* LineWidget = WidgetTree
		? WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass())
		: NewObject<UHorizontalBox>(this);
	if (!LineWidget)
	{
		return nullptr;
	}

	UTextBlock* NameText = WidgetTree
		? WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass())
		: NewObject<UTextBlock>(this);
	UTextBlock* MessageText = WidgetTree
		? WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass())
		: NewObject<UTextBlock>(this);
	if (!NameText || !MessageText)
	{
		return LineWidget;
	}

	FSlateFontInfo BaseFont = MakePretendardFont(13.0f);
	NameText->SetFont(BaseFont);
	MessageText->SetFont(BaseFont);
	NameText->SetText(FText::FromString(FString::Printf(TEXT("%s: "), *ResolveDisplaySpeakerName(Speaker))));
	NameText->SetColorAndOpacity(FSlateColor(ResolveSpeakerColor(Speaker)));
	NameText->SetAutoWrapText(false);

	MessageText->SetText(FText::FromString(Message));
	MessageText->SetColorAndOpacity(FSlateColor(ChatMessageColor));
	MessageText->SetAutoWrapText(true);
	MessageText->SetWrapTextAt(310.0f);

	if (UHorizontalBoxSlot* NameSlot = LineWidget->AddChildToHorizontalBox(NameText))
	{
		NameSlot->SetPadding(FMargin(0.0f, 0.0f, 2.0f, 0.0f));
		NameSlot->SetVerticalAlignment(VAlign_Top);
	}
	if (UHorizontalBoxSlot* MessageSlot = LineWidget->AddChildToHorizontalBox(MessageText))
	{
		MessageSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		MessageSlot->SetVerticalAlignment(VAlign_Top);
	}

	return LineWidget;
}

FString UShowDownChatWidget::ResolveDisplaySpeakerName(const FString& Speaker) const
{
	const FString TrimmedSpeaker = Speaker.TrimStartAndEnd();
	if (IsSystemSpeaker(TrimmedSpeaker))
	{
		return TEXT("시스템");
	}
	if (TrimmedSpeaker.Equals(TEXT("Collector"), ESearchCase::IgnoreCase))
	{
		return TEXT("김윤아");
	}

	return TrimmedSpeaker.IsEmpty() ? TEXT("Player") : TrimmedSpeaker.Left(32);
}

FString UShowDownChatWidget::ResolveLocalSpeakerName() const
{
	if (OwningShowDownController)
	{
		return OwningShowDownController->GetChatSenderName();
	}
	if (OwningShowDownPawn)
	{
		return OwningShowDownPawn->GetChatSenderName();
	}
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		const FString PlayerStateName = PlayerController->PlayerState
			? PlayerController->PlayerState->GetPlayerName()
			: FString();
		if (!PlayerStateName.TrimStartAndEnd().IsEmpty())
		{
			return PlayerStateName.TrimStartAndEnd().Left(32);
		}
	}

	return TEXT("Player");
}

FLinearColor UShowDownChatWidget::ResolveSpeakerColor(const FString& Speaker) const
{
	const FString TrimmedSpeaker = Speaker.TrimStartAndEnd();
	if (IsSystemSpeaker(TrimmedSpeaker))
	{
		return SystemSpeakerColor;
	}
	if (TrimmedSpeaker.Equals(TEXT("Collector"), ESearchCase::IgnoreCase))
	{
		return CollectorSpeakerColor;
	}

	const FString LocalSpeakerName = ResolveLocalSpeakerName();
	if (TrimmedSpeaker.Equals(LocalSpeakerName, ESearchCase::IgnoreCase)
		|| (TrimmedSpeaker.Equals(TEXT("Player"), ESearchCase::IgnoreCase) && LocalSpeakerName.Equals(TEXT("Player"), ESearchCase::IgnoreCase)))
	{
		return LocalSpeakerColor;
	}

	return OpponentSpeakerColor;
}

bool UShowDownChatWidget::IsSystemSpeaker(const FString& Speaker) const
{
	return Speaker.Equals(TEXT("System"), ESearchCase::IgnoreCase)
		|| Speaker.Equals(TEXT("시스템"), ESearchCase::IgnoreCase);
}

void UShowDownChatWidget::TrimRenderedChatLines()
{
	while (RenderedChatLines.Num() > MaxRenderedChatLines)
	{
		if (ScrollBox_ChatHistory)
		{
			ScrollBox_ChatHistory->RemoveChildAt(0);
		}
		RenderedChatLines.RemoveAt(0);
	}
}

void UShowDownChatWidget::ScrollChatHistoryToEnd()
{
	if (!ScrollBox_ChatHistory)
	{
		return;
	}

	ScrollBox_ChatHistory->ScrollToEnd();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (ScrollBox_ChatHistory)
			{
				ScrollBox_ChatHistory->ScrollToEnd();
			}
		}));
	}
}

void UShowDownChatWidget::UpdateChatVisualState(float InDeltaTime)
{
	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const bool bRecentMessage = (CurrentTime - LastChatLineTimeSeconds) <= ChatRecentVisibleSeconds;

	const float TargetBackgroundAlpha = bChatInputOpen
		? 0.88f
		: (bRecentMessage ? 0.54f : 0.40f);
	const float TargetHistoryOpacity = 1.0f;
	const float InputAnimationRaw = (CurrentTime - ChatInputStateChangedTimeSeconds) / ChatInputAnimationSeconds;
	const float InputAnimationAlpha = EaseOut(InputAnimationRaw);
	const float ChatRootOffsetY = bChatInputOpen
		? (1.0f - InputAnimationAlpha) * ChatClosedRootOffsetY
		: InputAnimationAlpha * ChatClosedRootOffsetY;

	CurrentHistoryBackgroundAlpha = InDeltaTime > 0.0f
		? FMath::FInterpTo(CurrentHistoryBackgroundAlpha, TargetBackgroundAlpha, InDeltaTime, ChatVisualInterpSpeed)
		: TargetBackgroundAlpha;
	CurrentHistoryOpacity = TargetHistoryOpacity;

	if (SizeBox_ChatRoot)
	{
		SizeBox_ChatRoot->SetRenderTranslation(FVector2D(0.0f, ChatRootOffsetY));
	}
	if (Border_ChatHistoryBackground)
	{
		Border_ChatHistoryBackground->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, CurrentHistoryBackgroundAlpha));
		Border_ChatHistoryBackground->SetRenderTranslation(FVector2D::ZeroVector);
	}
	if (ScrollBox_ChatHistory)
	{
		const float ChatContentMoveAlpha = EaseOut((CurrentTime - LastChatLineTimeSeconds) / ChatLineIntroSeconds);
		const float ChatContentOffsetY = (1.0f - ChatContentMoveAlpha) * 12.0f;
		ScrollBox_ChatHistory->SetRenderOpacity(CurrentHistoryOpacity);
		ScrollBox_ChatHistory->SetRenderTranslation(FVector2D(0.0f, ChatContentOffsetY));
		ScrollBox_ChatHistory->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	}
	if (Text_ChatHistory)
	{
		Text_ChatHistory->SetRenderOpacity(CurrentHistoryOpacity);
	}
	if (Text_LocalSpeakingIndicator)
	{
		const float TargetOpacity = bLocalSpeakingIndicatorVisible ? 1.0f : 0.0f;
		CurrentLocalSpeakingIndicatorOpacity = InDeltaTime > 0.0f
			? FMath::FInterpTo(CurrentLocalSpeakingIndicatorOpacity, TargetOpacity, InDeltaTime, SpeakingIndicatorInterpSpeed)
			: TargetOpacity;

		const bool bShouldShowSpeakingIndicator = bLocalSpeakingIndicatorVisible || CurrentLocalSpeakingIndicatorOpacity > 0.02f;
		Text_LocalSpeakingIndicator->SetVisibility(bShouldShowSpeakingIndicator
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
		Text_LocalSpeakingIndicator->SetRenderOpacity(CurrentLocalSpeakingIndicatorOpacity);
		Text_LocalSpeakingIndicator->SetRenderTranslation(FVector2D(
			0.0f,
			ChatRootOffsetY + (1.0f - CurrentLocalSpeakingIndicatorOpacity) * 4.0f));
	}

	for (FRenderedChatLine& RenderedLine : RenderedChatLines)
	{
		if (UWidget* RowWidget = RenderedLine.RowWidget.Get())
		{
			RowWidget->SetRenderOpacity(1.0f);
			RowWidget->SetRenderTranslation(FVector2D::ZeroVector);
		}
	}

	if (EditableTextBox_ChatInput || Button_Send)
	{
		const float VisibleAlpha = bChatInputOpen ? InputAnimationAlpha : (1.0f - InputAnimationAlpha);
		const float InputOffsetY = bChatInputOpen ? (1.0f - InputAnimationAlpha) * 8.0f : InputAnimationAlpha * 8.0f;

		if (Border_ChatInputBackground)
		{
			Border_ChatInputBackground->SetRenderOpacity(VisibleAlpha);
			Border_ChatInputBackground->SetRenderTranslation(FVector2D(0.0f, InputOffsetY));
		}
		if (EditableTextBox_ChatInput)
		{
			EditableTextBox_ChatInput->SetRenderOpacity(VisibleAlpha);
			EditableTextBox_ChatInput->SetRenderTranslation(FVector2D(0.0f, InputOffsetY));
		}
		if (Button_Send)
		{
			Button_Send->SetRenderOpacity(VisibleAlpha);
			Button_Send->SetRenderTranslation(FVector2D(0.0f, InputOffsetY));
		}

		if (!bChatInputOpen && bChatInputClosing && InputAnimationRaw >= 1.0f)
		{
			ApplyInputVisibility(ESlateVisibility::Hidden);
			bChatInputClosing = false;
		}
	}
}

void UShowDownChatWidget::ApplyInputVisibility(ESlateVisibility NewVisibility)
{
	const bool bShouldHide = NewVisibility == ESlateVisibility::Collapsed
		|| NewVisibility == ESlateVisibility::Hidden;
	if (EditableTextBox_ChatInput)
	{
		EditableTextBox_ChatInput->SetVisibility(NewVisibility);
		if (bShouldHide)
		{
			EditableTextBox_ChatInput->SetRenderOpacity(0.0f);
		}
	}
	if (Border_ChatInputBackground)
	{
		Border_ChatInputBackground->SetVisibility(NewVisibility);
		if (bShouldHide)
		{
			Border_ChatInputBackground->SetRenderOpacity(0.0f);
		}
	}
	if (Button_Send)
	{
		Button_Send->SetVisibility(NewVisibility);
		if (bShouldHide)
		{
			Button_Send->SetRenderOpacity(0.0f);
		}
	}
}

void UShowDownChatWidget::SetStatusMessage(const FString& Message, const FLinearColor& Color)
{
	if (Text_Status)
	{
		Text_Status->SetText(FText::FromString(Message));
		Text_Status->SetColorAndOpacity(FSlateColor(Color));
		Text_Status->SetVisibility(Message.TrimStartAndEnd().IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::HitTestInvisible);
	}
}

bool UShowDownChatWidget::SubmitCurrentText()
{
	if (!EditableTextBox_ChatInput || (!OwningShowDownController && !OwningShowDownPawn))
	{
		return false;
	}

	const FString Message = EditableTextBox_ChatInput->GetText().ToString().TrimStartAndEnd();
	if (Message.IsEmpty())
	{
		EditableTextBox_ChatInput->SetText(FText::GetEmpty());
		if (OwningShowDownController)
		{
			OwningShowDownController->CloseChat();
		}
		else if (OwningShowDownPawn)
		{
			OwningShowDownPawn->CloseChat();
		}
		return false;
	}

	if (OwningShowDownController)
	{
		OwningShowDownController->SubmitDialogueInput(Message);
	}
	else if (OwningShowDownPawn)
	{
		OwningShowDownPawn->SubmitDialogueInput(Message);
	}
	SetStatusMessage(FString(), FLinearColor::White);
	EditableTextBox_ChatInput->SetText(FText::GetEmpty());
	if (OwningShowDownController)
	{
		OwningShowDownController->CloseChat();
	}
	else if (OwningShowDownPawn)
	{
		OwningShowDownPawn->CloseChat();
	}
	return true;
}
