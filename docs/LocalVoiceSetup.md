# Local Voice Setup

이 문서는 OpenAI TTS 비용을 줄이기 위해 로컬 STT/TTS를 쓰는 팀원용 설정 방법입니다.

## 현재 구조

- STT: `whisper.cpp`
- TTS: `MeloTTS`
- STT/TTS 모델 파일: `Git LFS`
- 게임 설정 파일: `Config/DefaultGame.ini`
- MeloTTS 설치 스크립트: `tools/local_voice/setup_melotts.ps1`
- MeloTTS 실행 스크립트: `tools/local_voice/melotts_speak.py`

## 팀원이 처음 해야 할 일

먼저 Git LFS 파일까지 받습니다.

```powershell
git lfs pull
```

그 다음 PowerShell을 프로젝트 루트에서 열고 아래 명령어를 실행합니다.

```powershell
.\tools\local_voice\setup_melotts.ps1
```

처음 실행할 때는 Python과 MeloTTS 패키지를 받기 때문에 시간이 오래 걸릴 수 있습니다. 한국어 TTS 모델 캐시는 `ThirdParty/MeloTTS/hf-cache/`에 Git LFS로 포함됩니다.

빠르게 설치만 확인하고 모델 다운로드 테스트를 건너뛰려면 아래처럼 실행합니다.

```powershell
.\tools\local_voice\setup_melotts.ps1 -SkipWarmup
```

## 성공하면 생기는 로컬 폴더

아래 폴더들은 PC마다 만들어지는 로컬 실행 환경입니다.

- `ThirdParty/Python310/`
- `ThirdParty/Installers/`
- `ThirdParty/MeloTTS/.venv310/`

이 폴더들은 용량이 크고 PC마다 다시 만들 수 있으므로 Git에 커밋하지 않습니다.

아래 폴더는 TTS 모델 캐시라서 Git LFS로 커밋합니다.

- `ThirdParty/MeloTTS/hf-cache/`

## 게임 설정

기본 설정은 로컬 Whisper STT와 로컬 MeloTTS TTS를 사용합니다.

```ini
TranscriptionBackend=LocalWhisper
SpeechBackend=LocalMeloTTS
LocalTTSExecutablePath=ThirdParty/MeloTTS/.venv310/Scripts/python.exe
LocalTTSScriptPath=tools/local_voice/melotts_speak.py
LocalTTSCachePath=ThirdParty/MeloTTS/hf-cache
LocalTTSLanguage=kr
```

목소리 볼륨은 아래 값으로 조절합니다.

```ini
TTSPlaybackVolume=1.750000
```

소리가 작으면 `2.0` 근처로 올리고, 소리가 찢어지면 `1.3` 정도로 낮춥니다.

## 직접 테스트

설치 후 아래 명령어로 wav 생성만 따로 확인할 수 있습니다.

```powershell
.\ThirdParty\MeloTTS\.venv310\Scripts\python.exe .\tools\local_voice\melotts_speak.py --text "싸우자고? 좋아. 대신 후회하지 마." --output .\Saved\LocalVoice\TTS\melotts_manual_test.wav --language kr --cache-dir .\ThirdParty\MeloTTS\hf-cache
```

생성된 wav는 `Saved/LocalVoice/TTS/` 아래에 생깁니다.
