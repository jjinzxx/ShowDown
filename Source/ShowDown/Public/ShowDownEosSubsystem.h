#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/TimerHandle.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ShowDownEosSubsystem.generated.h"

class IVoiceChatUser;
class UNetDriver;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnShowDownEosResult,
	bool,
	bSuccess,
	const FString&,
	Message
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnShowDownLocalVoiceTalkingChanged,
	bool,
	bIsTalking
);

USTRUCT(BlueprintType)
struct FShowDownPublicRoomInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|EOS")
	int32 SearchResultIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|EOS")
	FString RoomCode;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|EOS")
	FString RoomName;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|EOS")
	int32 CurrentPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|EOS")
	int32 MaxPlayers = 4;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnShowDownPublicRoomsUpdated,
	bool,
	bSuccess,
	const TArray<FShowDownPublicRoomInfo>&,
	Rooms
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnShowDownManagedTravelFailed,
	const FString&,
	Message,
	bool,
	bReopenMultiplayerMenu
);

UCLASS()
class SHOWDOWN_API UShowDownEosSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|EOS")
	FOnShowDownEosResult OnEosLoginResult;

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|EOS")
	FOnShowDownEosResult OnSessionResult;

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|EOS")
	FOnShowDownPublicRoomsUpdated OnPublicRoomsUpdated;

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|EOS")
	FOnShowDownManagedTravelFailed OnManagedTravelFailed;

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|EOS|Voice")
	FOnShowDownLocalVoiceTalkingChanged OnLocalVoiceTalkingChanged;

	UFUNCTION(BlueprintCallable, Category = "ShowDown|EOS")
	bool IsEosLoggedIn() const;

	/** True while a create, update, destroy, find, or join request owns an EOS delegate. */
	bool IsSessionOperationInProgress() const;
	/** Excludes passive public-room browsing so UI failures can unlock correctly. */
	bool IsInteractiveSessionOperationInProgress() const;

	UFUNCTION(BlueprintCallable, Category = "ShowDown|EOS")
	void LoginWithSupabaseSession();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|EOS")
	void HostSession(FName MapName = TEXT("L_MultiplayerGame"));

	UFUNCTION(BlueprintCallable, Category = "ShowDown|EOS")
	void FindAndJoinFirstSession();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|EOS")
	void HostLobby(FName LobbyMapName = TEXT("L_MultiplayerLobby"), FName GameMapName = TEXT("L_MultiplayerGame"), const FString& RoomName = TEXT(""));

	UFUNCTION(BlueprintCallable, Category = "ShowDown|EOS")
	void HostPrivateLobby(FName LobbyMapName = TEXT("L_MultiplayerLobby"), FName GameMapName = TEXT("L_MultiplayerGame"), const FString& RoomName = TEXT(""));

	UFUNCTION(BlueprintCallable, Category = "ShowDown|EOS")
	void JoinLobbyByCode(const FString& RoomCode);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|EOS")
	void FindPublicLobbies();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|EOS")
	void JoinPublicLobbyByIndex(int32 SearchResultIndex);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|EOS")
	void StartHostedGame();
	void AbortHostedGameStart(const FString& Reason);

	// Leaves the current room and tears down the local EOS session before returning
	// to the hub. A host leaving closes its listen-server room for the other clients.
	UFUNCTION(BlueprintCallable, Category = "ShowDown|EOS")
	void LeaveLobby(FName HubMapName = TEXT("L_ShowdownMain"));

	UFUNCTION(BlueprintCallable, Category = "ShowDown|EOS")
	void StartLobbyStartPolling();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|EOS")
	void StopLobbyStartPolling();

	void MarkEnteredMultiplayerGame();

	// Keeps a one-shot, user-facing failure across map travel. This is used for
	// server-side lobby rejection/kick reasons, which would otherwise disappear
	// with the gameplay HUD before the hub map finishes loading.
	void QueuePendingHubError(const FString& Message);
	bool ConsumePendingHubError(FString& OutMessage);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|EOS")
	bool IsInMultiplayerLobby() const;

	UFUNCTION(BlueprintCallable, Category = "ShowDown|EOS")
	bool IsLobbyHost() const;

	UFUNCTION(BlueprintCallable, Category = "ShowDown|EOS")
	FString GetLobbyCode() const;
	FString GetLobbyRoomName() const { return LobbyRoomName; }

	int32 GetExpectedLobbyPlayerCount() const { return ExpectedLobbyPlayerCount; }
	bool IsHostedGameRosterLocked() const { return bHostedGameRosterLocked; }

	bool EnsureVoiceChatReady();
	bool BeginVoiceTransmission();
	void EndVoiceTransmission();

