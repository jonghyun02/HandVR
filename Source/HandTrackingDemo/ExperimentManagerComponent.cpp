#include "ExperimentManagerComponent.h"
#include "RubberHandPawn.h"
#include "BrushActor.h"
#include "HammerActor.h"
#include "RHISurveyWidget.h"
#include "Components/WidgetComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/DateTime.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "TimerManager.h"
#include "GameFramework/Actor.h"

const FRHICaseSpec UExperimentManagerComponent::InvalidCase{};

UExperimentManagerComponent::UExperimentManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UExperimentManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!TryLoadCasesFromJson())
	{
		InitDefaultCases();
	}
	OpenCsvLog();
	UE_LOG(LogTemp, Log, TEXT("[RHI] ExperimentManager ready. %d cases loaded."), Cases.Num());
}

void UExperimentManagerComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	StopExperiment();
	FlushCsv();
	Super::EndPlay(Reason);
}

void UExperimentManagerComponent::InitDefaultCases()
{
	Cases.Reset();

	// Phase 1 — 신체소유감 임계값
	Cases.Add({1, TEXT("Case1: 최상의 동기화 (대조군)"),    0.f,    0.f,  ERHIStimulusType::BrushSync,  30.f, true,  true });
	Cases.Add({2, TEXT("Case2: 공간오차 5~30cm"),          15.f,    0.f,  ERHIStimulusType::BrushSync,  30.f, true,  false});
	Cases.Add({3, TEXT("Case3: 시간오차 100~500ms"),        0.f,  300.f,  ERHIStimulusType::BrushSync,  30.f, true,  false});
	Cases.Add({4, TEXT("Case4: 시/공간 오차 공존"),        15.f,  300.f,  ERHIStimulusType::BrushSync,  30.f, true,  false});

	// Phase 2 — 촉각왜곡 검증
	Cases.Add({5, TEXT("Case5: 신체소유감 활성+불일치"),    0.f,    0.f,  ERHIStimulusType::BrushAsync, 30.f, true,  true });
	Cases.Add({6, TEXT("Case6: 신체소유감 비활성+불일치"), 30.f,  500.f,  ERHIStimulusType::BrushAsync, 30.f, true,  false});
	Cases.Add({7, TEXT("Case7: 신체소유감 활성+시각만"),    0.f,    0.f,  ERHIStimulusType::HammerThreat, 15.f, true, true });
	Cases.Add({8, TEXT("Case8: 신체소유감 비활성+시각만"),30.f,  500.f,  ERHIStimulusType::HammerThreat, 15.f, true, false});
}

bool UExperimentManagerComponent::TryLoadCasesFromJson()
{
	const FString FullPath = FPaths::ProjectContentDir() / CasesJsonRelativePath;
	FString Raw;
	if (!FFileHelper::LoadFileToString(Raw, *FullPath))
	{
		return false;
	}

	TSharedPtr<FJsonValue> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[RHI] Cases.json parse failed at %s"), *FullPath);
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (Root->Type == EJson::Array)
	{
		Arr = &Root->AsArray();
	}
	else if (Root->Type == EJson::Object && Root->AsObject()->HasField(TEXT("cases")))
	{
		Arr = &Root->AsObject()->GetArrayField(TEXT("cases"));
	}
	if (!Arr) return false;

	Cases.Reset();
	for (const TSharedPtr<FJsonValue>& V : *Arr)
	{
		const TSharedPtr<FJsonObject> Obj = V->AsObject();
		if (!Obj.IsValid()) continue;

		FRHICaseSpec C;
		C.CaseId         = static_cast<int32>(Obj->GetIntegerField(TEXT("caseId")));
		C.Label          = Obj->GetStringField(TEXT("label"));
		C.SpatialErrorCm = static_cast<float>(Obj->GetNumberField(TEXT("spatialErrCm")));
		C.TemporalDelayMs= static_cast<float>(Obj->GetNumberField(TEXT("temporalDelayMs")));
		C.DurationSec    = static_cast<float>(Obj->GetNumberField(TEXT("durationSec")));
		C.bShowSurveyOnEnd       = Obj->GetBoolField(TEXT("surveyOnEnd"));
		Obj->TryGetBoolField(TEXT("ownershipExpected"), C.bBodyOwnershipExpected);

		const FString StimStr = Obj->GetStringField(TEXT("stimulus"));
		if      (StimStr == TEXT("BrushSync"))    C.Stimulus = ERHIStimulusType::BrushSync;
		else if (StimStr == TEXT("BrushAsync"))   C.Stimulus = ERHIStimulusType::BrushAsync;
		else if (StimStr == TEXT("HammerThreat")) C.Stimulus = ERHIStimulusType::HammerThreat;
		else                                      C.Stimulus = ERHIStimulusType::None;

		Cases.Add(C);
	}
	return Cases.Num() > 0;
}

