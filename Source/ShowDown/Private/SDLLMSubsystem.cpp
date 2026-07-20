#include "SDLLMSubsystem.h"

#include "Engine/World.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// 로컬 키 파일(.gitignore 등록). 없으면 환경변수만 사용한다.
#if __has_include("SDLLMSecrets.h")
#include "SDLLMSecrets.h"
#endif

bool USDLLMSubsystem::IsConfigured() const
{
	return bEnableOpenAI && !ResolveApiKey().IsEmpty();
}

bool USDLLMSubsystem::CanMakeRequests() const
{
	const UWorld* World = GetWorld();
	return IsConfigured() && World && World->GetNetMode() == NM_Standalone;
}

void USDLLMSubsystem::RequestBossResponse(const FSDLLMBossContext& Context, FSDLLMBossResponseCallback Callback)
{
	if (!CanMakeRequests())
	{
		Callback.ExecuteIfBound(false, FSDLLMBossResponse());
		return;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(TEXT("https://api.openai.com/v1/responses"));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ResolveApiKey()));
	Request->SetTimeout(RequestTimeoutSeconds);
	Request->SetContentAsString(BuildRequestBody(Context));

	Request->OnProcessRequestComplete().BindWeakLambda(
		this,
		[this, Callback](FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bWasSuccessful)
		{
			FSDLLMBossResponse ParsedResponse;
			const bool bHttpOk = bWasSuccessful
				&& ResponsePtr.IsValid()
				&& EHttpResponseCodes::IsOk(ResponsePtr->GetResponseCode());

			const bool bParsed = bHttpOk && ParseBossResponse(ResponsePtr->GetContentAsString(), ParsedResponse);
			if (!bParsed)
			{
				const int32 ResponseCode = ResponsePtr.IsValid() ? ResponsePtr->GetResponseCode() : 0;
				const FString ResponsePreview = ResponsePtr.IsValid()
					? ResponsePtr->GetContentAsString().Left(512)
					: TEXT("No HTTP response");
				UE_LOG(LogTemp, Warning, TEXT("LLM boss response failed. HTTP %d. Body: %s"), ResponseCode, *ResponsePreview);
			}

			Callback.ExecuteIfBound(bParsed, ParsedResponse);
		});

	if (!Request->ProcessRequest())
	{
		Callback.ExecuteIfBound(false, FSDLLMBossResponse());
	}
}

void USDLLMSubsystem::RequestBossChatReply(const FSDLLMBossContext& Context, FSDLLMBossChatCallback Callback)
{
	if (!CanMakeRequests())
	{
		Callback.ExecuteIfBound(false, FString(), FString());
		return;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(TEXT("https://api.openai.com/v1/responses"));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ResolveApiKey()));
	Request->SetTimeout(RequestTimeoutSeconds);
	Request->SetContentAsString(BuildChatReplyRequestBody(Context));

	Request->OnProcessRequestComplete().BindWeakLambda(
		this,
		[this, Callback](FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bWasSuccessful)
		{
			FString Dialogue;
			FString Intent;
			const bool bHttpOk = bWasSuccessful
				&& ResponsePtr.IsValid()
				&& EHttpResponseCodes::IsOk(ResponsePtr->GetResponseCode());

			const bool bParsed = bHttpOk && ParseBossChatReply(ResponsePtr->GetContentAsString(), Dialogue, Intent);
			if (!bParsed)
			{
				const int32 ResponseCode = ResponsePtr.IsValid() ? ResponsePtr->GetResponseCode() : 0;
				const FString ResponsePreview = ResponsePtr.IsValid()
					? ResponsePtr->GetContentAsString().Left(512)
					: TEXT("No HTTP response");
				UE_LOG(LogTemp, Warning, TEXT("LLM boss chat reply failed. HTTP %d. Body: %s"), ResponseCode, *ResponsePreview);
			}

			Callback.ExecuteIfBound(bParsed, Dialogue, Intent);
		});

	if (!Request->ProcessRequest())
	{
		Callback.ExecuteIfBound(false, FString(), FString());
	}
}

