#include "WorldSetup.h"

#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
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

	// ChairMesh is an optional EditAnywhere property (assign in-editor if a dedicated SM_Chair is ever
	// imported). We deliberately do NOT ConstructorHelpers::FObjectFinder it: that asset does not ship,
	// and the finder logs a spurious "Failed to find" Error every launch. When ChairMesh is null,
	// BuildChair() builds a correctly-sized procedural chair instead.

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PaintBrushFinder(TEXT("/Game/Imported/Props/SM_PaintBrush.SM_PaintBrush"));
	if (PaintBrushFinder.Succeeded()) PaintBrushMesh = PaintBrushFinder.Object;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ClawHammerFinder(TEXT("/Game/Imported/Props/SM_ClawHammer.SM_ClawHammer"));
	if (ClawHammerFinder.Succeeded()) ClawHammerMesh = ClawHammerFinder.Object;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> GauntletHandFinder(TEXT("/Game/Imported/Props/SM_GauntletHand.SM_GauntletHand"));
	if (GauntletHandFinder.Succeeded()) GauntletHandMesh = GauntletHandFinder.Object;
}

void AWorldSetup::BeginPlay()
{
	Super::BeginPlay();

	// This actor is spawned at runtime by AHandTrackingGameMode::StartPlay, so BeginPlay — not
	// OnConstruction — is the correct once-only build hook. OnConstruction re-runs on editor edits,
	// actor moves, and RerunConstructionScripts, which would stack duplicate procedural components.
	BuildRoom();
	BuildDesk();
	BuildChair();
	BuildImportedPropShowcase();
	BuildLight();
}

// --- Sizing helpers ------------------------------------------------------------------------------------------------

FVector AWorldSetup::FitScalePerAxis(UStaticMesh* Mesh, FVector LocalTargetCm) const
{
	if (!Mesh) return FVector::OneVector;
	const FVector Full = Mesh->GetBounds().BoxExtent * 2.0f;
	return FVector(
		Full.X > KINDA_SMALL_NUMBER ? LocalTargetCm.X / Full.X : 1.0f,
		Full.Y > KINDA_SMALL_NUMBER ? LocalTargetCm.Y / Full.Y : 1.0f,
		Full.Z > KINDA_SMALL_NUMBER ? LocalTargetCm.Z / Full.Z : 1.0f);
}

float AWorldSetup::FitScaleUniform(UStaticMesh* Mesh, float TargetLongestCm) const
{
	if (!Mesh) return 1.0f;
	const FVector Full = Mesh->GetBounds().BoxExtent * 2.0f;
	const float Longest = FMath::Max3(Full.X, Full.Y, Full.Z);
	return Longest > KINDA_SMALL_NUMBER ? TargetLongestCm / Longest : 1.0f;
}

float AWorldSetup::GroundZForScale(UStaticMesh* Mesh, float ScaleZ) const
{
	if (!Mesh) return 0.0f;
	const FBoxSphereBounds B = Mesh->GetBounds();
	const float BottomLocal = B.Origin.Z - B.BoxExtent.Z;
	return -BottomLocal * ScaleZ;
}

// --- Spawning ------------------------------------------------------------------------------------------------------

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

	// Tint the procedural fallback so it renders in the intended greys (the default cube uses the grey-grid
	// WorldGridMaterial, which has no colour parameter). Engine BasicShapeMaterial exposes a "Color" param.
	if (!BasicShapeMaterial)
	{
		BasicShapeMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}
	if (BasicShapeMaterial)
	{
		if (UMaterialInstanceDynamic* MID = Comp->CreateDynamicMaterialInstance(0, BasicShapeMaterial))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Tint);
		}
	}
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

