# HandVR 설계 / 아키텍처 문서 (DESIGN.md)

> VR 인지왜곡 검증 실험 — Quest 3 · Unreal Engine 5.7.4 · C++
> 출처 교차참조: `.omc/DESIGN_BRIEF.md`(절충형 확정, 단일 진실원) · `.omc/report_extracted.txt`(팀 중간보고서 5장 실험계획) · `Source/HandTrackingDemo/*`(실제 코드) · `.omc/research/lab_asset_recommendation.md`(실험실 에셋 연구).
> 이 문서는 위 출처에 **명시된 사실만** 정리한다. 코드/보고서/브리프에 없는 내용은 기재하지 않는다.

---

## 1. 목표 · 핵심 가설

### 1.1 목표
VR 환경에서 **현실 소품(촉각)과 가상 오브젝트(시각)를 의도적으로 불일치**하게 설계하여, 사용자의 **감각 왜곡(인지왜곡) 과정**을 실험적으로 탐구한다. 구체적으로는 정밀한 핸드 트래킹과 물리 법칙 기반 렌더링을 통해 **시각 우위성(visual dominance)이 촉각 인지에 미치는 영향**을 정량 측정한다.

세부 목표(보고서 1.1):
- Meta Quest 3 기반 **1인 체험형 VR 실험 환경** 구축(실험실·책상·의자·조명 포함).
- OpenXR 핸드 트래킹으로 **컨트롤러 없는 자연스러운 손 인터랙션** 구현.
- 패시브 햅틱스 + 정교한 렌더링을 활용한 **신체화(embodiment) 유도**.
- 몰입감/실재감 변화 측정 및 **감각왜곡 발생 여부 확인**.

### 1.2 핵심 가설
> 시각 정보는 촉각 정보보다 우위에 있으며, 따라서 충분히 정교하게 설계된 VR 환경에서 사용자는 실제 자극과 불일치하는 **감각왜곡 현상을 경험**할 수 있을 것이다. (보고서 1.2)

실험계획(보고서 5장)에서 더 구체화한 두 하위 가설:
- **(공간 임계점)** 가상 손(시각)과 실제 손(고유수용감각)의 **공간적 좌표 불일치가 증가**할수록 신체소유감이 줄어들고, 특정 임계값을 넘는 순간 인지왜곡이 중단될 것이다.
- **(촉각 왜곡)** 신체소유감이 활성화된 상태에서는 실제 자극 부위와 시각 자극 부위가 달라도, 뇌는 **시각 정보를 우선**하여 표류된 고유수용감각에 따라 촉각왜곡이 일어날 것이다.

본 실험의 상위 목표(보고서 5.1): 가상 신체소유감을 형성하는 **시·공간적 허용 오차의 임계점**을 측정하고, 소유감 활성 상태에서 시각 자극이 물리적 촉각 불일치를 **어느 정도까지 무시하고 인지왜곡을 유지**하는지 정량 분석한다.

---

## 2. 이론적 근거

### 2.1 고무손 착각 (Rubber Hand Illusion, RHI)
시각 자극과 촉각 자극이 **동기화되어 일치**하면 가짜 손도 자신의 손처럼 느끼게 되는 현상이다. 1998년 Matthew Botvinick & Jonathan Cohen의 연구 이후 다양한 후속 연구로 반복 검증되었다. 본 프로젝트는 이 패러다임을 VR로 옮겨, 붓이 손을 **0.5Hz 왕복**으로 쓰다듬는 동기 자극(synchronous stroking)을 통해 신체소유감을 유도한다. 자극의 **일치/불일치(StimulusMatched)** 조건을 조작하여 동기성이 착각 강도에 미치는 영향을 본다.

### 2.2 시각적 포착 (Visual Capture)
여러 감각 정보가 충돌할 때 사람은 일반적으로 **눈으로 보는 정보를 더 우선**해서 받아들이는 경향이 있다. 이는 뇌가 비교적 신뢰도가 높은 정보를 기준으로 다른 감각을 해석하려는 과정으로 이해된다. 본 프로젝트는 보이는 손을 실제 손보다 **측면으로 이동(LateralOffsetCm, 기본 +30cm)**시킨 뒤 사용자가 "보이는 손"으로 책상 위 빨간 구 표적을 만지게 하여, 시각 위치와 실제 위치가 다를 때 어느 쪽을 신뢰하는지를 검증한다.

### 2.3 고유수용감각 표류 (Proprioceptive Drift)
시각 정보의 영향으로 자신의 신체 위치를 **실제보다 다른 위치에 있다고 느끼는** 현상이다. RHI 실험에서 함께 관찰되며, 신체 위치 감각이 쉽게 왜곡될 수 있음을 보여준다. 본 프로젝트는 Visual Capture에 더해 보이는 손의 Y 오프셋을 `base + DriftAmplitudeCm·sin(2π·DriftHz·t)`(기본 30 + 8·sin(2π·0.4·t) cm)로 **천천히 흔들고**, 동시에 **실제 손목 위치에 파란 큐브 마커**를 띄워 시각-고유수용감각 간 표류를 가시화·측정한다.