ARubberHandPawn* UExperimentManagerComponent::GetOwnerPawn() const
{
	return Cast<ARubberHandPawn>(GetOwner());
}

void UExperimentManagerComponent::StartExperiment()
{
	if (bRunning) return;
	if (Cases.Num() == 0) { InitDefaultCases(); }

	bRunning = true;
	ExperimentStartTime = GetWorld()->GetTimeSeconds();
	LogEvent(TEXT("ExperimentStart"), FString::Printf(TEXT("cases=%d"), Cases.Num()));
	BeginCase(0);
}

void UExperimentManagerComponent::StopExperiment()
{
	if (!bRunning) return;
	if (UWorld* W = GetWorld()) { W->GetTimerManager().ClearTimer(CaseTimer); }
	DespawnStimulusActors();
	ClearSurveyWidget();
	bRunning = false;
	CurrentCaseIndex = INDEX_NONE;
	LogEvent(TEXT("ExperimentStop"), TEXT(""));
	FlushCsv();

	// 가상 손 효과도 리셋(다음 시작까지 대기 상태).
	if (ARubberHandPawn* P = GetOwnerPawn())
	{
		P->SetSpatialOffsetCm(0.f);
		P->SetTemporalDelayMs(0.f);
	}
}

void UExperimentManagerComponent::SelectCase(int32 CaseId)
{
	// CaseId = 9 → Stop
	if (CaseId == 9)
	{
		if (bRunning) { StopExperiment(); }
		LogEvent(TEXT("ManualStop"), TEXT(""));
		return;
	}
	if (CaseId < 1 || CaseId > 8) return;

	// 진행 중이면 정리 후 즉시 새 케이스 시작.
	if (bRunning)
	{
		if (UWorld* W = GetWorld()) { W->GetTimerManager().ClearTimer(CaseTimer); }
		DespawnStimulusActors();
		ClearSurveyWidget();
	}
	if (Cases.Num() == 0) { InitDefaultCases(); }

	bRunning = true;
	if (ExperimentStartTime <= 0.0)
	{
		ExperimentStartTime = GetWorld()->GetTimeSeconds();
	}
	LogEvent(TEXT("ManualSelect"), FString::Printf(TEXT("caseId=%d"), CaseId));
	BeginCase(CaseId - 1);   // 1-indexed → 0-indexed
}

FString UExperimentManagerComponent::GetStatusString() const
{
	if (!bRunning || !Cases.IsValidIndex(CurrentCaseIndex))
	{
		return TEXT("IDLE\n[1]~[8]:Case  [S]:Stop");
	}
	const FRHICaseSpec& C = Cases[CurrentCaseIndex];
	float Remain = 0.f;
	if (UWorld* W = GetWorld())
	{
		Remain = W->GetTimerManager().GetTimerRemaining(CaseTimer);
		if (Remain < 0.f) Remain = 0.f;
	}
	const TCHAR* StimStr =
		C.Stimulus == ERHIStimulusType::BrushSync    ? TEXT("BrushSync")  :
		C.Stimulus == ERHIStimulusType::BrushAsync   ? TEXT("BrushAsync") :
		C.Stimulus == ERHIStimulusType::HammerThreat ? TEXT("Hammer")     : TEXT("None");
	return FString::Printf(
		TEXT("CASE %d / 8\n%.0fcm  %.0fms  %s\n%.1fs left"),
		C.CaseId, C.SpatialErrorCm, C.TemporalDelayMs, StimStr, Remain);
}

