#include "ExperimentManager.h"

#include "HandPawn.h"
#include "VRButton.h"
#include "SurveyManager.h"
#include "VRLabelWidget.h"

#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Sound/SoundBase.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/ConstructorHelpers.h"

// Button IDs
namespace
{
	constexpr int32 BID_Start  = 1;
	constexpr int32 BID_RHI    = 2;
	constexpr int32 BID_VC     = 3;
	constexpr int32 BID_Drift  = 4;
	constexpr int32 BID_Survey  = 5;
	constexpr int32 BID_Exit    = 6;
	constexpr int32 BID_Stop    = 7;
	constexpr int32 BID_Threat  = 8;
	constexpr int32 BID_Results = 9;
	constexpr int32 BID_Close   = 10;
	constexpr int32 BID_Condition = 11;

	// In-headset condition presets (보고서 5장 변인을 빌드 재컴파일 없이 순환). 메뉴의 [조건] 버튼이 순환하고,
	// 다음 실험이 활성 조건의 공간오차/시간오차/일치여부를 사용한다. 진행자가 헤드셋을 벗지 않고 조건 전환.
	struct FCondPreset { const TCHAR* Name; float OffsetCm; float LatencyMs; bool bMatched; };
	const FCondPreset kConditions[] = {
		{ TEXT("C1 동기화"),     0.0f,   0.0f,   true  }, // 최상의 동기화(기준)
		{ TEXT("C2 공간5"),      5.0f,   0.0f,   true  }, // 공간오차 가변
		{ TEXT("C2 공간15"),     15.0f,  0.0f,   true  },
		{ TEXT("C2 공간30"),     30.0f,  0.0f,   true  },
		{ TEXT("C3 시간100"),    0.0f,   100.0f, true  }, // 시간오차 가변
		{ TEXT("C3 시간300"),    0.0f,   300.0f, true  },
		{ TEXT("C3 시간500"),    0.0f,   500.0f, true  },
		{ TEXT("C4 공존"),       30.0f,  300.0f, true  }, // 시/공간 오차 공존
		{ TEXT("C5 불일치"),     30.0f,  0.0f,   false }, // 촉각 불일치(다른 손가락)
	};

	// Uniform fit-scale applied to an imported mesh so its LONGEST world dimension matches TargetBoxCm's largest
	// component (shape preserved). Pure function of mesh bounds + target, so spawn and tick agree on the scale.
	float ComputeFitScale(UStaticMesh* MeshOrNull, FVector TargetBoxCm)
	{
		if (!MeshOrNull) return 1.0f;
		const FVector Full = MeshOrNull->GetBounds().BoxExtent * 2.0f;
		const float Longest = FMath::Max3(Full.X, Full.Y, Full.Z);
		const float Target  = FMath::Max3(TargetBoxCm.X, TargetBoxCm.Y, TargetBoxCm.Z);
		return Longest > KINDA_SMALL_NUMBER ? Target / Longest : 1.0f;
	}

	// WORLD-space offset from the actor origin to the working part of MeshOrNull, given the uniform fit Scale and
	// the actor Rot. Working part = the +end (bPositiveEnd) or -end of the mesh's LONGEST local axis:
	// localOffset = Origin ± Extent along that axis. Caller does actorLoc = handLoc - thisOffset to land it.
	FVector WorkingPartWorldOffset(UStaticMesh* MeshOrNull, float Scale, const FRotator& Rot, bool bPositiveEnd)
	{
		if (!MeshOrNull) return FVector::ZeroVector;

		const FBoxSphereBounds B = MeshOrNull->GetBounds();
		const FVector Origin = B.Origin;
		const FVector Extent = B.BoxExtent;
		const float Sign = bPositiveEnd ? 1.0f : -1.0f;

		// Identify the longest local axis and build the end point along it (other axes stay at the bounds centre).
		FVector LocalEnd = Origin;
		if (Extent.X >= Extent.Y && Extent.X >= Extent.Z)      LocalEnd.X = Origin.X + Sign * Extent.X;
		else if (Extent.Y >= Extent.X && Extent.Y >= Extent.Z) LocalEnd.Y = Origin.Y + Sign * Extent.Y;
		else                                                    LocalEnd.Z = Origin.Z + Sign * Extent.Z;

		return Rot.RotateVector(LocalEnd * Scale);
	}

	// Fit boxes (cm) used both at spawn and in tick so the working-part offset stays consistent.
	// Brush native bounds ≈ 6.1 × 4.2 × 21.1 cm (longest LOCAL axis = Z). Hammer ≈ 14.8 × 34.1 × 4.5 cm (longest = Y).
	const FVector kBrushFitBox(24.0f, 24.0f, 24.0f);  // brush rendered ~24 cm long (real paint brush scale)
	const FVector kHammerFitBox(33.0f, 33.0f, 33.0f); // hammer rendered ~33 cm long
	const FRotator kBrushRot(0.0f, 0.0f, 90.0f);  // long axis swept roughly horizontal across the hand

	// Which END of each mesh's LONGEST local axis is the WORKING part that must land on the hand. The bristle /
	// hammer-head end cannot be inferred from bounds (symmetric) — these are set from a direct render of the mesh
	// (see .omc/render_props). Flip a bool if the wrong end lands on the hand in-headset.
	const bool kBrushBristleAtPositiveEnd = true;  // brush: bristle tip = +Z end?  (verified via render)
	const bool kHammerHeadAtPositiveEnd   = true;  // hammer: head = +Y end?        (verified via render)

