#include "ShowDownHubFlowManager.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/Guid.h"
#include "TimerManager.h"
#include "ShowDownGameModeBase.h"
#include "ShowDownGameStateBase.h"
#include "ShowDownCameraAspect.h"
#include "ShowDownEosSubsystem.h"
#include "ShowDownLobbyWidget.h"
#include "ShowDownLoginWidget.h"
#include "ShowDownMainMenuWidget.h"
#include "ShowDownMultiplayerWidget.h"
#include "ShowDownPlayerController.h"
#include "ShowDownRankWidget.h"
#include "ShowDownShopWidget.h"
#include "ShowDownVoiceSubsystem.h"
#include "SupabaseSubsystem.h"

AShowDownHubFlowManager::AShowDownHubFlowManager()
{
	PrimaryActorTick.bCanEverTick = false;
	ShopWidgetClass = UShowDownShopWidget::StaticClass();
	MultiplayerWidgetClass = UShowDownMultiplayerWidget::StaticClass();
	LobbyWidgetClass = UShowDownLobbyWidget::StaticClass();
}

void AShowDownHubFlowManager::BeginPlay()
{
	Super::BeginPlay();
	ApplySinglePlayerVoiceSettings();

	// GameInstanceSubsystem survives level travel, so returning players can skip login.
	bool bHasSession = false;
	bool bInMultiplayerLobby = false;
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (const USupabaseSubsystem* SupabaseSubsystem = GameInstance->GetSubsystem<USupabaseSubsystem>())
		{
			bHasSession = !SupabaseSubsystem->GetAccessToken().IsEmpty();
		}

		if (const UShowDownEosSubsystem* EosSubsystem = GameInstance->GetSubsystem<UShowDownEosSubsystem>())
		{
			bInMultiplayerLobby = EosSubsystem->IsInMultiplayerLobby();
		}
	}

	// 시작 순간엔 폰 스폰 카메라에서 패닝되지 않도록 시작 화면 카메라로 즉시 컷합니다.
	// 이후 ShowLogin/ShowMainMenu의 블렌드는 같은 카메라로의 블렌드라 화면 이동이 보이지 않습니다.
	if (GetNetMode() != NM_Standalone && !bInMultiplayerLobby)
	{
		UE_LOG(LogTemp, Log, TEXT("HubFlowManager disabled on networked gameplay map."));
		return;
	}

#if UE_BUILD_SHIPPING
	const bool bShouldDeveloperAutoStart = false;
#else
	const bool bShouldDeveloperAutoStart =
		bDeveloperAutoStartSinglePlayer
		&& !bInMultiplayerLobby
		&& GetNetMode() == NM_Standalone;
#endif

	PlayCamera(bShouldDeveloperAutoStart ? GameCamera : (bHasSession ? MainMenuCamera : LoginCamera), true);

	// 게임 종료(승/패)를 받아 허브로 복귀하기 위해 GameState 이벤트를 구독합니다.
	if (UWorld* World = GetWorld())
	{
		if (AShowDownGameStateBase* ShowDownGameState = World->GetGameState<AShowDownGameStateBase>())
		{
			ShowDownGameState->OnGameOver.AddDynamic(this, &AShowDownHubFlowManager::HandleGameOver);
		}
	}

	if (bShouldDeveloperAutoStart)
	{
		GetWorldTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &AShowDownHubFlowManager::StartDeveloperSinglePlayPreview));
		return;
	}

	if (bInMultiplayerLobby)
	{
		ShowLobby();
	}
	else if (bHasSession)
	{
		ShowMainMenu();
	}
	else
	{
		ShowLogin();
	}
}

void AShowDownHubFlowManager::ShowLogin()
{
	PlayCamera(LoginCamera);

	if (!LoginWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("LoginWidgetClass is not set on ShowDownHubFlowManager."));
		return;
	}

	LoginWidget = CreateWidget<UShowDownLoginWidget>(
		GetPrimaryPlayerController(),
		LoginWidgetClass
	);

	if (!LoginWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to create LoginWidget."));
		return;
	}

	LoginWidget->SetUseLegacyNavigation(false);
	LoginWidget->OnLoginSucceeded.AddDynamic(this, &AShowDownHubFlowManager::HandleLoginSucceeded);

	SetActiveWidget(LoginWidget);
	SetUiOnlyInput(LoginWidget);
	OnScreenChanged.Broadcast(EShowDownHubFlowScreen::Login);
}