private:
	enum class EManagedTravel : uint8
	{
		None,
		HostLobby,
		JoinLobby,
		JoinGame,
		LeaveHub,
		HostGame
	};

	enum class ESessionFlow
	{
		None,
		HostImmediateGame,
		HostLobby,
		JoinLobby,
		PollLobbyStart,
		JoinStartedGame,
		LeaveLobby,
		CleanupBeforeHostLobby,
		CleanupBeforeJoinLobby,
		BrowsePublicLobbies
	};

	FDelegateHandle LoginCompleteDelegateHandle;
	FDelegateHandle CreateSessionCompleteDelegateHandle;
	FDelegateHandle UpdateSessionCompleteDelegateHandle;
	FDelegateHandle DestroySessionCompleteDelegateHandle;
	FDelegateHandle FindSessionsCompleteDelegateHandle;
	FDelegateHandle JoinSessionCompleteDelegateHandle;
	FDelegateHandle NetworkFailureDelegateHandle;
	FDelegateHandle TravelFailureDelegateHandle;
	FDelegateHandle PostLoadMapDelegateHandle;

	TSharedPtr<FOnlineSessionSearch> SessionSearch;
	FOnlineSessionSearchResult PendingStartedGameSearchResult;
	TArray<FOnlineSessionSearchResult> PublicLobbySearchResults;
	FTimerHandle LobbyStartPollTimerHandle;
	FName PendingHostMapName = TEXT("L_MultiplayerLobby");
	FString PendingHostRoomName;
	FName PendingGameMapName = TEXT("L_MultiplayerGame");
	FString PendingJoinCode;
	FString LobbyCode;
	FString LobbyRoomName;
	int32 ExpectedLobbyPlayerCount = 4;
	ESessionFlow PendingSessionFlow = ESessionFlow::None;
	EManagedTravel PendingManagedTravel = EManagedTravel::None;
	bool bLobbyStartPollInFlight = false;
	bool bInMultiplayerLobby = false;
	bool bLobbyHost = false;
	bool bPendingLobbyIsPublic = true;
	// The host freezes the participant set when Start is pressed. Existing
	// controllers survive seamless travel; every later PostLogin is a late join.
	bool bHostedGameRosterLocked = false;
	IVoiceChatUser* VoiceChatUser = nullptr;
	FDelegateHandle VoiceTalkingUpdatedDelegateHandle;
	bool bLocalVoiceTalking = false;
	bool bVoiceTransmissionRequested = false;
	FString PendingHubError;

	class IOnlineSubsystem* GetEosSubsystem() const;
	IOnlineIdentityPtr GetIdentityInterface() const;
	IOnlineSessionPtr GetSessionInterface() const;

	void HandleLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);
	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleUpdateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void PollLobbyStart();
	void JoinStartedGameSession();
	void TravelHostedGame();
	void CompleteLobbyLeave(bool bSessionDestroyed);
	bool TravelToSearchResult(const FOnlineSessionSearchResult& SearchResult, const FString& StatusMessage);
	void HostLobbyWithVisibility(FName LobbyMapName, FName GameMapName, bool bPublicRoom, const FString& RoomName);
	void ClearOnlineDelegateHandles();
	void ClearTransientSearchState(bool bClearPublicRooms);
	void CancelPublicLobbyBrowse();
	void BeginManagedTravel(EManagedTravel Travel);
	void HandlePostLoadMap(UWorld* LoadedWorld);
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);
	void FailManagedTravel(const FString& Message);
	void UnbindVoiceChat();
	void HandleVoicePlayerTalkingUpdated(const FString& ChannelName, const FString& PlayerName, bool bIsTalking);
};
