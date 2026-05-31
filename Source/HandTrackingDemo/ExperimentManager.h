#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ExperimentManager.generated.h"

class AHandPawn;
class AHTDButton;
class ASurveyManager;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EExperimentState : uint8
{
	Start         UMETA(DisplayName = "Start"),
	MainMenu      UMETA(DisplayName = "MainMenu"),
	Experiment    UMETA(DisplayName = "Experiment"),
	Survey        UMETA(DisplayName = "Survey")
};

UENUM(BlueprintType)
enum class EExperimentType : uint8
{
	None          UMETA(DisplayName = "None"),
	RHI           UMETA(DisplayName = "Rubber Hand Illusion"),
	VisualCapture UMETA(DisplayName = "Visual Capture"),
	Drift         UMETA(DisplayName = "Proprioceptive Drift")
};

/**
 * AExperimentManager — runtime FSM for the three cognitive-distortion experiments.
 *
 *   App start ─▶ Start state (single floating "시작" button)
 *                  │ press
 *                  ▼
 *               MainMenu (5 buttons on desk: RHI / VC / Drift / 설문 / 종료)
 *                  │ press experiment                    │ press 설문
 *                  ▼                                     ▼
 *               Experiment (offset + props + 중단 btn)   Survey (8 Likert items)
 *                  │ 중단                                │ 완료
 *                  └────────────▶ MainMenu ◀─────────────┘
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

	UPROPERTY(EditAnywhere, Category = "Layout") FVector ButtonRowBaseLoc   = FVector(95.0f, 0.0f, 125.0f);
	UPROPERTY(EditAnywhere, Category = "Layout") FVector StartButtonLoc     = FVector(95.0f, 0.0f, 135.0f);
	UPROPERTY(EditAnywhere, Category = "Layout") FVector StopButtonLoc      = FVector(80.0f, -45.0f, 130.0f);

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

	void TickRHI(float DeltaTime);
	void TickDrift(float DeltaTime);

	void DespawnProps();
	void SetButtonsVisible(bool bStart, bool bMenu, bool bStop);

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
	UPROPERTY() AHTDButton* BtnSurvey  = nullptr;
	UPROPERTY() AHTDButton* BtnExit    = nullptr;
	UPROPERTY() AHTDButton* BtnStop    = nullptr;

	// Per-experiment props (spawned on entry, destroyed on exit).
	UPROPERTY() AActor* RHIFakeHand   = nullptr;
	UPROPERTY() AActor* RHIBrush      = nullptr;
	UPROPERTY() AActor* VCTarget      = nullptr;
	UPROPERTY() AActor* DriftGhost    = nullptr;

	UPROPERTY() ASurveyManager* Survey = nullptr;
};