void AShowDownHubFlowManager::ShowMainMenu()
{
	PlayCamera(MainMenuCamera);

	if (!MainMenuWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("MainMenuWidgetClass is not set on ShowDownHubFlowManager."));
		return;
	}

	MainMenuWidget = CreateWidget<UShowDownMainMenuWidget>(
		GetPrimaryPlayerController(),
		MainMenuWidgetClass
	);

	if (!MainMenuWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to create MainMenuWidget."));
		return;
	}

	MainMenuWidget->SetUseLegacyNavigation(false);
	MainMenuWidget->OnSinglePlayRequested.AddDynamic(this, &AShowDownHubFlowManager::HandleSinglePlayRequested);
	MainMenuWidget->OnMultiplayerRequested.AddDynamic(this, &AShowDownHubFlowManager::HandleMultiplayerRequested);
	MainMenuWidget->OnShopRequested.AddDynamic(this, &AShowDownHubFlowManager::HandleShopRequested);
	MainMenuWidget->OnRankingRequested.AddDynamic(this, &AShowDownHubFlowManager::HandleRankingRequested);
	MainMenuWidget->OnQuitRequested.AddDynamic(this, &AShowDownHubFlowManager::HandleQuitRequested);

	SetActiveWidget(MainMenuWidget);
	SetUiOnlyInput(MainMenuWidget);
	OnScreenChanged.Broadcast(EShowDownHubFlowScreen::MainMenu);
}

void AShowDownHubFlowManager::ShowShop()
{
	PlayCamera(ShopCamera);

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (USupabaseSubsystem* SupabaseSubsystem = GameInstance->GetSubsystem<USupabaseSubsystem>())
		{
			// Shop data is loaded on demand instead of during login.
			SupabaseSubsystem->LoadCosmeticData();
		}
	}

	TSubclassOf<UShowDownShopWidget> WidgetClass = ShopWidgetClass;
	if (!WidgetClass)
	{
		WidgetClass = UShowDownShopWidget::StaticClass();
	}

	ShopWidget = CreateWidget<UShowDownShopWidget>(
		GetPrimaryPlayerController(),
		WidgetClass
	);

	if (!ShopWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to create ShopWidget."));
		return;
	}

	ShopWidget->SetUseLegacyBackNavigation(false);
	ShopWidget->OnBackRequested.AddDynamic(this, &AShowDownHubFlowManager::HandleShopBackRequested);

	SetActiveWidget(ShopWidget);
	SetUiOnlyInput(ShopWidget);
	ShopWidget->SetKeyboardFocus();
	OnScreenChanged.Broadcast(EShowDownHubFlowScreen::Shop);
}

void AShowDownHubFlowManager::ShowRanking()
{
	if (!RankWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("RankWidgetClass is not set on ShowDownHubFlowManager."));
		return;
	}

	// 랭킹 전용 카메라가 지정돼 있으면 그쪽으로, 없으면 메뉴 카메라 시점으로 블렌드합니다.
	if (!PlayCamera(RankingCamera))
	{
		PlayCamera(MainMenuCamera);
	}

	RankWidget = CreateWidget<UShowDownRankWidget>(
		GetPrimaryPlayerController(),
		RankWidgetClass
	);

	if (!RankWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to create RankWidget."));
		return;
	}

	// 랭킹 화면의 "뒤로" 버튼을 메인메뉴 복귀에 연결합니다.
	RankWidget->OnBackRequested.AddDynamic(this, &AShowDownHubFlowManager::HandleRankBackRequested);

	SetActiveWidget(RankWidget);
	SetUiOnlyInput(RankWidget);
	OnScreenChanged.Broadcast(EShowDownHubFlowScreen::Ranking);
}

void AShowDownHubFlowManager::ShowMultiplayerMenu()
{
	UE_LOG(LogTemp, Log, TEXT("Showing multiplayer menu."));
	PlayCamera(MainMenuCamera);

	TSubclassOf<UShowDownMultiplayerWidget> WidgetClass = MultiplayerWidgetClass;
	if (!WidgetClass)
	{
		WidgetClass = UShowDownMultiplayerWidget::StaticClass();
	}

	MultiplayerWidget = CreateWidget<UShowDownMultiplayerWidget>(
		GetPrimaryPlayerController(),
		WidgetClass
	);

	if (!MultiplayerWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to create MultiplayerWidget."));
		return;
	}

	MultiplayerWidget->OnHostRequested.AddDynamic(this, &AShowDownHubFlowManager::HandleHostMultiplayerRequested);
	MultiplayerWidget->OnPrivateHostRequested.AddDynamic(this, &AShowDownHubFlowManager::HandleHostPrivateMultiplayerRequested);
	MultiplayerWidget->OnJoinRequested.AddDynamic(this, &AShowDownHubFlowManager::HandleJoinMultiplayerRequested);
	MultiplayerWidget->OnRefreshRoomsRequested.AddDynamic(this, &AShowDownHubFlowManager::HandleRefreshPublicRoomsRequested);
	MultiplayerWidget->OnJoinPublicRoomRequested.AddDynamic(this, &AShowDownHubFlowManager::HandleJoinPublicRoomRequested);
	MultiplayerWidget->OnBackRequested.AddDynamic(this, &AShowDownHubFlowManager::HandleMultiplayerBackRequested);

	SetActiveWidget(MultiplayerWidget);
	SetUiOnlyInput(MultiplayerWidget);
	OnScreenChanged.Broadcast(EShowDownHubFlowScreen::Multiplayer);
	UE_LOG(LogTemp, Log, TEXT("Multiplayer menu added to viewport."));
}

