#include "ShowDownPlayerController.h"
#include "ShowDownPauseMenuWidget.h"
#include "ShowDownSettingsWidget.h"
#include "ShowDownHubFlowManager.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Card.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Interaction/SDInteractable.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "PlayerPawn.h"
#include "Presentation/SDBetActionPanelActor.h"
#include "SDPlayerState.h"
#include "ShowDownCameraAspect.h"
#include "ShowDownCharacter.h"
#include "ShowDownCharacterSkinCatalog.h"
#include "ShowDownChatWidget.h"
#include "ShowDownEosSubsystem.h"
#include "ShowDownGameModeBase.h"
#include "ShowDownGameStateBase.h"
#include "ShowDownLeaveConfirmWidget.h"
#include "ShowDownLoadingScreen.h"
#include "ShowDownMultiRankWidget.h"
#include "ShowDownTransitionWidget.h"
#include "ShowDownVoiceSubsystem.h"
#include "UObject/ConstructorHelpers.h"
#include "SlateOptMacros.h"
#include "Styling/CoreStyle.h"
#include "SupabaseSubsystem.h"
#include "TimerManager.h"
#include "Widgets/SLeafWidget.h"

namespace
{
	const TCHAR* DefaultInteractionOutlineMaterialPath = TEXT("/Game/ArtTone/M_PP_InteractionOutline.M_PP_InteractionOutline");
	constexpr float CharacterHeadLookReplicationInterval = 0.05f;
	constexpr float CharacterHeadLookReplicationAngleThreshold = 0.5f;

	void RestoreMultiplayerNameTagScreenRegistrations(UWorld* World)
	{
		if (!World)
		{
			return;
		}

		for (TActorIterator<AShowDownCharacter> CharacterIt(World); CharacterIt; ++CharacterIt)
		{
			CharacterIt->RestoreNameTagScreenRegistration();
		}
	}

	bool IsOutlineablePrimitive(const UPrimitiveComponent* Component)
	{
		return Component
			&& Component->IsRegistered()
			&& Component->IsVisible()
			&& Component->IsA<UMeshComponent>();
	}

	EShowDownPlayerSlot GetPlayerSlotFromSeatIndex(int32 SeatIndex)
	{
		switch (SeatIndex)
		{
		case 0: return EShowDownPlayerSlot::Player1;
		case 1: return EShowDownPlayerSlot::Player2;
		case 2: return EShowDownPlayerSlot::Player3;
		case 3: return EShowDownPlayerSlot::Player4;
		default: return EShowDownPlayerSlot::None;
		}
	}
}

class SSDCenterCrosshairWidget : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SSDCenterCrosshairWidget)
		: _CrosshairSize(18.0f)
		, _CrosshairThickness(2.0f)
		, _CrosshairColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.9f))
	{
	}
		SLATE_ARGUMENT(float, CrosshairSize)
		SLATE_ARGUMENT(float, CrosshairThickness)
		SLATE_ARGUMENT(FLinearColor, CrosshairColor)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		CrosshairSize = InArgs._CrosshairSize;
		CrosshairThickness = InArgs._CrosshairThickness;
		CrosshairColor = InArgs._CrosshairColor;
	}

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
	{
		return FVector2D::ZeroVector;
	}

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override
	{
		const FVector2D Center = AllottedGeometry.GetLocalSize() * 0.5f;
		const float HalfSize = CrosshairSize * 0.5f;
		const float HalfThickness = CrosshairThickness * 0.5f;
		const FSlateColor SlateColor(CrosshairColor);

		const FPaintGeometry HorizontalGeometry = AllottedGeometry.ToPaintGeometry(
			FVector2f(CrosshairSize, CrosshairThickness),
			FSlateLayoutTransform(FVector2f(Center.X - HalfSize, Center.Y - HalfThickness)));

		const FPaintGeometry VerticalGeometry = AllottedGeometry.ToPaintGeometry(
			FVector2f(CrosshairThickness, CrosshairSize),
			FSlateLayoutTransform(FVector2f(Center.X - HalfThickness, Center.Y - HalfSize)));

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			HorizontalGeometry,
			FCoreStyle::Get().GetBrush("WhiteBrush"),
			ESlateDrawEffect::None,
			SlateColor.GetColor(InWidgetStyle));

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			VerticalGeometry,
			FCoreStyle::Get().GetBrush("WhiteBrush"),
			ESlateDrawEffect::None,
			SlateColor.GetColor(InWidgetStyle));

		return LayerId + 1;
	}

private:
	float CrosshairSize = 18.0f;
	float CrosshairThickness = 2.0f;
	FLinearColor CrosshairColor = FLinearColor::White;
};

/**
 * PlayerCameraManager fades are rendered below viewport UI, so screen-space
 * widget components and Slate HUD content would otherwise remain visible over
 * a full camera blackout. This leaf fills the viewport at the highest Z-order.
 */
class SSDHitBlackoutOverlay : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SSDHitBlackoutOverlay)
	{
	}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		SetVisibility(EVisibility::HitTestInvisible);
	}

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
	{
		return FVector2D::ZeroVector;
	}

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			FCoreStyle::Get().GetBrush("WhiteBrush"),
			ESlateDrawEffect::None,
			FLinearColor::Black);
		return LayerId + 1;
	}
};

namespace
{
	bool IsMultiplayerGameMap(const UWorld* World)
	{
		return World && World->GetNetMode() != NM_Standalone;
	}

	FString NormalizeSubmittedCharacterSkinId(const FString& SkinId)
	{
		FShowDownCharacterSkinDefinition Definition;
		FString ResolvedSkinId;
		UShowDownCharacterSkinCatalog::ResolveSkinDefinition(
			nullptr,
			SkinId,
			Definition,
			ResolvedSkinId);
		return ResolvedSkinId;
	}
}

AShowDownPlayerController::AShowDownPlayerController()
{
	PrimaryActorTick.bTickEvenWhenPaused = true;
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;

	// Keep multiplayer chat functional even when the level pawn Blueprint has
	// not supplied an override. This also makes the widget available early so it
	// can subscribe to replicated GameState chat broadcasts before the user
	// presses the chat key.
	ChatWidgetClass = UShowDownChatWidget::StaticClass();

	static ConstructorHelpers::FClassFinder<UShowDownMultiRankWidget> MultiRankWidgetBlueprint(
		TEXT("/Game/UI/WBP_MultiResult"));
	if (MultiRankWidgetBlueprint.Succeeded())
	{
		MultiplayerRankWidgetClass = MultiRankWidgetBlueprint.Class;
	}
	else
	{
		MultiplayerRankWidgetClass = UShowDownMultiRankWidget::StaticClass();
	}

	static ConstructorHelpers::FClassFinder<UShowDownPauseMenuWidget> PauseMenuWidgetBlueprint(
		TEXT("/Game/UI/WBP_PauseMenu"));
	if (PauseMenuWidgetBlueprint.Succeeded())
	{
		PauseMenuWidgetClass = PauseMenuWidgetBlueprint.Class;
	}
}

void AShowDownPlayerController::BeginPlay()
{
	Super::BeginPlay();
	GConfig->GetFloat(TEXT("ShowDown.UserSettings"), TEXT("MouseSensitivity"), UserMouseSensitivityMultiplier, GGameUserSettingsIni);
	UserMouseSensitivityMultiplier = FMath::Clamp(UserMouseSensitivityMultiplier, 0.2f, 2.0f);

	if (!CanCreateLocalPlayerWidgets())
	{
		return;
	}

	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
	InitializeFromPossessedPawn();
	InitializeInteractableOutlinePostProcess();
	CreateCenterCrosshairWidget();
	UpdateCenterCrosshairVisibility();
	TryBindVoiceChatEvents();
	SubmitLocalEquippedCharacterSkin();
}

void AShowDownPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelGunShotCameraOverride();
	SetHitBlackoutUiOpacity(0.0f);
	DisableGameplayChat();

	if (AShowDownGameStateBase* PreviousGameState = VoiceBoundGameState.Get())
	{
		PreviousGameState->OnChatMessageReceived.RemoveDynamic(this, &AShowDownPlayerController::HandleChatMessageReceived);
	}
	if (UShowDownVoiceSubsystem* VoiceSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UShowDownVoiceSubsystem>()
		: nullptr)
	{
		VoiceSubsystem->OnVoiceStatus.RemoveDynamic(this, &AShowDownPlayerController::HandleVoiceStatus);
		VoiceSubsystem->OnSpeechPlaybackStateChanged.RemoveDynamic(
			this,
			&AShowDownPlayerController::HandleSpeechPlaybackStateChanged);
	}
	if (UShowDownEosSubsystem* EosSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UShowDownEosSubsystem>()
		: nullptr)
	{
		EosSubsystem->EndVoiceTransmission();
		EosSubsystem->OnLocalVoiceTalkingChanged.RemoveDynamic(
			this,
			&AShowDownPlayerController::HandleLocalVoiceTalkingChanged);
	}
	VoiceBoundGameState.Reset();
	bVoiceChatEventsBound = false;
	bVoiceSubsystemEventsBound = false;
	bEosVoiceEventsBound = false;

	SetFocusedInteractable(nullptr);
	if (InteractionOutlinePostProcessVolume)
	{
		InteractionOutlinePostProcessVolume->Destroy();
		InteractionOutlinePostProcessVolume = nullptr;
	}
	SetHoveredCard(nullptr);
	RemoveCenterCrosshairWidget();
	if (MultiplayerLoadingWidget)
	{
		MultiplayerLoadingWidget->RemoveFromParent();
		MultiplayerLoadingWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void AShowDownPlayerController::OnPossess(APawn* InPawn)
{
	CancelGunShotCameraOverride();
	Super::OnPossess(InPawn);
	InitializeFromPossessedPawn();
	SubmitLocalMultiplayerDisplayName();
	SubmitLocalEquippedCharacterSkin();
}

void AShowDownPlayerController::ClientEnterMultiplayerGameplay_Implementation()
{
	if (!CanCreateLocalPlayerWidgets())
	{
		return;
	}

	bGameplayChatEnabled = true;
	SubmitLocalEquippedCharacterSkin();
	if (UShowDownEosSubsystem* EosSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UShowDownEosSubsystem>()
		: nullptr)
	{
		EosSubsystem->MarkEnteredMultiplayerGame();
	}

	RemoveCenterCrosshairWidget();
	if (AShowDownHubFlowManager* HubFlowManager = Cast<AShowDownHubFlowManager>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AShowDownHubFlowManager::StaticClass())))
	{
		HubFlowManager->PrepareForMultiplayerGameplay();
	}
	if (ChatWidget)
	{
		ChatWidget->RemoveFromParent();
		ChatWidget = nullptr;
	}
	if (LeaveConfirmWidget)
	{
		LeaveConfirmWidget->RemoveFromParent();
		LeaveConfirmWidget = nullptr;
	}
	if (MultiplayerRankWidget)
	{
		MultiplayerRankWidget->RemoveFromParent();
		MultiplayerRankWidget = nullptr;
	}
	if (MultiplayerLoadingWidget)
	{
		MultiplayerLoadingWidget->RemoveFromParent();
		MultiplayerLoadingWidget = nullptr;
	}
	if (PauseSettingsWidget)
	{
		PauseSettingsWidget->RemoveFromParent();
		PauseSettingsWidget = nullptr;
	}
	if (PauseMenuWidget)
	{
		PauseMenuWidget->RemoveFromParent();
		PauseMenuWidget = nullptr;
	}
	bPauseMenuOpen = false;
	RestoreMultiplayerNameTagScreenRegistrations(GetWorld());
	MultiplayerLoadingWidget = CreateWidget<UShowDownTransitionWidget>(this, UShowDownTransitionWidget::StaticClass());
	if (MultiplayerLoadingWidget)
	{
		MultiplayerLoadingWidget->SetTransitionText(
			TEXT("플레이 준비 중"),
			TEXT("좌석과 카메라를 안전하게 연결하고 있습니다."));
		MultiplayerLoadingWidget->AddToViewport(10000);
	}
	MultiplayerLoadingElapsedTime = 0.0f;
	bMultiplayerLoadingDelayMessageShown = false;
	bChatOpen = false;

	bHandleShowDownGameplayInput = false;
	// Multiplayer follows the same first-person interaction model as single
	// player: raw mouse movement controls the view and the centre reticle is
	// used for card/interactable tracing. UI temporarily reveals the cursor.
	bShowCenterCrosshair = false;
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;

	EnsureChatWidget();
	PawnCameraBaseRotation = GetControlRotation();
	bHasPawnCameraBaseRotation = true;
	UpdateCenterCrosshairVisibility();
	ClientShowStatusMessage(TEXT("로딩 중... 멀티플레이 좌석과 카메라를 확인하는 중입니다."));
}

void AShowDownPlayerController::ClientSetInitialCardDealInputLocked_Implementation(bool bLocked)
{
	if (!CanCreateLocalPlayerWidgets())
	{
		return;
	}

	if (!bGameplayChatEnabled)
	{
		bGameplayChatEnabled = true;
		EnsureChatWidget();
	}
	bInitialCardDealInputLocked = bLocked;
	if (bLocked)
	{
		bHandleShowDownGameplayInput = false;
		bShowCenterCrosshair = false;
		CancelPressedBetActionButton();
		SetFocusedInteractable(nullptr);
		SetHoveredCard(nullptr);
		if (!bInitialCardDealIgnoreMoveInputApplied)
		{
			SetIgnoreMoveInput(true);
			bInitialCardDealIgnoreMoveInputApplied = true;
		}
		UpdateCenterCrosshairVisibility();

		const bool bMultiplayerCameraReady = GetNetMode() != NM_Standalone
			&& !bPendingMultiplayerSeatCamera
			&& bUseCharacterPlayerCamera
			&& IsValid(LocalPlayerCameraCharacterTarget)
			&& GetViewTarget() == GetPawn();
		if (bMultiplayerCameraReady)
		{
			ServerNotifyInitialCardDealCameraReady();
		}
		return;
	}

	if (bInitialCardDealIgnoreMoveInputApplied)
	{
		SetIgnoreMoveInput(false);
		bInitialCardDealIgnoreMoveInputApplied = false;
	}
	if (GetNetMode() == NM_Standalone || !bPendingMultiplayerSeatCamera)
	{
		bHandleShowDownGameplayInput = true;
		bShowCenterCrosshair = true;
		if (GetNetMode() != NM_Standalone)
		{
			RestoreMultiplayerGameplayInput();
		}
	}
	UpdateCenterCrosshairVisibility();
}