---

## 3. 시스템 구성

보고서 2.1 기술 스택 + 코드(`Build.cs`, `HandPawn.cpp`) 기준.

| 구분 | 기술 / 도구 | 역할 |
|---|---|---|
| 엔진 | **Unreal Engine 5.7.4** | VR 환경 및 렌더링 전체 |
| 플러그인 | **OculusXR Plugin 201.0** | Quest 3 핸드 트래킹 / HMD 연동 |
| 표준 | **OpenXR (`XR_EXT_hand_tracking`)** | 손 관절 추적 데이터 수신 |
| 빌드 | **Visual Studio 2026 / Android SDK 34** | PC 에디터 빌드 및 Quest APK 빌드 |
| 버전관리 | **Git / GitHub** | 소스코드 및 씬 에셋 공유 |

빌드 의존 모듈(`HandTrackingDemo.Build.cs` `PublicDependencyModuleNames`):
`Core`, `CoreUObject`, `Engine`, `InputCore`, `HeadMountedDisplay`, `EnhancedInput`, `XRBase`, `OculusXRInput`, `OculusXRHMD`, `UMG`, `Slate`, `SlateCore`.
Android 타겟 시 `HandTrackingDemo_Quest_APL.xml`을 `AndroidPlugin`으로 추가한다.

핵심 런타임 특성(코드 확인):
- **트래킹 원점 = LocalFloor** — `AHandPawn::BeginPlay()`에서 `SetTrackingOrigin(EHMDTrackingOrigin::LocalFloor)` 호출. 착석/기립 모두 OK, 사용자 실제 바닥이 Z=0.
- **ConfidenceBehavior = None** — 트래킹 신뢰도가 낮아도 손 메쉬를 제거하지 않아 신체소유감 단절 방지(`HandPawn.cpp` `OutHand->ConfidenceBehavior`).
- **SetSimultaneousHandsAndControllersEnabled(true)는 현재 호출하지 않음** — Meta XR 플러그인이 VR 세션 바인딩 전에 호출되면 null OpenXR Session 핸들을 역참조해 크래시(`OculusXRSimultaneousHandsAndControllersExtensionPlugin.cpp:18`)하기 때문. "컨트롤러를 쥐면 손 메쉬가 사라지는" 실패 모드를 수용하기로 함(`HandPawn::BeginPlay()` 주석). 보고서 본문(2.x/4.1)에는 멀티모달 기능이 설계 의도로 기술돼 있으나, 실제 코드는 안정성 때문에 호출을 생략한 상태.

```
[Quest 3 HMD + 핸드센서]
        │  OpenXR XR_EXT_hand_tracking
        ▼
[OculusXR Plugin 201.0]  (OculusXRHMD / OculusXRInput)
        │  손목 포즈 + 관절 데이터
        ▼
[UE 5.7.4  C++ 게임 모듈 HandTrackingDemo]
   MotionController → (Offset) → OculusXRHand  +  UMG(Slate) 3D 라벨/버튼
        │
        ▼
[APK → Quest 3 단독 실행]
```

---

## 4. 소프트웨어 아키텍처

### 4.1 클래스별 역할표

| 클래스 (파일) | 베이스 | 역할 |
|---|---|---|
| `AHandTrackingGameMode` (`HandTrackingGameMode.*`) | `AGameModeBase` | 진입점. `DefaultPawnClass = AHandPawn`. `StartPlay()`에서 `AWorldSetup` + `AExperimentManager`를 런타임 스폰. `-AutoShot` 커맨드라인 시에만 진단용 스크린샷 시퀀스 캡처(평상시 Tick 조기 return). |
| `AHandPawn` (`HandPawn.*`) | `APawn` | VR 카메라 + 양손 OculusXR 핸드 트래킹 + **시각용 손 offset**. `SetVisualOffset` / `ResetVisualOffsets` / `SetHandsVisible` / `GetRealWristLocation` / `GetVisualWristLocation` 제공. 손 계층(MC→Trigger/Offset→OculusXRHand) 구성. Tick 없음(`bCanEverTick=false`). |
| `AExperimentManager` (`ExperimentManager.*`) | `AActor` | **런타임 FSM**(Start↔MainMenu↔Experiment/Survey/Results). 9개 메뉴 버튼 스폰·표시 제어, 실험별 프롭 스폰/디스폰, offset 적용, 실험 Tick(RHI/Drift/Threat), 효과음 재생, 결과 패널 파싱·표시. `LateralOffsetCm`/`DriftAmplitudeCm`/`DriftHz` 등 EditAnywhere 변인. |
| `AHTDButton` (`VRButton.*`) | `AActor` | 3D 손 터치 버튼. `"HandTouch"` 태그 프리미티브가 BoxComponent에 overlap 시 `OnPressed(ButtonId)` 브로드캐스트. `RetriggerCooldown=1.0s` 재진입 무시. UMG 라벨(한글 CJK fallback). |
| `ASurveyManager` (`SurveyManager.*`) | `AActor` | 3D 월드 공간 **8문항(코드상 9개 측정항목 정의) 5점 리커트** 설문. 응답 시 자동 다음 문항. 완료 시 CSV 저장(`SurveyResults_YYYYMMDD_HHMMSS.csv`, UTF-8 BOM) + `OnFinished` 브로드캐스트. |
| `AWorldSetup` (`WorldSetup.*`) | `AActor` | 절차적/임포트 **방·책상·의자·조명** 빌드. 임포트 메쉬(StudyRoom/DiningTable/Chair/Brush/Hammer/Gauntlet) 있으면 사용, 없으면 정확한 치수의 절차적 큐브 fallback. 노출 잠금 + 6 PointLight + SkyLight + DirectionalLight. |
| `UVRLabelWidget` (`VRLabelWidget.*`) | `UUserWidget` | C++ 전용 UMG 라벨. Slate Roboto 컴포지트 폰트의 CJK fallback으로 **한글 출력**(별도 폰트 임포트 불필요). UHT 헤더명 유니크 요구로 `LabelWidget`→`VRLabelWidget` 명명. |
| `FHandTrackingDemoModule` (`HandTrackingDemo.*`) | (게임 모듈) | 게임 모듈 진입점. |

