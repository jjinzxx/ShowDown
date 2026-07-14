# ShowDown 연출/사물 설정 위치 지도

이 문서는 "이 값 어디서 만지지?"를 빨리 찾기 위한 작업용 지도입니다. 리팩토링 문서가 아니라, 현재 프로젝트 기준으로 에디터에서 어떤 액터/블루프린트/클래스 값을 만져야 하는지 정리합니다.

## 빠른 찾기

| 하고 싶은 것 | 우선 확인할 곳 | 주로 만지는 값 |
| --- | --- | --- |
| 게임 전체 밝기, 채도, 대비, 픽셀/망점 느낌 조절 | `SDArtToneController` 배치 액터 또는 `ASDArtToneController` | `InitialSettings`, `ExposureCompensation`, `Saturation`, `Contrast`, `PixelCount`, `ColorSteps`, `Halftone...`, `Bloom...` |
| 테이블 주변만 보이고 바깥을 암흑 처리 | `TableVisionDirector` 배치 액터 | `CollapsedVision`, `RevealedVision`, `VisionRadius`, `VisionFeather`, `DarknessStrength`, `VisionCenterActor` |
| 암흑 시야가 커졌다가 줄어드는 연출 | `TableVisionDirector` | `RevealExpandTime`, `RevealHoldTime`, `RevealCollapseTime`, `RevealEaseExponent`, `bPlayRevealOnBeginPlay` |
| 기본 게임 카메라 마우스 감도/각도 제한 | `BP_ShowDownModeBase` 또는 `ShowDownGameModeBase` | `GameplayCameraLookSensitivity`, `GameplayCameraMin/MaxPitch`, `GameplayCameraMin/MaxYawOffset`, `bInvertGameplayCameraMouseY` |
| 게임 카메라 숨쉬기 흔들림 | `BP_ShowDownModeBase` 또는 `ShowDownGameModeBase` | `bEnableGameplayCameraBreathingSway`, `GameplayCameraBreathingSwaySpeed`, `RotationAmplitude`, `LocationAmplitude`, `BlendInTime` |
| 허브/메뉴/싱글 프리뷰 카메라 | `BP_ShowDownHubFlowManager` | `LoginCamera`, `MainMenuCamera`, `ShopCamera`, `RankingCamera`, `GameCamera`, `CameraBlendTime`, `Game Camera` 설정 |
| 특정 연출 카메라 컷 묶음 | `SDCameraDirector` 배치 액터 | `Shots` 배열의 `ShotId`, `Camera`, `BlendTime`, `FOV`, `Mouse Look`, `CameraShakeClass` |
| 총 위치/회전/쏘는 카메라/총구 방향 | `BP_SelfShotGun` 또는 `ASDSelfShotGunActor` | `First Person`, `Target Shot`, `Cinematic Camera`, `EnemyShotCinematicCamera`, `TargetShotRotationOffset` |
| 총 격발 타이밍/해머/방아쇠/실린더 움직임 | `BP_SelfShotGun` | `Timing`, `Mechanism`, `Mechanism|Timing`, `Mechanism|Rotation` |
| 총 소리, 섬광, 이명, 맞았을 때 화면 효과 | `BP_SelfShotGun` | `Effects`, `Muzzle Flash`, `Effects|Tinnitus`, `Hit Sequence` |
| 담배 드는 위치/연기/빨간 불빛/흔들림 | `MySDSmokableCigaretteActor` 또는 `ASDSmokableCigaretteActor` | `First Person Smoke`, `Timing`, `Ember`, `Motion`, `Screen Effect` |
| 버튼 눌리는 깊이/속도/튕김 | `MySDPressableButtonActor` 또는 `ASDPressableButtonActor` | `LocalPressOffset`, `PressDownTime`, `HoldTime`, `ReturnTime`, `bUseReturnBounce` |
| 카드가 손/이마로 날아가는 움직임 | `BP_Card` 또는 `ACard` | `Card|Slot Attach Motion`, `SelectedOffset`, `HoverOffset`, `MoveSpeed` |
| 손패 위치/간격/기울기 | 카드 앵커 BP 또는 `ASDCardPlacementAnchor` | `PlacementRole`, `CardSpacing`, `ForwardOffset`, `HeightOffset`, `LeanAngle`, `LayerStep` |
| 플레이어/상대 자리의 손패/이마 기준점 | `BP_PlayerHandAnchor`, `BP_PlayerForeheadAnchor`, `BP_OpponentHandAnchor`, `BP_OpponentForeheadAnchor` | 액터 위치/회전, `ShowDown|Placement`, `ShowDown|Hand Layout` |
| 스테이지별 목숨/최소 베팅/AI 성향 | `BP_ShowDownModeBase` 또는 `ShowDownGameModeBase` | `StageRules`, `StartingLives`, `MinimumBet`, `CollectorBluffRate`, `CollectorAggression`, `bSevenFoldLoadsSix` |
| 라운드 공개 후 자동 진행 여부 | `BP_ShowDownModeBase` | `bAutoAdvanceRevealWithoutPresentation`, `RevealAutoAdvanceSeconds` |
| 멀티 베팅/공개/총격 사이 템포 | `BP_ShowDownModeBase` | `MultiplayerBetActionIntervalSeconds`, `MultiplayerPreRevealDelaySeconds`, `MultiplayerPostRevealHoldSeconds`, `MultiplayerPostShotPauseSeconds` |
| 멀티 공개 카드 사이 간격 | `BP_ShowDownModeBase` | `MultiplayerCardRevealGap` |
| 중앙 십자선/상호작용 윤곽선 | `BP_ShowDownPlayerController` 또는 `ShowDownPlayerController` | `Input|Crosshair`, `Input|Interactable Outline` |
| 멀티 좌석 카메라 | 레벨의 `CameraActor` 태그 | `MP_SeatCamera_1`, `MP_SeatCamera_2` ... 태그, 없으면 코드가 fallback 카메라 생성 |

