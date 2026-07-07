#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "ShowDownTypes.h"
#include "ShowDownPlayerController.generated.h"

class ACard;
class ACameraActor;
class APostProcessVolume;
class AShowDownCharacter;
class AShowDownGameModeBase;
class SWidget;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPrimitiveComponent;
class USceneComponent;
class UShowDownChatWidget;
class UShowDownLeaveConfirmWidget;
class UShowDownMultiRankWidget;
class UShowDownVoiceSubsystem;

struct FSDPrimitiveCustomDepthState
{
	TWeakObjectPtr<UPrimitiveComponent> Component;
	bool bRenderCustomDepth = false;
	int32 CustomDepthStencilValue = 0;
};

UCLASS()
class SHOWDOWN_API AShowDownPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AShowDownPlayerController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void PlayerTick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Input")
	void HandlePrimaryClick();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Card")
	void SubmitSelectedCard(ACard* SelectedCard);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Chat")
	void ToggleChat();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Chat")
	void OpenChat();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Chat")
	void CloseChat();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Chat")
	void SubmitDialogueInput(const FString& Text);

	FString GetChatSenderName() const;

	UFUNCTION(Exec)
	void SDVoiceSubmitText(const FString& Text);

	UFUNCTION(Exec)
	void SDVoiceSpeak(const FString& Text);

	UFUNCTION(Exec)
	void SDVoiceStatus();

	UFUNCTION(Exec)
	void SDVoiceStart();

	UFUNCTION(Exec)
	void SDVoiceStop();

	UFUNCTION(Exec)
	void SDVoiceCancel();

	UFUNCTION(Exec)
	void SDVoiceEnable(bool bEnable);

	UFUNCTION(Exec)
	void SDVoiceMode(const FString& Mode);

	UFUNCTION(Exec)
	void SDVoiceTone(float FrequencyHz = 440.0f, float DurationSeconds = 0.75f);

	UFUNCTION(Exec)
	void SDVoiceSaveRecording();

	UFUNCTION(Exec)
	void SDVoiceTranscribeFile(const FString& FilePath);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Betting")
	void RequestPlayerCheck();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Betting")
	void RequestPlayerRaise();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Betting")
	void RequestPlayerRaiseTo(int32 BulletCount);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Betting")
	void RequestPlayerFold();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Multiplayer")
	void RequestLeaveMultiplayerMatch();

	void ConfirmLeaveMultiplayerMatch();
	void CancelLeaveMultiplayerMatch();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Camera")
	void SetFixedCameraMouseLook(
		ACameraActor* Camera,
		float Sensitivity,
		float MinPitchDegrees,
		float MaxPitchDegrees,
		float MinYawOffsetDegrees,
		float MaxYawOffsetDegrees,
		bool bInvertY);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Camera")
	void ClearFixedCameraMouseLook();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Camera")
	void SetFixedCameraBreathingSway(
		bool bEnable,
		float Speed,
		FRotator RotationAmplitude,
		FVector LocationAmplitude,
		float BlendInTime);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Camera")
	void SetPawnCameraMouseLook(
		float Sensitivity,
		float MinPitchDegrees,
		float MaxPitchDegrees,
		float MinYawOffsetDegrees,
		float MaxYawOffsetDegrees,
		bool bInvertY);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Camera")
	void PlayFixedCameraSteppedShake(
		float HoldDuration,
		float BlendOutTime,
		FRotator RotationAmplitude,
		FVector LocationAmplitude,
		float StepInterval);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Camera")
	void SetFixedCameraComponentMouseLook(
		USceneComponent* CameraComponent,
		float Sensitivity,
		float MinPitchDegrees,
		float MaxPitchDegrees,
		float MinYawOffsetDegrees,
		float MaxYawOffsetDegrees,
		bool bInvertY);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ShowDown|Input")
	bool HandlesShowDownGameplayInput() const { return bHandleShowDownGameplayInput; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Input")
	bool bHandleShowDownGameplayInput = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Input")
	bool bEnableLegacyKeyboardBetHotkeys = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Input")
	bool bEnablePrimaryClickTrace = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Input")
	bool bEnableInteractableTrace = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Input")
	bool bSubmitCardsOnSingleClick = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Input")
	bool bUseCenterScreenTraceWhenCursorHidden = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Input", meta = (ClampMin = "0.0"))
	float CenterScreenTraceDistance = 2500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Input")
	bool bUseCenterAimCardFallback = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Input")
	bool bEnableCardHoverPreview = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Input", meta = (ClampMin = "1.0"))
	float CenterAimCardPickRadiusPixels = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Input|Crosshair")
	bool bShowCenterCrosshair = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Input|Crosshair", meta = (ClampMin = "1.0"))
	float CenterCrosshairSize = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Input|Crosshair", meta = (ClampMin = "1.0"))
	float CenterCrosshairThickness = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Input|Crosshair")
	FLinearColor CenterCrosshairColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.9f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Input|Interactable Outline")
	bool bEnableInteractableAimOutline = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Input|Interactable Outline")
	TObjectPtr<UMaterialInterface> InteractionOutlineMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Input|Interactable Outline")
	FLinearColor InteractableOutlineColor = FLinearColor(1.0f, 0.78f, 0.05f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Input|Interactable Outline", meta = (ClampMin = "1.0", ClampMax = "8.0"))
	float InteractableOutlineThickness = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Input|Interactable Outline", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InteractableOutlineOpacity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Input|Interactable Outline", meta = (ClampMin = "1", ClampMax = "255"))
	int32 InteractableOutlineStencilValue = 252;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Camera")
	bool bEnablePawnCameraMouseLook = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Camera")
	bool bRequireRightMouseForPawnCameraLook = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Camera")
	bool bInvertPawnCameraMouseY = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Camera|Character")
	bool bUseCharacterPlayerCamera = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Camera|Character")
	bool bReplicateCharacterHeadLook = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Camera|Character", meta = (ClampMin = "0.0"))
	float CharacterPlayerCameraRetryInterval = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Camera")
	float LookSensitivity = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Camera")
	float MinPitch = -35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Camera")
	float MaxPitch = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Camera")
	float MinYaw = -45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Camera")
	float MaxYaw = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Camera|Breathing")
	bool bEnableFixedCameraBreathingSway = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Camera|Breathing", meta = (ClampMin = "0.0"))
	float BreathingSwaySpeed = 0.38f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Camera|Breathing")
	FRotator BreathingSwayRotationAmplitude = FRotator(0.12f, 0.05f, 0.08f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Camera|Breathing")
	FVector BreathingSwayLocationAmplitude = FVector(0.0f, 0.0f, 0.8f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Camera|Breathing", meta = (ClampMin = "0.0"))
	float BreathingSwayBlendInTime = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ShowDown|Chat")
	TSubclassOf<UShowDownChatWidget> ChatWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ShowDown|Multiplayer")
	TSubclassOf<UShowDownMultiRankWidget> MultiplayerRankWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Chat")
	FKey ToggleChatKey = EKeys::Enter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Chat")
	FKey CloseChatKey = EKeys::Escape;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Voice")
	bool bEnableVoicePushToTalk = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Voice")
	FKey VoicePushToTalkKey = EKeys::T;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Multiplayer")
	FKey LeaveMatchKey = EKeys::Escape;

	UFUNCTION(Server, Reliable)
	void ServerSubmitSelectedCard(ACard* SelectedCard);

	UFUNCTION(Server, Reliable)
	void ServerSubmitDialogueInput(const FString& Text, const FString& SenderName);

	UFUNCTION(Server, Reliable)
	void ServerPlayerCheck();

	UFUNCTION(Server, Reliable)
	void ServerPlayerRaise();

	UFUNCTION(Server, Reliable)
	void ServerPlayerRaiseTo(int32 BulletCount);

	UFUNCTION(Server, Reliable)
	void ServerPlayerFold();

	UFUNCTION(Server, Reliable)
	void ServerSetMultiplayerDisplayName(const FString& DisplayName);

	UFUNCTION(Server, Reliable)
	void ServerSetMultiplayerVoiceTalking(bool bIsTalking);

	UFUNCTION(Server, Reliable)
	void ServerRequestMultiplayerRestart();

	UFUNCTION(Server, Unreliable)
	void ServerUpdateCharacterHeadLookRotation(FRotator LookRotation);

	UFUNCTION(Client, Reliable)
	void ClientShowStatusMessage(const FString& Message);

	UFUNCTION(Client, Reliable)
	void ClientShowMultiplayerRank(const TArray<FString>& PlayerNames);

	// Restores gameplay input after travelling from the UI-only multiplayer lobby.
	UFUNCTION(Client, Reliable)
	void ClientEnterMultiplayerGameplay();

	// Applies the local multiplayer player's character head camera by zero-based seat index.
	UFUNCTION(Client, Reliable)
	void ClientUseMultiplayerSeatCamera(
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
		float InBreathingSwayBlendInTime);

	UFUNCTION(Client, Reliable)
	void ClientLeaveMultiplayerRoomToHub();

private:
	void InitializeFromPossessedPawn();
	void InitializeInteractableOutlinePostProcess();
	void TraceCardUnderCursor();
	bool TracePrimaryInteraction(FHitResult& OutHit) const;
	bool TraceUnderCursor(FHitResult& OutHit) const;
	bool TraceFromScreenCenter(FHitResult& OutHit) const;
	ACard* ResolveCardFromHit(const FHitResult& Hit) const;
	AActor* ResolveInteractableFromHit(const FHitResult& Hit) const;
	AActor* FindFocusedInteractable() const;
	void UpdateFocusedInteractable();
	void SetFocusedInteractable(AActor* NewFocusedInteractable);
	void ApplyInteractableOutline(AActor* Actor);
	void ClearInteractableOutline();
	void RefreshInteractableOutlineMaterialParameters();
	ACard* FindSelectableCardNearCenterAim() const;
	ACard* FindHoverPreviewCard() const;
	void UpdateHoveredCard();
	void SetHoveredCard(ACard* NewHoveredCard);
	void SelectCard(ACard* SelectedCard);
	void SubmitPlayerBetAction(EShowDownBetAction Action, int32 TargetBet);
	void ApplyPawnCameraInput(float YawInput, float PitchInput);
	AShowDownCharacter* FindLocalCharacterForPlayerCamera() const;
	void UpdateCharacterPlayerCamera(float DeltaTime);
	void UpdateFixedCameraMouseLook(float DeltaTime);
	void SubmitCharacterHeadLookRotation(const FRotator& LookRotation, float DeltaTime);
	void RestoreFixedCameraBaseTransform();
	FRotator GetBreathingSwayRotationOffset(float Strength) const;
	FVector GetBreathingSwayLocationOffset(const FRotator& CameraRotation, float Strength) const;
	FRotator GetCameraSteppedShakeRotationOffset() const;
	FVector GetCameraSteppedShakeLocationOffset(const FRotator& CameraRotation) const;
	void HandleBettingHotkeys();
	void HandleVoicePushToTalkInput();
	void EnsureChatWidget();
	void EnsureLeaveConfirmWidget();
	bool TryApplyPendingMultiplayerSeatCamera();
	bool TryApplyPendingMultiplayerCharacterCamera();
	void RestoreMultiplayerGameplayInput();
	void ApplyChatInputMode(bool bOpen);
	void CreateCenterCrosshairWidget();
	void UpdateCenterCrosshairVisibility();
	void RemoveCenterCrosshairWidget();
	void SubmitLocalMultiplayerDisplayName();
	void TryBindVoiceChatEvents();
	void BroadcastLocalCollectorStatus(bool bSuccess, const FString& Message) const;
	void SetLocalSpeakingIndicatorVisible(bool bVisible);
	void SetSingleOpponentSpeakingIndicatorVisible(bool bVisible) const;
	UFUNCTION()
	void HandleMultiRankRestartRequested();
	UFUNCTION()
	void HandleMultiRankMainMenuRequested();
	UFUNCTION()
	void HandleChatMessageReceived(const FString& SenderName, const FString& Message);
	UFUNCTION()
	void HandleVoiceStatus(bool bSuccess, const FString& Message);
	UFUNCTION()
	void HandleLocalVoiceTalkingChanged(bool bIsTalking);
	UFUNCTION()
	void HandleSpeechPlaybackStateChanged(bool bIsSpeaking);
	AShowDownGameModeBase* ResolveGameMode() const;

	UPROPERTY()
	ACard* HandCard = nullptr;

	UPROPERTY()
	ACard* CurrentSelectedCard = nullptr;

	UPROPERTY()
	ACard* HoveredCard = nullptr;

	UPROPERTY()
	TObjectPtr<AActor> FocusedInteractable = nullptr;

	UPROPERTY()
	UShowDownChatWidget* ChatWidget = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "ShowDown|Input|Interactable Outline")
	TObjectPtr<APostProcessVolume> InteractionOutlinePostProcessVolume;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> InteractionOutlineMID;

	UPROPERTY()
	UShowDownLeaveConfirmWidget* LeaveConfirmWidget = nullptr;

	UPROPERTY()
	UShowDownMultiRankWidget* MultiplayerRankWidget = nullptr;

	UPROPERTY()
	TObjectPtr<USceneComponent> FixedCameraMouseLookTarget = nullptr;

	UPROPERTY()
	TObjectPtr<AShowDownCharacter> LocalPlayerCameraCharacterTarget = nullptr;

	TSharedPtr<SWidget> CenterCrosshairWidget;
	TArray<FSDPrimitiveCustomDepthState> FocusedPrimitiveStates;

	bool bChatOpen = false;
	FString LastSubmittedMultiplayerDisplayName;
	float LastMultiplayerDisplayNameSubmitTime = -1000.0f;
	bool bPendingMultiplayerSeatCamera = false;
	int32 PendingMultiplayerSeatIndex = INDEX_NONE;
	float PendingMultiplayerSeatCameraLookSensitivity = 0.08f;
	float PendingMultiplayerCameraMinPitch = -35.0f;
	float PendingMultiplayerCameraMaxPitch = 35.0f;
	float PendingMultiplayerCameraMinYawOffset = -45.0f;
	float PendingMultiplayerCameraMaxYawOffset = 45.0f;
	bool bPendingMultiplayerCameraInvertMouseY = true;
	bool bPendingMultiplayerCameraBreathingSway = true;
	float PendingMultiplayerCameraBreathingSwaySpeed = 0.38f;
	FRotator PendingMultiplayerCameraBreathingSwayRotationAmplitude = FRotator(0.12f, 0.05f, 0.08f);
	FVector PendingMultiplayerCameraBreathingSwayLocationAmplitude = FVector(0.0f, 0.0f, 0.8f);
	float PendingMultiplayerCameraBreathingSwayBlendInTime = 1.0f;
	FRotator PawnCameraBaseRotation = FRotator::ZeroRotator;
	bool bHasPawnCameraBaseRotation = false;
	FRotator FixedCameraBaseRotation = FRotator::ZeroRotator;
	FRotator FixedCameraLookRotation = FRotator::ZeroRotator;
	FVector FixedCameraBaseLocation = FVector::ZeroVector;
	float FixedCameraLookSensitivity = 0.2f;
	float FixedCameraMinPitch = -35.0f;
	float FixedCameraMaxPitch = 35.0f;
	float FixedCameraMinYawOffset = -45.0f;
	float FixedCameraMaxYawOffset = 45.0f;
	float BreathingSwayElapsedTime = 0.0f;
	float BreathingSwayBlendElapsedTime = 0.0f;
	float CameraSteppedShakeHoldDuration = 0.0f;
	float CameraSteppedShakeBlendOutTime = 0.0f;
	float CameraSteppedShakeElapsedTime = 0.0f;
	float CameraSteppedShakeStepInterval = 0.06f;
	float CameraSteppedShakeSeed = 0.0f;
	FRotator CameraSteppedShakeRotationAmplitude = FRotator::ZeroRotator;
	FVector CameraSteppedShakeLocationAmplitude = FVector::ZeroVector;
	float CharacterPlayerCameraRetryElapsedTime = 0.0f;
	float CharacterHeadLookReplicationElapsedTime = 0.0f;
	FRotator LastSubmittedCharacterHeadLookRotation = FRotator::ZeroRotator;
	bool bFixedCameraInvertMouseY = true;
	bool bVoiceChatEventsBound = false;
	bool bVoiceSubsystemEventsBound = false;
	bool bEosVoiceEventsBound = false;
	TWeakObjectPtr<class AShowDownGameStateBase> VoiceBoundGameState;
};
