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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0.0, ClampMax = 60.0))
	float SpatialErrorCm = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0.0, ClampMax = 1000.0))
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
