#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldSetup.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UStaticMesh;

/**
 * AWorldSetup — minimal achromatic room with a desk + chair built from Engine basic cubes,
 * sized to the real-world props the team will sit at:
 *
 *   Desk:  72 (W, Y)  × 47 (D, X)  × 73 (H, Z) cm, top thickness 2 cm, 4 legs.
 *   Chair: seat 44 × 44 × ~3 cm, seat top at Z = 42 cm; backrest top at Z = 75 cm; 4 legs.
 *
 * Tracking origin is LocalFloor so Z = 0 is the user's floor. The desk's front edge is placed
 * at +X = DeskFrontEdgeX so it is reachable from the chair without leaning.
 *
 * This is procedural so the project runs out of the box. Once the team imports their real FBX
 * meshes from assets/ they can either hide this actor or swap the StaticMesh references via
 * the exposed component references.
 */
UCLASS()
class HANDTRACKINGDEMO_API AWorldSetup : public AActor
{
	GENERATED_BODY()

public:
	AWorldSetup();

	UPROPERTY(EditAnywhere, Category = "Layout") float DeskFrontEdgeX  = 30.0f; // cm in front of pawn origin
	UPROPERTY(EditAnywhere, Category = "Layout") float ChairCenterX    = 0.0f;
	UPROPERTY(EditAnywhere, Category = "Layout") float RoomHalfExtent  = 175.0f; // 3.5m room
	UPROPERTY(EditAnywhere, Category = "Layout") float CeilingHeight   = 250.0f;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

private:
	void BuildRoom();
	void BuildDesk();
	void BuildChair();
	void BuildLight();
	void BuildImportedPropShowcase();
	void LoadImportedEnvironment();

	UStaticMeshComponent* SpawnCube(const FString& Name, FVector LocalLocation, FVector ScaleCm, FLinearColor Tint);
	UStaticMeshComponent* SpawnMesh(const FString& Name, UStaticMesh* Mesh, FVector LocalLocation, FRotator LocalRotation, FVector Scale, bool bCollide = false);

	UStaticMesh* CubeMesh = nullptr;
	UStaticMesh* StudyRoomMesh = nullptr;
	UStaticMesh* DiningTableMesh = nullptr;
	UStaticMesh* PaintBrushMesh = nullptr;
	UStaticMesh* ClawHammerMesh = nullptr;
	UStaticMesh* GauntletHandMesh = nullptr;
};
