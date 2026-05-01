#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RubberHandGameMode.generated.h"

class ALabEnvironmentActor;

/**
 * RHI 실험 GameMode.
 * - DefaultPawnClass = ARubberHandPawn
 * - BeginPlay에서 ALabEnvironmentActor 스폰 (실험실 룸 + 테이블 + 의자 + 조명)
 */
UCLASS()
class HANDTRACKINGDEMO_API ARubberHandGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARubberHandGameMode();

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RHI|Lab")
	TSubclassOf<ALabEnvironmentActor> LabEnvironmentClass;

	/** 실험을 BeginPlay 직후 자동으로 시작할지. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RHI|Experiment")
	bool bAutoStartExperiment = false;

	/** 자동 시작 시 딜레이(초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RHI|Experiment", meta = (ClampMin = 0.0))
	float AutoStartDelaySec = 5.0f;

private:
	UPROPERTY()
	ALabEnvironmentActor* SpawnedLab = nullptr;

	FTimerHandle AutoStartTimer;
	void TryAutoStartExperiment();
};