## 화면 톤과 밝기

### `SDArtToneController`

게임 화면 전체의 후처리 톤을 만지는 중심 액터입니다. 밝기나 채도 같은 "전체 화면 느낌"은 조명보다 여기를 먼저 봅니다.

위치:
- 코드: `Source/ShowDown/Public/Presentation/SDArtToneController.h`
- 기본 머티리얼: `/Game/ArtTone/M_PP_Pixelate`
- 에디터 액터: 레벨에 배치된 `SDArtToneController` 계열 액터

주요 값:
- `InitialSettings`: BeginPlay 때 적용할 기본 화면 톤
- `PixelCount`: 작을수록 픽셀화가 큼
- `ColorSteps`: 색 단계 수. 작을수록 포스터라이즈 느낌
- `HalftoneStrength`: 망점 효과 세기
- `HalftoneCount`, `HalftoneRadius`, `HalftoneSoftness`, `HalftoneShape`: 망점 크기/부드러움/모양
- `ExposureCompensation`: 화면 밝기
- `Saturation`: 채도
- `Contrast`: 대비
- `VignetteIntensity`: 화면 가장자리 어두움
- `FilmGrainIntensity`: 필름 그레인
- `ChromaticAberrationIntensity`: 색수차
- `BloomIntensity`, `BloomThreshold`: 번짐

자주 하는 조정:
- 화면이 너무 어둡다: `ExposureCompensation` 증가
- 색이 너무 죽었다: `Saturation` 증가
- 만화/포스터 느낌을 줄이고 싶다: `PixelCount` 증가, `ColorSteps` 증가, `HalftoneStrength` 감소
- 맞았을 때 화면 효과: 총의 `Hit Sequence`가 이 `FSDArtToneSettings`를 사용합니다.

## 테이블 암흑 시야

### `TableVisionDirector`

테이블 중심으로 일정 반경만 보이고 바깥을 암흑 처리하는 액터입니다. 조명이 아니라 포스트프로세스 머티리얼 기반입니다.

