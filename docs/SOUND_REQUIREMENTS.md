# HandVR 효과음(SFX) 요구 목록

보고서 "미완료 항목 — 사운드(예정): 재질별 효과음, 공간 음향" 기준 +
실험 자극(RHI 쓰다듬기 / 시각적 포착 / 표류 / 망치 위협)·UI·설문 흐름에 필요한 사운드 정리.

> 포맷: **WAV (16-bit/48kHz 모노)** 권장 — UE 임포트 + 3D 공간화(Attenuation) 용이. 임포트 위치: `/Game/Audio/`.
> 모노 + Attenuation(감쇠) 적용해야 Quest에서 소리가 해당 프롭 위치에서 들림(공간 음향).

## 1. 핵심 실험 자극음 (★ 우선순위 높음 — 시각-촉각/시각-청각 동기화가 실험의 본질)

| # | 사운드 | 트리거(코드 위치) | 목적 | 특성 | 공간화 |
|---|--------|------------------|------|------|--------|
| 1 | **붓 쓰다듬기** `SFX_BrushStroke` | RHI `TickRHI` — 1Hz 좌우 왕복마다 (스트로크 방향 전환 시) | 시각(붓 움직임)과 청각/촉각 동기화 → 신체소유감 유도 | 부드러운 사각사각/스침, 0.3~0.5s, 루프 아님(스트로크당 1회) | 가짜 손 위치 |
| 2 | **망치 들어올림** `SFX_HammerLift` | Threat `TickThreat` stage0(lift) 진입 | 위협 예고(긴장 고조) | 낮은 우우웅/공기 가르는 소리, 0.5~0.8s | 망치 위치 |
| 3 | **망치 내려치기(스윙)** `SFX_HammerSwing` | Threat stage1(strike) 진입 | 빠른 하강의 위협감 | 빠른 휙! whoosh, 0.2~0.3s | 망치 위치 |
| 4 | **망치 충격(임팩트)** `SFX_HammerImpact` ★ | Threat stage2(impact) 진입 — 손/책상 도달 순간 | **위협 자극의 핵심**(놀람·위협감 설문) | 강하고 짧은 쾅/탕, 0.1~0.2s, 어택 강함 | 충격 지점(손) |
| 5 | **재질별 충격 변형** (선택) `SFX_ImpactWood` / `SFX_ImpactSkin` | 같은 stage2, 대상 재질에 따라 분기 | 보고서 "재질별 효과음" | 나무=딱딱/울림, 살=둔탁 | 충격 지점 |

## 2. UI / 인터랙션음 (우선순위 중)

| # | 사운드 | 트리거 | 목적 | 특성 |
|---|--------|--------|------|------|
| 6 | **버튼 누름** `SFX_ButtonPress` | `AHTDButton::HandleOverlap` 성공 시 | 손-버튼 접촉 피드백(컨트롤러 없는 핸드트래킹이라 청각 확인 중요) | 짧은 틱/클릭, 0.1s |
| 7 | **버튼 호버**(선택) `SFX_ButtonHover` | 손이 버튼 근처 진입 | 조준 보조 | 아주 작은 틱 |

## 3. 실험 흐름 / 피드백음 (우선순위 중)

| # | 사운드 | 트리거 | 목적 | 특성 |
|---|--------|--------|------|------|
| 8 | **조건 시작** `SFX_ConditionStart` | `EnterExperiment` | 새 조건 시작 알림 | 부드러운 상승 톤, 0.4s |
| 9 | **설문 표시** `SFX_SurveyAppear` | `ASurveyManager::BeginSurvey` | 설문 등장 알림(조건 종료→설문 자동전환) | 차분한 알림 chime, 0.5s |
| 10 | **응답 기록** `SFX_AnswerLogged` | `OnAnswerPressed` | 응답 확인 | 짧은 확인음(버튼음과 구분), 0.15s |
| 11 | **전체 완료** `SFX_AllDone` | 8조건+설문 모두 종료 | 세션 종료 | 완료 jingle, 1s |

## 4. 환경/공간 음향 (우선순위 낮음 — 몰입/실재감)

| # | 사운드 | 트리거 | 목적 | 특성 |
|---|--------|--------|------|------|
| 12 | **실험실 앰비언스** `AMB_Room` | `AWorldSetup::BeginPlay` 루프 재생 | 정적 공간의 실재감(report "공간 음향") | 조용한 실내 룸톤/공조음, 끊김없는 루프, -30dB 정도 낮게 | 무지향(2D) 또는 룸 중심 |

## 구현 메모 (오디오 파일 확보 후)
- C++ 훅: `UGameplayStatics::SpawnSoundAtLocation(World, USoundBase*, Loc)` 를 위 트리거 지점에 추가. 루프/앰비언스는 `UAudioComponent`.
- 각 프롭(붓/망치)에 `UAudioComponent` 자식으로 붙이면 위치 자동 추종.
- `USoundAttenuation` 에셋 1개 만들어 1·2·3·4·5·6·8 에 공통 적용(감쇠 반경 ~5m).
- 무료 소스: freesound.org(CC0), Quest 권장 포맷으로 변환(48kHz WAV).
- **현재 코드 상태**: 위 트리거 지점(TickRHI/TickThreat/HandleOverlap/EnterExperiment/BeginSurvey)은 이미 존재 → 오디오 파일만 주면 `SpawnSoundAtLocation` 한 줄씩 꽂으면 됨. 원하면 사운드 파일 없이도 훅(빈 USoundBase UPROPERTY + null-guard 재생)부터 먼저 깔아둘 수 있음.

## 최소 세트 (시간 없으면 이것만)
**#1 붓 쓰다듬기, #4 망치 충격, #6 버튼 누름, #9 설문 표시, #12 룸 앰비언스** — 이 5개면 실험의 시청각 동기화·위협·UI·몰입 핵심은 커버됨.
