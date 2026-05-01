#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LabEnvironmentActor.generated.h"

class UStaticMeshComponent;
class USpotLightComponent;
class UPointLightComponent;
class UMaterialInterface;
class UTextRenderComponent;

/**
 * 절차적 실험실 환경.
 * BeginPlay 직전(생성자/OnConstruction)에 박스 메시들을 동적 부착해서 룸 + 책상 + 의자 + 조명 구성.
 *
 * 보고서 사양:
 *   - 흰색 톤 마감, 작은 룸
 *   - 책상은 현실 1:1 (RealTableSizeCm으로 노출 → 실측치 입력)
 *   - 의자, 천장 조명
 *   - 로우폴리 정적 메시 + 라이트맵 베이킹 (정적 라이팅)
 */
UCLASS()
class HANDTRACKINGDEMO_API ALabEnvironmentActor : public AActor
{
	GENERATED_BODY()

public:
	ALabEnvironmentActor();

	virtual void OnConstruction(const FTransform& Transform) override;

	/** 룸 내부 치수 (cm). 기본 4m × 4m × 2.6m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lab|Room")
	FVector RoomInnerSizeCm = FVector(400.f, 400.f, 260.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lab|Room")
	float WallThicknessCm = 10.f;

	/** 테이블 치수 (cm) — 가로/세로/높이. 패시브 햅틱용 1:1 매핑. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lab|Table")
	FVector RealTableSizeCm = FVector(120.f, 60.f, 75.f);

	/** 테이블 위치 (룸 중심 기준 오프셋). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lab|Table")
	FVector TableOffsetCm = FVector(0.f, 0.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lab|Chair")
	FVector ChairSeatSizeCm = FVector(45.f, 45.f, 45.f);

	/** 의자 좌석 중앙의 월드 좌표 — Pawn 시작 위치 계산용. */
	UFUNCTION(BlueprintCallable, Category = "Lab|Chair")
	FVector GetChairWorldLocation() const;

	/** 의자에 앉은 자세에서 테이블을 향하는 회전. */
	UFUNCTION(BlueprintCallable, Category = "Lab|Chair")
	FRotator GetChairFacingRotation() const;

	/** 컨트롤 패널: 상태 텍스트 갱신. */
	UFUNCTION(BlueprintCallable, Category = "Lab|Panel")
	void UpdateStatusText(const FString& InText);

	/** 컨트롤 패널: 월드 좌표가 어느 버튼 위에 있는지 검사. 1~8=Case, 9=Stop, 0=miss. */
	UFUNCTION(BlueprintCallable, Category = "Lab|Panel")
	int32 HitTestButton(const FVector& WorldPos) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lab|Material")
	UMaterialInterface* WhiteMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lab|Material")
	UMaterialInterface* TableMaterial = nullptr;

	/** /Engine/BasicShapes/Cube 메시. 생성자에서 ConstructorHelpers::FObjectFinder로 캐싱.
	    AddBox는 OnConstruction에서 호출되므로 거기서는 FObjectFinder 사용이 fatal error. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lab|Mesh")
	UStaticMesh* CubeMesh = nullptr;

protected:
	UPROPERTY(VisibleAnywhere) USceneComponent* SceneRoot;

	UPROPERTY() TArray<UStaticMeshComponent*> SpawnedMeshes;
	UPROPERTY() USpotLightComponent* CeilingLight;
	UPROPERTY() UPointLightComponent* AmbientFill;

	// 컨트롤 패널 — 9개 버튼(1~8=Case, 9=Stop) + 상태 표시 텍스트.
	UPROPERTY() TArray<UStaticMeshComponent*> CaseButtons;
	UPROPERTY() TArray<UTextRenderComponent*> CaseButtonLabels;
	UPROPERTY() UTextRenderComponent* StatusText = nullptr;

	void RebuildEnvironment();
	UStaticMeshComponent* AddBox(FName Name, const FVector& LocalLocation, const FVector& BoxSizeCm, UMaterialInterface* Mat);
	void BuildRoom();
	void BuildTable();
	void BuildChair();
	void BuildLights();
	void BuildControlPanel();
	void ClearSpawned();
};
