# Im Hyunjin Art Tone Settings Log

이 문서는 임현진 담당 작업 중 에디터에서 설정한 화면 분위기, 포스트 프로세스, 필터, 에셋, 테스트맵 변경 사항을 계속 누적 기록하는 작업 로그입니다.

## 기록 원칙

- 언리얼 에디터에서 만진 설정은 가능한 한 하나씩 기록한다.
- 맵 이름, 액터 이름, 설정 위치, 값, 의도를 같이 적는다.
- 분위기 작업은 먼저 테스트맵에서 확인하고, 바로 메인 레벨에 넣지 않는다.
- 어둡고 거친 분위기를 만들더라도 카드, 총알, 버튼, 생명 카드 같은 플레이 정보는 잘 보여야 한다.
- 픽셀화, CRT, VHS, 글리치 효과는 기본 색감과 조명 톤을 잡은 뒤 추가한다.

## 현재 프로젝트 연결 상태

- 실제 프로젝트 위치: `C:\Users\user\Documents\Unreal Projects\showdown`
- 예전 Rider 경로: `C:\Users\user\RiderProjects\showdown`
- 예전 Rider 경로는 새 프로젝트 위치를 가리키는 Junction으로 연결되어 있다.
- Epic Launcher 프로젝트 검색 경로:
  - `C:/Users/user/Documents/Unreal Projects`
- 언리얼 에디터 언어: 한글

## 담당 역할 기준

- 담당자: 임현진
- 주요 역할:
  - 연출 총괄
  - 사물형 UI 공동 작업
  - 화면 분위기 방향 설정
  - 레벨/맵 통합 관리
  - 카드 공개, 룰렛, 생명 카드, 베팅 연출의 시각적 방향 잡기

## 첫 작업 목표

### 목표

`ArtToneTest` 테스트맵을 만들고, 포스트 프로세스 볼륨으로 기본 화면 톤을 잡는다.

### 작업 순서

1. 현재 레벨을 테스트맵으로 저장한다.
   - 메뉴: `파일` -> `현재 레벨을 다른 이름으로 저장`
   - 저장 위치: `콘텐츠/Maps`
   - 맵 이름: `ArtToneTest`

2. 포스트 프로세스 볼륨을 추가한다.
   - 액터 검색어: `포스트 프로세스 볼륨`
   - 영어로만 검색될 경우: `Post Process Volume`

3. 화면 전체에 적용한다.
   - 월드 아웃라이너에서 `PostProcessVolume` 선택
   - 디테일 패널에서 `무한` 또는 `Infinite` 검색
   - `무한 범위 (언바운드)` / `Infinite Extent (Unbound)` 체크

4. 기본 분위기 값을 조절한다.

## 추천 초기 포스트 프로세스 값

아직 실제 적용 전 추천값입니다. 적용 후 실제 값으로 갱신합니다.

| 항목 | 한글 검색어 | 추천값 | 의도 |
| --- | --- | --- | --- |
| Exposure Compensation | 노출 | `-0.5` | 화면을 살짝 어둡게 만든다. |
| Saturation | 채도 | `0.7` | 색을 빼서 낡고 건조한 느낌을 만든다. |
| Contrast | 대비 | `1.15` | 어둠과 밝음의 차이를 조금 강하게 만든다. |
| Vignette Intensity | 비네트 | `0.4` | 화면 가장자리를 어둡게 해 폐쇄감을 만든다. |
| Film Grain Intensity | 그레인 | `0.25` | 화면에 거친 질감을 더한다. |

## 검증 메모

### Exposure / EV100

- `Exposure Compensation`은 값이 낮아질수록 어두워지고, 높아질수록 밝아진다.
  - 예: `-1`은 더 어둡게, `+1`은 더 밝게.
