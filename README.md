# RubberHandVR — 가상현실 팀 프로젝트 (1팀)

> Konkuk University · 가상현실 팀 프로젝트 · 2026-1
> Meta Quest 3 기반 **고무손 착각(Rubber Hand Illusion, RHI) 실험 플랫폼**.

---

## 1. 프로젝트 목표

VR 환경에서 현실 소품(촉각)과 가상 오브젝트(시각)를 의도적으로 불일치하게 설계하여
사용자의 감각 왜곡 과정을 실험적으로 탐구합니다. 정밀한 핸드 트래킹과 물리 법칙 기반의
렌더링을 통해 **시각 우위성이 촉각 인지에 미치는 영향**을 측정하는 것이 핵심 목표입니다.

### 1.1 세부 목표
- Meta Quest 3 기반 1인 체험형 VR 실험 환경 구축 (실험실, 책상, 의자, 조명)
- OpenXR 핸드 트래킹으로 컨트롤러 없는 자연스러운 손 인터랙션
- 패시브 햅틱스 + 물리 법칙 기반 렌더링을 통한 신체화(embodiment) 유도
- 몰입감 / 실재감 변화 측정 및 감각왜곡 발생 여부 정량 확인

### 1.2 핵심 가설
> 시각 정보는 촉각 정보보다 우위에 있으며, 충분히 정교한 VR 환경에서 사용자는
> 실제 자극과 불일치하는 감각왜곡 현상을 경험할 수 있다.

### 1.3 이론적 배경
| 개념 | 핵심 |
|---|---|
| **Rubber Hand Illusion** (Botvinick & Cohen, 1998) | 시·촉각 자극이 동기화되면 가짜 손도 자기 손처럼 느낌 |
| **Visual Capture** | 감각 충돌 시 시각 정보가 우선됨 |
| **Proprioceptive Drift** | 시각 영향으로 자기 신체 위치가 표류하는 현상 |

---

## 2. 팀 구성

| 학번 | 이름 |
|---|---|
| 202012345 | 김희용 |
| 202111346 | 이종현 |
| 202212375 | 정윤아 |
| 202111334 | 이권수 |

---

## 3. 시스템 구성

| 구분 | 기술 / 도구 | 역할 |
|---|---|---|
| 엔진 | Unreal Engine 5.7.4 | VR 환경 및 렌더링 전체 |
| 플러그인 | OculusXR Plugin 201.0 | Quest 3 핸드 트래킹 / HMD 연동 |
| 표준 | OpenXR (`XR_EXT_hand_tracking`) | 손 관절 추적 데이터 수신 |
| 빌드 | Visual Studio 2026 / Android SDK 34 (NDK 27.2.12479018) / JDK 21 | PC 에디터 빌드 및 Quest APK 빌드 |
| 버전관리 | Git / GitHub | 소스코드 및 씬 에셋 공유 |

### 모듈 의존 (`HandTrackingDemo.Build.cs`)
```cs
PublicDependencyModuleNames = {
    "Core","CoreUObject","Engine","InputCore",
    "HeadMountedDisplay","EnhancedInput",
    "XRBase","OculusXRInput","OculusXRHMD",
    "UMG","NavigationSystem","AIModule"
};
PrivateDependencyModuleNames = { "Slate","SlateCore","Json","JsonUtilities" };
```

---

## 4. 폴더 / 파일 구성

### 4.1 폴더
| 폴더 | 역할 |
|---|---|
| `Config/` | 엔진 설정 파일 모음 |
| `Source/HandTrackingDemo/` | C++ 소스코드 |
| `Content/` | 언리얼 엔진 내 에셋 / 실험 데이터 |
| `Plugins/OculusXR/` | Meta Quest 핸드 트래킹 외부 플러그인 (엔진 동봉, 저장소 미포함) |

### 4.2 Config
| 파일 | 역할 |
|---|---|
| `DefaultEngine.ini` | 렌더링 설정, OpenXR 확장 등록, Quest 3 하드웨어 설정 |
| `DefaultGame.ini` | VR 모드로 실행 |
| `DefaultInput.ini` | 손동작(잡기) 관련 설정 |

