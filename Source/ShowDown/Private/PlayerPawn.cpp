#include "PlayerPawn.h"

#include "Camera/CameraComponent.h"
#include "Card.h"
#include "InputCoreTypes.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "InputActionValue.h"
#include "ShowDownGameModeBase.h"
#include "ShowDownGameStateBase.h"
#include "ShowDownChatWidget.h"
#include "ShowDownPlayerController.h"
#include "ShowDownVoiceSubsystem.h"
#include "SupabaseSubsystem.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	bool IsShowDownControllerHandlingInput(const APawn* Pawn)
	{
		const AShowDownPlayerController* ShowDownController =
			Pawn ? Cast<AShowDownPlayerController>(Pawn->GetController()) : nullptr;
		return ShowDownController && ShowDownController->HandlesShowDownGameplayInput();
	}
}

APlayerPawn::APlayerPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessPlayer = EAutoReceiveInput::Player0;
	bReplicates = true;

	static ConstructorHelpers::FClassFinder<UShowDownChatWidget> ChatWidgetBlueprint(
		TEXT("/Game/UI/WBP_Chat"));
	if (ChatWidgetBlueprint.Succeeded())
	{
		ChatWidgetClass = ChatWidgetBlueprint.Class;
	}
	else
	{
		ChatWidgetClass = UShowDownChatWidget::StaticClass();
	}

	// 루트 메시 컴포넌트 생성
	rootComp = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(rootComp);

	// 카메라 컴포넌트 생성
	cameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	cameraComp->SetupAttachment(rootComp);
	cameraComp->bUsePawnControlRotation = true;
	bUseControllerRotationPitch = true;
	bUseControllerRotationYaw = true;

	// 손패 카드 슬롯
	PlayerHandCard = CreateDefaultSubobject<USceneComponent>(TEXT("PlayerHandRoot"));
	PlayerHandCard->SetupAttachment(rootComp);

	// 머리 위 카드 슬롯
	PlayerHeadCard = CreateDefaultSubobject<USceneComponent>(TEXT("PlayerHeadCardSlot"));
	PlayerHeadCard->SetupAttachment(rootComp);
}

void APlayerPawn::PreInitializeComponents()
{
	// Preserve the authored standalone map behavior, while preventing every
	// replicated pawn from trying to claim local Player0 in a networked match.
	if (GetNetMode() != NM_Standalone)
	{
		AutoPossessPlayer = EAutoReceiveInput::Disabled;

		// The multiplayer map does not depend on authored pawn instances. Keep the
		// player's own hand directly in front of the camera so center-screen card
		// tracing works for every replicated pawn.
		cameraComp->SetRelativeLocation(FVector(-220.0f, 0.0f, 220.0f));
		// Each player's own hand is rendered in front of their seat camera.
		// Keeping it slightly below the horizon makes it selectable without
		// covering the shared table.
		PlayerHandCard->SetRelativeLocation(FVector(300.0f, 0.0f, -35.0f));
		PlayerHeadCard->SetRelativeLocation(FVector(250.0f, 0.0f, 125.0f));
	}

	Super::PreInitializeComponents();
}

void APlayerPawn::BeginPlay()
{
	Super::BeginPlay();

	if (ToggleChatKey == VoicePushToTalkKey)
	{
		ToggleChatKey = EKeys::Enter;
	}

	if (HasAuthority())
	{
		ModeBase = ResolveGameMode();
	}

	if (!IsShowDownControllerHandlingInput(this))
	{
		AddInputMappingContext();
	}
}

void APlayerPawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (!IsShowDownControllerHandlingInput(this))
	{
		AddInputMappingContext();
	}
	bHasPreviousMousePosition = false;
}

void APlayerPawn::AddInputMappingContext()
{
	if (IsShowDownControllerHandlingInput(this))
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		PC->bShowMouseCursor = true;
		PC->bEnableClickEvents = true;
		PC->bEnableMouseOverEvents = true;

		if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
				LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				if (imc_SD)
				{
					Subsystem->AddMappingContext(imc_SD, 0);
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("imc_SD is not assigned on %s."), *GetName());
				}
			}
		}
	}
}

void APlayerPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (IsShowDownControllerHandlingInput(this))
	{
		bHasPreviousMousePosition = false;
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		bHasPreviousMousePosition = false;
		return;
	}

	if (bChatOpen)
	{
		if (PC->WasInputKeyJustPressed(CloseChatKey))
		{
			CloseChat();
		}
		bHasPreviousMousePosition = false;
		return;
	}

	if (PC->WasInputKeyJustPressed(ToggleChatKey))
	{
		OpenChat();
		bHasPreviousMousePosition = false;
		return;
	}

	HandleVoicePushToTalkInput();

	if (bUseMousePositionLookFallback)
	{
		float MouseX = 0.0f;
		float MouseY = 0.0f;
		if (PC->GetMousePosition(MouseX, MouseY))
		{
			if (!bHasPreviousMousePosition)
			{
				PreviousMouseX = MouseX;
				PreviousMouseY = MouseY;
				bHasPreviousMousePosition = true;
			}
			else
			{
				const float DeltaX = MouseX - PreviousMouseX;
				const float DeltaY = MouseY - PreviousMouseY;

				PreviousMouseX = MouseX;
				PreviousMouseY = MouseY;

				if (!FMath::IsNearlyZero(DeltaX) || !FMath::IsNearlyZero(DeltaY))
				{
					ApplyCameraInput(DeltaX, -DeltaY);
				}
			}
		}
		else
		{
			bHasPreviousMousePosition = false;
		}
	}
	else
	{
		bHasPreviousMousePosition = false;
	}
	
	if (bEnableLegacyKeyboardBetHotkeys)
	{
		HandleBettingHotkeys();
	}
}

void APlayerPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (IsShowDownControllerHandlingInput(this))
	{
		return;
	}

	auto PlayerInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!PlayerInput)
	{
		UE_LOG(LogTemp, Error, TEXT("EnhancedInputComponent is missing."));
		return;
	}
	
	if (ia_Select)
	{
		PlayerInput->BindAction(ia_Select, ETriggerEvent::Started, this, &APlayerPawn::InputSelect);
	}

	if (ia_LookUp)
	{
		PlayerInput->BindAction(ia_LookUp, ETriggerEvent::Triggered, this, &APlayerPawn::LookUp);
	}

	if (ia_Turn)
	{
		PlayerInput->BindAction(ia_Turn, ETriggerEvent::Triggered, this, &APlayerPawn::Turn);
	}

	if (ia_CheckOrCall)
	{
		PlayerInput->BindAction(ia_CheckOrCall, ETriggerEvent::Started, this, &APlayerPawn::InputCheckOrCall);
	}

	if (ia_Raise)
	{
		PlayerInput->BindAction(ia_Raise, ETriggerEvent::Started, this, &APlayerPawn::InputRaise);
	}

	if (ia_Fold)
	{
		PlayerInput->BindAction(ia_Fold, ETriggerEvent::Started, this, &APlayerPawn::InputFold);
	}

	if (ia_RaiseTo2)
	{
		PlayerInput->BindAction(ia_RaiseTo2, ETriggerEvent::Started, this, &APlayerPawn::InputRaiseTo2);
	}

	if (ia_RaiseTo3)
	{
		PlayerInput->BindAction(ia_RaiseTo3, ETriggerEvent::Started, this, &APlayerPawn::InputRaiseTo3);
	}

	if (ia_RaiseTo4)
	{
		PlayerInput->BindAction(ia_RaiseTo4, ETriggerEvent::Started, this, &APlayerPawn::InputRaiseTo4);
	}

	if (ia_RaiseTo5)
	{
		PlayerInput->BindAction(ia_RaiseTo5, ETriggerEvent::Started, this, &APlayerPawn::InputRaiseTo5);
	}

	if (ia_RaiseTo6)
	{
		PlayerInput->BindAction(ia_RaiseTo6, ETriggerEvent::Started, this, &APlayerPawn::InputRaiseTo6);
	}
}