	// Threat-hammer rest orientation (user-tuned): yaw 90 = rotated 90° counter-clockwise (top-down) from the
	// old 180; roll 90 = rotated 90° about the handle's long axis so the FLAT STRIKING FACE lands on the hand.
	// If the face points the wrong way in-headset flip kHammerStrikeRoll's sign (±90); if the heading is off
	// adjust kHammerBaseYaw by ±90.
	const float kHammerBaseYaw    = 90.0f;
	const float kHammerStrikeRoll = 90.0f;
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

	// 메뉴 버튼은 4열 × 2행 그리드로 배치한다. 라벨 평면 폭이 약 20 cm (400px × 0.05) 이므로
	// 열 간격을 32 cm 로 잡으면 인접 라벨 사이 여백이 12 cm 이상 확보돼 한글이 절대 겹치지 않는다.
	// 행 간격은 30 cm (±15) — 라벨 높이 약 10 cm 대비 충분. 모두 x≈52, z≈90~120 cm 로 착석 손 닿는 범위.
	// Wider column pitch (40 cm: ±60 / ±20) and SHORT single-concept labels so the Korean never collides with
	// the neighbour (the long multi-line labels were overlapping). Rows 36 cm apart (±18).
	const float ColX[4] = { -60.0f, -20.0f, +20.0f, +60.0f }; // 열별 Y 오프셋(좌→우)
	const float RowTopZ    = +18.0f; // 윗줄
	const float RowBottomZ = -18.0f; // 아랫줄
	auto Cell = [&](int32 Col, float RowZ) { return ButtonRowBaseLoc + FVector(0.0f, ColX[Col], RowZ); };

	const FLinearColor ExpColor(0.3f, 0.5f, 0.9f);

	// Spawn the nine UI buttons once. Hidden/shown by state transitions. Short labels avoid overlap.
	BtnStart   = SpawnButton(BID_Start,   TEXT("시작"),         StartButtonLoc,      FLinearColor(0.2f, 0.8f, 0.3f));
	// 윗줄: 실험 1~4 (짧은 라벨)
	BtnRHI     = SpawnButton(BID_RHI,     TEXT("실험1\n고무손"),  Cell(0, RowTopZ),    ExpColor);
	BtnVC      = SpawnButton(BID_VC,      TEXT("실험2\n시각포착"), Cell(1, RowTopZ),    ExpColor);
	BtnDrift   = SpawnButton(BID_Drift,   TEXT("실험3\n표류"),    Cell(2, RowTopZ),    ExpColor);
	BtnThreat  = SpawnButton(BID_Threat,  TEXT("실험4\n망치"),    Cell(3, RowTopZ),    FLinearColor(0.6f, 0.35f, 0.85f));
	// 아랫줄: 설문 / 결과 / 종료
	BtnSurvey  = SpawnButton(BID_Survey,  TEXT("설문"),         Cell(0, RowBottomZ), FLinearColor(0.8f, 0.7f, 0.2f));
	BtnResults = SpawnButton(BID_Results, TEXT("결과"),         Cell(1, RowBottomZ), FLinearColor(0.2f, 0.7f, 0.7f));
	BtnExit    = SpawnButton(BID_Exit,    TEXT("종료"),         Cell(2, RowBottomZ), FLinearColor(0.7f, 0.3f, 0.3f));
	// 조건 순환 버튼(하단 우측) — 헤드셋 안에서 8조건 프리셋 전환(보고서 4.4).
	BtnCondition = SpawnButton(BID_Condition, TEXT("조건"),    Cell(3, RowBottomZ), FLinearColor(0.55f, 0.45f, 0.8f));
	// 실험 중단 / 결과 패널 닫기
	BtnStop    = SpawnButton(BID_Stop,    TEXT("완료\n설문"),             StopButtonLoc,            FLinearColor(0.9f, 0.2f, 0.2f));
	BtnClose   = SpawnButton(BID_Close,   TEXT("닫기"),                  StopButtonLoc,            FLinearColor(0.9f, 0.5f, 0.2f));

	ApplyActiveCondition(); // 초기 조건(C1) 적용 + 조건 버튼 라벨 설정
	EnterStart();

	UE_LOG(LogTemp, Display, TEXT("[HandVR] ExperimentManager ready: 10 buttons spawned, state=Start"));
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
		case EExperimentType::RHI:    TickRHI(DeltaTime);    break;
		case EExperimentType::Drift:  TickDrift(DeltaTime);  break;
		case EExperimentType::Threat: TickThreat(DeltaTime); break;
		default: break;
	}
}

// --- State transitions -------------------------------------------------------------------------------------------

void AExperimentManager::SetButtonsVisible(bool bStart, bool bMenu, bool bStop, bool bClose)
{
	if (BtnStart)   BtnStart->SetEnabledState(bStart);
	if (BtnRHI)     BtnRHI->SetEnabledState(bMenu);
	if (BtnVC)      BtnVC->SetEnabledState(bMenu);
	if (BtnDrift)   BtnDrift->SetEnabledState(bMenu);
	if (BtnThreat)  BtnThreat->SetEnabledState(bMenu);
	if (BtnSurvey)  BtnSurvey->SetEnabledState(bMenu);
	if (BtnResults) BtnResults->SetEnabledState(bMenu);
	if (BtnExit)    BtnExit->SetEnabledState(bMenu);
	if (BtnCondition) BtnCondition->SetEnabledState(bMenu);
	if (BtnStop)    BtnStop->SetEnabledState(bStop);
	if (BtnClose)   BtnClose->SetEnabledState(bClose);
}