void AShowDownPlayerController::ClientUseMultiplayerSeatCamera_Implementation(
	int32 SeatIndex,
	float SeatCameraLookSensitivity,
	float MinPitchDegrees,
	float MaxPitchDegrees,
	float MinYawOffsetDegrees,
	float MaxYawOffsetDegrees,
	bool bInvertY,
	bool bEnableBreathingSway,
	float InBreathingSwaySpeed,
	FRotator InBreathingSwayRotationAmplitude,
	FVector InBreathingSwayLocationAmplitude,
	float InBreathingSwayBlendInTime)
{
	CancelGunShotCameraOverride();

	const ASDPlayerState* ShowDownPlayerState = GetPlayerState<ASDPlayerState>();
	UE_LOG(
		LogTemp,
		Log,
		TEXT("멀티플레이 좌석 카메라 요청 수신: SeatIndex=%d LocalSlot=%d Player=%s"),
		SeatIndex,
		ShowDownPlayerState ? static_cast<int32>(ShowDownPlayerState->ShowDownSlot) : -1,
		ShowDownPlayerState ? *ShowDownPlayerState->GetPlayerName() : TEXT("None"));

	// A client can receive this RPC while its seamless level transition is still
	// loading. Keep the seat number and retry from PlayerTick once the matching
	// replicated ShowDownCharacter is available locally.
	PendingMultiplayerSeatIndex = SeatIndex;
	PendingMultiplayerSeatCameraLookSensitivity = SeatCameraLookSensitivity;
	PendingMultiplayerCameraMinPitch = MinPitchDegrees;
	PendingMultiplayerCameraMaxPitch = MaxPitchDegrees;
	PendingMultiplayerCameraMinYawOffset = MinYawOffsetDegrees;
	PendingMultiplayerCameraMaxYawOffset = MaxYawOffsetDegrees;
	bPendingMultiplayerCameraInvertMouseY = bInvertY;
	bPendingMultiplayerCameraBreathingSway = bEnableBreathingSway;
	PendingMultiplayerCameraBreathingSwaySpeed = InBreathingSwaySpeed;
	PendingMultiplayerCameraBreathingSwayRotationAmplitude = InBreathingSwayRotationAmplitude;
	PendingMultiplayerCameraBreathingSwayLocationAmplitude = InBreathingSwayLocationAmplitude;
	PendingMultiplayerCameraBreathingSwayBlendInTime = InBreathingSwayBlendInTime;
	ClearFixedCameraMouseLook();
	bUseCharacterPlayerCamera = true;
	SetPawnCameraMouseLook(
		SeatCameraLookSensitivity,
		MinPitchDegrees,
		MaxPitchDegrees,
		MinYawOffsetDegrees,
		MaxYawOffsetDegrees,
		bInvertY);
	bEnablePawnCameraMouseLook = true;
	bRequireRightMouseForPawnCameraLook = false;
	LocalPlayerCameraCharacterTarget = nullptr;
	bPendingMultiplayerSeatCamera = true;
	bHandleShowDownGameplayInput = false;
	bShowCenterCrosshair = false;
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;

	if (TryApplyPendingMultiplayerSeatCamera())
	{
		return;
	}

	// Until the game map finishes loading, retain a safe local pawn view.
	ClearFixedCameraMouseLook();
	if (APawn* ControlledPawn = GetPawn())
	{
		if (APlayerPawn* PlayerPawn = Cast<APlayerPawn>(ControlledPawn))
		{
			PlayerPawn->ApplyDefaultCameraAspect();
		}
		SetViewTarget(ControlledPawn);
	}
	UpdateCenterCrosshairVisibility();
	ClientShowStatusMessage(TEXT("로딩 중... 멀티플레이 좌석 카메라를 기다리는 중입니다."));
}

void AShowDownPlayerController::ClientLeaveMultiplayerRoomToHub_Implementation()
{
	if (UShowDownEosSubsystem* EosSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UShowDownEosSubsystem>()
		: nullptr)
	{
		EosSubsystem->LeaveLobby(FName(TEXT("L_ShowdownMain")));
		return;
	}

	ShowDownLoadingScreen::Prepare(
		TEXT("메인 화면으로 돌아가는 중"),
		TEXT("네트워크 연결을 정리하고 있습니다."));
	ClientTravel(TEXT("/Game/Maps/L_ShowdownMain"), TRAVEL_Absolute);
}

bool AShowDownPlayerController::TryApplyPendingMultiplayerSeatCamera()
{
	if (!bPendingMultiplayerSeatCamera || PendingMultiplayerSeatIndex == INDEX_NONE || !GetWorld())
	{
		return false;
	}

	return TryApplyPendingMultiplayerCharacterCamera();
}

bool AShowDownPlayerController::TryApplyPendingMultiplayerCharacterCamera()
{
	if (!bPendingMultiplayerSeatCamera || PendingMultiplayerSeatIndex == INDEX_NONE || !IsLocalController() || !GetWorld())
	{
		return false;
	}

	if (!IsMultiplayerGameMap(GetWorld()))
	{
		return false;
	}

	APlayerPawn* PlayerPawn = Cast<APlayerPawn>(GetPawn());
	UCameraComponent* PlayerCamera = PlayerPawn ? PlayerPawn->cameraComp : nullptr;
	if (!PlayerPawn || !PlayerCamera)
	{
		return false;
	}

	if (!IsValid(LocalPlayerCameraCharacterTarget))
	{
		LocalPlayerCameraCharacterTarget = FindLocalCharacterForPlayerCamera();
	}

	if (!IsValid(LocalPlayerCameraCharacterTarget) || !LocalPlayerCameraCharacterTarget->GetMesh())
	{
		UE_LOG(LogTemp, Verbose, TEXT("Waiting for local multiplayer character camera target. SeatIndex=%d"), PendingMultiplayerSeatIndex);
		return false;
	}

	ClearFixedCameraMouseLook();
	bUseCharacterPlayerCamera = true;
	bEnablePawnCameraMouseLook = true;
	bRequireRightMouseForPawnCameraLook = false;

	const FRotator InitialViewRotation(-12.0f, LocalPlayerCameraCharacterTarget->GetActorRotation().Yaw, 0.0f);
	SetControlRotation(InitialViewRotation);
	PawnCameraBaseRotation = InitialViewRotation;
	bHasPawnCameraBaseRotation = true;

	UpdateCharacterPlayerCamera(0.0f);

	const FName AttachName = LocalPlayerCameraCharacterTarget->ResolvePlayerCameraAttachName();
	const bool bAttachedToCharacter =
		PlayerCamera->GetAttachParent() == LocalPlayerCameraCharacterTarget->GetMesh()
		&& PlayerCamera->GetAttachSocketName() == AttachName
		&& GetViewTarget() == PlayerPawn;
	if (!bAttachedToCharacter)
	{
		return false;
	}

	EnsureChatWidget();
	bPendingMultiplayerSeatCamera = false;
	bHandleShowDownGameplayInput = !bInitialCardDealInputLocked;
	bShowCenterCrosshair = !bInitialCardDealInputLocked;
	if (bInitialCardDealInputLocked)
	{
		ServerNotifyInitialCardDealCameraReady();
	}
	else
	{
		RestoreMultiplayerGameplayInput();
	}
	CreateCenterCrosshairWidget();
	UpdateCenterCrosshairVisibility();
	RestoreMultiplayerNameTagScreenRegistrations(GetWorld());
	if (MultiplayerLoadingWidget)
	{
		MultiplayerLoadingWidget->Dismiss(0.22f);
		MultiplayerLoadingWidget = nullptr;
	}
	ClientShowStatusMessage(TEXT("Multiplayer character head camera ready."));
	UE_LOG(
		LogTemp,
		Log,
		TEXT("Applied multiplayer character head camera. SeatIndex=%d Slot=%d Character=%s"),
		PendingMultiplayerSeatIndex,
		static_cast<int32>(LocalPlayerCameraCharacterTarget->GetPlayerSlot()),
		*LocalPlayerCameraCharacterTarget->GetName());
	return true;
}

void AShowDownPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	UpdateGunShotCameraOverride(DeltaTime);
	if (MultiplayerLoadingWidget)
	{
		MultiplayerLoadingElapsedTime += FMath::Max(0.0f, DeltaTime);
		if (!bMultiplayerLoadingDelayMessageShown && MultiplayerLoadingElapsedTime >= 8.0f)
		{
			bMultiplayerLoadingDelayMessageShown = true;
			MultiplayerLoadingWidget->SetTransitionText(
				TEXT("플레이 준비가 지연되는 중"),
				TEXT("좌석과 캐릭터 동기화를 다시 확인하고 있습니다."));
		}
	}

	if (bPendingMultiplayerSeatCamera)
	{
		TryApplyPendingMultiplayerSeatCamera();
	}
	if (!bVoiceChatEventsBound || !bVoiceSubsystemEventsBound || !VoiceBoundGameState.IsValid())
	{
		TryBindVoiceChatEvents();
	}
	if (IsMultiplayerGameMap(GetWorld()))
	{
		SubmitLocalMultiplayerDisplayName();
		SubmitLocalEquippedCharacterSkin();
	}

	const bool bHasFixedCameraLook = FixedCameraMouseLookTarget != nullptr;
	if (bHasFixedCameraLook)
	{
		UpdateFixedCameraMouseLook(DeltaTime);
	}
	else if (bInitialCardDealInputLocked
		&& GetPawn()
		&& bEnablePawnCameraMouseLook
		&& (!bRequireRightMouseForPawnCameraLook || IsInputKeyDown(EKeys::RightMouseButton)))
	{
		float MouseDeltaX = 0.0f;
		float MouseDeltaY = 0.0f;
		GetInputMouseDelta(MouseDeltaX, MouseDeltaY);
		if (!FMath::IsNearlyZero(MouseDeltaX) || !FMath::IsNearlyZero(MouseDeltaY))
		{
			ApplyPawnCameraInput(MouseDeltaX, MouseDeltaY);
		}
	}
	if (bInitialCardDealInputLocked)
	{
		UpdateCharacterPlayerCamera(DeltaTime);
	}

	if (WasInputKeyJustPressed(LeaveMatchKey)
		&& (bHandleShowDownGameplayInput || bPauseMenuOpen)
		&& !bChatOpen
		&& (!LeaveConfirmWidget || LeaveConfirmWidget->GetVisibility() != ESlateVisibility::Visible))
	{
		TogglePauseMenu();
		return;
	}

	if (!bHandleShowDownGameplayInput)
	{
		CancelPressedBetActionButton();
		SetFocusedInteractable(nullptr);
		SetHoveredCard(nullptr);
		UpdateCenterCrosshairVisibility();
		return;
	}

	if (LeaveConfirmWidget && LeaveConfirmWidget->GetVisibility() == ESlateVisibility::Visible)
	{
		CancelPressedBetActionButton();
		if (WasInputKeyJustPressed(LeaveMatchKey))
		{
			CancelLeaveMultiplayerMatch();
		}
		SetHoveredCard(nullptr);
		return;
	}

	if (bChatOpen)
	{
		CancelPressedBetActionButton();
		SetFocusedInteractable(nullptr);
		SetHoveredCard(nullptr);
		UpdateCenterCrosshairVisibility();
		if (WasInputKeyJustPressed(CloseChatKey))
		{
			CloseChat();
			return;
		}
		if (WasInputKeyJustPressed(ToggleChatKey) && ChatWidget && !ChatWidget->IsChatInputFocused())
		{
			ChatWidget->SetVisibility(ESlateVisibility::Visible);
			ChatWidget->SetChatInputOpen(true);
			ApplyChatInputMode(true);
			ChatWidget->FocusChatInput();
		}
		return;
	}

	if (WasInputKeyJustPressed(ToggleChatKey))
	{
		CancelPressedBetActionButton();
		OpenChat();
		return;
	}

	HandleVoicePushToTalkInput();

	if (bEnablePrimaryClickTrace)
	{
		if (WasInputKeyJustPressed(EKeys::LeftMouseButton))
		{
			HandlePrimaryPress();
		}
		if (PressedBetActionButton.Get())
		{
			UpdatePressedBetActionButton();
		}
		if (WasInputKeyJustReleased(EKeys::LeftMouseButton))
		{
			HandlePrimaryRelease();
		}
	}
	else
	{
		CancelPressedBetActionButton();
	}

	if (bEnableLegacyKeyboardBetHotkeys)
	{
		HandleBettingHotkeys();
	}

	if (!bHasFixedCameraLook
		&& GetPawn()
		&& bEnablePawnCameraMouseLook
		&& (!bRequireRightMouseForPawnCameraLook || IsInputKeyDown(EKeys::RightMouseButton)))
	{
		float MouseDeltaX = 0.0f;
		float MouseDeltaY = 0.0f;
		GetInputMouseDelta(MouseDeltaX, MouseDeltaY);
		if (!FMath::IsNearlyZero(MouseDeltaX) || !FMath::IsNearlyZero(MouseDeltaY))
		{
			ApplyPawnCameraInput(MouseDeltaX, MouseDeltaY);
		}
	}

	UpdateCharacterPlayerCamera(DeltaTime);
	PrimaryInteractionTraceFrame = MAX_uint64;
	UpdateFocusedInteractable();
	UpdateHoveredCard();
	UpdateCenterCrosshairVisibility();
}

void AShowDownPlayerController::InitializeFromPossessedPawn()
{
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;

	APlayerPawn* ShowDownPawn = Cast<APlayerPawn>(GetPawn());
	if (!ShowDownPawn)
	{
		return;
	}
	ShowDownPawn->ReleaseChatWidget();

	if (!ChatWidgetClass)
	{
		ChatWidgetClass = ShowDownPawn->ChatWidgetClass;
	}

	LookSensitivity = ShowDownPawn->LookSensitivity;
	MinPitch = ShowDownPawn->MinPitch;
	MaxPitch = ShowDownPawn->MaxPitch;
	MinYaw = ShowDownPawn->MinYaw;
	MaxYaw = ShowDownPawn->MaxYaw;
	bEnableLegacyKeyboardBetHotkeys = ShowDownPawn->bEnableLegacyKeyboardBetHotkeys;
	ToggleChatKey = ShowDownPawn->ToggleChatKey;
	CloseChatKey = ShowDownPawn->CloseChatKey;
	bEnableVoicePushToTalk = ShowDownPawn->bEnableVoicePushToTalk;
	VoicePushToTalkKey = ShowDownPawn->VoicePushToTalkKey;
	if (ToggleChatKey == VoicePushToTalkKey)
	{
		ToggleChatKey = EKeys::Enter;
	}

	// Every multiplayer pawn starts facing the shared table. Keep camera limits
	// relative to that seat direction instead of clamping to world-space yaw.
	PawnCameraBaseRotation = GetControlRotation();
	bHasPawnCameraBaseRotation = true;
}