위치:
- 코드: `Source/ShowDown/Public/Presentation/SDTableVisionDirector.h`
- 머티리얼: `/Game/ArtTone/M_PP_TableVisionWorldRange`
- 에디터 액터: `TableVisionDirector`

중심 위치:
- `VisionCenterActor`가 있으면 그 액터 위치를 중심으로 사용
- 없으면 `TableVisionDirector` 자기 위치를 중심으로 사용
- 머티리얼의 `VisionCenter` 기본값은 미리보기용이고, 인게임에서는 Director가 매번 덮어씁니다.

주요 값:
- `CollapsedVision`: 평소 좁은 시야 상태
- `RevealedVision`: 주변이 보이는 넓은 시야 상태
- `VisionRadius`: 밝게 보이는 반경
- `VisionFeather`: 경계 부드러움
- `DarknessStrength`: 암흑 강도
- `SpotLightIntensity`, `SpotLightAttenuationRadius`: 테이블 위 스포트라이트
- `PointLightIntensity`, `PointLightAttenuationRadius`: 보조 포인트 라이트
- `ExposureCompensation`, `VignetteIntensity`, `BloomIntensity`: 이 연출 중 후처리 톤
- `RevealExpandTime`, `RevealHoldTime`, `RevealCollapseTime`: 시야 확장/유지/복귀 시간
- `EditorPreviewRevealAlpha`: 에디터에서 0~1로 상태 미리보기

자주 하는 조정:
- 책상만 보이게 더 좁히기: `CollapsedVision.VisionRadius` 감소
- 경계가 너무 딱 잘린다: `VisionFeather` 증가
- 주변이 너무 많이 보인다: `DarknessStrength` 증가 또는 `VisionRadius` 감소
- 한번씩 주변이 보이게: `PlayRevealPulse()` 또는 `RevealSurroundings()` 호출

## 카메라

### 기본 게임 카메라: `ShowDownGameModeBase`

게임플레이 카메라의 기본 감도, 각도 제한, 숨쉬기 흔들림 값은 게임모드 쪽 설정이 기준입니다. 보통 `BP_ShowDownModeBase` 디테일에서 만집니다.

주요 값:
- `GameplayCameraLookSensitivity`
- `GameplayFallbackCameraLookSensitivity`
- `GameplayCameraMinPitch`, `GameplayCameraMaxPitch`
- `GameplayCameraMinYawOffset`, `GameplayCameraMaxYawOffset`
- `bInvertGameplayCameraMouseY`
- `bEnableGameplayCameraBreathingSway`
- `GameplayCameraBreathingSwaySpeed`
- `GameplayCameraBreathingSwayRotationAmplitude`
- `GameplayCameraBreathingSwayLocationAmplitude`
- `GameplayCameraBreathingSwayBlendInTime`

숨쉬기 느낌:
- 더 빠르게: `GameplayCameraBreathingSwaySpeed` 증가
- 더 크게 흔들리게: `RotationAmplitude` 또는 `LocationAmplitude` 증가
- 시작할 때 천천히 들어오게: `BlendInTime` 증가

### 실제 입력/고정 카메라 처리: `ShowDownPlayerController`

마우스 입력, 중앙 조준, 십자선, 고정 카메라 흔들림은 플레이어 컨트롤러가 실제 적용합니다.

주요 값:
- `LookSensitivity`, `MinPitch`, `MaxPitch`, `MinYaw`, `MaxYaw`: Pawn 카메라 기본 조작
- `bEnableFixedCameraBreathingSway`, `BreathingSwaySpeed`, `BreathingSwayRotationAmplitude`, `BreathingSwayLocationAmplitude`: 고정 카메라 숨쉬기
- `bShowCenterCrosshair`, `CenterCrosshairSize`, `CenterCrosshairThickness`, `CenterCrosshairColor`: 중앙 십자선
- `bEnableInteractableAimOutline`, `InteractableOutlineColor`, `InteractableOutlineThickness`, `InteractableOutlineOpacity`: 상호작용 윤곽선
- `CenterScreenTraceDistance`: 중앙 조준 트레이스 거리