void AExperimentManager::EnterStart()
{
	State       = EExperimentState::Start;
	CurrentType = EExperimentType::None;
	DespawnProps();
	DespawnResultsPanel();
	if (AHandPawn* P = GetHandPawn()) { P->ResetVisualOffsets(); P->SetHandsVisible(true); }
	SetButtonsVisible(/*start*/ true, /*menu*/ false, /*stop*/ false, /*close*/ false);

	// 맨 처음 스토리(인트로) — 신체소유감 유도 사전 안내. [시작]을 누르면 EnterMainMenu가 DespawnProps로 제거.
	ShowExperimentInfo(
		TEXT("지각(知覺) 실험실에 오신 것을 환영합니다"),
		TEXT("의자에 편히 앉아 양손을 책상 위에 자연스럽게 올려 주세요.\n\n"
		     "잠시 후 당신의 손에서 '보이는 것'과 '느껴지는 것'이\n"
		     "서로 어긋날 수 있습니다. 옳고 그름을 판단하지 말고\n"
		     "그저 무엇이 느껴지는지 가만히 관찰해 주세요.\n\n"
		     "준비가 되면 아래 [시작] 버튼에 손을 가져가 주세요."));
}

void AExperimentManager::EnterMainMenu()
{
	State       = EExperimentState::MainMenu;
	CurrentType = EExperimentType::None;
	DespawnProps();
	DespawnResultsPanel();
	if (AHandPawn* P = GetHandPawn()) { P->ResetVisualOffsets(); P->SetHandsVisible(true); }
	SetButtonsVisible(false, true, false, false);
}