void UExperimentManagerComponent::BeginCase(int32 Index)
{
	if (!Cases.IsValidIndex(Index))
	{
		LogEvent(TEXT("ExperimentCompleted"), TEXT(""));
		bRunning = false;
		FlushCsv();
		OnExperimentCompleted.Broadcast();
		return;
	}

	CurrentCaseIndex = Index;
	const FRHICaseSpec& C = Cases[Index];

	if (ARubberHandPawn* P = GetOwnerPawn())
	{
		P->SetSpatialOffsetCm(C.SpatialErrorCm);
		P->SetTemporalDelayMs(C.TemporalDelayMs);
		P->SetSyntheticHandVisible(true);
	}

	LogEvent(TEXT("CaseStart"),
		FString::Printf(TEXT("caseId=%d offset=%.1fcm delay=%.0fms stim=%d dur=%.1fs"),
			C.CaseId, C.SpatialErrorCm, C.TemporalDelayMs, (int32)C.Stimulus, C.DurationSec));

	OnCaseStarted.Broadcast(C);

	// 자극 액터 스폰
	UWorld* W = GetWorld();
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (W && Pawn)
	{
		const FVector PawnLoc = Pawn->GetActorLocation();
		FActorSpawnParameters SP; SP.Owner = Pawn;

		if ((C.Stimulus == ERHIStimulusType::BrushSync || C.Stimulus == ERHIStimulusType::BrushAsync) && BrushActorClass)
		{
			ActiveBrush = W->SpawnActor<ABrushActor>(BrushActorClass, PawnLoc, FRotator::ZeroRotator, SP);
			if (ActiveBrush)
			{
				const bool bSync = (C.Stimulus == ERHIStimulusType::BrushSync);
				ActiveBrush->BeginStroking(Pawn, bSync);
			}
		}
		else if (C.Stimulus == ERHIStimulusType::HammerThreat && HammerActorClass)
		{
			ActiveHammer = W->SpawnActor<AHammerActor>(HammerActorClass, PawnLoc, FRotator::ZeroRotator, SP);
			if (ActiveHammer) { ActiveHammer->BeginThreat(Pawn); }
		}

		W->GetTimerManager().SetTimer(CaseTimer, this, &UExperimentManagerComponent::EndCurrentCase, FMath::Max(0.5f, C.DurationSec), false);
	}
}

void UExperimentManagerComponent::EndCurrentCase()
{
	if (!Cases.IsValidIndex(CurrentCaseIndex)) return;
	const FRHICaseSpec C = Cases[CurrentCaseIndex];

	DespawnStimulusActors();
	LogEvent(TEXT("CaseEnd"), FString::Printf(TEXT("caseId=%d"), C.CaseId));
	OnCaseEnded.Broadcast(C.CaseId);

	if (C.bShowSurveyOnEnd)
	{
		SpawnSurveyWidget();
		// 다음 케이스로의 진행은 OnSurveySubmitted에서.
	}
	else
	{
		BeginCase(CurrentCaseIndex + 1);
	}
}

void UExperimentManagerComponent::OnSurveySubmitted(const FRHISurveyResponse& Response)
{
	LogEvent(TEXT("Survey"),
		FString::Printf(TEXT("caseId=%d ownership=%d distort=%d visualDom=%d"),
			Response.CaseId, Response.BodyOwnership, Response.TactileDistort, Response.VisualDominance));
	ClearSurveyWidget();
	BeginCase(CurrentCaseIndex + 1);
}

