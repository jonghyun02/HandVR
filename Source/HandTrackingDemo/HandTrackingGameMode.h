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
};
