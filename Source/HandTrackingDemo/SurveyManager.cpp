#include "SurveyManager.h"

#include "VRLabelWidget.h"
#include "VRButton.h"

#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	// Verbatim from the team report (가상현실1팀보고서 5장 설문 표).
	// pair = (측정항목, 질문내용)
	static const TArray<TPair<FString, FString>>& GetQuestions()
	{
		static const TArray<TPair<FString, FString>> Qs = {
			{ TEXT("신체소유감"),   TEXT("가상 손이 실제 내 손처럼 느껴졌는가?") },
			{ TEXT("공간일치감"),   TEXT("가상 손의 위치가 실제 내 손의 위치와 비슷하게 느껴졌는가?") },
			{ TEXT("시간일치감"),   TEXT("눈으로 본 자극과 실제 촉각 자극이 동시에 일어난 것처럼 느껴졌는가?") },
			{ TEXT("촉각왜곡"),     TEXT("실제로 자극받은 부위가 아니라, 눈으로 본 위치에서 촉각이 느껴졌는가?") },
			{ TEXT("자극위치판단"), TEXT("자극이 가해진 위치를 실제 촉각보다 시각적으로 보인 위치 기준으로 판단했는가?") },
			{ TEXT("시각우위성"),   TEXT("실제 몸의 느낌보다 눈에 보이는 정보를 더 신뢰했는가?") },
			{ TEXT("위화감"),       TEXT("현재 조건에서 가상 손과 실제 손 사이에 어색함이나 위화감을 느꼈는가?") },
			{ TEXT("현실감"),       TEXT("현재 자극 상황이 실제로 내 손에 일어난 일처럼 느껴졌는가?") },
			{ TEXT("위협감"),       TEXT("가상 자극이 실제로 내 손을 위협한다고 느껴졌는가?") },
		};
		return Qs;
	}

	constexpr int32 BID_Answer_Base = 101; // ButtonId = BID_Answer_Base + likert_0_to_4
}

ASurveyManager::ASurveyManager()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	auto MakeText = [&](const TCHAR* Name, FVector RelLoc, FVector2D DrawSize, float WorldScale) -> UWidgetComponent*
	{
		UWidgetComponent* W = CreateDefaultSubobject<UWidgetComponent>(Name);
		W->SetupAttachment(Root);
		W->SetRelativeLocation(RelLoc);
		W->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f)); // face the player (-X)
		W->SetWidgetSpace(EWidgetSpace::World);
		W->SetDrawSize(DrawSize);
		W->SetWidgetClass(UVRLabelWidget::StaticClass());
		W->SetWorldScale3D(FVector(WorldScale));
		W->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return W;
	};

	TitleWidget    = MakeText(TEXT("TitleWidget"),    FVector(0.0f, 0.0f, 25.0f), FVector2D(1200.0f, 120.0f), 0.06f);
	// 긴 한글 문항이 양옆으로 잘리지 않도록 패널을 넓힘(2600x300). 실제 줄바꿈은 UVRLabelWidget의 AutoWrapText가 처리.
	QuestionWidget = MakeText(TEXT("QuestionWidget"), FVector(0.0f, 0.0f, 12.0f), FVector2D(2600.0f, 300.0f), 0.05f);
}

void ASurveyManager::BeginPlay()
{
	Super::BeginPlay();

	// Yellow title, white question — apply once the widgets have built their underlying UUserWidget.
	if (TitleWidget)
	{
		if (UVRLabelWidget* L = Cast<UVRLabelWidget>(TitleWidget->GetUserWidgetObject()))
		{
			L->SetLabelColor(FLinearColor(1.0f, 0.9f, 0.2f));
			L->SetLabelFontSize(56.0f);
		}
	}
	if (QuestionWidget)
	{
		if (UVRLabelWidget* L = Cast<UVRLabelWidget>(QuestionWidget->GetUserWidgetObject()))
		{
			L->SetLabelColor(FLinearColor::White);
			L->SetLabelFontSize(38.0f);
		}
	}

	UWorld* W = GetWorld();
	if (!W) return;

	// Spawn 5 Likert buttons spaced along Y at the bottom of the panel.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 i = 0; i < 5; ++i)
	{
		const FVector LocalOffset(0.0f, -28.0f + i * 14.0f, -10.0f);
		const FVector WorldLoc = GetActorTransform().TransformPosition(LocalOffset);
		AHTDButton* B = W->SpawnActor<AHTDButton>(AHTDButton::StaticClass(), FTransform(WorldLoc), Params);
		if (B)
		{
			const FString Label = FString::FromInt(i + 1);
			const FLinearColor Tint = FLinearColor::LerpUsingHSV(FLinearColor(0.9f, 0.2f, 0.2f), FLinearColor(0.2f, 0.8f, 0.4f), i / 4.0f);
			B->Configure(Label, BID_Answer_Base + i, Tint);
			B->OnPressed.AddDynamic(this, &ASurveyManager::OnAnswerPressed);
			B->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
			B->SetEnabledState(false); // hidden until BeginSurvey()
			AnswerButtons.Add(B);
		}
	}
}