void USDLLMSubsystem::RequestBossResultReaction(const FSDLLMBossContext& Context, FSDLLMBossChatCallback Callback)
{
	if (!CanMakeRequests())
	{
		Callback.ExecuteIfBound(false, FString(), FString());
		return;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(TEXT("https://api.openai.com/v1/responses"));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ResolveApiKey()));
	Request->SetTimeout(RequestTimeoutSeconds);
	Request->SetContentAsString(BuildResultReactionRequestBody(Context));

	Request->OnProcessRequestComplete().BindWeakLambda(
		this,
		[this, Callback](FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bWasSuccessful)
		{
			FString Dialogue;
			FString Intent;
			const bool bHttpOk = bWasSuccessful
				&& ResponsePtr.IsValid()
				&& EHttpResponseCodes::IsOk(ResponsePtr->GetResponseCode());

			const bool bParsed = bHttpOk && ParseBossChatReply(ResponsePtr->GetContentAsString(), Dialogue, Intent);
			if (!bParsed)
			{
				const int32 ResponseCode = ResponsePtr.IsValid() ? ResponsePtr->GetResponseCode() : 0;
				const FString ResponsePreview = ResponsePtr.IsValid()
					? ResponsePtr->GetContentAsString().Left(512)
					: TEXT("No HTTP response");
				UE_LOG(LogTemp, Warning, TEXT("LLM boss result reaction failed. HTTP %d. Body: %s"), ResponseCode, *ResponsePreview);
			}

			Callback.ExecuteIfBound(bParsed, Dialogue, Intent);
		});

	if (!Request->ProcessRequest())
	{
		Callback.ExecuteIfBound(false, FString(), FString());
	}
}

FString USDLLMSubsystem::ResolveApiKey() const
{
	FString Key = FPlatformMisc::GetEnvironmentVariable(*ApiKeyEnvironmentVariable).TrimStartAndEnd();

#ifdef SHOWDOWN_LLM_API_KEY
	// 환경변수가 비어 있으면 로컬 키 파일(SDLLMSecrets.h)의 값을 사용한다.
	if (Key.IsEmpty())
	{
		Key = FString(SHOWDOWN_LLM_API_KEY).TrimStartAndEnd();
	}
#endif

	return Key;
}

FString USDLLMSubsystem::BuildPrompt(const FSDLLMBossContext& Context) const
{
	FString HandRanksText;
	for (int32 Index = 0; Index < Context.CollectorHandRanks.Num(); ++Index)
	{
		if (Index > 0)
		{
			HandRanksText += TEXT(", ");
		}
		HandRanksText += FString::FromInt(Context.CollectorHandRanks[Index]);
	}

	return FString::Printf(
		TEXT("You are the Collector, a quiet analog-horror card boss in ShowDown. ")
		TEXT("Speech style: %s ")
		TEXT("Talk with the player, then directly choose your betting action. ")
		TEXT("Your personality must follow these configured values: low_card_bias=%.2f, bluff_rate=%.2f, aggression=%.2f, fold_tendency=%.2f, noise=%.2f. ")
		TEXT("High low_card_bias means you prefer giving the player low cards and behave as if the player is under pressure. ")
		TEXT("High bluff_rate means you may raise even from uncertain positions. High aggression means larger raises. High fold_tendency means folding earlier when pressured. ")
		TEXT("Use the round memory and action log to adapt to player habits, previous wins/losses, previous exchanged cards, life pressure, and repeated aggression. ")
		TEXT("Legal actions are check, call, raise, fold. If you raise, choose target_bet from current_bet+1 to 6. Keep Korean dialogue under 32 characters. ")
		TEXT("Recent chat:\n%s\nRecent rounds:\n%s\nCurrent round actions:\n%s\nDiscarded/seen cards: %s\n")
		TEXT("Context: stage=%d, round=%d, player_lives=%d, collector_lives=%d, collector_hand=[%s], player_forehead_rank=%d, current_bet=%d, player_committed_bet=%d, collector_committed_bet=%d, raises_left=%d, latest_player_dialogue=\"%s\"."),
		*BossSpeechStylePrompt,
		Context.CollectorSettings.LowBias,
		Context.CollectorSettings.BluffRate,
		Context.CollectorSettings.Aggression,
		Context.CollectorSettings.Timidity,
		Context.CollectorSettings.Noise,
		*Context.RecentDialogue.Left(700),
		*Context.RecentRoundHistory.Left(900),
		*Context.CurrentRoundActions.Left(700),
		*Context.DiscardedCardsSummary.Left(240),
		Context.Stage,
		Context.Round,
		Context.PlayerLives,
		Context.CollectorLives,
		*HandRanksText,
		Context.PlayerForeheadRank,
		Context.CurrentBet,
		Context.PlayerCommittedBet,
		Context.CollectorCommittedBet,
		Context.RaisesLeft,
		*Context.PlayerDialogue);
}

