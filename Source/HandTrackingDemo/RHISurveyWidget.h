#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RubberHandTypes.h"
#include "RHISurveyWidget.generated.h"

class UExperimentManagerComponent;
class USlider;
class UButton;
class UTextBlock;

/**
 * 케이스 종료 시 표시되는 설문 위젯 (3개 항목 Likert 1~7).
 *
 * BP 디자이너에서 BindWidget로 슬라이더/버튼/텍스트 채우는 것이 정석이지만,
 * BP 에셋 없이도 작동하도록 ApplyDefault*Response()로 키 입력 fallback 제공.
 *
 * 사용:
 *   1. ExperimentManagerComponent::SpawnSurveyWidget이 BP 클래스 인스턴스 생성
 *   2. BindManager(Component, CaseId)로 컨텍스트 주입
 *   3. 사용자가 슬라이더 조정 후 Submit 버튼 클릭 → NotifySubmit()
 *   4. 매니저로 응답 전달 → 다음 케이스
 */
UCLASS(Blueprintable, BlueprintType)
class HANDTRACKINGDEMO_API URHISurveyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "RHI|Survey")
	void BindManager(UExperimentManagerComponent* Manager, int32 InCaseId);

	UFUNCTION(BlueprintCallable, Category = "RHI|Survey")
	void NotifySubmit();

	UFUNCTION(BlueprintCallable, Category = "RHI|Survey")
	void SetResponseValues(int32 InOwnership, int32 InDistort, int32 InVisualDom);

	UPROPERTY(meta = (BindWidgetOptional)) USlider* OwnershipSlider = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) USlider* DistortSlider   = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) USlider* VisualDomSlider = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* SubmitButton    = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* CaseLabelText = nullptr;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	UFUNCTION() void OnSubmitClicked();

private:
	UPROPERTY() UExperimentManagerComponent* BoundManager = nullptr;
	int32 CaseId = 0;

	int32 PendingOwnership = 4;
	int32 PendingDistort = 4;
	int32 PendingVisualDom = 4;
};
