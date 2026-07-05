#include "ShowDownPlayerController.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
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
#include "SDPlayerState.h"
#include "SDCardPlacementAnchor.h"
#include "ShowDownCharacter.h"
#include "ShowDownChatWidget.h"
#include "ShowDownEosSubsystem.h"
#include "ShowDownGameModeBase.h"
#include "ShowDownGameStateBase.h"
#include "ShowDownLeaveConfirmWidget.h"
#include "ShowDownMultiRankWidget.h"
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

namespace
{
	bool IsMultiplayerGameMap(const UWorld* World)
	{
		return World && World->GetNetMode() != NM_Standalone;
	}

	bool TryGetSharedTableLocation(UWorld* World, FVector& OutLocation)
	{
		if (!World)
		{
			return false;
		}

		FVector HandAnchorSum = FVector::ZeroVector;
		int32 HandAnchorCount = 0;
		FVector AnyAnchorSum = FVector::ZeroVector;
		int32 AnyAnchorCount = 0;
		for (TActorIterator<ASDCardPlacementAnchor> It(World); It; ++It)
		{
			const ASDCardPlacementAnchor* Anchor = *It;
			if (!Anchor)
			{
				continue;
			}

			const FVector AnchorLocation = Anchor->GetActorLocation();
			AnyAnchorSum += AnchorLocation;
			++AnyAnchorCount;
			if (Anchor->IsHandAnchor())
			{
				HandAnchorSum += AnchorLocation;
				++HandAnchorCount;
			}
		}

		if (HandAnchorCount > 0)
		{
			OutLocation = HandAnchorSum / static_cast<float>(HandAnchorCount);
			return true;
		}

		if (AnyAnchorCount > 0)
		{
			OutLocation = AnyAnchorSum / static_cast<float>(AnyAnchorCount);
			OutLocation.Z -= 30.0f;
			return true;
		}

		OutLocation = FVector(-5457.0f, -670.0f, 324.0f);
		return true;
	}
}

AShowDownPlayerController::AShowDownPlayerController()
{
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;

	// Keep multiplayer chat functional even when the level pawn Blueprint has
	// not supplied an override. This also makes the widget available early so it
	// can subscribe to replicated GameState chat broadcasts before the user
	// presses the chat key.
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

	static ConstructorHelpers::FClassFinder<UShowDownMultiRankWidget> MultiRankWidgetBlueprint(
		TEXT("/Game/UI/WBP_MultiRank"));
	if (MultiRankWidgetBlueprint.Succeeded())
	{
		MultiplayerRankWidgetClass = MultiRankWidgetBlueprint.Class;
	}
	else
	{
		MultiplayerRankWidgetClass = UShowDownMultiRankWidget::StaticClass();
	}
}

void AShowDownPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!Player)
	{
		UE_LOG(LogTemp, Warning, TEXT("Skipping local player setup for ShowDownPlayerController without a UPlayer yet: %s"), *GetName());
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
}

void AShowDownPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AShowDownGameStateBase* PreviousGameState = VoiceBoundGameState.Get())
	{
		PreviousGameState->OnChatMessageReceived.RemoveDynamic(this, &AShowDownPlayerController::HandleChatMessageReceived);
	}
	if (UShowDownVoiceSubsystem* VoiceSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UShowDownVoiceSubsystem>()
		: nullptr)
	{
		VoiceSubsystem->OnVoiceStatus.RemoveDynamic(this, &AShowDownPlayerController::HandleVoiceStatus);
	}
	VoiceBoundGameState.Reset();
	bVoiceChatEventsBound = false;
	bVoiceSubsystemEventsBound = false;

	SetFocusedInteractable(nullptr);
	if (InteractionOutlinePostProcessVolume)
	{
		InteractionOutlinePostProcessVolume->Destroy();
		InteractionOutlinePostProcessVolume = nullptr;
	}
	SetHoveredCard(nullptr);
	RemoveCenterCrosshairWidget();
	Super::EndPlay(EndPlayReason);
}

void AShowDownPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	InitializeFromPossessedPawn();
	SubmitLocalMultiplayerDisplayName();
}

