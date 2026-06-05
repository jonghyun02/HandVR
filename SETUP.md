# HandTrackingDemo — Setup Guide

VR 인지왜곡 검증 실험 (Quest 3, Unreal 5.7, C++).
실험 3종: 고무손 착각(RHI) · 시각적 포착(Visual Capture) · 고유수용감각 표류(Proprioceptive Drift).

---

## 0. 이번 리빌드에서 바뀐 것

`HandVR_old_backup/` 에 이전 트리 통째로 보관됨. 새 프로젝트는 C++ 100% 코드 베이스로
- `AHandPawn`: VR 카메라 + 양손 OculusXR 핸드 트래킹 + 손목 트리거 + **시각용 손 offset**
- `AExperimentManager`: FSM (Start → MainMenu → Experiment/Survey → MainMenu)
- `AVRButton`: 3D 손 터치 버튼 (라벨 + 박스 트리거 + 1초 쿨다운)
- `ASurveyManager`: 보고서 5장 8문항 5점 리커트, CSV 자동 저장
- `AWorldSetup`: 절차적 방·책상·의자 (사이즈 정확히 박힘)

기존 `assets/` 폴더는 **그대로 보존**. 사용 방법은 §3.

---

## 1. 첫 빌드

### 1.1 Visual Studio 프로젝트 생성
1. `HandTrackingDemo.uproject` 우클릭 → **Generate Visual Studio project files**
2. 생성된 `HandTrackingDemo.sln` 더블클릭 → Visual Studio 2026 오픈
3. 솔루션 구성: **Development Editor / Win64** → **빌드**
4. 빌드 성공하면 `Binaries\Win64\UnrealEditor-HandTrackingDemo.dll` 생성됨

### 1.2 에디터 첫 실행
1. `HandTrackingDemo.uproject` 더블클릭 → 모듈 누락 다이얼로그가 뜨면 **Yes**로 재빌드
2. 에디터 진입하면 `Content/Maps/Main.umap`이 아직 없어서 빈 레벨로 시작될 거임

### 1.3 Main 맵 만들기
1. **File → New Level → Empty Level** 선택
2. **File → Save Current Level As...** → `/Game/Maps/Main`
3. 저장 후 자동으로 `DefaultEngine.ini`의 `GameDefaultMap=/Game/Maps/Main.Main`이 잡힘
4. **World Settings 패널**:
   - GameMode Override = `HandTrackingGameMode` (자동으로 잡혀 있을 거임)
   - 그 외 설정 건드릴 필요 없음
5. 레벨에는 아무 액터도 둘 필요 **없음**. GameMode가 알아서 다음을 스폰함:
   - `AWorldSetup` (방·책상·의자)
   - `AExperimentManager` (메뉴·실험 props·설문)
   - DefaultPawn = `AHandPawn` (자동)

---

## 2. Quest 3로 빌드/배포

### 2.1 한 번만 — Android SDK/NDK
`BuildAndDeployQuest3.cmd` 맨 위 경로 확인:
- `JAVA_HOME = C:\Program Files\Android\Android Studio\jbr`
- `ANDROID_HOME = %LOCALAPPDATA%\Android\Sdk`
- `NDKROOT = %ANDROID_HOME%\ndk\27.2.12479018`
- `UE_ROOT = C:\Program Files\Epic Games\UE_5.7`

NDK 버전 못 맞추면 `Edit → Project Settings → Android SDK`에서 직접 지정.

### 2.2 Quest 3 연결 후 빌드+배포
```cmd
HandVR> BuildAndDeployQuest3.cmd
```
`DEVICE=...` 줄을 본인 시리얼로 바꿔야 함 (`adb devices`로 확인).

APK만 (배포 X) 만들고 싶으면:
```cmd
HandVR> BuildApkOnlyQuest3.cmd
```

### 2.3 PIE (Quest Link 또는 데스크탑 미리보기)
Quest Link + SteamVR/Oculus 런타임 켠 상태에서 에디터 **Play (VR Preview)** 누르면 됨.

