#include "ShowDownEosSubsystem.h"

#include "Engine/Engine.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "ShowDownGameModeBase.h"
#include "ShowDownGameStateBase.h"
#include "ShowDownPlayerController.h"
#include "SupabaseSubsystem.h"

namespace
{
constexpr int32 LocalUserNum = 0;
const FName ShowDownSessionName = NAME_GameSession;
const FName ShowDownRoomCodeKey = TEXT("SHOWDOWN_ROOM_CODE");
const FName ShowDownRoomNameKey = TEXT("SHOWDOWN_ROOM_NAME");
const FName ShowDownRoomPublicKey = TEXT("SHOWDOWN_ROOM_PUBLIC");
const FName ShowDownGameStartedKey = TEXT("SHOWDOWN_GAME_STARTED");
const FName ShowDownGameMapKey = TEXT("SHOWDOWN_GAME_MAP");
const FName ShowDownExpectedPlayerCountKey = TEXT("SHOWDOWN_EXPECTED_PLAYER_COUNT");
const FString EosOpenIdCredentialType = TEXT("externalauth:OpenIdAccessToken");

FString MakeRoomCode()
{
	const uint32 CodeSeed = FGuid::NewGuid().A % 1000000;
	return FString::Printf(TEXT("%06u"), CodeSeed);
}

constexpr int32 ShowDownMaxLobbyPlayers = 4;

FString MakeDefaultRoomName(const USupabaseSubsystem* SupabaseSubsystem, const FString& RoomCode)
{
	const FString Nickname = SupabaseSubsystem ? SupabaseSubsystem->GetNickname().TrimStartAndEnd() : TEXT("");
	return Nickname.IsEmpty()
		? FString::Printf(TEXT("Room %s"), *RoomCode)
		: FString::Printf(TEXT("%s's Room"), *Nickname.Left(24));
}
}

void UShowDownEosSubsystem::Deinitialize()
{
	StopLobbyStartPolling();
	ClearOnlineDelegateHandles();

	ClearTransientSearchState(true);
	PendingJoinCode.Empty();
	LobbyCode.Empty();
	PendingSessionFlow = ESessionFlow::None;
	bLobbyStartPollInFlight = false;
	bInMultiplayerLobby = false;
	bLobbyHost = false;

	Super::Deinitialize();
}

IOnlineSubsystem* UShowDownEosSubsystem::GetEosSubsystem() const
{
	return IOnlineSubsystem::Get(FName(TEXT("EOS")));
}

IOnlineIdentityPtr UShowDownEosSubsystem::GetIdentityInterface() const
{
	if (IOnlineSubsystem* OnlineSubsystem = GetEosSubsystem())
	{
		return OnlineSubsystem->GetIdentityInterface();
	}

	return nullptr;
}

IOnlineSessionPtr UShowDownEosSubsystem::GetSessionInterface() const
{
	if (IOnlineSubsystem* OnlineSubsystem = GetEosSubsystem())
	{
		return OnlineSubsystem->GetSessionInterface();
	}

	return nullptr;
}

void UShowDownEosSubsystem::ClearOnlineDelegateHandles()
{
	const IOnlineIdentityPtr IdentityInterface = GetIdentityInterface();
	if (IdentityInterface.IsValid() && LoginCompleteDelegateHandle.IsValid())
	{
		IdentityInterface->ClearOnLoginCompleteDelegate_Handle(LocalUserNum, LoginCompleteDelegateHandle);
		LoginCompleteDelegateHandle.Reset();
	}

	const IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		CreateSessionCompleteDelegateHandle.Reset();
		UpdateSessionCompleteDelegateHandle.Reset();
		DestroySessionCompleteDelegateHandle.Reset();
		FindSessionsCompleteDelegateHandle.Reset();
		JoinSessionCompleteDelegateHandle.Reset();
		return;
	}

	if (CreateSessionCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		CreateSessionCompleteDelegateHandle.Reset();
	}
	if (UpdateSessionCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionCompleteDelegateHandle);
		UpdateSessionCompleteDelegateHandle.Reset();
	}
	if (DestroySessionCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		DestroySessionCompleteDelegateHandle.Reset();
	}
	if (FindSessionsCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		FindSessionsCompleteDelegateHandle.Reset();
	}
	if (JoinSessionCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		JoinSessionCompleteDelegateHandle.Reset();
	}
}

void UShowDownEosSubsystem::ClearTransientSearchState(bool bClearPublicRooms)
{
	SessionSearch.Reset();
	PendingStartedGameSearchResult = FOnlineSessionSearchResult();
	if (bClearPublicRooms)
	{
		PublicLobbySearchResults.Reset();
	}
}

bool UShowDownEosSubsystem::IsEosLoggedIn() const
{
	const IOnlineIdentityPtr IdentityInterface = GetIdentityInterface();
	return IdentityInterface.IsValid() &&
		IdentityInterface->GetLoginStatus(LocalUserNum) == ELoginStatus::LoggedIn;
}

bool UShowDownEosSubsystem::IsInMultiplayerLobby() const
{
	return bInMultiplayerLobby;
}

bool UShowDownEosSubsystem::IsLobbyHost() const
{
	return bLobbyHost;
}

FString UShowDownEosSubsystem::GetLobbyCode() const
{
	return LobbyCode;
}

void UShowDownEosSubsystem::LoginWithSupabaseSession()
{
	if (IsEosLoggedIn())
	{
		OnEosLoginResult.Broadcast(true, TEXT("EOS login already active."));
		return;
	}

	USupabaseSubsystem* SupabaseSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<USupabaseSubsystem>()
		: nullptr;

	if (!SupabaseSubsystem || SupabaseSubsystem->GetAccessToken().IsEmpty())
	{
		OnEosLoginResult.Broadcast(false, TEXT("Supabase login is required before EOS login."));
		return;
	}

	const IOnlineIdentityPtr IdentityInterface = GetIdentityInterface();
	if (!IdentityInterface.IsValid())
	{
		OnEosLoginResult.Broadcast(false, TEXT("EOS identity interface is unavailable."));
		return;
	}

	if (LoginCompleteDelegateHandle.IsValid())
	{
		IdentityInterface->ClearOnLoginCompleteDelegate_Handle(LocalUserNum, LoginCompleteDelegateHandle);
		LoginCompleteDelegateHandle.Reset();
	}

	LoginCompleteDelegateHandle = IdentityInterface->AddOnLoginCompleteDelegate_Handle(
		LocalUserNum,
		FOnLoginCompleteDelegate::CreateUObject(this, &UShowDownEosSubsystem::HandleLoginComplete)
	);

	FOnlineAccountCredentials Credentials;
	Credentials.Type = EosOpenIdCredentialType;
	Credentials.Id = SupabaseSubsystem->GetUserId();
	Credentials.Token = SupabaseSubsystem->GetAccessToken();

	OnEosLoginResult.Broadcast(false, TEXT("Logging in to EOS..."));

	if (!IdentityInterface->Login(LocalUserNum, Credentials))
	{
		IdentityInterface->ClearOnLoginCompleteDelegate_Handle(LocalUserNum, LoginCompleteDelegateHandle);
		LoginCompleteDelegateHandle.Reset();
		OnEosLoginResult.Broadcast(false, TEXT("EOS login request could not start."));
	}
}