협력 관계 요약:
```
GameMode ──spawn──▶ WorldSetup        (방/책상/의자/조명)
   │      ──spawn──▶ ExperimentManager (FSM·메뉴·실험·설문·결과)
   │      ──default pawn──▶ HandPawn   (양손 트래킹 + 시각 offset)
ExperimentManager ──spawn/own──▶ HTDButton ×9, SurveyManager, 프롭 액터, 정보/결과 패널
HTDButton ──OnPressed(id)──▶ ExperimentManager::OnButtonPressed / SurveyManager::OnAnswerPressed
HandPawn.WristTrigger("HandTouch") ──overlap──▶ HTDButton.TriggerBox
ExperimentManager ──SetVisualOffset / SetHandsVisible / Get*WristLocation──▶ HandPawn
```

### 4.2 FSM 상태도 (Intro→Start→Menu→Experiment/Survey/Results)

코드 `EExperimentState`는 **Start / MainMenu / Experiment / Survey / Results** 5개를 정의한다. 브리프 2장의 **Intro(인트로 스토리)**는 신규 설계 항목으로, 아직 별도 상태로 코드화되어 있지 않으며(현재 코드는 `EnterStart()`로 시작) Start 이전 단계로 계획되어 있다. 아래 도식에서 Intro는 점선으로 표기한다.

```
                 (앱 시작)
                     │
        ┌────────────▼─────────────┐
        ┊  Intro (브리프 2장, 계획) ┊   2~3장 내러티브 패널, 신체소유감 사전조건 형성
        ┊  - 환영/안내              ┊   (현재 코드 미구현: Start로 직접 진입)
        └────────────┬─────────────┘
                     │ [시작] 준비
                     ▼
              ┌─────────────┐
              │   Start     │  단일 "시작" 버튼(녹색)만 표시
              └──────┬──────┘
                     │ BID_Start press
                     ▼
        ┌──────────────────────────────┐
        │          MainMenu            │  4열×2행 그리드(9버튼: 실험1~4 / 설문 / 결과 / 종료) + 중단/닫기는 숨김
        └──┬─────────┬─────────┬───────┘
   실험1~4 │   설문   │   결과   │ 종료
   (RHI/VC/ │          │          │ (QuitGame)
   Drift/   │          │          │
   Threat)  │          │          │
           ▼          ▼          ▼
     ┌──────────┐ ┌────────┐ ┌──────────┐
     │Experiment│ │ Survey │ │ Results  │
     │offset+프롭│ │8문항   │ │최근 CSV  │
     │+설명패널 │ │리커트  │ │패널표시  │
     └────┬─────┘ └───┬────┘ └────┬─────┘
       중단│        완료│        닫기│
   (BID_Stop)   (OnFinished)  (BID_Close)
          └───────────┼───────────┘
                      ▼
                 MainMenu (복귀)
```

전이 규칙(`OnButtonPressed`, 상태 가드 포함):
- `Start` 상태에서 시작 → `MainMenu`
- `MainMenu`에서 실험1~4 → `Experiment`(해당 타입), 설문 → `Survey`, 결과 → `Results`, 종료 → `QuitGame`
- `Experiment`에서 중단 → `MainMenu` (offset/프롭 즉시 원복)
- `Survey` 완료 → `MainMenu`
- `Results`에서 닫기 → `MainMenu`

> 브리프 1장 의도: 각 Experiment 종료(또는 지정 시간 경과) 시 **해당 조건 설문 자동 표시** → 응답 후 메뉴 복귀. 현재 코드는 Experiment 중단(`BID_Stop`)이 바로 `MainMenu`로 가고, 설문은 메뉴의 별도 버튼으로 진입한다. 조건별 자동 설문 연결은 절충형 설계 목표(§5.3) 항목이다.

### 4.3 손 계층 다이어그램 (MC → Trigger/Offset → OculusXRHand)

