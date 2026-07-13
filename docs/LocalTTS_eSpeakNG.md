# Local TTS: eSpeak NG

ShowDown can use eSpeak NG for free local TTS without calling the OpenAI speech API.

## Required Files

Put the Windows eSpeak NG runtime here:

```text
Binaries/ThirdParty/eSpeakNG/espeak-ng.exe
Binaries/ThirdParty/eSpeakNG/espeak-ng-data/
```

The game config already points to these paths:

```ini
SpeechBackend=LocalESpeakNG
LocalTTSExecutablePath=Binaries/ThirdParty/eSpeakNG/espeak-ng.exe
LocalTTSDataPath=Binaries/ThirdParty/eSpeakNG/espeak-ng-data
LocalTTSVoice=ko
```

## Notes

- eSpeak NG is small and easy to commit compared with Python-based TTS stacks.
- Korean is supported, but the voice sounds mechanical.
- Keep the whole `espeak-ng-data` folder next to `espeak-ng.exe`.
- Files under `Binaries/ThirdParty/eSpeakNG/` are allowed by `.gitignore` and tracked by Git LFS.
- License files copied from the eSpeak NG 1.52.0 repository are in `docs/third_party/eSpeakNG/`.
