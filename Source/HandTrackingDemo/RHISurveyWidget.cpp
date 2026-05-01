#include "RHISurveyWidget.h"
#include "ExperimentManagerComponent.h"
#include "Components/Slider.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

void URHISurveyWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (SubmitButton && !SubmitButton->OnClicked.IsBound())
	{
		SubmitButton->OnClicked.AddDynamic(this, &URHISurveyWidget::OnSubmitClicked);
	}
}

void URHISurveyWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (CaseLabelText)
	{
		CaseLabelText->SetText(FText::FromString(FString::Printf(TEXT("Case %d - 설문에 응답해주세요"), CaseId)));
	}
	if (OwnershipSlider) { OwnershipSlider->SetValue(0.5f); }
	if (DistortSlider)   { DistortSlider->SetValue(0.5f); }
	if (VisualDomSlider) { VisualDomSlider->SetValue(0.5f); }
}

void URHISurveyWidget::BindManager(UExperimentManagerComponent* Manager, int32 InCaseId)
{
	BoundManager = Manager;
	CaseId = InCaseId;
	if (CaseLabelText)
	{
		CaseLabelText->SetText(FText::FromString(FString::Printf(TEXT("Case %d - 설문에 응답해주세요"), CaseId)));
	}
}

void URHISurveyWidget::SetResponseValues(int32 InOwnership, int32 InDistort, int32 InVisualDom)
{
	PendingOwnership = FMath::Clamp(InOwnership, 1, 7);
	PendingDistort   = FMath::Clamp(InDistort,   1, 7);
	PendingVisualDom = FMath::Clamp(InVisualDom, 1, 7);
}

void URHISurveyWidget::OnSubmitClicked()
{
	NotifySubmit();
}

void URHISurveyWidget::NotifySubmit()
{
	FRHISurveyResponse R;
	R.CaseId = CaseId;

	auto SliderToLikert = [](USlider* S, int32 Fallback) -> int32
	{
		if (!S) return Fallback;
		// Slider 0..1 → Likert 1..7
		return FMath::Clamp(FMath::RoundToInt(S->GetValue() * 6.f) + 1, 1, 7);
	};

	R.BodyOwnership   = SliderToLikert(OwnershipSlider, PendingOwnership);
	R.TactileDistort  = SliderToLikert(DistortSlider,   PendingDistort);
	R.VisualDominance = SliderToLikert(VisualDomSlider, PendingVisualDom);

	if (BoundManager)
	{
		BoundManager->OnSurveySubmitted(R);
	}
}