void UShowDownEosSubsystem::HostSession(FName MapName)
{
	if (!IsEosLoggedIn())
	{
		OnSessionResult.Broadcast(false, TEXT("EOS login is required before hosting."));
		LoginWithSupabaseSession();
		return;
	}

	const IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		OnSessionResult.Broadcast(false, TEXT("EOS session interface is unavailable."));
		return;
	}

	if (SessionInterface->GetNamedSession(ShowDownSessionName))
	{
		PendingSessionFlow = ESessionFlow::None;
		OnSessionResult.Broadcast(false, TEXT("An EOS session already exists. Destroy it before hosting again."));
		return;
	}

	PendingHostMapName = MapName.IsNone() ? FName(TEXT("L_MultiplayerGame")) : MapName;
	PendingSessionFlow = ESessionFlow::HostImmediateGame;

	if (CreateSessionCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		CreateSessionCompleteDelegateHandle.Reset();
	}

	CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UShowDownEosSubsystem::HandleCreateSessionComplete)
	);

	FOnlineSessionSettings Settings;
	Settings.bIsLANMatch = false;
	Settings.NumPublicConnections = ShowDownMaxLobbyPlayers;
	Settings.NumPrivateConnections = 0;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bUsesPresence = true;
	Settings.bAllowJoinViaPresence = true;
	Settings.bUseLobbiesIfAvailable = true;
	Settings.Set(SETTING_MAPNAME, PendingHostMapName.ToString(), EOnlineDataAdvertisementType::ViaOnlineService);

	OnSessionResult.Broadcast(false, TEXT("Creating EOS session..."));

	if (!SessionInterface->CreateSession(LocalUserNum, ShowDownSessionName, Settings))
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		CreateSessionCompleteDelegateHandle.Reset();
		PendingSessionFlow = ESessionFlow::None;
		ClearTransientSearchState(true);
		OnSessionResult.Broadcast(false, TEXT("EOS session creation could not start."));
	}
}

void UShowDownEosSubsystem::HostLobby(FName LobbyMapName, FName GameMapName)
{
	HostLobbyWithVisibility(LobbyMapName, GameMapName, true);
}

void UShowDownEosSubsystem::HostPrivateLobby(FName LobbyMapName, FName GameMapName)
{
	HostLobbyWithVisibility(LobbyMapName, GameMapName, false);
}

void UShowDownEosSubsystem::HostLobbyWithVisibility(FName LobbyMapName, FName GameMapName, bool bPublicRoom)
{
	if (!IsEosLoggedIn())
	{
		OnSessionResult.Broadcast(false, TEXT("EOS login is required before hosting."));
		LoginWithSupabaseSession();
		return;
	}

	const IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		OnSessionResult.Broadcast(false, TEXT("EOS session interface is unavailable."));
		return;
	}

	if (SessionInterface->GetNamedSession(ShowDownSessionName))
	{
		if (DestroySessionCompleteDelegateHandle.IsValid())
		{
			OnSessionResult.Broadcast(false, TEXT("이전 방 정리 중입니다. 잠시만 기다려주세요."));
			return;
		}

		PendingHostMapName = LobbyMapName.IsNone() ? FName(TEXT("L_MultiplayerLobby")) : LobbyMapName;
		PendingGameMapName = GameMapName.IsNone() ? FName(TEXT("L_MultiplayerGame")) : GameMapName;
		bPendingLobbyIsPublic = bPublicRoom;
		PendingJoinCode.Empty();
		LobbyCode.Empty();
		bLobbyHost = false;
		bInMultiplayerLobby = false;
		PendingSessionFlow = ESessionFlow::CleanupBeforeHostLobby;
		DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateUObject(this, &UShowDownEosSubsystem::HandleDestroySessionComplete)
		);

		OnSessionResult.Broadcast(false, TEXT("이전 방 정보를 정리한 뒤 새 방을 만듭니다..."));
		if (!SessionInterface->DestroySession(ShowDownSessionName))
		{
			SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
			DestroySessionCompleteDelegateHandle.Reset();
			PendingSessionFlow = ESessionFlow::None;
			PendingJoinCode.Empty();
			LobbyCode.Empty();
			ClearTransientSearchState(true);
			OnSessionResult.Broadcast(false, TEXT("이전 방 정리에 실패했습니다."));
		}
		return;
	}

	PendingHostMapName = LobbyMapName.IsNone() ? FName(TEXT("L_MultiplayerLobby")) : LobbyMapName;
	PendingGameMapName = GameMapName.IsNone() ? FName(TEXT("L_MultiplayerGame")) : GameMapName;
	bPendingLobbyIsPublic = bPublicRoom;
	LobbyCode = MakeRoomCode();
	USupabaseSubsystem* SupabaseSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<USupabaseSubsystem>()
		: nullptr;
	const FString RoomName = MakeDefaultRoomName(SupabaseSubsystem, LobbyCode);
	PendingJoinCode.Empty();
	bLobbyHost = true;
	bInMultiplayerLobby = true;
	ExpectedLobbyPlayerCount = ShowDownMaxLobbyPlayers;
	PendingSessionFlow = ESessionFlow::HostLobby;

	if (CreateSessionCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		CreateSessionCompleteDelegateHandle.Reset();
	}

	CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UShowDownEosSubsystem::HandleCreateSessionComplete)
	);

	FOnlineSessionSettings Settings;
	Settings.bIsLANMatch = false;
	Settings.NumPublicConnections = ShowDownMaxLobbyPlayers;
	Settings.NumPrivateConnections = 0;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bUsesPresence = true;
	Settings.bAllowJoinViaPresence = true;
	Settings.bUseLobbiesIfAvailable = true;
	Settings.Set(SETTING_MAPNAME, PendingHostMapName.ToString(), EOnlineDataAdvertisementType::ViaOnlineService);
	Settings.Set(ShowDownRoomCodeKey, LobbyCode, EOnlineDataAdvertisementType::ViaOnlineService);
	Settings.Set(ShowDownRoomNameKey, RoomName, EOnlineDataAdvertisementType::ViaOnlineService);
	Settings.Set(ShowDownRoomPublicKey, bPendingLobbyIsPublic, EOnlineDataAdvertisementType::ViaOnlineService);
	Settings.Set(ShowDownGameStartedKey, false, EOnlineDataAdvertisementType::ViaOnlineService);
	Settings.Set(ShowDownGameMapKey, PendingGameMapName.ToString(), EOnlineDataAdvertisementType::ViaOnlineService);
	Settings.Set(ShowDownExpectedPlayerCountKey, ExpectedLobbyPlayerCount, EOnlineDataAdvertisementType::ViaOnlineService);

	OnSessionResult.Broadcast(false, FString::Printf(
		TEXT("%s %s 생성 중..."),
		bPendingLobbyIsPublic ? TEXT("공개방") : TEXT("비공개방"),
		*LobbyCode));

	if (!SessionInterface->CreateSession(LocalUserNum, ShowDownSessionName, Settings))
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		CreateSessionCompleteDelegateHandle.Reset();
		PendingSessionFlow = ESessionFlow::None;
		ClearTransientSearchState(true);
		bInMultiplayerLobby = false;
		bLobbyHost = false;
		LobbyCode.Empty();
		OnSessionResult.Broadcast(false, TEXT("EOS lobby creation could not start."));
	}
}