`AHandPawn` 생성자(`BuildHand` 람다)가 양손을 동일 패턴으로 구성한다. **핵심 설계**: 실제 손목 포즈는 `MotionController`가, 사용자에게 보이는 손 위치는 그 하위 `Offset` 노드가 담당해, **버튼 충돌(실제 손) ↔ 시각 표시(보이는 손)를 분리**한다(보고서 4.2 "손을 두 개로 나누는 방식").

```
SceneRoot (RootComponent)
 ├── VRCamera (UCameraComponent, bLockToHmd=true)
 │
 ├── LeftMC  (UMotionControllerComponent, MotionSource="Left")   ◀─ 실제 손목 포즈(런타임 추적)
 │     ├── LeftWristTrigger (USphereComponent r=4.5cm, tag "HandTouch")  ◀─ 버튼 overlap = REAL 손 기준
 │     └── LeftHandOffset   (USceneComponent)                            ◀─ ExperimentManager가 SetRelativeLocation으로 시각 변위
 │           └── LeftHand   (UOculusXRHandComponent, HandLeft, ConfidenceBehavior=None) ◀─ REAL+offset 위치에 손 메쉬 렌더
 │
 └── RightMC (UMotionControllerComponent, MotionSource="Right")  ◀─ (오른손, 좌측과 대칭)
       ├── RightWristTrigger (USphereComponent r=4.5cm, tag "HandTouch")
       └── RightHandOffset   (USceneComponent)
             └── RightHand   (UOculusXRHandComponent, HandRight, ConfidenceBehavior=None)
```

- `GetRealWristLocation(hand)` → MC의 월드 위치(버튼 overlap·Drift 실제 손 마커용).
- `GetVisualWristLocation(hand)` → Offset 노드의 월드 위치(보이는 손에 프롭을 앵커: RHI 붓·Threat 망치).
- `SetVisualOffset(L,R)` → 각 Offset 노드의 RelativeLocation 설정(VC/Drift의 +Y 변위).
- 알려진 한계(SETUP §6): 버튼 충돌은 **실제 손목** 기준이라, VC/Drift 중 보이는 손으로 버튼을 누르려 하면 안 눌리고 실제 손을 더듬어야 함(실험 검증과 UI 분리 — 의도된 동작).

---

## 5. 실험 설계

### 5.1 실험 4종 (현 구현)

`EExperimentType` = None / RHI / VisualCapture / Drift / Threat. 모든 시각 효과는 **즉시(instant)** — 페이드/램프 없음. 버튼 누른 그 프레임에 offset이 스냅되고 프롭이 나타난다.

| # | 실험 | 진입 즉시 효과 | Tick 동작 | 효과음 |
|---|---|---|---|---|
| 1 | **고무손(RHI)** | 추적 손 표시 유지. 사용자 오른손 위에 붓 스폰(붓털 끝을 손에 정렬). | `TickRHI`: 붓털 끝을 오른손 위에서 **1Hz(±6cm)** 좌우 왕복 sweep. | 붓: sweep이 손 중심을 지날 때(부호 전환) 1회. `PrevBrushOff` edge-detect. |
| 2 | **시각포착(VC)** | 양손 시각 offset = `(0, +LateralOffsetCm, 0)`(기본 +30cm). 책상 위 **빨간 구**(지름 8cm, z=73+6) 스폰. | (Tick 없음) | (선택) |
| 3 | **표류(Drift)** | VC와 동일 offset + 책상 위 **파란 큐브**(4cm) 스폰. | `TickDrift`: offset Y = `LateralOffsetCm + DriftAmplitudeCm·sin(2π·DriftHz·t)`. 파란 큐브를 **실제 오른 손목**에 추종. | (선택) |
| 4 | **망치위협(Threat)** | 추적 손 표시 유지. 사용자 오른손 위에 **망치** 스폰. | `TickThreat`: 4단계(들어올림→내려치기→충격→복귀) **Period=3s** 반복. 망치 머리를 손에 정렬. | 망치: STRIKE→IMPACT 교차(t=1.4) 시 1회. `PrevThreatT` edge-detect. |

> 보고서 4.5/브리프 3장은 RHI 붓을 **0.5Hz 왕복(왕복=2s)**, 효과음 2초 클립 1회/왕복으로 규정한다. 현재 `TickRHI` 코드는 `Hz=1.0f`로 구현되어 있다(코드↔사양 불일치 항목 — 0.5Hz로 맞추려면 `Hz`를 0.5로 수정). 본 문서는 두 출처를 모두 명시한다.

### 5.2 8케이스 2페이즈 표 (보고서 5장)

4명의 조원이 각각 피실험자가 되어 다음 케이스를 검증한다. 모든 설문은 **5점 리커트**(1=전혀 그렇지 않다, 5=매우 그렇다), 케이스별 평균 비교로 분석.

**Phase 1 — 신체소유감 활성 임계값(Threshold) 도출**

