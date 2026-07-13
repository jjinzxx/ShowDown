#include "ShowDownVoiceSubsystem.h"

#include "Async/Async.h"
#include "Components/AudioComponent.h"
#include "Dom/JsonValue.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "JsonObjectConverter.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundWaveProcedural.h"
#include "TimerManager.h"

#if __has_include("SDLLMSecrets.h")
#include "SDLLMSecrets.h"
#endif

namespace
{
	const FString TranscriptionUrl = TEXT("https://api.openai.com/v1/audio/transcriptions");
	const FString SpeechUrl = TEXT("https://api.openai.com/v1/audio/speech");
	using FShowDownVoiceHttpRequestPtr = TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>;
	using FShowDownVoiceHttpResponsePtr = TSharedPtr<IHttpResponse, ESPMode::ThreadSafe>;

	void AppendUtf8(TArray<uint8>& OutBytes, const FString& Text)
	{
		FTCHARToUTF8 Converter(*Text);
		OutBytes.Append(reinterpret_cast<const uint8*>(Converter.Get()), Converter.Length());
	}

	void AppendUInt16LE(TArray<uint8>& OutBytes, uint16 Value)
	{
		OutBytes.Add(static_cast<uint8>(Value & 0xff));
		OutBytes.Add(static_cast<uint8>((Value >> 8) & 0xff));
	}

	void AppendUInt32LE(TArray<uint8>& OutBytes, uint32 Value)
	{
		OutBytes.Add(static_cast<uint8>(Value & 0xff));
		OutBytes.Add(static_cast<uint8>((Value >> 8) & 0xff));
		OutBytes.Add(static_cast<uint8>((Value >> 16) & 0xff));
		OutBytes.Add(static_cast<uint8>((Value >> 24) & 0xff));
	}

	uint16 ReadUInt16LE(const TArray<uint8>& Bytes, int32 Offset)
	{
		return static_cast<uint16>(Bytes[Offset] | (Bytes[Offset + 1] << 8));
	}

	uint32 ReadUInt32LE(const TArray<uint8>& Bytes, int32 Offset)
	{
		return static_cast<uint32>(Bytes[Offset])
			| (static_cast<uint32>(Bytes[Offset + 1]) << 8)
			| (static_cast<uint32>(Bytes[Offset + 2]) << 16)
			| (static_cast<uint32>(Bytes[Offset + 3]) << 24);
	}

	int16 FloatToInt16(float Value)
	{
		return static_cast<int16>(FMath::RoundToInt(FMath::Clamp(Value, -1.0f, 1.0f) * 32767.0f));
	}

	bool ConvertWavSamplesToInt16(
		const TArray<uint8>& Bytes,
		int32 DataOffset,
		uint32 DataSize,
		uint16 AudioFormat,
		uint16 BitsPerSample,
		TArray<uint8>& OutPcm16)
	{
		if (DataOffset < 0 || DataOffset >= Bytes.Num() || DataSize == 0)
		{
			return false;
		}

		const uint64 AvailableDataSize = static_cast<uint64>(Bytes.Num() - DataOffset);
		const uint64 RequestedDataSize = DataSize == MAX_uint32 ? AvailableDataSize : static_cast<uint64>(DataSize);
		const int32 SafeDataSize = static_cast<int32>(FMath::Min(RequestedDataSize, AvailableDataSize));

		if (AudioFormat == 1 && BitsPerSample == 16)
		{
			const int32 AlignedDataSize = SafeDataSize - (SafeDataSize % static_cast<int32>(sizeof(int16)));
			if (AlignedDataSize > 0)
			{
				OutPcm16.Append(Bytes.GetData() + DataOffset, AlignedDataSize);
			}
			return AlignedDataSize > 0;
		}

		const int32 BytesPerSample = BitsPerSample / 8;
		if (BytesPerSample <= 0)
		{
			return false;
		}

		const int32 SampleCount = SafeDataSize / BytesPerSample;
		if (SampleCount <= 0)
		{
			return false;
		}
		TArray<int16> ConvertedSamples;
		ConvertedSamples.Reserve(SampleCount);

		for (int32 SampleIndex = 0; SampleIndex < SampleCount; ++SampleIndex)
		{
			const int32 SampleOffset = DataOffset + SampleIndex * BytesPerSample;
			int16 ConvertedSample = 0;

			if (AudioFormat == 1)
			{
				if (BitsPerSample == 8)
				{
					const int32 UnsignedSample = Bytes[SampleOffset];
					ConvertedSample = static_cast<int16>((UnsignedSample - 128) << 8);
				}
				else if (BitsPerSample == 24)
				{
					int32 SignedSample = Bytes[SampleOffset]
						| (Bytes[SampleOffset + 1] << 8)
						| (Bytes[SampleOffset + 2] << 16);
					if (SignedSample & 0x00800000)
					{
						SignedSample |= 0xff000000;
					}
					ConvertedSample = static_cast<int16>(SignedSample >> 8);
				}
				else if (BitsPerSample == 32)
				{
					const int32 SignedSample = static_cast<int32>(ReadUInt32LE(Bytes, SampleOffset));
					ConvertedSample = static_cast<int16>(SignedSample >> 16);
				}
				else
				{
					return false;
				}
			}
			else if (AudioFormat == 3)
			{
				if (BitsPerSample == 32)
				{
					float FloatSample = 0.0f;
					FMemory::Memcpy(&FloatSample, Bytes.GetData() + SampleOffset, sizeof(float));
					ConvertedSample = FloatToInt16(FloatSample);
				}
				else if (BitsPerSample == 64)
				{
					double DoubleSample = 0.0;
					FMemory::Memcpy(&DoubleSample, Bytes.GetData() + SampleOffset, sizeof(double));
					ConvertedSample = FloatToInt16(static_cast<float>(DoubleSample));
				}
				else
				{
					return false;
				}
			}
			else
			{
				return false;
			}

			ConvertedSamples.Add(ConvertedSample);
		}

		OutPcm16.Append(reinterpret_cast<const uint8*>(ConvertedSamples.GetData()), ConvertedSamples.Num() * sizeof(int16));
		return true;
	}

	void BuildMultipartField(TArray<uint8>& Body, const FString& Boundary, const FString& Name, const FString& Value)
	{
		AppendUtf8(Body, FString::Printf(TEXT("--%s\r\n"), *Boundary));
		AppendUtf8(Body, FString::Printf(TEXT("Content-Disposition: form-data; name=\"%s\"\r\n\r\n"), *Name));
		AppendUtf8(Body, Value);
		AppendUtf8(Body, TEXT("\r\n"));
	}

	FString ResolveLocalVoicePath(const FString& Path)
	{
		const FString TrimmedPath = Path.TrimStartAndEnd();
		if (TrimmedPath.IsEmpty() || !FPaths::IsRelative(TrimmedPath))
		{
			return TrimmedPath;
		}

		return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), TrimmedPath);
	}

	FString ExtractWhisperCliText(const FString& Output)
	{
		TArray<FString> Lines;
		Output.ParseIntoArrayLines(Lines);

		FString Result;
		for (const FString& Line : Lines)
		{
			FString Left;
			FString Right;
			if (!Line.Split(TEXT("]"), &Left, &Right))
			{
				continue;
			}

			if (!Left.Contains(TEXT("-->")))
			{
				continue;
			}

			const FString Text = Right.TrimStartAndEnd();
			if (Text.IsEmpty())
			{
				continue;
			}

			if (!Result.IsEmpty())
			{
				Result += TEXT(" ");
			}
			Result += Text;
		}

		return Result.TrimStartAndEnd();
	}

	FString QuoteCommandLineArg(const FString& Arg)
	{
		return FString::Printf(TEXT("\"%s\""), *Arg.Replace(TEXT("\""), TEXT("\\\"")));
	}
}

