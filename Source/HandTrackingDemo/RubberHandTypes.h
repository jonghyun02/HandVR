#pragma once

#include "CoreMinimal.h"
#include "RubberHandTypes.generated.h"

UENUM(BlueprintType)
enum class ERHIStimulusType : uint8
{
	None        UMETA(DisplayName = "None"),
	BrushSync   UMETA(DisplayName = "Brush (Synchronous Touch)"),
	BrushAsync  UMETA(DisplayName = "Brush (Asynchronous, Visual Only)"),
	HammerThreat UMETA(DisplayName = "Hammer (Threat, Visual Only)")
};

// 중간발표 피드백 반영: 한 명당 1시간 이내 완주를 위해 Pilot(짧은 스크리닝)과
// Main(핵심 3~5케이스 검증)을 분리. Mode 전환 시 ExperimentManager가 해당 트랙의
// 케이스 배열만 로드한다.
UENUM(BlueprintType)
enum class ERHIExperimentMode : uint8
{
	Pilot UMETA(DisplayName = "Pilot (Quick Screening)"),
	Main  UMETA(DisplayName = "Main (Core Verification)")
};

UENUM(BlueprintType)
enum class ERHISurveyMetric : uint8
{
	BodyOwnership   UMETA(DisplayName = "신체소유감"),
	TactileDistort  UMETA(DisplayName = "촉각왜곡"),
	VisualDominance UMETA(DisplayName = "시각자극 위계")
};

USTRUCT(BlueprintType)
struct FRHICaseSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 CaseId = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0.0, ClampMax = 200.0))
	float SpatialErrorCm = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0.0, ClampMax = 3000.0))
	float TemporalDelayMs = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ERHIStimulusType Stimulus = ERHIStimulusType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 1.0, ClampMax = 600.0))
	float DurationSec = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bShowSurveyOnEnd = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bBodyOwnershipExpected = true;
};

USTRUCT(BlueprintType)
struct FRHISurveyResponse
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 CaseId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0, ClampMax = 7))
	int32 BodyOwnership = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0, ClampMax = 7))
	int32 TactileDistort = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0, ClampMax = 7))
	int32 VisualDominance = 0;
};

USTRUCT(BlueprintType)
struct FRHIBoneSnapshot
{
	GENERATED_BODY()

	UPROPERTY()
	double TimeSec = 0.0;

	UPROPERTY()
	TArray<FTransform> BoneWorld;
};