void AWorldSetup::BuildRoom()
{
	if (StudyRoomMesh)
	{
		// The imported classroom is ~21 m wide — cavernous for a 1-person seated experiment. Scale it down
		// by RoomScale so the user sits in a cozy room (per the report's simple single-room layout). Centre
		// its footprint on the user (scaled bounds origin) and drop its floor to Z = 0 regardless of pivot.
		const FBoxSphereBounds B = StudyRoomMesh->GetBounds();
		const float S = RoomScale;
		const FVector Loc(-B.Origin.X * S, -B.Origin.Y * S, -(B.Origin.Z - B.BoxExtent.Z) * S);
		SpawnMesh(TEXT("ImportedStudyRoom"), StudyRoomMesh, Loc, FRotator::ZeroRotator, FVector(S), true);
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
		// Real desk (에셋 사이즈.txt): width(Y)=72, depth(X)=47, height(Z)=73 cm.
		// Mesh native ≈ 299.6(X) × 114.1(Y) × 75.2(Z). Yaw 90° maps localX→worldY and localY→worldX,
		// so the long table axis becomes the desk's wide axis (least distortion). With yaw 90° the local
		// target is (worldWidth=72 on localX, worldDepth=47 on localY, height=73 on localZ).
		const FVector Scale   = FitScalePerAxis(DiningTableMesh, FVector(72.0f, 47.0f, 73.0f));
		const float   GZ      = GroundZForScale(DiningTableMesh, Scale.Z);   // top surface lands at z = 73
		const float   CenterX = DeskFrontEdgeX + 47.0f * 0.5f;              // front edge at +30 → center 53.5
		SpawnMesh(TEXT("ImportedDiningTable"), DiningTableMesh, FVector(CenterX, 0.0f, GZ), FRotator(0.0f, 90.0f, 0.0f), Scale, true);
		return;
	}

	// Procedural fallback desk: 47 (X) × 72 (Y) × 73 (Z); top thickness 2 cm; legs 5×5.
	constexpr float DeskDepth     = 47.0f;
	constexpr float DeskWidth     = 72.0f;
	constexpr float DeskHeight    = 73.0f;
	constexpr float TopThickness  = 2.0f;
	constexpr float LegSize       = 5.0f;
	constexpr float LegHeight     = DeskHeight - TopThickness; // 71 cm

	const float CenterX = DeskFrontEdgeX + DeskDepth * 0.5f;

	SpawnCube(TEXT("DeskTop"),
		FVector(CenterX, 0.0f, DeskHeight - TopThickness * 0.5f),
		FVector(DeskDepth, DeskWidth, TopThickness),
		FLinearColor(0.6f, 0.55f, 0.5f));

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

void AWorldSetup::BuildChair()
{
	// Pick up the imported chair at runtime if present (LoadObject returns null quietly when the asset is
	// missing — no CDO "Failed to find" Error, unlike a ConstructorHelpers finder). Editor-assigned ChairMesh wins.
	if (!ChairMesh)
	{
		ChairMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Imported/Furniture/SM_Chair.SM_Chair"));
	}

	if (ChairMesh)
	{
		// Fit the imported chair to a 44 × 44 × 75 cm envelope (footprint 44, backrest top 75). Grounded to floor.
		const FVector Scale = FitScalePerAxis(ChairMesh, FVector(44.0f, 44.0f, 75.0f));
		const float   GZ    = GroundZForScale(ChairMesh, Scale.Z);
		SpawnMesh(TEXT("ImportedChair"), ChairMesh, FVector(ChairCenterX, 0.0f, GZ), FRotator::ZeroRotator, Scale, true);
		return;
	}

	// Procedural chair: seat 44×44, seat-top z=42, backrest top z=75, seat thickness 3, legs 4×4.
	// Always built (even with the imported study room) so the user has a haptically-correct seat at the origin.
	constexpr float SeatSize        = 44.0f;
	constexpr float SeatTopZ        = 42.0f;
	constexpr float SeatThickness   = 3.0f;
	constexpr float LegSize         = 4.0f;
	constexpr float LegHeight       = SeatTopZ - SeatThickness;       // 39 cm
	constexpr float BackrestTopZ    = 75.0f;
	constexpr float BackrestHeight  = BackrestTopZ - SeatTopZ;        // 33 cm
	constexpr float BackrestThick   = 2.0f;

	const float CenterX = ChairCenterX;

	SpawnCube(TEXT("ChairSeat"),
		FVector(CenterX, 0.0f, SeatTopZ - SeatThickness * 0.5f),
		FVector(SeatSize, SeatSize, SeatThickness),
		FLinearColor(0.35f, 0.3f, 0.25f));

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

	SpawnCube(TEXT("ChairBack"),
		FVector(CenterX - SeatSize * 0.5f + BackrestThick * 0.5f, 0.0f, SeatTopZ + BackrestHeight * 0.5f),
		FVector(BackrestThick, SeatSize, BackrestHeight),
		FLinearColor(0.3f, 0.25f, 0.2f));
}

void AWorldSetup::BuildImportedPropShowcase()
{
	// Props resting on the desk top (z = 73) so they are visible on the menu screen, each sized to a
	// real-world graspable length via uniform fit (shape preserved).
	const float   CenterX  = DeskFrontEdgeX + 23.5f; // desk center 53.5
	const FVector TableTop = FVector(CenterX, 0.0f, 73.0f);

	// Intentionally EMPTY: the desk starts clear. The brush and hammer are NOT showcased here — they appear
	// only during their experiments (RHI brush-stroke, Hammer-threat), spawned by AExperimentManager.
	(void)TableTop;
}

void AWorldSetup::BuildLight()
{
	// Lock exposure FIRST. On Quest (mobile forward) the default auto-exposure (histogram/basic eye-adaptation)
	// reacts to whatever bright/dark patch dominates the view and was crushing this enclosed room toward black.
	// An UNBOUND APostProcessVolume forces MANUAL exposure everywhere, so the lights below map to a stable,
	// predictable image brightness regardless of where the user looks. AutoExposureBias in manual mode is EV
	// compensation in stops; ~1.0 lifts the room one stop (bright, not blown). Large values (e.g. 12) would
	// massively over-brighten to pure white — kept conservative here.
	if (UWorld* World = GetWorld())
	{
		if (APostProcessVolume* PPV = World->SpawnActor<APostProcessVolume>())
		{
			PPV->bUnbound = true;       // applies to the whole level, not just inside a box
			PPV->BlendWeight = 1.0f;

			FPostProcessSettings& S = PPV->Settings;
			S.bOverride_AutoExposureMethod = true;
			S.AutoExposureMethod = AEM_Manual;          // no eye-adaptation -> mobile can't crush the room
			S.bOverride_AutoExposureBias = true;
			S.AutoExposureBias = 1.0f;                   // neutral EV lift: bright but not white
		}
	}

	// Ambient base so surfaces not hit by a direct light are still visible (the room was reading near-black
	// because there was no ambient term). NOTE: an enclosed room captures ~black via SLS_CapturedScene, so we
	// DON'T rely on capture — a bright, non-black lower hemisphere gives a guaranteed constant ambient fill on
	// mobile forward. Bumped up a touch now that manual exposure no longer fights the fill light.
	USkyLightComponent* Sky = NewObject<USkyLightComponent>(this, TEXT("AmbientSky"));
	Sky->SetupAttachment(GetRootComponent());
	Sky->SetMobility(EComponentMobility::Movable);
	Sky->SourceType = ESkyLightSourceType::SLS_SpecifiedCubemap; // ignore the dark captured scene
	Sky->bLowerHemisphereIsBlack = false;
	Sky->SetLightColor(FLinearColor(0.6f, 0.61f, 0.65f));
	Sky->SetLowerHemisphereColor(FLinearColor(0.5f, 0.5f, 0.55f));
	Sky->SetIntensity(4.0f);
	Sky->RegisterComponent();

	// EVEN grid of equal MODERATE point lights at mid-height — NOT one hot key. A single very bright light blows
	// out the desk+floor; many equal lights give uniform fill with no hotspot. With manual exposure locked we can
	// push these harder (~4500cd) and widen attenuation so the fill actually reaches the walls and floor of the
	// ~9 m room. Big soft source radius keeps highlights gentle so nothing reads as a pure-white blowout.
	const float LightZ = 195.0f; // mid-height (desk top 73, ceiling ~247 at RoomScale) so light reaches the floor
	const FVector PointLocs[] = {
		FVector(DeskFrontEdgeX + 23.5f, 0.0f, 150.0f),   // gentle key over the desk (lower + soft)
		FVector( 90.0f,  140.0f, LightZ),
		FVector( 90.0f, -140.0f, LightZ),
		FVector(-130.0f, 140.0f, LightZ),
		FVector(-130.0f,-140.0f, LightZ),
		FVector(-150.0f,  0.0f,  LightZ),
	};
	const float PointIntensity[] = { 4000.0f, 4500.0f, 4500.0f, 4500.0f, 4500.0f, 4500.0f };
	for (int32 i = 0; i < 6; ++i)
	{
		UPointLightComponent* P = NewObject<UPointLightComponent>(this, *FString::Printf(TEXT("RoomLight_%d"), i));
		P->SetupAttachment(GetRootComponent());
		P->SetMobility(EComponentMobility::Movable);
		P->SetRelativeLocation(PointLocs[i]);
		P->SetIntensity(PointIntensity[i]);
		P->SetAttenuationRadius(1800.0f); // wide reach so the fill blankets the whole ~9 m room
		P->SetSourceRadius(60.0f);        // big soft source -> no harsh hotspot
		P->SetCastShadows(false);         // mobile perf + avoid harsh contrast
		P->RegisterComponent();
	}

	// Strong directional for an even base across every surface.
	UDirectionalLightComponent* Sun = NewObject<UDirectionalLightComponent>(this, TEXT("FillSun"));
	Sun->SetupAttachment(GetRootComponent());
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetWorldRotation(FRotator(-55.0f, 35.0f, 0.0f));
	Sun->SetIntensity(5.5f);
	Sun->SetCastShadows(false);
	Sun->RegisterComponent();
}