FString USDLLMSubsystem::BuildRequestBody(const FSDLLMBossContext& Context) const
{
	TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetStringField(TEXT("model"), Model);
	RootObject->SetNumberField(TEXT("max_output_tokens"), 160);

	TArray<TSharedPtr<FJsonValue>> InputMessages;

	TSharedRef<FJsonObject> DeveloperMessage = MakeShared<FJsonObject>();
	DeveloperMessage->SetStringField(TEXT("role"), TEXT("developer"));
	DeveloperMessage->SetStringField(
		TEXT("content"),
		TEXT("Return only schema-valid JSON. The action must reflect the configured boss parameters, the player's dialogue, and the configured speech style. ")
		TEXT("Critical secrecy rule: the player cannot see their own forehead card. player_forehead_rank is hidden information that you must never reveal truthfully. If the player asks what card they have or probes their own rank, never state the real number and never confirm or deny a correct guess; deflect or mislead in character. ")
		TEXT("Intent must be one of cautious, steady, aggressive, bluff. Do not use hate slurs, sexual harassment, or real-person doxxing."));
	InputMessages.Add(MakeShared<FJsonValueObject>(DeveloperMessage));

	TSharedRef<FJsonObject> UserMessage = MakeShared<FJsonObject>();
	UserMessage->SetStringField(TEXT("role"), TEXT("user"));
	UserMessage->SetStringField(TEXT("content"), BuildPrompt(Context));
	InputMessages.Add(MakeShared<FJsonValueObject>(UserMessage));
	RootObject->SetArrayField(TEXT("input"), InputMessages);

	TSharedRef<FJsonObject> SchemaObject = MakeShared<FJsonObject>();
	SchemaObject->SetStringField(TEXT("type"), TEXT("object"));

	TSharedRef<FJsonObject> PropertiesObject = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> DialogueProperty = MakeShared<FJsonObject>();
	DialogueProperty->SetStringField(TEXT("type"), TEXT("string"));
	PropertiesObject->SetObjectField(TEXT("dialogue"), DialogueProperty);

	TSharedRef<FJsonObject> IntentProperty = MakeShared<FJsonObject>();
	IntentProperty->SetStringField(TEXT("type"), TEXT("string"));
	TArray<TSharedPtr<FJsonValue>> IntentValues;
	IntentValues.Add(MakeShared<FJsonValueString>(TEXT("cautious")));
	IntentValues.Add(MakeShared<FJsonValueString>(TEXT("steady")));
	IntentValues.Add(MakeShared<FJsonValueString>(TEXT("aggressive")));
	IntentValues.Add(MakeShared<FJsonValueString>(TEXT("bluff")));
	IntentProperty->SetArrayField(TEXT("enum"), IntentValues);
	PropertiesObject->SetObjectField(TEXT("intent"), IntentProperty);

	TSharedRef<FJsonObject> ActionProperty = MakeShared<FJsonObject>();
	ActionProperty->SetStringField(TEXT("type"), TEXT("string"));
	TArray<TSharedPtr<FJsonValue>> ActionValues;
	ActionValues.Add(MakeShared<FJsonValueString>(TEXT("check")));
	ActionValues.Add(MakeShared<FJsonValueString>(TEXT("call")));
	ActionValues.Add(MakeShared<FJsonValueString>(TEXT("raise")));
	ActionValues.Add(MakeShared<FJsonValueString>(TEXT("fold")));
	ActionProperty->SetArrayField(TEXT("enum"), ActionValues);
	PropertiesObject->SetObjectField(TEXT("action"), ActionProperty);

	TSharedRef<FJsonObject> TargetBetProperty = MakeShared<FJsonObject>();
	TargetBetProperty->SetStringField(TEXT("type"), TEXT("integer"));
	TargetBetProperty->SetNumberField(TEXT("minimum"), 0);
	TargetBetProperty->SetNumberField(TEXT("maximum"), 6);
	PropertiesObject->SetObjectField(TEXT("target_bet"), TargetBetProperty);
	SchemaObject->SetObjectField(TEXT("properties"), PropertiesObject);

	TArray<TSharedPtr<FJsonValue>> RequiredFields;
	RequiredFields.Add(MakeShared<FJsonValueString>(TEXT("dialogue")));
	RequiredFields.Add(MakeShared<FJsonValueString>(TEXT("intent")));
	RequiredFields.Add(MakeShared<FJsonValueString>(TEXT("action")));
	RequiredFields.Add(MakeShared<FJsonValueString>(TEXT("target_bet")));
	SchemaObject->SetArrayField(TEXT("required"), RequiredFields);
	SchemaObject->SetBoolField(TEXT("additionalProperties"), false);

	TSharedRef<FJsonObject> FormatObject = MakeShared<FJsonObject>();
	FormatObject->SetStringField(TEXT("type"), TEXT("json_schema"));
	FormatObject->SetStringField(TEXT("name"), TEXT("showdown_boss_response"));
	FormatObject->SetBoolField(TEXT("strict"), true);
	FormatObject->SetObjectField(TEXT("schema"), SchemaObject);

	TSharedRef<FJsonObject> TextObject = MakeShared<FJsonObject>();
	TextObject->SetObjectField(TEXT("format"), FormatObject);
	RootObject->SetObjectField(TEXT("text"), TextObject);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootObject, Writer);
	return OutputString;
}