void UShowDownEosSubsystem::JoinLobbyByCode(const FString& RoomCode)
{
	if (!IsEosLoggedIn())
	{
		OnSessionResult.Broadcast(false, TEXT("EOS login is required before joining."));
		LoginWithSupabaseSession();
		return;
	}

	const FString NormalizedCode = RoomCode.TrimStartAndEnd().ToUpper();
	if (NormalizedCode.IsEmpty())
	{
		OnSessionResult.Broadcast(false, TEXT("Enter a room code."));
		return;
	}

	const IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		OnSessionResult.Broadcast(false, TEXT("EOS session interface is unavailable."));
		return;
	}

	if (SessionInterface->GetNamedSession(ShowDownSessionName))
	{
		if (DestroySessionCompleteDelegateHandle.IsValid())
		{
			OnSessionResult.Broadcast(false, TEXT("이전 방 정리 중입니다. 잠시만 기다려주세요."));
			return;
		}

		PendingJoinCode = NormalizedCode;
		LobbyCode = NormalizedCode;
		bLobbyHost = false;
		bInMultiplayerLobby = false;
		PendingSessionFlow = ESessionFlow::CleanupBeforeJoinLobby;
		DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateUObject(this, &UShowDownEosSubsystem::HandleDestroySessionComplete)
		);

		OnSessionResult.Broadcast(false, TEXT("이전 방 정보를 정리한 뒤 입장합니다..."));
		if (!SessionInterface->DestroySession(ShowDownSessionName))
		{
			SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
			DestroySessionCompleteDelegateHandle.Reset();
			PendingSessionFlow = ESessionFlow::None;
			PendingJoinCode.Empty();
			LobbyCode.Empty();
			ClearTransientSearchState(false);
			OnSessionResult.Broadcast(false, TEXT("이전 방 정리에 실패했습니다."));
		}
		return;
	}

	PendingJoinCode = NormalizedCode;
	LobbyCode = NormalizedCode;
	bLobbyHost = false;
	PendingSessionFlow = ESessionFlow::JoinLobby;

	if (FindSessionsCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		FindSessionsCompleteDelegateHandle.Reset();
	}

	FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UShowDownEosSubsystem::HandleFindSessionsComplete)
	);

	SessionSearch = MakeShared<FOnlineSessionSearch>();
	SessionSearch->MaxSearchResults = 100;
	SessionSearch->bIsLanQuery = false;
	SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	SessionSearch->QuerySettings.Set(ShowDownRoomCodeKey, NormalizedCode, EOnlineComparisonOp::Equals);

	OnSessionResult.Broadcast(false, FString::Printf(TEXT("Searching room %s..."), *NormalizedCode));

	if (!SessionInterface->FindSessions(LocalUserNum, SessionSearch.ToSharedRef()))
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		FindSessionsCompleteDelegateHandle.Reset();
		PendingSessionFlow = ESessionFlow::None;
		ClearTransientSearchState(false);
		OnSessionResult.Broadcast(false, TEXT("EOS lobby search could not start."));
	}
}

void UShowDownEosSubsystem::FindPublicLobbies()
{
	if (!IsEosLoggedIn())
	{
		ClearTransientSearchState(true);
		OnSessionResult.Broadcast(false, TEXT("EOS login is required before browsing rooms."));
		OnPublicRoomsUpdated.Broadcast(false, TArray<FShowDownPublicRoomInfo>());
		LoginWithSupabaseSession();
		return;
	}

	const IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		ClearTransientSearchState(true);
		OnSessionResult.Broadcast(false, TEXT("EOS session interface is unavailable."));
		OnPublicRoomsUpdated.Broadcast(false, TArray<FShowDownPublicRoomInfo>());
		return;
	}

	if (FindSessionsCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		FindSessionsCompleteDelegateHandle.Reset();
	}

	FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UShowDownEosSubsystem::HandleFindSessionsComplete)
	);

	PendingSessionFlow = ESessionFlow::BrowsePublicLobbies;
	PendingJoinCode.Empty();
	PublicLobbySearchResults.Reset();

	SessionSearch = MakeShared<FOnlineSessionSearch>();
	SessionSearch->MaxSearchResults = 100;
	SessionSearch->bIsLanQuery = false;
	SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	SessionSearch->QuerySettings.Set(ShowDownRoomPublicKey, true, EOnlineComparisonOp::Equals);

	OnSessionResult.Broadcast(false, TEXT("공개방 목록을 불러오는 중..."));

	if (!SessionInterface->FindSessions(LocalUserNum, SessionSearch.ToSharedRef()))
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		FindSessionsCompleteDelegateHandle.Reset();
		PendingSessionFlow = ESessionFlow::None;
		ClearTransientSearchState(true);
		OnSessionResult.Broadcast(false, TEXT("공개방 검색을 시작하지 못했습니다."));
		OnPublicRoomsUpdated.Broadcast(false, TArray<FShowDownPublicRoomInfo>());
	}
}

