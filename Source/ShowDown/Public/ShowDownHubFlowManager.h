#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Templates/SubclassOf.h"
#include "Engine/TimerHandle.h"
#include "ShowDownEosSubsystem.h"
#include "ShowDownTypes.h"
#include "ShowDownHubFlowManager.generated.h"

class ACameraActor;
class APlayerController;
class APlayerCameraManager;
class UShowDownLoginWidget;
class UShowDownLobbyWidget;
class UShowDownMainMenuWidget;
class UShowDownMultiplayerWidget;
class UShowDownShopWidget;
class UShowDownRankWidget;
class UShowDownSettingsWidget;
class UShowDownTransitionWidget;
class UUserWidget;
class AShowDownGameStateBase;
class AShowDownShopPreviewActor;
class UShowDownCharacterSkinCatalog;

enum class EShowDownHubTransitionOperation : uint8
{
	None,
	CreateRoom,
	JoinRoom,
	StartGame,
	LeaveRoom
};

UENUM(BlueprintType)
enum class EShowDownHubFlowScreen : uint8
{
	Login,
	MainMenu,
	Shop,
	Ranking,
	Multiplayer,
	Lobby,
	Settings,
	SinglePlayPreview
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnShowDownHubFlowScreenChanged,
	EShowDownHubFlowScreen,
	Screen
);

UCLASS()
class SHOWDOWN_API AShowDownHubFlowManager : public AActor
{
	GENERATED_BODY()

public:
	AShowDownHubFlowManager();

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Flow")
	FOnShowDownHubFlowScreenChanged OnScreenChanged;

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Flow")
	void ShowLogin();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Flow")
	void ShowMainMenu();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Flow")
	void ShowShop();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Flow")
	void ShowRanking();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Flow")
	void ShowMultiplayerMenu();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Flow")
	void ShowLobby();

	void ShowLobbyKickResult(bool bSuccess);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Flow")
	void ShowSettings();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Flow")
	void ShowSinglePlayPreview();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Flow")
	void OpenMultiplayerLevel();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Flow")
	void QuitGame();

	/** Removes only hub-owned UI before gameplay, preserving screen-space world widgets. */
	void PrepareForMultiplayerGameplay();

	/** Returns to the local pawn and reports readiness only after that exact blend completes. */
	void EnsureSinglePlayerGameplayCameraReady();

	// [연출 파트용 훅] 게임이 끝나면(승/패) 호출되는 블루프린트 이벤트입니다.
	// 여기서 결과 카메라 연출, 승/패 결과 위젯, 사운드 등을 재생하면 됩니다.
	// 연출 카메라 컷은 Level Sequence가 담당하고, 인게임 시점은 플레이어 카메라를 사용합니다.
	// 연출이 없거나 짧으면 ReturnToHubDelay 후 자동으로 허브(메인메뉴)로 복귀합니다.
	UFUNCTION(BlueprintImplementableEvent, Category = "ShowDown|Flow")
	void OnGameResultPresentation(EShowDownSide Winner);

	// [연출 파트용] 결과 연출을 끝냈을 때 호출하면 자동 복귀 타이머를 기다리지 않고
	// 즉시 허브로 돌아갑니다. (연출 길이를 직접 제어하고 싶을 때 사용)
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Flow")
	void FinishResultAndReturnToHub();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// Login WBP shown when the player has no active session.
	UPROPERTY(EditAnywhere, Category = "ShowDown|UI")
	TSubclassOf<UShowDownLoginWidget> LoginWidgetClass;

	// Main menu WBP shown after login or when returning to the hub.
	UPROPERTY(EditAnywhere, Category = "ShowDown|UI")
	TSubclassOf<UShowDownMainMenuWidget> MainMenuWidgetClass;

	// Defaults to the C++ demo shop and can be replaced with WBP_Shop later.
	UPROPERTY(EditAnywhere, Category = "ShowDown|UI")
	TSubclassOf<UShowDownShopWidget> ShopWidgetClass;

	// 랭킹(점수 확인) 화면 WBP. 에디터에서 WBP_Rank를 지정합니다.
	UPROPERTY(EditAnywhere, Category = "ShowDown|UI")
	TSubclassOf<UShowDownRankWidget> RankWidgetClass;

	UPROPERTY(EditAnywhere, Category = "ShowDown|UI")
	TSubclassOf<UShowDownMultiplayerWidget> MultiplayerWidgetClass;

	UPROPERTY(EditAnywhere, Category = "ShowDown|UI")
	TSubclassOf<UShowDownLobbyWidget> LobbyWidgetClass;