void AShowDownHubFlowManager::ShowLobby()
{
	UE_LOG(LogTemp, Log, TEXT("Showing multiplayer lobby."));
	PlayCamera(MainMenuCamera);

	TSubclassOf<UShowDownLobbyWidget> WidgetClass = LobbyWidgetClass;
	if (!WidgetClass)
	{
		WidgetClass = UShowDownLobbyWidget::StaticClass();
	}

	LobbyWidget = CreateWidget<UShowDownLobbyWidget>(
		GetPrimaryPlayerController(),
		WidgetClass
	);

	if (!LobbyWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to create LobbyWidget."));
		return;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UShowDownEosSubsystem* EosSubsystem = GameInstance->GetSubsystem<UShowDownEosSubsystem>())
		{
			LobbyWidget->SetLobbyInfo(EosSubsystem->GetLobbyCode(), EosSubsystem->IsLobbyHost());
			EosSubsystem->OnSessionResult.RemoveDynamic(this, &AShowDownHubFlowManager::HandleEosSessionResult);
			EosSubsystem->OnSessionResult.AddDynamic(this, &AShowDownHubFlowManager::HandleEosSessionResult);
			EosSubsystem->OnPublicRoomsUpdated.RemoveDynamic(this, &AShowDownHubFlowManager::HandlePublicRoomsUpdated);
			if (EosSubsystem->IsLobbyHost())
			{
				EosSubsystem->StopLobbyStartPolling();
			}
			else
			{
				EosSubsystem->StartLobbyStartPolling();
			}
		}
	}

	LobbyWidget->OnStartRequested.AddDynamic(this, &AShowDownHubFlowManager::HandleLobbyStartRequested);
	LobbyWidget->OnLeaveRequested.AddDynamic(this, &AShowDownHubFlowManager::HandleLobbyLeaveRequested);

	SetActiveWidget(LobbyWidget);
	SetUiOnlyInput(LobbyWidget);
	OnScreenChanged.Broadcast(EShowDownHubFlowScreen::Lobby);
}

void AShowDownHubFlowManager::ShowSinglePlayPreview()
{
	ShowSinglePlayPreviewInternal(true);
}

void AShowDownHubFlowManager::StartDeveloperSinglePlayPreview()
{
	ShowSinglePlayPreviewInternal(!bDeveloperSkipOnlineReward);
}