### 2.4 Meta XR Simulator로 테스트 (헤드셋 없이)

이 프로젝트는 OculusXRHMD + OpenXR 기반이라 **Meta XR Simulator**로 PC 단독 검증이 가능함. 실험 흐름(상태 머신·버튼·offset 적용)을 헤드셋 안 쓰고 빠르게 검증하는 데 추천.

**설치 — 한 번만**
1. https://developer.oculus.com/downloads/package/meta-xr-simulator/ 또는 Meta Quest Developer Hub(MQDH)에서 `Meta XR Simulator`를 받는다.
2. 설치하고 한 번 실행하면 `meta_openxr_simulator.exe`가 본인 PC의 OpenXR 런타임으로 자동 등록된다 (또는 Simulator 메뉴에서 "Set as active OpenXR runtime").
3. 활성 런타임 확인: 레지스트리 `HKLM\SOFTWARE\Khronos\OpenXR\1\ActiveRuntime`이 simulator의 `openxr_runtime_64.json`을 가리키는지 본다. (또는 MQDH UI에서 토글).

**Unreal에서 띄우기**
1. UE 에디터 열기 전에 Simulator를 먼저 켜둔다 — Simulator는 별도의 컨트롤 패널 창을 띄움.
2. 에디터 진입 → **Play 드롭다운** → **VR Preview** 선택.
3. UE가 OpenXR 런타임을 잡아서 Simulator 창에 출력을 보낸다. Simulator 창에 가상 Quest 3 화면 + 좌우 손 모델 + 키보드/마우스 컨트롤 패널이 표시됨.

**Simulator에서 손 입력 시뮬레이션**
- 기본 매핑: 좌측 컨트롤은 키보드, 우측 컨트롤은 마우스. Simulator GUI에 단축키 일람 있음.
- **Hand Tracking**: Simulator 메뉴 → `Hand Tracking → Enable Synthetic Hands`. 키보드 단축키로 손 포즈 토글(Pinch / Open Palm 등) 가능.
- 그냥 손목 위치만 움직여도 우리 프로젝트의 버튼은 눌림 (트리거 반경 4.5cm).
- `SetSimultaneousHandsAndControllersEnabled`는 Simulator에서도 호출되긴 하지만 진짜 멀티모달은 시뮬되지 않음 — 동작 확인은 실기기에서.

**Simulator로 검증 가능한 것 (헤드셋 없이도 OK)**
- 메뉴 ↔ 실험 ↔ 설문 FSM
- 30cm offset이 가상 손에 즉시 적용되는지
- 책상/의자 위치 (z=73 / z=42)와 사용자 시점 정합
- 설문 8문항 진행 + CSV 저장

**Simulator로는 검증 불가능 (실기기 필수)**
- 실제 핸드트래킹 노이즈/끊김 (`ConfidenceBehavior=None` 효과)
- 진짜 패시브 햅틱스 (현실 책상 표면과의 동기화)
- 컨트롤러 잡고 있는 동안 손이 같이 잡히는 동작 — `SetSimultaneousHandsAndControllersEnabled` 효과 확인은 Quest 3 실기기 필요

---

## 3. assets/ 폴더 활용 (선택)

기본 빌드는 절차적 큐브로 동작함 — Quest 3에 올려서 바로 체험 가능.
실제 모델로 교체하려면:

| 자리       | 추천 소스                                                            |
|------------|-----------------------------------------------------------------------|
| 방         | `assets/room/source/*.fbx`                                          |
| 책상       | `assets/table/elepheant_dining_table.fbx` (또는 직접 모델링)          |
| 의자       | `assets/chair/source/*.fbx`                                          |
| 가짜 손    | `assets/hand/source/GenericGauntlet_Low.obj` (원본 재다운로드 필요)   |
| 가상 붓    | `assets/brush/source/*.fbx`                                         |