### 허브/메뉴 카메라: `ShowDownHubFlowManager`

메뉴, 상점, 랭킹, 싱글 프리뷰에서 어떤 카메라를 볼지 관리합니다. 보통 `BP_ShowDownHubFlowManager`에서 만집니다.

주요 값:
- `LoginCamera`
- `MainMenuCamera`
- `ShopCamera`
- `RankingCamera`
- `GameCamera`
- `CameraBlendTime`
- `GameCameraLookSensitivity`
- `GameCameraMin/MaxPitch`
- `GameCameraMin/MaxYawOffset`
- `GameCameraBreathingSway...`
- `ReturnToHubDelay`

주의:
- `GameCamera`는 허브에서 싱글 플레이 프리뷰/게임 화면으로 들어갈 때 쓰는 카메라입니다.
- 게임 진행 중 카메라 감도 기준은 `ShowDownGameModeBase` 값과 연결됩니다.

### 컷신 카메라 묶음: `SDCameraDirector`

여러 카메라 샷을 이름으로 관리할 때 씁니다. "gunShotCamera", "gunShotEnemyCamera" 같은 것을 하드코딩하지 않고 `ShotId`로 관리하고 싶을 때 여기가 맞습니다.

주요 값:
- `Shots`: 샷 목록
- `ShotId`: 호출할 이름
- `Camera`: 실제 카메라 액터
- `BlendTime`, `BlendFunction`, `BlendExponent`
- `bLockMoveInput`, `bLockLookInput`, `bShowMouseCursor`
- `bEnableMouseLook`, `MouseLookSensitivity`, `MinPitch`, `MaxPitch`, `MinYawOffset`, `MaxYawOffset`
- `CameraShakeClass`, `CameraShakeScale`
- `bOverrideFOV`, `TargetFOV`, `FOVBlendTime`

### 테스트용 카메라: `SDCameraBlendTester`

카메라 블렌드/중앙 상호작용 테스트용 액터입니다. 최종 게임 규칙의 중심은 아니고, 테스트 레벨이나 빠른 확인용으로 봅니다.

## 총 연출

### `BP_SelfShotGun` / `SDSelfShotGunActor`

총 위치, 발사 타이밍, 카메라, 해머/방아쇠, 소리, 섬광, 맞았을 때 화면 효과가 모두 여기 있습니다.

주요 카테고리:
- `Self Shot Gun|First Person`: 내가 들고 쏠 때 위치/회전
- `Self Shot Gun|Target Shot`: 상대를 향해 쏠 때 위치/회전/조준점
- `Self Shot Gun|Cinematic Camera`: 총 쏠 때 카메라 전환
- `Self Shot Gun|Timing`: 들기/조준 유지/발사 유지/복귀 시간
- `Self Shot Gun|Mechanism`: 해머, 방아쇠, 실린더 애니메이션
- `Self Shot Gun|Shot Result`: 테스트용 실탄/공포탄 결과
- `Self Shot Gun|Effects`: 총소리/방아쇠 소리
- `Self Shot Gun|Effects|Empty Shot`: 공포탄 흔들림
- `Self Shot Gun|Effects|Tinnitus`: 이명 사운드
- `Self Shot Gun|Muzzle Flash`: 총구 섬광
- `Self Shot Gun|Hit Sequence`: 맞았을 때 화면 톤/흔들림/암전/회복

자주 하는 조정:
- 내가 쏠 때 총 위치: `FirstPersonCameraOffset`
- 내가 쏠 때 총 방향: `FirstPersonRotationOffset`
- 상대를 쏠 때 총 위치 기준: `TargetShotSourcePullDistance`, `TargetShotAimOffset`
- 상대를 쏠 때 총 방향: `TargetShotRotationOffset`
- 상대가 쏠 때 카메라: `EnemyShotCinematicCamera`
- 내가 맞았을 때 화면 번쩍/암전/회복: `Hit Sequence`
- 총구 섬광 세기: `MuzzleFlashIntensity`, `MuzzleFlashDuration`, `MuzzleFlashColor`

