#include "HammerActor.h"
#include "RubberHandPawn.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInterface.h"
#include "DrawDebugHelpers.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"

AHammerActor::AHammerActor()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	HandleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HandleMesh"));
	HandleMesh->SetupAttachment(SceneRoot);
	HandleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
	HeadMesh->SetupAttachment(HandleMesh);
	HeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ImpactAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("ImpactAudio"));
	ImpactAudio->SetupAttachment(HeadMesh);
	ImpactAudio->bAutoActivate = false;
	ImpactAudio->bAllowSpatialization = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
	if (Cylinder.Succeeded()) { HandleMesh->SetStaticMesh(Cylinder.Object); }
	if (Cube.Succeeded())     { HeadMesh->SetStaticMesh(Cube.Object); }
	if (Mat.Succeeded())
	{
		HandleMesh->SetMaterial(0, Mat.Object);
		HeadMesh->SetMaterial(0, Mat.Object);
	}

	// 손잡이 25cm × 2cm
	HandleMesh->SetRelativeScale3D(FVector(0.25f, 0.20f, 0.20f));
	// 헤드 (망치 머리) 손잡이 끝쪽에 직각으로
	HeadMesh->SetRelativeLocation(FVector(125.f, 0.f, 0.f));
	HeadMesh->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	HeadMesh->SetRelativeScale3D(FVector(0.06f, 0.06f, 0.10f));
}

void AHammerActor::BeginThreat(APawn* TargetPawn)
{
	OwnerPawn = TargetPawn;
	bActive = true;
	CycleTime = 0.f;
	bImpactedThisCycle = false;
}

void AHammerActor::EndThreat()
{
	bActive = false;
}

void AHammerActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bActive || !OwnerPawn) return;

	CycleTime += DeltaSeconds;
	if (CycleTime >= CycleSec) { CycleTime = 0.f; bImpactedThisCycle = false; }

	ARubberHandPawn* RH = Cast<ARubberHandPawn>(OwnerPawn);
	if (!RH) return;
	UPoseableMeshComponent* TargetHand = RH->LeftHandSynth ? RH->LeftHandSynth : RH->RightHandSynth;
	if (!TargetHand || TargetHand->GetNumBones() == 0) return;

	const FTransform Wrist = TargetHand->GetBoneTransformByName(TargetHand->GetBoneName(0), EBoneSpaces::WorldSpace);
	const FVector HandLoc = Wrist.GetLocation();

	// 위상 별 위치
	// 0~0.5    Idle     : H + (0,0,40cm)
	// 0.5~1.0  WindUp   : H + (0,0,60cm)
	// 1.0~1.15 Strike   : 60cm → 5cm 빠르게
	// 1.15     Impact
	// 1.15~3.0 Recovery : 5cm → 40cm 복귀
	// 3.0~ Cycle 끝
	float HeightCm = 40.f;
	if      (CycleTime < 0.5f)              { HeightCm = 40.f; }
	else if (CycleTime < 1.0f)              { HeightCm = FMath::Lerp(40.f, 60.f, (CycleTime - 0.5f) / 0.5f); }
	else if (CycleTime < 1.15f)             { HeightCm = FMath::Lerp(60.f,  5.f, (CycleTime - 1.0f) / 0.15f); }
	else if (CycleTime < 3.0f)              { HeightCm = FMath::Lerp( 5.f, 40.f, (CycleTime - 1.15f) / 1.85f); }
	else                                    { HeightCm = 40.f; }

	if (CycleTime >= 1.15f && !bImpactedThisCycle)
	{
		bImpactedThisCycle = true;
		// 충격: 빨간 플래시 + 카메라 셰이크 + 충격음
		if (ImpactAudio && ImpactSound)
		{
			ImpactAudio->SetSound(ImpactSound);
			ImpactAudio->Play();
		}
		else if (ImpactSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, HandLoc);
		}

		// 디버그 플래시: 빨간 sphere 0.15초
		DrawDebugSphere(GetWorld(), HandLoc, 25.f, 16, FColor::Red, false, 0.15f, 0, 1.0f);

		if (APlayerCameraManager* CM = UGameplayStatics::GetPlayerCameraManager(this, 0))
		{
			CM->StartCameraFade(0.6f, 0.f, 0.4f, FLinearColor::Red, false, true);
		}
	}

	const FVector Loc = HandLoc + FVector(0.f, 0.f, HeightCm);
	SetActorLocation(Loc);
	// 망치 헤드가 아래를 향하도록
	SetActorRotation(FRotator(-90.f, 0.f, 0.f));
}