void UShowDownEosSubsystem::JoinPublicLobbyByIndex(int32 SearchResultIndex)
{
	if (!PublicLobbySearchResults.IsValidIndex(SearchResultIndex))
	{
		OnSessionResult.Broadcast(false, TEXT("선택한 공개방 정보를 찾을 수 없습니다. 목록을 새로고침하세요."));
		return;
	}

	FString RoomCodeToJoin;
	PublicLobbySearchResults[SearchResultIndex].Session.SessionSettings.Get(ShowDownRoomCodeKey, RoomCodeToJoin);
	if (RoomCodeToJoin.IsEmpty())
	{
		OnSessionResult.Broadcast(false, TEXT("선택한 공개방의 방 코드가 비어 있습니다."));
		return;
	}

	PendingJoinCode = RoomCodeToJoin;
	LobbyCode = RoomCodeToJoin;
	bLobbyHost = false;
	PendingSessionFlow = ESessionFlow::JoinLobby;

	const IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		PendingSessionFlow = ESessionFlow::None;
		PendingJoinCode.Empty();
		LobbyCode.Empty();
		OnSessionResult.Broadcast(false, TEXT("EOS session interface is unavailable."));
		return;
	}

	if (SessionInterface->GetNamedSession(ShowDownSessionName))
	{
		JoinLobbyByCode(RoomCodeToJoin);
		return;
	}

	if (JoinSessionCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		JoinSessionCompleteDelegateHandle.Reset();
	}

	JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UShowDownEosSubsystem::HandleJoinSessionComplete)
	);

	const FOnlineSessionSearchResult& SearchResult = PublicLobbySearchResults[SearchResultIndex];
	if (SearchResult.Session.NumOpenPublicConnections <= 0)
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		JoinSessionCompleteDelegateHandle.Reset();
		PendingSessionFlow = ESessionFlow::None;
		ClearTransientSearchState(false);
		PendingJoinCode.Empty();
		LobbyCode.Empty();
		OnSessionResult.Broadcast(false, TEXT("방이 가득 찼습니다. 목록을 새로고침하세요."));
		return;
	}

	OnSessionResult.Broadcast(false, FString::Printf(TEXT("공개방 %s 입장 요청 중..."), *RoomCodeToJoin));
	if (!SessionInterface->JoinSession(LocalUserNum, ShowDownSessionName, SearchResult))
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		JoinSessionCompleteDelegateHandle.Reset();
		PendingSessionFlow = ESessionFlow::None;
		ClearTransientSearchState(false);
		PendingJoinCode.Empty();
		LobbyCode.Empty();
		OnSessionResult.Broadcast(false, TEXT("공개방 입장 요청을 시작하지 못했습니다."));
	}
}

void UShowDownEosSubsystem::StartHostedGame()
{
	if (!bInMultiplayerLobby || !bLobbyHost)
	{
		OnSessionResult.Broadcast(false, TEXT("Only the room host can start the game."));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		OnSessionResult.Broadcast(false, TEXT("World is unavailable."));
		return;
	}

	AGameModeBase* GameMode = World->GetAuthGameMode();
	if (!GameMode)
	{
		OnSessionResult.Broadcast(false, TEXT("Only the listen server host can start the game."));
		return;
	}

	if (AShowDownGameModeBase* ShowDownGameMode = Cast<AShowDownGameModeBase>(GameMode))
	{
		ShowDownGameMode->RefreshMultiplayerLobbyPlayers();
	}

	int32 PlayerControllerCount = 0;
	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		if (const APlayerController* PlayerController = Iterator->Get(); PlayerController && PlayerController->Player)
		{
			++PlayerControllerCount;
		}
	}
	const int32 LobbySlotCount = World->GetGameState<AShowDownGameStateBase>()
		? World->GetGameState<AShowDownGameStateBase>()->PlayerSlots.Num()
		: 0;

	UE_LOG(
		LogTemp,
		Log,
		TEXT("Starting hosted game. NetMode=%d, PlayerControllers=%d, LobbySlots=%d, TargetMap=%s"),
		static_cast<int32>(World->GetNetMode()),
		PlayerControllerCount,
		LobbySlotCount,
		*PendingGameMapName.ToString()
	);

	const IOnlineSessionPtr SessionInterface = GetSessionInterface();
	FNamedOnlineSession* NamedSession = SessionInterface.IsValid()
		? SessionInterface->GetNamedSession(ShowDownSessionName)
		: nullptr;

	if (!SessionInterface.IsValid() || !NamedSession)
	{
		ExpectedLobbyPlayerCount = FMath::Clamp(LobbySlotCount > 0 ? LobbySlotCount : PlayerControllerCount, 2, 4);
		OnSessionResult.Broadcast(true, TEXT("Starting game without EOS session update..."));
		TravelHostedGame();
		return;
	}

	ExpectedLobbyPlayerCount = FMath::Clamp(LobbySlotCount > 0 ? LobbySlotCount : PlayerControllerCount, 2, 4);

	if (UpdateSessionCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionCompleteDelegateHandle);
		UpdateSessionCompleteDelegateHandle.Reset();
	}

	UpdateSessionCompleteDelegateHandle = SessionInterface->AddOnUpdateSessionCompleteDelegate_Handle(
		FOnUpdateSessionCompleteDelegate::CreateUObject(this, &UShowDownEosSubsystem::HandleUpdateSessionComplete)
	);

	FOnlineSessionSettings UpdatedSettings = NamedSession->SessionSettings;
	UpdatedSettings.bAllowJoinInProgress = true;
	UpdatedSettings.Set(SETTING_MAPNAME, PendingGameMapName.ToString(), EOnlineDataAdvertisementType::ViaOnlineService);
	UpdatedSettings.Set(ShowDownGameStartedKey, true, EOnlineDataAdvertisementType::ViaOnlineService);
	UpdatedSettings.Set(ShowDownGameMapKey, PendingGameMapName.ToString(), EOnlineDataAdvertisementType::ViaOnlineService);
	UpdatedSettings.Set(ShowDownExpectedPlayerCountKey, ExpectedLobbyPlayerCount, EOnlineDataAdvertisementType::ViaOnlineService);

	OnSessionResult.Broadcast(
		true,
		FString::Printf(TEXT("Starting game. Connected players: %d"), PlayerControllerCount)
	);

	if (!SessionInterface->UpdateSession(ShowDownSessionName, UpdatedSettings, true))
	{
		SessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionCompleteDelegateHandle);
		UpdateSessionCompleteDelegateHandle.Reset();
		OnSessionResult.Broadcast(true, TEXT("Session update skipped. Starting game..."));
		TravelHostedGame();
	}
}