주의:
- 실제 라운드 결과에서 총 연출을 호출하는 쪽은 `ShowDownGameModeBase::PlaySelfShotGunPresentationThen`.
- 테스트로 강제 결과를 보고 싶으면 `UseGunWithForcedResult...` 계열 함수를 사용합니다.

## 담배 연출

### `MySDSmokableCigaretteActor` / `SDSmokableCigaretteActor`

담배를 집어 드는 위치, 연기, 빨간 불빛, 흔들림, 화면 효과를 조정합니다.

주요 값:
- `FirstPersonCameraOffset`, `FirstPersonRotationOffset`: 1인칭 위치/회전
- `LiftTime`, `PreSmokeMouthHoldTime`, `SmokeHoldTime`, `PostSmokeMouthHoldTime`, `ReturnTime`: 전체 타이밍
- `SmokeParticleDelay`, `SmokeSoundDelay`: 연기/소리 시작 시점
- `EmberIdleIntensity`, `EmberSmokeIntensity`: 담뱃불 밝기
- `EmberFlickerAmount`, `EmberFlickerSpeed`: 깜빡임
- `bEnableSmokeSway`, `SmokeSwayLocationAmplitude`, `SmokeSwayRotationAmplitude`, `SmokeSwaySpeed`: 손떨림/흔들림
- `bEnableSmokeScreenEffect`, `ArtToneController`, `SmokeScreenEffectSettings`: 담배 필 때 화면 톤 변화
- `ScreenEffectFadeInTime`, `ScreenEffectFadeOutTime`, `ScreenEffectMaxStrength`: 화면 효과 진입/복귀

## 버튼 연출

### `MySDPressableButtonActor` / `SDPressableButtonActor`

눌리는 사물형 버튼의 단순 애니메이션입니다.

주요 값:
- `LocalPressOffset`: 눌리는 방향/깊이
- `PressDownTime`: 내려가는 시간
- `HoldTime`: 눌린 상태 유지
- `ReturnTime`: 돌아오는 시간
- `bUseReturnBounce`: 돌아올 때 튕김 사용
- `ReturnBounceDistance`: 튕김 거리
- `OnPressed`, `OnPressFinished`: 블루프린트에서 이벤트 연결

## 카드와 카드 배치

### 카드 자체: `BP_Card` / `ACard`

카드 한 장의 선택/호버/슬롯 이동 연출을 만집니다.

주요 값:
- `Rank`: 카드 숫자
- `bFaceUp`: 앞면/뒷면
- `SelectedOffset`: 선택했을 때 올라오는 높이
- `HoverOffset`: 마우스 올렸을 때 올라오는 높이
- `MoveSpeed`: 선택/호버 이동 속도
- `bUseSlotAttachMotion`: 손/이마 위치로 이동할 때 연출 사용
- `SlotAttachDuration`: 날아가는 시간
- `SlotAttachArcHeight`: 날아가는 포물선 높이
- `SlotAttachOvershootDistance`: 살짝 지나쳤다가 돌아오는 거리
- `SlotAttachTargetScale`: 슬롯에 붙을 때 목표 크기
- `SlotAttachFlightRotationAmplitude`: 날아가며 흔들리는 회전
- `SlotAttachSettleDuration`, `SlotAttachSettleLocationAmplitude`, `SlotAttachSettleRotationAmplitude`, `SlotAttachSettleOscillations`: 도착 후 흔들림
- `InteractionBoundsExtent`: 클릭/조준 판정 크기

### 카드 앵커: `BP_PlayerHandAnchor`, `BP_PlayerForeheadAnchor`, `BP_OpponentHandAnchor`, `BP_OpponentForeheadAnchor`