임포트 절차:
1. 에디터 `Content Browser` → 새 폴더 `/Game/Imported` 생성
2. `assets/<폴더>/source/*.fbx`를 Content Browser로 드래그
3. 임포트 옵션: **Static Mesh / Auto Generate Collision OFF / Combine Meshes ON**
4. 임포트된 StaticMesh를 `BP_ExperimentManager` 또는 `AExperimentManager` (월드에 자동 스폰된 인스턴스)의 디테일 패널에서
   - `FakeHandMesh` = (임포트한 손 메쉬)
   - `BrushMesh`    = (임포트한 붓 메쉬)
   - `TargetMesh`   = (취향대로)

`AWorldSetup`은 코드로 큐브를 만들고 있어서, FBX로 교체하려면 액터를 일단 숨기고
**직접 임포트한 방·책상·의자 액터를 레벨에 배치**하는 게 빠름.
이때 책상 윗면 z=73cm, 의자 좌석 z=42cm를 맞춰야 패시브 햅틱 동기화가 깨지지 않음.

---

## 4. 실험별 동작 메모

| 실험                       | 즉시 효과 (버튼 누른 그 프레임에 발생)                                              |
|----------------------------|-------------------------------------------------------------------------------------|
| 1. 고무손 착각 (RHI)        | 추적 손은 그대로 보임. 그 위로 세로로 선 가상 붓이 1Hz로 왕복하며 손등을 쓰다듬음        |
| 2. 시각적 포착 (VC)         | 양손 시각 위치 = 실제 손목 + **(0, +30, 0) cm** (오른쪽으로 30cm 점프). 별도 타겟 없음   |
| 3. 고유수용감각 표류 (Drift)| VC와 동일 + Y offset = 30 + 8·sin(2π·0.4·t) cm 으로 흔들림. 별도 마커 없음               |

중단 버튼 (왼쪽 상부, 빨간색) 누르면 즉시 메뉴로 복귀.

값을 바꾸고 싶으면 `AExperimentManager`의 EditAnywhere 프로퍼티:
- `LateralOffsetCm` (기본 30)
- `DriftAmplitudeCm` (기본 8)
- `DriftHz` (기본 0.4)

---

## 5. 설문

메뉴에서 **설문** 누르면 보고서 5장 표 그대로 8문항 5점 리커트가 뜸.
응답 자동 진행. 완료 시 `Saved/SurveyResults_YYYYMMDD_HHMMSS.csv` 로 저장됨.

```csv
공간일치감,시간일치감,촉각왜곡,자극위치판단,시각우위성,위화감,현실감,위협감
4,3,5,4,5,2,4,2
```

---

## 6. 알려진 한계 / 추후 손볼 것

- 메뉴/중단 버튼 충돌은 **실제 손목 위치** 기준임. VC/Drift 실험 중에는 시각 손이 30cm 옆에 보이므로
  사용자가 "보이는 곳"으로 손을 가져가도 버튼이 안 눌림 → 사용자가 잠시 실험 환영을 깨고 실제 손을 더듬어야 함.
  의도된 동작 (실험 검증과 분리). 더 깔끔하게 하려면 Y/B 컨트롤러 버튼에 중단 액션을 매핑.
- 가짜 손/붓 모델은 큐브 placeholder. assets/ 폴더 FBX 임포트 후 `FakeHandMesh`, `BrushMesh`에 할당.
- 광원 1개 (책상 위 PointLight)만 있음 — 분위기는 §3에서 임포트한 룸 라이트로 교체 추천.
- 결과 csv 한 줄만 누적이라 여러 회차면 파일이 여러 개 쌓임. 한 파일에 append하려면 SurveyManager `SaveResults` 수정.

---

## 7. 트러블슈팅