void AShowDownHubFlowManager::ShowSinglePlayPreviewInternal(bool bAllowOnlineReward)
{
	ApplySinglePlayerVoiceSettings();
	CurrentRewardMatchId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	UE_LOG(LogTemp, Log, TEXT("Single play reward match id: %s"), *CurrentRewardMatchId);
	bCurrentMatchAllowsOnlineReward = false;
	if (bAllowOnlineReward)
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (const USupabaseSubsystem* SupabaseSubsystem = GameInstance->GetSubsystem<USupabaseSubsystem>())
			{
				bCurrentMatchAllowsOnlineReward = !SupabaseSubsystem->GetAccessToken().IsEmpty();
			}
		}
	}

	// 메뉴 UI를 걷어내 카드 클릭/베팅 입력이 폰으로 가도록 합니다.
	SetActiveWidget(nullptr);

	APlayerController* PlayerController = GetPrimaryPlayerController();
	if (AShowDownPlayerController* ShowDownController = Cast<AShowDownPlayerController>(PlayerController))
	{
		ShowDownController->bUseCharacterPlayerCamera = bUseCharacterPlayerCameraForSinglePlay;
		if (bUseCharacterPlayerCameraForSinglePlay)
		{
			float LookSensitivity = GameCameraLookSensitivity;
			float MinPitch = GameCameraMinPitch;
			float MaxPitch = GameCameraMaxPitch;
			float MinYawOffset = GameCameraMinYawOffset;
			float MaxYawOffset = GameCameraMaxYawOffset;
			bool bInvertMouseY = bInvertGameCameraMouseY;
			if (UWorld* World = GetWorld())
			{
				if (const AShowDownGameModeBase* GameMode = World->GetAuthGameMode<AShowDownGameModeBase>())
				{
					LookSensitivity = GameMode->GameplayCameraLookSensitivity;
					MinPitch = GameMode->GameplayCameraMinPitch;
					MaxPitch = GameMode->GameplayCameraMaxPitch;
					MinYawOffset = GameMode->GameplayCameraMinYawOffset;
					MaxYawOffset = GameMode->GameplayCameraMaxYawOffset;
					bInvertMouseY = GameMode->bInvertGameplayCameraMouseY;
				}
			}

			ShowDownController->ClearFixedCameraMouseLook();
			ShowDownController->SetPawnCameraMouseLook(
				LookSensitivity,
				MinPitch,
				MaxPitch,
				MinYawOffset,
				MaxYawOffset,
				bInvertMouseY);
			ShowDownController->bEnablePawnCameraMouseLook = true;
			ShowDownController->bRequireRightMouseForPawnCameraLook = false;
		}
	}

	if (GameCamera && GameCamera->GetCameraComponent())
	{
		ShowDownCameraAspect::ApplyForced16By9(GameCamera);
	}

	const bool bPlayedGameCamera = bUseCharacterPlayerCameraForSinglePlay ? false : PlayCamera(GameCamera);
	if (!bPlayedGameCamera && PlayerController && PlayerController->GetPawn())
	{
		PlayerController->SetViewTargetWithBlend(
			PlayerController->GetPawn(),
			CameraBlendTime,
			VTBlend_EaseInOut,
			CameraBlendEaseExponent);
	}

	// 카드 커서 트레이스·카메라 조작·베팅 핫키가 모두 폰에 전달되도록 게임 입력 모드로 전환합니다.
	if (PlayerController)
	{
		const bool bUseGameCameraLook = bEnableGameCameraMouseLook && bPlayedGameCamera && GameCamera;
		if (AShowDownPlayerController* ShowDownController = Cast<AShowDownPlayerController>(PlayerController))
		{
			ShowDownController->bHandleShowDownGameplayInput = true;
		}

		if (bUseGameCameraLook)
		{
			if (AShowDownPlayerController* ShowDownController = Cast<AShowDownPlayerController>(PlayerController))
			{
				float LookSensitivity = GameCameraLookSensitivity;
				float MinPitch = GameCameraMinPitch;
				float MaxPitch = GameCameraMaxPitch;
				float MinYawOffset = GameCameraMinYawOffset;
				float MaxYawOffset = GameCameraMaxYawOffset;
				bool bInvertMouseY = bInvertGameCameraMouseY;
				bool bEnableBreathingSway = bEnableGameCameraBreathingSway;
				float BreathingSwaySpeed = GameCameraBreathingSwaySpeed;
				FRotator BreathingSwayRotationAmplitude = GameCameraBreathingSwayRotationAmplitude;
				FVector BreathingSwayLocationAmplitude = GameCameraBreathingSwayLocationAmplitude;
				float BreathingSwayBlendInTime = GameCameraBreathingSwayBlendInTime;

				if (UWorld* World = GetWorld())
				{
					if (const AShowDownGameModeBase* GameMode = World->GetAuthGameMode<AShowDownGameModeBase>())
					{
						LookSensitivity = GameMode->GameplayCameraLookSensitivity;
						MinPitch = GameMode->GameplayCameraMinPitch;
						MaxPitch = GameMode->GameplayCameraMaxPitch;
						MinYawOffset = GameMode->GameplayCameraMinYawOffset;
						MaxYawOffset = GameMode->GameplayCameraMaxYawOffset;
						bInvertMouseY = GameMode->bInvertGameplayCameraMouseY;
						bEnableBreathingSway = GameMode->bEnableGameplayCameraBreathingSway;
						BreathingSwaySpeed = GameMode->GameplayCameraBreathingSwaySpeed;
						BreathingSwayRotationAmplitude = GameMode->GameplayCameraBreathingSwayRotationAmplitude;
						BreathingSwayLocationAmplitude = GameMode->GameplayCameraBreathingSwayLocationAmplitude;
						BreathingSwayBlendInTime = GameMode->GameplayCameraBreathingSwayBlendInTime;
					}
				}

				ShowDownController->SetFixedCameraMouseLook(
					GameCamera,
					LookSensitivity,
					MinPitch,
					MaxPitch,
					MinYawOffset,
					MaxYawOffset,
					bInvertMouseY);
				ShowDownController->SetFixedCameraBreathingSway(
					bEnableBreathingSway,
					BreathingSwaySpeed,
					BreathingSwayRotationAmplitude,
					BreathingSwayLocationAmplitude,
					BreathingSwayBlendInTime);
			}

		}
		else
		{
			ClearGameplayCameraLook();
		}

		FInputModeGameOnly InputMode;
		PlayerController->SetInputMode(InputMode);
		PlayerController->bShowMouseCursor = false;
	}

	// 실제 게임 한 판을 시작합니다.
	if (UWorld* World = GetWorld())
	{
		if (AShowDownGameModeBase* GameMode = World->GetAuthGameMode<AShowDownGameModeBase>())
		{
			GameMode->StartSinglePlayer();
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Main level GameMode is not AShowDownGameModeBase. Single play cannot start."));
		}
	}

	OnScreenChanged.Broadcast(EShowDownHubFlowScreen::SinglePlayPreview);
	UE_LOG(LogTemp, Log, TEXT("Single play started from HubFlowManager."));
}