void AShowDownPlayerController::InitializeInteractableOutlinePostProcess()
{
	if (!IsLocalController())
	{
		if (InteractionOutlinePostProcessVolume)
		{
			InteractionOutlinePostProcessVolume->BlendWeight = 0.0f;
		}
		return;
	}

	if (!InteractionOutlineMaterial)
	{
		InteractionOutlineMaterial = LoadObject<UMaterialInterface>(nullptr, DefaultInteractionOutlineMaterialPath);
	}

	if (!InteractionOutlineMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("Interaction outline material is missing: %s"), DefaultInteractionOutlineMaterialPath);
		return;
	}

	if (!InteractionOutlineMID)
	{
		InteractionOutlineMID = UMaterialInstanceDynamic::Create(InteractionOutlineMaterial, this);
	}

	if (!InteractionOutlineMID)
	{
		return;
	}

	if (!InteractionOutlinePostProcessVolume)
	{
		UWorld* World = GetWorld();
		if (!World)
		{
			return;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.ObjectFlags |= RF_Transient;
		InteractionOutlinePostProcessVolume = World->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FTransform::Identity, SpawnParams);
	}

	if (!InteractionOutlinePostProcessVolume)
	{
		return;
	}

	InteractionOutlinePostProcessVolume->bUnbound = true;
	InteractionOutlinePostProcessVolume->Priority = 1000.0f;
	InteractionOutlinePostProcessVolume->BlendWeight = 1.0f;

	RefreshInteractableOutlineMaterialParameters();

	FWeightedBlendables& WeightedBlendables = InteractionOutlinePostProcessVolume->Settings.WeightedBlendables;
	for (int32 BlendableIndex = WeightedBlendables.Array.Num() - 1; BlendableIndex >= 0; --BlendableIndex)
	{
		if (WeightedBlendables.Array[BlendableIndex].Object.Get() == InteractionOutlineMID)
		{
			WeightedBlendables.Array.RemoveAt(BlendableIndex);
		}
	}

	WeightedBlendables.Array.Add(FWeightedBlendable(1.0f, InteractionOutlineMID));
}

void AShowDownPlayerController::HandlePrimaryClick()
{
	FHitResult Hit;
	const bool bHasHit = TracePrimaryInteraction(Hit);

	if (bHasHit)
	{
		AActor* HitActor = Hit.GetActor();
		if (HitActor && bEnableInteractableTrace && HitActor->GetClass()->ImplementsInterface(USDInteractable::StaticClass()))
		{
			if (ISDInteractable::Execute_CanInteract(HitActor, this))
			{
				ISDInteractable::Execute_Interact(HitActor, this);
				return;
			}
		}

		HandCard = ResolveCardFromHit(Hit);
		if (IsCardSelectableForLocalPlayer(HandCard))
		{
			SelectCard(HandCard);
			if (bSubmitCardsOnSingleClick)
			{
				SubmitSelectedCard(HandCard);
			}
			return;
		}
	}

	HandCard = bUseCenterAimCardFallback ? FindSelectableCardNearCenterAim() : nullptr;
	if (IsCardSelectableForLocalPlayer(HandCard))
	{
		SelectCard(HandCard);
		if (bSubmitCardsOnSingleClick)
		{
			SubmitSelectedCard(HandCard);
		}
		return;
	}

	HandCard = nullptr;
}

void AShowDownPlayerController::HandlePrimaryPress()
{
	CancelPressedBetActionButton();

	FHitResult Hit;
	if (bEnableInteractableTrace && TracePrimaryInteraction(Hit))
	{
		if (ASDBetActionButtonActor* Button = ResolveBetActionButtonFromHit(Hit))
		{
			if (ISDInteractable::Execute_CanInteract(Button, this))
			{
				PressedBetActionButton = Button;
				Button->BeginPointerPress();
				return;
			}
		}
	}

	HandlePrimaryClick();
}

void AShowDownPlayerController::HandlePrimaryRelease()
{
	ASDBetActionButtonActor* Button = PressedBetActionButton.Get();
	if (!IsValid(Button))
	{
		PressedBetActionButton = nullptr;
		return;
	}

	PressedBetActionButton = nullptr;

	FHitResult Hit;
	const bool bCommit =
		bEnableInteractableTrace
		&& TracePrimaryInteraction(Hit)
		&& ResolveBetActionButtonFromHit(Hit) == Button
		&& ISDInteractable::Execute_CanInteract(Button, this);

	Button->ReleasePointerPress(this, bCommit);
}

void AShowDownPlayerController::UpdatePressedBetActionButton()
{
	ASDBetActionButtonActor* Button = PressedBetActionButton.Get();
	if (!IsValid(Button))
	{
		PressedBetActionButton = nullptr;
		return;
	}

	if (!IsInputKeyDown(EKeys::LeftMouseButton))
	{
		HandlePrimaryRelease();
		return;
	}

	FHitResult Hit;
	const bool bStillHovering =
		bEnableInteractableTrace
		&& TracePrimaryInteraction(Hit)
		&& ResolveBetActionButtonFromHit(Hit) == Button
		&& ISDInteractable::Execute_CanInteract(Button, this);

	if (!bStillHovering)
	{
		CancelPressedBetActionButton();
	}
}

void AShowDownPlayerController::CancelPressedBetActionButton()
{
	ASDBetActionButtonActor* Button = PressedBetActionButton.Get();
	if (!IsValid(Button))
	{
		PressedBetActionButton = nullptr;
		return;
	}

	Button->CancelPointerPress();
	PressedBetActionButton = nullptr;
}

void AShowDownPlayerController::TraceCardUnderCursor()
{
	HandCard = nullptr;

	FHitResult Hit;
	if (!TracePrimaryInteraction(Hit))
	{
		return;
	}

	HandCard = ResolveCardFromHit(Hit);
	if (!IsCardSelectableForLocalPlayer(HandCard))
	{
		HandCard = nullptr;
	}
}

bool AShowDownPlayerController::TracePrimaryInteraction(FHitResult& OutHit) const
{
	if (PrimaryInteractionTraceFrame == GFrameCounter)
	{
		OutHit = CachedPrimaryInteractionTraceHit;
		return bCachedPrimaryInteractionTraceHit;
	}

	FHitResult TraceHit;
	bool bHasHit = false;
	if (bShowMouseCursor)
	{
		bHasHit = TraceUnderCursor(TraceHit);
	}

	if (!bHasHit && bHandleShowDownGameplayInput && !bChatOpen)
	{
		bHasHit = bUseCenterScreenTraceWhenCursorHidden && TraceFromScreenCenter(TraceHit);
	}
	else if (!bHasHit)
	{
		if (!bShowMouseCursor)
		{
			bHasHit = TraceUnderCursor(TraceHit);
		}
		if (!bHasHit && bUseCenterScreenTraceWhenCursorHidden)
		{
			bHasHit = TraceFromScreenCenter(TraceHit);
		}
	}

	PrimaryInteractionTraceFrame = GFrameCounter;
	bCachedPrimaryInteractionTraceHit = bHasHit;
	CachedPrimaryInteractionTraceHit = bHasHit ? TraceHit : FHitResult();
	OutHit = CachedPrimaryInteractionTraceHit;

	return bHasHit;
}

bool AShowDownPlayerController::TraceUnderCursor(FHitResult& OutHit) const
{
	return GetHitResultUnderCursor(ECC_Visibility, false, OutHit);
}

bool AShowDownPlayerController::TraceFromScreenCenter(FHitResult& OutHit) const
{
	int32 ViewportSizeX = 0;
	int32 ViewportSizeY = 0;
	GetViewportSize(ViewportSizeX, ViewportSizeY);
	if (ViewportSizeX <= 0 || ViewportSizeY <= 0)
	{
		return false;
	}

	FVector WorldLocation = FVector::ZeroVector;
	FVector WorldDirection = FVector::ForwardVector;
	if (!DeprojectScreenPositionToWorld(
		static_cast<float>(ViewportSizeX) * 0.5f,
		static_cast<float>(ViewportSizeY) * 0.5f,
		WorldLocation,
		WorldDirection))
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ShowDownPrimaryInteraction), true);
	if (APawn* ControlledPawn = GetPawn())
	{
		QueryParams.AddIgnoredActor(ControlledPawn);
	}
	if (IsValid(LocalPlayerCameraCharacterTarget))
	{
		QueryParams.AddIgnoredActor(LocalPlayerCameraCharacterTarget);
	}

	const FVector TraceEnd = WorldLocation + WorldDirection * CenterScreenTraceDistance;
	return World->LineTraceSingleByChannel(OutHit, WorldLocation, TraceEnd, ECC_Visibility, QueryParams);
}

ACard* AShowDownPlayerController::ResolveCardFromHit(const FHitResult& Hit) const
{
	if (ACard* HitCard = Cast<ACard>(Hit.GetActor()))
	{
		return HitCard;
	}

	if (const UPrimitiveComponent* HitComponent = Hit.GetComponent())
	{
		if (ACard* OwnerCard = Cast<ACard>(HitComponent->GetOwner()))
		{
			return OwnerCard;
		}
	}

	return nullptr;
}

bool AShowDownPlayerController::IsCardSelectableForLocalPlayer(const ACard* Card) const
{
	const ASDPlayerState* ShowDownPlayerState = GetPlayerState<ASDPlayerState>();
	const EShowDownPlayerSlot LocalSlot = ShowDownPlayerState
		? ShowDownPlayerState->ShowDownSlot
		: EShowDownPlayerSlot::None;

	return IsValid(Card) && Card->IsCardSelectableForSlot(LocalSlot);
}

ASDBetActionButtonActor* AShowDownPlayerController::ResolveBetActionButtonFromHit(const FHitResult& Hit) const
{
	if (ASDBetActionButtonActor* Button = Cast<ASDBetActionButtonActor>(Hit.GetActor()))
	{
		return Button;
	}

	if (const UPrimitiveComponent* HitComponent = Hit.GetComponent())
	{
		return Cast<ASDBetActionButtonActor>(HitComponent->GetOwner());
	}

	return nullptr;
}

AActor* AShowDownPlayerController::ResolveInteractableFromHit(const FHitResult& Hit) const
{
	AActor* HitActor = Hit.GetActor();
	if (!HitActor && Hit.GetComponent())
	{
		HitActor = Hit.GetComponent()->GetOwner();
	}

	if (!HitActor || !bEnableInteractableTrace || !HitActor->GetClass()->ImplementsInterface(USDInteractable::StaticClass()))
	{
		return nullptr;
	}

	return ISDInteractable::Execute_CanInteract(HitActor, const_cast<AShowDownPlayerController*>(this))
		? HitActor
		: nullptr;
}

AActor* AShowDownPlayerController::FindFocusedInteractable() const
{
	if (!bEnableInteractableAimOutline || !bHandleShowDownGameplayInput || bChatOpen)
	{
		return nullptr;
	}

	FHitResult Hit;
	return TracePrimaryInteraction(Hit) ? ResolveInteractableFromHit(Hit) : nullptr;
}

void AShowDownPlayerController::UpdateFocusedInteractable()
{
	SetFocusedInteractable(FindFocusedInteractable());
}

void AShowDownPlayerController::SetFocusedInteractable(AActor* NewFocusedInteractable)
{
	if (!IsValid(NewFocusedInteractable))
	{
		NewFocusedInteractable = nullptr;
	}

	if (FocusedInteractable == NewFocusedInteractable)
	{
		if (FocusedInteractable && InteractionOutlineMID)
		{
			RefreshInteractableOutlineMaterialParameters();
		}
		return;
	}

	ClearInteractableOutline();
	FocusedInteractable = NewFocusedInteractable;

	if (FocusedInteractable)
	{
		ApplyInteractableOutline(FocusedInteractable);
	}
}

void AShowDownPlayerController::ApplyInteractableOutline(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	InitializeInteractableOutlinePostProcess();
	RefreshInteractableOutlineMaterialParameters();

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	for (UPrimitiveComponent* Component : PrimitiveComponents)
	{
		if (!IsOutlineablePrimitive(Component))
		{
			continue;
		}

		FSDPrimitiveCustomDepthState& SavedState = FocusedPrimitiveStates.AddDefaulted_GetRef();
		SavedState.Component = Component;
		SavedState.bRenderCustomDepth = Component->bRenderCustomDepth;
		SavedState.CustomDepthStencilValue = Component->CustomDepthStencilValue;

		Component->SetRenderCustomDepth(true);
		Component->SetCustomDepthStencilValue(InteractableOutlineStencilValue);
	}
}

void AShowDownPlayerController::ClearInteractableOutline()
{
	for (const FSDPrimitiveCustomDepthState& SavedState : FocusedPrimitiveStates)
	{
		if (UPrimitiveComponent* Component = SavedState.Component.Get())
		{
			Component->SetCustomDepthStencilValue(SavedState.CustomDepthStencilValue);
			Component->SetRenderCustomDepth(SavedState.bRenderCustomDepth);
		}
	}

	FocusedPrimitiveStates.Reset();
	FocusedInteractable = nullptr;
}

void AShowDownPlayerController::RefreshInteractableOutlineMaterialParameters()
{
	if (!InteractionOutlineMID)
	{
		return;
	}

	InteractionOutlineMID->SetVectorParameterValue(TEXT("OutlineColor"), InteractableOutlineColor);
	InteractionOutlineMID->SetScalarParameterValue(TEXT("OutlineThickness"), FMath::Max(1.0f, InteractableOutlineThickness));
	InteractionOutlineMID->SetScalarParameterValue(TEXT("OutlineOpacity"), FMath::Clamp(InteractableOutlineOpacity, 0.0f, 1.0f));
	InteractionOutlineMID->SetScalarParameterValue(TEXT("TargetStencil"), FMath::Clamp(static_cast<float>(InteractableOutlineStencilValue), 1.0f, 255.0f));
}

ACard* AShowDownPlayerController::FindSelectableCardNearCenterAim() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	int32 ViewportSizeX = 0;
	int32 ViewportSizeY = 0;
	GetViewportSize(ViewportSizeX, ViewportSizeY);
	if (ViewportSizeX <= 0 || ViewportSizeY <= 0)
	{
		return nullptr;
	}

	const FVector2D ScreenCenter(
		static_cast<float>(ViewportSizeX) * 0.5f,
		static_cast<float>(ViewportSizeY) * 0.5f);
	const float PickRadiusSq = FMath::Square(CenterAimCardPickRadiusPixels);

	ACard* BestCard = nullptr;
	float BestDistanceSq = PickRadiusSq;

	for (TActorIterator<ACard> It(World); It; ++It)
	{
		ACard* Card = *It;
		if (!IsCardSelectableForLocalPlayer(Card))
		{
			continue;
		}

		FVector2D CardScreenPosition = FVector2D::ZeroVector;
		if (!ProjectWorldLocationToScreen(Card->GetActorLocation(), CardScreenPosition, false))
		{
			continue;
		}

		const float DistanceSq = FVector2D::DistSquared(ScreenCenter, CardScreenPosition);
		if (DistanceSq <= BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			BestCard = Card;
		}
	}

	return BestCard;
}