void UShowDownEosSubsystem::LeaveLobby(FName HubMapName)
{
	StopLobbyStartPolling();
	PendingHostMapName = HubMapName.IsNone() ? FName(TEXT("L_ShowdownMain")) : HubMapName;
	PendingJoinCode.Empty();
	SessionSearch.Reset();

	// A listen-server host owns the session. Send the already connected players
	// home before destroying that session so they do not remain in a dead match.
	if (UWorld* World = GetWorld(); World && World->GetNetMode() != NM_Client)
	{
		const FString HubPath = FString::Printf(TEXT("/Game/Maps/%s"), *PendingHostMapName.ToString());
		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			if (APlayerController* PlayerController = Iterator->Get(); PlayerController && !PlayerController->IsLocalController())
			{
				if (AShowDownPlayerController* ShowDownController = Cast<AShowDownPlayerController>(PlayerController))
				{
					ShowDownController->ClientLeaveMultiplayerRoomToHub();
				}
				else
				{
					PlayerController->ClientTravel(HubPath, TRAVEL_Absolute);
				}
			}
		}
	}

	const IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (!SessionInterface.IsValid() || !SessionInterface->GetNamedSession(ShowDownSessionName))
	{
		CompleteLobbyLeave(true);
		return;
	}

	if (DestroySessionCompleteDelegateHandle.IsValid())
	{
		if (SessionInterface.IsValid())
		{
			SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		}
		DestroySessionCompleteDelegateHandle.Reset();
		CompleteLobbyLeave(false);
		return;
	}

	PendingSessionFlow = ESessionFlow::LeaveLobby;
	DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &UShowDownEosSubsystem::HandleDestroySessionComplete)
	);

	OnSessionResult.Broadcast(true, TEXT("Leaving room..."));
	if (!SessionInterface->DestroySession(ShowDownSessionName))
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		DestroySessionCompleteDelegateHandle.Reset();
		CompleteLobbyLeave(false);
	}
}

void UShowDownEosSubsystem::StartLobbyStartPolling()
{
	if (!bInMultiplayerLobby || bLobbyHost || LobbyCode.IsEmpty())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LobbyStartPollTimerHandle);
		World->GetTimerManager().SetTimer(
			LobbyStartPollTimerHandle,
			this,
			&UShowDownEosSubsystem::PollLobbyStart,
			1.0f,
			true,
			0.2f
		);
	}
}

void UShowDownEosSubsystem::StopLobbyStartPolling()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LobbyStartPollTimerHandle);
	}

	bLobbyStartPollInFlight = false;
}

void UShowDownEosSubsystem::MarkEnteredMultiplayerGame()
{
	StopLobbyStartPolling();
	bInMultiplayerLobby = false;
	bLobbyHost = false;
	PendingJoinCode.Empty();
	ClearTransientSearchState(true);
	PendingSessionFlow = ESessionFlow::None;
}

void UShowDownEosSubsystem::FindAndJoinFirstSession()
{
	if (!IsEosLoggedIn())
	{
		ClearTransientSearchState(true);
		OnSessionResult.Broadcast(false, TEXT("EOS login is required before joining."));
		LoginWithSupabaseSession();
		return;
	}

	const IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		ClearTransientSearchState(true);
		OnSessionResult.Broadcast(false, TEXT("EOS session interface is unavailable."));
		return;
	}

	if (FindSessionsCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		FindSessionsCompleteDelegateHandle.Reset();
	}

	FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UShowDownEosSubsystem::HandleFindSessionsComplete)
	);

	SessionSearch = MakeShared<FOnlineSessionSearch>();
	SessionSearch->MaxSearchResults = 50;
	SessionSearch->bIsLanQuery = false;
	SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);

	OnSessionResult.Broadcast(false, TEXT("Searching EOS sessions..."));

	if (!SessionInterface->FindSessions(LocalUserNum, SessionSearch.ToSharedRef()))
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		FindSessionsCompleteDelegateHandle.Reset();
		ClearTransientSearchState(false);
		OnSessionResult.Broadcast(false, TEXT("EOS session search could not start."));
	}
}

void UShowDownEosSubsystem::HandleLoginComplete(
	int32 InLocalUserNum,
	bool bWasSuccessful,
	const FUniqueNetId& UserId,
	const FString& Error
)
{
	if (const IOnlineIdentityPtr IdentityInterface = GetIdentityInterface(); IdentityInterface.IsValid())
	{
		IdentityInterface->ClearOnLoginCompleteDelegate_Handle(InLocalUserNum, LoginCompleteDelegateHandle);
		LoginCompleteDelegateHandle.Reset();
	}

	if (!bWasSuccessful)
	{
		const FString Message = Error.IsEmpty()
			? TEXT("EOS login failed. Check EOS Connect/OpenID provider settings.")
			: FString::Printf(TEXT("EOS login failed: %s"), *Error);

		UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);
		OnEosLoginResult.Broadcast(false, Message);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("EOS login success: %s"), *UserId.ToString());
	OnEosLoginResult.Broadcast(true, TEXT("EOS login success."));
}

void UShowDownEosSubsystem::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	const IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		CreateSessionCompleteDelegateHandle.Reset();
	}

	if (!bWasSuccessful)
	{
		PendingSessionFlow = ESessionFlow::None;
		ClearTransientSearchState(true);
		PendingJoinCode.Empty();
		LobbyCode.Empty();
		bInMultiplayerLobby = false;
		bLobbyHost = false;
		OnSessionResult.Broadcast(false, TEXT("EOS session creation failed."));
		return;
	}

	const bool bOpeningLobby = PendingSessionFlow == ESessionFlow::HostLobby;
	OnSessionResult.Broadcast(
		true,
		bOpeningLobby
			? FString::Printf(TEXT("Room created. Code: %s"), *LobbyCode)
			: TEXT("EOS session created.")
	);

	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::OpenLevel(World, PendingHostMapName, true, TEXT("listen"));
	}
}

void UShowDownEosSubsystem::HandleUpdateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	const IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionCompleteDelegateHandle);
		UpdateSessionCompleteDelegateHandle.Reset();
	}

	if (!bWasSuccessful)
	{
		OnSessionResult.Broadcast(true, TEXT("Session update failed, starting game anyway..."));
	}

	TravelHostedGame();
}

void UShowDownEosSubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
	const IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		FindSessionsCompleteDelegateHandle.Reset();
	}

	const bool bPollingLobbyStart = PendingSessionFlow == ESessionFlow::PollLobbyStart;
	if (bPollingLobbyStart)
	{
		bLobbyStartPollInFlight = false;
	}
	const bool bBrowsingPublicLobbies = PendingSessionFlow == ESessionFlow::BrowsePublicLobbies;

	if (!bWasSuccessful || !SessionSearch.IsValid() || SessionSearch->SearchResults.Num() == 0)
	{
		ClearTransientSearchState(bBrowsingPublicLobbies);
		if (bBrowsingPublicLobbies)
		{
			PublicLobbySearchResults.Reset();
			PendingSessionFlow = ESessionFlow::None;
			OnPublicRoomsUpdated.Broadcast(bWasSuccessful, TArray<FShowDownPublicRoomInfo>());
			OnSessionResult.Broadcast(bWasSuccessful, TEXT("참가 가능한 공개방이 없습니다."));
		}
		else if (!bPollingLobbyStart)
		{
			PendingSessionFlow = ESessionFlow::None;
			PendingJoinCode.Empty();
			LobbyCode.Empty();
			OnSessionResult.Broadcast(false, TEXT("No EOS sessions found."));
		}
		return;
	}

	if (SessionInterface.IsValid())
	{
		if (bBrowsingPublicLobbies)
		{
			PublicLobbySearchResults.Reset();
			TArray<FShowDownPublicRoomInfo> PublicRooms;
			int32 RejectedPrivateRooms = 0;
			int32 RejectedStartedRooms = 0;
			int32 RejectedFullRooms = 0;
			int32 RejectedInvalidRooms = 0;

			for (const FOnlineSessionSearchResult& SearchResult : SessionSearch->SearchResults)
			{
				FString FoundCode;
				SearchResult.Session.SessionSettings.Get(ShowDownRoomCodeKey, FoundCode);
				if (FoundCode.IsEmpty())
				{
					++RejectedInvalidRooms;
					continue;
				}

				bool bPublicRoom = false;
				SearchResult.Session.SessionSettings.Get(ShowDownRoomPublicKey, bPublicRoom);
				if (!bPublicRoom)
				{
					++RejectedPrivateRooms;
					continue;
				}

				bool bGameStarted = false;
				SearchResult.Session.SessionSettings.Get(ShowDownGameStartedKey, bGameStarted);
				if (bGameStarted)
				{
					++RejectedStartedRooms;
					continue;
				}

				if (SearchResult.Session.NumOpenPublicConnections <= 0)
				{
					++RejectedFullRooms;
					continue;
				}

				FString RoomName;
				SearchResult.Session.SessionSettings.Get(ShowDownRoomNameKey, RoomName);
				if (RoomName.IsEmpty())
				{
					RoomName = FString::Printf(TEXT("Room %s"), *FoundCode);
				}

				const int32 PublicSearchIndex = PublicLobbySearchResults.Add(SearchResult);
				FShowDownPublicRoomInfo RoomInfo;
				RoomInfo.SearchResultIndex = PublicSearchIndex;
				RoomInfo.RoomCode = FoundCode;
				RoomInfo.RoomName = RoomName;
				RoomInfo.MaxPlayers = ShowDownMaxLobbyPlayers;
				RoomInfo.CurrentPlayers = FMath::Clamp(
					ShowDownMaxLobbyPlayers - SearchResult.Session.NumOpenPublicConnections,
					0,
					ShowDownMaxLobbyPlayers);
				PublicRooms.Add(RoomInfo);
			}

			UE_LOG(
				LogTemp,
				Log,
				TEXT("Public lobby browse complete. Raw=%d Listed=%d RejectedInvalid=%d RejectedPrivate=%d RejectedStarted=%d RejectedFull=%d"),
				SessionSearch->SearchResults.Num(),
				PublicRooms.Num(),
				RejectedInvalidRooms,
				RejectedPrivateRooms,
				RejectedStartedRooms,
				RejectedFullRooms);

			PendingSessionFlow = ESessionFlow::None;
			OnPublicRoomsUpdated.Broadcast(true, PublicRooms);
			ClearTransientSearchState(false);
			OnSessionResult.Broadcast(
				true,
				PublicRooms.Num() > 0
					? FString::Printf(TEXT("공개방 %d개를 찾았습니다."), PublicRooms.Num())
					: TEXT("참가 가능한 공개방이 없습니다.")
			);
			return;
		}

		int32 MatchIndex = 0;
		if (!PendingJoinCode.IsEmpty())
		{
			MatchIndex = INDEX_NONE;
			for (int32 Index = 0; Index < SessionSearch->SearchResults.Num(); ++Index)
			{
				FString FoundCode;
				SessionSearch->SearchResults[Index].Session.SessionSettings.Get(ShowDownRoomCodeKey, FoundCode);
				if (FoundCode.Equals(PendingJoinCode, ESearchCase::IgnoreCase))
				{
					MatchIndex = Index;
					break;
				}
			}

			if (MatchIndex == INDEX_NONE)
			{
				if (!bPollingLobbyStart)
				{
					PendingSessionFlow = ESessionFlow::None;
					ClearTransientSearchState(false);
					LobbyCode.Empty();
					OnSessionResult.Broadcast(false, FString::Printf(TEXT("Room %s was not found."), *PendingJoinCode));
					PendingJoinCode.Empty();
				}
				else
				{
					SessionSearch.Reset();
				}
				return;
			}
		}

		if (bPollingLobbyStart)
		{
			bool bGameStarted = false;
			SessionSearch->SearchResults[MatchIndex].Session.SessionSettings.Get(ShowDownGameStartedKey, bGameStarted);

			if (bGameStarted)
			{
				FString GameMapName;
				SessionSearch->SearchResults[MatchIndex].Session.SessionSettings.Get(ShowDownGameMapKey, GameMapName);
				if (!GameMapName.IsEmpty())
				{
					PendingGameMapName = FName(*GameMapName);
				}

				StopLobbyStartPolling();
				bInMultiplayerLobby = false;
				PendingJoinCode.Empty();
				PendingStartedGameSearchResult = SessionSearch->SearchResults[MatchIndex];
				PendingSessionFlow = ESessionFlow::None;
				SessionSearch.Reset();
				if (UWorld* World = GetWorld(); World && World->GetNetMode() == NM_Client)
				{
					OnSessionResult.Broadcast(true, TEXT("EOS game joined."));
				}
				else
				{
					OnSessionResult.Broadcast(true, TEXT("Host started. Entering game..."));
					JoinStartedGameSession();
				}
			}
			else
			{
				SessionSearch.Reset();
			}
			return;
		}

		if (JoinSessionCompleteDelegateHandle.IsValid())
		{
			SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
			JoinSessionCompleteDelegateHandle.Reset();
		}

		JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
			FOnJoinSessionCompleteDelegate::CreateUObject(this, &UShowDownEosSubsystem::HandleJoinSessionComplete)
		);

		const FOnlineSessionSearchResult& SearchResult = SessionSearch->SearchResults[MatchIndex];
		int32 AdvertisedExpectedPlayerCount = ShowDownMaxLobbyPlayers;
		SearchResult.Session.SessionSettings.Get(ShowDownExpectedPlayerCountKey, AdvertisedExpectedPlayerCount);
		ExpectedLobbyPlayerCount = FMath::Clamp(AdvertisedExpectedPlayerCount, 2, ShowDownMaxLobbyPlayers);
		if (SearchResult.Session.NumOpenPublicConnections <= 0)
		{
			SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
			JoinSessionCompleteDelegateHandle.Reset();
			PendingSessionFlow = ESessionFlow::None;
			ClearTransientSearchState(false);
			PendingJoinCode.Empty();
			LobbyCode.Empty();
			OnSessionResult.Broadcast(false, TEXT("방이 가득 찼습니다. 최대 4명까지 입장할 수 있습니다."));
			return;
		}

		OnSessionResult.Broadcast(false, FString::Printf(
			TEXT("방 입장 요청 중... 남은 자리: %d"), SearchResult.Session.NumOpenPublicConnections));
		if (!SessionInterface->JoinSession(LocalUserNum, ShowDownSessionName, SearchResult))
		{
			SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
			JoinSessionCompleteDelegateHandle.Reset();
			PendingSessionFlow = ESessionFlow::None;
			ClearTransientSearchState(false);
			PendingJoinCode.Empty();
			LobbyCode.Empty();
			OnSessionResult.Broadcast(false, TEXT("방 입장 요청을 시작하지 못했습니다. 잠시 후 다시 시도하세요."));
		}
	}
}