bool UShowDownVoiceSubsystem::IsConfigured() const
{
	return CanUseTranscriptionBackend() || CanUseSpeechBackend();
}

bool UShowDownVoiceSubsystem::CanUseTranscriptionBackend() const
{
	switch (TranscriptionBackend)
	{
	case EShowDownTranscriptionBackend::LocalWhisper:
		return true;
	case EShowDownTranscriptionBackend::OpenAI:
	default:
		return bEnableOpenAIVoice && !ResolveApiKey().IsEmpty();
	}
}

bool UShowDownVoiceSubsystem::CanUseSpeechBackend() const
{
	if (!bEnableOpenAIVoice)
	{
		return false;
	}

	switch (SpeechBackend)
	{
	case EShowDownSpeechBackend::LocalMeloTTS:
		return true;
	case EShowDownSpeechBackend::OpenAI:
	default:
		return !ResolveApiKey().IsEmpty();
	}
}

int32 UShowDownVoiceSubsystem::GetCapturedSampleCount() const
{
	FScopeLock Lock(&CaptureCriticalSection);
	return CapturedSamples.Num();
}

float UShowDownVoiceSubsystem::GetCurrentRecordingSeconds() const
{
	FScopeLock Lock(&CaptureCriticalSection);
	const int32 SafeSampleRate = FMath::Max(1, CaptureSampleRate);
	const int32 SafeNumChannels = FMath::Max(1, CaptureNumChannels);
	return static_cast<float>(CapturedSamples.Num()) / static_cast<float>(SafeSampleRate * SafeNumChannels);
}

FString UShowDownVoiceSubsystem::GetVoiceDebugSummary() const
{
	return FString::Printf(
		TEXT("Voice enabled=%s configured=%s transcription_backend=%d speech_backend=%d mode=%d recording=%s stt=%s tts=%s pending_tts=%s recorded=%.2fs samples=%d STT=%s TTS=%s voice=%s speed=%.2f pitch=%.2f volume=%.2f last_text=\"%s\" last_error=\"%s\" last_recording_bytes=%d last_tts_bytes=%d"),
		bEnableOpenAIVoice ? TEXT("true") : TEXT("false"),
		IsConfigured() ? TEXT("true") : TEXT("false"),
		static_cast<int32>(TranscriptionBackend),
		static_cast<int32>(SpeechBackend),
		static_cast<int32>(VoiceInputMode),
		IsRecording() ? TEXT("true") : TEXT("false"),
		IsTranscriptionInFlight() ? TEXT("true") : TEXT("false"),
		IsSpeechInFlight() ? TEXT("true") : TEXT("false"),
		HasPendingSpeech() ? TEXT("true") : TEXT("false"),
		GetCurrentRecordingSeconds(),
		GetCapturedSampleCount(),
		*TranscriptionModel,
		*TTSModel,
		*TTSVoice,
		TTSPlaybackSpeed,
		TTSPlaybackPitch,
		TTSPlaybackVolume,
		*LastTranscribedText.Left(48),
		*LastVoiceError.Left(80),
		LastRecordedWavData.Num(),
		LastSpeechByteCount);
}

void UShowDownVoiceSubsystem::Deinitialize()
{
	CancelPushToTalk();
	if (ActiveTranscriptionRequest.IsValid())
	{
		ActiveTranscriptionRequest->OnProcessRequestComplete().Unbind();
		ActiveTranscriptionRequest->CancelRequest();
		ActiveTranscriptionRequest.Reset();
	}
	if (ActiveSpeechRequest.IsValid())
	{
		ActiveSpeechRequest->OnProcessRequestComplete().Unbind();
		ActiveSpeechRequest->CancelRequest();
		ActiveSpeechRequest.Reset();
	}
	bTranscriptionInFlight = false;
	bSpeechInFlight = false;
	PendingTranscriptionCallback.Unbind();
	PendingSpeechText.Empty();
	bHasPendingSpeech = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpeechPlaybackFallbackTimerHandle);
	}
	if (ActiveSpeechComponent)
	{
		ActiveSpeechComponent->OnAudioFinished.RemoveDynamic(this, &UShowDownVoiceSubsystem::HandleSpeechAudioFinished);
		ActiveSpeechComponent->Stop();
		ActiveSpeechComponent->DestroyComponent();
		ActiveSpeechComponent = nullptr;
	}
	BroadcastSpeechPlaybackState(false);
	Super::Deinitialize();
}

bool UShowDownVoiceSubsystem::BeginPushToTalk(FShowDownVoiceTextCallback Callback)
{
	if (VoiceInputMode == EShowDownVoiceInputMode::Off)
	{
		LastVoiceError = TEXT("Voice input mode is off.");
		BroadcastVoiceStatus(false, TEXT("음성 꺼짐."));
		return false;
	}

	if (!CanUseTranscriptionBackend())
	{
		LastVoiceError = TEXT("Voice transcription backend is not configured.");
		BroadcastVoiceStatus(false, TEXT("음성 인식 설정 없음."));
		return false;
	}

	if (bRecording || bTranscriptionInFlight)
	{
		LastVoiceError = TEXT("Voice recognition is already running.");
		BroadcastVoiceStatus(true, TEXT("인식 중..."));
		return false;
	}

	LastVoiceError.Empty();
	PendingTranscriptionCallback = Callback;
	CapturedSamples.Reset();
	CaptureSampleRate = 48000;
	CaptureNumChannels = 1;
	bLoggedFirstCaptureBuffer = false;

	OpenAndStartCaptureStream();
	if (!bRecording)
	{
		PendingTranscriptionCallback.Unbind();
		return false;
	}

	BroadcastVoiceStatus(true, TEXT("듣는 중..."));
	return true;
}

void UShowDownVoiceSubsystem::EndPushToTalk()
{
	if (!bRecording)
	{
		return;
	}

	bRecording = false;
	if (AudioCapture)
	{
		if (AudioCapture->IsCapturing())
		{
			AudioCapture->StopStream();
		}
		if (AudioCapture->IsStreamOpen())
		{
			AudioCapture->CloseStream();
		}
		AudioCapture.Reset();
	}

	TArray<uint8> WavData;
	float DurationSeconds = 0.0f;
	if (!BuildRecordedWav(WavData, DurationSeconds))
	{
		LastVoiceError = TEXT("Failed to build recording WAV.");
		BroadcastVoiceStatus(false, TEXT("녹음 파일 생성 실패."));
		PendingTranscriptionCallback.ExecuteIfBound(false, FString());
		PendingTranscriptionCallback.Unbind();
		return;
	}

	LastRecordedWavData = WavData;
	if (DurationSeconds < MinimumRecordingSeconds)
	{
		LastVoiceError = TEXT("Recording is too short.");
		BroadcastVoiceStatus(false, TEXT("녹음이 너무 짧습니다."));
		PendingTranscriptionCallback.ExecuteIfBound(false, FString());
		PendingTranscriptionCallback.Unbind();
		return;
	}

	BroadcastVoiceStatus(true, TEXT("인식 중..."));
	RequestTranscription(MoveTemp(WavData));
}