ACard* AShowDownPlayerController::FindHoverPreviewCard() const
{
	if (!bEnableCardHoverPreview)
	{
		return nullptr;
	}

	FHitResult Hit;
	if (TracePrimaryInteraction(Hit))
	{
		ACard* HitCard = ResolveCardFromHit(Hit);
		if (IsCardSelectableForLocalPlayer(HitCard))
		{
			return HitCard;
		}
	}

	return bUseCenterAimCardFallback ? FindSelectableCardNearCenterAim() : nullptr;
}

void AShowDownPlayerController::UpdateHoveredCard()
{
	SetHoveredCard(FindHoverPreviewCard());
}

void AShowDownPlayerController::SetHoveredCard(ACard* NewHoveredCard)
{
	if (!IsCardSelectableForLocalPlayer(NewHoveredCard))
	{
		NewHoveredCard = nullptr;
	}

	if (HoveredCard == NewHoveredCard)
	{
		return;
	}

	if (IsValid(HoveredCard))
	{
		HoveredCard->SetHovered(false);
	}

	HoveredCard = NewHoveredCard;

	if (IsValid(HoveredCard))
	{
		HoveredCard->SetHovered(true);
	}
}

void AShowDownPlayerController::SelectCard(ACard* SelectedCard)
{
	if (!IsCardSelectableForLocalPlayer(SelectedCard))
	{
		return;
	}

	if (CurrentSelectedCard && !IsValid(CurrentSelectedCard))
	{
		CurrentSelectedCard = nullptr;
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

void AShowDownPlayerController::SubmitSelectedCard(ACard* SelectedCard)
{
	if (!IsCardSelectableForLocalPlayer(SelectedCard))
	{
		return;
	}

	if (CurrentSelectedCard == SelectedCard)
	{
		CurrentSelectedCard->SelectCard(false);
		CurrentSelectedCard = nullptr;
	}

	if (HandCard == SelectedCard)
	{
		HandCard = nullptr;
	}

	if (HoveredCard == SelectedCard)
	{
		if (IsValid(HoveredCard))
		{
			HoveredCard->SetHovered(false);
		}
		HoveredCard = nullptr;
	}

	if (HasAuthority())
	{
		if (AShowDownGameModeBase* GameMode = ResolveGameMode())
		{
			GameMode->PlayerSelectedCardFromController(this, SelectedCard);
		}
		return;
	}

	ServerSubmitSelectedCard(SelectedCard);
}

void AShowDownPlayerController::ToggleChat()
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

void AShowDownPlayerController::OpenChat()
{
	SetFocusedInteractable(nullptr);
	SetHoveredCard(nullptr);
	EnsureChatWidget();
	if (!ChatWidget)
	{
		return;
	}

	bChatOpen = true;
	ChatWidget->SetVisibility(ESlateVisibility::Visible);
	ChatWidget->SetChatInputOpen(true);
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

void AShowDownPlayerController::CloseChat()
{
	bChatOpen = false;
	if (ChatWidget)
	{
		ChatWidget->SetChatInputOpen(false);
		ChatWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	ApplyChatInputMode(false);
}

void AShowDownPlayerController::SubmitDialogueInput(const FString& Text)
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

	ServerSubmitDialogueInput(TrimmedText);
}

void AShowDownPlayerController::SDVoiceSubmitText(const FString& Text)
{
	SubmitDialogueInput(Text);
}

void AShowDownPlayerController::SDVoiceSpeak(const FString& Text)
{
	const FString TrimmedText = Text.TrimStartAndEnd();
	if (TrimmedText.IsEmpty())
	{
		return;
	}

	if (UShowDownVoiceSubsystem* VoiceSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UShowDownVoiceSubsystem>()
		: nullptr)
	{
		VoiceSubsystem->SpeakCollectorLine(TrimmedText);
	}
}

void AShowDownPlayerController::SDVoiceStatus()
{
	UShowDownVoiceSubsystem* VoiceSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UShowDownVoiceSubsystem>()
		: nullptr;
	if (!VoiceSubsystem)
	{
		ClientShowStatusMessage(TEXT("Voice subsystem missing."));
		return;
	}

	const FString Status = VoiceSubsystem->GetVoiceDebugSummary();
	UE_LOG(LogTemp, Log, TEXT("%s"), *Status);
	ClientShowStatusMessage(Status.Left(220));
}

void AShowDownPlayerController::SDVoiceStart()
{
	UShowDownVoiceSubsystem* VoiceSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UShowDownVoiceSubsystem>()
		: nullptr;
	if (!VoiceSubsystem)
	{
		ClientShowStatusMessage(TEXT("Voice subsystem missing."));
		return;
	}

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

void AShowDownPlayerController::SDVoiceStop()
{
	if (UShowDownVoiceSubsystem* VoiceSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UShowDownVoiceSubsystem>()
		: nullptr)
	{
		VoiceSubsystem->EndPushToTalk();
	}
}

void AShowDownPlayerController::SDVoiceCancel()
{
	if (UShowDownVoiceSubsystem* VoiceSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UShowDownVoiceSubsystem>()
		: nullptr)
	{
		VoiceSubsystem->CancelPushToTalk();
	}
}

void AShowDownPlayerController::SDVoiceEnable(bool bEnable)
{
	UShowDownVoiceSubsystem* VoiceSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UShowDownVoiceSubsystem>()
		: nullptr;
	if (!VoiceSubsystem)
	{
		ClientShowStatusMessage(TEXT("Voice subsystem missing."));
		return;
	}

	VoiceSubsystem->SetOpenAIVoiceEnabled(bEnable);
	const FString Status = VoiceSubsystem->GetVoiceDebugSummary();
	UE_LOG(LogTemp, Log, TEXT("%s"), *Status);
	ClientShowStatusMessage(Status.Left(220));
}

void AShowDownPlayerController::SDVoiceMode(const FString& Mode)
{
	UShowDownVoiceSubsystem* VoiceSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UShowDownVoiceSubsystem>()
		: nullptr;
	if (!VoiceSubsystem)
	{
		ClientShowStatusMessage(TEXT("Voice subsystem missing."));
		return;
	}

	const FString NormalizedMode = Mode.TrimStartAndEnd().ToLower();
	if (NormalizedMode == TEXT("off") || NormalizedMode == TEXT("0"))
	{
		VoiceSubsystem->SetVoiceInputMode(EShowDownVoiceInputMode::Off);
	}
	else if (NormalizedMode == TEXT("push") || NormalizedMode == TEXT("pushtotalk") || NormalizedMode == TEXT("ptt") || NormalizedMode == TEXT("1"))
	{
		VoiceSubsystem->SetVoiceInputMode(EShowDownVoiceInputMode::PushToTalk);
	}
	else if (NormalizedMode == TEXT("always") || NormalizedMode == TEXT("alwaysopen") || NormalizedMode == TEXT("open") || NormalizedMode == TEXT("2"))
	{
		VoiceSubsystem->SetVoiceInputMode(EShowDownVoiceInputMode::AlwaysOpen);
	}
	else
	{
		ClientShowStatusMessage(TEXT("Usage: SDVoiceMode off|push|always"));
		return;
	}

	const FString Status = VoiceSubsystem->GetVoiceDebugSummary();
	UE_LOG(LogTemp, Log, TEXT("%s"), *Status);
	ClientShowStatusMessage(Status.Left(220));
}

void AShowDownPlayerController::SDVoiceTone(float FrequencyHz, float DurationSeconds)
{
	UShowDownVoiceSubsystem* VoiceSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UShowDownVoiceSubsystem>()
		: nullptr;
	if (!VoiceSubsystem)
	{
		ClientShowStatusMessage(TEXT("Voice subsystem missing."));
		return;
	}

	VoiceSubsystem->PlayDebugTone(FrequencyHz, DurationSeconds);
	const FString Status = VoiceSubsystem->GetVoiceDebugSummary();
	UE_LOG(LogTemp, Log, TEXT("%s"), *Status);
	ClientShowStatusMessage(Status.Left(220));
}

void AShowDownPlayerController::SDVoiceSaveRecording()
{
	UShowDownVoiceSubsystem* VoiceSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UShowDownVoiceSubsystem>()
		: nullptr;
	if (!VoiceSubsystem)
	{
		ClientShowStatusMessage(TEXT("Voice subsystem missing."));
		return;
	}

	FString SavedFilePath;
	if (!VoiceSubsystem->SaveLastRecordingWav(SavedFilePath))
	{
		const FString Error = VoiceSubsystem->GetLastVoiceError();
		UE_LOG(LogTemp, Warning, TEXT("SDVoiceSaveRecording failed: %s"), *Error);
		ClientShowStatusMessage(Error.Left(220));
		return;
	}

	const FString Message = FString::Printf(TEXT("Voice recording saved: %s"), *SavedFilePath);
	UE_LOG(LogTemp, Log, TEXT("%s"), *Message);
	ClientShowStatusMessage(Message.Left(220));
}

void AShowDownPlayerController::SDVoiceTranscribeFile(const FString& FilePath)
{
	UShowDownVoiceSubsystem* VoiceSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UShowDownVoiceSubsystem>()
		: nullptr;
	if (!VoiceSubsystem)
	{
		ClientShowStatusMessage(TEXT("Voice subsystem missing."));
		return;
	}

	const bool bStarted = VoiceSubsystem->TranscribeWavFileForDebug(
		FilePath,
		FShowDownVoiceTextCallback::CreateWeakLambda(
			this,
			[this](bool bSuccess, const FString& Text)
			{
				const FString Message = bSuccess
					? FString::Printf(TEXT("Debug STT: %s"), *Text)
					: TEXT("Debug STT failed.");
				UE_LOG(LogTemp, Log, TEXT("%s"), *Message);
				ClientShowStatusMessage(Message.Left(220));
			}));

	if (!bStarted)
	{
		const FString Error = VoiceSubsystem->GetLastVoiceError();
		UE_LOG(LogTemp, Warning, TEXT("SDVoiceTranscribeFile failed: %s"), *Error);
		ClientShowStatusMessage(Error.Left(220));
	}
}

void AShowDownPlayerController::RequestPlayerCheck()
{
	SubmitPlayerBetAction(EShowDownBetAction::Check, 0);
}

void AShowDownPlayerController::RequestPlayerRaise()
{
	SubmitPlayerBetAction(EShowDownBetAction::Raise, 0);
}

void AShowDownPlayerController::RequestPlayerRaiseTo(int32 BulletCount)
{
	SubmitPlayerBetAction(EShowDownBetAction::Raise, BulletCount);
}

void AShowDownPlayerController::RequestRaisePreviewTarget(int32 BulletCount)
{
	const int32 ClampedTarget = FMath::Clamp(BulletCount, 1, 6);
	if (HasAuthority())
	{
		if (AShowDownGameModeBase* GameMode = ResolveGameMode())
		{
			GameMode->SetMultiplayerRaisePreviewTarget(GetPlayerState<ASDPlayerState>(), ClampedTarget);
		}
		return;
	}

	ServerSetRaisePreviewTarget(ClampedTarget);
}

void AShowDownPlayerController::RequestPlayerFold()
{
	SubmitPlayerBetAction(EShowDownBetAction::Fold, 0);
}

void AShowDownPlayerController::RequestLeaveMultiplayerMatch()
{
	EnsureLeaveConfirmWidget();
	if (!LeaveConfirmWidget)
	{
		return;
	}

	if (LeaveConfirmWidget->GetVisibility() == ESlateVisibility::Visible)
	{
		CancelLeaveMultiplayerMatch();
		return;
	}

	LeaveConfirmWidget->SetVisibility(ESlateVisibility::Visible);
	bShowMouseCursor = true;
	SetHoveredCard(nullptr);
	SetIgnoreLookInput(true);
	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(LeaveConfirmWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
}

void AShowDownPlayerController::ConfirmLeaveMultiplayerMatch()
{
	if (UShowDownEosSubsystem* EosSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UShowDownEosSubsystem>()
		: nullptr)
	{
		if (LeaveConfirmWidget)
		{
			LeaveConfirmWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
		bHandleShowDownGameplayInput = false;
		SetHoveredCard(nullptr);
		EosSubsystem->LeaveLobby(FName(TEXT("L_ShowdownMain")));
	}
}

void AShowDownPlayerController::CancelLeaveMultiplayerMatch()
{
	if (LeaveConfirmWidget)
	{
		LeaveConfirmWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	RestoreMultiplayerGameplayInput();
}

void AShowDownPlayerController::SetFixedCameraMouseLook(
	ACameraActor* Camera,
	float Sensitivity,
	float MinPitchDegrees,
	float MaxPitchDegrees,
	float MinYawOffsetDegrees,
	float MaxYawOffsetDegrees,
	bool bInvertY)
{
	ShowDownCameraAspect::ApplyForced16By9(Camera);
	SetFixedCameraComponentMouseLook(
		Camera ? Camera->GetCameraComponent() : nullptr,
		Sensitivity,
		MinPitchDegrees,
		MaxPitchDegrees,
		MinYawOffsetDegrees,
		MaxYawOffsetDegrees,
		bInvertY);
}

void AShowDownPlayerController::SetFixedCameraComponentMouseLook(
	USceneComponent* CameraComponent,
	float Sensitivity,
	float MinPitchDegrees,
	float MaxPitchDegrees,
	float MinYawOffsetDegrees,
	float MaxYawOffsetDegrees,
	bool bInvertY)
{
	if (FixedCameraMouseLookTarget)
	{
		RestoreFixedCameraBaseTransform();
	}

	FixedCameraMouseLookTarget = CameraComponent;
	if (!FixedCameraMouseLookTarget)
	{
		return;
	}

	if (UCameraComponent* FixedCameraComponent = Cast<UCameraComponent>(FixedCameraMouseLookTarget.Get()))
	{
		ShowDownCameraAspect::ApplyForced16By9(FixedCameraComponent);
	}

	FixedCameraBaseRotation = FixedCameraMouseLookTarget->GetComponentRotation();
	FixedCameraLookRotation = FixedCameraBaseRotation;
	FixedCameraBaseLocation = FixedCameraMouseLookTarget->GetComponentLocation();
	FixedCameraLookSensitivity = Sensitivity;
	FixedCameraMinPitch = MinPitchDegrees;
	FixedCameraMaxPitch = MaxPitchDegrees;
	FixedCameraMinYawOffset = MinYawOffsetDegrees;
	FixedCameraMaxYawOffset = MaxYawOffsetDegrees;
	bFixedCameraInvertMouseY = bInvertY;
	BreathingSwayElapsedTime = 0.0f;
	BreathingSwayBlendElapsedTime = 0.0f;
	UpdateCenterCrosshairVisibility();
}

void AShowDownPlayerController::SetPawnCameraMouseLook(
	float Sensitivity,
	float MinPitchDegrees,
	float MaxPitchDegrees,
	float MinYawOffsetDegrees,
	float MaxYawOffsetDegrees,
	bool bInvertY)
{
	LookSensitivity = FMath::Max(0.0f, Sensitivity);
	MinPitch = MinPitchDegrees;
	MaxPitch = MaxPitchDegrees;
	MinYaw = MinYawOffsetDegrees;
	MaxYaw = MaxYawOffsetDegrees;
	bInvertPawnCameraMouseY = bInvertY;
	bHasPawnCameraBaseRotation = false;
}

void AShowDownPlayerController::ClearFixedCameraMouseLook()
{
	RestoreFixedCameraBaseTransform();
	FixedCameraMouseLookTarget = nullptr;
	UpdateCenterCrosshairVisibility();
}

bool AShowDownPlayerController::BeginGunShotCameraOverride(
	ACameraActor* Camera,
	float BlendInTime,
	float BlendExponent)
{
	if (!IsLocalController() || !GetLocalPlayer() || !IsValid(Camera))
	{
		return false;
	}
	ClearEliminatedSpectatorViewState();

	if (bGunShotCameraOverrideActive && GunShotCameraOverrideTarget.Get() != Camera)
	{
		CancelGunShotCameraOverride();
	}

	if (!bGunShotCameraOverrideActive)
	{
		AActor* CurrentViewTarget = GetViewTarget();
		GunShotCameraReturnViewTarget = IsValid(CurrentViewTarget) && CurrentViewTarget != Camera
			? CurrentViewTarget
			: GetPawn();
	}

	GunShotCameraOverrideTarget = Camera;
	bGunShotCameraOverrideActive = true;
	bGunShotCameraBlendingOut = false;
	GunShotCameraBlendOutTimeRemaining = 0.0f;
	SetViewTargetWithBlend(
		Camera,
		FMath::Max(0.0f, BlendInTime),
		VTBlend_EaseInOut,
		FMath::Max(1.0f, BlendExponent));
	return true;
}

void AShowDownPlayerController::EndGunShotCameraOverride(
	ACameraActor* Camera,
	float BlendOutTime,
	float BlendExponent)
{
	if (!bGunShotCameraOverrideActive
		|| (IsValid(Camera) && GunShotCameraOverrideTarget.Get() != Camera))
	{
		return;
	}

	ACameraActor* ActiveCamera = GunShotCameraOverrideTarget.Get();
	if (IsValid(ActiveCamera) && GetViewTarget() != ActiveCamera)
	{
		// Another presentation deliberately took the view. Do not overwrite it
		// with the gameplay pawn while releasing this override.
		ClearGunShotCameraOverrideState();
		return;
	}

	AActor* ReturnViewTarget = GunShotCameraReturnViewTarget.Get();
	if (!IsValid(ReturnViewTarget))
	{
		ReturnViewTarget = GetPawn();
	}

	const float SafeBlendOutTime = FMath::Max(0.0f, BlendOutTime);
	if (IsValid(ReturnViewTarget))
	{
		SetViewTargetWithBlend(
			ReturnViewTarget,
			SafeBlendOutTime,
			VTBlend_EaseInOut,
			FMath::Max(1.0f, BlendExponent));
	}

	if (SafeBlendOutTime <= KINDA_SMALL_NUMBER || !IsValid(ReturnViewTarget))
	{
		ClearGunShotCameraOverrideState();
		return;
	}

	bGunShotCameraBlendingOut = true;
	GunShotCameraBlendOutTimeRemaining = SafeBlendOutTime;
}

void AShowDownPlayerController::ReleaseGunShotCameraOverrideForElimination(ACameraActor* Camera)
{
	if (!bGunShotCameraOverrideActive
		|| !IsValid(Camera)
		|| GunShotCameraOverrideTarget.Get() != Camera)
	{
		return;
	}

	// A result/ranking presentation may have deliberately taken the view while
	// this shot was finishing. Never steal it back just to establish the fallback
	// spectator view.
	if (GetViewTarget() != Camera)
	{
		ClearGunShotCameraOverrideState();
		return;
	}

	// The transient camera is owned by the gun actor and remains valid after the
	// override bookkeeping is cleared. Keep it as the view target until a rematch
	// restores this player's lives and normal character-camera maintenance resumes.
	EliminatedSpectatorCameraTarget = Camera;
	bEliminatedSpectatorViewActive = true;
	ClearGunShotCameraOverrideState();
}

void AShowDownPlayerController::CancelGunShotCameraOverride(ACameraActor* ExpectedCamera)
{
	if (!bGunShotCameraOverrideActive
		|| (IsValid(ExpectedCamera) && GunShotCameraOverrideTarget.Get() != ExpectedCamera))
	{
		return;
	}

	ACameraActor* ActiveCamera = GunShotCameraOverrideTarget.Get();
	AActor* ReturnViewTarget = GunShotCameraReturnViewTarget.Get();
	if (!IsValid(ReturnViewTarget))
	{
		ReturnViewTarget = GetPawn();
	}

	AActor* CurrentViewTarget = GetViewTarget();
	if (IsValid(ReturnViewTarget)
		&& (CurrentViewTarget == ActiveCamera || CurrentViewTarget == ReturnViewTarget))
	{
		SetViewTarget(ReturnViewTarget);
	}

	ClearGunShotCameraOverrideState();
}

void AShowDownPlayerController::UpdateGunShotCameraOverride(float DeltaTime)
{
	if (!bGunShotCameraOverrideActive)
	{
		return;
	}

	if (!GunShotCameraOverrideTarget.IsValid())
	{
		CancelGunShotCameraOverride();
		return;
	}

	if (!bGunShotCameraBlendingOut)
	{
		return;
	}

	GunShotCameraBlendOutTimeRemaining = FMath::Max(
		0.0f,
		GunShotCameraBlendOutTimeRemaining - FMath::Max(0.0f, DeltaTime));
	if (GunShotCameraBlendOutTimeRemaining <= KINDA_SMALL_NUMBER)
	{
		ClearGunShotCameraOverrideState();
	}
}

void AShowDownPlayerController::ClearGunShotCameraOverrideState()
{
	GunShotCameraOverrideTarget.Reset();
	GunShotCameraReturnViewTarget.Reset();
	GunShotCameraBlendOutTimeRemaining = 0.0f;
	bGunShotCameraOverrideActive = false;
	bGunShotCameraBlendingOut = false;
}

void AShowDownPlayerController::ClearEliminatedSpectatorViewState()
{
	EliminatedSpectatorCameraTarget.Reset();
	bEliminatedSpectatorViewActive = false;
}

void AShowDownPlayerController::SetFixedCameraBreathingSway(
	bool bEnable,
	float Speed,
	FRotator RotationAmplitude,
	FVector LocationAmplitude,
	float BlendInTime)
{
	bEnableFixedCameraBreathingSway = bEnable;
	BreathingSwaySpeed = FMath::Max(0.0f, Speed);
	BreathingSwayRotationAmplitude = RotationAmplitude;
	BreathingSwayLocationAmplitude = LocationAmplitude;
	BreathingSwayBlendInTime = FMath::Max(0.0f, BlendInTime);
	BreathingSwayElapsedTime = 0.0f;
	BreathingSwayBlendElapsedTime = 0.0f;
}

void AShowDownPlayerController::PlayFixedCameraSteppedShake(
	float HoldDuration,
	float BlendOutTime,
	FRotator RotationAmplitude,
	FVector LocationAmplitude,
	float StepInterval)
{
	CameraSteppedShakeHoldDuration = FMath::Max(0.0f, HoldDuration);
	CameraSteppedShakeBlendOutTime = FMath::Max(0.0f, BlendOutTime);
	CameraSteppedShakeElapsedTime = 0.0f;
	CameraSteppedShakeRotationAmplitude = RotationAmplitude;
	CameraSteppedShakeLocationAmplitude = LocationAmplitude;
	CameraSteppedShakeStepInterval = FMath::Max(0.01f, StepInterval);
	CameraSteppedShakeSeed = FMath::FRandRange(10.0f, 10000.0f);
}

void AShowDownPlayerController::SubmitPlayerBetAction(EShowDownBetAction Action, int32 TargetBet)
{
	if (HasAuthority())
	{
		if (AShowDownGameModeBase* GameMode = ResolveGameMode())
		{
			GameMode->RequestPlayerBetActionFromController(this, Action, TargetBet);
		}
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

void AShowDownPlayerController::ApplyPawnCameraInput(float YawInput, float PitchInput)
{
	const FRotator CurrentControlRotation = GetControlRotation();
	if (!bHasPawnCameraBaseRotation)
	{
		PawnCameraBaseRotation = CurrentControlRotation;
		bHasPawnCameraBaseRotation = true;
	}

	const float CurrentPitch = FRotator::NormalizeAxis(CurrentControlRotation.Pitch);
	const float CurrentYawOffset = FRotator::NormalizeAxis(CurrentControlRotation.Yaw - PawnCameraBaseRotation.Yaw);

	const float PitchSign = bInvertPawnCameraMouseY ? 1.0f : -1.0f;
	const float NewPitch = FMath::Clamp(
		CurrentPitch + PitchInput * LookSensitivity * UserMouseSensitivityMultiplier * PitchSign,
		PawnCameraBaseRotation.Pitch + MinPitch,
		PawnCameraBaseRotation.Pitch + MaxPitch);
	const float NewYawOffset = FMath::Clamp(CurrentYawOffset + YawInput * LookSensitivity * UserMouseSensitivityMultiplier, MinYaw, MaxYaw);
	const float NewYaw = PawnCameraBaseRotation.Yaw + NewYawOffset;

	SetControlRotation(FRotator(NewPitch, NewYaw, 0.0f));
}

AShowDownCharacter* AShowDownPlayerController::FindLocalCharacterForPlayerCamera() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const ASDPlayerState* ShowDownPlayerState = GetPlayerState<ASDPlayerState>();
	EShowDownPlayerSlot LocalSlot = ShowDownPlayerState
		? ShowDownPlayerState->ShowDownSlot
		: EShowDownPlayerSlot::None;
	if (LocalSlot == EShowDownPlayerSlot::None && PendingMultiplayerSeatIndex != INDEX_NONE)
	{
		LocalSlot = GetPlayerSlotFromSeatIndex(PendingMultiplayerSeatIndex);
	}

	AShowDownCharacter* FirstCharacter = nullptr;
	AShowDownCharacter* FallbackPlayerCharacter = nullptr;
	AShowDownCharacter* BestSinglePlayerCharacter = nullptr;
	EShowDownPlayerSlot BestSinglePlayerSlot = EShowDownPlayerSlot::None;
	for (TActorIterator<AShowDownCharacter> It(World); It; ++It)
	{
		AShowDownCharacter* CandidateCharacter = *It;
		if (!IsValid(CandidateCharacter) || !CandidateCharacter->IsCharacterSceneActive())
		{
			continue;
		}

		if (!FirstCharacter)
		{
			FirstCharacter = CandidateCharacter;
		}

		if (LocalSlot != EShowDownPlayerSlot::None && CandidateCharacter->IsAssignedToSlot(LocalSlot))
		{
			return CandidateCharacter;
		}

		if (CandidateCharacter->GetCharacterRole() == EShowDownCharacterRole::Player)
		{
			if (!FallbackPlayerCharacter)
			{
				FallbackPlayerCharacter = CandidateCharacter;
			}

			const EShowDownPlayerSlot CandidateSlot = CandidateCharacter->GetPlayerSlot();
			if (CandidateSlot != EShowDownPlayerSlot::None
				&& (!BestSinglePlayerCharacter
					|| static_cast<uint8>(CandidateSlot) < static_cast<uint8>(BestSinglePlayerSlot)))
			{
				BestSinglePlayerCharacter = CandidateCharacter;
				BestSinglePlayerSlot = CandidateSlot;
			}
		}
	}

	if (World->GetNetMode() != NM_Standalone)
	{
		return nullptr;
	}

	if (BestSinglePlayerCharacter)
	{
		return BestSinglePlayerCharacter;
	}

	if (FallbackPlayerCharacter)
	{
		return FallbackPlayerCharacter;
	}

	return FirstCharacter;
}

void AShowDownPlayerController::UpdateCharacterPlayerCamera(float DeltaTime)
{
	if (bGunShotCameraOverrideActive)
	{
		return;
	}

	if (bEliminatedSpectatorViewActive)
	{
		if (!EliminatedSpectatorCameraTarget.IsValid())
		{
			ClearEliminatedSpectatorViewState();
		}
		else
		{
			const bool bLivesRestored = IsValid(LocalPlayerCameraCharacterTarget)
				&& LocalPlayerCameraCharacterTarget->GetCharacterLives() > 0
				&& LocalPlayerCameraCharacterTarget->IsCharacterSceneActive();
			if (!bLivesRestored)
			{
				return;
			}
			ClearEliminatedSpectatorViewState();
		}
	}

	APlayerPawn* PlayerPawn = Cast<APlayerPawn>(GetPawn());
	UCameraComponent* PlayerCamera = PlayerPawn ? PlayerPawn->cameraComp : nullptr;
	if (!IsLocalController() || !bUseCharacterPlayerCamera || FixedCameraMouseLookTarget || !PlayerPawn || !PlayerCamera)
	{
		if (LocalPlayerCameraCharacterTarget)
		{
			LocalPlayerCameraCharacterTarget = nullptr;
		}
		return;
	}

	CharacterPlayerCameraRetryElapsedTime += DeltaTime;
	if (!IsValid(LocalPlayerCameraCharacterTarget)
		&& CharacterPlayerCameraRetryElapsedTime >= CharacterPlayerCameraRetryInterval)
	{
		CharacterPlayerCameraRetryElapsedTime = 0.0f;
		LocalPlayerCameraCharacterTarget = FindLocalCharacterForPlayerCamera();
	}

	if (!IsValid(LocalPlayerCameraCharacterTarget) || !LocalPlayerCameraCharacterTarget->GetMesh())
	{
		return;
	}

	// Final elimination deliberately leaves this controller on a detached
	// table-overview camera. Do not snap back to the hidden character. Restoring
	// lives for a rematch automatically allows the normal setup below to run.
	if (LocalPlayerCameraCharacterTarget->GetCharacterLives() <= 0
		|| !LocalPlayerCameraCharacterTarget->IsCharacterSceneActive())
	{
		return;
	}

	const FName AttachName = LocalPlayerCameraCharacterTarget->ResolvePlayerCameraAttachName();
	const bool bAlreadyAttached =
		PlayerCamera->GetAttachParent() == LocalPlayerCameraCharacterTarget->GetMesh()
		&& PlayerCamera->GetAttachSocketName() == AttachName;
	const bool bNeedsAspectSetup =
		ShowDownCameraAspect::NeedsForced16By9(PlayerCamera);
	const bool bNeedsCameraSetup =
		!bAlreadyAttached
		|| GetViewTarget() != PlayerPawn
		|| !PlayerCamera->bUsePawnControlRotation
		|| bNeedsAspectSetup;

	if (bNeedsCameraSetup)
	{
		if (bNeedsAspectSetup)
		{
			PlayerPawn->ApplyDefaultCameraAspect();
		}

		if (!bAlreadyAttached)
		{
			PlayerCamera->AttachToComponent(
				LocalPlayerCameraCharacterTarget->GetMesh(),
				FAttachmentTransformRules::SnapToTargetNotIncludingScale,
				AttachName);
		}

		PlayerCamera->bUsePawnControlRotation = true;

		if (GetViewTarget() != PlayerPawn)
		{
			SetViewTarget(PlayerPawn);
		}

		SetIgnoreLookInput(false);
		bShowMouseCursor = false;
		bEnableClickEvents = false;
		bEnableMouseOverEvents = false;
		FInputModeGameOnly InputMode;
		InputMode.SetConsumeCaptureMouseDown(false);
		SetInputMode(InputMode);
	}

	FVector CameraRelativeLocation = LocalPlayerCameraCharacterTarget->GetPlayerCameraRelativeLocation();
	const FVector StableLocationOffset = LocalPlayerCameraCharacterTarget->GetPlayerCameraStableLocationOffset();
	if (!StableLocationOffset.IsNearlyZero())
	{
		const USceneComponent* AttachParent = PlayerCamera->GetAttachParent();
		if (AttachParent)
		{
			const FTransform AttachTransform = AttachParent->GetSocketTransform(
				PlayerCamera->GetAttachSocketName(),
				RTS_World);
			const FVector StableWorldOffset =
				LocalPlayerCameraCharacterTarget->GetActorTransform().TransformVectorNoScale(StableLocationOffset);
			CameraRelativeLocation += AttachTransform.InverseTransformVectorNoScale(StableWorldOffset);
		}
	}

	PlayerCamera->SetRelativeLocation(CameraRelativeLocation);
	PlayerCamera->SetRelativeRotation(LocalPlayerCameraCharacterTarget->GetPlayerCameraRotationOffset());
	PlayerCamera->SetFieldOfView(LocalPlayerCameraCharacterTarget->GetPlayerCameraFOV());

	SubmitCharacterHeadLookRotation(GetControlRotation(), DeltaTime);
}

void AShowDownPlayerController::UpdateFixedCameraMouseLook(float DeltaTime)
{
	if (!FixedCameraMouseLookTarget)
	{
		return;
	}

	BreathingSwayElapsedTime += DeltaTime;
	BreathingSwayBlendElapsedTime += DeltaTime;
	const float SteppedShakeTotalTime = CameraSteppedShakeHoldDuration + CameraSteppedShakeBlendOutTime;
	if (CameraSteppedShakeElapsedTime < SteppedShakeTotalTime)
	{
		CameraSteppedShakeElapsedTime = FMath::Min(CameraSteppedShakeElapsedTime + DeltaTime, SteppedShakeTotalTime);
	}

	float MouseDeltaX = 0.0f;
	float MouseDeltaY = 0.0f;
	GetInputMouseDelta(MouseDeltaX, MouseDeltaY);
	if (!FMath::IsNearlyZero(MouseDeltaX) || !FMath::IsNearlyZero(MouseDeltaY))
	{
		FixedCameraLookRotation.Yaw += MouseDeltaX * FixedCameraLookSensitivity * UserMouseSensitivityMultiplier;

		const float RelativeYaw = FRotator::NormalizeAxis(FixedCameraLookRotation.Yaw - FixedCameraBaseRotation.Yaw);
		const float ClampedRelativeYaw = FMath::Clamp(RelativeYaw, FixedCameraMinYawOffset, FixedCameraMaxYawOffset);
		FixedCameraLookRotation.Yaw = FixedCameraBaseRotation.Yaw + ClampedRelativeYaw;

		const float PitchInputSign = bFixedCameraInvertMouseY ? 1.0f : -1.0f;
		const float CurrentPitch = FRotator::NormalizeAxis(FixedCameraLookRotation.Pitch);
		FixedCameraLookRotation.Pitch = FMath::Clamp(
			CurrentPitch + MouseDeltaY * FixedCameraLookSensitivity * UserMouseSensitivityMultiplier * PitchInputSign,
			FixedCameraMinPitch,
			FixedCameraMaxPitch);
		FixedCameraLookRotation.Roll = 0.0f;
	}

	const float SwayStrength = bEnableFixedCameraBreathingSway
		? (BreathingSwayBlendInTime > KINDA_SMALL_NUMBER
			? FMath::Clamp(BreathingSwayBlendElapsedTime / BreathingSwayBlendInTime, 0.0f, 1.0f)
			: 1.0f)
		: 0.0f;

	const FRotator SwayRotation = GetBreathingSwayRotationOffset(SwayStrength);
	const FVector SwayLocation = GetBreathingSwayLocationOffset(FixedCameraLookRotation, SwayStrength);
	const FRotator SteppedShakeRotation = GetCameraSteppedShakeRotationOffset();
	const FVector SteppedShakeLocation = GetCameraSteppedShakeLocationOffset(FixedCameraLookRotation);
	FixedCameraMouseLookTarget->SetWorldLocationAndRotation(
		FixedCameraBaseLocation + SwayLocation + SteppedShakeLocation,
		FixedCameraLookRotation + SwayRotation + SteppedShakeRotation);
	SubmitCharacterHeadLookRotation(FixedCameraLookRotation, DeltaTime);
}

void AShowDownPlayerController::SubmitCharacterHeadLookRotation(const FRotator& LookRotation, float DeltaTime)
{
	if (!IsLocalController())
	{
		return;
	}

	if (!IsValid(LocalPlayerCameraCharacterTarget))
	{
		LocalPlayerCameraCharacterTarget = FindLocalCharacterForPlayerCamera();
	}

	if (!IsValid(LocalPlayerCameraCharacterTarget))
	{
		return;
	}

	LocalPlayerCameraCharacterTarget->SetPlayerViewRotation(LookRotation);

	if (!bReplicateCharacterHeadLook)
	{
		return;
	}

	CharacterHeadLookReplicationElapsedTime += DeltaTime;
	const float PitchDelta = FMath::Abs(FRotator::NormalizeAxis(LookRotation.Pitch - LastSubmittedCharacterHeadLookRotation.Pitch));
	const float YawDelta = FMath::Abs(FRotator::NormalizeAxis(LookRotation.Yaw - LastSubmittedCharacterHeadLookRotation.Yaw));
	const bool bReplicationIntervalElapsed = CharacterHeadLookReplicationElapsedTime >= CharacterHeadLookReplicationInterval;
	const bool bRotationChanged = PitchDelta >= CharacterHeadLookReplicationAngleThreshold
		|| YawDelta >= CharacterHeadLookReplicationAngleThreshold;
	if (!bReplicationIntervalElapsed || !bRotationChanged)
	{
		return;
	}

	CharacterHeadLookReplicationElapsedTime = 0.0f;
	LastSubmittedCharacterHeadLookRotation = LookRotation;
	if (HasAuthority())
	{
		LocalPlayerCameraCharacterTarget->SetPlayerViewRotation(LookRotation);
		return;
	}

	ServerUpdateCharacterHeadLookRotation(LookRotation);
}

void AShowDownPlayerController::RestoreFixedCameraBaseTransform()
{
	if (!FixedCameraMouseLookTarget)
	{
		return;
	}

	FixedCameraMouseLookTarget->SetWorldLocationAndRotation(FixedCameraBaseLocation, FixedCameraLookRotation);
}

FRotator AShowDownPlayerController::GetBreathingSwayRotationOffset(float Strength) const
{
	if (Strength <= 0.0f || BreathingSwaySpeed <= 0.0f)
	{
		return FRotator::ZeroRotator;
	}

	const float Time = BreathingSwayElapsedTime * BreathingSwaySpeed * 2.0f * PI;
	return FRotator(
		FMath::Sin(Time) * BreathingSwayRotationAmplitude.Pitch,
		FMath::Sin(Time * 0.72f + 1.1f) * BreathingSwayRotationAmplitude.Yaw,
		FMath::Sin(Time * 0.91f + 2.0f) * BreathingSwayRotationAmplitude.Roll) * Strength;
}

FVector AShowDownPlayerController::GetBreathingSwayLocationOffset(const FRotator& CameraRotation, float Strength) const
{
	if (Strength <= 0.0f || BreathingSwaySpeed <= 0.0f)
	{
		return FVector::ZeroVector;
	}

	const float Time = BreathingSwayElapsedTime * BreathingSwaySpeed * 2.0f * PI;
	const FRotationMatrix CameraMatrix(CameraRotation);
	const FVector Forward = CameraMatrix.GetScaledAxis(EAxis::X);
	const FVector Right = CameraMatrix.GetScaledAxis(EAxis::Y);
	const FVector Up = CameraMatrix.GetScaledAxis(EAxis::Z);

	return (
		Forward * FMath::Sin(Time * 0.53f + 0.4f) * BreathingSwayLocationAmplitude.X +
		Right * FMath::Sin(Time * 0.79f + 1.7f) * BreathingSwayLocationAmplitude.Y +
		Up * FMath::Sin(Time) * BreathingSwayLocationAmplitude.Z) * Strength;
}

FRotator AShowDownPlayerController::GetCameraSteppedShakeRotationOffset() const
{
	const float TotalTime = CameraSteppedShakeHoldDuration + CameraSteppedShakeBlendOutTime;
	if (CameraSteppedShakeElapsedTime >= TotalTime || TotalTime <= KINDA_SMALL_NUMBER)
	{
		return FRotator::ZeroRotator;
	}

	const float Strength = CameraSteppedShakeElapsedTime <= CameraSteppedShakeHoldDuration
		? 1.0f
		: (CameraSteppedShakeBlendOutTime > KINDA_SMALL_NUMBER
			? 1.0f - FMath::Clamp(
				(CameraSteppedShakeElapsedTime - CameraSteppedShakeHoldDuration) / CameraSteppedShakeBlendOutTime,
				0.0f,
				1.0f)
			: 0.0f);
	const float Step = FMath::FloorToFloat(CameraSteppedShakeElapsedTime / CameraSteppedShakeStepInterval);

	return FRotator(
		FMath::PerlinNoise1D(Step * 1.73f + CameraSteppedShakeSeed) * CameraSteppedShakeRotationAmplitude.Pitch,
		FMath::PerlinNoise1D(Step * 2.11f + CameraSteppedShakeSeed + 31.0f) * CameraSteppedShakeRotationAmplitude.Yaw,
		FMath::PerlinNoise1D(Step * 2.67f + CameraSteppedShakeSeed + 73.0f) * CameraSteppedShakeRotationAmplitude.Roll) * Strength;
}

FVector AShowDownPlayerController::GetCameraSteppedShakeLocationOffset(const FRotator& CameraRotation) const
{
	const float TotalTime = CameraSteppedShakeHoldDuration + CameraSteppedShakeBlendOutTime;
	if (CameraSteppedShakeElapsedTime >= TotalTime || TotalTime <= KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	const float Strength = CameraSteppedShakeElapsedTime <= CameraSteppedShakeHoldDuration
		? 1.0f
		: (CameraSteppedShakeBlendOutTime > KINDA_SMALL_NUMBER
			? 1.0f - FMath::Clamp(
				(CameraSteppedShakeElapsedTime - CameraSteppedShakeHoldDuration) / CameraSteppedShakeBlendOutTime,
				0.0f,
				1.0f)
			: 0.0f);
	const float Step = FMath::FloorToFloat(CameraSteppedShakeElapsedTime / CameraSteppedShakeStepInterval);
	const FRotationMatrix CameraMatrix(CameraRotation);
	const FVector Forward = CameraMatrix.GetScaledAxis(EAxis::X);
	const FVector Right = CameraMatrix.GetScaledAxis(EAxis::Y);
	const FVector Up = CameraMatrix.GetScaledAxis(EAxis::Z);

	return (
		Forward * FMath::PerlinNoise1D(Step * 1.91f + CameraSteppedShakeSeed + 101.0f) * CameraSteppedShakeLocationAmplitude.X +
		Right * FMath::PerlinNoise1D(Step * 2.29f + CameraSteppedShakeSeed + 151.0f) * CameraSteppedShakeLocationAmplitude.Y +
		Up * FMath::PerlinNoise1D(Step * 2.83f + CameraSteppedShakeSeed + 211.0f) * CameraSteppedShakeLocationAmplitude.Z) * Strength;
}

void AShowDownPlayerController::HandleBettingHotkeys()
{
	if (WasInputKeyJustPressed(EKeys::Q))
	{
		RequestPlayerCheck();
	}

	if (WasInputKeyJustPressed(EKeys::R))
	{
		RequestPlayerFold();
	}

	if (WasInputKeyJustPressed(EKeys::E))
	{
		RequestPlayerRaise();
	}

	if (WasInputKeyJustPressed(EKeys::One))
	{
		SubmitPlayerBetAction(EShowDownBetAction::Raise, -1);
	}

	if (WasInputKeyJustPressed(EKeys::Two))
	{
		SubmitPlayerBetAction(EShowDownBetAction::Raise, -2);
	}

	if (WasInputKeyJustPressed(EKeys::Three))
	{
		SubmitPlayerBetAction(EShowDownBetAction::Raise, -3);
	}

	if (WasInputKeyJustPressed(EKeys::Four))
	{
		SubmitPlayerBetAction(EShowDownBetAction::Raise, -4);
	}

	if (WasInputKeyJustPressed(EKeys::Five))
	{
		SubmitPlayerBetAction(EShowDownBetAction::Raise, -5);
	}
}

void AShowDownPlayerController::HandleVoicePushToTalkInput()
{
	if (!bEnableVoicePushToTalk || !IsLocalController())
	{
		return;
	}

	if (IsMultiplayerGameMap(GetWorld()))
	{
		UShowDownEosSubsystem* EosSubsystem = GetGameInstance()
			? GetGameInstance()->GetSubsystem<UShowDownEosSubsystem>()
			: nullptr;
		if (!EosSubsystem)
		{
			return;
		}

		if (WasInputKeyJustPressed(VoicePushToTalkKey))
		{
			if (EosSubsystem->BeginVoiceTransmission())
			{
				SetLocalSpeakingIndicatorVisible(true);
			}
		}
		if (WasInputKeyJustReleased(VoicePushToTalkKey))
		{
			EosSubsystem->EndVoiceTransmission();
			SetLocalSpeakingIndicatorVisible(false);
		}
		return;
	}

	UShowDownVoiceSubsystem* VoiceSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UShowDownVoiceSubsystem>()
		: nullptr;
	if (!VoiceSubsystem || VoiceSubsystem->VoiceInputMode != EShowDownVoiceInputMode::PushToTalk)
	{
		return;
	}

	if (WasInputKeyJustPressed(VoicePushToTalkKey))
	{
		const bool bStartedRecording = VoiceSubsystem->BeginPushToTalk(
			FShowDownVoiceTextCallback::CreateWeakLambda(
				this,
				[this](bool bSuccess, const FString& Text)
				{
					if (!bSuccess)
					{
						return;
					}

					const FString TrimmedText = Text.TrimStartAndEnd();
					if (!TrimmedText.IsEmpty())
					{
						SubmitDialogueInput(TrimmedText);
					}
				}));
		if (bStartedRecording)
		{
			SetLocalSpeakingIndicatorVisible(true);
		}
	}

	if (WasInputKeyJustReleased(VoicePushToTalkKey))
	{
		VoiceSubsystem->EndPushToTalk();
		SetLocalSpeakingIndicatorVisible(false);
	}
}

bool AShowDownPlayerController::CanCreateLocalPlayerWidgets() const
{
	return IsLocalController() && GetLocalPlayer() != nullptr;
}

void AShowDownPlayerController::EnsureChatWidget()
{
	if (!CanCreateLocalPlayerWidgets() || !bGameplayChatEnabled)
	{
		return;
	}

	if (APlayerPawn* ShowDownPawn = Cast<APlayerPawn>(GetPawn()))
	{
		ShowDownPawn->ReleaseChatWidget();
	}

	if (ChatWidget && !ChatWidget->IsInViewport())
	{
		ChatWidget->RemoveFromParent();
		ChatWidget = nullptr;
	}
	RemoveLocalChatWidgetsExcept(ChatWidget);
	if (ChatWidget)
	{
		return;
	}

	ChatWidgetClass = UShowDownChatWidget::StaticClass();
	if (!ChatWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("ChatWidgetClass is not assigned on %s."), *GetName());
		return;
	}

	ChatWidget = CreateWidget<UShowDownChatWidget>(this, ChatWidgetClass);
	if (!ChatWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to create chat widget on %s."), *GetName());
		return;
	}

	ChatWidget->SetOwningShowDownController(this);
	ChatWidget->AddToViewport();
	ChatWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	ChatWidget->SetChatInputOpen(false);
}

void AShowDownPlayerController::RemoveLocalChatWidgetsExcept(UShowDownChatWidget* WidgetToKeep)
{
	if (!GetWorld() || !GetLocalPlayer())
	{
		return;
	}

	TArray<UUserWidget*> ChatWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
		this,
		ChatWidgets,
		UShowDownChatWidget::StaticClass(),
		true);
	for (UUserWidget* FoundWidget : ChatWidgets)
	{
		UShowDownChatWidget* FoundChatWidget = Cast<UShowDownChatWidget>(FoundWidget);
		if (FoundChatWidget
			&& FoundChatWidget != WidgetToKeep
			&& FoundChatWidget->GetOwningLocalPlayer() == GetLocalPlayer())
		{
			FoundChatWidget->RemoveFromParent();
		}
	}
}

void AShowDownPlayerController::DisableGameplayChat()
{
	bGameplayChatEnabled = false;
	bChatOpen = false;
	if (APlayerPawn* ShowDownPawn = Cast<APlayerPawn>(GetPawn()))
	{
		ShowDownPawn->ReleaseChatWidget();
	}
	if (ChatWidget)
	{
		ChatWidget->RemoveFromParent();
		ChatWidget = nullptr;
	}
	RemoveLocalChatWidgetsExcept(nullptr);
}

void AShowDownPlayerController::EnsureLeaveConfirmWidget()
{
	if (!CanCreateLocalPlayerWidgets() || LeaveConfirmWidget)
	{
		return;
	}

	LeaveConfirmWidget = CreateWidget<UShowDownLeaveConfirmWidget>(this, UShowDownLeaveConfirmWidget::StaticClass());
	if (!LeaveConfirmWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("멀티플레이 퇴장 확인창을 만들지 못했습니다."));
		return;
	}

	LeaveConfirmWidget->SetOwningShowDownController(this);
	LeaveConfirmWidget->AddToViewport(100);
	LeaveConfirmWidget->SetVisibility(ESlateVisibility::Collapsed);
}

void AShowDownPlayerController::RestoreMultiplayerGameplayInput()
{
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
	SetIgnoreLookInput(false);

	FInputModeGameOnly InputMode;
	InputMode.SetConsumeCaptureMouseDown(false);
	SetInputMode(InputMode);
}

void AShowDownPlayerController::ApplyChatInputMode(bool bOpen)
{
	if (bOpen && ChatWidget)
	{
		bShowMouseCursor = true;
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(ChatWidget->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
	}
	else if (FixedCameraMouseLookTarget)
	{
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
		bShowMouseCursor = false;
	}
	else
	{
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
		bShowMouseCursor = false;
	}
	UpdateCenterCrosshairVisibility();
}

void AShowDownPlayerController::CreateCenterCrosshairWidget()
{
	if (CenterCrosshairWidget.IsValid() || !CanCreateLocalPlayerWidgets())
	{
		return;
	}

	CenterCrosshairWidget = SNew(SSDCenterCrosshairWidget)
		.CrosshairSize(CenterCrosshairSize)
		.CrosshairThickness(CenterCrosshairThickness)
		.CrosshairColor(CenterCrosshairColor);

	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->AddViewportWidgetContent(CenterCrosshairWidget.ToSharedRef(), 50);
	}
}

void AShowDownPlayerController::UpdateCenterCrosshairVisibility()
{
	if (!CenterCrosshairWidget.IsValid())
	{
		return;
	}

	const bool bShouldShow =
		bShowCenterCrosshair
		&& bHandleShowDownGameplayInput
		&& !bChatOpen
		&& !bShowMouseCursor;

	CenterCrosshairWidget->SetVisibility(bShouldShow ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
}

void AShowDownPlayerController::RemoveCenterCrosshairWidget()
{
	if (!CenterCrosshairWidget.IsValid())
	{
		return;
	}

	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(CenterCrosshairWidget.ToSharedRef());
	}
	CenterCrosshairWidget.Reset();
}

void AShowDownPlayerController::SetHitBlackoutUiOpacity(float Opacity)
{
	const float ClampedOpacity = FMath::Clamp(Opacity, 0.0f, 1.0f);
	if (ClampedOpacity <= 0.0f)
	{
		if (!HitBlackoutOverlayWidget.IsValid())
		{
			return;
		}

		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(HitBlackoutOverlayWidget.ToSharedRef());
		}
		HitBlackoutOverlayWidget.Reset();
		return;
	}

	if (!HitBlackoutOverlayWidget.IsValid())
	{
		if (!CanCreateLocalPlayerWidgets())
		{
			return;
		}

		HitBlackoutOverlayWidget = SNew(SSDHitBlackoutOverlay);
		// Set the current fade amount before attaching the widget so a partial
		// camera fade never produces a one-frame fully opaque UI blackout.
		HitBlackoutOverlayWidget->SetRenderOpacity(ClampedOpacity);
		if (GEngine && GEngine->GameViewport)
		{
			// Screen-space WidgetComponents use shared viewport layers, so the mask
			// must sit above both those layers and ordinary UMG/Slate HUD content.
			GEngine->GameViewport->AddViewportWidgetContent(
				HitBlackoutOverlayWidget.ToSharedRef(),
				MAX_int32);
		}
		else
		{
			HitBlackoutOverlayWidget.Reset();
		}
	}

	if (HitBlackoutOverlayWidget.IsValid())
	{
		HitBlackoutOverlayWidget->SetRenderOpacity(ClampedOpacity);
	}
}

FString AShowDownPlayerController::GetChatSenderName() const
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

	const APlayerState* CurrentPlayerState = PlayerState;
	FString SenderName = CurrentPlayerState ? CurrentPlayerState->GetPlayerName() : TEXT("Player");
	SenderName = SenderName.TrimStartAndEnd();
	if (SenderName.IsEmpty() || SenderName.StartsWith(TEXT("DESKTOP-")))
	{
		return TEXT("Player");
	}

	return SenderName.Left(32);
}

void AShowDownPlayerController::TryBindVoiceChatEvents()
{
	if (!IsLocalController())
	{
		return;
	}

	AShowDownGameStateBase* ShowDownGameState = GetWorld()
		? GetWorld()->GetGameState<AShowDownGameStateBase>()
		: nullptr;
	if (ShowDownGameState
		&& VoiceBoundGameState.Get() == ShowDownGameState
		&& bVoiceChatEventsBound
		&& bVoiceSubsystemEventsBound
		&& (!IsMultiplayerGameMap(GetWorld()) || bEosVoiceEventsBound))
	{
		return;
	}

	if (ShowDownGameState && VoiceBoundGameState.Get() != ShowDownGameState)
	{
		if (AShowDownGameStateBase* PreviousGameState = VoiceBoundGameState.Get())
		{
			PreviousGameState->OnChatMessageReceived.RemoveDynamic(this, &AShowDownPlayerController::HandleChatMessageReceived);
		}

		ShowDownGameState->OnChatMessageReceived.AddDynamic(this, &AShowDownPlayerController::HandleChatMessageReceived);
		VoiceBoundGameState = ShowDownGameState;
		bVoiceChatEventsBound = true;
	}

	if (UShowDownVoiceSubsystem* VoiceSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UShowDownVoiceSubsystem>()
		: nullptr)
	{
		if (!bVoiceSubsystemEventsBound)
		{
			VoiceSubsystem->OnVoiceStatus.RemoveDynamic(this, &AShowDownPlayerController::HandleVoiceStatus);
			VoiceSubsystem->OnVoiceStatus.AddDynamic(this, &AShowDownPlayerController::HandleVoiceStatus);
			VoiceSubsystem->OnSpeechPlaybackStateChanged.RemoveDynamic(
				this,
				&AShowDownPlayerController::HandleSpeechPlaybackStateChanged);
			VoiceSubsystem->OnSpeechPlaybackStateChanged.AddDynamic(
				this,
				&AShowDownPlayerController::HandleSpeechPlaybackStateChanged);
			bVoiceSubsystemEventsBound = true;
		}
	}

	if (IsMultiplayerGameMap(GetWorld()))
	{
		if (UShowDownEosSubsystem* EosSubsystem = GetGameInstance()
			? GetGameInstance()->GetSubsystem<UShowDownEosSubsystem>()
			: nullptr)
		{
			if (!bEosVoiceEventsBound)
			{
				EosSubsystem->OnLocalVoiceTalkingChanged.RemoveDynamic(
					this,
					&AShowDownPlayerController::HandleLocalVoiceTalkingChanged);
				EosSubsystem->OnLocalVoiceTalkingChanged.AddDynamic(
					this,
					&AShowDownPlayerController::HandleLocalVoiceTalkingChanged);
				EosSubsystem->EnsureVoiceChatReady();
				bEosVoiceEventsBound = true;
			}
		}
	}
}

void AShowDownPlayerController::BroadcastLocalCollectorStatus(bool bSuccess, const FString& Message) const
{
	if (AShowDownGameStateBase* ShowDownGameState = GetWorld()
		? GetWorld()->GetGameState<AShowDownGameStateBase>()
		: nullptr)
	{
		ShowDownGameState->BroadcastCollectorLLMStatus(bSuccess, Message);
	}
}

void AShowDownPlayerController::SetLocalSpeakingIndicatorVisible(bool bVisible)
{
	if (!ChatWidget)
	{
		EnsureChatWidget();
	}
	if (ChatWidget)
	{
		ChatWidget->SetLocalSpeakingIndicatorVisible(bVisible);
	}
}

void AShowDownPlayerController::SetSingleOpponentSpeakingIndicatorVisible(bool bVisible) const
{
	if (!IsLocalController() || IsMultiplayerGameMap(GetWorld()))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AShowDownCharacter> It(World); It; ++It)
	{
		AShowDownCharacter* ShowDownCharacter = *It;
		if (IsValid(ShowDownCharacter) && ShowDownCharacter->GetCharacterRole() == EShowDownCharacterRole::Opponent)
		{
			ShowDownCharacter->SetVoiceTalking(bVisible);
			return;
		}
	}
}

void AShowDownPlayerController::HandleChatMessageReceived(const FString& SenderName, const FString& Message)
{
	if (!IsLocalController() || !SenderName.Equals(TEXT("Collector"), ESearchCase::IgnoreCase))
	{
		return;
	}

	if (UShowDownVoiceSubsystem* VoiceSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UShowDownVoiceSubsystem>()
		: nullptr)
	{
		VoiceSubsystem->SpeakCollectorLine(Message);
	}
}

void AShowDownPlayerController::HandleVoiceStatus(bool bSuccess, const FString& Message)
{
	BroadcastLocalCollectorStatus(bSuccess, Message);
}

void AShowDownPlayerController::HandleLocalVoiceTalkingChanged(bool bIsTalking)
{
	if (IsLocalController() && IsMultiplayerGameMap(GetWorld()))
	{
		SetLocalSpeakingIndicatorVisible(bIsTalking);
		ServerSetMultiplayerVoiceTalking(bIsTalking);
	}
}

void AShowDownPlayerController::HandleSpeechPlaybackStateChanged(bool bIsSpeaking)
{
	SetSingleOpponentSpeakingIndicatorVisible(bIsSpeaking);
}

void AShowDownPlayerController::SubmitLocalMultiplayerDisplayName()
{
	if (!IsLocalController())
	{
		return;
	}

	if (const UWorld* World = GetWorld())
	{
		const float CurrentTime = World->GetTimeSeconds();
		if (CurrentTime - LastMultiplayerDisplayNameSubmitTime < 1.0f)
		{
			return;
		}
		LastMultiplayerDisplayNameSubmitTime = CurrentTime;
	}

	const USupabaseSubsystem* SupabaseSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<USupabaseSubsystem>()
		: nullptr;
	if (!SupabaseSubsystem)
	{
		return;
	}

	const FString Nickname = SupabaseSubsystem->GetNickname().TrimStartAndEnd();
	if (!Nickname.IsEmpty())
	{
		const FString SanitizedName = Nickname.Left(32);
		if (!SanitizedName.Equals(LastSubmittedMultiplayerDisplayName, ESearchCase::CaseSensitive))
		{
			LastSubmittedMultiplayerDisplayName = SanitizedName;
			ServerSetMultiplayerDisplayName(SanitizedName);
		}
	}
}

void AShowDownPlayerController::SubmitLocalEquippedCharacterSkin()
{
	if (!IsLocalController() || !IsMultiplayerGameMap(GetWorld()))
	{
		return;
	}

	if (const UWorld* World = GetWorld())
	{
		const float CurrentTime = World->GetTimeSeconds();
		if (CurrentTime - LastEquippedCharacterSkinSubmitTime < 1.0f)
		{
			return;
		}
		LastEquippedCharacterSkinSubmitTime = CurrentTime;
	}

	FString SkinId = AShowDownCharacter::GetDefaultCharacterSkinId();
	if (const USupabaseSubsystem* SupabaseSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<USupabaseSubsystem>()
		: nullptr)
	{
		const FString EquippedSkinId = SupabaseSubsystem->GetEquippedSkinId(TEXT("character"));
		if (!EquippedSkinId.TrimStartAndEnd().IsEmpty())
		{
			SkinId = EquippedSkinId;
		}
	}
	SkinId = NormalizeSubmittedCharacterSkinId(SkinId);

	const ASDPlayerState* ShowDownPlayerState = GetPlayerState<ASDPlayerState>();
	const bool bReplicatedStateMatches = ShowDownPlayerState
		&& ShowDownPlayerState->GetEquippedCharacterSkinId().Equals(SkinId, ESearchCase::CaseSensitive);
	if (!SkinId.Equals(LastSubmittedEquippedCharacterSkinId, ESearchCase::CaseSensitive)
		|| !bReplicatedStateMatches)
	{
		LastSubmittedEquippedCharacterSkinId = SkinId;
		ServerSetEquippedCharacterSkinId(SkinId);
	}
}

AShowDownGameModeBase* AShowDownPlayerController::ResolveGameMode() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<AShowDownGameModeBase>() : nullptr;
}

void AShowDownPlayerController::ServerSubmitSelectedCard_Implementation(ACard* SelectedCard)
{
	if (SelectedCard)
	{
		if (AShowDownGameModeBase* GameMode = ResolveGameMode())
		{
			GameMode->PlayerSelectedCardFromController(this, SelectedCard);
		}
	}
}

void AShowDownPlayerController::ServerSubmitDialogueInput_Implementation(const FString& Text)
{
	const FString TrimmedText = Text.TrimStartAndEnd().Left(240);
	if (!TrimmedText.IsEmpty())
	{
		if (AShowDownGameModeBase* GameMode = ResolveGameMode())
		{
			FString SenderName = PlayerState ? PlayerState->GetPlayerName().TrimStartAndEnd() : FString();
			if (SenderName.IsEmpty() || SenderName.StartsWith(TEXT("DESKTOP-")))
			{
				SenderName = TEXT("Player");
			}

			GameMode->SubmitPlayerDialogueInputFromPlayer(TrimmedText, SenderName.Left(32));
		}
	}
}

void AShowDownPlayerController::ServerSetMultiplayerVoiceTalking_Implementation(bool bIsTalking)
{
	if (AShowDownGameModeBase* GameMode = ResolveGameMode())
	{
		GameMode->SetMultiplayerVoiceTalking(this, bIsTalking);
	}
}

void AShowDownPlayerController::ServerPlayerCheck_Implementation()
{
	SubmitPlayerBetAction(EShowDownBetAction::Check, 0);
}

void AShowDownPlayerController::ServerPlayerRaise_Implementation()
{
	SubmitPlayerBetAction(EShowDownBetAction::Raise, 0);
}

void AShowDownPlayerController::ServerPlayerRaiseTo_Implementation(int32 BulletCount)
{
	SubmitPlayerBetAction(EShowDownBetAction::Raise, BulletCount);
}

void AShowDownPlayerController::ServerSetRaisePreviewTarget_Implementation(int32 BulletCount)
{
	if (AShowDownGameModeBase* GameMode = ResolveGameMode())
	{
		GameMode->SetMultiplayerRaisePreviewTarget(
			GetPlayerState<ASDPlayerState>(),
			FMath::Clamp(BulletCount, 1, 6));
	}
}

void AShowDownPlayerController::ServerPlayerFold_Implementation()
{
	SubmitPlayerBetAction(EShowDownBetAction::Fold, 0);
}

void AShowDownPlayerController::ServerSetMultiplayerDisplayName_Implementation(const FString& DisplayName)
{
	const FString SanitizedName = DisplayName.TrimStartAndEnd().Left(32);
	if (SanitizedName.IsEmpty())
	{
		return;
	}

	if (APlayerState* CurrentPlayerState = PlayerState)
	{
		CurrentPlayerState->SetPlayerName(SanitizedName);
		if (const ASDPlayerState* ShowDownPlayerState = Cast<ASDPlayerState>(CurrentPlayerState))
		{
			for (TActorIterator<AShowDownCharacter> It(GetWorld()); It; ++It)
			{
				AShowDownCharacter* ShowDownCharacter = *It;
				if (IsValid(ShowDownCharacter) && ShowDownCharacter->IsAssignedToSlot(ShowDownPlayerState->ShowDownSlot))
				{
					ShowDownCharacter->SetCharacterDisplayName(SanitizedName);
					break;
				}
			}
		}

		if (AShowDownGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AShowDownGameModeBase>() : nullptr)
		{
			GameMode->RefreshMultiplayerLobbyPlayers();
		}
	}
}

void AShowDownPlayerController::ServerSetEquippedCharacterSkinId_Implementation(const FString& SkinId)
{
	ASDPlayerState* ShowDownPlayerState = GetPlayerState<ASDPlayerState>();
	if (!ShowDownPlayerState)
	{
		return;
	}

	const FString PreviousSkinId = ShowDownPlayerState->GetEquippedCharacterSkinId();
	ShowDownPlayerState->SetEquippedCharacterSkinId(SkinId);
	if (!PreviousSkinId.Equals(
		ShowDownPlayerState->GetEquippedCharacterSkinId(),
		ESearchCase::CaseSensitive))
	{
		if (AShowDownGameModeBase* GameMode = ResolveGameMode())
		{
			GameMode->RefreshMultiplayerLobbyPlayers();
		}
	}
}

void AShowDownPlayerController::ClientShowStatusMessage_Implementation(const FString& Message)
{
	UE_LOG(LogTemp, Log, TEXT("ShowDown status: %s"), *Message);
	if (MultiplayerRankWidget)
	{
		MultiplayerRankWidget->SetRestartStatus(Message);
	}
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Green, Message);
	}
}

