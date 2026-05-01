#include "RubberHandPawn.h"
#include "Camera/CameraComponent.h"
#include "MotionControllerComponent.h"
#include "Components/SceneComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "OculusXRHandComponent.h"
#include "OculusXRInputFunctionLibrary.h"
#include "ExperimentManagerComponent.h"
#include "LabEnvironmentActor.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

ARubberHandPawn::ARubberHandPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork; // 본 갱신 후

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	VRCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("VRCamera"));
	VRCamera->SetupAttachment(SceneRoot);
	VRCamera->bLockToHmd = true;

	// MotionController + OculusXRHand 계층 (보고서 명시 구조 그대로)
	LeftMC = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("LeftMC"));
	LeftMC->SetupAttachment(SceneRoot);
	LeftMC->SetTrackingMotionSource(FName(TEXT("Left")));

	RightMC = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("RightMC"));
	RightMC->SetupAttachment(SceneRoot);
	RightMC->SetTrackingMotionSource(FName(TEXT("Right")));

	LeftHandTracker = CreateDefaultSubobject<UOculusXRHandComponent>(TEXT("LeftHandTracker"));
	LeftHandTracker->SetupAttachment(LeftMC);
	LeftHandTracker->SkeletonType       = EOculusXRHandType::HandLeft;
	LeftHandTracker->MeshType           = EOculusXRHandType::HandLeft;
	LeftHandTracker->ConfidenceBehavior = EOculusXRConfidenceBehavior::None;
	// Tracker는 데이터 소스만. 화면엔 안 보이게.
	LeftHandTracker->SetVisibility(true);

	RightHandTracker = CreateDefaultSubobject<UOculusXRHandComponent>(TEXT("RightHandTracker"));
	RightHandTracker->SetupAttachment(RightMC);
	RightHandTracker->SkeletonType       = EOculusXRHandType::HandRight;
	RightHandTracker->MeshType           = EOculusXRHandType::HandRight;
	RightHandTracker->ConfidenceBehavior = EOculusXRConfidenceBehavior::None;
	RightHandTracker->SetVisibility(true);

	// 합성 손 (offset + delay 적용된 가짜 손)
	LeftHandSynth = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("LeftHandSynth"));
	LeftHandSynth->SetupAttachment(SceneRoot); // 월드 컴포넌트 - 매 틱 본 트랜스폼을 직접 세팅
	LeftHandSynth->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	RightHandSynth = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("RightHandSynth"));
	RightHandSynth->SetupAttachment(SceneRoot);
	RightHandSynth->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ExperimentManager = CreateDefaultSubobject<UExperimentManagerComponent>(TEXT("ExperimentManager"));

	AutoPossessPlayer = EAutoReceiveInput::Player0;
}

void ARubberHandPawn::BeginPlay()
{
	Super::BeginPlay();

	UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(EHMDTrackingOrigin::LocalFloor);
	UOculusXRInputFunctionLibrary::SetSimultaneousHandsAndControllersEnabled(true);

	// 합성 손에 Tracker의 SkeletalMesh 공유 (런타임 직후엔 비어있을 수 있음 — 매 틱에서 lazy bind)
	if (LeftHandTracker && LeftHandSynth && LeftHandTracker->GetSkinnedAsset())
	{
		LeftHandSynth->SetSkinnedAsset(LeftHandTracker->GetSkinnedAsset());
	}
	if (RightHandTracker && RightHandSynth && RightHandTracker->GetSkinnedAsset())
	{
		RightHandSynth->SetSkinnedAsset(RightHandTracker->GetSkinnedAsset());
	}

	SetSyntheticHandVisible(bShowSyntheticHand);

	UE_LOG(LogTemp, Log, TEXT("[RHI] RubberHandPawn BeginPlay — offset=%.1fcm delay=%.0fms"),
		SpatialOffsetCm, TemporalDelayMs);
}

void ARubberHandPawn::SetSyntheticHandVisible(bool bVisible)
{
	bShowSyntheticHand = bVisible;
	if (LeftHandSynth)  { LeftHandSynth->SetVisibility(bVisible);  }
	if (RightHandSynth) { RightHandSynth->SetVisibility(bVisible); }
}

void ARubberHandPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const double NowSec = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	// Lazy bind SkeletalMesh (OpenXR 핸드 메쉬는 session 시작 후에 생성됨)
	if (LeftHandSynth && !LeftHandSynth->GetSkinnedAsset() && LeftHandTracker && LeftHandTracker->GetSkinnedAsset())
	{
		LeftHandSynth->SetSkinnedAsset(LeftHandTracker->GetSkinnedAsset());
	}
	if (RightHandSynth && !RightHandSynth->GetSkinnedAsset() && RightHandTracker && RightHandTracker->GetSkinnedAsset())
	{
		RightHandSynth->SetSkinnedAsset(RightHandTracker->GetSkinnedAsset());
	}

	// 1. 현재 Tracker 본을 ring buffer에 push
	RecordTrackerSnapshot(LeftHandTracker,  LeftBuffer,  NowSec);
	RecordTrackerSnapshot(RightHandTracker, RightBuffer, NowSec);

	// 2. delay 적용된 본을 Synth로 복사 + offset
	ApplyDelayedSnapshotToSynth(LeftBuffer,  LeftHandSynth,  NowSec);
	ApplyDelayedSnapshotToSynth(RightBuffer, RightHandSynth, NowSec);

	// 3. 컨트롤 패널 — 손가락 끝으로 버튼 poke + 상태 텍스트 갱신
	ALabEnvironmentActor* Lab = nullptr;
	{
		TArray<AActor*> Found;
		UGameplayStatics::GetAllActorsOfClass(this, ALabEnvironmentActor::StaticClass(), Found);
		if (Found.Num() > 0) { Lab = Cast<ALabEnvironmentActor>(Found[0]); }
	}
	if (!Lab) return;

	// 상태 텍스트 갱신.
	if (ExperimentManager)
	{
		Lab->UpdateStatusText(ExperimentManager->GetStatusString());
	}

	// poke 감지 — 양손 검지 끝 위치 vs 버튼.
	int32 HitNow = 0;
	const FVector LIdx = GetIndexTipWorldPos(LeftHandTracker);
	if (!LIdx.IsNearlyZero())
	{
		HitNow = Lab->HitTestButton(LIdx);
	}
	if (HitNow == 0)
	{
		const FVector RIdx = GetIndexTipWorldPos(RightHandTracker);
		if (!RIdx.IsNearlyZero())
		{
			HitNow = Lab->HitTestButton(RIdx);
		}
	}

	// 엣지 트리거 + 쿨다운 → 단일 클릭만 발생.
	if (HitNow != 0 && HitNow != LastHitButton && (NowSec - LastButtonHitTime) > ButtonCooldownSec)
	{
		if (ExperimentManager)
		{
			ExperimentManager->SelectCase(HitNow);
		}
		LastButtonHitTime = NowSec;
		UE_LOG(LogTemp, Log, TEXT("[RHI] Button poked: %d"), HitNow);
	}
	LastHitButton = HitNow;
}

FVector ARubberHandPawn::GetIndexTipWorldPos(UOculusXRHandComponent* Tracker) const
{
	if (!Tracker || Tracker->GetNumBones() <= 0) return FVector::ZeroVector;

	// OculusXR 핸드 본 이름은 EOculusXRBone::Index_Tip → FString.
	static const FName IndexTipBone(*UOculusXRInputFunctionLibrary::GetBoneName(EOculusXRBone::Index_Tip));
	const FTransform T = Tracker->GetBoneTransformByName(IndexTipBone, EBoneSpaces::WorldSpace);
	const FVector P = T.GetLocation();
	if (!P.IsNearlyZero()) return P;

	// 폴백: 손목 본.
	return Tracker->GetComponentLocation();
}

void ARubberHandPawn::RecordTrackerSnapshot(UOculusXRHandComponent* Tracker, TArray<FRHIBoneSnapshot>& Buffer, double NowSec) const
{
	if (!Tracker) return;
	const int32 NumBones = Tracker->GetNumBones();
	if (NumBones <= 0) return;

	FRHIBoneSnapshot Snap;
	Snap.TimeSec = NowSec;
	Snap.BoneWorld.Reserve(NumBones);
	for (int32 i = 0; i < NumBones; ++i)
	{
		Snap.BoneWorld.Add(Tracker->GetBoneTransformByName(Tracker->GetBoneName(i), EBoneSpaces::WorldSpace));
	}
	Buffer.Add(MoveTemp(Snap));

	// 오래된 항목 trim — 한 번에 trim 후보 개수만큼 제거 (TArray API 호환)
	const double Cutoff = NowSec - MaxBufferDurationSec;
	int32 TrimCount = 0;
	while (TrimCount < Buffer.Num() && Buffer[TrimCount].TimeSec < Cutoff) { ++TrimCount; }
	if (TrimCount > 0) { Buffer.RemoveAt(0, TrimCount); }
}

void ARubberHandPawn::ApplyDelayedSnapshotToSynth(const TArray<FRHIBoneSnapshot>& Buffer, UPoseableMeshComponent* Synth, double NowSec) const
{
	if (!Synth || !Synth->GetSkinnedAsset() || Buffer.Num() == 0) return;

	const double TargetTime = NowSec - (TemporalDelayMs * 0.001);

	// 가장 가까운 (작거나 같은) timestamp 찾기 — 끝부터 역순
	int32 PickIdx = Buffer.Num() - 1;
	for (int32 i = Buffer.Num() - 1; i >= 0; --i)
	{
		if (Buffer[i].TimeSec <= TargetTime) { PickIdx = i; break; }
	}
	const FRHIBoneSnapshot& Snap = Buffer[PickIdx];

	// 공간 오프셋 (HMD forward 방향 기준)
	FVector OffsetWorld = FVector::ZeroVector;
	if (SpatialOffsetCm > KINDA_SMALL_NUMBER)
	{
		FVector Dir = SpatialOffsetDirection.IsNearlyZero() ? FVector(1.f, 0.f, 0.f) : SpatialOffsetDirection.GetSafeNormal();
		// HMD forward 기준이 자연스러움 — 카메라 회전 적용
		if (VRCamera)
		{
			Dir = VRCamera->GetComponentTransform().TransformVectorNoScale(Dir);
		}
		OffsetWorld = Dir * SpatialOffsetCm; // 1cm = 1 UU (UE 기본)
	}

	const int32 N = FMath::Min(Snap.BoneWorld.Num(), Synth->GetNumBones());
	for (int32 i = 0; i < N; ++i)
	{
		FTransform T = Snap.BoneWorld[i];
		T.AddToTranslation(OffsetWorld);
		Synth->SetBoneTransformByName(Synth->GetBoneName(i), T, EBoneSpaces::WorldSpace);
	}
}