- `Min EV100` / `Max EV100`은 값이 올라갈수록 화면이 어두워지는 방향이다.
- 자동 노출을 고정하려면 `Min EV100`과 `Max EV100`을 같은 값으로 맞춘다.
- 고정값을 바꿀 때의 감각:
  - `Min EV100 = Max EV100 = 0`보다
  - `Min EV100 = Max EV100 = 1`이 더 어둡다.
- 따라서 호러 톤을 잡을 때는 값을 올리면 더 어두워지고, 값을 내리면 더 밝아지는 것으로 판단한다.

## 실제 적용 로그

### 기본 포스트 프로세스 1차

아래 값은 현진님이 `ArtToneTest`에서 실제로 잡은 초반 분위기 테스트 기준값입니다.

| 맵 | 액터 | 설정 | 값 | 의도 |
| --- | --- | --- | --- | --- |
| `ArtToneTest` | `PostProcessVolume` | `Infinite Extent (Unbound)` | On | 볼륨 위치와 상관없이 화면 전체에 적용 |
| `ArtToneTest` | `PostProcessVolume` | `Exposure Compensation` | `1.0` | 전체 밝기 기준 조절 |
| `ArtToneTest` | `PostProcessVolume` | `Saturation` | `0.7` | 채도를 낮춰 낡고 건조한 톤 형성 |
| `ArtToneTest` | `PostProcessVolume` | `Contrast` | `0.8` | 대비를 낮춰 색이 더 눌린 느낌 형성 |
| `ArtToneTest` | `PostProcessVolume` | `Temperature` | 기본값 | 색온도는 아직 적극적으로 조절하지 않음 |
| `ArtToneTest` | `PostProcessVolume` | `Tint` | `0.2` | 화면 색조에 약한 편향 추가 |
| `ArtToneTest` | `PostProcessVolume` | `Chromatic Aberration Intensity` | `0.2` | 색수차/RGB 번짐을 아주 약하게 추가 |
| `ArtToneTest` | `PostProcessVolume` | `Vignette Intensity` | `0.55` | 화면 가장자리 어둡게 처리, 폐쇄감 강화 |
| `ArtToneTest` | `PostProcessVolume` | `Film Grain Intensity` | `0.2` | 거친 화면 질감 추가 |
| `ArtToneTest` | `PostProcessVolume` | `Film Grain Texel Size` | `1.0` | 그레인 입자 크기 기준 |
| `ArtToneTest` | `PostProcessVolume` | `Bloom Intensity` | `0.3` | 밝은 부분의 빛 번짐 후보값 |
| `ArtToneTest` | `PostProcessVolume` | `Bloom Threshold` | `0.5` | 블룸이 적용되기 시작하는 밝기 기준 |
| `ArtToneTest` | `PostProcessVolume` | `Min EV100` | `0` | 자동 노출 고정 |
| `ArtToneTest` | `PostProcessVolume` | `Max EV100` | `0` | 자동 노출 고정 |

### 현재 포스트 프로세스 머티리얼 값

| 머티리얼 | 파라미터 | 값 | 의도 |
| --- | --- | --- | --- |
| `M_PP_Pixelate` | `PixelCount` | `320` | 화면 전체를 저해상도/Lo-fi 느낌으로 처리 |
| `M_PP_Pixelate` | `ScanlineCount` | `480` | CRT/VHS 가로줄 간격 기준 |
| `M_PP_Pixelate` | `ScanlineStrength` | `0.02` | 가로줄 강도를 약하게 유지 |
| `M_PP_Pixelate` | `ColorSteps` | `6` | 색 단계 수를 크게 줄여 강한 포스터라이즈/저비트 느낌 형성 |

### 현재 화면 느낌 메모

- 전체적으로 강한 포스터라이즈와 스캔라인이 보여서 저비트 VHS/Lo-fi 분위기가 확실하다.
- 어두운 영역이 크게 눌려 있고, 밝은 테이블 영역은 색 단계가 뚝뚝 끊겨 보인다.
- `ColorSteps = 6`은 효과가 강한 편이라, 나중에 카드 숫자나 버튼 가독성 테스트가 필요하다.
- 현재 단계에서는 게임플레이 오브젝트가 없으므로 분위기 방향 테스트값으로 유지한다.

