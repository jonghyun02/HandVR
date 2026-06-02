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
}

AHandPawn::AHandPawn()
{
	PrimaryActorTick.bCanEverTick = false;

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

void AHandPawn::SetVisualOffset(const FVector& LeftOffset, const FVector& RightOffset)
{
	if (LeftHandOffset)  LeftHandOffset->SetRelativeLocation(LeftOffset);
	if (RightHandOffset) RightHandOffset->SetRelativeLocation(RightOffset);
}

void AHandPawn::ResetVisualOffsets()
{
	SetVisualOffset(FVector::ZeroVector, FVector::ZeroVector);
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
