#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HandTrackingGameMode.generated.h"

UCLASS()
class HANDTRACKINGDEMO_API AHandTrackingGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHandTrackingGameMode();

	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	// -AutoShot diagnostic only: capture an early frame, then exit cleanly before the PC Meta XR Simulator's
	// D3D11 device-loss (which fires ~frame 19). No effect unless -AutoShot is on the command line.
	bool    bAutoShotActive = false;
	int32   AutoShotFrame   = 0;
	int32   AutoShotCount   = 0;
	FString AutoShotDir;
};