void AShowDownPlayerController::ServerRequestMultiplayerRestart_Implementation()
{
	if (AShowDownGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AShowDownGameModeBase>() : nullptr)
	{
		GameMode->RequestMultiplayerRestartFromController(this);
	}
}

void AShowDownPlayerController::ServerNotifyInitialCardDealCameraReady_Implementation()
{
	if (AShowDownGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AShowDownGameModeBase>() : nullptr)
	{
		GameMode->NotifyInitialCardDealCameraReady(this);
	}
}

void AShowDownPlayerController::ServerUpdateCharacterHeadLookRotation_Implementation(FRotator LookRotation)
{
	if (AShowDownCharacter* TargetCharacter = FindLocalCharacterForPlayerCamera())
	{
		TargetCharacter->SetPlayerViewRotation(LookRotation);
	}
}

void AShowDownPlayerController::ClientShowMultiplayerRank_Implementation(const TArray<FString>& PlayerNames)
{
	if (!CanCreateLocalPlayerWidgets())
	{
		return;
	}

	DisableGameplayChat();
	if (bPauseMenuOpen)
	{
		ResumeFromPauseMenu();
	}
	RemoveCenterCrosshairWidget();
	if (LeaveConfirmWidget)
	{
		LeaveConfirmWidget->RemoveFromParent();
		LeaveConfirmWidget = nullptr;
	}

	if (MultiplayerRankWidget)
	{
		MultiplayerRankWidget->RemoveFromParent();
		MultiplayerRankWidget = nullptr;
	}

	if (!MultiplayerRankWidgetClass)
	{
		MultiplayerRankWidgetClass = UShowDownMultiRankWidget::StaticClass();
	}

	MultiplayerRankWidget = CreateWidget<UShowDownMultiRankWidget>(this, MultiplayerRankWidgetClass);
	if (!MultiplayerRankWidget)
	{
		return;
	}

	MultiplayerRankWidget->OnRestartRequested.AddDynamic(this, &AShowDownPlayerController::HandleMultiRankRestartRequested);
	MultiplayerRankWidget->OnMainMenuRequested.AddDynamic(this, &AShowDownPlayerController::HandleMultiRankMainMenuRequested);
	MultiplayerRankWidget->SetRanking(PlayerNames);
	MultiplayerRankWidget->AddToViewport(100);
	MultiplayerRankWidget->SetIsFocusable(true);

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(MultiplayerRankWidget->TakeWidget());
	SetInputMode(InputMode);
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	bHandleShowDownGameplayInput = false;
	UpdateCenterCrosshairVisibility();
}