void AShowDownHubFlowManager::ApplySinglePlayerVoiceSettings()
{
	if (UShowDownVoiceSubsystem* VoiceSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UShowDownVoiceSubsystem>()
		: nullptr)
	{
		VoiceSubsystem->TTSPlaybackSpeed = FMath::Clamp(GameCameraVoiceSpeed, 0.5f, 2.0f);
		VoiceSubsystem->TTSPlaybackPitch = FMath::Clamp(GameCameraVoicePitch, 0.5f, 2.0f);
	}
}

void AShowDownHubFlowManager::OpenMultiplayerLevel()
{
	if (MultiplayerLevelName.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("MultiplayerLevelName is not set."));
		return;
	}

	UGameplayStatics::OpenLevel(this, MultiplayerLevelName);
}

void AShowDownHubFlowManager::QuitGame()
{
	if (APlayerController* PlayerController = GetPrimaryPlayerController())
	{
		UKismetSystemLibrary::QuitGame(
			this,
			PlayerController,
			EQuitPreference::Quit,
			false
		);
	}
}

void AShowDownHubFlowManager::SetActiveWidget(UUserWidget* NextWidget)
{
	if (ActiveWidget)
	{
		ActiveWidget->RemoveFromParent();
	}

	ActiveWidget = NextWidget;

	if (ActiveWidget)
	{
		ActiveWidget->AddToViewport();
	}
}

void AShowDownHubFlowManager::SetUiOnlyInput(UUserWidget* FocusWidget)
{
	ClearGameplayCameraLook();

	if (APlayerController* PlayerController = GetPrimaryPlayerController())
	{
		if (AShowDownPlayerController* ShowDownController = Cast<AShowDownPlayerController>(PlayerController))
		{
			ShowDownController->bHandleShowDownGameplayInput = false;
		}

		PlayerController->bShowMouseCursor = true;

		FInputModeUIOnly InputMode;

		if (FocusWidget)
		{
			InputMode.SetWidgetToFocus(FocusWidget->TakeWidget());
		}

		PlayerController->SetInputMode(InputMode);
	}
}

bool AShowDownHubFlowManager::PlayCamera(ACameraActor* Camera, bool bCut)
{
	ShowDownCameraAspect::ApplyForced16By9(Camera);
	return PlayViewTarget(Camera, bCut);
}

bool AShowDownHubFlowManager::PlayViewTarget(AActor* ViewTarget, bool bCut)
{
	if (!ViewTarget)
	{
		return false;
	}

	if (APlayerController* PlayerController = GetPrimaryPlayerController())
	{
		if (ACameraActor* CameraActor = Cast<ACameraActor>(ViewTarget))
		{
			ShowDownCameraAspect::ApplyForced16By9(CameraActor);
		}

		if (bCut || CameraBlendTime <= 0.0f)
		{
			PlayerController->SetViewTarget(ViewTarget);
		}
		else
		{
			PlayerController->SetViewTargetWithBlend(
				ViewTarget,
				CameraBlendTime,
				VTBlend_EaseInOut,
				CameraBlendEaseExponent);
		}
		return true;
	}

	return false;
}

void AShowDownHubFlowManager::ClearGameplayCameraLook()
{
	if (AShowDownPlayerController* ShowDownController = Cast<AShowDownPlayerController>(GetPrimaryPlayerController()))
	{
		ShowDownController->ClearFixedCameraMouseLook();
	}
}

APlayerController* AShowDownHubFlowManager::GetPrimaryPlayerController() const
{
	return UGameplayStatics::GetPlayerController(this, 0);
}