### 4.3 Source — RHI 실험 메인
| 파일 | 역할 |
|---|---|
| `HandTrackingDemo.h/.cpp` | 게임 모듈 진입점 |
| `HandTrackingDemo.Build.cs` | 빌드 의존 모듈 (OculusXR Plugin 등) 선언 |
| `HandTrackingDemo.Target.cs` / `HandTrackingDemoEditor.Target.cs` | 게임/에디터 빌드 타겟 |
| `RubberHandTypes.h` | RHI Enum / Struct (`ERHIStimulusType`, `FRHICaseSpec`, `FRHISurveyResponse`, `FRHIBoneSnapshot`) |
| `RubberHandPawn.h/.cpp` | RHI VR Pawn — Tracker / Synth 이중 손 구조 + ring buffer |
| `RubberHandGameMode.h/.cpp` | DefaultPawn = `ARubberHandPawn`, Lab 자동 스폰 |
| `ExperimentManagerComponent.h/.cpp` | 8케이스 자동 진행, JSON 로드, CSV 로깅 |
| `RHISurveyWidget.h/.cpp` | 케이스 종료 시 표시되는 Likert 1~7 설문 위젯 |
| `LabEnvironmentActor.h/.cpp` | 절차적 실험실 — 룸 / 테이블 / 의자 / 조명 / 9버튼 컨트롤 패널 |
| `BrushActor.h/.cpp` | 동기/비동기 터치 자극 (Brush Sync / Async) |
| `HammerActor.h/.cpp` | 비동기 위협 자극 (4단계: WindUp → Strike → Impact → Recovery) |

### 4.4 Source — 룬 드로잉 (부가 / legacy)
| 파일 | 역할 |
|---|---|
| `HandPawn.h/.cpp`, `HandGameMode.h/.cpp` | 룬 드로잉용 별도 Pawn / GameMode |
| `RuneTypes.h`, `RuneDrawingComponent.*`, `RuneRecognizer.*`, `RuneSpellComponent.*` | 핀치 기반 도형 인식 + 6종 마법 디스패치 |
| `RuneRecognizerEvalCommandlet.*` | 자동 정확도 평가 commandlet |

> 룬 시스템은 본 보고서의 RHI 실험과 무관한 베이스 프로젝트 산출물입니다.
> 동일 솔루션 안에 있지만 `RubberHandGameMode`만 사용하면 RHI 실험 흐름만 작동합니다.

### 4.5 Content / 실험 데이터
| 파일 | 역할 |
|---|---|
| `Content/Experiment/Cases.json` | RHI 8케이스 정의 (보고서 Phase 1 + Phase 2) |
| `Content/Eval/RuneEvalSet.json` | 룬 도형 평가셋 (legacy) |

---

## 5. 빌드 / 실행

### 5.1 사전 요구사항
| 도구 | 버전 |
|---|---|
| Unreal Engine | 5.7.4 |
| Microsoft OpenJDK | 21 |
| Android Studio | Koala (Android SDK 34, NDK 27.2.12479018) |
| Quest 3 | OS 60+ (USB 디버깅 허용) |

`UE_ROOT/Engine/Extras/Android/SetupAndroid.bat` 실행으로 SDK/NDK/JDK 자동 셋업하는 것이 가장 깔끔.

### 5.2 PC 에디터 빌드
1. `HandTrackingDemo.uproject` 우클릭 → "Generate Visual Studio project files"
2. `HandTrackingDemo.sln` → Win64 / Development Editor 빌드 (`Ctrl+B`)
3. 에디터 자동 실행

### 5.3 Quest 3 APK 빌드 + 배포
프로젝트 루트의 원클릭 스크립트 사용 (`JAVA_HOME` / `ANDROID_HOME` / `DEVICE`는 본인 환경에 맞게 수정):

- `BuildAndDeployQuest3.cmd` — 컴파일 → cook → APK 패키징 → adb install
- `BuildApkOnlyQuest3.cmd` — APK 패키징까지만 (배포 제외)

```bat
set "JAVA_HOME=C:\Program Files\Microsoft\jdk-21.0.10.7-hotspot"
set "ANDROID_HOME=C:\Users\YOU\AppData\Local\Android\Sdk"
set "NDKROOT=%ANDROID_HOME%\ndk\27.2.12479018"
set "DEVICE=YOUR_QUEST_SERIAL"   REM `adb devices`로 확인
```