FString USDLLMSubsystem::BuildChatReplyRequestBody(const FSDLLMBossContext& Context) const
{
	TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetStringField(TEXT("model"), Model);
	RootObject->SetNumberField(TEXT("max_output_tokens"), 80);

	TArray<TSharedPtr<FJsonValue>> InputMessages;

	TSharedRef<FJsonObject> DeveloperMessage = MakeShared<FJsonObject>();
	DeveloperMessage->SetStringField(TEXT("role"), TEXT("developer"));
	DeveloperMessage->SetStringField(
		TEXT("content"),
		TEXT("Return only schema-valid JSON. Reply as the Collector. This is chat only; do not choose or mention a betting action. Directly answer the player's latest line. Do not invent context, do not change the subject, and do not repeat catchphrases unless the latest line invites it. ")
		TEXT("Critical secrecy rule: the player cannot see their own forehead card. player_forehead_rank is hidden information that you must never reveal truthfully. If the player asks what card they have, what you gave them, or otherwise probes their own rank, never state the real number and never confirm or deny a correct guess. Deflect, tease, mislead, or give an evasive in-character answer instead. ")
		TEXT("Intent must be one of cautious, steady, aggressive, bluff. Do not use hate slurs, sexual harassment, or real-person doxxing."));
	InputMessages.Add(MakeShared<FJsonValueObject>(DeveloperMessage));

	TSharedRef<FJsonObject> UserMessage = MakeShared<FJsonObject>();
	UserMessage->SetStringField(TEXT("role"), TEXT("user"));
	UserMessage->SetStringField(
		TEXT("content"),
		FString::Printf(
			TEXT("Speech style: %s Recent chat:\n%s\nRecent rounds:\n%s\nCurrent round actions:\n%s\nDiscarded/seen cards: %s\nLatest player line: \"%s\"\nGame state: stage=%d, round=%d, player_lives=%d, collector_lives=%d, player_forehead_rank=%d, current_bet=%d, player_committed_bet=%d, collector_committed_bet=%d, raises_left=%d.\nReply in Korean under 32 characters. Make the reply relevant to the latest line first. Use the game memory only when it fits naturally; do not randomly mention old facts."),
			*BossSpeechStylePrompt,
			*Context.RecentDialogue.Left(700),
			*Context.RecentRoundHistory.Left(900),
			*Context.CurrentRoundActions.Left(700),
			*Context.DiscardedCardsSummary.Left(240),
			*Context.PlayerDialogue.Left(240),
			Context.Stage,
			Context.Round,
			Context.PlayerLives,
			Context.CollectorLives,
			Context.PlayerForeheadRank,
			Context.CurrentBet,
			Context.PlayerCommittedBet,
			Context.CollectorCommittedBet,
			Context.RaisesLeft));
	InputMessages.Add(MakeShared<FJsonValueObject>(UserMessage));
	RootObject->SetArrayField(TEXT("input"), InputMessages);

	TSharedRef<FJsonObject> SchemaObject = MakeShared<FJsonObject>();
	SchemaObject->SetStringField(TEXT("type"), TEXT("object"));

	TSharedRef<FJsonObject> PropertiesObject = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> DialogueProperty = MakeShared<FJsonObject>();
	DialogueProperty->SetStringField(TEXT("type"), TEXT("string"));
	PropertiesObject->SetObjectField(TEXT("dialogue"), DialogueProperty);

	TSharedRef<FJsonObject> IntentProperty = MakeShared<FJsonObject>();
	IntentProperty->SetStringField(TEXT("type"), TEXT("string"));
	TArray<TSharedPtr<FJsonValue>> IntentValues;
	IntentValues.Add(MakeShared<FJsonValueString>(TEXT("cautious")));
	IntentValues.Add(MakeShared<FJsonValueString>(TEXT("steady")));
	IntentValues.Add(MakeShared<FJsonValueString>(TEXT("aggressive")));
	IntentValues.Add(MakeShared<FJsonValueString>(TEXT("bluff")));
	IntentProperty->SetArrayField(TEXT("enum"), IntentValues);
	PropertiesObject->SetObjectField(TEXT("intent"), IntentProperty);

	SchemaObject->SetObjectField(TEXT("properties"), PropertiesObject);
	TArray<TSharedPtr<FJsonValue>> RequiredFields;
	RequiredFields.Add(MakeShared<FJsonValueString>(TEXT("dialogue")));
	RequiredFields.Add(MakeShared<FJsonValueString>(TEXT("intent")));
	SchemaObject->SetArrayField(TEXT("required"), RequiredFields);
	SchemaObject->SetBoolField(TEXT("additionalProperties"), false);

	TSharedRef<FJsonObject> FormatObject = MakeShared<FJsonObject>();
	FormatObject->SetStringField(TEXT("type"), TEXT("json_schema"));
	FormatObject->SetStringField(TEXT("name"), TEXT("showdown_boss_chat_reply"));
	FormatObject->SetBoolField(TEXT("strict"), true);
	FormatObject->SetObjectField(TEXT("schema"), SchemaObject);

	TSharedRef<FJsonObject> TextObject = MakeShared<FJsonObject>();
	TextObject->SetObjectField(TEXT("format"), FormatObject);
	RootObject->SetObjectField(TEXT("text"), TextObject);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootObject, Writer);
	return OutputString;
}

