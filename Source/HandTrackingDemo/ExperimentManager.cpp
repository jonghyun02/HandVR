#include "ExperimentManager.h"

#include "HandPawn.h"
#include "VRButton.h"
#include "SurveyManager.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UObject/ConstructorHelpers.h"

// Button IDs
namespace
{
	constexpr int32 BID_Start  = 1;
	constexpr int32 BID_RHI    = 2;
	constexpr int32 BID_VC     = 3;
	constexpr int32 BID_Drift  = 4;
	constexpr int32 BID_Survey = 5;
	constexpr int32 BID_Exit   = 6;
	constexpr int32 BID_Stop   = 7;
}

AExperimentManager::AExperimentManager()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> FakeHandFinder(TEXT("/Game/Imported/Props/SM_GauntletHand.SM_GauntletHand"));
	if (FakeHandFinder.Succeeded()) FakeHandMesh = FakeHandFinder.Object;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BrushFinder(TEXT("/Game/Imported/Props/SM_PaintBrush.SM_PaintBrush"));
	if (BrushFinder.Succeeded()) BrushMesh = BrushFinder.Object;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> TargetFinder(TEXT("/Game/Imported/Props/SM_ClawHammer.SM_ClawHammer"));
	if (TargetFinder.Succeeded()) TargetMesh = TargetFinder.Object;
}

void AExperimentManager::BeginPlay()
{
	Super::BeginPlay();

	// Spawn the seven UI buttons once. Hidden/shown by state transitions.
	BtnStart  = SpawnButton(BID_Start,  TEXT("시작"),                StartButtonLoc,                                  FLinearColor(0.2f, 0.8f, 0.3f));
	BtnRHI    = SpawnButton(BID_RHI,    TEXT("실험1\n고무손 착각"),     ButtonRowBaseLoc + FVector(0.0f, -36.0f, 0.0f), FLinearColor(0.3f, 0.5f, 0.9f));
	BtnVC     = SpawnButton(BID_VC,     TEXT("실험2\n시각적 포착"),     ButtonRowBaseLoc + FVector(0.0f, -18.0f, 0.0f), FLinearColor(0.3f, 0.5f, 0.9f));
	BtnDrift  = SpawnButton(BID_Drift,  TEXT("실험3\n고유수용감각 표류"), ButtonRowBaseLoc + FVector(0.0f,   0.0f, 0.0f), FLinearColor(0.3f, 0.5f, 0.9f));
	BtnSurvey = SpawnButton(BID_Survey, TEXT("설문"),                ButtonRowBaseLoc + FVector(0.0f, +18.0f, 0.0f), FLinearColor(0.8f, 0.7f, 0.2f));
	BtnExit   = SpawnButton(BID_Exit,   TEXT("종료"),                ButtonRowBaseLoc + FVector(0.0f, +36.0f, 0.0f), FLinearColor(0.7f, 0.3f, 0.3f));
	BtnStop   = SpawnButton(BID_Stop,   TEXT("중단"),                StopButtonLoc,                                   FLinearColor(0.9f, 0.2f, 0.2f));

	EnterStart();
}

AHTDButton* AExperimentManager::SpawnButton(int32 ButtonId, const FString& Label, FVector Loc, FLinearColor Tint)
{
	UWorld* World = GetWorld();
	if (!World) return nullptr;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AHTDButton* Btn = World->SpawnActor<AHTDButton>(AHTDButton::StaticClass(), FTransform(Loc), Params);
	if (Btn)
	{
		Btn->Configure(Label, ButtonId, Tint);
		Btn->OnPressed.AddDynamic(this, &AExperimentManager::OnButtonPressed);
	}
	return Btn;
}

AHandPawn* AExperimentManager::GetHandPawn() const
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	return PC ? Cast<AHandPawn>(PC->GetPawn()) : nullptr;
}

void AExperimentManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (State != EExperimentState::Experiment) return;

	ExperimentTime += DeltaTime;
	switch (CurrentType)
	{
		case EExperimentType::RHI:   TickRHI(DeltaTime);   break;
		case EExperimentType::Drift: TickDrift(DeltaTime); break;
		default: break;
	}
}

// --- State transitions -------------------------------------------------------------------------------------------

void AExperimentManager::SetButtonsVisible(bool bStart, bool bMenu, bool bStop)
{
	if (BtnStart)  BtnStart->SetEnabledState(bStart);
	if (BtnRHI)    BtnRHI->SetEnabledState(bMenu);
	if (BtnVC)     BtnVC->SetEnabledState(bMenu);
	if (BtnDrift)  BtnDrift->SetEnabledState(bMenu);
	if (BtnSurvey) BtnSurvey->SetEnabledState(bMenu);
	if (BtnExit)   BtnExit->SetEnabledState(bMenu);
	if (BtnStop)   BtnStop->SetEnabledState(bStop);
}