앞으로 현진님이 에디터에서 값을 바꾸거나 에셋을 추가하면 이 아래에 날짜별로 누적 기록한다.

### 2026-06-12

- 프로젝트 폴더를 `C:\Users\user\Documents\Unreal Projects\showdown` 위치로 이동했다.
- `C:\Users\user\RiderProjects\showdown` 경로는 새 위치로 연결되는 Junction으로 설정했다.
- Epic Launcher가 `C:/Users/user/Documents/Unreal Projects` 경로를 프로젝트 검색 위치로 사용하고 있음을 확인했다.
- 현진님 작업 방향을 화면 필터, 포스트 프로세스, 픽셀화, VHS/CRT, 노이즈 등 기본 분위기 틀 잡기로 정리했다.
- 첫 에디터 작업은 `ArtToneTest` 테스트맵 생성 후 `Post Process Volume`을 추가하는 것으로 정했다.
- `ArtToneTest`에서 기본 포스트 프로세스 1차 값 적용을 진행한 것으로 기록했다.
- `콘텐츠/ArtTone` 폴더를 만들었다.
- `M_PP_Pixelate` 머티리얼을 만들었다.
- `M_PP_Pixelate`의 `Material Domain`을 `Post Process`로 설정했다.
- `M_PP_Pixelate` 머티리얼 그래프에 픽셀화 노드를 구성했다.
  - `ScreenPosition(ViewportUV)` -> `Mask(R,G)` -> `Multiply` -> `Floor` -> `Divide` -> `SceneTexture(PostProcessInput0).UVs`
  - `SceneTexture(PostProcessInput0).Color` -> `Emissive Color`
  - `Scalar Parameter`: `PixelCount`
- `M_PP_Pixelate`를 `PostProcessVolume`의 `Post Process Materials` / `Blendables`에 추가해 화면에 적용했다.
- `PostProcessVolume`에서 색수차 / Chromatic Aberration 계열 값을 테스트했다.
  - 기본 후보값: `0.2`
  - `2.0`은 효과가 강하게 보이지만 기본 화면용으로는 과해 보여 연출 순간용 후보로 보류했다.

### 2026-06-15

- 카메라 전환 테스트용 C++ 액터 `ASDCameraBlendTester`를 추가했다.
  - Header: `Source/ShowDown/Public/Presentation/SDCameraBlendTester.h`
  - Source: `Source/ShowDown/Private/Presentation/SDCameraBlendTester.cpp`
- 목적:
  - 테스트 레벨에 액터를 배치하고 카메라 2개를 지정한 뒤, 키보드 `1` / `2`로 `SetViewTargetWithBlend` 카메라 전환을 공부한다.
- 사용 방식:
  - 레벨에 `CameraActor` 2개 배치
  - `ASDCameraBlendTester` 액터 배치
  - 디테일 패널에서 `DefaultCamera`, `CloseCamera`에 카메라 액터 지정
  - `BlendTime`, `BlendFunction`, `BlendExponent`로 전환 느낌 조절
  - 플레이 중 `1`: 기본 카메라, `2`: 가까운 카메라, `3`: 플레이어 자유시점으로 전환
- 추가 설정:
  - `FreeViewBlendTime`: 자유시점으로 돌아갈 때의 전환 시간
  - `bRestoreGameInputOnFreeView`: 자유시점 복귀 시 이동/시야 입력을 다시 활성화할지 여부