void AExperimentManager::EnterExperiment(EExperimentType Type)
{
	State          = EExperimentState::Experiment;
	CurrentType    = Type;
	ExperimentTime = 0.0f;
	DespawnProps();
	SetButtonsVisible(false, false, true, false);

	AHandPawn* P = GetHandPawn();
	UWorld*    W = GetWorld();
	if (!P || !W) return;

	// Clean visual state each entry; VC/Drift set spatial offset + temporal latency below.
	P->ResetVisualOffsets();
	BrushCycle = -1;

	// Record this run's condition (실험종류·공간오차·시간오차·일치여부) — logged with the survey row.
	const TCHAR* TName = TEXT("None");
	switch (Type)
	{
		case EExperimentType::RHI:           TName = TEXT("RHI");    break;
		case EExperimentType::VisualCapture: TName = TEXT("VC");     break;
		case EExperimentType::Drift:         TName = TEXT("Drift");  break;
		case EExperimentType::Threat:        TName = TEXT("Threat"); break;
		default: break;
	}
	CurrentConditionTag = FString::Printf(TEXT("%s|offset=%.0fcm|latency=%.0fms|stim=%s"),
		TName, LateralOffsetCm, LatencyMs, bBrushMismatch ? TEXT("mismatch") : TEXT("match"));

	const float DeskHeight   = 73.0f;
	const float DeskFrontX   = 30.0f;
	const float DeskCenterX  = DeskFrontX + 23.5f;
	const FVector DeskTopCtr = FVector(DeskCenterX, 0.0f, DeskHeight);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// TargetBoxCm = desired world bounding box in cm. Imported meshes are uniform-fitted to the largest
	// target dimension (shape preserved); the engine-cube fallback is fitted per-axis (cube is 100³).
	// This reads each mesh's actual bounds, so props render at real-world size regardless of FBX import units.
	// Tint is wired to a dynamic BasicShapeMaterial "Color" so props never render as the default grey checker.
	auto MakeMesh = [&](FName Name, UStaticMesh* MeshOrNull, FVector Loc, FRotator Rot, FVector TargetBoxCm, FLinearColor Tint, bool bForceTint = false) -> AStaticMeshActor*
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
			UStaticMesh* Mesh = MeshOrNull;
			if (!Mesh)
			{
				static UStaticMesh* CubeFallback = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
				Mesh = CubeFallback;
			}
			SMC->SetStaticMesh(Mesh);

			const FVector Full = Mesh ? Mesh->GetBounds().BoxExtent * 2.0f : FVector(100.0f);
			FVector Scale = FVector::OneVector;
			if (MeshOrNull)
			{
				Scale = FVector(ComputeFitScale(MeshOrNull, TargetBoxCm));
			}
			else
			{
				Scale = FVector(
					Full.X > KINDA_SMALL_NUMBER ? TargetBoxCm.X / Full.X : 1.0f,
					Full.Y > KINDA_SMALL_NUMBER ? TargetBoxCm.Y / Full.Y : 1.0f,
					Full.Z > KINDA_SMALL_NUMBER ? TargetBoxCm.Z / Full.Z : 1.0f);
			}
			SMC->SetWorldScale3D(Scale);
			SMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);

			// Tint the cube fallback (null mesh) AND any basic engine shape passed with bForceTint (e.g. the red
			// VC target sphere). Imported meshes (brush/hammer) keep their own imported material+textures
			// (e.g. SM_PaintBrush -> PaintBrush3v2): overriding slot 0 for those would hide the texture.
			if (!MeshOrNull || bForceTint)
			{
				static UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
				if (BaseMat)
				{
					if (UMaterialInstanceDynamic* MID = SMC->CreateDynamicMaterialInstance(0, BaseMat))
					{
						MID->SetVectorParameterValue(TEXT("Color"), Tint);
					}
				}
			}
		}
		return A;
	};

	switch (Type)
	{
		case EExperimentType::RHI:
		{
			// Stroke the user's OWN tracked hand (the one they see) with a brush — no separate fake hand.
			// Keep the tracked hands visible; TickRHI moves the brush 1 Hz back-and-forth over the right hand.
			P->SetHandsVisible(true);
			RHIBrush = MakeMesh(TEXT("RHI_Brush"),
				BrushMesh,
				P->GetVisualWristLocation(/*right*/ true), // refined immediately below so the BRISTLE TIP is at the hand
				kBrushRot,
				BrushMesh ? kBrushFitBox : FVector(2.0f, 18.0f, 2.0f),
				FLinearColor(0.9f, 0.88f, 0.82f)); // cream brush handle
			UE_LOG(LogTemp, Display, TEXT("[HandVR] RHI spawn: BrushMesh %s (fitScale=%.3f)"),
				BrushMesh ? TEXT("LOADED") : TEXT("NULL -> cube fallback"), ComputeFitScale(BrushMesh, kBrushFitBox));
			if (RHIBrush)
			{
				// Place so the bristle tip (+end of the longest local axis) sits ~1 cm above the hand surface.
				const FVector HandLoc = P->GetVisualWristLocation(/*right*/ true);
				const float   Scale   = ComputeFitScale(BrushMesh, kBrushFitBox);
				const FVector ToTip   = WorkingPartWorldOffset(BrushMesh, Scale, kBrushRot, /*bPositiveEnd*/ kBrushBristleAtPositiveEnd);
				RHIBrush->SetActorLocation(HandLoc + FVector(0.0f, 0.0f, 1.0f) - ToTip);
			}
			// Brush-stroke SFX as a prop-attached AudioComponent: Play() restarts the 2 s clip once per 왕복 so it
			// is spatialized at the brush and never overlaps (vs SpawnSoundAtLocation, which piled up at 2 Hz).
			if (!BrushSound) BrushSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Imported/Audio/brush.brush"));
			if (RHIBrush && BrushSound)
			{
				BrushAudio = NewObject<UAudioComponent>(RHIBrush);
				BrushAudio->SetupAttachment(RHIBrush->GetRootComponent());
				BrushAudio->SetSound(BrushSound);
				BrushAudio->bAutoActivate = false;
				BrushAudio->RegisterComponent();
			}
			PrevBrushOff = 0.0f;
			break;
		}

		case EExperimentType::VisualCapture:
		{
			// Snap visual hands +LateralOffsetCm laterally — instant, no fade. Real wrist position unchanged.
			P->SetVisualOffset(FVector(0.0f, LateralOffsetCm, 0.0f), FVector(0.0f, LateralOffsetCm, 0.0f));
			P->SetVisualLatencyMs(LatencyMs); // 시간오차 변인 (보고서 5장)

			// Target on desk — a plain RED SPHERE the user reaches for with the seen hand. (Previously this used
			// TargetMesh = SM_ClawHammer, which is why a HAMMER appeared on the desk in 실험2 — removed per request.
			// The hammer now belongs only to 실험4/Threat.)
			static UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
			VCTarget = MakeMesh(TEXT("VC_Target"),
				SphereMesh,
				DeskTopCtr + FVector(0.0f, 0.0f, 6.0f),
				FRotator::ZeroRotator,
				FVector(8.0f, 8.0f, 8.0f),
				FLinearColor(1.0f, 0.15f, 0.15f),
				/*bForceTint*/ true);
			break;
		}

		case EExperimentType::Drift:
		{
			P->SetVisualOffset(FVector(0.0f, LateralOffsetCm, 0.0f), FVector(0.0f, LateralOffsetCm, 0.0f));
			P->SetVisualLatencyMs(LatencyMs); // 시간오차 변인 (보고서 5장)

			// Ghost cube at the REAL hand position — tick updates its transform to track real wrist.
			DriftGhost = MakeMesh(TEXT("Drift_Ghost"),
				nullptr,
				DeskTopCtr,
				FRotator::ZeroRotator,
				FVector(4.0f, 4.0f, 4.0f),
				FLinearColor(0.3f, 0.7f, 1.0f));
			break;
		}

		case EExperimentType::Threat:
		{
			// Threaten the user's OWN tracked hand with the hammer — no separate fake hand. Keep hands visible;
			// TickThreat animates the hammer head lift->strike->impact->return onto the right tracked hand.
			P->SetHandsVisible(true);
			ThreatHammer = MakeMesh(TEXT("Threat_Hammer"),
				TargetMesh,
				P->GetVisualWristLocation(/*right*/ true), // positioned every tick in TickThreat so the HEAD lands on the hand
				FRotator(0.0f, kHammerBaseYaw, kHammerStrikeRoll),
				TargetMesh ? kHammerFitBox : FVector(4.0f, 4.0f, 30.0f),
				FLinearColor(0.25f, 0.25f, 0.27f)); // dark grey hammer
			UE_LOG(LogTemp, Display, TEXT("[HandVR] Threat spawn: TargetMesh %s (fitScale=%.3f)"),
				TargetMesh ? TEXT("LOADED") : TEXT("NULL -> cube fallback"), ComputeFitScale(TargetMesh, kHammerFitBox));
			// Hammer impact SFX as a prop-attached AudioComponent: Play() restarts the 2 s clip on each strike.
			if (!HammerSound) HammerSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Imported/Audio/hammer.hammer"));
			if (ThreatHammer && HammerSound)
			{
				ThreatAudio = NewObject<UAudioComponent>(ThreatHammer);
				ThreatAudio->SetupAttachment(ThreatHammer->GetRootComponent());
				ThreatAudio->SetSound(HammerSound);
				ThreatAudio->bAutoActivate = false;
				ThreatAudio->RegisterComponent();
			}
			PrevThreatT = 0.0f;
			break;
		}

		default: break;
	}

	// 실험 설명 패널 — "이 실험이 무엇인지" 사용자 앞 위쪽에 안내(시작 전/진행 중 항상 보임).
	{
		FString InfoTitle, InfoBody;
		GetExperimentInfo(Type, InfoTitle, InfoBody);
		ShowExperimentInfo(InfoTitle, InfoBody);
	}
}