void AExperimentManager::EnterStart()
{
	State       = EExperimentState::Start;
	CurrentType = EExperimentType::None;
	DespawnProps();
	if (AHandPawn* P = GetHandPawn()) { P->ResetVisualOffsets(); P->SetHandsVisible(true); }
	SetButtonsVisible(/*start*/ true, /*menu*/ false, /*stop*/ false);
}

void AExperimentManager::EnterMainMenu()
{
	State       = EExperimentState::MainMenu;
	CurrentType = EExperimentType::None;
	DespawnProps();
	if (AHandPawn* P = GetHandPawn()) { P->ResetVisualOffsets(); P->SetHandsVisible(true); }
	SetButtonsVisible(false, true, false);
}

void AExperimentManager::EnterExperiment(EExperimentType Type)
{
	State          = EExperimentState::Experiment;
	CurrentType    = Type;
	ExperimentTime = 0.0f;
	DespawnProps();
	SetButtonsVisible(false, false, true);

	AHandPawn* P = GetHandPawn();
	UWorld*    W = GetWorld();
	if (!P || !W) return;

	const float DeskHeight   = 73.0f;
	const float DeskFrontX   = 30.0f;
	const float DeskCenterX  = DeskFrontX + 23.5f;
	const FVector DeskTopCtr = FVector(DeskCenterX, 0.0f, DeskHeight);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	auto MakeMesh = [&](FName Name, UStaticMesh* MeshOrNull, FVector Loc, FRotator Rot, FVector ScaleCm, FLinearColor /*Tint*/) -> AStaticMeshActor*
	{
		AStaticMeshActor* A = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(Rot, Loc), Params);
		if (!A) return nullptr;
#if WITH_EDITOR
		// Outliner label — editor-only API, omitted in Android/Shipping builds.
		A->SetActorLabel(Name.ToString());
#endif
		A->SetMobility(EComponentMobility::Movable);
		if (UStaticMeshComponent* SMC = A->GetStaticMeshComponent())
		{
			// Fallback to engine basic cube if no FBX assigned. Cube is 100×100×100 → scale = size_cm / 100.
			UStaticMesh* Mesh = MeshOrNull;
			if (!Mesh)
			{
				static UStaticMesh* CubeFallback = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
				Mesh = CubeFallback;
			}
			SMC->SetStaticMesh(Mesh);
			SMC->SetWorldScale3D(ScaleCm / 100.0f);
			SMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		return A;
	};

	switch (Type)
	{
		case EExperimentType::RHI:
		{
			// Hide the real-tracked virtual hands — only the static fake hand is visible.
			P->SetHandsVisible(false);

			// Static fake "rubber hand" on desk center, palm-up. Cube placeholder ~ 20 (long) × 9 (wide) × 3 (thick) cm.
			RHIFakeHand = MakeMesh(TEXT("RHI_FakeHand"),
				FakeHandMesh,
				DeskTopCtr + FVector(0.0f, 0.0f, 1.5f),
				FRotator(0.0f, 90.0f, 0.0f),
				FakeHandMesh ? FVector(12.0f, 12.0f, 12.0f) : FVector(20.0f, 9.0f, 3.0f),
				FLinearColor(0.95f, 0.85f, 0.75f));

			// Virtual brush — small cylinder-shaped object via cube fallback. We tick its position to stroke.
			RHIBrush = MakeMesh(TEXT("RHI_Brush"),
				BrushMesh,
				DeskTopCtr + FVector(0.0f, 0.0f, 8.0f),
				FRotator(0.0f, 0.0f, 90.0f),
				BrushMesh ? FVector(8.0f, 8.0f, 8.0f) : FVector(2.0f, 18.0f, 2.0f),
				FLinearColor(0.6f, 0.4f, 0.2f));
			break;
		}

		case EExperimentType::VisualCapture:
		{
			// Snap visual hands +30 cm laterally — instant, no fade. Real wrist position unchanged.
			P->SetVisualOffset(FVector(0.0f, LateralOffsetCm, 0.0f), FVector(0.0f, LateralOffsetCm, 0.0f));

			// Target object on desk — user is supposed to try to touch it with their seen hand.
			VCTarget = MakeMesh(TEXT("VC_Target"),
				TargetMesh,
				DeskTopCtr + FVector(0.0f, 0.0f, 5.0f),
				FRotator(0.0f, 90.0f, 0.0f),
				TargetMesh ? FVector(8.0f, 8.0f, 8.0f) : FVector(6.0f, 6.0f, 6.0f),
				FLinearColor(1.0f, 0.3f, 0.3f));
			break;
		}

		case EExperimentType::Drift:
		{
			P->SetVisualOffset(FVector(0.0f, LateralOffsetCm, 0.0f), FVector(0.0f, LateralOffsetCm, 0.0f));

			// Ghost cube at the REAL hand position — tick updates its transform to track real wrist.
			DriftGhost = MakeMesh(TEXT("Drift_Ghost"),
				nullptr,
				DeskTopCtr,
				FRotator::ZeroRotator,
				FVector(4.0f, 4.0f, 4.0f),
				FLinearColor(0.3f, 0.7f, 1.0f));
			break;
		}

		default: break;
	}
}

