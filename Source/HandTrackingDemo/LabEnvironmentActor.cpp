#include "LabEnvironmentActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ALabEnvironmentActor::ALabEnvironmentActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// 메시 + 머티리얼은 생성자에서만 ConstructorHelpers::FObjectFinder로 안전하게 로드.
	// AddBox 등 런타임 함수에서 FObjectFinder 호출하면 fatal error 발생.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		CubeMesh = CubeMeshFinder.Object;
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WhiteMat(TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
	if (WhiteMat.Succeeded())
	{
		WhiteMaterial = WhiteMat.Object;
		TableMaterial = WhiteMat.Object;
	}
}

void ALabEnvironmentActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildEnvironment();
}

void ALabEnvironmentActor::RebuildEnvironment()
{
	ClearSpawned();
	BuildRoom();
	BuildTable();
	BuildChair();
	BuildLights();
	BuildControlPanel();
}

void ALabEnvironmentActor::ClearSpawned()
{
	for (UStaticMeshComponent* M : SpawnedMeshes)
	{
		if (M) { M->DestroyComponent(); }
	}
	SpawnedMeshes.Reset();
	if (CeilingLight) { CeilingLight->DestroyComponent(); CeilingLight = nullptr; }
	if (AmbientFill)  { AmbientFill->DestroyComponent();  AmbientFill  = nullptr; }

	// 컨트롤 패널 정리.
	for (UStaticMeshComponent* B : CaseButtons)    { if (B) { B->DestroyComponent(); } }
	for (UTextRenderComponent* L : CaseButtonLabels) { if (L) { L->DestroyComponent(); } }
	CaseButtons.Reset();
	CaseButtonLabels.Reset();
	if (StatusText) { StatusText->DestroyComponent(); StatusText = nullptr; }
}

UStaticMeshComponent* ALabEnvironmentActor::AddBox(FName Name, const FVector& LocalLocation, const FVector& BoxSizeCm, UMaterialInterface* Mat)
{
	UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(this, Name);
	MC->SetupAttachment(SceneRoot);
	MC->RegisterComponent();

	// 생성자에서 캐싱한 CubeMesh 사용. ConstructorHelpers::FObjectFinder는 여기서 호출 불가.
	if (CubeMesh)
	{
		MC->SetStaticMesh(CubeMesh);
	}

	// /Engine/BasicShapes/Cube 는 100uu × 100uu × 100uu — UE에서 1uu = 1cm
	MC->SetRelativeLocation(LocalLocation);
	MC->SetRelativeScale3D(BoxSizeCm / 100.f);
	if (Mat) { MC->SetMaterial(0, Mat); }
	MC->SetMobility(EComponentMobility::Static);
	MC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MC->SetCollisionResponseToAllChannels(ECR_Block);

	SpawnedMeshes.Add(MC);
	return MC;
}

void ALabEnvironmentActor::BuildRoom()
{
	const FVector S = RoomInnerSizeCm;
	const float T = WallThicknessCm;

	// 바닥
	AddBox(TEXT("Floor"), FVector(0, 0, -T * 0.5f), FVector(S.X + T * 2, S.Y + T * 2, T), WhiteMaterial);
	// 천장
	AddBox(TEXT("Ceiling"), FVector(0, 0, S.Z + T * 0.5f), FVector(S.X + T * 2, S.Y + T * 2, T), WhiteMaterial);
	// 벽 4면 (룸 중심을 (0,0,Z/2)로)
	AddBox(TEXT("Wall_X+"), FVector( S.X * 0.5f + T * 0.5f, 0, S.Z * 0.5f), FVector(T, S.Y, S.Z), WhiteMaterial);
	AddBox(TEXT("Wall_X-"), FVector(-S.X * 0.5f - T * 0.5f, 0, S.Z * 0.5f), FVector(T, S.Y, S.Z), WhiteMaterial);
	AddBox(TEXT("Wall_Y+"), FVector(0,  S.Y * 0.5f + T * 0.5f, S.Z * 0.5f), FVector(S.X + T * 2, T, S.Z), WhiteMaterial);
	AddBox(TEXT("Wall_Y-"), FVector(0, -S.Y * 0.5f - T * 0.5f, S.Z * 0.5f), FVector(S.X + T * 2, T, S.Z), WhiteMaterial);
}