카드가 놓이는 기준점입니다. 위치/회전은 액터 Transform으로 조정하고, 손패 배열은 레이아웃 값으로 조정합니다.

주요 값:
- `PlacementRole`: PlayerHand, PlayerForehead, OpponentHand, OpponentForehead
- `CardSpacing`: 카드 간격
- `ForwardOffset`: 카드가 앞쪽으로 펼쳐지는 거리
- `HeightOffset`: 높이
- `LeanAngle`: 기울기
- `LayerStep`: 겹침 깊이
- `SelectedOffset`, `HoverOffset`, `MoveSpeed`: 해당 위치의 카드 상호작용 움직임

### 좌석 기반 배치: `SDPlayerSeat`

플레이어/상대 자리 단위로 손패/이마 슬롯을 묶어 조정할 때 씁니다.

주요 값:
- `SeatSide`
- `HandSlotLocation`, `HandSlotRotation`
- `HeadSlotLocation`, `HeadSlotRotation`
- `bOverrideGameModeHandLayout`
- `HandSpacing`, `HandForwardOffset`, `HandHeightOffset`, `HandLeanAngle`
- `bOverrideCardMotion`, `SelectedOffset`, `HoverOffset`, `MoveSpeed`

## 게임 흐름과 스테이지 값

### `BP_ShowDownModeBase` / `ShowDownGameModeBase`

게임 규칙, 카드 수, 스테이지, 베팅, 자동 진행, 카메라 기본값이 있습니다.

주요 값:
- `CardClass`: 생성할 카드 블루프린트
- `HandCount`: 손패 수
- `CardSpacing`, `ForwardOffset`, `HeightOffset`, `LeanAngle`, `LayerStep`: 기본 손패 레이아웃
- `StageRules`: 스테이지별 규칙
- `StartingLives`: 시작 목숨
- `MinimumBet`: 최소 베팅 총알 수
- `CollectorBluffRate`: AI 블러프 확률
- `CollectorAggression`: AI 공격성
- `bSevenFoldLoadsSix`: 7 카드로 폴드할 때 6발 장전 규칙
- `bAutoStartOnBeginPlay`: BeginPlay 자동 시작
- `bAutoAdvanceRevealWithoutPresentation`: 공개 연출이 없어도 자동 진행
- `RevealAutoAdvanceSeconds`: 자동 진행 대기 시간
- `MultiplayerCardRevealGap`: 멀티 공개 카드 중 가장 가까운 두 카드의 목표 간격. 기본값 `20.0`
- `MultiplayerBetActionIntervalSeconds`: 다음 플레이어에게 베팅 차례가 넘어가기 전 대기. 기본값 `0.8초`
- `MultiplayerPreRevealDelaySeconds`: 전원 베팅 완료 후 카드 공개 시작 전 대기. 기본값 `1.2초`
- `MultiplayerPostRevealHoldSeconds`: 공개 카드가 모두 정착한 뒤 첫 총격 전 대기. 기본값 `1.5초`
- `MultiplayerPostShotPauseSeconds`: 연속 총격 사이와 마지막 총격 후 다음 흐름 전 대기. 기본값 `0.5초`
- `bShowGameFlowDebugMessages`: 게임 흐름 디버그 메시지

멀티 흐름:
- 일반 베팅은 행동을 보여 준 뒤 `0.8초` 후 다음 차례로 넘어갑니다. 마지막 베팅에서는 이 값을 더하지 않고 `1.2초` 후 공개를 시작합니다.
- 폴드는 폴드 확정 → 해당 카드 공개 → 카드 정착 후 대기 → 폴드한 플레이어 총격 순서입니다. 총격 뒤 한 명만 남으면 라운드를 끝내고, 둘 이상이면 남은 베팅 상태에 따라 다음 차례 또는 전체 공개로 이어집니다.
- `MultiplayerCardRevealGap`은 테이블 중심 반지름이 아니라 실제 카드 사이 간격입니다. 좌석이 마주 보거나 이웃해 있어도 가장 가까운 공개 카드 간격이 이 값에 맞춰집니다.