void UExperimentManagerComponent::SpawnSurveyWidget()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn) return;

	// BP 위젯 미바인딩 시 데드락 방지 — default 중립 응답(4) 자동 제출 후 다음 케이스 진행.
	if (!SurveyWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RHI] SurveyWidgetClass not set — auto-submitting neutral response (4) to proceed."));
		FRHISurveyResponse Auto;
		Auto.CaseId          = Cases.IsValidIndex(CurrentCaseIndex) ? Cases[CurrentCaseIndex].CaseId : 0;
		Auto.BodyOwnership   = 4;
		Auto.TactileDistort  = 4;
		Auto.VisualDominance = 4;
		OnSurveySubmitted(Auto);
		return;
	}

	if (!SurveyWidgetComp)
	{
		SurveyWidgetComp = NewObject<UWidgetComponent>(Pawn, TEXT("SurveyWidgetComp"));
		SurveyWidgetComp->SetupAttachment(Pawn->GetRootComponent());
		SurveyWidgetComp->RegisterComponent();
		SurveyWidgetComp->SetWidgetSpace(EWidgetSpace::World);
		SurveyWidgetComp->SetDrawSize(FVector2D(800, 600));
		SurveyWidgetComp->SetTwoSided(true);
	}

	// 카메라 앞 1m
	if (ARubberHandPawn* RH = Cast<ARubberHandPawn>(Pawn))
	{
		if (RH->VRCamera)
		{
			const FTransform CT = RH->VRCamera->GetComponentTransform();
			SurveyWidgetComp->SetWorldLocationAndRotation(
				CT.GetLocation() + CT.GetRotation().GetForwardVector() * 100.f,
				CT.GetRotation());
		}
	}

	SurveyWidgetComp->SetWidgetClass(SurveyWidgetClass);
	SurveyWidgetComp->SetVisibility(true);

	if (URHISurveyWidget* W = Cast<URHISurveyWidget>(SurveyWidgetComp->GetUserWidgetObject()))
	{
		const FRHICaseSpec& C = Cases.IsValidIndex(CurrentCaseIndex) ? Cases[CurrentCaseIndex] : InvalidCase;
		W->BindManager(this, C.CaseId);
	}
}

void UExperimentManagerComponent::ClearSurveyWidget()
{
	if (SurveyWidgetComp)
	{
		SurveyWidgetComp->SetVisibility(false);
	}
}

void UExperimentManagerComponent::DespawnStimulusActors()
{
	if (ActiveBrush)  { ActiveBrush->Destroy();  ActiveBrush  = nullptr; }
	if (ActiveHammer) { ActiveHammer->Destroy(); ActiveHammer = nullptr; }
}

void UExperimentManagerComponent::OpenCsvLog()
{
	const FString TS = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	CsvPath = FPaths::ProjectSavedDir() / TEXT("Logs") / FString::Printf(TEXT("RHI_%s.csv"), *TS);
	CsvBuffer.Reset();
	CsvBuffer.Add(TEXT("event,time_sec,detail"));
}

void UExperimentManagerComponent::LogEvent(const FString& EventName, const FString& Detail)
{
	const double T = GetWorld() ? (GetWorld()->GetTimeSeconds() - ExperimentStartTime) : 0.0;
	FString Safe = Detail.Replace(TEXT(","), TEXT(";"));
	CsvBuffer.Add(FString::Printf(TEXT("%s,%.3f,%s"), *EventName, T, *Safe));
	UE_LOG(LogTemp, Log, TEXT("[RHI/%s] t=%.3f %s"), *EventName, T, *Safe);

	if (CsvBuffer.Num() >= 32) { FlushCsv(); }
}

void UExperimentManagerComponent::FlushCsv()
{
	if (CsvBuffer.Num() == 0 || CsvPath.IsEmpty()) return;
	FString Out;
	for (const FString& L : CsvBuffer) { Out += L; Out += TEXT("\n"); }
	const uint32 Flag = IFileManager::Get().FileExists(*CsvPath) ? FILEWRITE_Append : FILEWRITE_None;
	FFileHelper::SaveStringToFile(Out, *CsvPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
		&IFileManager::Get(), Flag);
	CsvBuffer.Reset();
}

const FRHICaseSpec& UExperimentManagerComponent::GetCurrentCase() const
{
	return Cases.IsValidIndex(CurrentCaseIndex) ? Cases[CurrentCaseIndex] : InvalidCase;
}