void AShowDownPlayerController::HandleMultiRankRestartRequested()
{
	if (MultiplayerRankWidget)
	{
		MultiplayerRankWidget->SetRestartStatus(TEXT("재시작 동의 완료. 다른 참가자를 기다리는 중..."));
	}

	ServerRequestMultiplayerRestart();
}

void AShowDownPlayerController::HandleMultiRankMainMenuRequested()
{
	ConfirmLeaveMultiplayerMatch();
}
void AShowDownPlayerController::TogglePauseMenu()
{
	if (!CanCreateLocalPlayerWidgets()) return;
	if (bPauseMenuOpen) { ResumeFromPauseMenu(); return; }
	if (!PauseMenuWidgetClass) PauseMenuWidgetClass = LoadClass<UShowDownPauseMenuWidget>(nullptr, TEXT("/Game/UI/WBP_PauseMenu.WBP_PauseMenu_C"));
	if (!PauseMenuWidgetClass) return;
	PauseMenuWidget = CreateWidget<UShowDownPauseMenuWidget>(this, PauseMenuWidgetClass);
	if (!PauseMenuWidget) return;
	PauseMenuWidget->OnResume.AddUniqueDynamic(this,&AShowDownPlayerController::ResumeFromPauseMenu);
	PauseMenuWidget->OnMainMenu.AddUniqueDynamic(this,&AShowDownPlayerController::ReturnToMainMenuFromPause);
	PauseMenuWidget->OnSettings.AddUniqueDynamic(this,&AShowDownPlayerController::OpenSettingsFromPause);
	PauseMenuWidget->OnQuit.AddUniqueDynamic(this,&AShowDownPlayerController::QuitFromPauseMenu);
	PauseMenuWidget->AddToViewport(100);
	PauseMenuWidget->SetIsFocusable(true);
	bPauseMenuOpen=true; bGameplayInputBeforePause=bHandleShowDownGameplayInput; bHandleShowDownGameplayInput=false;
	bShowMouseCursor=true; FInputModeUIOnly Mode; Mode.SetWidgetToFocus(PauseMenuWidget->TakeWidget()); SetInputMode(Mode);
	if (GetWorld() && GetWorld()->GetNetMode()==NM_Standalone) UGameplayStatics::SetGamePaused(this,true);
}

