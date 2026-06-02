#include "HandTrackingGameMode.h"

#include "HandPawn.h"
#include "ExperimentManager.h"
#include "WorldSetup.h"
#include "Engine/World.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"

AHandTrackingGameMode::AHandTrackingGameMode()
{
	DefaultPawnClass = AHandPawn::StaticClass();
	PrimaryActorTick.bCanEverTick = true; // only used by the -AutoShot diagnostic; Tick early-returns otherwise
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

	UE_LOG(LogTemp, Display, TEXT("[HandVR] StartPlay: spawned WorldSetup + ExperimentManager"));

	// Diagnostics-only: with -AutoShot on the command line, capture the rendered viewport to disk a few
	// seconds after the scene is up. Used to verify the scene in the Meta XR Simulator without a headset.
	// No effect on normal runs (gated on the command-line switch).
	if (FParse::Param(FCommandLine::Get(), TEXT("AutoShot")))
	{
		// Capture a numbered SEQUENCE (seq_NN.png) so the user can interact while frames are captured over
		// time (≈ a flipbook/video) and the app does NOT exit. Portable; gated behind -AutoShot.
		AutoShotDir = FPaths::ProjectSavedDir() / TEXT("Screenshots");
		IFileManager::Get().MakeDirectory(*AutoShotDir, /*Tree*/ true);
		bAutoShotActive = true;
		AutoShotFrame = 0;
		AutoShotCount = 0;
		UE_LOG(LogTemp, Display, TEXT("[HandVR] AutoShot active (sequence) -> %s"), *AutoShotDir);
	}
}

void AHandTrackingGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bAutoShotActive)
	{
		return;
	}

	// The PC Meta XR Simulator renders the first frames then risks a D3D11 device-loss (~frame 19), and its
	// frame loop idles when unfocused. So capture on a couple of early frames, then exit cleanly well before
	// the device-loss window rather than leaving the app idling under GPU load.
	++AutoShotFrame;
	// Capture one frame after the scene settles, then one every ~90 frames (~1.2s), up to 40. NO RequestExit —
	// the user keeps interacting (press buttons, run experiments) and we get a flipbook sequence to review.
	if (AutoShotCount < 40 && (AutoShotFrame == 10 || (AutoShotFrame % 90) == 0))
	{
		const FString Path = AutoShotDir / FString::Printf(TEXT("seq_%02d.png"), AutoShotCount);
		FScreenshotRequest::RequestScreenshot(Path, /*bShowUI*/ false, /*bAddFilenameSuffix*/ false);
		UE_LOG(LogTemp, Display, TEXT("[HandVR] AutoShot seq %d -> %s"), AutoShotCount, *Path);
		++AutoShotCount;
	}
	else if (AutoShotCount >= 40)
	{
		bAutoShotActive = false;
		UE_LOG(LogTemp, Display, TEXT("[HandVR] AutoShot sequence complete (%d frames)."), AutoShotCount);
	}
}
