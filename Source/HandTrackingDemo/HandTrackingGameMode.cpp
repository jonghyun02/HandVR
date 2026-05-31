#include "HandTrackingGameMode.h"

#include "HandPawn.h"
#include "ExperimentManager.h"
#include "WorldSetup.h"
#include "Engine/World.h"

AHandTrackingGameMode::AHandTrackingGameMode()
{
	DefaultPawnClass = AHandPawn::StaticClass();
}

void AHandTrackingGameMode::StartPlay()
{
	Super::StartPlay();

	UWorld* World = GetWorld();
	if (!World) return;

	// Drop a procedural room/desk/chair so the project is runnable before the user imports the assets/ FBX.
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	World->SpawnActor<AWorldSetup>(AWorldSetup::StaticClass(), FTransform::Identity, SpawnParams);

	// The ExperimentManager owns the menu / survey / experiment props lifecycle.
	World->SpawnActor<AExperimentManager>(AExperimentManager::StaticClass(), FTransform::Identity, SpawnParams);
}