void ALabEnvironmentActor::BuildTable()
{
	const FVector TS = RealTableSizeCm;
	const FVector C  = TableOffsetCm;
	const float TopH = 4.f;     // 상판 두께
	const float LegT = 5.f;     // 다리 굵기

	// 상판
	AddBox(TEXT("Table_Top"),
		FVector(C.X, C.Y, C.Z + TS.Z - TopH * 0.5f),
		FVector(TS.X, TS.Y, TopH),
		TableMaterial);

	// 다리 4개
	const float HX = TS.X * 0.5f - LegT * 0.5f;
	const float HY = TS.Y * 0.5f - LegT * 0.5f;
	const float LegH = TS.Z - TopH;
	const float LegZ = C.Z + LegH * 0.5f;
	AddBox(TEXT("Table_Leg1"), FVector(C.X + HX, C.Y + HY, LegZ), FVector(LegT, LegT, LegH), TableMaterial);
	AddBox(TEXT("Table_Leg2"), FVector(C.X - HX, C.Y + HY, LegZ), FVector(LegT, LegT, LegH), TableMaterial);
	AddBox(TEXT("Table_Leg3"), FVector(C.X + HX, C.Y - HY, LegZ), FVector(LegT, LegT, LegH), TableMaterial);
	AddBox(TEXT("Table_Leg4"), FVector(C.X - HX, C.Y - HY, LegZ), FVector(LegT, LegT, LegH), TableMaterial);
}

void ALabEnvironmentActor::BuildChair()
{
	// 의자는 책상의 짧은 변(Y축 +) 쪽에 배치 — 사용자가 테이블 짧은 변에 앉아 긴 변을 마주봄.
	// gap=25cm: 의자 좌석(45cm) 절반 + 책상 모서리 → 무릎이 책상 밑으로 살짝 들어가 자연스러움.
	const FVector S = ChairSeatSizeCm;
	const float ChairX = TableOffsetCm.X;
	const float ChairY = TableOffsetCm.Y + RealTableSizeCm.Y * 0.5f + 25.f;
	const float SeatZ  = S.Z;
	const float SeatTopH = 5.f;

	AddBox(TEXT("Chair_Seat"),
		FVector(ChairX, ChairY, SeatZ - SeatTopH * 0.5f),
		FVector(S.X, S.Y, SeatTopH),
		WhiteMaterial);

	// 다리 4개
	const float HX = S.X * 0.5f - 3.f;
	const float HY = S.Y * 0.5f - 3.f;
	const float LegH = SeatZ - SeatTopH;
	const float LegZ = LegH * 0.5f;
	AddBox(TEXT("Chair_Leg1"), FVector(ChairX + HX, ChairY + HY, LegZ), FVector(5, 5, LegH), WhiteMaterial);
	AddBox(TEXT("Chair_Leg2"), FVector(ChairX - HX, ChairY + HY, LegZ), FVector(5, 5, LegH), WhiteMaterial);
	AddBox(TEXT("Chair_Leg3"), FVector(ChairX + HX, ChairY - HY, LegZ), FVector(5, 5, LegH), WhiteMaterial);
	AddBox(TEXT("Chair_Leg4"), FVector(ChairX - HX, ChairY - HY, LegZ), FVector(5, 5, LegH), WhiteMaterial);

	// 등받이 — 의자 뒤쪽(Y+ 끝)에 위치, 사용자는 테이블 쪽(Y-)을 향함.
	AddBox(TEXT("Chair_Back"),
		FVector(ChairX, ChairY + S.Y * 0.5f - 3.f, SeatZ + S.Z * 0.5f),
		FVector(S.X, 5, S.Z),
		WhiteMaterial);
}

FVector ALabEnvironmentActor::GetChairWorldLocation() const
{
	// BuildChair의 gap과 동일하게 유지.
	const float ChairX = TableOffsetCm.X;
	const float ChairY = TableOffsetCm.Y + RealTableSizeCm.Y * 0.5f + 25.f;
	return GetActorLocation() + FVector(ChairX, ChairY, 0.f);
}

FRotator ALabEnvironmentActor::GetChairFacingRotation() const
{
	// 의자는 Y+에 있고 사용자는 테이블(Y-)을 바라봄. Pawn forward(X+) 기준 Yaw -90.
	return FRotator(0.f, -90.f, 0.f);
}