void UShowDownEosSubsystem::HandleJoinSessionComplete(
	FName SessionName,
	EOnJoinSessionCompleteResult::Type Result
)
{
	const IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		JoinSessionCompleteDelegateHandle.Reset();
	}

	if (Result != EOnJoinSessionCompleteResult::Success || !SessionInterface.IsValid())
	{
		const bool bJoiningStartedGame = PendingSessionFlow == ESessionFlow::JoinStartedGame;
		FString FailureMessage = TEXT("방 입장에 실패했습니다.");
		switch (Result)
		{
		case EOnJoinSessionCompleteResult::SessionIsFull:
			FailureMessage = TEXT("방이 가득 찼습니다. 최대 4명까지 입장할 수 있습니다.");
			break;
		case EOnJoinSessionCompleteResult::SessionDoesNotExist:
			FailureMessage = TEXT("방이 더 이상 존재하지 않습니다.");
			break;
		case EOnJoinSessionCompleteResult::AlreadyInSession:
			FailureMessage = TEXT("이미 이 방에 입장해 있습니다.");
			break;
		case EOnJoinSessionCompleteResult::CouldNotRetrieveAddress:
			FailureMessage = TEXT("방의 접속 주소를 가져오지 못했습니다.");
			break;
		default:
			break;
		}
		PendingSessionFlow = ESessionFlow::None;
		ClearTransientSearchState(false);
		if (!bJoiningStartedGame)
		{
			PendingJoinCode.Empty();
			LobbyCode.Empty();
			bInMultiplayerLobby = false;
		}
		OnSessionResult.Broadcast(false, FailureMessage);
		if (bJoiningStartedGame)
		{
			StartLobbyStartPolling();
		}
		return;
	}

	FString ConnectString;
	if (!SessionInterface->GetResolvedConnectString(SessionName, ConnectString, NAME_GamePort) || ConnectString.IsEmpty())
	{
		const bool bJoiningStartedGame = PendingSessionFlow == ESessionFlow::JoinStartedGame;
		PendingSessionFlow = ESessionFlow::None;
		ClearTransientSearchState(false);
		if (!bJoiningStartedGame)
		{
			PendingJoinCode.Empty();
			LobbyCode.Empty();
			bInMultiplayerLobby = false;
		}
		OnSessionResult.Broadcast(false, TEXT("EOS connect string was empty."));
		if (bJoiningStartedGame)
		{
			StartLobbyStartPolling();
		}
		return;
	}

	if (APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		const bool bJoiningStartedGame = PendingSessionFlow == ESessionFlow::JoinStartedGame;
		bInMultiplayerLobby = !bJoiningStartedGame && (PendingSessionFlow == ESessionFlow::JoinLobby || bInMultiplayerLobby);
		bLobbyHost = false;
		if (!PendingJoinCode.IsEmpty())
		{
			LobbyCode = PendingJoinCode;
		}
		ClearTransientSearchState(true);
		PendingJoinCode.Empty();
		PendingSessionFlow = ESessionFlow::None;
		if (bJoiningStartedGame)
		{
			LobbyCode.Empty();
		}
		PlayerController->ClientTravel(ConnectString, TRAVEL_Absolute);
		OnSessionResult.Broadcast(true, bJoiningStartedGame ? TEXT("EOS game joined.") : TEXT("EOS lobby joined."));
		return;
	}

	const bool bJoiningStartedGame = PendingSessionFlow == ESessionFlow::JoinStartedGame;
	PendingSessionFlow = ESessionFlow::None;
	ClearTransientSearchState(false);
	if (!bJoiningStartedGame)
	{
		PendingJoinCode.Empty();
		LobbyCode.Empty();
		bInMultiplayerLobby = false;
	}
	OnSessionResult.Broadcast(false, TEXT("Player controller was unavailable for travel."));
	if (bJoiningStartedGame)
	{
		StartLobbyStartPolling();
	}
}

void UShowDownEosSubsystem::HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	const IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		DestroySessionCompleteDelegateHandle.Reset();
	}

	if (PendingSessionFlow == ESessionFlow::LeaveLobby)
	{
		CompleteLobbyLeave(bWasSuccessful);
		return;
	}

	if (PendingSessionFlow == ESessionFlow::CleanupBeforeHostLobby)
	{
		const FName LobbyMapName = PendingHostMapName;
		const FName GameMapName = PendingGameMapName;
		PendingSessionFlow = ESessionFlow::None;
		bInMultiplayerLobby = false;
		bLobbyHost = false;
		LobbyCode.Empty();
		PendingJoinCode.Empty();

		if (!bWasSuccessful)
		{
			ClearTransientSearchState(true);
			OnSessionResult.Broadcast(false, TEXT("이전 방 정리에 실패했습니다."));
			return;
		}

		HostLobbyWithVisibility(LobbyMapName, GameMapName, bPendingLobbyIsPublic);
		return;
	}

	if (PendingSessionFlow == ESessionFlow::CleanupBeforeJoinLobby)
	{
		const FString RoomCode = PendingJoinCode;
		PendingSessionFlow = ESessionFlow::None;
		bInMultiplayerLobby = false;
		bLobbyHost = false;
		LobbyCode.Empty();

		if (!bWasSuccessful)
		{
			PendingJoinCode.Empty();
			ClearTransientSearchState(false);
			OnSessionResult.Broadcast(false, TEXT("이전 방 정리에 실패했습니다."));
			return;
		}

		JoinLobbyByCode(RoomCode);
		return;
	}

	if (!bWasSuccessful)
	{
		PendingSessionFlow = ESessionFlow::None;
		ClearTransientSearchState(false);
		OnSessionResult.Broadcast(false, TEXT("Could not refresh EOS session before joining game."));
		return;
	}

	JoinStartedGameSession();
}

