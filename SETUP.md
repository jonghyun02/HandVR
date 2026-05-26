# HandTrackingDemo - 실행 가이드

UE 5.7.4 / Quest 3 / Meta XR Simulator.

RHI(Rubber Hand Illusion) 실험 시스템이 C++ `ARubberHandPawn` + `UExperimentManagerComponent`에 내장되어 있어 BP 셋업 없이 바로 PIE 누르면 됩니다.

## 1. 빌드

1. 프로젝트 루트에서 `HandTrackingDemo.uproject` 우클릭 → "Generate Visual Studio project files"
2. `HandTrackingDemo.sln` 열고 Win64 / Development Editor 빌드 (Ctrl+B)
3. 에디터 자동 실행

## 2. Meta XR Simulator 활성화 (PIE 핸드 시뮬용)

1. Edit → Plugins → "Meta XR Simulator" 검색 → Enable
2. 에디터 재시작
3. Toolbar의 PIE 옆 드롭다운에서 **Meta XR Simulator** 선택

## 3. 실행

1. PIE 재생 ▶
2. Simulator 창에서 양손 핸드트래킹 모드 켜기
3. `RubberHandGameMode`가 `ARubberHandPawn`을 자동 스폰 → `UExperimentManagerComponent`가 `Content/Experiment/Cases.json` 로드 → 8케이스 자동 진행
4. 각 케이스 종료 시 `URHISurveyWidget` Likert 1~7 설문 표시

## 4. 출력 확인

성공 로그:
```
[RHI] RubberHandPawn BeginPlay — offset=... delay=...ms
```

CSV 로깅은 `Saved/` 이하에 케이스별로 저장 (`ExperimentManagerComponent` 참조).

## 5. 트러블슈팅

- **PIE에서 손이 안 보임**: Meta XR Simulator 플러그인 활성화 + Simulator 모드로 PIE 시작했는지 확인
- **빌드 에러 OculusXRHandTracking 모듈 없음**: `Build.cs`의 `PublicDependencyModuleNames`에 `"OculusXRHandTracking"` 추가
