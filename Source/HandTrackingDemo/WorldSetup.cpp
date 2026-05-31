#include "WorldSetup.h"

#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AWorldSetup::AWorldSetup()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded()) CubeMesh = CubeFinder.Object;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> StudyRoomFinder(TEXT("/Game/Imported/Environment/SM_StudyRoom.SM_StudyRoom"));
	if (StudyRoomFinder.Succeeded()) StudyRoomMesh = StudyRoomFinder.Object;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DiningTableFinder(TEXT("/Game/Imported/Furniture/SM_DiningTable.SM_DiningTable"));
	if (DiningTableFinder.Succeeded()) DiningTableMesh = DiningTableFinder.Object;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PaintBrushFinder(TEXT("/Game/Imported/Props/SM_PaintBrush.SM_PaintBrush"));
	if (PaintBrushFinder.Succeeded()) PaintBrushMesh = PaintBrushFinder.Object;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ClawHammerFinder(TEXT("/Game/Imported/Props/SM_ClawHammer.SM_ClawHammer"));
	if (ClawHammerFinder.Succeeded()) ClawHammerMesh = ClawHammerFinder.Object;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> GauntletHandFinder(TEXT("/Game/Imported/Props/SM_GauntletHand.SM_GauntletHand"));
	if (GauntletHandFinder.Succeeded()) GauntletHandMesh = GauntletHandFinder.Object;
}

void AWorldSetup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	BuildRoom();
	BuildDesk();
	BuildImportedPropShowcase();
	BuildChair();
	BuildLight();
}

void AWorldSetup::BeginPlay()
{
	Super::BeginPlay();
}

UStaticMeshComponent* AWorldSetup::SpawnCube(const FString& Name, FVector LocalLocation, FVector ScaleCm, FLinearColor Tint)
{
	// Engine basic cube is 100 cm on each side, so scale = size_cm / 100.
	UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this, FName(*Name));
	Comp->SetupAttachment(GetRootComponent());
	if (CubeMesh) Comp->SetStaticMesh(CubeMesh);
	Comp->SetRelativeLocation(LocalLocation);
	Comp->SetRelativeScale3D(ScaleCm / 100.0f);
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->RegisterComponent();

	// Default cube uses WorldGridMaterial which doesn't expose a color param. We leave it as is;
	// the team's imported FBX will replace these visually anyway. Tint param kept for future material swaps.
	(void)Tint;
	return Comp;
}

UStaticMeshComponent* AWorldSetup::SpawnMesh(const FString& Name, UStaticMesh* Mesh, FVector LocalLocation, FRotator LocalRotation, FVector Scale, bool bCollide)
{
	if (!Mesh) return nullptr;

	UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this, FName(*Name));
	Comp->SetupAttachment(GetRootComponent());
	Comp->SetStaticMesh(Mesh);
	Comp->SetRelativeLocation(LocalLocation);
	Comp->SetRelativeRotation(LocalRotation);
	Comp->SetRelativeScale3D(Scale);
	Comp->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	Comp->RegisterComponent();
	return Comp;
}

void AWorldSetup::LoadImportedEnvironment()
{
	if (!StudyRoomMesh)
	{
		StudyRoomMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Imported/Environment/SM_StudyRoom.SM_StudyRoom"));
	}
	if (StudyRoomMesh)
	{
		SpawnMesh(TEXT("ImportedStudyRoomRuntime"), StudyRoomMesh, FVector(170.0f, 0.0f, 0.0f), FRotator(0.0f, 180.0f, 0.0f), FVector(1.0f), false);
	}

	if (!DiningTableMesh)
	{
		DiningTableMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Imported/Furniture/SM_DiningTable.SM_DiningTable"));
	}
	if (DiningTableMesh)
	{
		SpawnMesh(TEXT("ImportedDiningTableRuntime"), DiningTableMesh, FVector(53.5f, 0.0f, 0.0f), FRotator::ZeroRotator, FVector(1.0f), false);
	}
}