| 증상                                                   | 해결                                                                                    |
|--------------------------------------------------------|------------------------------------------------------------------------------------------|
| 에디터에서 모듈 누락 (Missing modules)                  | `.uproject` 우클릭 → Generate VS project files → VS에서 Development Editor 빌드          |
| Android 빌드 실패 (NDK)                                | `BuildAndDeployQuest3.cmd`의 NDKROOT 경로 확인, 또는 Project Settings → Android SDK     |
| Quest 3에서 손이 안 보임                               | 헤드셋 설정 → 손 추적 ON, 앱 권한 → 손 추적 허용 (`AndroidManifest`에 이미 선언돼 있음)   |
| 가상 손 떨림 / 끊김                                    | `ConfidenceBehavior=None`은 이미 적용. Quest 3 OS 최신으로                              |
| 버튼이 안 눌림                                          | 실제 손목을 버튼 위치까지 가져가야 함. `LeftWristTrigger`/`RightWristTrigger` 반경 4.5cm |
| 버튼/설문 라벨이 □□□로 깨짐                            | UMG Slate 기본 폰트가 한글 fallback을 못 잡는 빌드면 §8 참고 (드문 케이스)              |
| **Meta XR Simulator 크래시** `check(MotionVectorIndex == MotionVectorDepthIndex)` (OpenXRHMD.cpp:2765) | 엔진 `UE_5.7/Engine/Config/ConsoleVariables.ini` 의 `[Startup]` 에 **`xr.OpenXRAcquireMode=1`** 추가 후 **에디터 완전 재시작**. §9 참고. (프로젝트 `DefaultEngine.ini`의 `[ConsoleVariables]`는 OpenXR HMD가 생성된 *뒤에* 적용돼 너무 늦음 → 시뮬레이터의 깨진 swapchain 인덱스를 그대로 신뢰하다 assert) |

---

## 8. 한글이 깨지면 (UMG fallback 실패 시 — 드물지만 대비책)

이 프로젝트는 `UWidgetComponent` + UMG `UTextBlock`을 통해 라벨을 그린다. Slate의 기본 Roboto 컴포지트 폰트는 CJK 글리프를 fallback typeface (보통 DroidSansFallback)로 떨어뜨려서 별도 작업 없이 한글이 출력된다.

만약 그래도 □□□로 보인다면 (커스텀 엔진 빌드, fallback 폰트 제거 등) 둘 중 하나:

**A. NanumGothic 한 번 임포트하고 코드에서 명시적으로 가리키기**
1. `NanumGothic.ttf` (OFL, 무료) 또는 `C:\Windows\Fonts\malgun.ttf` 을 Content Browser → `/Game/Fonts/` 로 드래그
2. Asset Type = **Font** / Font Cache Type = **Runtime** 으로 임포트 → Composite Font 에셋 한 개 생성됨
3. `LabelWidget.cpp` 의 `RebuildWidget()` 안에서:
   ```cpp
   static ConstructorHelpers::FObjectFinder<UFont> KFont(TEXT("/Game/Fonts/NanumGothic.NanumGothic"));
   if (KFont.Succeeded()) {
       FSlateFontInfo Font(KFont.Object, PendingSize);
       TextBlock->SetFont(Font);
   }
   ```

**B. 영어 라벨로 일괄 폴백**
`ExperimentManager.cpp` 의 한글 라벨을 `"Start"`, `"Exp1 RHI"`, `"Exp2 VC"`, `"Exp3 Drift"`, `"Survey"`, `"Exit"`, `"Stop"` 으로 바꾸고, `SurveyManager.cpp` 의 8문항도 영문 번역으로 교체.

---

## 9. Meta XR Simulator 크래시/불안정 — ★해법은 RHI를 D3D11로 (커맨드라인 검증 완료 2026-06-05)

PC에서 Meta XR Simulator로 VR Preview 시 **두 개의 별개 문제**가 있다. 둘을 헷갈리지 말 것.