void AExperimentManager::EnterSurvey()
{
	State = EExperimentState::Survey;
	DespawnProps();
	DespawnResultsPanel();
	if (AHandPawn* P = GetHandPawn()) { P->ResetVisualOffsets(); P->SetHandsVisible(true); }
	SetButtonsVisible(false, false, false, false);

	UWorld* W = GetWorld();
	if (!W) return;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Survey = W->SpawnActor<ASurveyManager>(ASurveyManager::StaticClass(), FTransform(ButtonRowBaseLoc + FVector(0.0f, 0.0f, 30.0f)), Params);
	if (Survey)
	{
		Survey->OnFinished.AddDynamic(this, &AExperimentManager::OnSurveyFinished);
		Survey->BeginSurvey(CurrentConditionTag);
	}
}

// --- 결과 보기 ----------------------------------------------------------------------------------------------------

void AExperimentManager::EnterResults()
{
	State       = EExperimentState::Results;
	CurrentType = EExperimentType::None;
	DespawnProps();
	if (AHandPawn* P = GetHandPawn()) { P->ResetVisualOffsets(); P->SetHandsVisible(true); }
	// 메뉴 버튼은 숨기고, 닫기 버튼만 보이게.
	SetButtonsVisible(false, false, false, true);
	ShowResultsPanel();
}

FString AExperimentManager::FindLatestSurveyCsv() const
{
	const FString Dir = FPaths::ProjectSavedDir();
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(Dir / TEXT("SurveyResults_*.csv")), /*Files*/ true, /*Directories*/ false);
	if (Files.Num() == 0) return FString();

	// 파일명에 YYYYMMDD_HHMMSS 타임스탬프가 들어있어 이름 정렬 = 시간 정렬. 마지막이 최신.
	Files.Sort();
	return Dir / Files.Last();
}

void AExperimentManager::ShowResultsPanel()
{
	DespawnResultsPanel();

	UWorld* W = GetWorld();
	if (!W) return;

	// 패널 본체를 사용자 앞 가슴 높이에 띄운다 (메뉴 그리드와 같은 x, 약간 위).
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector PanelLoc = ButtonRowBaseLoc + FVector(0.0f, 0.0f, 22.0f);
	ResultsPanel = W->SpawnActor<AActor>(AActor::StaticClass(), FTransform(PanelLoc), Params);
	if (!ResultsPanel) return;

	USceneComponent* Root = NewObject<USceneComponent>(ResultsPanel, TEXT("ResultsRoot"));
	Root->RegisterComponent();
	ResultsPanel->SetRootComponent(Root);

	// SurveyManager 와 동일한 월드 텍스트 위젯 구성 패턴.
	auto MakeText = [&](const TCHAR* Name, FVector RelLoc, FVector2D DrawSize, float WorldScale) -> UWidgetComponent*
	{
		UWidgetComponent* WC = NewObject<UWidgetComponent>(ResultsPanel, Name);
		WC->SetupAttachment(Root);
		WC->SetRelativeLocation(RelLoc);
		WC->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f)); // 플레이어(-X) 쪽을 향함
		WC->SetWidgetSpace(EWidgetSpace::World);
		WC->SetDrawSize(DrawSize);
		WC->SetWidgetClass(UVRLabelWidget::StaticClass());
		WC->SetWorldScale3D(FVector(WorldScale));
		WC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WC->RegisterComponent();
		WC->InitWidget(); // UUserWidget 인스턴스를 즉시 생성 → 바로 아래서 GetUserWidgetObject() 사용 가능.
		return WC;
	};

	ResultsTitleWidget = MakeText(TEXT("ResultsTitle"), FVector(0.0f, 0.0f, 30.0f), FVector2D(1400.0f, 120.0f),  0.06f);
	ResultsBodyWidget  = MakeText(TEXT("ResultsBody"),  FVector(0.0f, 0.0f, -8.0f), FVector2D(1600.0f, 900.0f), 0.05f);

	if (ResultsTitleWidget)
	{
		if (UVRLabelWidget* L = Cast<UVRLabelWidget>(ResultsTitleWidget->GetUserWidgetObject()))
		{
			L->SetLabelColor(FLinearColor(0.3f, 0.9f, 0.9f));
			L->SetLabelFontSize(52.0f);
			L->SetLabelText(FText::FromString(TEXT("설문 결과")));
		}
	}

	// 최신 CSV 파싱: 헤더(9개 측정항목) + 마지막 데이터 행 → "항목: 점수" 한 줄씩.
	FString Body;
	const FString CsvPath = FindLatestSurveyCsv();
	if (CsvPath.IsEmpty())
	{
		Body = TEXT("저장된 결과 없음");
	}
	else
	{
		TArray<FString> Lines;
		FFileHelper::LoadFileToStringArray(Lines, *CsvPath);
		if (Lines.Num() < 2)
		{
			Body = TEXT("저장된 결과 없음");
		}
		else
		{
			TArray<FString> Headers;
			Lines[0].ParseIntoArray(Headers, TEXT(","), /*CullEmpty*/ false);
			TArray<FString> Scores;
			Lines.Last().ParseIntoArray(Scores, TEXT(","), /*CullEmpty*/ false);

			TArray<FString> BodyLines;
			const int32 Count = FMath::Min(Headers.Num(), Scores.Num());
			for (int32 i = 0; i < Count; ++i)
			{
				BodyLines.Add(FString::Printf(TEXT("%s: %s"), *Headers[i].TrimStartAndEnd(), *Scores[i].TrimStartAndEnd()));
			}
			Body = BodyLines.Num() > 0 ? FString::Join(BodyLines, TEXT("\n")) : TEXT("저장된 결과 없음");
		}
	}

	if (ResultsBodyWidget)
	{
		if (UVRLabelWidget* L = Cast<UVRLabelWidget>(ResultsBodyWidget->GetUserWidgetObject()))
		{
			L->SetLabelColor(FLinearColor::White);
			L->SetLabelFontSize(34.0f);
			L->SetLabelText(FText::FromString(Body));
		}
	}
}