void AShowDownHubFlowManager::HandleLoginSucceeded()
{
	ShowMainMenu();
}

void AShowDownHubFlowManager::HandleSinglePlayRequested()
{
	ShowSinglePlayPreview();
}

void AShowDownHubFlowManager::HandleMultiplayerRequested()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UShowDownEosSubsystem* EosSubsystem = GameInstance->GetSubsystem<UShowDownEosSubsystem>())
		{
			if (EosSubsystem->IsEosLoggedIn())
			{
				if (MainMenuWidget)
				{
					MainMenuWidget->ShowStatusMessage(TEXT("EOS login ready."), FLinearColor::Green);
				}
				ShowMultiplayerMenu();
				return;
			}

			bPendingMultiplayerOpenAfterEosLogin = true;
			EosSubsystem->OnEosLoginResult.RemoveDynamic(this, &AShowDownHubFlowManager::HandleEosLoginForMultiplayer);
			EosSubsystem->OnEosLoginResult.AddDynamic(this, &AShowDownHubFlowManager::HandleEosLoginForMultiplayer);
			if (MainMenuWidget)
			{
				MainMenuWidget->ShowStatusMessage(TEXT("Logging in to EOS..."), FLinearColor::Yellow);
			}
			EosSubsystem->LoginWithSupabaseSession();
			return;
		}
	}

	if (MainMenuWidget)
	{
		MainMenuWidget->ShowStatusMessage(TEXT("EOS subsystem is unavailable."), FLinearColor::Red);
	}
	UE_LOG(LogTemp, Warning, TEXT("EOS subsystem is unavailable. Multiplayer level was not opened."));
}

void AShowDownHubFlowManager::HandleEosLoginForMultiplayer(bool bSuccess, const FString& Message)
{
	UE_LOG(LogTemp, Log, TEXT("EOS login for multiplayer: %s"), *Message);

	if (Message == TEXT("Logging in to EOS..."))
	{
		if (MainMenuWidget)
		{
			MainMenuWidget->ShowStatusMessage(Message, FLinearColor::Yellow);
		}
		return;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UShowDownEosSubsystem* EosSubsystem = GameInstance->GetSubsystem<UShowDownEosSubsystem>())
		{
			EosSubsystem->OnEosLoginResult.RemoveDynamic(this, &AShowDownHubFlowManager::HandleEosLoginForMultiplayer);
		}
	}

	if (!bPendingMultiplayerOpenAfterEosLogin)
	{
		return;
	}

	bPendingMultiplayerOpenAfterEosLogin = false;

	if (bSuccess)
	{
		if (MainMenuWidget)
		{
			MainMenuWidget->ShowStatusMessage(TEXT("EOS login success."), FLinearColor::Green);
		}
		ShowMultiplayerMenu();
		return;
	}

	if (MainMenuWidget)
	{
		MainMenuWidget->ShowStatusMessage(Message, FLinearColor::Red);
	}
}

void AShowDownHubFlowManager::HandleHostMultiplayerRequested()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UShowDownEosSubsystem* EosSubsystem = GameInstance->GetSubsystem<UShowDownEosSubsystem>())
		{
			EosSubsystem->OnSessionResult.RemoveDynamic(this, &AShowDownHubFlowManager::HandleEosSessionResult);
			EosSubsystem->OnSessionResult.AddDynamic(this, &AShowDownHubFlowManager::HandleEosSessionResult);
			EosSubsystem->HostLobby(MultiplayerLobbyLevelName, MultiplayerLevelName);
			return;
		}
	}

	if (MultiplayerWidget)
	{
		MultiplayerWidget->ShowStatusMessage(TEXT("EOS subsystem is unavailable."), FLinearColor::Red);
	}
}

void AShowDownHubFlowManager::HandleHostPrivateMultiplayerRequested()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UShowDownEosSubsystem* EosSubsystem = GameInstance->GetSubsystem<UShowDownEosSubsystem>())
		{
			EosSubsystem->OnSessionResult.RemoveDynamic(this, &AShowDownHubFlowManager::HandleEosSessionResult);
			EosSubsystem->OnSessionResult.AddDynamic(this, &AShowDownHubFlowManager::HandleEosSessionResult);
			EosSubsystem->HostPrivateLobby(MultiplayerLobbyLevelName, MultiplayerLevelName);
			return;
		}
	}

	if (MultiplayerWidget)
	{
		MultiplayerWidget->ShowStatusMessage(TEXT("EOS subsystem is unavailable."), FLinearColor::Red);
	}
}