FString USDLLMSubsystem::BuildResultReactionRequestBody(const FSDLLMBossContext& Context) const
{
	TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetStringField(TEXT("model"), Model);
	RootObject->SetNumberField(TEXT("max_output_tokens"), 80);

	TArray<TSharedPtr<FJsonValue>> InputMessages;

	TSharedRef<FJsonObject> DeveloperMessage = MakeShared<FJsonObject>();
	DeveloperMessage->SetStringField(TEXT("role"), TEXT("developer"));
	DeveloperMessage->SetStringField(
		TEXT("content"),
		TEXT("Return only schema-valid JSON. You are the Collector and a betting round just ended; the cards are revealed. React to the outcome in one short in-character Korean line based on round_outcome. ")
		TEXT("round_outcome=collector_won means you won: be smug, menacing, or coldly satisfied. round_outcome=collector_lost means you lost: sound reluctantly impressed or regretful, like \"아쉽군\" — never celebrate. round_outcome=draw means a tie: be coldly indifferent or unsettled. ")
		TEXT("Use the recent chat and round history so the line feels like a continuation, not a random shout. Do not choose or mention a betting action. Intent must be one of cautious, steady, aggressive, bluff. Do not use hate slurs, sexual harassment, or real-person doxxing."));
	InputMessages.Add(MakeShared<FJsonValueObject>(DeveloperMessage));

	TSharedRef<FJsonObject> UserMessage = MakeShared<FJsonObject>();
	UserMessage->SetStringField(TEXT("role"), TEXT("user"));
	UserMessage->SetStringField(
		TEXT("content"),
		FString::Printf(
			TEXT("Speech style: %s Round just ended. round_outcome=%s\nRecent chat:\n%s\nRecent rounds:\n%s\nThis round actions:\n%s\nGame state: stage=%d, round=%d, player_lives=%d, collector_lives=%d, player_forehead_rank=%d, collector_forehead_rank=%d.\nReply in Korean under 32 characters as a reaction to the result. Match the tone to round_outcome."),
			*BossSpeechStylePrompt,
			*Context.RoundOutcome,
			*Context.RecentDialogue.Left(700),
			*Context.RecentRoundHistory.Left(900),
			*Context.CurrentRoundActions.Left(700),
			Context.Stage,
			Context.Round,
			Context.PlayerLives,
			Context.CollectorLives,
			Context.PlayerForeheadRank,
			Context.CollectorForeheadRank));
	InputMessages.Add(MakeShared<FJsonValueObject>(UserMessage));
	RootObject->SetArrayField(TEXT("input"), InputMessages);

	TSharedRef<FJsonObject> SchemaObject = MakeShared<FJsonObject>();
	SchemaObject->SetStringField(TEXT("type"), TEXT("object"));

	TSharedRef<FJsonObject> PropertiesObject = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> DialogueProperty = MakeShared<FJsonObject>();
	DialogueProperty->SetStringField(TEXT("type"), TEXT("string"));
	PropertiesObject->SetObjectField(TEXT("dialogue"), DialogueProperty);

	TSharedRef<FJsonObject> IntentProperty = MakeShared<FJsonObject>();
	IntentProperty->SetStringField(TEXT("type"), TEXT("string"));
	TArray<TSharedPtr<FJsonValue>> IntentValues;
	IntentValues.Add(MakeShared<FJsonValueString>(TEXT("cautious")));
	IntentValues.Add(MakeShared<FJsonValueString>(TEXT("steady")));
	IntentValues.Add(MakeShared<FJsonValueString>(TEXT("aggressive")));
	IntentValues.Add(MakeShared<FJsonValueString>(TEXT("bluff")));
	IntentProperty->SetArrayField(TEXT("enum"), IntentValues);
	PropertiesObject->SetObjectField(TEXT("intent"), IntentProperty);

	SchemaObject->SetObjectField(TEXT("properties"), PropertiesObject);
	TArray<TSharedPtr<FJsonValue>> RequiredFields;
	RequiredFields.Add(MakeShared<FJsonValueString>(TEXT("dialogue")));
	RequiredFields.Add(MakeShared<FJsonValueString>(TEXT("intent")));
	SchemaObject->SetArrayField(TEXT("required"), RequiredFields);
	SchemaObject->SetBoolField(TEXT("additionalProperties"), false);

	TSharedRef<FJsonObject> FormatObject = MakeShared<FJsonObject>();
	FormatObject->SetStringField(TEXT("type"), TEXT("json_schema"));
	FormatObject->SetStringField(TEXT("name"), TEXT("showdown_boss_result_reaction"));
	FormatObject->SetBoolField(TEXT("strict"), true);
	FormatObject->SetObjectField(TEXT("schema"), SchemaObject);

	TSharedRef<FJsonObject> TextObject = MakeShared<FJsonObject>();
	TextObject->SetObjectField(TEXT("format"), FormatObject);
	RootObject->SetObjectField(TEXT("text"), TextObject);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootObject, Writer);
	return OutputString;
}