void AWorldSetup::BuildRoom()
{
	if (StudyRoomMesh)
	{
		SpawnMesh(TEXT("ImportedStudyRoom"), StudyRoomMesh, FVector(0.0f, 0.0f, 0.0f), FRotator::ZeroRotator, FVector(1.0f), true);
		return;
	}

	const float HE = RoomHalfExtent;
	const float Z  = CeilingHeight;

	// Floor — 1 cm slab centered at z = -0.5 so its top is exactly z = 0.
	SpawnCube(TEXT("Floor"),   FVector(0.0f, 0.0f, -0.5f),            FVector(HE * 2, HE * 2, 1.0f),   FLinearColor(0.3f, 0.3f, 0.3f));
	SpawnCube(TEXT("Ceiling"), FVector(0.0f, 0.0f, Z + 0.5f),         FVector(HE * 2, HE * 2, 1.0f),   FLinearColor(0.5f, 0.5f, 0.5f));
	// 4 walls — 5 cm thick, full ceiling height.
	SpawnCube(TEXT("WallN"),   FVector( HE + 2.5f, 0.0f, Z * 0.5f),   FVector(5.0f,  HE * 2, Z),       FLinearColor(0.45f, 0.45f, 0.45f));
	SpawnCube(TEXT("WallS"),   FVector(-HE - 2.5f, 0.0f, Z * 0.5f),   FVector(5.0f,  HE * 2, Z),       FLinearColor(0.45f, 0.45f, 0.45f));
	SpawnCube(TEXT("WallE"),   FVector(0.0f,  HE + 2.5f, Z * 0.5f),   FVector(HE * 2, 5.0f, Z),        FLinearColor(0.45f, 0.45f, 0.45f));
	SpawnCube(TEXT("WallW"),   FVector(0.0f, -HE - 2.5f, Z * 0.5f),   FVector(HE * 2, 5.0f, Z),        FLinearColor(0.45f, 0.45f, 0.45f));
}

void AWorldSetup::BuildDesk()
{
	if (DiningTableMesh)
	{
		SpawnMesh(TEXT("ImportedDiningTable"), DiningTableMesh, FVector(53.5f, 0.0f, 0.0f), FRotator::ZeroRotator, FVector(1.0f), true);
		return;
	}

	// Desk dims: 47 (X, depth) × 72 (Y, width) × 73 (Z, height); top thickness = 2 cm; legs 5×5.
	constexpr float DeskDepth     = 47.0f;
	constexpr float DeskWidth     = 72.0f;
	constexpr float DeskHeight    = 73.0f;
	constexpr float TopThickness  = 2.0f;
	constexpr float LegSize       = 5.0f;
	constexpr float LegHeight     = DeskHeight - TopThickness; // 71 cm

	const float CenterX = DeskFrontEdgeX + DeskDepth * 0.5f;

	// Top slab — top surface lands exactly at z = 73.
	SpawnCube(TEXT("DeskTop"),
		FVector(CenterX, 0.0f, DeskHeight - TopThickness * 0.5f),
		FVector(DeskDepth, DeskWidth, TopThickness),
		FLinearColor(0.6f, 0.55f, 0.5f));

	// 4 legs — placed at desk corners, centered vertically below the slab.
	const float LegInset = LegSize * 0.5f;
	const float LegZ     = LegHeight * 0.5f;
	const float HalfD    = DeskDepth * 0.5f - LegInset;
	const float HalfW    = DeskWidth * 0.5f - LegInset;

	const FVector Corners[4] = {
		FVector(CenterX + HalfD, +HalfW, LegZ),
		FVector(CenterX + HalfD, -HalfW, LegZ),
		FVector(CenterX - HalfD, +HalfW, LegZ),
		FVector(CenterX - HalfD, -HalfW, LegZ),
	};
	for (int i = 0; i < 4; ++i)
	{
		SpawnCube(FString::Printf(TEXT("DeskLeg_%d"), i),
			Corners[i], FVector(LegSize, LegSize, LegHeight),
			FLinearColor(0.3f, 0.25f, 0.2f));
	}
}