bool UShowDownVoiceSubsystem::SaveLastRecordingWav(FString& OutFilePath)
{
	OutFilePath.Empty();
	if (LastRecordedWavData.IsEmpty())
	{
		LastVoiceError = TEXT("No recorded WAV is available.");
		return false;
	}

	const FString DebugDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("VoiceDebug"));
	if (!IFileManager::Get().MakeDirectory(*DebugDirectory, true))
	{
		LastVoiceError = FString::Printf(TEXT("Failed to create voice debug directory: %s"), *DebugDirectory);
		return false;
	}

	OutFilePath = FPaths::Combine(DebugDirectory, TEXT("LastRecording.wav"));
	if (!FFileHelper::SaveArrayToFile(LastRecordedWavData, *OutFilePath))
	{
		LastVoiceError = FString::Printf(TEXT("Failed to save recording WAV: %s"), *OutFilePath);
		OutFilePath.Empty();
		return false;
	}

	LastVoiceError.Empty();
	UE_LOG(LogTemp, Log, TEXT("Saved last voice recording: %s (%d bytes)"), *OutFilePath, LastRecordedWavData.Num());
	return true;
}

bool UShowDownVoiceSubsystem::TranscribeWavFileForDebug(const FString& FilePath, FShowDownVoiceTextCallback Callback)
{
	if (!CanUseTranscriptionBackend())
	{
		LastVoiceError = TEXT("Voice transcription backend is not configured.");
		return false;
	}

	if (bRecording || bTranscriptionInFlight)
	{
		LastVoiceError = TEXT("Voice recognition is already running.");
		return false;
	}

	const FString TrimmedPath = FilePath.TrimStartAndEnd();
	const FString ResolvedPath = FPaths::IsRelative(TrimmedPath)
		? FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), TrimmedPath)
		: TrimmedPath;

	TArray<uint8> WavData;
	if (!FFileHelper::LoadFileToArray(WavData, *ResolvedPath) || WavData.Num() < 44)
	{
		LastVoiceError = FString::Printf(TEXT("Could not load debug WAV: %s"), *ResolvedPath);
		return false;
	}

	constexpr int32 MaximumDebugWavBytes = 25 * 1024 * 1024;
	if (WavData.Num() > MaximumDebugWavBytes)
	{
		LastVoiceError = TEXT("Debug WAV exceeds 25 MB.");
		return false;
	}

	LastRecordedWavData = WavData;
	PendingTranscriptionCallback = MoveTemp(Callback);
	LastVoiceError.Empty();
	BroadcastVoiceStatus(true, TEXT("인식 중..."));
	UE_LOG(LogTemp, Log, TEXT("Submitting debug WAV for transcription: %s (%d bytes)"), *ResolvedPath, WavData.Num());
	RequestTranscription(MoveTemp(WavData));
	return true;
}

void UShowDownVoiceSubsystem::CancelPushToTalk()
{
	if (!bRecording)
	{
		return;
	}

	bRecording = false;
	if (AudioCapture)
	{
		AudioCapture->AbortStream();
		AudioCapture.Reset();
	}

	{
		FScopeLock Lock(&CaptureCriticalSection);
		CapturedSamples.Reset();
	}
	PendingTranscriptionCallback.Unbind();
	LastVoiceError = TEXT("Recording canceled.");
	BroadcastVoiceStatus(false, TEXT("녹음 취소."));
}

void UShowDownVoiceSubsystem::SpeakCollectorLine(const FString& Text)
{
	const FString TrimmedText = Text.TrimStartAndEnd();
	if (TrimmedText.IsEmpty() || !CanUseSpeechBackend())
	{
		return;
	}

	const FString SpeechText = TrimmedText.Left(180);
	const double CurrentTimeSeconds = FPlatformTime::Seconds();
	constexpr double DuplicateSpeechWindowSeconds = 0.5;
	if (SpeechText.Equals(LastAcceptedSpeechText, ESearchCase::CaseSensitive)
		&& LastAcceptedSpeechTimeSeconds >= 0.0
		&& (CurrentTimeSeconds - LastAcceptedSpeechTimeSeconds) <= DuplicateSpeechWindowSeconds)
	{
		UE_LOG(LogTemp, Log, TEXT("Ignoring duplicate collector speech: %s"), *SpeechText);
		return;
	}

	LastAcceptedSpeechText = SpeechText;
	LastAcceptedSpeechTimeSeconds = CurrentTimeSeconds;
	if (bSpeechInFlight)
	{
		UE_LOG(LogTemp, Log, TEXT("Replacing pending collector speech: %s"), *SpeechText);
		PendingSpeechText = SpeechText;
		bHasPendingSpeech = true;
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Queueing collector speech: %s"), *SpeechText);
	RequestSpeech(SpeechText);
}

void UShowDownVoiceSubsystem::PlayDebugTone(float FrequencyHz, float DurationSeconds)
{
	constexpr int32 SampleRate = 24000;
	constexpr int32 NumChannels = 1;
	const float ClampedFrequency = FMath::Clamp(FrequencyHz, 60.0f, 4000.0f);
	const float ClampedDuration = FMath::Clamp(DurationSeconds, 0.05f, 5.0f);
	const int32 NumFrames = FMath::Max(1, FMath::RoundToInt(SampleRate * ClampedDuration));

	TArray<int16> Samples;
	Samples.Reserve(NumFrames);
	for (int32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex)
	{
		const float Phase = static_cast<float>(FrameIndex) * ClampedFrequency * 2.0f * PI / static_cast<float>(SampleRate);
		const float FadeIn = FMath::Clamp(static_cast<float>(FrameIndex) / 240.0f, 0.0f, 1.0f);
		const float FadeOut = FMath::Clamp(static_cast<float>(NumFrames - FrameIndex) / 240.0f, 0.0f, 1.0f);
		Samples.Add(FloatToInt16(FMath::Sin(Phase) * 0.22f * FMath::Min(FadeIn, FadeOut)));
	}

	const uint16 BitsPerSample = 16;
	const uint32 DataSize = Samples.Num() * sizeof(int16);
	const uint32 ByteRate = SampleRate * NumChannels * BitsPerSample / 8;
	const uint16 BlockAlign = NumChannels * BitsPerSample / 8;

	TArray<uint8> WavData;
	AppendUtf8(WavData, TEXT("RIFF"));
	AppendUInt32LE(WavData, 36 + DataSize);
	AppendUtf8(WavData, TEXT("WAVE"));
	AppendUtf8(WavData, TEXT("fmt "));
	AppendUInt32LE(WavData, 16);
	AppendUInt16LE(WavData, 1);
	AppendUInt16LE(WavData, NumChannels);
	AppendUInt32LE(WavData, SampleRate);
	AppendUInt32LE(WavData, ByteRate);
	AppendUInt16LE(WavData, BlockAlign);
	AppendUInt16LE(WavData, BitsPerSample);
	AppendUtf8(WavData, TEXT("data"));
	AppendUInt32LE(WavData, DataSize);
	WavData.Append(reinterpret_cast<const uint8*>(Samples.GetData()), DataSize);

	LastSpeechByteCount = WavData.Num();
	PlaySpeechWav(WavData);
}

void UShowDownVoiceSubsystem::SetVoiceInputMode(EShowDownVoiceInputMode NewMode)
{
	if (VoiceInputMode == NewMode)
	{
		return;
	}

	if (bRecording)
	{
		CancelPushToTalk();
	}

	VoiceInputMode = NewMode;
}

void UShowDownVoiceSubsystem::SetOpenAIVoiceEnabled(bool bInEnableOpenAIVoice)
{
	bEnableOpenAIVoice = bInEnableOpenAIVoice;
	if (!bEnableOpenAIVoice)
	{
		CancelPushToTalk();
		if (ActiveTranscriptionRequest.IsValid())
		{
			ActiveTranscriptionRequest->OnProcessRequestComplete().Unbind();
			ActiveTranscriptionRequest->CancelRequest();
			ActiveTranscriptionRequest.Reset();
		}
		if (ActiveSpeechRequest.IsValid())
		{
			ActiveSpeechRequest->OnProcessRequestComplete().Unbind();
			ActiveSpeechRequest->CancelRequest();
			ActiveSpeechRequest.Reset();
		}
		bTranscriptionInFlight = false;
		bSpeechInFlight = false;
		PendingTranscriptionCallback.ExecuteIfBound(false, FString());
		PendingTranscriptionCallback.Unbind();
		PendingSpeechText.Empty();
		bHasPendingSpeech = false;
		LastAcceptedSpeechText.Empty();
		LastAcceptedSpeechTimeSeconds = -1.0;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(SpeechPlaybackFallbackTimerHandle);
		}
		if (ActiveSpeechComponent)
		{
			ActiveSpeechComponent->OnAudioFinished.RemoveDynamic(this, &UShowDownVoiceSubsystem::HandleSpeechAudioFinished);
			ActiveSpeechComponent->Stop();
			ActiveSpeechComponent->DestroyComponent();
			ActiveSpeechComponent = nullptr;
		}
		ActiveSpeechWave = nullptr;
		BroadcastSpeechPlaybackState(false);
	}
}