void ASurveyManager::BeginSurvey(const FString& InConditionTag)
{
	ConditionTag    = InConditionTag;
	CurrentQuestion = 0;
	Answers.Reset();
	for (AHTDButton* B : AnswerButtons) if (B) B->SetEnabledState(true);
	ShowQuestion(0);
}

void ASurveyManager::ShowQuestion(int32 Index)
{
	const auto& Qs = GetQuestions();
	if (!Qs.IsValidIndex(Index))
	{
		SaveResults();

		// 설문 종료 시 1~5 Likert 버튼을 화면에서 제거. 먼저 비활성화(만일을 위한 안전장치) 후 액터 파괴.
		for (AHTDButton* B : AnswerButtons)
		{
			if (B)
			{
				B->SetEnabledState(false);
				B->Destroy();
			}
		}
		AnswerButtons.Empty(); // 파괴 후 댕글링 포인터 역참조 방지

		OnFinished.Broadcast();
		return;
	}

	const FString Title    = FString::Printf(TEXT("[%d / %d]  %s"), Index + 1, Qs.Num(), *Qs[Index].Key);
	const FString Question = Qs[Index].Value;

	if (TitleWidget)
	{
		if (UVRLabelWidget* L = Cast<UVRLabelWidget>(TitleWidget->GetUserWidgetObject()))
			L->SetLabelText(FText::FromString(Title));
	}
	if (QuestionWidget)
	{
		if (UVRLabelWidget* L = Cast<UVRLabelWidget>(QuestionWidget->GetUserWidgetObject()))
			L->SetLabelText(FText::FromString(Question));
	}
}

void ASurveyManager::OnAnswerPressed(int32 ButtonId)
{
	const int32 Score = ButtonId - BID_Answer_Base + 1;
	if (Score < 1 || Score > 5) return;

	Answers.Add(Score);
	++CurrentQuestion;
	ShowQuestion(CurrentQuestion);
}

void ASurveyManager::SaveResults()
{
	const auto& Qs = GetQuestions();
	// One CUMULATIVE file per project so all conditions/participants accumulate (보고서 4.6 "한 파일 누적"),
	// each row stamped + tagged with its condition. Matches ExperimentManager's SurveyResults_*.csv glob.
	const FString Path  = FPaths::ProjectSavedDir() / TEXT("SurveyResults_log.csv");
	const FString Stamp = FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S"));

	// 미완료 응답은 중간값(3)으로 패딩 — 사고로 일부만 답해도 행 보존(보고서 4.6).
	TArray<int32> Padded = Answers;
	while (Padded.Num() < Qs.Num()) Padded.Add(3);

	FString SafeTag = ConditionTag.IsEmpty() ? TEXT("manual") : ConditionTag;
	SafeTag.ReplaceInline(TEXT(","), TEXT(";")); // keep CSV columns intact

	// Write the header once (with BOM for Excel); append each data row without BOM.
	if (!IFileManager::Get().FileExists(*Path))
	{
		TArray<FString> Header; Header.Add(TEXT("Timestamp")); Header.Add(TEXT("Condition"));
		for (auto& Q : Qs) Header.Add(Q.Key);
		FFileHelper::SaveStringToFile(FString::Join(Header, TEXT(",")) + TEXT("\r\n"), *Path,
			FFileHelper::EEncodingOptions::ForceUTF8);
	}

	TArray<FString> Row; Row.Add(Stamp); Row.Add(SafeTag);
	for (int32 i = 0; i < Qs.Num(); ++i) Row.Add(FString::FromInt(Padded[i]));
	// FILEWRITE_Append + WithoutBOM → row appends cleanly; flushed immediately so a crash keeps prior rows.
	FFileHelper::SaveStringToFile(FString::Join(Row, TEXT(",")) + TEXT("\r\n"), *Path,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), EFileWrite::FILEWRITE_Append);
}
