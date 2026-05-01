#include "BrushActor.h"
#include "RubberHandPawn.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInterface.h"
#include "DrawDebugHelpers.h"

ABrushActor::ABrushActor()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	HandleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HandleMesh"));
	HandleMesh->SetupAttachment(SceneRoot);
	HandleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	TipMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TipMesh"));
	TipMesh->SetupAttachment(HandleMesh);
	TipMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	StrokeAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("StrokeAudio"));
	StrokeAudio->SetupAttachment(TipMesh);
	StrokeAudio->bAutoActivate = false;
	StrokeAudio->bAllowSpatialization = true;
	StrokeAudio->bOverrideAttenuation = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
	if (Cylinder.Succeeded()) { HandleMesh->SetStaticMesh(Cylinder.Object); }
	if (Cone.Succeeded())     { TipMesh->SetStaticMesh(Cone.Object); }
	if (Mat.Succeeded())
	{
		HandleMesh->SetMaterial(0, Mat.Object);
		TipMesh->SetMaterial(0, Mat.Object);
	}

	// 손잡이 12cm × 1cm × 1cm
	HandleMesh->SetRelativeScale3D(FVector(0.12f, 0.10f, 0.10f));
	// 끝 (브러시 헤드) 손잡이 끝쪽에 부착, 살짝 가늘고 굵게
	TipMesh->SetRelativeLocation(FVector(60.f, 0.f, 0.f));
	TipMesh->SetRelativeScale3D(FVector(0.04f, 0.06f, 0.06f));
}

void ABrushActor::BeginStroking(APawn* TargetPawn, bool bSync)
{
	OwnerPawn = TargetPawn;
	bSynchronized = bSync;
	bActive = true;
	ElapsedTime = 0.f;

	if (StrokeAudio && StrokeSound)
	{
		StrokeAudio->SetSound(StrokeSound);
		StrokeAudio->Play();
	}
}

void ABrushActor::EndStroking()
{
	bActive = false;
	if (StrokeAudio) { StrokeAudio->Stop(); }
}

void ABrushActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bActive || !OwnerPawn) return;

	ElapsedTime += DeltaSeconds;

	ARubberHandPawn* RH = Cast<ARubberHandPawn>(OwnerPawn);
	if (!RH) return;

	UPoseableMeshComponent* TargetHand = RH->LeftHandSynth ? RH->LeftHandSynth : RH->RightHandSynth;
	if (!TargetHand || TargetHand->GetNumBones() == 0) return;

	// bSync면 검지/중지/약지 본 위에 갖다대고 좌우 왕복.
	// async면 약지/소지 부근으로 의도적 위치 어긋남 (시각-촉각 불일치).
	const FName FocusBone = bSynchronized ? FName(TEXT("Hand_IndexTip")) : FName(TEXT("Hand_RingTip"));
	FTransform Bone = TargetHand->GetBoneTransformByName(FocusBone, EBoneSpaces::WorldSpace);
	if (Bone.GetLocation().IsNearlyZero())
	{
		// fallback - 0번 본 (wrist)
		Bone = TargetHand->GetBoneTransformByName(TargetHand->GetBoneName(0), EBoneSpaces::WorldSpace);
	}

	const float Phase = ElapsedTime * StrokeFrequencyHz * 2.f * PI;
	const FVector Wobble = Bone.GetRotation().GetRightVector() * (FMath::Sin(Phase) * StrokeAmplitudeCm);
	const FVector Above  = Bone.GetRotation().GetUpVector() * 1.5f; // 손등 위 1.5cm

	const FVector NewLoc = Bone.GetLocation() + Wobble + Above;
	SetActorLocation(NewLoc);
	SetActorRotation(Bone.GetRotation().Rotator() + FRotator(0.f, 90.f, 0.f));

	// 디버그 비주얼 (BP 에셋 없을 때 placeholder)
	DrawDebugLine(GetWorld(), NewLoc, NewLoc - Above * 4.f, FColor::Cyan, false, DeltaSeconds * 1.05f, 0, 0.2f);
}