void AExperimentManager::DespawnResultsPanel()
{
	if (ResultsPanel) { ResultsPanel->Destroy(); ResultsPanel = nullptr; }
	ResultsTitleWidget = nullptr;
	ResultsBodyWidget  = nullptr;
}

// --- 실험 설명 패널 -----------------------------------------------------------------------------------------------

void AExperimentManager::GetExperimentInfo(EExperimentType Type, FString& OutTitle, FString& OutBody)
{
	switch (Type)
	{
		case EExperimentType::RHI:
			OutTitle = TEXT("실험 1 · 고무손 착각 (RHI)");
			OutBody  = TEXT("붓이 당신의 손을 일정한 속도로 쓰다듬습니다.\n"
			               "손을 책상에 가만히 올려두고 붓의 움직임을 바라보세요.\n"
			               "보이는 자극과 느껴지는 촉각이 일치할 때\n"
			               "그 손을 '내 손'처럼 느끼게 되는지 확인하는 실험입니다.");
			break;
		case EExperimentType::VisualCapture:
			OutTitle = TEXT("실험 2 · 시각적 포착");
			OutBody  = TEXT("보이는 손이 실제 손보다 옆으로 이동해 있습니다.\n"
			               "책상 위 빨간 표적을 '보이는 손'으로 만져 보세요.\n"
			               "눈으로 본 위치와 실제 위치가 다를 때\n"
			               "어느 쪽을 더 믿게 되는지 확인하는 실험입니다.");
			break;
		case EExperimentType::Drift:
			OutTitle = TEXT("실험 3 · 고유수용감각 표류");
			OutBody  = TEXT("보이는 손의 위치가 천천히 좌우로 흔들립니다.\n"
			               "파란 표식은 당신의 '실제' 손 위치입니다.\n"
			               "시각 정보 때문에 실제 손 위치 감각이\n"
			               "점점 표류하는지 확인하는 실험입니다.");
			break;
		case EExperimentType::Threat:
			OutTitle = TEXT("실험 4 · 망치 위협");
			OutBody  = TEXT("망치가 당신의 손 위로 반복해서 내려칩니다.\n"
			               "(실제로 손에 닿지는 않습니다)\n"
			               "가상의 위협 자극이 실제 위협처럼\n"
			               "느껴지는지(정서적 반응) 확인하는 실험입니다.");
			break;
		default:
			OutTitle = TEXT("실험");
			OutBody  = TEXT("");
			break;
	}
}