void APlayerPawn::SDWin()
{
	if (!bEnableDebugWinCommand)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Debug] SDWin ignored. Enable bEnableDebugWinCommand on the player pawn to use it."));
		return;
	}

	AShowDownGameStateBase* ShowDownGameState =
		GetWorld() ? GetWorld()->GetGameState<AShowDownGameStateBase>() : nullptr;

	if (!ShowDownGameState)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Debug] SDWin: ShowDownGameState not found."));
		return;
	}

	// 실제 전 스테이지 클리어 승리와 동일한 이벤트를 발생시켜
	// HubFlowManager의 보상 지급 + 허브 복귀 흐름을 그대로 태웁니다.
	ShowDownGameState->SetPhase(EShowDownPhase::GameOver);
	ShowDownGameState->OnGameOver.Broadcast(EShowDownSide::Player);

	UE_LOG(LogTemp, Log, TEXT("[Debug] SDWin: forced player victory."));
}

void APlayerPawn::InputSelect(const FInputActionValue& InputValue)
{
	TraceCardUnderCursor();

	if (HandCard)
	{
		SelectCard(HandCard);
	}
}

void APlayerPawn::TraceCardUnderCursor()
{
	HandCard = nullptr;

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	FHitResult Hit;
	PC->GetHitResultUnderCursor(ECC_Visibility, false, Hit);

	HandCard = Cast<ACard>(Hit.GetActor());
	if (HandCard && !HandCard->IsCardSelectable())
	{
		HandCard = nullptr;
	}
}

void APlayerPawn::SelectCard(ACard* SelectedCard)
{
	if (!SelectedCard)
	{
		return;
	}

	if (CurrentSelectedCard == SelectedCard)
	{
		SubmitSelectedCard(SelectedCard);
		return;
	}

	if (CurrentSelectedCard)
	{
		CurrentSelectedCard->SelectCard(false);
	}

	SelectedCard->SelectCard(true);
	CurrentSelectedCard = SelectedCard;
}

void APlayerPawn::SubmitSelectedCard(ACard* SelectedCard)
{
	if (!SelectedCard)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Submit selected card: %s"), *SelectedCard->GetName());

	if (HasAuthority())
	{
		if (AShowDownGameModeBase* GameMode = ResolveGameMode())
		{
			GameMode->PlayerSelectedCardFromController(GetController(), SelectedCard);
		}
		return;
	}

	ServerSubmitSelectedCard(SelectedCard);
}

void APlayerPawn::ToggleChat()
{
	if (bChatOpen)
	{
		CloseChat();
	}
	else
	{
		OpenChat();
	}
}

void APlayerPawn::OpenChat()
{
	EnsureChatWidget();
	if (!ChatWidget)
	{
		return;
	}

	bChatOpen = true;
	ChatWidget->SetVisibility(ESlateVisibility::Visible);
	ApplyChatInputMode(true);
	ChatWidget->FocusChatInput();
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (bChatOpen && ChatWidget)
		{
			ChatWidget->FocusChatInput();
		}
	}));
}