| 케이스 | 공간오차 | 시간오차 | 실험 목적 |
|---|---|---|---|
| Case1 | 최소화 | 최소화 | 최상의 동기화 상태에서 신체소유감 측정 |
| Case2 | 가변(5~30cm) | 최소화 | 신체소유감이 사라지는 **오차거리** 측정 |
| Case3 | 최소화 | 가변(100~500ms) | 신체소유감이 사라지는 **오차시간** 측정 |
| Case4 | 가변(5~30cm) | 가변(100~500ms) | 시/공간 오차 공존 시 임계값 측정 |

**Phase 2 — 신체소유감 활성화 상태에서의 촉각왜곡 검증**

| 케이스 | 신체소유감 | 촉각자극 | 예상결과 |
|---|---|---|---|
| Case5 | 활성 | 불일치 | 시각+표류된 고유수용감각(가상)이 촉각(현실)을 왜곡 |
| Case6 | 비활성 | 불일치 | 촉각왜곡이 일어나지 않음 |
| Case7 | 활성 | 시각자극만 부여 | 시각+표류된 고유수용감각(가상)이 촉각(현실)을 왜곡 |
| Case8 | 비활성 | 시각자극만 부여 | 촉각왜곡이 일어나지 않음 |

> '일치'는 실제 자극 부위와 시각적 자극 부위가 동일한 경우를 의미한다(보고서 주).

### 5.3 절충형 조건변인 매핑표 (8케이스 → 현 4실험 "조건 프리셋")

브리프 3장의 절충형 핵심: 보고서 8케이스 변인을 현 4실험 위에 **조건 프리셋**으로 얹는다. 공통 조정 변인 4종:

| 변인 | 코드/메커니즘 | 프리셋(브리프) | 비고 |
|---|---|---|---|
| **공간오차** `LateralOffsetCm` | `HandPawn::SetVisualOffset` Y변위(VC/Drift) | 5 / 15 / 30 cm | 시각 손을 +Y로 이동. 현 EditAnywhere 기본 30. |
| **시간오차** `LatencyMs` (신규) | **HandPawn 링버퍼 지연 렌더** — 시각 손을 실제보다 N ms 지연 표시 | 0 / 100 / 300 / 500 ms | 보고서 미완료 "현실-가상 좌표/시간 정합"(이종현 담당). 현 코드 미구현, 신규 추가 대상. |
| **자극 일치/불일치** `StimulusMatched` | RHI 붓을 보이는 위치(일치) 또는 다른 손가락쪽(불일치)에 자극 | matched / mismatched | 보고서 4.5/Phase2 변인. |
| **신체소유감 유도 on/off** | 붓 동기 자극(유도) 단계 유무 | on / off | Phase2 활성/비활성. |

8케이스 ↔ 변인 프리셋 매핑(절충형 운용):

| 케이스 | 기반 실험 | 공간오차 | 시간오차(Latency) | 일치/불일치 | 소유감 유도 |
|---|---|---|---|---|---|
| Case1 | RHI(또는 VC 동기 기준) | 최소(≈0~5) | 최소(0ms) | 일치 | on |
| Case2 | VC | 가변 5~30 | 최소(0ms) | 일치 | on |
| Case3 | RHI/VC | 최소 | 가변 100~500ms | 일치 | on |
| Case4 | VC/Drift | 가변 5~30 | 가변 100~500ms | 일치 | on |
| Case5 | RHI(불일치) | (활성 유지) | 동기 | **불일치** | **활성(on)** |
| Case6 | RHI(불일치) | (비활성) | 동기 | **불일치** | **비활성(off)** |
| Case7 | VC(시각자극만) | (활성 유지) | 동기 | 시각자극만 | **활성(on)** |
| Case8 | VC(시각자극만) | (비활성) | 동기 | 시각자극만 | **비활성(off)** |

운용 원칙(브리프 1장 / 보고서 4.4·4.6):
- 각 Experiment 종료(또는 지정 시간 경과) 시 **해당 조건 설문 자동 표시** → 응답 후 메뉴 복귀(목표).
- 8개 조건의 세부 값(거리·시간·자극 종류·지속시간)은 **별도 관리**로 빼서 재빌드 없이 전환(보고서 4.4).
- 실험실 안 **1~9번 버튼**으로 진행자가 헤드셋을 벗지 않고 원하는 조건만 재실행(보고서 4.4). 현 코드의 9개 메뉴 버튼이 그 기반.
- 모든 이벤트(조건 시작/끝/설문 답변)는 **발생 즉시 CSV 기록**(보고서 4.6).

---

## 6. 자극 사양

브리프 3~5장 + 코드(`ExperimentManager.cpp`) 기준.