### 9-A. OpenXRHMD.cpp:2765 크래시 — **D3D11로 해결됨**
**증상**: VR 진입 직후(프레임 ~10~390)
```
Assertion failed: MotionVectorIndex == MotionVectorDepthIndex  [OpenXRHMD.cpp] [Line: 2765]
```
**원인 (UE 5.7 코드 추적)**: 시뮬레이터가 `XR_FB_space_warp` 확장을 광고 → UE가 frame-synthesis용 motion-vector swapchain 2개를 **확장 유무만으로** 할당(`GetRecommendedMotionVectorTextureSize`). 시뮬레이터의 **D3D12** swapchain acquire가 두 swapchain 인덱스를 어긋나게 반환 → `check()` 실패.
**검증으로 밝혀진 것**:
- `xr.OpenXRFrameSynthesis=0` → swapchain 할당을 못 막음(확장 게이트라). **효과 없음.**
- `xr.OpenXRAcquireMode=1` → **이미 엔진 코드가 1로 박아둠**(런타임 `LastSetBy: Code` 확인). 그런데도 D3D12에선 매 런 크래시. **효과 없음.**
- **`-d3d11`(또는 DefaultGraphicsRHI_DX11) → 2765 사라짐.** D3D11(`XR_KHR_D3D11_enable`) 경로는 그 assert를 안 침. 커맨드라인 `UnrealEditor.exe <proj> /Game/Maps/Main -game -vr -d3d11` 로 프레임 정상 진행 + 2765 0건 확인. D3D12는 100% 크래시.

**해결 (적용됨, git에 들어감)**: `Config/DefaultEngine.ini`
```ini
[/Script/WindowsTargetPlatform.WindowsTargetSettings]
DefaultGraphicsRHI=DefaultGraphicsRHI_DX11
```
에디터가 이 Default RHI를 따르므로 **에디터 완전 종료 후 재시작**하면 VR Preview가 D3D11로 떠서 2765가 안 난다. (Quest3 Android 빌드는 Vulkan이라 무관.) 확인: 로그에 `Using Forced/Default RHI: D3D11` + `XR_KHR_D3D11_enable`.

### 9-B. DXGI_ERROR_DEVICE_REMOVED (D3D11Query.cpp:396) — **가상 디스플레이 어댑터 GPU TDR (환경 문제)**
9-A를 고쳐 D3D11로 프레임이 돌면, VR 버퍼(3360×1760) **첫 스테레오 present** 직후 `DXGI_ERROR_DEVICE_REMOVED`로 GPU가 죽을 수 있다. 이 PC엔 **Parsec Virtual Display Adapter + Meta Virtual Monitor + 실제 NVIDIA** 3개 디스플레이 어댑터가 있고, VR present가 가상 디스플레이 경로로 새서 GPU device-removed가 난다. **코드/설정 문제 아님**(메모리 #7-B 동일 머신).

★ **함정: Parsec 앱(`parsecd`)을 꺼도 "Parsec Virtual Display Adapter"는 장치로 그대로 남는다** (확인: parsecd 0개인데 어댑터 잔존 + 여전히 크래시). 앱 종료 ≠ 어댑터 제거.

**해결 (효과 큰 순):**
1. **장치 관리자 → 디스플레이 어댑터 → "Parsec Virtual Display Adapter" 우클릭 → 디바이스 사용 안 함**(Disable). (필요시 "Meta Virtual Monitor"도. Parsec 다시 쓸 때 재활성화.) 가상 디스플레이가 빠지면 present가 실제 NVIDIA로만 가서 TDR이 사라진다.
2. **Quest 3 실기기**(`BuildAndDeployQuest3.cmd`) — 헤드셋이 자기 디스플레이라 PC present 경로 자체를 회피. 가장 확실.
3. (소프트 완화) `[ConsoleVariables] vr.PixelDensity=0.5` 로 VR 해상도를 낮춰 present 부하를 줄이면 TDR 확률이 떨어진다(근본 해결 아님). 저부하(640×480) -game 런은 95초+ 생존 확인.

> 정리: 시뮬레이터로 PC 검증할 거면 **DefaultGraphicsRHI_DX11 + Parsec 끄고 직결**. 안정적 최종 검증은 **Quest 3 실기기**.