- `ASDCameraBlendTester`에 Default 카메라 자유 회전과 중앙 상호작용 테스트 기능을 추가했다.
  - `1`번으로 `DefaultCamera`에 들어가면 마우스 이동으로 카메라 액터를 직접 회전한다.
  - 좌클릭하면 `DefaultCamera` 정면으로 `Visibility` 라인트레이스를 쏜다.
  - 맞은 액터가 `ASDPressableButtonActor`이면 `Press()`를 호출한다.
  - `2`번 `CloseCamera`에서는 고정 카메라처럼 유지한다.
  - `3`번은 플레이어 자유시점으로 돌아간다.
  - 현재는 화면 중앙 기준 판정만 구현했고, 보이는 조준점 UI는 아직 별도 작업으로 남겨둔다.
- Default 카메라 상호작용 주요 조절값:
  - `bEnableDefaultCameraMouseLook`: Default 카메라 마우스 회전 사용 여부
  - `MouseLookSensitivity`: 마우스 회전 감도
  - `MinPitch`, `MaxPitch`: 위아래 회전 제한
  - `InteractTraceDistance`: 중앙 클릭 판정 거리
  - `bDrawDebugInteractTrace`: 클릭 판정 라인 디버그 표시 여부
- Default 카메라 마우스 조작 기본값을 조정했다.
  - `MouseLookSensitivity`: `0.08` -> `0.25`
  - `bInvertMouseY`: `false` -> `true`
  - 이유: 기존 감도가 너무 느리고, 위아래 조작이 기대 방향과 반대로 느껴졌기 때문.
- `ASDCameraBlendTester`에 C++ Slate 기반 중앙 조준점을 추가했다.
  - 블루프린트 위젯 없이 코드에서 화면 중앙 `+` 모양을 그린다.
  - `DefaultCamera` 상태에서만 표시하고, `CloseCamera` / 자유시점에서는 숨긴다.
  - 조준점은 `HitTestInvisible`이라 마우스 클릭 판정을 막지 않는다.
  - 주요 조절값: `bShowCrosshair`, `CrosshairSize`, `CrosshairThickness`, `CrosshairColor`.
- 빌드 메모:
  - UnrealHeaderTool 코드 생성은 통과했다.
  - 정식 빌드는 에디터 Live Coding 활성화 상태 때문에 중단되었다.
  - 에디터를 닫고 다시 빌드하거나, 에디터에서 `Ctrl + Alt + F11`로 Live Coding 컴파일을 사용해 확인한다.
- 포스트 프로세스 현재값을 최신 기준으로 갱신했다.
  - `Exposure Compensation`: `0.5` -> `1.0`
  - `PixelCount`: `480` -> `320`
  - 현재값 표에 `Chromatic Aberration Intensity = 0.2`를 추가했다.

### 2026-06-16

- BGM 방향을 공포 앰비언스보다 긴박하고 신나는 전자음악 계열로 잡는 방향을 검토했다.
  - 레퍼런스 감각: `Buckshot Roulette`처럼 어둡지만 리듬이 살아 있는 도박판/클럽/지하실 느낌.
  - 키워드: EDM, dark club, industrial beat, dirty synth, tense groove.
  - 목표: 무섭게 가라앉히기보다 베팅과 룰렛의 속도를 밀어주는 음악.
  - 주의: 레퍼런스 곡을 그대로 따라 하기보다, 리듬감과 에너지 구조만 참고한다.
- 사물형 UI 테스트용 C++ 액터 `ASDPressableButtonActor`를 추가했다.
  - Header: `Source/ShowDown/Public/Presentation/SDPressableButtonActor.h`
  - Source: `Source/ShowDown/Private/Presentation/SDPressableButtonActor.cpp`
- 목적:
  - 블루프린트 그래프를 복잡하게 쓰지 않고, C++ 로직으로 버튼 눌림 연출을 테스트한다.
  - 블루프린트는 메시, 위치, 눌림 거리, 속도 같은 값 조절용으로만 사용한다.