void APlayerPawn::CloseChat()
{
	bChatOpen = false;
	if (ChatWidget)
	{
		ChatWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	ApplyChatInputMode(false);
}

void APlayerPawn::SubmitDialogueInput(const FString& Text)
{
	const FString TrimmedText = Text.TrimStartAndEnd();
	if (TrimmedText.IsEmpty())
	{
		return;
	}

	if (HasAuthority())
	{
		if (AShowDownGameModeBase* GameMode = ResolveGameMode())
		{
			GameMode->SubmitPlayerDialogueInputFromPlayer(TrimmedText, GetChatSenderName());
		}
		return;
	}

	ServerSubmitDialogueInput(TrimmedText, GetChatSenderName());
}

void APlayerPawn::RequestPlayerCheck()
{
	SubmitPlayerBetAction(EShowDownBetAction::Check, 0);
}

void APlayerPawn::RequestPlayerRaise()
{
	SubmitPlayerBetAction(EShowDownBetAction::Raise, 0);
}

void APlayerPawn::RequestPlayerRaiseTo(int32 BulletCount)
{
	SubmitPlayerBetAction(EShowDownBetAction::Raise, BulletCount);
}

void APlayerPawn::RequestPlayerFold()
{
	SubmitPlayerBetAction(EShowDownBetAction::Fold, 0);
}

void APlayerPawn::SubmitPlayerBetAction(EShowDownBetAction Action, int32 TargetBet)
{
	if (HasAuthority())
	{
		AShowDownGameModeBase* GameMode = ResolveGameMode();
		if (!GameMode)
		{
			return;
		}

		GameMode->RequestPlayerBetActionFromController(GetController(), Action, TargetBet);
		return;
	}

	switch (Action)
	{
	case EShowDownBetAction::Check:
	case EShowDownBetAction::Call:
		ServerPlayerCheck();
		break;

	case EShowDownBetAction::Raise:
		if (TargetBet != 0)
		{
			ServerPlayerRaiseTo(TargetBet);
		}
		else
		{
			ServerPlayerRaise();
		}
		break;

	case EShowDownBetAction::Fold:
		ServerPlayerFold();
		break;

	default:
		break;
	}
}

AShowDownGameModeBase* APlayerPawn::ResolveGameMode()
{
	if (!ModeBase && HasAuthority())
	{
		ModeBase = GetWorld() ? GetWorld()->GetAuthGameMode<AShowDownGameModeBase>() : nullptr;
	}

	return ModeBase;
}

void APlayerPawn::ServerSubmitDialogueInput_Implementation(const FString& Text, const FString& SenderName)
{
	const FString TrimmedText = Text.TrimStartAndEnd();
	if (!TrimmedText.IsEmpty())
	{
		if (AShowDownGameModeBase* GameMode = ResolveGameMode())
		{
			GameMode->SubmitPlayerDialogueInputFromPlayer(TrimmedText, SenderName);
		}
	}
}

void APlayerPawn::ServerSubmitSelectedCard_Implementation(ACard* SelectedCard)
{
	if (SelectedCard)
	{
		if (AShowDownGameModeBase* GameMode = ResolveGameMode())
		{
			GameMode->PlayerSelectedCardFromController(GetController(), SelectedCard);
		}
	}
}

// 상하 회전 입력에 따른 콜백 함수 구현
void APlayerPawn::LookUp(const FInputActionValue& inputValue)
{
	float value = inputValue.Get<float>();
	ApplyCameraInput(0.0f, value);
}

// 좌우 회전 입력에 따른 콜백 함수 구현
void APlayerPawn::Turn(const FInputActionValue& inputValue)
{
	float value = inputValue.Get<float>();
	ApplyCameraInput(value, 0.0f);
}

void APlayerPawn::InputCheckOrCall(const FInputActionValue& InputValue)
{
	(void)InputValue;
	RequestPlayerCheck();
}

void APlayerPawn::InputRaise(const FInputActionValue& InputValue)
{
	(void)InputValue;
	RequestPlayerRaise();
}

void APlayerPawn::InputFold(const FInputActionValue& InputValue)
{
	(void)InputValue;
	RequestPlayerFold();
}

void APlayerPawn::InputRaiseTo2(const FInputActionValue& InputValue)
{
	(void)InputValue;
	RequestPlayerRaiseTo(2);
}

void APlayerPawn::InputRaiseTo3(const FInputActionValue& InputValue)
{
	(void)InputValue;
	RequestPlayerRaiseTo(3);
}

void APlayerPawn::InputRaiseTo4(const FInputActionValue& InputValue)
{
	(void)InputValue;
	RequestPlayerRaiseTo(4);
}

void APlayerPawn::InputRaiseTo5(const FInputActionValue& InputValue)
{
	(void)InputValue;
	RequestPlayerRaiseTo(5);
}

void APlayerPawn::InputRaiseTo6(const FInputActionValue& InputValue)
{
	(void)InputValue;
	RequestPlayerRaiseTo(6);
}

void APlayerPawn::ApplyCameraInput(float YawInput, float PitchInput)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	const FRotator ControlRotation = PC->GetControlRotation();
	const float CurrentPitch = FRotator::NormalizeAxis(ControlRotation.Pitch);
	const float CurrentYaw = FRotator::NormalizeAxis(ControlRotation.Yaw);

	const float NewPitch = FMath::Clamp(CurrentPitch + PitchInput * LookSensitivity, MinPitch, MaxPitch);
	const float NewYaw = FMath::Clamp(CurrentYaw + YawInput * LookSensitivity, MinYaw, MaxYaw);

	PC->SetControlRotation(FRotator(NewPitch, NewYaw, 0.0f));
}