| 자극 | 사양(브리프/보고서) | 코드 현황 |
|---|---|---|
| **붓 (RHI)** | 0.5Hz 왕복(왕복=2s), 손가락 위 고정 좌우 왕복. 일치=보이는 위치, 불일치=다른 손가락쪽. 붓 2초 효과음 1회/왕복. | `TickRHI`: 1Hz·±6cm sweep(사양 0.5Hz와 불일치), 붓털 끝을 손 위 ~1cm 정렬, sweep 부호전환마다 `brush` 효과음. 붓 렌더 길이 ~24cm, 핸들 크림색. |
| **망치 (Threat)** | 4단계 lift→strike→impact→return 반복. 충격 순간 망치 2초 효과음 1회. 동일 강도 반복(보고서 4.5). | `TickThreat`: Period 3s, LIFT(0~1s, 6→42cm + 롤 -35°), STRIKE(1~1.4s, 가속 하강 ease-in), IMPACT(1.4~1.7s, 머리=손, 미세 진동 0~0.6cm), RETURN(1.7~3s). t=1.4 교차 시 `hammer` 효과음. 머리를 손에 정렬, 다크 그레이, 렌더 길이 ~33cm. **실제로 손에 닿지 않음**(설명 패널 명시). |
| **빨간 구 (VC 표적)** | `/Engine/BasicShapes/Sphere`, 지름 ~8cm, 책상 위 z=73+6, BasicShapeMaterial 빨강(`bForceTint`). 닿으면 피드백음(선택). | 코드 일치: Sphere, 8cm, z=79, Tint(1.0,0.15,0.15) `bForceTint=true`, NoCollision. |
| **파란 큐브 (Drift 마커)** | 4cm, 실제 손목 추종. 시각 손(+offset)과 분리돼 "진짜 손 위치"를 표시. 반투명/발광 고려. | 코드 일치: 큐브 fallback 4cm, Tint(0.3,0.7,1.0), `TickDrift`에서 실제 오른 손목에 추종, NoCollision. |

공통(브리프 4·5장): 프롭은 모두 **충돌 없음(NoCollision)**, 매 틱 위치 갱신. 효과음은 이벤트마다 `SpawnSoundAtLocation`/`Play()`로 재생(누적 방지). 음원: `/Game/Imported/Audio/brush`, `/Game/Imported/Audio/hammer`(임포트 완료). 선택: 버튼음·설문표시음·룸 앰비언스(`SOUND_REQUIREMENTS.md` 참조).

```
RHI 붓 sweep (위에서 본 손):     Threat 망치 4단계 (옆에서 본 손):
   ←──●──→   (1Hz, ±6cm)          42cm ┤ ╲LIFT      ╱RETURN
   [  손 책상  ]                        │  ╲       ╱
   붓털 끝이 손 위 1cm                   │   ╲STRIKE╱
                                   0cm ┤    ▼━━━━  ← IMPACT(머리=손, t=1.4 효과음)
                                        └────[ 손 ]────  Period 3s 반복
```

---

## 7. CSV 데이터 스키마

현 코드(`SurveyManager::SaveResults`) + 브리프 6장 목표를 함께 명시한다.

**현 구현 (코드 확인)**
- 파일: `<ProjectSavedDir>/SurveyResults_YYYYMMDD_HHMMSS.csv`, 인코딩 **UTF-8 BOM**(Excel 호환), 줄 구분 `\r\n`.
- 1행 헤더 = 측정항목, 2행 데이터 = 점수. 회차마다 **새 파일**로 누적.
- 측정항목(코드 `GetQuestions()`는 **9개** 정의 — 보고서 5장 8문항 + `신체소유감` 1개 추가):
  `신체소유감, 공간일치감, 시간일치감, 촉각왜곡, 자극위치판단, 시각우위성, 위화감, 현실감, 위협감`

```csv
신체소유감,공간일치감,시간일치감,촉각왜곡,자극위치판단,시각우위성,위화감,현실감,위협감
4,4,3,5,4,5,2,4,2
```
> SETUP/README의 예시는 보고서 5장 **8문항** 헤더를 보이나, 현 코드의 `GetQuestions()`는 맨 앞에 `신체소유감`을 더해 9개 측정항목을 출력한다(코드↔문서 불일치 항목, 양쪽 모두 기재).

**브리프 6장 목표 스키마 (절충형 — 확장 대상)**
조건별로 **조건 메타 + 8문항(또는 9측정항목) 점수 + 타임스탬프**를 한 행으로 기록:

| 조건 메타 | 설문 점수 | 시각 |
|---|---|---|
| `실험종류`, `LateralOffsetCm`, `LatencyMs`, `Matched` | 측정항목별 1~5 | `Timestamp` |

규칙(보고서 4.6 사고 방지):
- 설문 **미완료 시 중간값(3)** 자동 기록 후 진행.
- 이벤트 **즉시 flush** — 앱 중단에도 직전까지 보존.
- `결과 보기`(Results)는 최신 CSV의 헤더+마지막 행을 파싱해 "항목: 점수"로 패널 표시(`ShowResultsPanel`).

---

## 8. 좌표계 / 패시브 햅틱

브리프 8장(불변) + 코드(`WorldSetup.cpp`, `ExperimentManager.cpp`, `HandPawn.cpp`) 기준.

- **원점 = LocalFloor** — Z=0 = 사용자 실제 바닥. 메쉬는 바닥에 grounded.
- **가상=실제 책상 정합 필수** — 가상 책상/의자 치수를 실제와 일치시켜 손이 가상 책상을 뚫거나 허공에서 멈추지 않게 함(보고서 4.3).

