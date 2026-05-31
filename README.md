# HandTrackingDemo

VR 인지왜곡 검증 실험 (Quest 3 · Unreal 5.7 · C++).

3개 실험:
1. **고무손 착각** (Rubber Hand Illusion) — 실제 추적 손 숨기고 책상 위 가짜 손에 가상 붓 자극
2. **시각적 포착** (Visual Capture) — 가상 손이 실제 손에서 30cm 측면으로 점프
3. **고유수용감각 표류** (Proprioceptive Drift) — VC + 8cm/0.4Hz 사인 흔들림 + 실제 손 위치 마커

UI: 시작 버튼 → 실험 메뉴(3 실험 + 설문 + 종료) → 실험/설문 → 중단 → 메뉴 복귀.
설문: 보고서 5장 8문항 5점 리커트, 결과 `Saved/SurveyResults_*.csv` 로 저장.

빌드/배포는 [SETUP.md](./SETUP.md) 참고.

```
HandVR/
├── assets/                            # 기존 보존 (FBX 임포트 소스)
├── Plugins/OculusXR/                  # Meta XR SDK
├── Source/HandTrackingDemo/           # 게임 모듈 (C++)
│   ├── HandTrackingDemo.{h,cpp,Build.cs}
│   ├── HandTrackingGameMode.{h,cpp}
│   ├── HandPawn.{h,cpp}               # VR pawn + 손 추적 + 시각 offset
│   ├── ExperimentManager.{h,cpp}      # 상태 머신
│   ├── VRButton.{h,cpp}               # 3D 손 터치 버튼
│   ├── SurveyManager.{h,cpp}          # 8문항 5점 리커트
│   └── WorldSetup.{h,cpp}             # 절차적 방·책상·의자
├── Config/                            # DefaultEngine/Game/Input.ini
├── HandTrackingDemo.uproject
├── BuildAndDeployQuest3.cmd
└── BuildApkOnlyQuest3.cmd
```

이전 트리 통째 백업: `C:/projects/VR/HandVR_old_backup/`
