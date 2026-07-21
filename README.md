# SHOWDOWN

> 카드로 승부하고, 총알로 대가를 치르는 1:1 심리전 게임

<p align="center">
  <a href="https://drive.google.com/file/d/1BOEzRdhACCg_lNfIhQVS-qlFr1SjOYXc/view?usp=sharing"><strong>다운로드 바로가기</strong></a>
  &nbsp;·&nbsp;
  <a href="https://jjinzxx.github.io/showdown-page/"><strong>회원가입 사이트 바로가기</strong></a>
</p>

## 목차

- [프로젝트 개요](#프로젝트-개요)
- [팀원 소개 & 타임라인](#팀원-소개--타임라인)
- [게임 소개](#게임-소개)
- [기술 스택](#기술-스택)
- [프로젝트 디테일](#프로젝트-디테일)

## 프로젝트 개요

| 항목 | 내용 |
| --- | --- |
| 프로젝트명 | **SHOWDOWN** |
| 장르 | 1:1 카드 심리전 · 러시안룰렛 · 멀티플레이 |
| 개발 기간 | 2026.06.11 ~ 2026.07.21 |
| 개발 인원 | 4명 |
| 엔진 | Unreal Engine 5.6 |
| 플랫폼 | Windows PC |
| 핵심 키워드 | 카드 베팅, 확률과 리스크, 사물형 UI, 시네마틱 연출, 온라인 대전 |

SHOWDOWN은 카드의 숫자만으로 승패를 결정하지 않습니다. 플레이어는 자신의 패와 상대의 행동을 읽으며 `Check`, `Raise`, `Call`, `Fold` 중 하나를 선택하고, 베팅한 총알 수만큼 커지는 위험을 감수해야 합니다. 카드 승부의 결과는 러시안룰렛으로 이어지며, 매 라운드의 선택이 생존 확률을 바꿉니다.

## 팀원 소개 & 타임라인

### 팀원 소개

| 팀원 | 담당 |
| --- | --- |
| 임현진 | 연출 총괄, 아트 톤, 사물형 UI, 레벨 통합 |
| 김윤아 | 에셋 리서치, 분위기 탐색, 맵 드레싱 |
| 이진헌 | 기술 통합, 멀티플레이, LLM·DB, Git 병합 |
| 박형빈 | 코어 게임 루프, 카드·베팅·룰렛 규칙, 게임 이벤트 |

### 타임라인

| 기간 | 주요 작업 |
| --- | --- |
| 06.11 ~ 06.17 | 게임 규칙 설계, 역할 분담, 카드·베팅·룰렛 코어 프로토타입 |
| 06.18 ~ 06.30 | 사물형 UI, 테스트 레벨, 아트 톤 및 에셋 방향 구축 |
| 07.01 ~ 07.10 | EOS 멀티플레이, Supabase 로그인·데이터 연동, UI 통합 |
| 07.11 ~ 07.17 | 카드 공개·사격·카메라 연출, 상점·랭크·음성 기능 통합 |
| 07.18 ~ 07.21 | 멀티 동기화 안정화, 오디오·HUD 개선, 최종 빌드 점검 |

## 게임 소개

### 기본 흐름

```text
카드 선택 → 베팅(Check/Raise/Call/Fold) → 카드 공개
        → 패배 대상 결정 → 러시안룰렛 → 생명 차감 → 다음 라운드
```

### 주요 특징

- **카드와 러시안룰렛의 결합**: 패배가 즉시 끝으로 이어지지 않고, 베팅한 총알 수에 따라 생존 확률이 달라집니다.
- **1:1 심리전**: 상대의 카드와 베팅 행동을 추측해 위험을 키우거나 물러날 수 있습니다.
- **사물형 UI**: 버튼, 카드, 총알과 같은 월드 오브젝트를 통해 게임 상태와 행동을 표현합니다.
- **온라인 대전**: Epic Online Services 기반 로비와 상태 동기화로 1:1 플레이를 지원합니다.
- **시네마틱 연출**: 카드 공개, 약실 회전, 사격과 피격을 카메라·조명·사운드로 전달합니다.
- **계정과 성장 요소**: Supabase 기반 로그인, 프로필, 재화, 랭크와 캐릭터 상점을 제공합니다.
- **음성 기능**: OpenAI 음성 API와 로컬 Whisper·eSpeak NG 런타임을 활용할 수 있습니다.

## 기술 스택

| 구분 | 기술 |
| --- | --- |
| Game Engine | Unreal Engine 5.6 |
| Language | C++, Blueprint |
| UI | UMG, Common UI |
| Online | Epic Online Services, Online Subsystem EOS, EOS Voice Chat |
| Backend | Supabase, PostgreSQL, Edge Functions, Realtime |
| AI & Voice | OpenAI API, Whisper, eSpeak NG |
| Media | Level Sequence, Movie Scene, Unreal Audio |
| Collaboration | Git, GitHub, Git LFS |

## 프로젝트 디테일

| 문서 | 내용 |
| --- | --- |
| [프로젝트 관리](docs/project-management.md) | 역할 분담, 브랜치·에셋·레벨 관리 방식 |
| [기능 구현 현황](docs/implementation-status.md) | 코어, 멀티플레이, 백엔드, UI 구현 상태 |
| [트러블 슈팅](docs/troubleshooting.md) | LFS, 멀티 동기화, 맵 충돌, 음성 런타임 문제 해결 |
| [회고](docs/retrospective.md) | 프로젝트에서 얻은 교훈과 KPT 회고 |

---

© 2026 SHOWDOWN Team