| 요소 | 좌표 (cm) | 출처 |
|---|---|---|
| 책상 윗면 | **z = 73** | 브리프 8 / `WorldSetup::BuildDesk` |
| 책상 앞 가장자리 | **x = +30** (`DeskFrontEdgeX`) | 브리프 8 / 코드 |
| 책상 중심 X | x = 53.5 (front 30 + depth/2) | 코드 |
| 책상 치수 | 72(W,Y) × 47(D,X) × 73(H,Z) | `WorldSetup.h` 주석 / 코드 |
| 의자 좌석 | **z = 42** (`SeatTopZ`) | 브리프 8 / `WorldSetup::BuildChair` |
| 의자 등받이 top | z = 75 | 코드 |
| 메뉴 버튼 그리드 기준 | `ButtonRowBaseLoc=(52,0,100)` | `ExperimentManager.h` |
| 손목 트리거 반경 | 4.5 cm | `HandPawn.cpp` |

```
  옆에서 본 좌표계 (X=정면, Z=위):
  z=100 ┤  [메뉴/중단 버튼 그리드 z≈82~118]
  z=75  ┤  ┌의자 등받이 top
  z=73  ┤  ════════════ 책상 윗면(패시브 햅틱: 실제 책상 표면)
  z=42  ┤  ──── 의자 좌석(패시브 햅틱: 실제 의자)
  z=0   ┴──────────────────────────── 실제 바닥(LocalFloor 원점)
        x=0(사용자)        x=30(책상 앞)   x=53.5(책상 중심)
```

패시브 햅틱 원리: 사용자가 **실제로 만지는 것은 현실 책상/의자**(촉각), 보이는 것은 가상 책상/프롭(시각)이다. 두 좌표가 정합되어야 시각-촉각 불일치를 **의도한 만큼만** 만들 수 있다(실험의 핵심). 미세한 개인차(키·팔 길이)는 보고서 4.3에서 미해결로 명시 — 추후 보정 단계 추가 예정.

---

## 9. 빌드 · 배포 + 검증 한계

### 9.1 빌드/배포 (SETUP.md 요약)
- **첫 빌드**: `.uproject` 우클릭 → Generate VS project files → VS 2026에서 *Development Editor / Win64* 빌드 → `Content/Maps/Main` 빈 레벨 생성(GameMode가 액터 자동 스폰).
- **Quest 3 배포**: `BuildAndDeployQuest3.cmd`(JAVA_HOME/ANDROID_HOME/NDKROOT/UE_ROOT, `DEVICE=`를 본인 시리얼로) / APK만은 `BuildApkOnlyQuest3.cmd`.
- **PIE**: Quest Link + 런타임 켠 상태에서 *Play(VR Preview)*.
- **Meta XR Simulator**: 헤드셋 없이 PC 단독 검증 — 활성 OpenXR 런타임을 simulator로 등록 후 VR Preview. Synthetic Hands로 손목 위치만 움직여도 버튼(반경 4.5cm) 눌림.

### 9.2 검증 가능 / 불가능 (브리프 9장 · SETUP §2.4 — 정직하게)

**자동/PC 검증 가능**
- C++ 컴파일 그린, 에셋 임포트, Blender 기하/비율, 데이터 로깅 로직.
- 메뉴↔실험↔설문 FSM, 30cm offset 즉시 적용, 책상/의자 정합(z=73/42), 설문 8문항 진행 + CSV 저장(Simulator).

**실기기(Quest 3) 필수 — 시뮬/헤드리스 불안정**
- 헤드리스 SceneCapture / Meta XR Simulator는 이 PC(Parsec/TDR + UE5.7 OpenXR 버그)에서 불안정. PC Simulator는 초반 몇 프레임 뒤 D3D11 device-loss(~frame 19) 위험 → `-AutoShot`은 조기 종료 진단용.
- 실제 핸드트래킹 노이즈/끊김(`ConfidenceBehavior=None` 효과), **진짜 패시브 햅틱**(현실 책상 표면 동기화), `SetSimultaneousHandsAndControllersEnabled` 효과 확인.
- **시각 최종 검증 = Quest 3 실기기**(월요일 팀 실험).

### 9.3 알려진 한계 (SETUP §6)
- 버튼 충돌은 실제 손목 기준 → VC/Drift 중 보이는 손으로는 버튼이 안 눌림(의도된 분리).
- 가짜 손/붓 등 일부 placeholder는 `assets/` FBX 임포트 후 메쉬 슬롯에 할당.
- 결과 CSV는 회차마다 별도 파일 누적(한 파일 append는 `SurveyManager::SaveResults` 수정 필요).
- 한글이 □□□로 깨지면(드묾) NanumGothic 명시 지정 또는 영문 라벨 폴백(SETUP §8).

---

## 10. 실험실 에셋 결정 (`.omc/research/lab_asset_recommendation.md` 요약)

**결정(브리프 7장 / 연구문서 §3~4)**: **미니멀 절차적 룸을 기본**으로 유지하고, 발표/표면타당성용으로 저폴리 랩을 **선택 임포트**.