void AShowDownPlayerController::ClientEnterMultiplayerGameplay_Implementation()
{
	if (UShowDownEosSubsystem* EosSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UShowDownEosSubsystem>()
		: nullptr)
	{
		EosSubsystem->MarkEnteredMultiplayerGame();
	}

	RemoveCenterCrosshairWidget();
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveAllViewportWidgets();
	}
	ChatWidget = nullptr;
	LeaveConfirmWidget = nullptr;
	MultiplayerRankWidget = nullptr;
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
	const ASDPlayerState* ShowDownPlayerState = GetPlayerState<ASDPlayerState>();
	UE_LOG(
		LogTemp,
		Log,
		TEXT("멀티플레이 좌석 카메라 요청 수신: SeatIndex=%d LocalSlot=%d Player=%s"),
		SeatIndex,
		ShowDownPlayerState ? static_cast<int32>(ShowDownPlayerState->ShowDownSlot) : -1,
		ShowDownPlayerState ? *ShowDownPlayerState->GetPlayerName() : TEXT("None"));

	// A client can receive this RPC while its seamless level transition is still
	// loading. Keep the seat number and retry from PlayerTick once the target
	// map's CameraActors actually exist locally.
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

	ClientTravel(TEXT("/Game/Maps/L_ShowdownMain"), TRAVEL_Absolute);
}

bool AShowDownPlayerController::TryApplyPendingMultiplayerSeatCamera()
{
	if (!bPendingMultiplayerSeatCamera || PendingMultiplayerSeatIndex == INDEX_NONE || !GetWorld())
	{
		return false;
	}

	return TryApplyPendingMultiplayerCharacterCamera();

	FVector TableLocation = FVector::ZeroVector;
	if (!TryGetSharedTableLocation(GetWorld(), TableLocation))
	{
		UE_LOG(LogTemp, Verbose, TEXT("멀티플레이 테이블 대기 중: SeatIndex=%d"), PendingMultiplayerSeatIndex);
		return false;
	}

	return IsMultiplayerGameMap(GetWorld())
		&& UseFallbackMultiplayerSeatCamera(PendingMultiplayerSeatIndex);

	const FName SeatCameraTag(*FString::Printf(TEXT("MP_SeatCamera_%d"), PendingMultiplayerSeatIndex + 1));
	for (TActorIterator<ACameraActor> It(GetWorld()); It; ++It)
	{
		ACameraActor* SeatCamera = *It;
		if (!SeatCamera || !SeatCamera->ActorHasTag(SeatCameraTag))
		{
			continue;
		}

		const float DistanceFromTable = FVector::Dist2D(SeatCamera->GetActorLocation(), TableLocation);
		UE_LOG(
			LogTemp,
			Log,
			TEXT("멀티플레이 좌석 카메라 후보: Tag=%s Actor=%s Location=%s Rotation=%s TableDistance2D=%.1f"),
			*SeatCameraTag.ToString(),
			*SeatCamera->GetName(),
			*SeatCamera->GetActorLocation().ToCompactString(),
			*SeatCamera->GetActorRotation().ToCompactString(),
			DistanceFromTable);
		if (DistanceFromTable > 2500.0f)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("%s 카메라가 테이블에서 너무 멀어 무시합니다. Actor=%s Distance=%.1f"),
				*SeatCameraTag.ToString(),
				*SeatCamera->GetName(),
				DistanceFromTable);
			continue;
		}

		SetViewTarget(SeatCamera);
		SetFixedCameraMouseLook(
			SeatCamera,
			PendingMultiplayerSeatCameraLookSensitivity,
			PendingMultiplayerCameraMinPitch,
			PendingMultiplayerCameraMaxPitch,
			PendingMultiplayerCameraMinYawOffset,
			PendingMultiplayerCameraMaxYawOffset,
			bPendingMultiplayerCameraInvertMouseY);
		SetFixedCameraBreathingSway(
			bPendingMultiplayerCameraBreathingSway,
			PendingMultiplayerCameraBreathingSwaySpeed,
			PendingMultiplayerCameraBreathingSwayRotationAmplitude,
			PendingMultiplayerCameraBreathingSwayLocationAmplitude,
			PendingMultiplayerCameraBreathingSwayBlendInTime);
		EnsureChatWidget();
		bPendingMultiplayerSeatCamera = false;
		bHandleShowDownGameplayInput = true;
		bShowCenterCrosshair = true;
		RestoreMultiplayerGameplayInput();
		CreateCenterCrosshairWidget();
		UpdateCenterCrosshairVisibility();
		UE_LOG(LogTemp, Log, TEXT("멀티플레이 좌석 카메라 적용: %s"), *SeatCameraTag.ToString());
		return true;
	}

	// Some older copies of the level do not contain the named CameraActors (or
	// do not preserve their actor tags after a map migration). Do not leave the
	// player at a PlayerStart in that case: create the same four table views
	// locally from the authoritative replicated table position.
	return IsMultiplayerGameMap(GetWorld())
		&& UseFallbackMultiplayerSeatCamera(PendingMultiplayerSeatIndex);
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
	bHandleShowDownGameplayInput = true;
	bShowCenterCrosshair = true;
	RestoreMultiplayerGameplayInput();
	CreateCenterCrosshairWidget();
	UpdateCenterCrosshairVisibility();
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