void ALabEnvironmentActor::BuildControlPanel()
{
	// 9개 버튼: 1~8 = Case, 9 = Stop. 책상 위 3×3 그리드로 배치.
	// 사용자는 의자(Y+)에 앉아 Y- 방향을 향함 → 버튼은 Y+ 쪽(가까운 쪽)에 모이게 하는 게 자연스러움.
	const float TableTopZ = TableOffsetCm.Z + RealTableSizeCm.Z;     // 75cm
	const float ButtonZ   = TableTopZ + 4.f;                          // 79cm (버튼 중심)
	const FVector BtnSize(8.f, 8.f, 4.f);
	const float StepX = 14.f;
	const float StepY = 12.f;

	for (int32 i = 0; i < 9; ++i)
	{
		const int32 Row = i / 3;   // 0=user side(가까움), 1=mid, 2=far
		const int32 Col = i % 3;
		const float X = TableOffsetCm.X + (Col - 1) * StepX;
		const float Y = TableOffsetCm.Y + (1 - Row) * StepY - 5.f;   // user side로 약간 shift

		UStaticMeshComponent* MC = AddBox(
			FName(*FString::Printf(TEXT("CaseBtn_%d"), i + 1)),
			FVector(X, Y, ButtonZ), BtnSize, WhiteMaterial);
		// 버튼은 손이 통과해도 OK — 충돌 끔.
		if (MC)
		{
			MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			SpawnedMeshes.Remove(MC);   // ClearSpawned가 따로 처리하므로 SpawnedMeshes에선 빼둠
		}
		CaseButtons.Add(MC);

		// 버튼 위쪽에 식별용 텍스트(작게). 위에서 내려다볼 때 보임.
		// 텍스트는 yaw=90으로 사용자 쪽(+Y) 향함 — 좌우 반전이지만 단일 숫자라 인식 가능.
		UTextRenderComponent* T = NewObject<UTextRenderComponent>(this,
			*FString::Printf(TEXT("CaseLabel_%d"), i + 1));
		T->SetupAttachment(SceneRoot);
		T->RegisterComponent();
		T->SetRelativeLocation(FVector(X, Y, ButtonZ + 4.f));
		T->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));
		const FString Lbl = (i == 8) ? TEXT("S") : FString::FromInt(i + 1);
		T->SetText(FText::FromString(Lbl));
		T->SetHorizontalAlignment(EHTA_Center);
		T->SetVerticalAlignment(EVRTA_TextCenter);
		T->SetWorldSize(4.f);
		T->SetTextRenderColor((i == 8) ? FColor::Red : FColor::Black);
		CaseButtonLabels.Add(T);
	}

	// 상태 표시 — 책상 너머 벽 앞에 큰 텍스트, 사용자 향함(yaw=90, 좌우 반전 감수).
	StatusText = NewObject<UTextRenderComponent>(this, TEXT("StatusText"));
	StatusText->SetupAttachment(SceneRoot);
	StatusText->RegisterComponent();
	StatusText->SetRelativeLocation(FVector(
		TableOffsetCm.X,
		TableOffsetCm.Y - RealTableSizeCm.Y * 0.5f - 30.f,   // 책상 너머 30cm
		TableTopZ + 40.f));                                   // 책상 위 40cm
	StatusText->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));
	StatusText->SetText(FText::FromString(TEXT("IDLE\n[1]~[8]:Case  [S]:Stop")));
	StatusText->SetHorizontalAlignment(EHTA_Center);
	StatusText->SetVerticalAlignment(EVRTA_TextCenter);
	StatusText->SetWorldSize(6.f);
	StatusText->SetTextRenderColor(FColor::White);
}

void ALabEnvironmentActor::UpdateStatusText(const FString& InText)
{
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(InText));
	}
}

int32 ALabEnvironmentActor::HitTestButton(const FVector& WorldPos) const
{
	const FVector HalfExt(6.f, 6.f, 6.f);   // 12cm cube poke 박스 (조금 관대하게)
	for (int32 i = 0; i < CaseButtons.Num(); ++i)
	{
		const UStaticMeshComponent* B = CaseButtons[i];
		if (!B) continue;
		const FVector D = WorldPos - B->GetComponentLocation();
		if (FMath::Abs(D.X) < HalfExt.X &&
			FMath::Abs(D.Y) < HalfExt.Y &&
			FMath::Abs(D.Z) < HalfExt.Z)
		{
			return i + 1;   // 1-8: case, 9: stop
		}
	}
	return 0;
}

void ALabEnvironmentActor::BuildLights()
{
	CeilingLight = NewObject<USpotLightComponent>(this, TEXT("CeilingLight"));
	CeilingLight->SetupAttachment(SceneRoot);
	CeilingLight->RegisterComponent();
	CeilingLight->SetRelativeLocation(FVector(TableOffsetCm.X, TableOffsetCm.Y, RoomInnerSizeCm.Z - 30.f));
	CeilingLight->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	CeilingLight->SetIntensity(8000.f);
	CeilingLight->SetInnerConeAngle(20.f);
	CeilingLight->SetOuterConeAngle(50.f);
	CeilingLight->SetMobility(EComponentMobility::Static);

	AmbientFill = NewObject<UPointLightComponent>(this, TEXT("AmbientFill"));
	AmbientFill->SetupAttachment(SceneRoot);
	AmbientFill->RegisterComponent();
	AmbientFill->SetRelativeLocation(FVector(0, 0, RoomInnerSizeCm.Z * 0.5f));
	AmbientFill->SetIntensity(800.f);
	AmbientFill->SetAttenuationRadius(800.f);
	AmbientFill->SetMobility(EComponentMobility::Static);
}