FString UShowDownVoiceSubsystem::ResolveApiKey() const
{
	FString Key = FPlatformMisc::GetEnvironmentVariable(*ApiKeyEnvironmentVariable).TrimStartAndEnd();

#ifdef SHOWDOWN_LLM_API_KEY
	if (Key.IsEmpty())
	{
		Key = FString(SHOWDOWN_LLM_API_KEY).TrimStartAndEnd();
	}
#endif

	return Key;
}

void UShowDownVoiceSubsystem::BroadcastVoiceStatus(bool bSuccess, const FString& Message)
{
	OnVoiceStatus.Broadcast(bSuccess, Message);
}

void UShowDownVoiceSubsystem::BroadcastSpeechPlaybackState(bool bIsSpeaking)
{
	if (!bIsSpeaking)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(SpeechPlaybackFallbackTimerHandle);
		}
	}

	if (bSpeechPlaybackActive == bIsSpeaking)
	{
		return;
	}

	bSpeechPlaybackActive = bIsSpeaking;
	OnSpeechPlaybackStateChanged.Broadcast(bIsSpeaking);
}

void UShowDownVoiceSubsystem::OpenAndStartCaptureStream()
{
	FModuleManager::Get().LoadModule(FName(TEXT("AudioCapture")));

	FString CaptureImplementationModule;
	if (GConfig)
	{
		GConfig->GetString(TEXT("Audio"), TEXT("AudioCaptureModuleName"), CaptureImplementationModule, GEngineIni);
	}
#if PLATFORM_WINDOWS
	if (CaptureImplementationModule.IsEmpty())
	{
		CaptureImplementationModule = TEXT("AudioCaptureWasapi");
	}
#endif

	if (!CaptureImplementationModule.IsEmpty())
	{
		if (FModuleManager::Get().LoadModule(FName(*CaptureImplementationModule)))
		{
			UE_LOG(LogTemp, Log, TEXT("Loaded audio capture implementation: %s"), *CaptureImplementationModule);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Could not load audio capture implementation: %s"), *CaptureImplementationModule);
		}
	}

	AudioCapture = MakeUnique<Audio::FAudioCapture>();

	Audio::FAudioCaptureDeviceParams Params;
	Params.DeviceIndex = Audio::DefaultDeviceIndex;
	Params.NumInputChannels = Audio::InvalidDeviceChannelCount;
	Params.SampleRate = Audio::InvalidDeviceSampleRate;
	Params.PCMAudioEncoding = Audio::DefaultDeviceEncoding;

	Audio::FCaptureDeviceInfo SelectedDeviceInfo;
	if (!AudioCapture->GetCaptureDeviceInfo(SelectedDeviceInfo, Params.DeviceIndex))
	{
		TArray<Audio::FCaptureDeviceInfo> CaptureDevices;
		AudioCapture->GetCaptureDevicesAvailable(CaptureDevices);
		UE_LOG(LogTemp, Log, TEXT("Default microphone unavailable; enumerated %d capture device(s)."), CaptureDevices.Num());
		for (int32 DeviceIndex = 0; DeviceIndex < CaptureDevices.Num(); ++DeviceIndex)
		{
			const Audio::FCaptureDeviceInfo& Device = CaptureDevices[DeviceIndex];
			UE_LOG(
				LogTemp,
				Log,
				TEXT("Microphone [%d]: %s, %d Hz, %d channel(s)."),
				DeviceIndex,
				*Device.DeviceName,
				Device.PreferredSampleRate,
				Device.InputChannels);
		}

		if (!CaptureDevices.IsEmpty())
		{
			Params.DeviceIndex = 0;
			SelectedDeviceInfo = CaptureDevices[0];
		}
		else
		{
			LastVoiceError = TEXT("No active microphone capture device was found.");
			BroadcastVoiceStatus(false, TEXT("활성 마이크가 없습니다."));
			AudioCapture.Reset();
			return;
		}
	}

	TWeakObjectPtr<UShowDownVoiceSubsystem> WeakThis(this);
	Audio::FOnAudioCaptureFunction OnCapture = [WeakThis](const void* AudioData, int32 NumFrames, int32 NumChannels, int32 SampleRate, double StreamTime, bool bOverflow)
	{
		if (UShowDownVoiceSubsystem* VoiceSubsystem = WeakThis.Get())
		{
			VoiceSubsystem->HandleCapturedAudio(AudioData, NumFrames, NumChannels, SampleRate);
		}
	};

	if (!AudioCapture->OpenAudioCaptureStream(Params, MoveTemp(OnCapture), 1024))
	{
		LastVoiceError = TEXT("Could not open microphone.");
		BroadcastVoiceStatus(false, TEXT("마이크를 열 수 없습니다."));
		AudioCapture.Reset();
		return;
	}

	if (SelectedDeviceInfo.DeviceName.IsEmpty())
	{
		AudioCapture->GetCaptureDeviceInfo(SelectedDeviceInfo, Params.DeviceIndex);
	}
	if (!SelectedDeviceInfo.DeviceName.IsEmpty())
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("Opened microphone: %s, %d Hz, %d channel(s)."),
			*SelectedDeviceInfo.DeviceName,
			SelectedDeviceInfo.PreferredSampleRate,
			SelectedDeviceInfo.InputChannels);
	}

	if (!AudioCapture->StartStream())
	{
		LastVoiceError = TEXT("Could not start microphone capture.");
		BroadcastVoiceStatus(false, TEXT("마이크 녹음을 시작할 수 없습니다."));
		AudioCapture->CloseStream();
		AudioCapture.Reset();
		return;
	}

	bRecording = true;
}

