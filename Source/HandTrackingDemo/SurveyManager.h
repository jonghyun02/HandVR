#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SurveyManager.generated.h"

class AHTDButton;
class UWidgetComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSurveyFinished);

/**
 * ASurveyManager — 3D world-space 8-item Likert survey, verbatim from the team report (5장 설문 표).
 *
 *   화면 구성:
 *     [측정항목]
 *     문항 텍스트
 *     [ 1 ] [ 2 ] [ 3 ] [ 4 ] [ 5 ]    ← 5 likert 버튼
 *
 *   매 응답마다 자동으로 다음 문항으로 진행. 8문항 완료 시 CSV 저장 + OnFinished broadcast.
 *
 *   결과 파일: <ProjectSavedDir>/SurveyResults_YYYYMMDD_HHMMSS.csv  (UTF-8 BOM)
 *
 *   라벨/문항은 UWidgetComponent (Slate UMG) 로 출력 → 기본 Roboto 컴포지트 폰트의
 *   CJK fallback 으로 한글 글리프 정상 출력됨. 별도 폰트 임포트 불필요.
 */
UCLASS()
class HANDTRACKINGDEMO_API ASurveyManager : public AActor
{
	GENERATED_BODY()

public:
	ASurveyManager();

	UPROPERTY(BlueprintAssignable) FSurveyFinished OnFinished;

	/** ConditionTag(실험종류·공간오차·시간오차·일치여부)는 CSV 행에 함께 기록된다. */
	UFUNCTION(BlueprintCallable) void BeginSurvey(const FString& ConditionTag = TEXT(""));

protected:
	UFUNCTION() void OnAnswerPressed(int32 ButtonId);

	virtual void BeginPlay() override;

private:
	void ShowQuestion(int32 Index);
	void SaveResults();

	UPROPERTY() UWidgetComponent*  TitleWidget    = nullptr;
	UPROPERTY() UWidgetComponent*  QuestionWidget = nullptr;
	UPROPERTY() TArray<AHTDButton*> AnswerButtons;

	int32         CurrentQuestion = 0;
	TArray<int32> Answers;
	FString       ConditionTag;   // 이번 응답의 실험 조건 메타 (CSV 행에 기록)
};