void AShowDownPlayerController::SetUserMouseSensitivity(float Multiplier)
{
	UserMouseSensitivityMultiplier = FMath::Clamp(Multiplier, 0.2f, 2.0f);
}

void AShowDownPlayerController::ResumeFromPauseMenu()
{
	if(PauseSettingsWidget){PauseSettingsWidget->RemoveFromParent();PauseSettingsWidget=nullptr;}
	if(PauseMenuWidget){PauseMenuWidget->RemoveFromParent();PauseMenuWidget=nullptr;}
	if(GetWorld()&&GetWorld()->GetNetMode()==NM_Standalone) UGameplayStatics::SetGamePaused(this,false);
	bPauseMenuOpen=false;bHandleShowDownGameplayInput=bGameplayInputBeforePause;bShowMouseCursor=false;SetInputMode(FInputModeGameOnly());
}

void AShowDownPlayerController::ReturnToMainMenuFromPause()
{
	if(GetWorld()&&GetWorld()->GetNetMode()!=NM_Standalone){ResumeFromPauseMenu();RequestLeaveMultiplayerMatch();return;}
	ResumeFromPauseMenu();
	DisableGameplayChat();
	TArray<AActor*> Managers; UGameplayStatics::GetAllActorsOfClass(this,AShowDownHubFlowManager::StaticClass(),Managers);
	if(Managers.Num()>0)
	{
		// Returning from an active single-player match must stop the match before
		// swapping the camera/UI. This clears gameplay timers, pending reveals,
		// cards, gun callbacks, and the current reward context.
		CastChecked<AShowDownHubFlowManager>(Managers[0])->FinishResultAndReturnToHub();
	}
}