void UShowDownVoiceSubsystem::HandleCapturedAudio(const void* AudioData, int32 NumFrames, int32 NumChannels, int32 SampleRate)
{
	if (!bRecording || !AudioData || NumFrames <= 0 || NumChannels <= 0 || SampleRate <= 0)
	{
		return;
	}

	const int32 MaxSamples = FMath::Max(1, FMath::RoundToInt(MaximumRecordingSeconds * SampleRate * NumChannels));
	const float* FloatAudio = static_cast<const float*>(AudioData);
	const int32 IncomingSamples = NumFrames * NumChannels;
	bool bReachedLimit = false;
	if (!bLoggedFirstCaptureBuffer)
	{
		bLoggedFirstCaptureBuffer = true;
		UE_LOG(
			LogTemp,
			Log,
			TEXT("Voice capture received first buffer: %d frame(s), %d channel(s), %d Hz."),
			NumFrames,
			NumChannels,
			SampleRate);
	}

	{
		FScopeLock Lock(&CaptureCriticalSection);
		CaptureSampleRate = SampleRate;
		CaptureNumChannels = NumChannels;

		const int32 RemainingSamples = FMath::Max(0, MaxSamples - CapturedSamples.Num());
		const int32 SamplesToCopy = FMath::Min(RemainingSamples, IncomingSamples);
		if (SamplesToCopy > 0)
		{
			CapturedSamples.Append(FloatAudio, SamplesToCopy);
		}
		bReachedLimit = CapturedSamples.Num() >= MaxSamples;
	}

	if (bReachedLimit)
	{
		TWeakObjectPtr<UShowDownVoiceSubsystem> WeakThis(this);
		AsyncTask(ENamedThreads::GameThread, [WeakThis]()
		{
			if (UShowDownVoiceSubsystem* VoiceSubsystem = WeakThis.Get())
			{
				VoiceSubsystem->EndPushToTalk();
			}
		});
	}
}

void UShowDownVoiceSubsystem::RequestTranscription(TArray<uint8>&& WavData)
{
	if (TranscriptionBackend == EShowDownTranscriptionBackend::LocalWhisper)
	{
		RequestLocalTranscription(MoveTemp(WavData));
		return;
	}

	if (!CanUseTranscriptionBackend())
	{
		LastVoiceError = TEXT("OpenAI voice API key is missing.");
		BroadcastVoiceStatus(false, TEXT("음성 API 키 없음."));
		PendingTranscriptionCallback.ExecuteIfBound(false, FString());
		PendingTranscriptionCallback.Unbind();
		return;
	}

	bTranscriptionInFlight = true;

	const FString Boundary = FString::Printf(TEXT("----ShowDownVoiceBoundary%d"), FMath::Rand());
	TArray<uint8> Body;
	BuildMultipartField(Body, Boundary, TEXT("model"), TranscriptionModel);
	if (!TranscriptionLanguage.IsEmpty())
	{
		BuildMultipartField(Body, Boundary, TEXT("language"), TranscriptionLanguage);
	}
	BuildMultipartField(Body, Boundary, TEXT("response_format"), TEXT("json"));

	AppendUtf8(Body, FString::Printf(TEXT("--%s\r\n"), *Boundary));
	AppendUtf8(Body, TEXT("Content-Disposition: form-data; name=\"file\"; filename=\"showdown_voice.wav\"\r\n"));
	AppendUtf8(Body, TEXT("Content-Type: audio/wav\r\n\r\n"));
	Body.Append(WavData);
	AppendUtf8(Body, TEXT("\r\n"));
	AppendUtf8(Body, FString::Printf(TEXT("--%s--\r\n"), *Boundary));

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	ActiveTranscriptionRequest = Request;
	Request->SetURL(TranscriptionUrl);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ResolveApiKey()));
	Request->SetHeader(TEXT("Content-Type"), FString::Printf(TEXT("multipart/form-data; boundary=%s"), *Boundary));
	Request->SetTimeout(RequestTimeoutSeconds);
	Request->SetContent(Body);

	Request->OnProcessRequestComplete().BindWeakLambda(
		this,
		[this](FShowDownVoiceHttpRequestPtr RequestPtr, FShowDownVoiceHttpResponsePtr ResponsePtr, bool bWasSuccessful)
		{
			if (ActiveTranscriptionRequest == RequestPtr)
			{
				ActiveTranscriptionRequest.Reset();
			}
			bTranscriptionInFlight = false;
			if (!bEnableOpenAIVoice)
			{
				PendingTranscriptionCallback.ExecuteIfBound(false, FString());
				PendingTranscriptionCallback.Unbind();
				return;
			}

			FString TranscribedText;
			const bool bHttpOk = bWasSuccessful
				&& ResponsePtr.IsValid()
				&& EHttpResponseCodes::IsOk(ResponsePtr->GetResponseCode());
			const bool bParsed = bHttpOk && ParseTranscriptionResponse(ResponsePtr->GetContentAsString(), TranscribedText);
			if (!bParsed)
			{
				const int32 ResponseCode = ResponsePtr.IsValid() ? ResponsePtr->GetResponseCode() : 0;
				const FString ResponsePreview = ResponsePtr.IsValid() ? ResponsePtr->GetContentAsString().Left(512) : TEXT("No HTTP response");
				LastVoiceError = FString::Printf(TEXT("STT failed. HTTP %d. %s"), ResponseCode, *ResponsePreview.Left(160));
				UE_LOG(LogTemp, Warning, TEXT("Voice transcription failed. HTTP %d. Body: %s"), ResponseCode, *ResponsePreview);
				BroadcastVoiceStatus(false, TEXT("음성 인식 실패."));
			}
			else
			{
				LastTranscribedText = TranscribedText;
				LastVoiceError.Empty();
				UE_LOG(LogTemp, Log, TEXT("Voice transcription: %s"), *TranscribedText);
			}

			PendingTranscriptionCallback.ExecuteIfBound(bParsed, TranscribedText);
			PendingTranscriptionCallback.Unbind();
		});

	if (!Request->ProcessRequest())
	{
		ActiveTranscriptionRequest.Reset();
		bTranscriptionInFlight = false;
		LastVoiceError = TEXT("Could not start STT request.");
		BroadcastVoiceStatus(false, TEXT("음성 인식 요청 실패."));
		PendingTranscriptionCallback.ExecuteIfBound(false, FString());
		PendingTranscriptionCallback.Unbind();
	}
}