void UShowDownEosSubsystem::CompleteLobbyLeave(bool bSessionDestroyed)
{
	StopLobbyStartPolling();
	bInMultiplayerLobby = false;
	bLobbyHost = false;
	LobbyCode.Empty();
	PendingJoinCode.Empty();
	PendingSessionFlow = ESessionFlow::None;
	ClearTransientSearchState(true);

	const FString TravelPath = FString::Printf(TEXT("/Game/Maps/%s"), *PendingHostMapName.ToString());
	OnSessionResult.Broadcast(
		bSessionDestroyed,
		bSessionDestroyed ? TEXT("Left room.") : TEXT("Room cleanup failed; returning to hub anyway.")
	);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (World->GetNetMode() == NM_Client)
	{
		if (APlayerController* PlayerController = World->GetFirstPlayerController())
		{
			PlayerController->ClientTravel(TravelPath, TRAVEL_Absolute);
		}
		return;
	}

	UGameplayStatics::OpenLevel(World, PendingHostMapName);
}

void UShowDownEosSubsystem::PollLobbyStart()
{
	if (bLobbyStartPollInFlight || !bInMultiplayerLobby || bLobbyHost || LobbyCode.IsEmpty())
	{
		return;
	}

	const IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		return;
	}

	if (FindSessionsCompleteDelegateHandle.IsValid())
	{
		return;
	}

	FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UShowDownEosSubsystem::HandleFindSessionsComplete)
	);

	PendingJoinCode = LobbyCode;
	PendingSessionFlow = ESessionFlow::PollLobbyStart;
	bLobbyStartPollInFlight = true;

	SessionSearch = MakeShared<FOnlineSessionSearch>();
	SessionSearch->MaxSearchResults = 100;
	SessionSearch->bIsLanQuery = false;
	SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	SessionSearch->QuerySettings.Set(ShowDownRoomCodeKey, LobbyCode, EOnlineComparisonOp::Equals);

	if (!SessionInterface->FindSessions(LocalUserNum, SessionSearch.ToSharedRef()))
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		FindSessionsCompleteDelegateHandle.Reset();
		bLobbyStartPollInFlight = false;
		PendingSessionFlow = ESessionFlow::None;
		ClearTransientSearchState(false);
	}
}

void UShowDownEosSubsystem::JoinStartedGameSession()
{
	const IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		PendingSessionFlow = ESessionFlow::None;
		ClearTransientSearchState(false);
		OnSessionResult.Broadcast(false, TEXT("EOS session interface is unavailable."));
		return;
	}

	if (SessionInterface->GetNamedSession(ShowDownSessionName))
	{
		if (DestroySessionCompleteDelegateHandle.IsValid())
		{
			return;
		}

		DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateUObject(this, &UShowDownEosSubsystem::HandleDestroySessionComplete)
		);

		OnSessionResult.Broadcast(true, TEXT("Host started. Refreshing connection..."));
		if (!SessionInterface->DestroySession(ShowDownSessionName))
		{
			SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
			DestroySessionCompleteDelegateHandle.Reset();
			ClearTransientSearchState(false);
			OnSessionResult.Broadcast(false, TEXT("Could not refresh EOS session before joining game."));
			StartLobbyStartPolling();
		}
		return;
	}

	if (JoinSessionCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		JoinSessionCompleteDelegateHandle.Reset();
	}

	JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UShowDownEosSubsystem::HandleJoinSessionComplete)
	);

	PendingSessionFlow = ESessionFlow::JoinStartedGame;
	OnSessionResult.Broadcast(true, TEXT("Host started. Joining game..."));
	if (!SessionInterface->JoinSession(LocalUserNum, ShowDownSessionName, PendingStartedGameSearchResult))
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		JoinSessionCompleteDelegateHandle.Reset();
		ClearTransientSearchState(false);
		OnSessionResult.Broadcast(false, TEXT("EOS game join could not start."));
		StartLobbyStartPolling();
	}
}

void UShowDownEosSubsystem::TravelHostedGame()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		OnSessionResult.Broadcast(false, TEXT("World is unavailable."));
		return;
	}

	AGameModeBase* GameMode = World->GetAuthGameMode();
	if (!GameMode)
	{
		OnSessionResult.Broadcast(false, TEXT("Only the listen server host can start the game."));
		return;
	}

	bInMultiplayerLobby = false;

	FString CurrentMapName = World->GetMapName();
	CurrentMapName.RemoveFromStart(World->StreamingLevelsPrefix);
	const bool bAlreadyOnTargetMap = CurrentMapName.Equals(PendingGameMapName.ToString(), ESearchCase::IgnoreCase);
	if (bAlreadyOnTargetMap)
	{
		if (AShowDownGameModeBase* ShowDownGameMode = Cast<AShowDownGameModeBase>(GameMode))
		{
			OnSessionResult.Broadcast(true, TEXT("Starting game..."));
			ShowDownGameMode->StartMultiplayerGame();
			return;
		}
	}

	const FString TravelPath = FString::Printf(TEXT("/Game/Maps/%s"), *PendingGameMapName.ToString());
	OnSessionResult.Broadcast(true, TEXT("Traveling to game..."));
	GameMode->ProcessServerTravel(TravelPath, false);
}

bool UShowDownEosSubsystem::TravelToSearchResult(
	const FOnlineSessionSearchResult& SearchResult,
	const FString& StatusMessage
)
{
	const IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		OnSessionResult.Broadcast(false, TEXT("EOS session interface is unavailable."));
		return false;
	}

	FString ConnectString;
	if (!SessionInterface->GetResolvedConnectString(SearchResult, NAME_GamePort, ConnectString) || ConnectString.IsEmpty())
	{
		OnSessionResult.Broadcast(false, TEXT("EOS connect string was empty."));
		return false;
	}

	APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PlayerController)
	{
		OnSessionResult.Broadcast(false, TEXT("Player controller was unavailable for travel."));
		return false;
	}

	OnSessionResult.Broadcast(true, StatusMessage);
	PlayerController->ClientTravel(ConnectString, TRAVEL_Absolute);
	return true;
}
