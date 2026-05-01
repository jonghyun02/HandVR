#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "RubberHandTypes.h"
#include "RubberHandPawn.generated.h"

class UCameraComponent;
class UMotionControllerComponent;
class USceneComponent;
class UOculusXRHandComponent;
class UPoseableMeshComponent;
class UExperimentManagerComponent;

/**
 * Rubber Hand Illusion VR Pawn.
 *
 * 구조:
 *  - VRCamera (HMD lock)
 *  - LeftMC / RightMC                       — MotionController, 시스템 손 위치 추적
 *  - LeftHandTracker / RightHandTracker     — UOculusXRHandComponent, OpenXR 본 데이터 소스 (시각적 hidden)
 *  - LeftHandSynth   / RightHandSynth       — UPoseableMeshComponent, 화면에 보이는 "가상 손".
 *                                             매 틱 Tracker 본을 ring buffer에 push, 지연된 본을
 *                                             공간 오프셋 적용해서 Synth로 복사 → RHI 핵심 메커니즘
 *
 * AHandPawn과 차이점:
 *  - 룬 드로잉 컴포넌트 없음 (실험과 무관)
 *  - 합성 손 + ring buffer + offset (가짜손 착각 유도)
 *  - ExperimentManager 컴포넌트 부착
 */
UCLASS(Blueprintable, BlueprintType)
class HANDTRACKINGDEMO_API ARubberHandPawn : public APawn
{
	GENERATED_BODY()

public:
	ARubberHandPawn();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	void RecordTrackerSnapshot(UOculusXRHandComponent* Tracker, TArray<FRHIBoneSnapshot>& Buffer, double NowSec) const;
	void ApplyDelayedSnapshotToSynth(const TArray<FRHIBoneSnapshot>& Buffer, UPoseableMeshComponent* Synth, double NowSec) const;

public:
	// ----- 컴포넌트 -----
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RHI|VR")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RHI|VR")
	UCameraComponent* VRCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RHI|VR")
	UMotionControllerComponent* LeftMC;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RHI|VR")
	UMotionControllerComponent* RightMC;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RHI|HandTracking")
	UOculusXRHandComponent* LeftHandTracker;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RHI|HandTracking")
	UOculusXRHandComponent* RightHandTracker;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RHI|Synthetic")
	UPoseableMeshComponent* LeftHandSynth;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RHI|Synthetic")
	UPoseableMeshComponent* RightHandSynth;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RHI|Experiment")
	UExperimentManagerComponent* ExperimentManager;

	// ----- 실험 파라미터 (매 틱 적용) -----

	/** 가상 손에 적용되는 공간 오프셋 (cm). 양수면 사용자 forward 방향으로 떨어짐. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RHI|Offset", meta = (ClampMin = 0.0, ClampMax = 60.0))
	float SpatialOffsetCm = 0.0f;

	/** 공간 오프셋이 적용되는 방향 (Pawn local space). 기본 = HMD forward. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RHI|Offset")
	FVector SpatialOffsetDirection = FVector(1.f, 0.f, 0.f);

	/** 시간 지연 (ms). ring buffer에서 N ms 전 본을 가져옴. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RHI|Delay", meta = (ClampMin = 0.0, ClampMax = 1000.0))
	float TemporalDelayMs = 0.0f;

	/** 합성 손 가시성 (실험 시작 전엔 끔). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RHI|Synthetic")
	bool bShowSyntheticHand = true;

	UFUNCTION(BlueprintCallable, Category = "RHI|Offset")
	void SetSpatialOffsetCm(float NewValue) { SpatialOffsetCm = FMath::Clamp(NewValue, 0.f, 60.f); }

	UFUNCTION(BlueprintCallable, Category = "RHI|Delay")
	void SetTemporalDelayMs(float NewValue) { TemporalDelayMs = FMath::Clamp(NewValue, 0.f, 1000.f); }

	UFUNCTION(BlueprintCallable, Category = "RHI|Synthetic")
	void SetSyntheticHandVisible(bool bVisible);

private:
	// 본 트랜스폼 ring buffer. 최대 1초 기록.
	TArray<FRHIBoneSnapshot> LeftBuffer;
	TArray<FRHIBoneSnapshot> RightBuffer;

	static constexpr float MaxBufferDurationSec = 1.0f;

	// 컨트롤 패널 poke 검출 — 직전 틱 hit ID로 enter 검출(엣지 트리거).
	int32 LastHitButton = 0;
	double LastButtonHitTime = 0.0;
	static constexpr float ButtonCooldownSec = 0.6f;

	FVector GetIndexTipWorldPos(class UOculusXRHandComponent* Tracker) const;
};