내부에서 UAT가 `BuildCookRun -platform=Android -cookflavor=ASTC ...`로 호출됩니다. 핫 캐시 기준 3~4분.

### 5.4 헤드셋이 자꾸 sleep으로 들어갈 때
```bat
adb shell svc power stayon usb
adb shell am broadcast -a com.oculus.vrpowermanager.automation_disable
adb shell am broadcast -a com.oculus.vrpowermanager.prox_close
```

---

## 6. 핵심 구현

### 6.1 핸드 트래킹 계층 — `MotionController` ⊃ `OculusXRHandComponent`
컨트롤러를 잡고 있어도 손 관절이 끊기지 않도록 `MotionController`(부모)에 `OculusXRHandComponent`(자식)를 부착하는 계층 구조를 사용합니다.
이 구조는 기기 런타임에서 송신되는 **손목 포즈 데이터**를 부모 노드의 기준 좌표로 삼고,
그 위에 OpenXR이 제공하는 **손가락 관절 데이터**를 결합합니다. 이렇게 하면 손목과 손가락의
좌표 불일치가 없고, 컨트롤러를 들고 있을 때도 손 메시가 자연스럽게 따라옵니다.

```cpp
// HandPawn / RubberHandPawn 공통 골격
LeftMC = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("LeftMC"));
LeftMC->SetupAttachment(SceneRoot);
LeftMC->SetTrackingMotionSource(FName(TEXT("Left")));

LeftHand = CreateDefaultSubobject<UOculusXRHandComponent>(TEXT("LeftHand"));
LeftHand->SetupAttachment(LeftMC);                 // ← 컨트롤러 자식
LeftHand->SkeletonType        = EOculusXRHandType::HandLeft;
LeftHand->MeshType            = EOculusXRHandType::HandLeft;
LeftHand->ConfidenceBehavior  = EOculusXRConfidenceBehavior::None; // 신뢰도 낮아도 메시 유지
```

### 6.2 동시 입력 (Hands + Controllers)
일반 VR에서는 컨트롤러를 잡으면 핸드 트래킹이 강제 종료되지만, Quest 3부터 지원되는 멀티모달
기능(`XR_META_simultaneous_hands_and_controllers`)을 활성화해 두 입력을 동시에 받습니다.
컨트롤러를 쥔 상태에서도 손가락 관절 추적이 살아 있어 **신체소유감의 단절을 방지**합니다.

```cpp
void AHandPawn::BeginPlay() {
    Super::BeginPlay();
    UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(EHMDTrackingOrigin::LocalFloor);
    UOculusXRInputFunctionLibrary::SetSimultaneousHandsAndControllersEnabled(true);
}
```

### 6.3 가짜 손 합성 — `ARubberHandPawn`의 RHI 메커니즘
RHI Pawn은 **두 개의 손 구조**를 가집니다:

```
ARubberHandPawn
├─ VRCamera (HMD)
├─ LeftMC, RightMC                 — MotionController
│   └─ Left/RightHandTracker       — UOculusXRHandComponent (visible=false, 본 데이터 소스)
└─ Left/RightHandSynth             — UPoseableMeshComponent (실제로 보이는 "가상 손")
   └─ ExperimentManager            — UExperimentManagerComponent
```

매 틱 동작:
1. `RecordTrackerSnapshot(Tracker, Buffer, NowSec)` — Tracker의 본 트랜스폼을 시각 ring buffer에 push (최대 1초 분량)
2. `ApplyDelayedSnapshotToSynth(Buffer, Synth, NowSec)` — `NowSec - TemporalDelayMs/1000` 시점의 본을 꺼내 Synth에 복사
3. **공간 오프셋**: HMD forward 방향으로 `SpatialOffsetCm`만큼 가상 손 전체 평행이동

→ 이로써 Tracker는 실제 손 위치를 그대로 보유, Synth는 시간/공간이 어긋난 "가짜 손"으로 분리됩니다.
실험 케이스가 바뀔 때 매니저가 `SetSpatialOffsetCm`/`SetTemporalDelayMs`만 조정하면
즉시 모든 케이스 조건을 재현할 수 있습니다.

