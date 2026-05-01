#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RubberHandTypes.h"
#include "ExperimentManagerComponent.generated.h"

class ARubberHandPawn;
class ABrushActor;
class AHammerActor;
class URHISurveyWidget;
class UWidgetComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRHICaseStarted, const FRHICaseSpec&, Case);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRHICaseEnded, int32, CaseId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRHIExperimentCompleted);

/**
 * 8 케이스 자동 진행 매니저 + 이벤트 CSV 로거.
 *
 * 케이스 데이터:
 *   - Default: 보고서의 8케이스 하드코딩 (`InitDefaultCases`)
 *   - Optional: `Content/Experiment/Cases.json`이 있으면 로드해서 덮어씀
 *
 * 진행 흐름:
 *   StartExperiment()
 *     → BeginCase(0)            : Pawn에 offset/delay 적용 + 자극 시작
 *     → Duration 후 EndCase()   : 자극 종료 + 설문 위젯 표시
 *     → 설문 응답 수신 후       : 다음 케이스 BeginCase(i+1)
 *     → 마지막 케이스 끝나면     : ExperimentCompleted 브로드캐스트 + 로그 flush
 */
UCLASS(ClassGroup = (RHI), meta = (BlueprintSpawnableComponent))
class HANDTRACKINGDEMO_API UExperimentManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UExperimentManagerComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	UFUNCTION(BlueprintCallable, Category = "RHI|Experiment")
	void StartExperiment();

	UFUNCTION(BlueprintCallable, Category = "RHI|Experiment")
	void StopExperiment();

	/** 단일 케이스 즉시 실행. 1~8 = 해당 케이스, 9 = Stop. 진행 중이면 중단 후 새로 시작. */
	UFUNCTION(BlueprintCallable, Category = "RHI|Experiment")
	void SelectCase(int32 CaseId);

	/** HUD/벽면 표시용 상태 문자열. */
	UFUNCTION(BlueprintCallable, Category = "RHI|Experiment")
	FString GetStatusString() const;

	UFUNCTION(BlueprintCallable, Category = "RHI|Experiment")
	void OnSurveySubmitted(const FRHISurveyResponse& Response);

	UFUNCTION(BlueprintCallable, Category = "RHI|Experiment")
	void LogEvent(const FString& EventName, const FString& Detail);

	UFUNCTION(BlueprintCallable, Category = "RHI|Experiment")
	const FRHICaseSpec& GetCurrentCase() const;

	UFUNCTION(BlueprintCallable, Category = "RHI|Experiment")
	int32 GetCurrentCaseIndex() const { return CurrentCaseIndex; }

	UFUNCTION(BlueprintCallable, Category = "RHI|Experiment")
	bool IsRunning() const { return bRunning; }

	UPROPERTY(BlueprintAssignable, Category = "RHI|Experiment")
	FOnRHICaseStarted OnCaseStarted;

	UPROPERTY(BlueprintAssignable, Category = "RHI|Experiment")
	FOnRHICaseEnded OnCaseEnded;

	UPROPERTY(BlueprintAssignable, Category = "RHI|Experiment")
	FOnRHIExperimentCompleted OnExperimentCompleted;

	// ----- 데이터 -----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RHI|Experiment")
	TArray<FRHICaseSpec> Cases;

	/** Cases.json 상대 경로 (Content/...). 비어있으면 기본 8케이스 사용. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RHI|Experiment")
	FString CasesJsonRelativePath = TEXT("Experiment/Cases.json");

	/** 자극 액터 클래스. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RHI|Stimulus")
	TSubclassOf<ABrushActor> BrushActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RHI|Stimulus")
	TSubclassOf<AHammerActor> HammerActorClass;

	/** 설문 Widget 클래스. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RHI|Survey")
	TSubclassOf<URHISurveyWidget> SurveyWidgetClass;

protected:
	void InitDefaultCases();
	bool TryLoadCasesFromJson();

	void BeginCase(int32 Index);
	void EndCurrentCase();

	void SpawnSurveyWidget();
	void ClearSurveyWidget();
	void DespawnStimulusActors();

	void OpenCsvLog();
	void FlushCsv();

	ARubberHandPawn* GetOwnerPawn() const;

private:
	UPROPERTY() ABrushActor*  ActiveBrush  = nullptr;
	UPROPERTY() AHammerActor* ActiveHammer = nullptr;
	UPROPERTY() UWidgetComponent* SurveyWidgetComp = nullptr;

	int32 CurrentCaseIndex = INDEX_NONE;
	bool bRunning = false;
	FTimerHandle CaseTimer;

	FString CsvPath;
	TArray<FString> CsvBuffer;
	double ExperimentStartTime = 0.0;

	static const FRHICaseSpec InvalidCase;
};