void AShowDownHubFlowManager::HandleJoinMultiplayerRequested(const FString& RoomCode)
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UShowDownEosSubsystem* EosSubsystem = GameInstance->GetSubsystem<UShowDownEosSubsystem>())
		{
			EosSubsystem->OnSessionResult.RemoveDynamic(this, &AShowDownHubFlowManager::HandleEosSessionResult);
			EosSubsystem->OnSessionResult.AddDynamic(this, &AShowDownHubFlowManager::HandleEosSessionResult);
			EosSubsystem->JoinLobbyByCode(RoomCode);
			return;
		}
	}

	if (MultiplayerWidget)
	{
		MultiplayerWidget->ShowStatusMessage(TEXT("EOS subsystem is unavailable."), FLinearColor::Red);
	}
}

void AShowDownHubFlowManager::HandleRefreshPublicRoomsRequested()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UShowDownEosSubsystem* EosSubsystem = GameInstance->GetSubsystem<UShowDownEosSubsystem>())
		{
			EosSubsystem->OnSessionResult.RemoveDynamic(this, &AShowDownHubFlowManager::HandleEosSessionResult);
			EosSubsystem->OnSessionResult.AddDynamic(this, &AShowDownHubFlowManager::HandleEosSessionResult);
			EosSubsystem->OnPublicRoomsUpdated.RemoveDynamic(this, &AShowDownHubFlowManager::HandlePublicRoomsUpdated);
			EosSubsystem->OnPublicRoomsUpdated.AddDynamic(this, &AShowDownHubFlowManager::HandlePublicRoomsUpdated);
			EosSubsystem->FindPublicLobbies();
			return;
		}
	}

	if (MultiplayerWidget)
	{
		MultiplayerWidget->ShowStatusMessage(TEXT("EOS subsystem is unavailable."), FLinearColor::Red);
		MultiplayerWidget->SetPublicRooms(TArray<FShowDownPublicRoomInfo>());
	}
}

void AShowDownHubFlowManager::HandleJoinPublicRoomRequested(int32 SearchResultIndex)
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UShowDownEosSubsystem* EosSubsystem = GameInstance->GetSubsystem<UShowDownEosSubsystem>())
		{
			EosSubsystem->OnSessionResult.RemoveDynamic(this, &AShowDownHubFlowManager::HandleEosSessionResult);
			EosSubsystem->OnSessionResult.AddDynamic(this, &AShowDownHubFlowManager::HandleEosSessionResult);
			EosSubsystem->JoinPublicLobbyByIndex(SearchResultIndex);
			return;
		}
	}

	if (MultiplayerWidget)
	{
		MultiplayerWidget->ShowStatusMessage(TEXT("EOS subsystem is unavailable."), FLinearColor::Red);
	}
}

void AShowDownHubFlowManager::HandlePublicRoomsUpdated(bool bSuccess, const TArray<FShowDownPublicRoomInfo>& Rooms)
{
	if (MultiplayerWidget)
	{
		MultiplayerWidget->SetPublicRooms(Rooms);
		if (!bSuccess)
		{
			MultiplayerWidget->ShowStatusMessage(TEXT("공개방 목록을 불러오지 못했습니다."), FLinearColor::Red);
		}
	}
}

void AShowDownHubFlowManager::HandleMultiplayerBackRequested()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UShowDownEosSubsystem* EosSubsystem = GameInstance->GetSubsystem<UShowDownEosSubsystem>())
		{
			EosSubsystem->OnSessionResult.RemoveDynamic(this, &AShowDownHubFlowManager::HandleEosSessionResult);
			EosSubsystem->OnPublicRoomsUpdated.RemoveDynamic(this, &AShowDownHubFlowManager::HandlePublicRoomsUpdated);
		}
	}

	ShowMainMenu();
}

void AShowDownHubFlowManager::HandleEosSessionResult(bool bSuccess, const FString& Message)
{
	UE_LOG(LogTemp, Log, TEXT("EOS session result: %s"), *Message);

	if (bSuccess && Message == TEXT("EOS game joined."))
	{
		SetActiveWidget(nullptr);
		LobbyWidget = nullptr;
		MultiplayerWidget = nullptr;

		if (APlayerController* PlayerController = GetPrimaryPlayerController())
		{
			FInputModeGameOnly InputMode;
			PlayerController->SetInputMode(InputMode);
			PlayerController->bShowMouseCursor = false;
			PlayerController->bEnableClickEvents = false;
			PlayerController->bEnableMouseOverEvents = false;

			if (AShowDownPlayerController* ShowDownController = Cast<AShowDownPlayerController>(PlayerController))
			{
				ShowDownController->bHandleShowDownGameplayInput = true;
			}
		}
		return;
	}

	if (MultiplayerWidget)
	{
		MultiplayerWidget->ShowStatusMessage(Message, bSuccess ? FLinearColor::Green : FLinearColor::Yellow);
	}

	if (LobbyWidget)
	{
		LobbyWidget->ShowStatusMessage(Message, bSuccess ? FLinearColor::Green : FLinearColor::Yellow);
	}
}