### 6.4 실험 자동화 — `UExperimentManagerComponent`
```
StartExperiment()
  → BeginCase(0)             : Pawn에 offset/delay 적용 + 자극 액터 스폰
  → DurationSec 후 EndCase() : 자극 종료 + 설문 위젯 표시
  → 설문 응답 수신 후         : 다음 케이스 BeginCase(i+1)
  → 마지막 케이스 종료        : OnExperimentCompleted 브로드캐스트 + CSV flush
```
- **케이스 데이터**: `Content/Experiment/Cases.json`이 있으면 로드, 없으면 `InitDefaultCases()` 하드코딩 8케이스 사용
- **단일 케이스 실행**: `SelectCase(N)` — Lab의 9버튼 컨트롤 패널(1~8=Case, 9=Stop)에서 손가락 poke로 호출 가능
- **CSV 로깅**: `LogEvent(name, detail)` — 모든 케이스 시작/종료/설문 응답이 타임스탬프와 함께 기록

### 6.5 자극 액터
| 액터 | 자극 | 동작 |
|---|---|---|
| `ABrushActor` (Sync) | 동기 터치 | 가상 손 표면에서 ±6cm, 1Hz로 좌우 왕복. 시각-촉각 50ms 이내 매칭 가정 |
| `ABrushActor` (Async) | 비동기 터치 | 위치만 동일, 의도적으로 **잘못된 손가락 부위**를 쓸어줌 (Phase 2 Case 5/6) |
| `AHammerActor` | 위협 자극 | Idle → WindUp(0.5s) → Strike(0.15s) → Impact(충격음+빨간 플래시) → Recovery(1s). 시각·청각만으로 위협 반응 유도 (Phase 2 Case 7/8) |

### 6.6 절차적 실험실 — `ALabEnvironmentActor`
`OnConstruction`에서 박스 메시를 동적으로 부착하여 룸/책상/의자/조명/컨트롤 패널을 즉석 구성.
- 룸: 4m × 4m × 2.6m (기본값)
- 테이블: 120 × 60 × 75cm — `RealTableSizeCm`로 노출되어 **현실 책상과 1:1 매핑**(패시브 햅틱)
- 컨트롤 패널: 1~8(Case 시작) + 9(Stop), `HitTestButton(WorldPos)`로 손끝 위치 hit-test

---

## 7. 실험 프로토콜

### 7.1 Phase 1 — 신체소유감 활성 임계값(Threshold) 도출
| 케이스 | 공간오차 | 시간오차 | 자극 | 실험목적 |
|---|---|---|---|---|
| Case 1 | 0 cm | 0 ms | BrushSync | 최상의 동기화 상태 (대조군) |
| Case 2 | 15 cm | 0 ms | BrushSync | 신체소유감이 사라지는 오차거리 측정 |
| Case 3 | 0 cm | 300 ms | BrushSync | 신체소유감이 사라지는 오차시간 측정 |
| Case 4 | 15 cm | 300 ms | BrushSync | 시·공간 오차 공존 시 임계값 측정 |

### 7.2 Phase 2 — 신체소유감 활성화 상태에서의 촉각왜곡 검증
| 케이스 | 신체소유감 | 촉각자극 | 예상결과 |
|---|---|---|---|
| Case 5 | 활성 | BrushAsync (불일치) | 시각/표류된 고유수용감각이 촉각을 왜곡 |
| Case 6 | 비활성 | BrushAsync (불일치) | 촉각왜곡 없음 |
| Case 7 | 활성 | HammerThreat (시각만) | 시각 자극만으로 위협 반응 유도 |
| Case 8 | 비활성 | HammerThreat (시각만) | 위협 반응 없음 |

(상세 파라미터: `Content/Experiment/Cases.json`)

### 7.3 설문 (Likert 1~7)
| 측정항목 | 질문 | 분석용도 |
|---|---|---|
| **신체소유감** | 가상 손이 실제 내 손처럼 느껴졌는가? | 신체소유감 임계값 판별 |
| **촉각왜곡** | 실제가 아닌 시각자극 부위에서 촉각이 느껴졌는가? | 활성 상태에서의 왜곡 검증 |
| **시각 위계** (Case 7) | 실제 몸의 느낌보다 눈에 보이는 정보를 더 신뢰했는가? | 시각자극의 위계 확인 |

