#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BrushActor.generated.h"

class UStaticMeshComponent;
class UAudioComponent;
class USoundBase;
class APawn;

/**
 * 동기 터치 도구 — 가상 손 표면을 위에서 일정한 패턴으로 쓸어주는 자극.
 *
 * 동작:
 *  - bSynchronized=true : 가상 손 표면 위 일정 거리에서 좌우로 왕복하며 트레일/사운드.
 *                         보고서 명세대로 시각-촉각 동기화 = 50ms 이내 타이밍 일치를 가정.
 *  - bSynchronized=false: 위치만 동일, 그러나 의도적으로 잘못된 부위(예: 검지 끝 대신 약지 끝)를
 *                         쓸어줌 → 시각-촉각 불일치 (Phase 2 Case5/6용)
 *
 * Spatial audio: AudioComponent->SetbSpatialize(true), 가상 손 위치에서 소리.
 */
UCLASS()
class HANDTRACKINGDEMO_API ABrushActor : public AActor
{
	GENERATED_BODY()

public:
	ABrushActor();

	virtual void Tick(float DeltaSeconds) override;

	/** 자극 시작. */
	UFUNCTION(BlueprintCallable, Category = "RHI|Brush")
	void BeginStroking(APawn* TargetPawn, bool bSync);

	/** 자극 종료. */
	UFUNCTION(BlueprintCallable, Category = "RHI|Brush")
	void EndStroking();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RHI|Brush")
	float StrokeAmplitudeCm = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RHI|Brush")
	float StrokeFrequencyHz = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RHI|Brush")
	USoundBase* StrokeSound = nullptr;

protected:
	UPROPERTY(VisibleAnywhere) USceneComponent* SceneRoot;
	UPROPERTY(VisibleAnywhere) UStaticMeshComponent* HandleMesh;
	UPROPERTY(VisibleAnywhere) UStaticMeshComponent* TipMesh;
	UPROPERTY(VisibleAnywhere) UAudioComponent* StrokeAudio;

private:
	UPROPERTY() APawn* OwnerPawn = nullptr;
	bool bSynchronized = true;
	float ElapsedTime = 0.f;
	bool bActive = false;
};
