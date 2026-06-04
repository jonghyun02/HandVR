#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "HandPawn.generated.h"

class UCameraComponent;
class UMotionControllerComponent;
class USphereComponent;
class USceneComponent;
class UOculusXRHandComponent;
class UStaticMeshComponent;
class UPointLightComponent;

/**
 * AHandPawn — Quest 3 VR pawn with hand tracking + per-hand visual offset for cognitive-distortion experiments.
 *
 * Hierarchy per hand:
 *   SceneRoot
 *     └── VRCamera
 *     └── LeftMC  (tracks real wrist pose from runtime)
 *           ├── LeftWristTrigger  (USphereComponent, tag "HandTouch" — drives 3D button presses by REAL hand)
 *           └── LeftHandOffset    (USceneComponent — ExperimentManager nudges this for visual displacement)
 *                 └── LeftHand    (UOculusXRHandComponent — articulated hand mesh rendered at REAL+offset)
 *   (right hand mirrors)
 *
 * Tracking origin is LocalFloor (sitting/standing OK). SimultaneousHandsAndControllers is enabled so the user
 * can grip a Quest controller and the hand mesh keeps reading finger poses; per the report it's also fine if
 * the hand mesh disappears when a controller is held — that's an acceptable failure mode for the experiment.
 */
UCLASS()
class HANDTRACKINGDEMO_API AHandPawn : public APawn
{
	GENERATED_BODY()

public:
	AHandPawn();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Set WORLD-space visual offset for each hand (cm). +Y = user's right. Applied every tick on top of latency. */
	UFUNCTION(BlueprintCallable, Category = "Hands")
	void SetVisualOffset(const FVector& LeftOffset, const FVector& RightOffset);

	/** Temporal mismatch: render the SEEN hand this many ms BEHIND the real wrist (0 = realtime). */
	UFUNCTION(BlueprintCallable, Category = "Hands")
	void SetVisualLatencyMs(float Ms);

	/** Shorthand: reset both hands to no offset AND no latency. */
	UFUNCTION(BlueprintCallable, Category = "Hands")
	void ResetVisualOffsets();

	/** Show / hide both rendered hand meshes (real hand still tracked for button overlap). */
	UFUNCTION(BlueprintCallable, Category = "Hands")
	void SetHandsVisible(bool bVisible);

	/** World-space position of REAL (untouched-by-offset) wrist. Used for ghost-hand reveal and button overlap. */
	UFUNCTION(BlueprintCallable, Category = "Hands")
	FVector GetRealWristLocation(bool bRightHand) const;

	/** World-space position of VISUAL wrist (after offset). Used by experiments that anchor props to the seen hand. */
	UFUNCTION(BlueprintCallable, Category = "Hands")
	FVector GetVisualWristLocation(bool bRightHand) const;

	// --- Components ------------------------------------------------------------------------------------------------
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR") USceneComponent*           SceneRoot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR") UCameraComponent*          VRCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR") UMotionControllerComponent* LeftMC;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR") USphereComponent*          LeftWristTrigger;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR") USceneComponent*           LeftHandOffset;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR") UOculusXRHandComponent*    LeftHand;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR") UMotionControllerComponent* RightMC;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR") USphereComponent*          RightWristTrigger;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR") USceneComponent*           RightHandOffset;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR") UOculusXRHandComponent*    RightHand;

private:
	// One recorded real-wrist sample (world space) used to replay the seen hand with a temporal delay.
	struct FWristSample { double Time; FVector Left; FVector Right; };
	TArray<FWristSample> WristHistory;   // ring (append each tick, trim older than kMaxHistorySec)

	FVector LeftOffsetWorld  = FVector::ZeroVector;  // current world-space visual offset (set by ExperimentManager)
	FVector RightOffsetWorld = FVector::ZeroVector;
	float   VisualLatencySec = 0.0f;                 // seen-hand delay (s)

	// Returns the buffered real-wrist world location at (now - VisualLatencySec); falls back to the live MC
	// location when latency is 0 or the history is too short. Linear-interpolated between bracketing samples.
	FVector SampleDelayedWrist(bool bRight, double Now) const;
	void    ApplyVisualState();  // recompute both HandOffset transforms from offset + latency (called each tick)
};