설문 응답은 `URHISurveyWidget`을 통해 수집되어 `UExperimentManagerComponent::OnSurveySubmitted`로 전달, CSV에 기록됩니다.

---

## 8. 진행 현황

### 8.1 완료
- **개발환경**: UE 5.7.4 + OculusXR Plugin 201.0 환경 빌드 / Git 협업 환경
- **VR 환경**: 실험실 씬 (룸·책상·의자·조명) 절차적 구현 (`ALabEnvironmentActor`)
- **핸드 트래킹**: `OculusXRHMD` + `OculusXRInput` 모듈 의존성 추가, MC ⊃ OculusXRHand 계층 구현
- **멀티모달 입력**: `SetSimultaneousHandsAndControllersEnabled(true)` 적용
- **트래킹 강건화**: `ConfidenceBehavior::None`으로 신뢰도 저하 시에도 메시 유지
- **RHI 핵심 메커니즘**: ring buffer 기반 시간지연 + HMD forward 공간오프셋 (`ARubberHandPawn`)
- **실험 자동화**: 8케이스 자동 진행, JSON 로드, CSV 로깅 (`UExperimentManagerComponent`)
- **자극 시스템**: Brush Sync/Async, Hammer Threat 4단계 시퀀스
- **설문 시스템**: 3항목 Likert 1~7 위젯 (`URHISurveyWidget`)

### 8.2 미완료
- 현실 ↔ 가상 좌표 정합 (캘리브레이션 절차)
- 사운드 시스템 구현 (현재 `USoundBase` 슬롯만 있음)
- 실험 도구 모델링 / 텍스쳐 (현재는 `Engine/BasicShapes/Cube` 박스 메시)

---

## 9. 역할 분담

| 팀원 | 역할 |
|---|---|
| 김희용 |  |
| 이종현 |  |
| 정윤아 |  |
| 이권수 |  |

---

## 10. 트러블슈팅

| 증상 | 원인 / 해결 |
|---|---|
| 손 메시가 안 보임 | Quest 설정 → 손과 컨트롤러 → 핸드트래킹 ON / OculusXR 플러그인 활성화 확인 |
| 본 매칭 실패 (`HandRef=<none>`) | 플러그인 버전마다 본 이름 다름. Output Log 본 이름 dump 확인 후 후보 리스트에 추가 |
| 컨트롤러 잡으면 손 트래킹 끊김 | `SetSimultaneousHandsAndControllersEnabled(true)` 호출 누락 |
| Synth 손이 갑자기 사라짐 | `ConfidenceBehavior`가 `None`이 아닐 때 발생. RHI Pawn에선 강제 None |
| 설문 위젯이 안 뜸 | `ExperimentManager.SurveyWidgetClass`에 BP 클래스 미할당. `BindWidgetOptional`이라 BP 없어도 키 입력 fallback 가능 |
| 시스템 메뉴가 자꾸 뜸 | 엄지+검지 시스템 핀치 제스처. (룬 시스템 한정) 가운데 손가락 핀치 사용 |

---

## 11. 부록 — 룬 드로잉 시스템 (legacy)

본 저장소에는 RHI 실험과 별개로 핀치 기반 도형 인식 + 6종 마법 디스패치 시스템이 함께 들어있습니다.
GameMode를 `AHandGameMode`로 바꾸면 룬 시스템이 동작합니다.

| 도형 | 마법 | 디버그 비주얼 |
|---|---|---|
| 원 | 푸른 방어막 5초 | Sphere |
| 직선 | 광선 | Line |
| 삼각형 | 룬 서클 + 소환 | Circle + Sphere |
| V자 | 분할 공격 | Line × 2 |
| 지그재그 | 연쇄 번개 | Line × 4-5 |
| 별 | 폭발 발사체 | Line + Sphere |

평가 commandlet:
```bat
"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
  HandTrackingDemo.uproject -run=RuneRecognizerEval -log
```
→ `Saved/Logs/HandTrackingDemo.log`에 confusion matrix + macro F1 출력.