bool USDLLMSubsystem::ParseBossResponse(const FString& ResponseBody, FSDLLMBossResponse& OutResponse) const
{
	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		return false;
	}

	const FString OutputText = ExtractOutputText(RootObject);
	if (OutputText.IsEmpty())
	{
		return false;
	}

	TSharedPtr<FJsonObject> BossObject;
	TSharedRef<TJsonReader<>> BossReader = TJsonReaderFactory<>::Create(OutputText);
	if (!FJsonSerializer::Deserialize(BossReader, BossObject) || !BossObject.IsValid())
	{
		return false;
	}

	BossObject->TryGetStringField(TEXT("dialogue"), OutResponse.Dialogue);
	BossObject->TryGetStringField(TEXT("intent"), OutResponse.Intent);

	FString ActionText;
	BossObject->TryGetStringField(TEXT("action"), ActionText);
	ActionText = ActionText.ToLower();
	if (ActionText == TEXT("check"))
	{
		OutResponse.Decision.Action = EShowDownBetAction::Check;
	}
	else if (ActionText == TEXT("call"))
	{
		OutResponse.Decision.Action = EShowDownBetAction::Call;
	}
	else if (ActionText == TEXT("raise"))
	{
		OutResponse.Decision.Action = EShowDownBetAction::Raise;
	}
	else if (ActionText == TEXT("fold"))
	{
		OutResponse.Decision.Action = EShowDownBetAction::Fold;
	}
	else
	{
		return false;
	}

	double TargetBet = 0.0;
	BossObject->TryGetNumberField(TEXT("target_bet"), TargetBet);
	OutResponse.Decision.TargetBet = FMath::RoundToInt(TargetBet);

	ClampResponse(OutResponse);
	return !OutResponse.Dialogue.IsEmpty();
}