void UShowDownVoiceSubsystem::RequestLocalTranscription(TArray<uint8>&& WavData)
{
	const FString ResolvedExecutablePath = ResolveLocalVoicePath(LocalSTTExecutablePath);
	const FString ResolvedModelPath = ResolveLocalVoicePath(LocalSTTModelPath);
	if (ResolvedExecutablePath.IsEmpty() || !FPaths::FileExists(ResolvedExecutablePath))
	{
		LastVoiceError = FString::Printf(TEXT("Local STT executable is missing: %s"), *ResolvedExecutablePath);
		UE_LOG(LogTemp, Warning, TEXT("%s"), *LastVoiceError);
		BroadcastVoiceStatus(false, TEXT("로컬 STT 실행 파일 없음."));
		PendingTranscriptionCallback.ExecuteIfBound(false, FString());
		PendingTranscriptionCallback.Unbind();
		return;
	}

	if (ResolvedModelPath.IsEmpty() || !FPaths::FileExists(ResolvedModelPath))
	{
		LastVoiceError = FString::Printf(TEXT("Local STT model is missing: %s"), *ResolvedModelPath);
		UE_LOG(LogTemp, Warning, TEXT("%s"), *LastVoiceError);
		BroadcastVoiceStatus(false, TEXT("로컬 STT 모델 없음."));
		PendingTranscriptionCallback.ExecuteIfBound(false, FString());
		PendingTranscriptionCallback.Unbind();
		return;
	}

	const FString STTDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("LocalVoice"), TEXT("STT"));
	if (!IFileManager::Get().MakeDirectory(*STTDirectory, true))
	{
		LastVoiceError = FString::Printf(TEXT("Failed to create local STT directory: %s"), *STTDirectory);
		UE_LOG(LogTemp, Warning, TEXT("%s"), *LastVoiceError);
		BroadcastVoiceStatus(false, TEXT("로컬 STT 폴더 생성 실패."));
		PendingTranscriptionCallback.ExecuteIfBound(false, FString());
		PendingTranscriptionCallback.Unbind();
		return;
	}

	const FString WavFilePath = FPaths::Combine(STTDirectory, FString::Printf(TEXT("showdown_stt_%s.wav"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	if (!FFileHelper::SaveArrayToFile(WavData, *WavFilePath))
	{
		LastVoiceError = FString::Printf(TEXT("Failed to save local STT WAV: %s"), *WavFilePath);
		UE_LOG(LogTemp, Warning, TEXT("%s"), *LastVoiceError);
		BroadcastVoiceStatus(false, TEXT("로컬 STT 녹음 저장 실패."));
		PendingTranscriptionCallback.ExecuteIfBound(false, FString());
		PendingTranscriptionCallback.Unbind();
		return;
	}

	bTranscriptionInFlight = true;
	BroadcastVoiceStatus(true, TEXT("로컬 음성 인식 중..."));

	const FString LanguageArg = TranscriptionLanguage.TrimStartAndEnd().IsEmpty()
		? FString()
		: FString::Printf(TEXT(" -l \"%s\""), *TranscriptionLanguage.TrimStartAndEnd());
	const FString Args = FString::Printf(TEXT("-m \"%s\" -f \"%s\"%s"), *ResolvedModelPath, *WavFilePath, *LanguageArg);

	int32 ReturnCode = -1;
	FString StdOut;
	FString StdErr;
	const bool bExecuted = FPlatformProcess::ExecProcess(*ResolvedExecutablePath, *Args, &ReturnCode, &StdOut, &StdErr);
	bTranscriptionInFlight = false;

	const FString CombinedOutput = StdOut + TEXT("\n") + StdErr;
	const FString TranscribedText = ExtractWhisperCliText(CombinedOutput);
	const bool bSuccess = bExecuted && ReturnCode == 0 && !TranscribedText.IsEmpty();
	if (!bSuccess)
	{
		LastVoiceError = FString::Printf(
			TEXT("Local STT failed. executed=%s code=%d output=%s"),
			bExecuted ? TEXT("true") : TEXT("false"),
			ReturnCode,
			*CombinedOutput.Left(256));
		UE_LOG(LogTemp, Warning, TEXT("%s"), *LastVoiceError);
		BroadcastVoiceStatus(false, TEXT("로컬 STT 실패."));
		PendingTranscriptionCallback.ExecuteIfBound(false, FString());
		PendingTranscriptionCallback.Unbind();
		return;
	}

	LastTranscribedText = TranscribedText;
	LastVoiceError.Empty();
	UE_LOG(LogTemp, Log, TEXT("Local voice transcription: %s"), *TranscribedText);
	PendingTranscriptionCallback.ExecuteIfBound(true, TranscribedText);
	PendingTranscriptionCallback.Unbind();
}

void UShowDownVoiceSubsystem::RequestSpeech(const FString& Text)
{
	if (!CanUseSpeechBackend())
	{
		LastVoiceError = TEXT("Voice speech backend is not configured.");
		bSpeechInFlight = false;
		PendingSpeechText.Empty();
		bHasPendingSpeech = false;
		return;
	}

	if (SpeechBackend == EShowDownSpeechBackend::LocalMeloTTS)
	{
		RequestLocalSpeech(Text);
		return;
	}

	bSpeechInFlight = true;
	UE_LOG(LogTemp, Log, TEXT("Requesting collector TTS: %s"), *Text);

	TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetStringField(TEXT("model"), TTSModel);
	RootObject->SetStringField(TEXT("voice"), TTSVoice);
	RootObject->SetStringField(TEXT("input"), Text);
	if (!TTSInstructions.TrimStartAndEnd().IsEmpty())
	{
		RootObject->SetStringField(TEXT("instructions"), TTSInstructions);
	}
	RootObject->SetStringField(TEXT("response_format"), TEXT("wav"));
	const double SafePlaybackPitch = FMath::Clamp(static_cast<double>(TTSPlaybackPitch), 0.5, 2.0);
	const double RawApiSpeechSpeed = FMath::Clamp(static_cast<double>(TTSPlaybackSpeed) / SafePlaybackPitch, 0.25, 4.0);
	const double ApiSpeechSpeed = FMath::RoundToDouble(RawApiSpeechSpeed * 100.0) / 100.0;
	const FString ApiSpeechSpeedJson = FString::Printf(TEXT("%.2f"), ApiSpeechSpeed);
	RootObject->SetField(TEXT("speed"), MakeShared<FJsonValueNumberString>(ApiSpeechSpeedJson));
	UE_LOG(
		LogTemp,
		Log,
		TEXT("TTS controls: target speed=%.2f pitch=%.2f API speed=%.2f"),
		TTSPlaybackSpeed,
		SafePlaybackPitch,
		ApiSpeechSpeed);

	FString Body;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(RootObject, Writer);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	ActiveSpeechRequest = Request;
	Request->SetURL(SpeechUrl);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ResolveApiKey()));
	Request->SetTimeout(RequestTimeoutSeconds);
	Request->SetContentAsString(Body);

	Request->OnProcessRequestComplete().BindWeakLambda(
		this,
		[this](FShowDownVoiceHttpRequestPtr RequestPtr, FShowDownVoiceHttpResponsePtr ResponsePtr, bool bWasSuccessful)
		{
			if (ActiveSpeechRequest == RequestPtr)
			{
				ActiveSpeechRequest.Reset();
			}
			bSpeechInFlight = false;
			if (!bEnableOpenAIVoice)
			{
				return;
			}

			const bool bHttpOk = bWasSuccessful
				&& ResponsePtr.IsValid()
				&& EHttpResponseCodes::IsOk(ResponsePtr->GetResponseCode());

			if (!bHttpOk)
			{
				const int32 ResponseCode = ResponsePtr.IsValid() ? ResponsePtr->GetResponseCode() : 0;
				const FString ResponsePreview = ResponsePtr.IsValid() ? ResponsePtr->GetContentAsString().Left(512) : TEXT("No HTTP response");
				LastVoiceError = FString::Printf(TEXT("TTS failed. HTTP %d. %s"), ResponseCode, *ResponsePreview.Left(160));
				UE_LOG(LogTemp, Warning, TEXT("Voice speech failed. HTTP %d. Body: %s"), ResponseCode, *ResponsePreview);
				RequestPendingSpeech();
				return;
			}

			LastSpeechByteCount = ResponsePtr->GetContent().Num();
			LastVoiceError.Empty();
			UE_LOG(LogTemp, Log, TEXT("Voice speech received %d bytes."), LastSpeechByteCount);
			PlaySpeechWav(ResponsePtr->GetContent());
			RequestPendingSpeech();
		});

	if (!Request->ProcessRequest())
	{
		ActiveSpeechRequest.Reset();
		bSpeechInFlight = false;
		LastVoiceError = TEXT("Could not start TTS request.");
		RequestPendingSpeech();
	}
}

