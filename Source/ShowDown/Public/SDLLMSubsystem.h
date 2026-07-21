#pragma once

#include "CoreMinimal.h"
#include "CollectorAISystem.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SDLLMSubsystem.generated.h"

class ACard;
class IHttpRequest;
class IHttpResponse;

using FHttpRequestPtr = TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>;
using FHttpResponsePtr = TSharedPtr<IHttpResponse, ESPMode::ThreadSafe>;

USTRUCT(BlueprintType)
struct FSDLLMBossContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "ShowDown|LLM")
	TArray<int32> CollectorHandRanks;

	UPROPERTY(BlueprintReadWrite, Category = "ShowDown|LLM")
	FString PlayerDialogue;

	UPROPERTY(BlueprintReadWrite, Category = "ShowDown|LLM")
	FString RecentDialogue;

	UPROPERTY(BlueprintReadWrite, Category = "ShowDown|LLM")
	FCollectorAISettings CollectorSettings;

	UPROPERTY(BlueprintReadWrite, Category = "ShowDown|LLM")
	FString RecentRoundHistory;

	UPROPERTY(BlueprintReadWrite, Category = "ShowDown|LLM")
	FString CurrentRoundActions;

	UPROPERTY(BlueprintReadWrite, Category = "ShowDown|LLM")
	FString DiscardedCardsSummary;

	UPROPERTY(BlueprintReadWrite, Category = "ShowDown|LLM")
	FString CurrentPhase = TEXT("none");

	UPROPERTY(BlueprintReadWrite, Category = "ShowDown|LLM")
	FString CurrentTurn = TEXT("none");

	UPROPERTY(BlueprintReadWrite, Category = "ShowDown|LLM")
	FString ExpectedAction = TEXT("wait");

	UPROPERTY(BlueprintReadWrite, Category = "ShowDown|LLM")
	FString OpponentName = TEXT("상대");

	UPROPERTY(BlueprintReadWrite, Category = "ShowDown|LLM")
	int32 PlayerForeheadRank = 0;

	UPROPERTY(BlueprintReadWrite, Category = "ShowDown|LLM")
	int32 CollectorForeheadRank = 0;

	// This server-authored policy stays fixed for the current card round.
	UPROPERTY(BlueprintReadWrite, Category = "ShowDown|LLM")
	FString PlayerCardClaimMode = TEXT("evasive");

	UPROPERTY(BlueprintReadWrite, Category = "ShowDown|LLM")
	int32 PlayerCardClaimRank = 0;

	UPROPERTY(BlueprintReadWrite, Category = "ShowDown|LLM")
	FString PlayerCardClaimDetail = TEXT("evasive");

	UPROPERTY(BlueprintReadWrite, Category = "ShowDown|LLM")
	int32 CurrentBet = 1;

	UPROPERTY(BlueprintReadWrite, Category = "ShowDown|LLM")
	int32 PlayerCommittedBet = 1;

	UPROPERTY(BlueprintReadWrite, Category = "ShowDown|LLM")
	int32 CollectorCommittedBet = 1;

	UPROPERTY(BlueprintReadWrite, Category = "ShowDown|LLM")
	int32 PlayerLives = 3;

	UPROPERTY(BlueprintReadWrite, Category = "ShowDown|LLM")
	int32 CollectorLives = 3;

	UPROPERTY(BlueprintReadWrite, Category = "ShowDown|LLM")
	int32 RaisesLeft = 0;

	UPROPERTY(BlueprintReadWrite, Category = "ShowDown|LLM")
	int32 Stage = 1;

	UPROPERTY(BlueprintReadWrite, Category = "ShowDown|LLM")
	int32 Round = 1;

	// 라운드 결과 반응 요청 시 보스(Collector) 관점의 결과: "collector_won" / "collector_lost" / "draw"
	UPROPERTY(BlueprintReadWrite, Category = "ShowDown|LLM")
	FString RoundOutcome;
};

USTRUCT(BlueprintType)
struct FSDLLMBossResponse
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|LLM")
	FString Dialogue = TEXT("...");

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|LLM")
	FString Intent = TEXT("steady");

	UPROPERTY(BlueprintReadOnly, Category = "ShowDown|LLM")
	FCollectorBetDecision Decision;
};

DECLARE_DELEGATE_TwoParams(FSDLLMBossResponseCallback, bool /*bSuccess*/, const FSDLLMBossResponse& /*Response*/);
DECLARE_DELEGATE_ThreeParams(FSDLLMBossChatCallback, bool /*bSuccess*/, const FString& /*Dialogue*/, const FString& /*Intent*/);

UCLASS(Config=Game)
class SHOWDOWN_API USDLLMSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "ShowDown|LLM")
	bool bEnableOpenAI = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "ShowDown|LLM")
	FString Model = TEXT("gpt-5.6-terra");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "ShowDown|LLM")
	FString ApiKeyEnvironmentVariable = TEXT("OPENAI_API_KEY");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "ShowDown|LLM")
	float RequestTimeoutSeconds = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "ShowDown|LLM")
	FString BossSpeechStylePrompt = TEXT("Speak in short, natural Korean banmal with the loose swagger of an underground street punk. Always understand and answer the player's latest line directly before adding attitude. For greetings, casual remarks, or neutral questions, respond casually without profanity or hostility. Use mild profanity only rarely and only when the player insults, provokes, threatens, applies strong pressure, or when a tense game result genuinely calls for it. Never insert profanity as filler, never open a reply with unrelated profanity, and never copy stock examples. Keep the confidence relaxed and streetwise rather than constantly angry. No anonymous-board slang, \"ㅋㅋ\", random memes, repetitive catchphrases, hate slurs, sexual insults, threats of real violence, or real-person references.");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "ShowDown|LLM")
	bool bEnableInstantBossChatReply = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "ShowDown|LLM")
	bool bAllowOverlappingBossChatReplies = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "ShowDown|LLM")
	float BossChatReplyCooldownSeconds = 3.0f;

	bool IsConfigured() const;
	bool CanMakeRequests() const;
	void RequestBossResponse(const FSDLLMBossContext& Context, FSDLLMBossResponseCallback Callback);
	void RequestBossChatReply(const FSDLLMBossContext& Context, FSDLLMBossChatCallback Callback);
	void RequestBossResultReaction(const FSDLLMBossContext& Context, FSDLLMBossChatCallback Callback);

private:
	FString ResolveApiKey() const;
	FString BuildPrompt(const FSDLLMBossContext& Context) const;
	FString BuildRequestBody(const FSDLLMBossContext& Context) const;
	FString BuildChatReplyRequestBody(const FSDLLMBossContext& Context) const;
	FString BuildResultReactionRequestBody(const FSDLLMBossContext& Context) const;
	bool ParseBossResponse(const FString& ResponseBody, FSDLLMBossResponse& OutResponse) const;
	bool ParseBossChatReply(const FString& ResponseBody, FString& OutDialogue, FString& OutIntent) const;
	FString ExtractOutputText(const TSharedPtr<FJsonObject>& RootObject) const;
	void ClampResponse(FSDLLMBossResponse& Response) const;
};