void AExperimentManager::EnterSurvey()
{
	State = EExperimentState::Survey;
	DespawnProps();
	if (AHandPawn* P = GetHandPawn()) { P->ResetVisualOffsets(); P->SetHandsVisible(true); }
	SetButtonsVisible(false, false, false);

	UWorld* W = GetWorld();
	if (!W) return;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Survey = W->SpawnActor<ASurveyManager>(ASurveyManager::StaticClass(), FTransform(ButtonRowBaseLoc + FVector(0.0f, 0.0f, 30.0f)), Params);
	if (Survey)
	{
		Survey->OnFinished.AddDynamic(this, &AExperimentManager::OnSurveyFinished);
		Survey->BeginSurvey();
	}
}

// --- Per-experiment tick -----------------------------------------------------------------------------------------

void AExperimentManager::TickRHI(float /*DeltaTime*/)
{
	if (!RHIBrush || !RHIFakeHand) return;

	// Stroke the fake hand from finger-tip to wrist along its long axis (X). 1 Hz back-and-forth.
	const FVector Center  = RHIFakeHand->GetActorLocation();
	const float   AmplX   = 8.0f; // cm — stroke length
	const float   Hz      = 1.0f;
	const float   X       = Center.X + FMath::Sin(ExperimentTime * Hz * 2.0f * PI) * AmplX;
	RHIBrush->SetActorLocation(FVector(X, Center.Y, Center.Z + 5.0f));
}

void AExperimentManager::TickDrift(float /*DeltaTime*/)
{
	AHandPawn* P = GetHandPawn();
	if (!P) return;

	// Drift Y offset oscillates around LateralOffsetCm to make the position-mismatch more salient.
	const float OffsetY = LateralOffsetCm + FMath::Sin(ExperimentTime * DriftHz * 2.0f * PI) * DriftAmplitudeCm;
	P->SetVisualOffset(FVector(0.0f, OffsetY, 0.0f), FVector(0.0f, OffsetY, 0.0f));

	// Ghost marker pinned to the user's REAL right wrist position (where proprioception says the hand is).
	if (DriftGhost)
	{
		const FVector RealRight = P->GetRealWristLocation(/*right*/ true);
		DriftGhost->SetActorLocation(RealRight);
	}
}

// --- Events ------------------------------------------------------------------------------------------------------

void AExperimentManager::OnButtonPressed(int32 ButtonId)
{
	switch (ButtonId)
	{
		case BID_Start:  if (State == EExperimentState::Start)       EnterMainMenu();                          break;
		case BID_RHI:    if (State == EExperimentState::MainMenu)    EnterExperiment(EExperimentType::RHI);    break;
		case BID_VC:     if (State == EExperimentState::MainMenu)    EnterExperiment(EExperimentType::VisualCapture); break;
		case BID_Drift:  if (State == EExperimentState::MainMenu)    EnterExperiment(EExperimentType::Drift);  break;
		case BID_Survey: if (State == EExperimentState::MainMenu)    EnterSurvey();                            break;
		case BID_Exit:   if (State == EExperimentState::MainMenu)    UKismetSystemLibrary::QuitGame(GetWorld(), nullptr, EQuitPreference::Quit, false); break;
		case BID_Stop:   if (State == EExperimentState::Experiment)  EnterMainMenu();                          break;
		default: break;
	}
}

void AExperimentManager::OnSurveyFinished()
{
	if (Survey) { Survey->Destroy(); Survey = nullptr; }
	EnterMainMenu();
}

void AExperimentManager::DespawnProps()
{
	auto Kill = [](AActor*& A) { if (A) { A->Destroy(); A = nullptr; } };
	Kill(RHIFakeHand);
	Kill(RHIBrush);
	Kill(VCTarget);
	Kill(DriftGhost);
}