	UPROPERTY(EditAnywhere, Category = "ShowDown|UI")
	TSubclassOf<UShowDownSettingsWidget> SettingsWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "ShowDown|UI")
	TSubclassOf<UShowDownTransitionWidget> TransitionWidgetClass;

	UPROPERTY(EditAnywhere, Category = "ShowDown|Camera")
	ACameraActor* LoginCamera;

	UPROPERTY(EditAnywhere, Category = "ShowDown|Camera")
	ACameraActor* MainMenuCamera;

	UPROPERTY(EditAnywhere, Category = "ShowDown|Camera")
	ACameraActor* ShopCamera;

	// Optional data asset for future character skins. Robot, hoodman, and micu
	// still resolve in code when this is left empty.
	UPROPERTY(EditDefaultsOnly, Category = "ShowDown|Shop Preview")
	TObjectPtr<UShowDownCharacterSkinCatalog> CharacterSkinCatalog;

	UPROPERTY(EditDefaultsOnly, Category = "ShowDown|Shop Preview")
	TSubclassOf<AShowDownShopPreviewActor> ShopPreviewActorClass;

	UPROPERTY(
		EditAnywhere,
		Category = "ShowDown|Shop Preview",
		meta = (ClampMin = "50.0", UIMin = "100.0", UIMax = "1000.0"))
	float ShopPreviewDistance = 450.0f;

	UPROPERTY(
		EditAnywhere,
		Category = "ShowDown|Shop Preview",
		meta = (UIMin = "-300.0", UIMax = "300.0"))
	float ShopPreviewHeight = -120.0f;

	UPROPERTY(
		EditAnywhere,
		Category = "ShowDown|Shop Preview",
		meta = (UIMin = "-180.0", UIMax = "180.0"))
	float ShopPreviewYawOffset = 0.0f;

	// Camera used by the multiplayer browser and lobby. Falls back to MainMenuCamera.
	UPROPERTY(EditAnywhere, Category = "ShowDown|Camera", meta = (DisplayName = "Multiplayer Camera"))
	ACameraActor* MultiplayerCamera;

	// Camera used by the options/settings screen. Falls back to MainMenuCamera.
	UPROPERTY(EditAnywhere, Category = "ShowDown|Camera", meta = (DisplayName = "Options Camera"))
	ACameraActor* OptionsCamera;

	// 랭킹 화면용 카메라. 비워두면 메뉴 카메라(MainMenuCamera) 시점을 사용합니다.
	UPROPERTY(EditAnywhere, Category = "ShowDown|Camera")
	ACameraActor* RankingCamera;

	UPROPERTY(EditAnywhere, Category = "ShowDown|Camera", meta = (ClampMin = "0.0"))
	float CameraBlendTime = 0.75f;

	UPROPERTY(EditAnywhere, Category = "ShowDown|Camera", meta = (ClampMin = "0.1", UIMin = "1.0", UIMax = "6.0"))
	float CameraBlendEaseExponent = 3.0f;

	UPROPERTY(
		EditAnywhere,
		Category = "ShowDown|Single Player|Voice",
		meta = (DisplayName = "Voice Speed", ClampMin = "0.5", ClampMax = "2.0", UIMin = "0.5", UIMax = "2.0"))
	float SinglePlayerVoiceSpeed = 1.2f;

	UPROPERTY(
		EditAnywhere,
		Category = "ShowDown|Single Player|Voice",
		meta = (DisplayName = "Voice Pitch", ClampMin = "0.5", ClampMax = "2.0", UIMin = "0.5", UIMax = "2.0"))
	float SinglePlayerVoicePitch = 1.25f;

	UPROPERTY(EditAnywhere, Category = "ShowDown|Developer")
	bool bDeveloperAutoStartSinglePlayer = true;

	UPROPERTY(EditAnywhere, Category = "ShowDown|Developer")
	bool bDeveloperSkipOnlineReward = true;

	// Lobby and gameplay now travel through the same authored main level.
	UPROPERTY(EditAnywhere, Category = "ShowDown|Level")
	FName MultiplayerLobbyLevelName = TEXT("L_MultiplayerLobby");

	// Same level reloaded as a listen/server-travel target after the lobby host starts the match.
	UPROPERTY(EditAnywhere, Category = "ShowDown|Level")
	FName MultiplayerLevelName = TEXT("L_MultiplayerGame");

	// 게임 종료(승/패) 후 메인메뉴로 돌아가기까지의 대기 시간(초). 결과를 잠시 보여주기 위함.
	UPROPERTY(EditAnywhere, Category = "ShowDown|Flow")
	float ReturnToHubDelay = 4.0f;

	FTimerHandle ReturnToHubTimerHandle;
	FString CurrentRewardMatchId;
	bool bPendingMultiplayerOpenAfterEosLogin = false;
	bool bCurrentMatchAllowsOnlineReward = false;
	TWeakObjectPtr<AShowDownGameStateBase> BoundGameState;

	UPROPERTY()
	UShowDownLoginWidget* LoginWidget;

	UPROPERTY()
	UShowDownMainMenuWidget* MainMenuWidget;

	UPROPERTY()
	UShowDownShopWidget* ShopWidget;

	UPROPERTY()
	UShowDownRankWidget* RankWidget;

	UPROPERTY()
	UShowDownMultiplayerWidget* MultiplayerWidget;

	UPROPERTY()
	UShowDownLobbyWidget* LobbyWidget;

	UPROPERTY()
	UShowDownSettingsWidget* SettingsWidget;

	UPROPERTY()
	UUserWidget* ActiveWidget;

	UPROPERTY(Transient)
	TObjectPtr<UShowDownTransitionWidget> TransitionWidget;

	UPROPERTY(Transient)
	TObjectPtr<AShowDownShopPreviewActor> ShopPreviewActor;

	EShowDownHubTransitionOperation TransitionOperation = EShowDownHubTransitionOperation::None;
	TWeakObjectPtr<APlayerController> SinglePlayerBlendPlayerController;
	TWeakObjectPtr<APlayerCameraManager> SinglePlayerBlendCameraManager;
	TWeakObjectPtr<AActor> SinglePlayerBlendViewTarget;
	FDelegateHandle SinglePlayerBlendCompleteHandle;
	FTimerHandle SinglePlayerBlendFallbackTimerHandle;
	bool bRetrySinglePlayerGameplayCameraUntilReady = false;

	void SetActiveWidget(UUserWidget* NextWidget);
	void ShowTransitionOverlay(
		EShowDownHubTransitionOperation Operation,
		const FString& Title,
		const FString& Detail);
	void HideTransitionOverlay();
	void BindTopNavigation(UUserWidget* Widget);
	void SetUiOnlyInput(UUserWidget* FocusWidget);

	UFUNCTION() void HandleTopNavSinglePlay();
	UFUNCTION() void HandleTopNavMultiplayer();
	UFUNCTION() void HandleTopNavShop();
	UFUNCTION() void HandleTopNavRanking();
	UFUNCTION() void HandleTopNavSettings();
	void StartDeveloperSinglePlayPreview();
	void ShowSinglePlayPreviewInternal(bool bAllowOnlineReward);
	void ArmSinglePlayerCameraBlendCompletion(APlayerController* PlayerController, AActor* ViewTarget);
	void HandleSinglePlayerCameraBlendComplete();
	void HandleSinglePlayerCameraBlendFallback();
	void ClearSinglePlayerCameraBlendCompletion();
	void ApplySinglePlayerVoiceSettings();
	bool PlayCamera(ACameraActor* Camera, bool bCut = false);
	bool PlayViewTarget(AActor* ViewTarget, bool bCut = false);
	void ClearGameplayCameraLook();
	APlayerController* GetPrimaryPlayerController() const;
	void SpawnShopPreviewActor();
	void DestroyShopPreviewActor();

	UFUNCTION()
	void HandleShopPreviewSkinChanged(const FString& SkinId);

	UFUNCTION()
	void HandleLoginSucceeded();

	UFUNCTION()
	void HandleSinglePlayRequested();

	UFUNCTION()
	void HandleMultiplayerRequested();

	UFUNCTION()
	void HandleEosLoginForMultiplayer(bool bSuccess, const FString& Message);

	UFUNCTION()
	void HandleHostMultiplayerRequested(const FString& RoomName);

	UFUNCTION()
	void HandleHostPrivateMultiplayerRequested(const FString& RoomName);

	UFUNCTION()
	void HandleJoinMultiplayerRequested(const FString& RoomCode);

	UFUNCTION()
	void HandleRefreshPublicRoomsRequested();

	UFUNCTION()
	void HandleJoinPublicRoomRequested(int32 SearchResultIndex);

	UFUNCTION()
	void HandlePublicRoomsUpdated(bool bSuccess, const TArray<FShowDownPublicRoomInfo>& Rooms);

	UFUNCTION()
	void HandleMultiplayerBackRequested();

	UFUNCTION()
	void HandleEosSessionResult(bool bSuccess, const FString& Message);

	UFUNCTION()
	void HandleManagedTravelFailed(const FString& Message, bool bReopenMultiplayerMenu);

	UFUNCTION()
	void HandleLobbyStartRequested();

	UFUNCTION()
	void HandleSettingsBackRequested();

	UFUNCTION()
	void HandleSettingsQuitRequested();

	UFUNCTION()
	void HandleLobbyLeaveRequested();

	UFUNCTION()
	void HandleLobbyKickRequested(const FString& PlayerId);

	UFUNCTION()
	void HandleShopRequested();

	UFUNCTION()
	void HandleRankingRequested();

	UFUNCTION()
	void HandleQuitRequested();

	UFUNCTION()
	void HandleShopBackRequested();

	UFUNCTION()
	void HandleRankBackRequested();

	// 게임이 끝나면(승/패) 호출됩니다. 결과를 잠시 보여준 뒤 허브로 복귀시킵니다.
	UFUNCTION()
	void HandleGameOver(EShowDownSide Winner);

	// 게임판을 정리하고 메인메뉴로 돌아갑니다.
	void ReturnToHub();
};
