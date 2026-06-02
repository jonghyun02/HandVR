#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ExperimentManager.generated.h"

class AHandPawn;
class AHTDButton;
class ASurveyManager;
class UStaticMeshComponent;
class UWidgetComponent;
class USoundBase;

UENUM(BlueprintType)
enum class EExperimentState : uint8
{
	Start         UMETA(DisplayName = "Start"),
	MainMenu      UMETA(DisplayName = "MainMenu"),
	Experiment    UMETA(DisplayName = "Experiment"),
	Survey        UMETA(DisplayName = "Survey"),
	Results       UMETA(DisplayName = "Results")
};

UENUM(BlueprintType)
enum class EExperimentType : uint8
{
	None          UMETA(DisplayName = "None"),
	RHI           UMETA(DisplayName = "Rubber Hand Illusion"),
	VisualCapture UMETA(DisplayName = "Visual Capture"),
	Drift         UMETA(DisplayName = "Proprioceptive Drift"),
	Threat        UMETA(DisplayName = "Hammer Threat")
};

/**
 * AExperimentManager — runtime FSM for the three cognitive-distortion experiments.
 *
 *   App start ─▶ Start state (single floating "시작" button)
 *                  │ press
 *                  ▼
 *               MainMenu (4×2 그리드: 실험1~4 / 설문 / 결과 보기 / 종료)
 *                  │ press experiment       │ press 설문        │ press 결과 보기
 *                  ▼                        ▼                  ▼
 *               Experiment (offset + props) Survey (Likert)    Results (최근 CSV 패널 + 닫기)
 *                  │ 중단                    │ 완료              │ 닫기
 *                  └────────────────▶ MainMenu ◀────────────────┘
 *
 * Per the spec: all visual effects are INSTANT — no fade / ramp. Offsets snap to ±30 cm the moment
 * the experiment button is pressed, and props appear on the same tick. Stopping snaps everything back.
 */
UCLASS()
class HANDTRACKINGDEMO_API AExperimentManager : public AActor
{
	GENERATED_BODY()

public:
	AExperimentManager();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// Allow the user to drop in their imported FBX meshes after they import assets/ (see SETUP.md).
	UPROPERTY(EditAnywhere, Category = "Props") UStaticMesh* FakeHandMesh   = nullptr;
	UPROPERTY(EditAnywhere, Category = "Props") UStaticMesh* BrushMesh      = nullptr;
	UPROPERTY(EditAnywhere, Category = "Props") UStaticMesh* TargetMesh     = nullptr;

	// Within a seated user's arm reach: ~45-55 cm in front (desk front edge is at x=30, top z=73), ~chest height.
	UPROPERTY(EditAnywhere, Category = "Layout") FVector ButtonRowBaseLoc   = FVector(52.0f, 0.0f, 100.0f);
	UPROPERTY(EditAnywhere, Category = "Layout") FVector StartButtonLoc     = FVector(46.0f, 0.0f, 106.0f);
	UPROPERTY(EditAnywhere, Category = "Layout") FVector StopButtonLoc      = FVector(46.0f, -34.0f, 100.0f);

	/** The lateral (Y-axis) jump applied to the visual hand during VC/Drift. Positive = user's right. */
	UPROPERTY(EditAnywhere, Category = "Experiments") float LateralOffsetCm = 30.0f;

	/** Drift only — peak-to-peak oscillation added on top of LateralOffsetCm. */
	UPROPERTY(EditAnywhere, Category = "Experiments") float DriftAmplitudeCm = 8.0f;
	UPROPERTY(EditAnywhere, Category = "Experiments") float DriftHz          = 0.4f;

protected:
	UFUNCTION() void OnButtonPressed(int32 ButtonId);
	UFUNCTION() void OnSurveyFinished();

private:
	void EnterStart();
	void EnterMainMenu();
	void EnterExperiment(EExperimentType Type);
	void EnterSurvey();
	void EnterResults();

	// 결과 보기 패널을 띄우고/지운다. ShowResultsPanel은 최근 CSV를 파싱해 "항목: 점수" 형식으로 출력.
	void ShowResultsPanel();
	void DespawnResultsPanel();