void AWorldSetup::BuildImportedPropShowcase()
{
	const FVector TableTop = FVector(53.5f, 0.0f, 82.0f);

	if (GauntletHandMesh)
	{
		SpawnMesh(TEXT("ImportedGauntletHand_Showcase"), GauntletHandMesh,
			TableTop + FVector(0.0f, -18.0f, 2.0f),
			FRotator(0.0f, 90.0f, 0.0f),
			FVector(0.12f), false);
	}

	if (PaintBrushMesh)
	{
		SpawnMesh(TEXT("ImportedPaintBrush_Showcase"), PaintBrushMesh,
			TableTop + FVector(0.0f, 0.0f, 5.0f),
			FRotator(0.0f, 0.0f, 90.0f),
			FVector(0.12f), false);
	}

	if (ClawHammerMesh)
	{
		SpawnMesh(TEXT("ImportedClawHammer_Showcase"), ClawHammerMesh,
			TableTop + FVector(0.0f, 18.0f, 5.0f),
			FRotator(0.0f, 90.0f, 0.0f),
			FVector(0.12f), false);
	}
}

void AWorldSetup::BuildChair()
{
	if (StudyRoomMesh)
	{
		return;
	}

	// Chair: seat 44×44, seat-top at z=42, backrest top at z=75, seat thickness 3, legs 4×4.
	constexpr float SeatSize        = 44.0f;
	constexpr float SeatTopZ        = 42.0f;
	constexpr float SeatThickness   = 3.0f;
	constexpr float LegSize         = 4.0f;
	constexpr float LegHeight       = SeatTopZ - SeatThickness;       // 39 cm
	constexpr float BackrestTopZ    = 75.0f;
	constexpr float BackrestHeight  = BackrestTopZ - SeatTopZ;        // 33 cm
	constexpr float BackrestThick   = 2.0f;

	const float CenterX = ChairCenterX;

	// Seat — seat top z = 42 cm.
	SpawnCube(TEXT("ChairSeat"),
		FVector(CenterX, 0.0f, SeatTopZ - SeatThickness * 0.5f),
		FVector(SeatSize, SeatSize, SeatThickness),
		FLinearColor(0.35f, 0.3f, 0.25f));

	// 4 legs.
	const float Half = SeatSize * 0.5f - LegSize * 0.5f;
	const FVector Corners[4] = {
		FVector(CenterX + Half, +Half, LegHeight * 0.5f),
		FVector(CenterX + Half, -Half, LegHeight * 0.5f),
		FVector(CenterX - Half, +Half, LegHeight * 0.5f),
		FVector(CenterX - Half, -Half, LegHeight * 0.5f),
	};
	for (int i = 0; i < 4; ++i)
	{
		SpawnCube(FString::Printf(TEXT("ChairLeg_%d"), i),
			Corners[i], FVector(LegSize, LegSize, LegHeight),
			FLinearColor(0.2f, 0.18f, 0.15f));
	}

	// Backrest — vertical slab at the back of the seat (negative X side, since pawn faces +X).
	SpawnCube(TEXT("ChairBack"),
		FVector(CenterX - SeatSize * 0.5f + BackrestThick * 0.5f, 0.0f, SeatTopZ + BackrestHeight * 0.5f),
		FVector(BackrestThick, SeatSize, BackrestHeight),
		FLinearColor(0.3f, 0.25f, 0.2f));
}

void AWorldSetup::BuildLight()
{
	UPointLightComponent* Light = NewObject<UPointLightComponent>(this, TEXT("OverheadLight"));
	Light->SetupAttachment(GetRootComponent());
	Light->SetRelativeLocation(FVector(DeskFrontEdgeX + 23.5f, 0.0f, CeilingHeight - 20.0f));
	Light->SetIntensity(3500.0f);
	Light->SetAttenuationRadius(600.0f);
	Light->SetSourceRadius(8.0f);
	Light->SetCastShadows(true);
	Light->RegisterComponent();
}
