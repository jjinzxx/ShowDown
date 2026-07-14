#pragma once

#include "CoreMinimal.h"
#include "AudioCaptureCore.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"
#include "ShowDownVoiceSubsystem.generated.h"

class IHttpRequest;
class IHttpResponse;
class UAudioComponent;
class USoundWaveProcedural;

UENUM(BlueprintType)
enum class EShowDownVoiceInputMode : uint8
{
	Off UMETA(DisplayName = "Off"),
	PushToTalk UMETA(DisplayName = "Push To Talk"),
	AlwaysOpen UMETA(DisplayName = "Always Open")
};

DECLARE_DELEGATE_TwoParams(FShowDownVoiceTextCallback, bool /*bSuccess*/, const FString& /*Text*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FShowDownVoiceStatusSignature, bool, bSuccess, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShowDownSpeechPlaybackStateSignature, bool, bIsSpeaking);

UCLASS(Config=Game)
class SHOWDOWN_API UShowDownVoiceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "ShowDown|Voice")
	bool bEnableOpenAIVoice = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "ShowDown|Voice")
	EShowDownVoiceInputMode VoiceInputMode = EShowDownVoiceInputMode::PushToTalk;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "ShowDown|Voice")
	FString ApiKeyEnvironmentVariable = TEXT("OPENAI_API_KEY");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "ShowDown|Voice")
	FString TranscriptionModel = TEXT("gpt-4o-mini-transcribe");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "ShowDown|Voice")
	FString TTSModel = TEXT("gpt-4o-mini-tts");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "ShowDown|Voice")
	FString TTSVoice = TEXT("echo");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "ShowDown|Voice", meta = (MultiLine = "true"))
	FString TTSInstructions = TEXT("Identity: A robot. Affect: Monotone, mechanical, and neutral, reflecting the robotic nature of the customer service agent. Tone: Efficient, direct, and formal, with a focus on delivering information clearly and without emotion. Emotion: Neutral and impersonal, with no emotional inflection, as the robot voice is focused purely on functionality. Pauses: Brief and purposeful, allowing for processing and separating key pieces of information, such as confirming the return and refund details. Pronunciation: Clear, precise, and consistent, with each word spoken distinctly to ensure the customer can easily follow the automated process.");

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Config,
		Category = "ShowDown|Voice",
		meta = (DisplayName = "Voice Speed", ClampMin = "0.5", ClampMax = "2.0", UIMin = "0.5", UIMax = "2.0"))
	float TTSPlaybackSpeed = 1.2f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Config,
		Category = "ShowDown|Voice",
		meta = (DisplayName = "Voice Pitch", ClampMin = "0.5", ClampMax = "2.0", UIMin = "0.5", UIMax = "2.0"))
	float TTSPlaybackPitch = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "ShowDown|Voice")
	FString TranscriptionLanguage = TEXT("ko");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "ShowDown|Voice", meta = (ClampMin = "0.1"))
	float MinimumRecordingSeconds = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "ShowDown|Voice", meta = (ClampMin = "1.0"))
	float MaximumRecordingSeconds = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "ShowDown|Voice", meta = (ClampMin = "1.0"))
	float RequestTimeoutSeconds = 20.0f;

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Voice")
	FShowDownVoiceStatusSignature OnVoiceStatus;

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Voice")
	FShowDownSpeechPlaybackStateSignature OnSpeechPlaybackStateChanged;

	UFUNCTION(BlueprintPure, Category = "ShowDown|Voice")
	bool IsConfigured() const;

	UFUNCTION(BlueprintPure, Category = "ShowDown|Voice")
	bool IsRecording() const { return bRecording; }

	UFUNCTION(BlueprintPure, Category = "ShowDown|Voice")
	bool IsTranscriptionInFlight() const { return bTranscriptionInFlight; }

	UFUNCTION(BlueprintPure, Category = "ShowDown|Voice")
	bool IsSpeechInFlight() const { return bSpeechInFlight; }

	UFUNCTION(BlueprintPure, Category = "ShowDown|Voice")
	bool HasPendingSpeech() const { return bHasPendingSpeech; }

	UFUNCTION(BlueprintPure, Category = "ShowDown|Voice")
	int32 GetCapturedSampleCount() const;

	UFUNCTION(BlueprintPure, Category = "ShowDown|Voice")
	float GetCurrentRecordingSeconds() const;

	UFUNCTION(BlueprintPure, Category = "ShowDown|Voice")
	FString GetVoiceDebugSummary() const;

	UFUNCTION(BlueprintPure, Category = "ShowDown|Voice")
	FString GetLastTranscribedText() const { return LastTranscribedText; }

	UFUNCTION(BlueprintPure, Category = "ShowDown|Voice")
	FString GetLastVoiceError() const { return LastVoiceError; }

	UFUNCTION(BlueprintPure, Category = "ShowDown|Voice")
	int32 GetLastSpeechByteCount() const { return LastSpeechByteCount; }

	UFUNCTION(BlueprintPure, Category = "ShowDown|Voice")
	int32 GetLastRecordingWavByteCount() const { return LastRecordedWavData.Num(); }

	bool BeginPushToTalk(FShowDownVoiceTextCallback Callback);
	void EndPushToTalk();
	void CancelPushToTalk();
	void SpeakCollectorLine(const FString& Text);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Voice")
	bool SaveLastRecordingWav(FString& OutFilePath);

	bool TranscribeWavFileForDebug(const FString& FilePath, FShowDownVoiceTextCallback Callback);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Voice")
	void PlayDebugTone(float FrequencyHz = 440.0f, float DurationSeconds = 0.75f);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Voice")
	void SetVoiceInputMode(EShowDownVoiceInputMode NewMode);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Voice")
	void SetOpenAIVoiceEnabled(bool bInEnableOpenAIVoice);

private:
	FString ResolveApiKey() const;
	void BroadcastVoiceStatus(bool bSuccess, const FString& Message);
	void BroadcastSpeechPlaybackState(bool bIsSpeaking);
	void OpenAndStartCaptureStream();
	void HandleCapturedAudio(const void* AudioData, int32 NumFrames, int32 NumChannels, int32 SampleRate);
	void RequestTranscription(TArray<uint8>&& WavData);
	void RequestSpeech(const FString& Text);
	void RequestPendingSpeech();
	bool ParseTranscriptionResponse(const FString& ResponseBody, FString& OutText) const;
	bool BuildRecordedWav(TArray<uint8>& OutWavData, float& OutDurationSeconds) const;
	void PlaySpeechWav(const TArray<uint8>& WavData);

	UFUNCTION()
	void HandleSpeechAudioFinished();

	TUniquePtr<Audio::FAudioCapture> AudioCapture;
	mutable FCriticalSection CaptureCriticalSection;
	TArray<float> CapturedSamples;
	int32 CaptureSampleRate = 48000;
	int32 CaptureNumChannels = 1;
	bool bRecording = false;
	bool bLoggedFirstCaptureBuffer = false;
	bool bTranscriptionInFlight = false;
	bool bSpeechInFlight = false;
	bool bHasPendingSpeech = false;
	bool bSpeechPlaybackActive = false;
	int32 LastSpeechByteCount = 0;
	TArray<uint8> LastRecordedWavData;
	double LastAcceptedSpeechTimeSeconds = -1.0;
	FString PendingSpeechText;
	FString LastAcceptedSpeechText;
	FString LastTranscribedText;
	FString LastVoiceError;
	FShowDownVoiceTextCallback PendingTranscriptionCallback;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActiveTranscriptionRequest;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActiveSpeechRequest;

	UPROPERTY(Transient)
	TObjectPtr<USoundWaveProcedural> ActiveSpeechWave;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ActiveSpeechComponent;

	FTimerHandle SpeechPlaybackFallbackTimerHandle;
};