bool USDLLMSubsystem::ParseBossChatReply(const FString& ResponseBody, FString& OutDialogue, FString& OutIntent) const
{
	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		return false;
	}

	const FString OutputText = ExtractOutputText(RootObject);
	if (OutputText.IsEmpty())
	{
		return false;
	}

	TSharedPtr<FJsonObject> BossObject;
	TSharedRef<TJsonReader<>> BossReader = TJsonReaderFactory<>::Create(OutputText);
	if (!FJsonSerializer::Deserialize(BossReader, BossObject) || !BossObject.IsValid())
	{
		return false;
	}

	BossObject->TryGetStringField(TEXT("dialogue"), OutDialogue);
	BossObject->TryGetStringField(TEXT("intent"), OutIntent);
	OutDialogue = OutDialogue.Left(48);
	if (OutIntent.IsEmpty())
	{
		OutIntent = TEXT("steady");
	}

	return !OutDialogue.IsEmpty();
}

FString USDLLMSubsystem::ExtractOutputText(const TSharedPtr<FJsonObject>& RootObject) const
{
	FString OutputText;
	if (RootObject->TryGetStringField(TEXT("output_text"), OutputText) && !OutputText.IsEmpty())
	{
		return OutputText;
	}

	const TArray<TSharedPtr<FJsonValue>>* OutputArray = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("output"), OutputArray))
	{
		return FString();
	}

	for (const TSharedPtr<FJsonValue>& OutputValue : *OutputArray)
	{
		const TSharedPtr<FJsonObject> OutputObject = OutputValue->AsObject();
		if (!OutputObject.IsValid())
		{
			continue;
		}

		const TArray<TSharedPtr<FJsonValue>>* ContentArray = nullptr;
		if (!OutputObject->TryGetArrayField(TEXT("content"), ContentArray))
		{
			continue;
		}

		for (const TSharedPtr<FJsonValue>& ContentValue : *ContentArray)
		{
			const TSharedPtr<FJsonObject> ContentObject = ContentValue->AsObject();
			if (ContentObject.IsValid() && ContentObject->TryGetStringField(TEXT("text"), OutputText))
			{
				return OutputText;
			}
		}
	}

	return FString();
}

void USDLLMSubsystem::ClampResponse(FSDLLMBossResponse& Response) const
{
	Response.Dialogue = Response.Dialogue.Left(48);
	Response.Decision.TargetBet = FMath::Clamp(Response.Decision.TargetBet, 0, 6);
}