bool AShowDownPlayerController::UseFallbackMultiplayerSeatCamera(int32 SeatIndex)
{
	if (!IsLocalController() || !GetWorld() || SeatIndex < 0 || SeatIndex > 3)
	{
		return false;
	}

	static const FVector SeatOffsets[] = {
		FVector(-85.0f, 0.0f, 45.0f),
		FVector(85.0f, 0.0f, 45.0f),
		FVector(0.0f, -200.0f, 45.0f),
		FVector(0.0f, 200.0f, 45.0f)
	};
	static const float SeatYaws[] = { 0.0f, 180.0f, 90.0f, -90.0f };

	FVector TableCenter = FVector::ZeroVector;
	if (!TryGetSharedTableLocation(GetWorld(), TableCenter))
	{
		return false;
	}

	const FTransform CameraTransform(
		FRotator(-20.0f, SeatYaws[SeatIndex], 0.0f),
		TableCenter + SeatOffsets[SeatIndex]);
	if (!IsValid(LocalFallbackSeatCamera))
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		LocalFallbackSeatCamera = GetWorld()->SpawnActor<ACameraActor>(
			ACameraActor::StaticClass(), CameraTransform, SpawnParams);
	}
	else
	{
		LocalFallbackSeatCamera->SetActorTransform(CameraTransform);
	}

	if (!IsValid(LocalFallbackSeatCamera))
	{
		return false;
	}

	SetViewTarget(LocalFallbackSeatCamera);
	SetFixedCameraMouseLook(
		LocalFallbackSeatCamera,
		PendingMultiplayerSeatCameraLookSensitivity,
		PendingMultiplayerCameraMinPitch,
		PendingMultiplayerCameraMaxPitch,
		PendingMultiplayerCameraMinYawOffset,
		PendingMultiplayerCameraMaxYawOffset,
		bPendingMultiplayerCameraInvertMouseY);
	SetFixedCameraBreathingSway(
		bPendingMultiplayerCameraBreathingSway,
		PendingMultiplayerCameraBreathingSwaySpeed,
		PendingMultiplayerCameraBreathingSwayRotationAmplitude,
		PendingMultiplayerCameraBreathingSwayLocationAmplitude,
		PendingMultiplayerCameraBreathingSwayBlendInTime);
	EnsureChatWidget();
	bPendingMultiplayerSeatCamera = false;
	bHandleShowDownGameplayInput = true;
	bShowCenterCrosshair = true;
	RestoreMultiplayerGameplayInput();
	CreateCenterCrosshairWidget();
	UpdateCenterCrosshairVisibility();
	UE_LOG(LogTemp, Warning, TEXT("Authored seat camera missing; using fallback multiplayer seat camera %d."), SeatIndex + 1);
	return true;
}

void AShowDownPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (bPendingMultiplayerSeatCamera)
	{
		TryApplyPendingMultiplayerSeatCamera();
	}
	TryBindVoiceChatEvents();
	if (IsMultiplayerGameMap(GetWorld()))
	{
		SubmitLocalMultiplayerDisplayName();
	}

	const bool bHasFixedCameraLook = FixedCameraMouseLookTarget != nullptr;
	if (bHasFixedCameraLook)
	{
		UpdateFixedCameraMouseLook(DeltaTime);
	}

	if (!bHandleShowDownGameplayInput)
	{
		SetFocusedInteractable(nullptr);
		SetHoveredCard(nullptr);
		UpdateCenterCrosshairVisibility();
		return;
	}

	if (LeaveConfirmWidget && LeaveConfirmWidget->GetVisibility() == ESlateVisibility::Visible)
	{
		if (WasInputKeyJustPressed(LeaveMatchKey))
		{
			CancelLeaveMultiplayerMatch();
		}
		SetHoveredCard(nullptr);
		return;
	}

	if (bChatOpen)
	{
		SetFocusedInteractable(nullptr);
		SetHoveredCard(nullptr);
		UpdateCenterCrosshairVisibility();
		if (WasInputKeyJustPressed(CloseChatKey))
		{
			CloseChat();
		}
		return;
	}

	if (WasInputKeyJustPressed(ToggleChatKey))
	{
		OpenChat();
		return;
	}

	HandleVoicePushToTalkInput();

	if (WasInputKeyJustPressed(LeaveMatchKey))
	{
		RequestLeaveMultiplayerMatch();
		return;
	}

	if (bEnablePrimaryClickTrace && WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		HandlePrimaryClick();
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
	UpdateFocusedInteractable();
	UpdateHoveredCard();
	UpdateCenterCrosshairVisibility();
}