void AShowDownPlayerController::OpenSettingsFromPause()
{
	if(!CanCreateLocalPlayerWidgets() || !PauseMenuWidget || PauseSettingsWidget)return;
	UClass* SettingsClass=LoadClass<UShowDownSettingsWidget>(nullptr,TEXT("/Game/UI/WBP_Settings.WBP_Settings_C"));
	PauseSettingsWidget=SettingsClass?CreateWidget<UShowDownSettingsWidget>(this,SettingsClass):nullptr;
	if(PauseSettingsWidget)
	{
		PauseMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
		PauseSettingsWidget->OnBackRequested.AddUniqueDynamic(this,&AShowDownPlayerController::ReturnToPauseFromSettings);
		PauseSettingsWidget->OnQuitRequested.AddUniqueDynamic(this,&AShowDownPlayerController::QuitFromPauseMenu);
		PauseSettingsWidget->AddToViewport(101);
		PauseSettingsWidget->SetIsFocusable(true);
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(PauseSettingsWidget->TakeWidget());
		SetInputMode(Mode);
	}
}
void AShowDownPlayerController::ReturnToPauseFromSettings()
{
	if(PauseSettingsWidget){PauseSettingsWidget->RemoveFromParent();PauseSettingsWidget=nullptr;}
	if(PauseMenuWidget)
	{
		PauseMenuWidget->SetVisibility(ESlateVisibility::Visible);
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(PauseMenuWidget->TakeWidget());
		SetInputMode(Mode);
	}
}
void AShowDownPlayerController::QuitFromPauseMenu(){UKismetSystemLibrary::QuitGame(this,this,EQuitPreference::Quit,false);}