void AExperimentManager::ShowExperimentInfo(const FString& Title, const FString& Body)
{
	DespawnExperimentInfo();

	UWorld* W = GetWorld();
	if (!W) return;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// 사용자 정면, 책상/손/프롭(z≈75~80)과 중단 버튼(z=100) 위쪽에 띄워 시야를 가리지 않게 한다.
	const FVector PanelLoc = FVector(60.0f, 0.0f, 124.0f);
	InfoPanel = W->SpawnActor<AActor>(AActor::StaticClass(), FTransform(PanelLoc), Params);
	if (!InfoPanel) return;

	USceneComponent* Root = NewObject<USceneComponent>(InfoPanel, TEXT("InfoRoot"));
	Root->RegisterComponent();
	InfoPanel->SetRootComponent(Root);

	auto MakeText = [&](const TCHAR* Name, FVector RelLoc, FVector2D DrawSize, float WorldScale) -> UWidgetComponent*
	{
		UWidgetComponent* WC = NewObject<UWidgetComponent>(InfoPanel, Name);
		WC->SetupAttachment(Root);
		WC->SetRelativeLocation(RelLoc);
		WC->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f)); // 플레이어(-X) 쪽을 향함
		WC->SetWidgetSpace(EWidgetSpace::World);
		WC->SetDrawSize(DrawSize);
		WC->SetWidgetClass(UVRLabelWidget::StaticClass());
		WC->SetWorldScale3D(FVector(WorldScale));
		WC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WC->RegisterComponent();
		WC->InitWidget();
		return WC;
	};

	InfoTitleWidget = MakeText(TEXT("InfoTitle"), FVector(0.0f, 0.0f, 20.0f), FVector2D(1300.0f, 130.0f), 0.05f);
	InfoBodyWidget  = MakeText(TEXT("InfoBody"),  FVector(0.0f, 0.0f, -14.0f), FVector2D(1500.0f, 760.0f), 0.045f);

	if (InfoTitleWidget)
	{
		if (UVRLabelWidget* L = Cast<UVRLabelWidget>(InfoTitleWidget->GetUserWidgetObject()))
		{
			L->SetLabelColor(FLinearColor(0.95f, 0.85f, 0.4f));
			L->SetLabelFontSize(46.0f);
			L->SetLabelText(FText::FromString(Title));
		}
	}
	if (InfoBodyWidget)
	{
		if (UVRLabelWidget* L = Cast<UVRLabelWidget>(InfoBodyWidget->GetUserWidgetObject()))
		{
			L->SetLabelColor(FLinearColor::White);
			L->SetLabelFontSize(30.0f);
			L->SetLabelText(FText::FromString(Body));
		}
	}
}

void AExperimentManager::DespawnExperimentInfo()
{
	if (InfoPanel) { InfoPanel->Destroy(); InfoPanel = nullptr; }
	InfoTitleWidget = nullptr;
	InfoBodyWidget  = nullptr;
}

// --- Per-experiment tick -----------------------------------------------------------------------------------------

