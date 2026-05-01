#include "RubberHandGameMode.h"
#include "RubberHandPawn.h"
#include "LabEnvironmentActor.h"
#include "ExperimentManagerComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

ARubberHandGameMode::ARubberHandGameMode()
{
	DefaultPawnClass = ARubberHandPawn::StaticClass();
	LabEnvironmentClass = ALabEnvironmentActor::StaticClass();
}

void ARubberHandGameMode::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (!World) return;

	// 실험실 환경 스폰 (절차적 박스 메시)
	if (LabEnvironmentClass)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnedLab = World->SpawnActor<ALabEnvironmentActor>(LabEnvironmentClass, FVector::ZeroVector, FRotator::ZeroRotator, Params);
		UE_LOG(LogTemp, Log, TEXT("[RHI] LabEnvironment spawned: %s"), *GetNameSafe(SpawnedLab));
	}

	// 사용자 시작 위치를 의자에 앉은 자세로 이동
	if (SpawnedLab)
	{
		APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
		if (Pawn)
		{
			const FVector  ChairLoc  = SpawnedLab->GetChairWorldLocation();
			const FRotator ChairFace = SpawnedLab->GetChairFacingRotation();
			Pawn->SetActorLocationAndRotation(ChairLoc, ChairFace);
			UE_LOG(LogTemp, Log, TEXT("[RHI] Pawn moved to chair loc=%s rot=%s"),
				*ChairLoc.ToString(), *ChairFace.ToString());
		}
	}

	if (bAutoStartExperiment)
	{
		World->GetTimerManager().SetTimer(AutoStartTimer, this, &ARubberHandGameMode::TryAutoStartExperiment, AutoStartDelaySec, false);
	}
}

void ARubberHandGameMode::TryAutoStartExperiment()
{
	APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	ARubberHandPawn* RHPawn = Cast<ARubberHandPawn>(Pawn);
	if (RHPawn && RHPawn->ExperimentManager)
	{
		RHPawn->ExperimentManager->StartExperiment();
		UE_LOG(LogTemp, Log, TEXT("[RHI] Experiment auto-started"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[RHI] Auto-start skipped — pawn or manager missing"));
	}
}