void AShowDownPlayerController::InitializeFromPossessedPawn()
{
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;

	const APlayerPawn* ShowDownPawn = Cast<APlayerPawn>(GetPawn());
	if (!ShowDownPawn)
	{
		return;
	}

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
		if (HandCard && HandCard->IsCardSelectable())
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
	if (HandCard && HandCard->IsCardSelectable())
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

void AShowDownPlayerController::TraceCardUnderCursor()
{
	HandCard = nullptr;

	FHitResult Hit;
	if (!TracePrimaryInteraction(Hit))
	{
		return;
	}

	HandCard = ResolveCardFromHit(Hit);
	if (HandCard && !HandCard->IsCardSelectable())
	{
		HandCard = nullptr;
	}
}

bool AShowDownPlayerController::TracePrimaryInteraction(FHitResult& OutHit) const
{
	if (bShowMouseCursor && TraceUnderCursor(OutHit))
	{
		return true;
	}

	if (bHandleShowDownGameplayInput && !bChatOpen)
	{
		return bUseCenterScreenTraceWhenCursorHidden && TraceFromScreenCenter(OutHit);
	}

	if (TraceUnderCursor(OutHit))
	{
		return true;
	}

	return bUseCenterScreenTraceWhenCursorHidden && TraceFromScreenCenter(OutHit);
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
		if (!IsValid(Card) || !Card->IsCardSelectable())
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
		if (IsValid(HitCard) && HitCard->IsCardSelectable())
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
	if (!IsValid(NewHoveredCard) || !NewHoveredCard->IsCardSelectable())
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
	if (!IsValid(SelectedCard))
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
	if (!IsValid(SelectedCard))
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
		ChatWidget->SetVisibility(ESlateVisibility::Collapsed);
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

	ServerSubmitDialogueInput(TrimmedText, GetChatSenderName());
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
		CurrentPitch + PitchInput * LookSensitivity * PitchSign,
		PawnCameraBaseRotation.Pitch + MinPitch,
		PawnCameraBaseRotation.Pitch + MaxPitch);
	const float NewYawOffset = FMath::Clamp(CurrentYawOffset + YawInput * LookSensitivity, MinYaw, MaxYaw);
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
		if (!IsValid(CandidateCharacter))
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

	const FName AttachName = LocalPlayerCameraCharacterTarget->ResolvePlayerCameraAttachName();
	const bool bAlreadyAttached =
		PlayerCamera->GetAttachParent() == LocalPlayerCameraCharacterTarget->GetMesh()
		&& PlayerCamera->GetAttachSocketName() == AttachName;
	const bool bNeedsCameraSetup =
		!bAlreadyAttached
		|| GetViewTarget() != PlayerPawn
		|| !PlayerCamera->bUsePawnControlRotation;

	if (bNeedsCameraSetup)
	{
		if (!bAlreadyAttached)
		{
			PlayerCamera->AttachToComponent(
				LocalPlayerCameraCharacterTarget->GetMesh(),
				FAttachmentTransformRules::SnapToTargetNotIncludingScale,
				AttachName);
		}

		PlayerCamera->SetRelativeLocation(LocalPlayerCameraCharacterTarget->GetPlayerCameraRelativeLocation());
		PlayerCamera->SetRelativeRotation(LocalPlayerCameraCharacterTarget->GetPlayerCameraRotationOffset());
		PlayerCamera->SetFieldOfView(LocalPlayerCameraCharacterTarget->GetPlayerCameraFOV());
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
		FixedCameraLookRotation.Yaw += MouseDeltaX * FixedCameraLookSensitivity;

		const float RelativeYaw = FRotator::NormalizeAxis(FixedCameraLookRotation.Yaw - FixedCameraBaseRotation.Yaw);
		const float ClampedRelativeYaw = FMath::Clamp(RelativeYaw, FixedCameraMinYawOffset, FixedCameraMaxYawOffset);
		FixedCameraLookRotation.Yaw = FixedCameraBaseRotation.Yaw + ClampedRelativeYaw;

		const float PitchInputSign = bFixedCameraInvertMouseY ? 1.0f : -1.0f;
		const float CurrentPitch = FRotator::NormalizeAxis(FixedCameraLookRotation.Pitch);
		FixedCameraLookRotation.Pitch = FMath::Clamp(
			CurrentPitch + MouseDeltaY * FixedCameraLookSensitivity * PitchInputSign,
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
	SubmitDebugCameraLookRotation(FixedCameraLookRotation, DeltaTime);
	SubmitCharacterHeadLookRotation(FixedCameraLookRotation, DeltaTime);
}

void AShowDownPlayerController::SubmitDebugCameraLookRotation(const FRotator& LookRotation, float DeltaTime)
{
	if (!IsLocalController() || !IsMultiplayerGameMap(GetWorld()))
	{
		return;
	}

	DebugCameraLookReplicationElapsedTime += DeltaTime;
	const float PitchDelta = FMath::Abs(FRotator::NormalizeAxis(LookRotation.Pitch - LastSubmittedDebugCameraLookRotation.Pitch));
	const float YawDelta = FMath::Abs(FRotator::NormalizeAxis(LookRotation.Yaw - LastSubmittedDebugCameraLookRotation.Yaw));
	if (DebugCameraLookReplicationElapsedTime < 0.05f && PitchDelta < 0.5f && YawDelta < 0.5f)
	{
		return;
	}

	DebugCameraLookReplicationElapsedTime = 0.0f;
	LastSubmittedDebugCameraLookRotation = LookRotation;

	if (HasAuthority())
	{
		if (APlayerPawn* PlayerPawn = Cast<APlayerPawn>(GetPawn()))
		{
			PlayerPawn->SetReplicatedCameraLookRotation(LookRotation);
		}
		return;
	}

	ServerUpdateDebugCameraLookRotation(LookRotation);
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
	if (CharacterHeadLookReplicationElapsedTime < 0.05f && PitchDelta < 0.5f && YawDelta < 0.5f)
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

	UShowDownVoiceSubsystem* VoiceSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UShowDownVoiceSubsystem>()
		: nullptr;
	if (!VoiceSubsystem || VoiceSubsystem->VoiceInputMode != EShowDownVoiceInputMode::PushToTalk)
	{
		return;
	}

	if (WasInputKeyJustPressed(VoicePushToTalkKey))
	{
		VoiceSubsystem->BeginPushToTalk(
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
	}

	if (WasInputKeyJustReleased(VoicePushToTalkKey))
	{
		VoiceSubsystem->EndPushToTalk();
	}
}

void AShowDownPlayerController::EnsureChatWidget()
{
	if (ChatWidget)
	{
		return;
	}

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
	ChatWidget->SetVisibility(ESlateVisibility::Collapsed);
}

void AShowDownPlayerController::EnsureLeaveConfirmWidget()
{
	if (LeaveConfirmWidget)
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
	if (CenterCrosshairWidget.IsValid() || !IsLocalController())
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
			bVoiceSubsystemEventsBound = true;
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

void AShowDownPlayerController::SubmitLocalMultiplayerDisplayName()
{
	if (!IsLocalController())
	{
		return;
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

void AShowDownPlayerController::ServerSubmitDialogueInput_Implementation(const FString& Text, const FString& SenderName)
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
		if (AShowDownGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AShowDownGameModeBase>() : nullptr)
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

void AShowDownPlayerController::ServerUpdateDebugCameraLookRotation_Implementation(FRotator LookRotation)
{
	if (APlayerPawn* PlayerPawn = Cast<APlayerPawn>(GetPawn()))
	{
		PlayerPawn->SetReplicatedCameraLookRotation(LookRotation);
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
	RemoveCenterCrosshairWidget();
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