- 위치/움직임 기준:
  - 버튼의 월드 목표 위치를 직접 찍지 않는다.
  - 레벨에서는 버튼 액터를 원하는 위치에 배치한다.
  - 눌림 방향은 `LocalPressOffset`으로 지정한다.
  - 기본값 `LocalPressOffset = (0, 0, -8)`은 버튼 메시가 자기 로컬 기준 아래로 8cm 눌리는 의미다.
- 기본 사용:
  - `ASDPressableButtonActor` 기반 블루프린트 생성
  - `ButtonMesh`에 버튼용 Static Mesh 지정
  - 테스트 레벨에 배치
  - 클릭하거나 `E` / `Space`를 누르면 버튼이 눌렸다가 튕기며 돌아온다.
- 주요 조절값:
  - `LocalPressOffset`: 눌림 방향과 거리
  - `PressDownTime`: 내려가는 시간
  - `HoldTime`: 눌린 상태로 머무는 시간
  - `ReturnTime`: 돌아오는 시간
  - `ReturnBounceDistance`: 돌아올 때 살짝 튕기는 거리
  - `bEnableKeyboardTestInput`: 테스트 키 입력 사용 여부
- 공통 상호작용 인터페이스 `ISDInteractable`을 추가했다.
  - Header: `Source/ShowDown/Public/Interaction/SDInteractable.h`
  - 목적: 버튼, 담배, 맥주처럼 종류가 다른 사물도 중앙 조준점 클릭에서 같은 방식으로 `Interact()`를 호출하게 한다.
- `ASDCameraBlendTester`의 중앙 클릭 판정을 버튼 전용에서 공통 인터페이스 방식으로 변경했다.
  - 기존: `ASDPressableButtonActor`만 찾아서 `Press()` 호출
  - 변경: `ISDInteractable` 구현 액터를 찾고 `CanInteract()` 확인 후 `Interact()` 호출
- `ASDPressableButtonActor`가 `ISDInteractable`을 구현하도록 변경했다.
- 담배 상호작용 테스트용 C++ 액터 `ASDSmokableCigaretteActor`를 추가했다.
  - Header: `Source/ShowDown/Public/Presentation/SDSmokableCigaretteActor.h`
  - Source: `Source/ShowDown/Private/Presentation/SDSmokableCigaretteActor.cpp`
  - 클릭 또는 중앙 조준점 상호작용 시 `Smoke()` 실행
  - 담배 메시가 로컬 오프셋/회전값만큼 들렸다가 돌아온다.
  - `EmberLight`로 담배 끝 불빛을 켜고 흔들림을 준다.
  - `SmokeParticle`에 연기 파티클을 지정하면 흡연 중 활성화된다.
  - `SmokeSound`를 지정하면 상호작용 시 소리를 재생한다.
- 담배 주요 조절값:
  - `LocalSmokeOffset`: 담배가 들어올려지는 로컬 위치
  - `LocalSmokeRotationOffset`: 담배가 들어올려질 때의 로컬 회전
  - `LiftTime`, `SmokeHoldTime`, `ReturnTime`: 들어올림/유지/복귀 시간
  - `EmberSmokeIntensity`, `EmberFlickerAmount`, `EmberFlickerSpeed`: 담배 끝 불빛 느낌
- 담배 연출을 1인칭 카메라 앞 연출로 확장했다.
  - `bUseFirstPersonSmokePose`가 켜져 있으면 담배가 클릭된 자리에서 현재 카메라 앞으로 이동한다.
  - 피는 동안 `PlayerCameraManager`의 현재 카메라 위치/회전을 따라다닌다.
  - 연출이 끝나면 원래 레벨 배치 위치로 돌아간다.
  - `FirstPersonCameraOffset`: 카메라 기준 위치. X=앞, Y=오른쪽, Z=위/아래.
  - `FirstPersonRotationOffset`: 카메라 기준 담배 회전 보정.
  - `bDisableCollisionWhileSmoking`: 연출 중 담배 충돌을 잠시 꺼서 카메라 앞에서 방해되지 않게 한다.
