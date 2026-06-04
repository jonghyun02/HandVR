#include "HandPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "MotionControllerComponent.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "HeadMountedDisplayTypes.h"
#include "UObject/ConstructorHelpers.h"

#include "OculusXRHandComponent.h"
#include "OculusXRInputFunctionLibrary.h"

namespace
{
	// Wrist trigger volume — large enough to be hit reliably by a relaxed reach.
	constexpr float WristTriggerRadius = 4.5f; // cm
	// Keep ~1.2 s of wrist history — covers the largest experiment latency (500 ms) with margin.
	constexpr double kMaxHistorySec = 1.2;
}

AHandPawn::AHandPawn()
{
	PrimaryActorTick.bCanEverTick = true; // drives the wrist-history ring buffer + visual offset/latency apply

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	VRCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("VRCamera"));
	VRCamera->SetupAttachment(SceneRoot);
	VRCamera->bLockToHmd = true;

	// (Removed LaunchMarker + LaunchLight: a leftover head-locked cube panel 140 cm in front of the camera
	//  — the mysterious "screen floating in front" — plus a head-locked 8000cd point light that created a
	//  moving hotspot / blow-out. Neither belongs in the seated experiment; scene lighting lives in WorldSetup.)

	auto BuildHand = [this](
		const TCHAR* MCName, const TCHAR* TriggerName, const TCHAR* OffsetName, const TCHAR* HandName,
		FName MotionSource, EOculusXRHandType HandType,
		UMotionControllerComponent*& OutMC, USphereComponent*& OutTrigger,
		USceneComponent*& OutOffset, UOculusXRHandComponent*& OutHand)
	{
		OutMC = CreateDefaultSubobject<UMotionControllerComponent>(MCName);
		OutMC->SetupAttachment(SceneRoot);
		OutMC->SetTrackingMotionSource(MotionSource);

		OutTrigger = CreateDefaultSubobject<USphereComponent>(TriggerName);
		OutTrigger->SetupAttachment(OutMC);
		OutTrigger->InitSphereRadius(WristTriggerRadius);
		OutTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		OutTrigger->SetCollisionObjectType(ECC_WorldDynamic);
		OutTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
		OutTrigger->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
		OutTrigger->SetGenerateOverlapEvents(true);
		OutTrigger->ComponentTags.Add(TEXT("HandTouch"));

		OutOffset = CreateDefaultSubobject<USceneComponent>(OffsetName);
		OutOffset->SetupAttachment(OutMC);

		OutHand = CreateDefaultSubobject<UOculusXRHandComponent>(HandName);
		OutHand->SetupAttachment(OutOffset);
		OutHand->SkeletonType        = HandType;
		OutHand->MeshType            = HandType;
		OutHand->ConfidenceBehavior  = EOculusXRConfidenceBehavior::None;
	};

	BuildHand(TEXT("LeftMC"),  TEXT("LeftWristTrigger"),  TEXT("LeftHandOffset"),  TEXT("LeftHand"),
		FName(TEXT("Left")),  EOculusXRHandType::HandLeft,
		LeftMC,  LeftWristTrigger,  LeftHandOffset,  LeftHand);

	BuildHand(TEXT("RightMC"), TEXT("RightWristTrigger"), TEXT("RightHandOffset"), TEXT("RightHand"),
		FName(TEXT("Right")), EOculusXRHandType::HandRight,
		RightMC, RightWristTrigger, RightHandOffset, RightHand);
}

void AHandPawn::BeginPlay()
{
	Super::BeginPlay();

	// Local floor → consistent desk height across users (matches passive-haptic 73cm desktop).
	UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(EHMDTrackingOrigin::LocalFloor);

	// SetSimultaneousHandsAndControllersEnabled(true) intentionally NOT called here:
	// the Meta XR plugin dereferences a null OpenXR Session handle if the call lands before
	// the VR session is fully bound (crashes in OculusXRSimultaneousHandsAndControllersExtensionPlugin.cpp:18).
	// User accepted "hand disappears when controller is held" as the failure mode, so we just skip the call.
}

void AHandPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	// Record this frame's REAL wrist positions, then drop anything older than the history window.
	FWristSample S;
	S.Time  = Now;
	S.Left  = LeftMC  ? LeftMC->GetComponentLocation()  : FVector::ZeroVector;
	S.Right = RightMC ? RightMC->GetComponentLocation() : FVector::ZeroVector;
	WristHistory.Add(S);

	int32 FirstKeep = 0;
	while (FirstKeep < WristHistory.Num() && (Now - WristHistory[FirstKeep].Time) > kMaxHistorySec) ++FirstKeep;
	if (FirstKeep > 0) WristHistory.RemoveAt(0, FirstKeep);

	ApplyVisualState();
}

FVector AHandPawn::SampleDelayedWrist(bool bRight, double Now) const
{
	const UMotionControllerComponent* MC = bRight ? RightMC : LeftMC;
	const FVector Live = MC ? MC->GetComponentLocation() : FVector::ZeroVector;
	if (VisualLatencySec <= KINDA_SMALL_NUMBER || WristHistory.Num() < 2) return Live;

	const double Target = Now - VisualLatencySec;
	if (Target <= WristHistory[0].Time) return bRight ? WristHistory[0].Right : WristHistory[0].Left;

	for (int32 i = WristHistory.Num() - 1; i >= 1; --i)
	{
		if (WristHistory[i - 1].Time <= Target && Target <= WristHistory[i].Time)
		{
			const FWristSample& A = WristHistory[i - 1];
			const FWristSample& B = WristHistory[i];
			const double Span  = B.Time - A.Time;
			const float  Alpha = Span > 1e-6 ? static_cast<float>((Target - A.Time) / Span) : 0.0f;
			return FMath::Lerp(bRight ? A.Right : A.Left, bRight ? B.Right : B.Left, Alpha);
		}
	}
	return Live;
}

void AHandPawn::ApplyVisualState()
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	auto Apply = [&](UMotionControllerComponent* MC, USceneComponent* Offset, bool bRight, const FVector& OffWorld)
	{
		if (!MC || !Offset) return;
		const FVector DelayedWorld = SampleDelayedWrist(bRight, Now);
		const FVector DesiredWorld = DelayedWorld + OffWorld;            // delayed real wrist + lateral offset (world)
		Offset->SetRelativeLocation(MC->GetComponentTransform().InverseTransformPosition(DesiredWorld));
	};
	Apply(LeftMC,  LeftHandOffset,  false, LeftOffsetWorld);
	Apply(RightMC, RightHandOffset, true,  RightOffsetWorld);
}

void AHandPawn::SetVisualOffset(const FVector& LeftOffset, const FVector& RightOffset)
{
	LeftOffsetWorld  = LeftOffset;
	RightOffsetWorld = RightOffset;
	ApplyVisualState();
}

void AHandPawn::SetVisualLatencyMs(float Ms)
{
	VisualLatencySec = FMath::Max(0.0f, Ms) * 0.001f;
}

void AHandPawn::ResetVisualOffsets()
{
	LeftOffsetWorld  = FVector::ZeroVector;
	RightOffsetWorld = FVector::ZeroVector;
	VisualLatencySec = 0.0f;
	// Keep WristHistory rolling so a latency experiment applies immediately (no buffer-refill warm-up).
	ApplyVisualState();
}

void AHandPawn::SetHandsVisible(bool bVisible)
{
	if (LeftHand)  LeftHand->SetVisibility(bVisible, true);
	if (RightHand) RightHand->SetVisibility(bVisible, true);
}

FVector AHandPawn::GetRealWristLocation(bool bRightHand) const
{
	const UMotionControllerComponent* MC = bRightHand ? RightMC : LeftMC;
	return MC ? MC->GetComponentLocation() : FVector::ZeroVector;
}

FVector AHandPawn::GetVisualWristLocation(bool bRightHand) const
{
	const USceneComponent* Offset = bRightHand ? RightHandOffset : LeftHandOffset;
	return Offset ? Offset->GetComponentLocation() : FVector::ZeroVector;
}