	// 실험 설명 패널 — 실험 진입 시 사용자 앞 위쪽에 "이 실험이 무엇인지"를 띄운다. 실험 종료/전환 시 제거.
	void ShowExperimentInfo(const FString& Title, const FString& Body);
	void DespawnExperimentInfo();
	// 실험 종류별 설명 텍스트(제목/본문)를 채운다.
	static void GetExperimentInfo(EExperimentType Type, FString& OutTitle, FString& OutBody);
	// <ProjectSavedDir>의 SurveyResults_*.csv 중 가장 최근(이름=타임스탬프 기준) 파일 경로. 없으면 빈 문자열.
	FString FindLatestSurveyCsv() const;

	void TickRHI(float DeltaTime);
	void TickDrift(float DeltaTime);
	void TickThreat(float DeltaTime);

	void DespawnProps();
	void SetButtonsVisible(bool bStart, bool bMenu, bool bStop, bool bClose);

	AHTDButton* SpawnButton(int32 ButtonId, const FString& Label, FVector Loc, FLinearColor Tint);

	AHandPawn* GetHandPawn() const;

	// --- State -----------------------------------------------------------------------------------------------------
	EExperimentState State           = EExperimentState::Start;
	EExperimentType  CurrentType     = EExperimentType::None;
	float            ExperimentTime  = 0.0f;

	// Persistent UI buttons (spawned once, shown/hidden as state changes).
	UPROPERTY() AHTDButton* BtnStart   = nullptr;
	UPROPERTY() AHTDButton* BtnRHI     = nullptr;
	UPROPERTY() AHTDButton* BtnVC      = nullptr;
	UPROPERTY() AHTDButton* BtnDrift   = nullptr;
	UPROPERTY() AHTDButton* BtnThreat  = nullptr;
	UPROPERTY() AHTDButton* BtnSurvey  = nullptr;
	UPROPERTY() AHTDButton* BtnResults = nullptr;
	UPROPERTY() AHTDButton* BtnExit    = nullptr;
	UPROPERTY() AHTDButton* BtnStop    = nullptr;
	UPROPERTY() AHTDButton* BtnClose   = nullptr; // 결과 패널 닫기 → MainMenu 복귀

	// Per-experiment props (spawned on entry, destroyed on exit).
	UPROPERTY() AActor* RHIFakeHand   = nullptr; // also the threatened fake hand in the Threat experiment
	UPROPERTY() AActor* RHIBrush      = nullptr;
	UPROPERTY() AActor* VCTarget      = nullptr;
	UPROPERTY() AActor* DriftGhost    = nullptr;
	UPROPERTY() AActor* ThreatHammer  = nullptr;

	// Hammer impact SFX (runtime-loaded from /Game/Imported/Audio/hammer); PrevThreatT edge-detects the
	// strike->impact crossing each cycle so the sound fires exactly once per hit.
	UPROPERTY() USoundBase* HammerSound = nullptr;
	float PrevThreatT = 0.0f;

	// Brush-stroke SFX (runtime-loaded from /Game/Imported/Audio/brush); PrevBrushOff edge-detects each sweep
	// across the hand so the sound fires once per stroke, not every frame.
	UPROPERTY() USoundBase* BrushSound = nullptr;
	float PrevBrushOff = 0.0f;

	UPROPERTY() ASurveyManager* Survey = nullptr;

	// 결과 보기 — 월드 공간 텍스트 패널(액터 + UWidgetComponent×2). Results 상태에서만 존재.
	UPROPERTY() AActor*           ResultsPanel      = nullptr;
	UPROPERTY() UWidgetComponent* ResultsTitleWidget = nullptr;
	UPROPERTY() UWidgetComponent* ResultsBodyWidget  = nullptr;

	// 실험 설명 — 월드 공간 텍스트 패널(액터 + UWidgetComponent×2). Experiment 상태에서만 존재.
	UPROPERTY() AActor*           InfoPanel       = nullptr;
	UPROPERTY() UWidgetComponent* InfoTitleWidget = nullptr;
	UPROPERTY() UWidgetComponent* InfoBodyWidget  = nullptr;
};