void AExperimentManager::TickRHI(float /*DeltaTime*/)
{
	if (!RHIBrush) return;
	AHandPawn* P = GetHandPawn();
	if (!P) return;

	// Sweep the BRISTLE TIP back and forth over the user's seen (right) hand. Convert "origin at hand" into
	// "bristle tip at hand" via the bounds: actorLoc = tipTarget - worldOffset(origin -> bristle tip), so the
	// TIP (not the origin) traces the stroke ~1 cm above the hand surface.
	FVector Center = P->GetVisualWristLocation(/*right*/ true);
	// 불일치(mismatch) 조건: 보이는 손 중심이 아니라 다른 손가락쪽(+X)으로 스트로크 중심을 옮긴다 (보고서 4.5).
	if (bBrushMismatch) Center += FVector(BrushMismatchShiftCm, 0.0f, 0.0f);

	const float   Ampl  = 6.0f; // cm — stroke length (±6)
	const float   Hz    = FMath::Max(0.05f, BrushStrokeHz); // 0.5 Hz = 1왕복/2s
	const float   Off   = FMath::Sin(ExperimentTime * Hz * 2.0f * PI) * Ampl;
	const float   Scale = ComputeFitScale(BrushMesh, kBrushFitBox);
	const FVector ToTip = WorkingPartWorldOffset(BrushMesh, Scale, kBrushRot, /*bPositiveEnd*/ kBrushBristleAtPositiveEnd);
	RHIBrush->SetActorLocation(Center + FVector(Off, 0.0f, 1.0f) - ToTip);

	// Play the 2 s brush clip once per 왕복 (one full sine cycle). floor(t*Hz) ticks up once per cycle; Play()
	// restarts the prop-attached AudioComponent so clips never overlap.
	const int32 Cycle = FMath::FloorToInt(ExperimentTime * Hz);
	if (BrushAudio && Cycle > BrushCycle)
	{
		BrushAudio->Play();
		BrushCycle = Cycle;
	}
	PrevBrushOff = Off;
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

void AExperimentManager::TickThreat(float /*DeltaTime*/)
{
	if (!ThreatHammer) return;
	AHandPawn* P = GetHandPawn();
	if (!P) return;

	// 4-stage automated threat over the user's OWN (right) tracked hand (report: 들어올림→내려치기→충격→복귀), looping.
	const FVector HandLoc = P->GetVisualWristLocation(/*right*/ true);
	const float   Period  = 3.0f;
	const float   t       = FMath::Fmod(ExperimentTime, Period);

	float    HeightCm; // height of the HAMMER HEAD above the hand surface this frame
	// Rest orientation: yaw 90 (90° CCW from old 180, top-down) + roll 90 (about the handle axis so the flat
	// striking face hits the hand). The swing cocks roll back relative to that base, returning to it on impact.
	FRotator Rot(0.0f, kHammerBaseYaw, kHammerStrikeRoll);
	if (t < 1.0f)          // 1) LIFT — raise the head above the hand and cock back
	{
		const float a = t / 1.0f;
		HeightCm  = FMath::Lerp(6.0f, 42.0f, a);
		Rot.Roll  = kHammerStrikeRoll + FMath::Lerp(0.0f, -35.0f, a);
	}
	else if (t < 1.4f)     // 2) STRIKE — head accelerates straight DOWN toward the hand
	{
		const float a = (t - 1.0f) / 0.4f;
		HeightCm  = FMath::Lerp(42.0f, 0.0f, a * a); // ease-in (accelerating swing) to head-on-hand
		Rot.Roll  = kHammerStrikeRoll + FMath::Lerp(-35.0f, 0.0f, a);
	}
	else if (t < 1.7f)     // 3) IMPACT — HEAD position == hand position, with a tiny contact shake
	{
		HeightCm  = FMath::Abs(FMath::Sin((t - 1.4f) * 60.0f)) * 0.6f; // 0..0.6 cm, stays on the hand
		Rot.Roll  = kHammerStrikeRoll;
	}
	else                   // 4) RETURN — head rises back to the rest pose
	{
		const float a = (t - 1.7f) / (Period - 1.7f);
		HeightCm  = FMath::Lerp(0.0f, 6.0f, a);
		Rot.Roll  = kHammerStrikeRoll;
	}

	// Convert "head at target" into actor origin: actorLoc = headTarget - worldOffset(origin -> head), with the
	// offset recomputed against the LIVE rotation each frame so the HEAD (not the handle/origin) lands on the hand.
	const float   Scale   = ComputeFitScale(TargetMesh, kHammerFitBox);
	const FVector ToHead  = WorkingPartWorldOffset(TargetMesh, Scale, Rot, /*bPositiveEnd*/ kHammerHeadAtPositiveEnd);
	const FVector HeadTgt = HandLoc + FVector(0.0f, 0.0f, HeightCm);
	ThreatHammer->SetActorLocation(HeadTgt - ToHead);
	ThreatHammer->SetActorRotation(Rot);

	// Hammer impact SFX: fire exactly ONCE per cycle at the STRIKE->IMPACT crossing (t passes 1.4 = head hits
	// the hand). Play() restarts the prop-attached 2 s clip each strike (period 3 s > 2 s clip → no overlap).
	if (ThreatAudio && PrevThreatT < 1.4f && t >= 1.4f)
	{
		ThreatAudio->Play();
	}
	PrevThreatT = t;
}

// --- Condition presets -------------------------------------------------------------------------------------------

void AExperimentManager::ApplyActiveCondition()
{
	const int32 N = UE_ARRAY_COUNT(kConditions);
	ActiveConditionIdx = ((ActiveConditionIdx % N) + N) % N; // wrap (handles the ++ in the button handler)
	const FCondPreset& C = kConditions[ActiveConditionIdx];

	// Drive the experiment variables from the active preset; EnterExperiment reads these.
	LateralOffsetCm = C.OffsetCm;
	LatencyMs       = C.LatencyMs;
	bBrushMismatch  = !C.bMatched;

	if (BtnCondition)
	{
		BtnCondition->Configure(
			FString::Printf(TEXT("조건 %d/%d\n%s"), ActiveConditionIdx + 1, N, C.Name),
			BID_Condition, FLinearColor(0.55f, 0.45f, 0.8f));
	}
	UE_LOG(LogTemp, Display, TEXT("[HandVR] Condition %d/%d %s (offset=%.0f latency=%.0f match=%d)"),
		ActiveConditionIdx + 1, N, C.Name, LateralOffsetCm, LatencyMs, C.bMatched ? 1 : 0);
}

// --- Events ------------------------------------------------------------------------------------------------------

void AExperimentManager::OnButtonPressed(int32 ButtonId)
{
	switch (ButtonId)
	{
		case BID_Start:   if (State == EExperimentState::Start)      EnterMainMenu();                          break;
		case BID_RHI:     if (State == EExperimentState::MainMenu)   EnterExperiment(EExperimentType::RHI);    break;
		case BID_VC:      if (State == EExperimentState::MainMenu)   EnterExperiment(EExperimentType::VisualCapture); break;
		case BID_Drift:   if (State == EExperimentState::MainMenu)   EnterExperiment(EExperimentType::Drift);  break;
		case BID_Threat:  if (State == EExperimentState::MainMenu)   EnterExperiment(EExperimentType::Threat); break;
		case BID_Survey:  if (State == EExperimentState::MainMenu)   { CurrentConditionTag = TEXT("manual"); EnterSurvey(); } break;
		case BID_Results: if (State == EExperimentState::MainMenu)   EnterResults();                           break;
		case BID_Condition: if (State == EExperimentState::MainMenu) { ++ActiveConditionIdx; ApplyActiveCondition(); } break;
		case BID_Exit:    if (State == EExperimentState::MainMenu)   UKismetSystemLibrary::QuitGame(GetWorld(), nullptr, EQuitPreference::Quit, false); break;
		// 실험 종료 → 해당 조건 설문 자동 표시(보고서 4.4) → 응답 후 메뉴 복귀.
		case BID_Stop:    if (State == EExperimentState::Experiment) EnterSurvey();                            break;
		case BID_Close:   if (State == EExperimentState::Results)    EnterMainMenu();                          break;
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
	// Audio components are attached to RHIBrush / ThreatHammer, so destroying those actors destroys the
	// components too — just drop our pointers so we never touch a dangling component.
	BrushAudio  = nullptr;
	ThreatAudio = nullptr;
	auto Kill = [](AActor*& A) { if (A) { A->Destroy(); A = nullptr; } };
	Kill(RHIFakeHand);
	Kill(RHIBrush);
	Kill(VCTarget);
	Kill(DriftGhost);
	Kill(ThreatHammer);
	// 실험 설명 패널도 함께 제거 — 모든 상태 전환에서 DespawnProps가 호출되므로 패널 누수가 없다.
	DespawnExperimentInfo();
}
