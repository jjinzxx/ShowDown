# 기능 구현 현황

[← README로 돌아가기](../README.md)

> 2026.07.21 최종 빌드 기준입니다. `완료`는 저장소에 기능 구현이 존재한다는 뜻이며, 서비스 운영 수준의 무결함을 의미하지는 않습니다.

## 코어 게임

| 기능 | 상태 | 관련 구현 |
| --- | --- | --- |
| 카드 생성·분배·선택 | 완료 | `Card`, `CardSystem` |
| Check·Raise·Call·Fold | 완료 | `BettingSystem`, `SDBetActionPanelActor` |
| 카드 공개와 승패 판정 | 완료 | `RoundResolver`, `SDCardRevealLayout` |
| 러시안룰렛 확률 계산 | 완료 | `RouletteSystem` |
| 생명·스테이지·게임오버 | 완료 | `ShowDownGameModeBase`, `ShowDownGameStateBase` |
| 상대 AI | 완료 | `Collector`, `CollectorAISystem` |
| 코어 자동화 테스트 | 구현 | `ShowDownCoreSystemsTests.cpp` |

## 멀티플레이

| 기능 | 상태 | 관련 구현 |
| --- | --- | --- |
| EOS 로그인과 세션 | 완료 | `ShowDownEosSubsystem` |
| 멀티플레이 로비 | 완료 | `ShowDownLobbyWidget`, `ShowDownMultiplayerWidget` |
| 좌석·플레이어 상태 동기화 | 완료 | `SDPlayerSeat`, `SDPlayerState` |
| 라운드·베팅·카드 공개 동기화 | 완료 | `SDMultiplayerRoundFlow` |
| 사격·피격 연출 동기화 | 안정화 진행 | 멀티 RPC 및 로컬 연출 흐름 |
| 채팅·음성 | 구현 | `ShowDownChatWidget`, `ShowDownVoiceSubsystem` |

## 계정과 백엔드

| 기능 | 상태 | 관련 구현 |
| --- | --- | --- |
| ID 기반 로그인 | 완료 | Supabase Edge Function `login-with-id` |
| EOS 사용자 정보 연결 | 완료 | Edge Function `eos-userinfo` |
| 프로필·재화·보상 | 완료 | `SupabaseSubsystem`, DB migrations |
| 캐릭터 상점·스킨 | 완료 | `ShowDownShopWidget`, character shop migrations |
| 랭크 표시 | 완료 | `ShowDownRankWidget`, `ShowDownMultiRankWidget` |
| 중복 보상 방지 | 완료 | reward claim idempotency migration |

## UI와 연출

| 기능 | 상태 | 관련 구현 |
| --- | --- | --- |
| 로그인·메인 메뉴·설정 | 완료 | UMG 위젯 및 C++ UI 클래스 |
| 카드·베팅·탄약 상태 HUD | 완료 | 상태 위젯과 월드 UI |
| 카드 공개·카메라 전환 | 완료 | `SDCameraDirector`, `SDCardRevealLayout` |
| 장전·약실·사격 연출 | 완료 | `SDGunVisionSequenceSubsystem`, `SDSelfShotGunActor` |
| 아트 톤·포스트 프로세스 | 완료 | `SDArtToneController`, ArtTone assets |
| BGM·효과음 | 완료 | `ShowDownAudioSubsystem` |

## 남은 개선 과제

- 다양한 네트워크 지연 환경에서 멀티플레이 재검증
- 세션 이탈·재접속과 비정상 종료 복구 강화
- 미사용 대용량 에셋 정리와 패키지 크기 최적화
- 외부 API 장애 시 사용자 안내와 대체 대사 흐름 개선
- 전체 게임 흐름을 대상으로 한 통합·패키징 테스트 확대

알려진 멀티플레이 이슈는 [Multiplayer_Known_Issues.md](Multiplayer_Known_Issues.md)에서 별도로 관리합니다.
