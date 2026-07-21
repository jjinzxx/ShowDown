# 트러블 슈팅

[← README로 돌아가기](../README.md)

## Git LFS 파일이 포인터로 남아 패키징이 실패하는 문제

### 증상

Whisper 실행 파일이나 모델이 실제 바이너리가 아니라 `version https://git-lfs.github.com/spec/v1`로 시작하는 작은 텍스트 파일로 남습니다. 빌드 과정에서는 필수 음성 런타임이 없다는 오류가 발생합니다.

### 원인

대용량 에셋과 음성 런타임이 Git LFS로 관리되지만 clone 또는 checkout 후 LFS 객체가 내려받아지지 않았습니다.

### 해결

```bash
git lfs install
git lfs pull
```

`ShowDown.Build.cs`에서도 패키징 전에 필수 런타임의 존재 여부와 LFS 포인터 여부를 검사하도록 방어 로직을 추가했습니다.

## 멀티플레이에서 카드·연출 상태가 다르게 보이는 문제

### 증상

서버에서는 카드가 공개됐지만 클라이언트에는 뒷면이 남거나, 사격·피격 카메라가 한쪽에서만 재생됩니다.

### 원인

게임 판정, 복제 상태, 로컬 카메라 연출의 책임이 섞이면 RPC 도착 순서와 위젯 생성 시점에 따라 결과가 달라집니다.

### 해결 방향

- 승패와 룰렛 결과는 서버 권한으로 한 번만 계산합니다.
- 카드·베팅·생명 같은 지속 상태는 복제하고 `OnRep`에서 화면을 갱신합니다.
- 카메라·사운드처럼 각 화면에서 재생해야 하는 표현은 결과를 받은 뒤 로컬에서 실행합니다.
- 위젯이 늦게 생성되는 경우 현재 상태를 다시 읽어 초기 화면을 구성합니다.

## `.umap` 충돌로 레벨 변경을 합칠 수 없는 문제

### 증상

두 브랜치가 같은 맵을 수정한 뒤 Git에서 바이너리 충돌이 발생합니다.

### 원인

Unreal 레벨 파일은 일반 텍스트처럼 줄 단위 병합할 수 없습니다.

### 해결 방향

- 메인 레벨 담당자와 수정 시간을 사전에 공유합니다.
- 기능별 테스트맵에서 작업한 뒤 액터 또는 서브레벨 단위로 통합합니다.
- 에셋 이동 후 Redirector를 정리하고 참조가 유지되는지 확인합니다.
- 충돌이 발생하면 두 버전을 억지로 병합하지 않고 한쪽을 기준으로 필요한 변경을 에디터에서 재적용합니다.

## 로컬 음성 인식·합성이 실행되지 않는 문제

### 확인할 경로

```text
Binaries/ThirdParty/Whisper/whisper-cli.exe
Content/LocalModels/Whisper/ggml-base.bin
Binaries/ThirdParty/eSpeakNG/espeak-ng.exe
Binaries/ThirdParty/eSpeakNG/espeak-ng-data/
```

### 점검 순서

1. Git LFS 파일이 실제로 내려받아졌는지 확인합니다.
2. `Config/DefaultGame.ini`의 실행 파일·모델 경로를 확인합니다.
3. 패키징 결과에 Non-UFS 런타임 파일이 포함됐는지 확인합니다.
4. 마이크 권한과 오디오 입력 장치를 확인합니다.

세부 설정은 [LocalTTS_eSpeakNG.md](LocalTTS_eSpeakNG.md)를 참고합니다.

## API 키가 저장소에 노출될 위험

### 대응

- 실제 키 파일 `SDLLMSecrets.h`는 `.gitignore`로 제외합니다.
- 저장소에는 `SDLLMSecrets.h.template`만 유지합니다.
- Supabase와 외부 API의 비밀값은 환경 변수 또는 배포 환경의 Secret으로 주입합니다.
- 로그와 커밋 이력에 키가 들어가지 않았는지 배포 전에 확인합니다.