- **기본 = 미니멀 절차적 룸** — RHI/Visual-Capture에 과학적·기술적 최선. 이유: ① **방해요소 최소**(주의가 손에 집중되어 착각 강화), ② **Quest 모바일 성능 보장**(드로콜/텍스처 메모리 안정, 프레임 유지가 곧 실재감), ③ **임포트/라이선스/어트리뷰션 리스크 0**, ④ `AWorldSetup::BuildRoom()`이 이미 절차적 fallback(바닥+천장+4벽, 노출 잠금 조명)을 제공 → 가장 낮은 노력·리스크.
  - 주의(연구문서 §4): 표면타당성/보고서 optics상 빈 회색 박스는 미완성처럼 보일 수 있음 → 조명 품질(부드럽고 균일, 핫스팟 없음. 코드는 이미 노출 잠금)과 **순백 아님**의 미세 틴트 벽에 투자.
- **선택(발표/face validity용)** — **"Science Lab Lowpoly"(Helyx Silveira, Sketchfab, CC-BY 4.0, ~4.3k tris, glTF/FBX)**가 최우선 추천. 깔끔·저대비·소형으로 Quest forward shading에서 사실상 무비용. CC-BY는 **저자 크레딧 1줄** 필요(보고서 감사의 글/인앱 크레딧).
  - 턴키 대안: **Fab "Interior Laboratory Low Poly"**(UE-native, 임포트 리스크 0, 벽 리컬러 가능; 단 프롭 많아 RHI 방해 가능 → 정리 필요, Epic 계정·$0 확인). 백업: Low Poly Chemistry Lab(~15.9k tris), Office Room 15(단일 아틀라스). CC0 무어트리뷰션: OpenGameArt 인테리어 팩, Poly Haven 스튜디오 HDRI(조명만).
- **임포트 경로(연구문서 §5)**: 룸 메쉬 하드코딩 경로 `/Game/Imported/Environment/SM_StudyRoom.SM_StudyRoom`(`WorldSetup.cpp:24`). 동일 경로/이름으로 임포트하면 C++ 수정 불필요. 임포트 옵션: Static Mesh / **Combine Meshes ON** / Auto Collision **OFF** / Lightmap UV ON / Materials·Textures ON. 모바일 머티리얼은 단순 Default Lit·불투명·저인스트럭션 유지(SSR/SSAO/굴절/POM 금지), 텍스처 1k/512·ASTC, 정적/베이크 조명 권장.
- **하이브리드 권장(연구문서 §4 결론)**: 실제 RHI/Drift 시행은 절차적 룸 백드롭, richness가 필요한 메뉴/쇼케이스 상태에서만 임포트 랩 사용 — 둘은 상호배타적이지 않음.

---

### 부록: 코드 ↔ 사양 정합 상태 (2026-06-04 구현 반영, 빌드 그린)
1. **RHI 붓 주파수**: ✅ 해결 — `BrushStrokeHz=0.5`(왕복 2s)로 변경, 2초 효과음 1회/왕복.
2. **시간오차(Latency)**: ✅ 구현 — `AHandPawn` 월드좌표 손목 링버퍼(`WristHistory`, ~1.2s) + `SampleDelayedWrist`(선형보간) + `ApplyVisualState`. `AExperimentManager.LatencyMs`(EditAnywhere)를 VC/Drift 진입 시 `SetVisualLatencyMs`로 적용.
3. **조건별 자동 설문 + 조건 메타 CSV**: ✅ 구현 — `완료/설문` 버튼(구 `중단`)이 실험 종료 시 해당 조건 설문 자동 표시. `SurveyManager`가 `Timestamp,Condition,<9문항>`를 `Saved/SurveyResults_log.csv`에 **누적 append**, 미완료 응답은 중간값(3) 패딩, 즉시 flush.
4. **CSV 측정항목 수**: 코드 9개(보고서 8문항 앞에 `신체소유감` 추가) — 의도적 유지.
5. **Intro 스토리**: ✅ 구현 — `EnterStart`에서 신체소유감 유도 안내 패널 표시, [시작] 시 제거.
6. **일치/불일치 자극**: ✅ 구현 — `bBrushMismatch`/`BrushMismatchShiftCm`로 RHI 붓 스트로크 중심을 다른 손가락쪽으로 이동.
7. **사운드 per-event**: ✅ 구현 — 붓/망치 효과음을 프롭 부착 `UAudioComponent.Play()`로 이벤트마다 재생(2초 클립, 중첩 없음). `brush.wav`·`hammer.wav` → `/Game/Imported/Audio/`.
8. **붓 에셋 정합**: ✅ 해결 — 올바른 메쉬(`paint.blend`의 `PaintBrush3v2`)를 PCA 정렬·솔끝 +Z로 재임포트, `M_PaintBrushTex`(텍스처 4종) 연결.
9. **SimultaneousHandsAndControllers**: 보고서 설계 의도 ↔ 코드는 안정성 위해 호출 생략(메모리 #5: BeginPlay 호출 시 null Session AV). 컨트롤러 쥐면 손 사라짐을 허용 실패모드로 채택.
10. **실기기 시각 검증**: 헤드리스/시뮬 불안정 → 월요일 Quest 3 실험에서 최종 확인 필요(빌드는 그린).