void UShowDownVoiceSubsystem::RequestLocalSpeech(const FString& Text)
{
	const FString ResolvedExecutablePath = ResolveLocalVoicePath(LocalTTSExecutablePath);
	const FString ResolvedScriptPath = ResolveLocalVoicePath(LocalTTSScriptPath);
	const FString ResolvedCachePath = ResolveLocalVoicePath(LocalTTSCachePath);
	if (ResolvedExecutablePath.IsEmpty() || !FPaths::FileExists(ResolvedExecutablePath))
	{
		LastVoiceError = FString::Printf(TEXT("Local TTS executable is missing: %s"), *ResolvedExecutablePath);
		UE_LOG(LogTemp, Warning, TEXT("%s"), *LastVoiceError);
		RequestPendingSpeech();
		return;
	}

	if (ResolvedScriptPath.IsEmpty() || !FPaths::FileExists(ResolvedScriptPath))
	{
		LastVoiceError = FString::Printf(TEXT("Local TTS script is missing: %s"), *ResolvedScriptPath);
		UE_LOG(LogTemp, Warning, TEXT("%s"), *LastVoiceError);
		RequestPendingSpeech();
		return;
	}

	const FString TTSDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("LocalVoice"), TEXT("TTS"));
	if (!IFileManager::Get().MakeDirectory(*TTSDirectory, true))
	{
		LastVoiceError = FString::Printf(TEXT("Failed to create local TTS directory: %s"), *TTSDirectory);
		UE_LOG(LogTemp, Warning, TEXT("%s"), *LastVoiceError);
		RequestPendingSpeech();
		return;
	}

	const FString RequestId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString TextFilePath = FPaths::Combine(TTSDirectory, FString::Printf(TEXT("showdown_tts_%s.txt"), *RequestId));
	const FString WavFilePath = FPaths::Combine(TTSDirectory, FString::Printf(TEXT("showdown_tts_%s.wav"), *RequestId));
	if (!FFileHelper::SaveStringToFile(Text, *TextFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		LastVoiceError = FString::Printf(TEXT("Failed to save local TTS text: %s"), *TextFilePath);
		UE_LOG(LogTemp, Warning, TEXT("%s"), *LastVoiceError);
		RequestPendingSpeech();
		return;
	}

	bSpeechInFlight = true;
	BroadcastVoiceStatus(true, TEXT("로컬 음성 합성 중..."));

	const float SafePlaybackPitch = FMath::Clamp(TTSPlaybackPitch, 0.5f, 2.0f);
	const float LocalSpeechSpeed = FMath::Clamp(TTSPlaybackSpeed / SafePlaybackPitch, 0.5f, 2.0f);
	const FString Language = LocalTTSLanguage.TrimStartAndEnd().IsEmpty() ? TEXT("kr") : LocalTTSLanguage.TrimStartAndEnd();
	FString Args = FString::Printf(
		TEXT("%s --text-file %s --output %s --language %s --speed %.2f"),
		*QuoteCommandLineArg(ResolvedScriptPath),
		*QuoteCommandLineArg(TextFilePath),
		*QuoteCommandLineArg(WavFilePath),
		*QuoteCommandLineArg(Language),
		LocalSpeechSpeed);
	if (!ResolvedCachePath.IsEmpty())
	{
		Args += FString::Printf(TEXT(" --cache-dir %s"), *QuoteCommandLineArg(ResolvedCachePath));
	}

	TWeakObjectPtr<UShowDownVoiceSubsystem> WeakThis(this);
	AsyncTask(
		ENamedThreads::AnyBackgroundThreadNormalTask,
		[WeakThis, ResolvedExecutablePath, Args, WavFilePath]()
		{
			int32 ReturnCode = -1;
			FString StdOut;
			FString StdErr;
			const bool bExecuted = FPlatformProcess::ExecProcess(*ResolvedExecutablePath, *Args, &ReturnCode, &StdOut, &StdErr);

			TArray<uint8> WavData;
			const bool bLoadedWav = bExecuted
				&& ReturnCode == 0
				&& FFileHelper::LoadFileToArray(WavData, *WavFilePath)
				&& WavData.Num() > 44;
			const FString CombinedOutput = StdOut + TEXT("\n") + StdErr;

			AsyncTask(
				ENamedThreads::GameThread,
				[WeakThis, bExecuted, ReturnCode, bLoadedWav, CombinedOutput, WavData = MoveTemp(WavData)]() mutable
				{
					if (!WeakThis.IsValid())
					{
						return;
					}

					UShowDownVoiceSubsystem* VoiceSubsystem = WeakThis.Get();
					VoiceSubsystem->bSpeechInFlight = false;
					if (!bLoadedWav)
					{
						VoiceSubsystem->LastVoiceError = FString::Printf(
							TEXT("Local TTS failed. executed=%s code=%d output=%s"),
							bExecuted ? TEXT("true") : TEXT("false"),
							ReturnCode,
							*CombinedOutput.Left(256));
						UE_LOG(LogTemp, Warning, TEXT("%s"), *VoiceSubsystem->LastVoiceError);
						VoiceSubsystem->BroadcastVoiceStatus(false, TEXT("로컬 TTS 실패."));
						VoiceSubsystem->RequestPendingSpeech();
						return;
					}

					VoiceSubsystem->LastSpeechByteCount = WavData.Num();
					VoiceSubsystem->LastVoiceError.Empty();
					UE_LOG(LogTemp, Log, TEXT("Local voice speech received %d bytes."), VoiceSubsystem->LastSpeechByteCount);
					VoiceSubsystem->PlaySpeechWav(WavData);
					VoiceSubsystem->RequestPendingSpeech();
				});
		});
}

void UShowDownVoiceSubsystem::RequestPendingSpeech()
{
	if (!bHasPendingSpeech)
	{
		return;
	}

	const FString SpeechText = PendingSpeechText;
	PendingSpeechText.Empty();
	bHasPendingSpeech = false;
	RequestSpeech(SpeechText);
}

bool UShowDownVoiceSubsystem::ParseTranscriptionResponse(const FString& ResponseBody, FString& OutText) const
{
	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		return false;
	}

	if (!RootObject->TryGetStringField(TEXT("text"), OutText))
	{
		return false;
	}

	OutText = OutText.TrimStartAndEnd();
	return !OutText.IsEmpty();
}

bool UShowDownVoiceSubsystem::BuildRecordedWav(TArray<uint8>& OutWavData, float& OutDurationSeconds) const
{
	TArray<float> Samples;
	int32 SampleRate = 48000;
	int32 NumChannels = 1;
	{
		FScopeLock Lock(&CaptureCriticalSection);
		Samples = CapturedSamples;
		SampleRate = CaptureSampleRate;
		NumChannels = FMath::Max(1, CaptureNumChannels);
	}

	const int32 NumFrames = Samples.Num() / NumChannels;
	if (NumFrames <= 0 || SampleRate <= 0)
	{
		return false;
	}

	OutDurationSeconds = static_cast<float>(NumFrames) / static_cast<float>(SampleRate);

	TArray<int16> MonoPcm;
	MonoPcm.Reserve(NumFrames);
	for (int32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex)
	{
		float MixedSample = 0.0f;
		for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
		{
			MixedSample += Samples[FrameIndex * NumChannels + ChannelIndex];
		}
		MixedSample = FMath::Clamp(MixedSample / static_cast<float>(NumChannels), -1.0f, 1.0f);
		MonoPcm.Add(static_cast<int16>(FMath::RoundToInt(MixedSample * 32767.0f)));
	}

	const uint16 OutputChannels = 1;
	const uint16 BitsPerSample = 16;
	const uint32 DataSize = MonoPcm.Num() * sizeof(int16);
	const uint32 ByteRate = SampleRate * OutputChannels * BitsPerSample / 8;
	const uint16 BlockAlign = OutputChannels * BitsPerSample / 8;

	OutWavData.Reset();
	AppendUtf8(OutWavData, TEXT("RIFF"));
	AppendUInt32LE(OutWavData, 36 + DataSize);
	AppendUtf8(OutWavData, TEXT("WAVE"));
	AppendUtf8(OutWavData, TEXT("fmt "));
	AppendUInt32LE(OutWavData, 16);
	AppendUInt16LE(OutWavData, 1);
	AppendUInt16LE(OutWavData, OutputChannels);
	AppendUInt32LE(OutWavData, SampleRate);
	AppendUInt32LE(OutWavData, ByteRate);
	AppendUInt16LE(OutWavData, BlockAlign);
	AppendUInt16LE(OutWavData, BitsPerSample);
	AppendUtf8(OutWavData, TEXT("data"));
	AppendUInt32LE(OutWavData, DataSize);
	OutWavData.Append(reinterpret_cast<const uint8*>(MonoPcm.GetData()), DataSize);
	return true;
}