void APlayerPawn::HandleBettingHotkeys()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	if (PC->WasInputKeyJustPressed(EKeys::Q))
	{
		RequestPlayerCheck();
	}

	if (PC->WasInputKeyJustPressed(EKeys::R))
	{
		RequestPlayerFold();
	}

	if (PC->WasInputKeyJustPressed(EKeys::E))
	{
		RequestPlayerRaise();
	}

	if (PC->WasInputKeyJustPressed(EKeys::One))
	{
		SubmitPlayerBetAction(EShowDownBetAction::Raise, -1);
	}

	if (PC->WasInputKeyJustPressed(EKeys::Two))
	{
		SubmitPlayerBetAction(EShowDownBetAction::Raise, -2);
	}

	if (PC->WasInputKeyJustPressed(EKeys::Three))
	{
		SubmitPlayerBetAction(EShowDownBetAction::Raise, -3);
	}

	if (PC->WasInputKeyJustPressed(EKeys::Four))
	{
		SubmitPlayerBetAction(EShowDownBetAction::Raise, -4);
	}

	if (PC->WasInputKeyJustPressed(EKeys::Five))
	{
		SubmitPlayerBetAction(EShowDownBetAction::Raise, -5);
	}
}

void APlayerPawn::HandleVoicePushToTalkInput()
{
	if (!bEnableVoicePushToTalk)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	UShowDownVoiceSubsystem* VoiceSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UShowDownVoiceSubsystem>()
		: nullptr;
	if (!PC || !VoiceSubsystem || VoiceSubsystem->VoiceInputMode != EShowDownVoiceInputMode::PushToTalk)
	{
		return;
	}

	if (PC->WasInputKeyJustPressed(VoicePushToTalkKey))
	{
		VoiceSubsystem->BeginPushToTalk(
			FShowDownVoiceTextCallback::CreateWeakLambda(
				this,
				[this](bool bSuccess, const FString& Text)
				{
					if (bSuccess)
					{
						SubmitDialogueInput(Text);
					}
				}));
	}

	if (PC->WasInputKeyJustReleased(VoicePushToTalkKey))
	{
		VoiceSubsystem->EndPushToTalk();
	}
}

void APlayerPawn::ServerPlayerCheck_Implementation()
{
	SubmitPlayerBetAction(EShowDownBetAction::Check, 0);
}

void APlayerPawn::ServerPlayerRaise_Implementation()
{
	SubmitPlayerBetAction(EShowDownBetAction::Raise, 0);
}

void APlayerPawn::ServerPlayerRaiseTo_Implementation(int32 BulletCount)
{
	SubmitPlayerBetAction(EShowDownBetAction::Raise, BulletCount);
}

void APlayerPawn::ServerPlayerFold_Implementation()
{
	SubmitPlayerBetAction(EShowDownBetAction::Fold, 0);
}

void APlayerPawn::EnsureChatWidget()
{
	if (ChatWidget)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !ChatWidgetClass)
	{
		if (!ChatWidgetClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("ChatWidgetClass is not assigned on %s."), *GetName());
		}
		return;
	}

	ChatWidget = CreateWidget<UShowDownChatWidget>(PC, ChatWidgetClass);
	if (!ChatWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to create chat widget on %s."), *GetName());
		return;
	}

	ChatWidget->SetOwningShowDownPawn(this);
	ChatWidget->AddToViewport();
	ChatWidget->SetVisibility(ESlateVisibility::Collapsed);
}

void APlayerPawn::ApplyChatInputMode(bool bOpen)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	PC->bShowMouseCursor = true;
	if (bOpen && ChatWidget)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(ChatWidget->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
	}
	else
	{
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
	}
}

FString APlayerPawn::GetChatSenderName() const
{
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const USupabaseSubsystem* SupabaseSubsystem = GameInstance->GetSubsystem<USupabaseSubsystem>())
		{
			const FString Nickname = SupabaseSubsystem->GetNickname().TrimStartAndEnd();
			if (!Nickname.IsEmpty())
			{
				return Nickname.Left(32);
			}
		}
	}

	const APlayerState* CurrentPlayerState = GetPlayerState();
	FString SenderName = CurrentPlayerState ? CurrentPlayerState->GetPlayerName() : TEXT("Player");
	SenderName = SenderName.TrimStartAndEnd();
	if (SenderName.IsEmpty() || SenderName.StartsWith(TEXT("DESKTOP-")))
	{
		return TEXT("Player");
	}

	return SenderName.Left(32);
}