void AShowDownHubFlowManager::HandleLobbyStartRequested()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UShowDownEosSubsystem* EosSubsystem = GameInstance->GetSubsystem<UShowDownEosSubsystem>())
		{
			EosSubsystem->StartHostedGame();
		}
	}
}

void AShowDownHubFlowManager::HandleLobbyLeaveRequested()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UShowDownEosSubsystem* EosSubsystem = GameInstance->GetSubsystem<UShowDownEosSubsystem>())
		{
			EosSubsystem->LeaveLobby(FName(TEXT("L_ShowdownMain")));
			return;
		}
	}

	ShowMainMenu();
}

void AShowDownHubFlowManager::HandleShopRequested()
{
	ShowShop();
}

void AShowDownHubFlowManager::HandleRankingRequested()
{
	ShowRanking();
}

void AShowDownHubFlowManager::HandleQuitRequested()
{
	QuitGame();
}

void AShowDownHubFlowManager::HandleShopBackRequested()
{
	ShowMainMenu();
}

void AShowDownHubFlowManager::HandleRankBackRequested()
{
	ShowMainMenu();
}

void AShowDownHubFlowManager::HandleGameOver(EShowDownSide Winner)
{
	if (UWorld* World = GetWorld(); World && World->GetNetMode() != NM_Standalone)
	{
		UE_LOG(LogTemp, Log, TEXT("Ignoring hub auto-return for multiplayer game over."));
		World->GetTimerManager().ClearTimer(ReturnToHubTimerHandle);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Game over. Winner: %s. Returning to hub in %.1fs."),
		Winner == EShowDownSide::Player ? TEXT("Player") : TEXT("Collector"),
		ReturnToHubDelay);

	// 플레이어가 모든 스테이지를 클리어해 승리하면 랭크 점수 보상을 지급합니다.
	// 보상 금액(10~50)은 서버의 award_win_reward RPC가 결정하므로 여기서는 호출만 합니다.
	if (Winner == EShowDownSide::Player && bCurrentMatchAllowsOnlineReward)
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (USupabaseSubsystem* SupabaseSubsystem = GameInstance->GetSubsystem<USupabaseSubsystem>())
			{
				SupabaseSubsystem->AwardWinReward(CurrentRewardMatchId);
			}
		}
	}

	// 연출 파트가 결과 카메라/위젯 연출을 붙일 수 있는 훅입니다(블루프린트에서 구현).
	OnGameResultPresentation(Winner);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 대기 시간이 0 이하면 즉시 복귀합니다.
	// (연출 파트가 길이를 직접 제어하려면 ReturnToHubDelay를 충분히 크게 두고
	//  연출이 끝날 때 FinishResultAndReturnToHub()를 호출하면 됩니다.)
	if (ReturnToHubDelay <= 0.0f)
	{
		ReturnToHub();
		return;
	}

	World->GetTimerManager().SetTimer(
		ReturnToHubTimerHandle,
		this,
		&AShowDownHubFlowManager::ReturnToHub,
		ReturnToHubDelay,
		false);
}

void AShowDownHubFlowManager::FinishResultAndReturnToHub()
{
	// 자동 복귀 타이머가 걸려 있으면 취소하고 즉시 복귀합니다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReturnToHubTimerHandle);
	}

	ReturnToHub();
}

void AShowDownHubFlowManager::ReturnToHub()
{
	// 테이블의 카드와 진행 상태를 정리합니다.
	if (UWorld* World = GetWorld())
	{
		if (AShowDownGameModeBase* GameMode = World->GetAuthGameMode<AShowDownGameModeBase>())
		{
			GameMode->ResetForHubReturn();
		}
	}

	// 보상(점수/코인) 반영분을 서버에서 다시 불러와 캐시를 최신으로 맞춥니다.
	// 새로 뜨는 메인메뉴가 OnPlayerDataLoaded를 받아 갱신된 값을 표시합니다.
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (USupabaseSubsystem* SupabaseSubsystem = GameInstance->GetSubsystem<USupabaseSubsystem>())
		{
			SupabaseSubsystem->LoadPlayerData();
		}
	}

	// 카메라/입력/위젯을 메인메뉴 상태로 되돌립니다.
	CurrentRewardMatchId.Empty();
	bCurrentMatchAllowsOnlineReward = false;
	ShowMainMenu();
}
