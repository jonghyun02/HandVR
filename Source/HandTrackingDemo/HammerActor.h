#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HammerActor.generated.h"

class UStaticMeshComponent;
class UAudioComponent;
class USoundBase;
class UMaterialInstanceDynamic;
class APawn;

/**
 * 비동기 위협 자극 — 망치가 위에서 가상 손으로 내려치는 시퀀스.
 *
 * Phase:
 *  0. Idle      : 가상 손 위 50cm에서 대기
 *  1. WindUp    : 0.5초 위로 들었다가 (anticipation)
 *  2. Strike    : 0.15초간 빠르게 하강 → 가상 손 위치
 *  3. Impact    : 충격 (충격음 + 빨간 플래시 + 카메라 셰이크)
 *  4. Recovery  : 1초간 들어올렸다가 다시 idle
 *
 * 보고서: "현실에는 자극 없음, 가상 손에 망치가 내려치는 시각·청각 자극" — 시각만으로 위협 반응 유도.
 */
UCLASS()
class HANDTRACKINGDEMO_API AHammerActor : public AActor
{
	GENERATED_BODY()

public:
	AHammerActor();
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "RHI|Hammer")
	void BeginThreat(APawn* TargetPawn);

	UFUNCTION(BlueprintCallable, Category = "RHI|Hammer")
	void EndThreat();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RHI|Hammer")
	float CycleSec = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RHI|Hammer")
	USoundBase* ImpactSound = nullptr;

protected:
	UPROPERTY(VisibleAnywhere) USceneComponent* SceneRoot;
	UPROPERTY(VisibleAnywhere) UStaticMeshComponent* HandleMesh;
	UPROPERTY(VisibleAnywhere) UStaticMeshComponent* HeadMesh;
	UPROPERTY(VisibleAnywhere) UAudioComponent* ImpactAudio;

private:
	UPROPERTY() APawn* OwnerPawn = nullptr;
	bool bActive = false;
	float CycleTime = 0.f;
	bool bImpactedThisCycle = false;
};