## 허브, UI, 결과 연출

### `BP_ShowDownHubFlowManager`

허브 화면과 메뉴 카메라, 싱글/멀티 진입, 게임 종료 후 복귀를 담당합니다.

주요 값:
- `LoginWidgetClass`, `MainMenuWidgetClass`, `ShopWidgetClass`, `RankWidgetClass`, `MultiplayerWidgetClass`, `LobbyWidgetClass`
- `LoginCamera`, `MainMenuCamera`, `ShopCamera`, `RankingCamera`, `GameCamera`
- `CameraBlendTime`
- `bDeveloperAutoStartSinglePlayer`
- `bDeveloperSkipOnlineReward`
- `MultiplayerLobbyLevelName`, `MultiplayerLevelName`
- `ReturnToHubDelay`

결과 연출:
- `OnGameResultPresentation(Winner)`는 블루프린트 구현 이벤트입니다.
- 결과 카메라/결과 UI/사운드를 넣고 끝나면 `FinishResultAndReturnToHub()`를 호출합니다.

## 이벤트를 걸어야 할 때

### `ShowDownGameStateBase`

UI나 연출 블루프린트가 게임 흐름에 반응하려면 GameState의 이벤트를 구독하는 쪽이 가장 안전합니다.

주요 이벤트:
- `OnPhaseChanged`
- `OnBetChanged`
- `OnCardsRevealed`
- `OnRoundResolved`
- `OnRouletteStarted`
- `OnRouletteResult`
- `OnLifeChanged`
- `OnStageChanged`
- `OnGameOver`
- `OnPresentationStarted`
- `OnPresentationFinished`
- `OnCollectorDialogue`
- `OnCollectorLLMDecision`
- `OnChatMessageReceived`
- `OnMultiplayerRouletteStarted`
- `OnMultiplayerRouletteResult`

사용 기준:
- "게임 규칙이 바뀌었을 때 UI/연출을 띄우고 싶다"는 GameState 이벤트
- "특정 사물 자체가 눌렸거나 사용됐다"는 해당 사물 액터의 `OnPressed`, `OnSmokeStarted`, `OnGunFired` 같은 이벤트
- "라운드 진행을 멈추고 연출 끝을 기다리게 하고 싶다"는 `StartPresentation/EventStart`와 `FinishPresentation/EventEnd`

## 멀티플레이 카메라와 좌석

멀티플레이 좌석 카메라는 레벨에 배치한 `CameraActor` 태그로 찾습니다.

규칙:
- 1번 좌석 카메라 태그: `MP_SeatCamera_1`
- 2번 좌석 카메라 태그: `MP_SeatCamera_2`
- 이후 같은 패턴

카메라가 없으면:
- `ShowDownPlayerController`가 fallback seat camera를 로컬로 생성합니다.

좌석 앵커:
- `SDMultiplayerSeatAnchor`는 멀티 좌석 카메라 transform에서 손패/이마 슬롯을 런타임으로 만듭니다.

## 설정할 때 주의할 것

- 같은 기능 값이 여러 곳에 있으면 "현재 어떤 흐름에서 쓰이는지"가 중요합니다.
- 허브 카메라/프리뷰는 `ShowDownHubFlowManager`.
- 실제 게임 진행 카메라 기본값은 `ShowDownGameModeBase`.
- 실제 카메라 입력 적용은 `ShowDownPlayerController`.
- 화면 톤은 `SDArtToneController`.
- 암흑 시야는 `TableVisionDirector`.
- 총/담배/버튼은 각각 사물 액터 BP에서 만지는 것이 가장 안전합니다.
- 코드 기본값을 바꾸면 새로 배치되는 액터에는 적용되지만, 이미 레벨/BP 인스턴스에서 오버라이드된 값은 그대로 남을 수 있습니다.