void UShowDownVoiceSubsystem::PlaySpeechWav(const TArray<uint8>& WavData)
{
	if (WavData.Num() < 44)
	{
		LastVoiceError = FString::Printf(TEXT("TTS WAV is too small. bytes=%d"), WavData.Num());
		UE_LOG(LogTemp, Warning, TEXT("%s"), *LastVoiceError);
		return;
	}

	int32 Offset = 12;
	uint16 AudioFormat = 0;
	uint16 NumChannels = 0;
	uint32 SampleRate = 0;
	uint16 BitsPerSample = 0;
	int32 DataOffset = INDEX_NONE;
	uint32 DataSize = 0;

	while (Offset + 8 <= WavData.Num())
	{
		const FString ChunkId = FString::Printf(TEXT("%c%c%c%c"), WavData[Offset], WavData[Offset + 1], WavData[Offset + 2], WavData[Offset + 3]);
		const uint32 DeclaredChunkSize = ReadUInt32LE(WavData, Offset + 4);
		const int32 ChunkDataOffset = Offset + 8;
		const uint64 AvailableChunkSize = static_cast<uint64>(WavData.Num() - ChunkDataOffset);
		const uint64 ResolvedChunkSize = DeclaredChunkSize == MAX_uint32
			? AvailableChunkSize
			: static_cast<uint64>(DeclaredChunkSize);
		if (ResolvedChunkSize > AvailableChunkSize || ResolvedChunkSize > static_cast<uint64>(MAX_int32))
		{
			break;
		}
		const int32 ChunkSize = static_cast<int32>(ResolvedChunkSize);

		if (ChunkId == TEXT("fmt ") && ChunkSize >= 16)
		{
			AudioFormat = ReadUInt16LE(WavData, ChunkDataOffset);
			NumChannels = ReadUInt16LE(WavData, ChunkDataOffset + 2);
			SampleRate = ReadUInt32LE(WavData, ChunkDataOffset + 4);
			BitsPerSample = ReadUInt16LE(WavData, ChunkDataOffset + 14);
		}
		else if (ChunkId == TEXT("data"))
		{
			DataOffset = ChunkDataOffset;
			DataSize = static_cast<uint32>(ChunkSize);
			break;
		}

		Offset = ChunkDataOffset + static_cast<int32>(ChunkSize);
		if ((Offset % 2) != 0)
		{
			++Offset;
		}
	}

	TArray<uint8> PlaybackPcm16;
	const bool bCanPlayWav = DataOffset != INDEX_NONE
		&& DataSize > 0
		&& NumChannels > 0
		&& SampleRate > 0
		&& ConvertWavSamplesToInt16(WavData, DataOffset, DataSize, AudioFormat, BitsPerSample, PlaybackPcm16)
		&& PlaybackPcm16.Num() > 0;

	if (!bCanPlayWav)
	{
		LastVoiceError = FString::Printf(
			TEXT("Unsupported TTS WAV. format=%d channels=%d rate=%d bits=%d data=%d"),
			AudioFormat,
			NumChannels,
			SampleRate,
			BitsPerSample,
			DataSize);
		UE_LOG(LogTemp, Warning, TEXT("%s"), *LastVoiceError);
		return;
	}

	ActiveSpeechWave = NewObject<USoundWaveProcedural>(this);
	if (!ActiveSpeechWave)
	{
		return;
	}

	if (ActiveSpeechComponent)
	{
		ActiveSpeechComponent->OnAudioFinished.RemoveDynamic(this, &UShowDownVoiceSubsystem::HandleSpeechAudioFinished);
		ActiveSpeechComponent->Stop();
		ActiveSpeechComponent->DestroyComponent();
		ActiveSpeechComponent = nullptr;
		BroadcastSpeechPlaybackState(false);
	}

	ActiveSpeechWave->NumChannels = NumChannels;
	ActiveSpeechWave->SetSampleRate(SampleRate);
	ActiveSpeechWave->SampleByteSize = sizeof(int16);
	ActiveSpeechWave->Duration = static_cast<float>(PlaybackPcm16.Num()) / static_cast<float>(SampleRate * NumChannels * sizeof(int16));
	ActiveSpeechWave->SoundGroup = SOUNDGROUP_Voice;
	ActiveSpeechWave->bLooping = false;
	ActiveSpeechWave->QueueAudio(PlaybackPcm16.GetData(), PlaybackPcm16.Num());
	LastVoiceError.Empty();
	UE_LOG(
		LogTemp,
		Log,
		TEXT("Playing TTS WAV: %.2fs, %d Hz, %d channel(s), source format=%d bits=%d."),
		ActiveSpeechWave->Duration,
		SampleRate,
		NumChannels,
		AudioFormat,
		BitsPerSample);

	if (UWorld* World = GetWorld())
	{
		const float SafePlaybackPitch = FMath::Clamp(TTSPlaybackPitch, 0.5f, 2.0f);
		ActiveSpeechComponent = UGameplayStatics::SpawnSound2D(
			World,
			ActiveSpeechWave,
			FMath::Clamp(TTSPlaybackVolume, 0.1f, 3.0f),
			SafePlaybackPitch,
			0.0f,
			nullptr,
			false,
			false);
		if (ActiveSpeechComponent)
		{
			ActiveSpeechComponent->OnAudioFinished.AddDynamic(this, &UShowDownVoiceSubsystem::HandleSpeechAudioFinished);
			BroadcastSpeechPlaybackState(true);
			const float ExpectedPlaybackSeconds = FMath::Max(0.05f, ActiveSpeechWave->Duration / SafePlaybackPitch);
			World->GetTimerManager().SetTimer(
				SpeechPlaybackFallbackTimerHandle,
				this,
				&UShowDownVoiceSubsystem::HandleSpeechAudioFinished,
				ExpectedPlaybackSeconds + 0.25f,
				false);
		}
	}
}

void UShowDownVoiceSubsystem::HandleSpeechAudioFinished()
{
	if (ActiveSpeechComponent)
	{
		ActiveSpeechComponent->OnAudioFinished.RemoveDynamic(this, &UShowDownVoiceSubsystem::HandleSpeechAudioFinished);
		ActiveSpeechComponent->DestroyComponent();
		ActiveSpeechComponent = nullptr;
	}

	BroadcastSpeechPlaybackState(false);
}
