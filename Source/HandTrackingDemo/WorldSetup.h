#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldSetup.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UStaticMesh;
class UMaterialInterface;

/**
 * AWorldSetup — builds the experiment environment from the team's imported FBX assets, sized to the
 * real-world props the team sits at (에셋 사이즈.txt):
 *
 *   Desk:  72 (W, Y) × 47 (D, X) × 73 (H, Z) cm, top surface at Z = 73.
 *   Chair: 44 × 44 footprint, seat-top at Z = 42 cm, backrest top at Z = 75 cm.
 *
 * All imported meshes are fitted to those targets at runtime via their measured bounds
 * (FitScalePerAxis / FitScaleUniform) rather than hardcoded scale guesses, so sizing stays correct
 * even if an asset is re-imported at a different unit scale. If an imported mesh is missing, a
 * correctly-sized procedural cube version is built so the project still runs.
 *
 * Tracking origin is LocalFloor, so Z = 0 is the user's real floor and meshes are grounded to it.
 */
UCLASS()
class HANDTRACKINGDEMO_API AWorldSetup : public AActor
{
	GENERATED_BODY()

public:
	AWorldSetup();

	UPROPERTY(EditAnywhere, Category = "Layout") float DeskFrontEdgeX  = 30.0f; // cm in front of pawn origin
	UPROPERTY(EditAnywhere, Category = "Layout") float ChairCenterX    = 0.0f;
	UPROPERTY(EditAnywhere, Category = "Layout") float RoomHalfExtent  = 175.0f; // 3.5m room (procedural fallback)
	UPROPERTY(EditAnywhere, Category = "Layout") float CeilingHeight   = 250.0f;
	// Downscale factor for the imported ~21 m StudyRoom so a seated user gets a cozy room, not a hall.
	UPROPERTY(EditAnywhere, Category = "Layout") float RoomScale       = 0.42f;

protected:
	virtual void BeginPlay() override;

private:
	void BuildRoom();
	void BuildDesk();
	void BuildChair();
	void BuildLight();
	void BuildImportedPropShowcase();

	UStaticMeshComponent* SpawnCube(const FString& Name, FVector LocalLocation, FVector ScaleCm, FLinearColor Tint);
	UStaticMeshComponent* SpawnMesh(const FString& Name, UStaticMesh* Mesh, FVector LocalLocation, FRotator LocalRotation, FVector Scale, bool bCollide = false);

	/** Per-axis scale that fits the mesh's local bounding box to LocalTargetCm (exact box match; may distort). */
	FVector FitScalePerAxis(UStaticMesh* Mesh, FVector LocalTargetCm) const;
	/** Uniform scale that fits the mesh's longest local axis to TargetLongestCm (preserves shape). */
	float   FitScaleUniform(UStaticMesh* Mesh, float TargetLongestCm) const;
	/** Actor Z so the scaled mesh's lowest point lands exactly on Z = 0 (grounded to the floor). */
	float   GroundZForScale(UStaticMesh* Mesh, float ScaleZ) const;

	// Optional dedicated chair mesh — assign in-editor if a real SM_Chair is ever imported; otherwise
	// BuildChair() builds a correctly-sized procedural chair. EditAnywhere (not a ConstructorHelpers
	// finder) so a missing asset does not spam a launch-time "Failed to find" Error.
	UPROPERTY(EditAnywhere, Category = "Props") UStaticMesh* ChairMesh = nullptr;

	// UPROPERTY so the GC keeps these alive for the actor's lifetime (correctness-by-construction; matches
	// AExperimentManager's mesh members). They are assigned from ConstructorHelpers/LoadObject and read in
	// BeginPlay; marking them tracked removes any reliance on the source packages staying rooted.
	UPROPERTY() UStaticMesh* CubeMesh = nullptr;
	UPROPERTY() UMaterialInterface* BasicShapeMaterial = nullptr; // tintable material for the procedural fallback cubes
	UPROPERTY() UStaticMesh* StudyRoomMesh = nullptr;
	UPROPERTY() UStaticMesh* DiningTableMesh = nullptr;
	UPROPERTY() UStaticMesh* PaintBrushMesh = nullptr;
	UPROPERTY() UStaticMesh* ClawHammerMesh = nullptr;
	UPROPERTY() UStaticMesh* GauntletHandMesh = nullptr;
};
