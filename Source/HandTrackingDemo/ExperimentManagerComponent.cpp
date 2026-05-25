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
	CurrentMode = DefaultMode;
	ApplyModeCases();
	OpenCsvLog();
	UE_LOG(LogTemp, Log, TEXT("[RHI] ExperimentManager ready. mode=%s pilot=%d main=%d active=%d"),
		CurrentMode == ERHIExperimentMode::Pilot ? TEXT("Pilot") : TEXT("Main"),
		PilotCases.Num(), MainCases.Num(), Cases.Num());
}

void UExperimentManagerComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	StopExperiment();
	FlushCsv();
	Super::EndPlay(Reason);
}

void UExperimentManagerComponent::InitDefaultCases()
{
	// 효과가 확실히 느껴지도록 수치 대폭 확대. Cases.json 미존재 시 폴백.
	PilotCases.Reset();
	MainCases.Reset();

	// Pilot 7케이스 — 공간(0/30/60/100cm)·시간(0/500/1500/3000ms) 극단 비교. 각 20s.
	PilotCases.Add({11, TEXT("P1: baseline (0cm/0ms)"),         0.f,    0.f,  ERHIStimulusType::BrushSync, 20.f, true, true });
	PilotCases.Add({12, TEXT("P2: 공간 30cm"),                 30.f,    0.f,  ERHIStimulusType::BrushSync, 20.f, true, true });
	PilotCases.Add({13, TEXT("P3: 공간 60cm"),                 60.f,    0.f,  ERHIStimulusType::BrushSync, 20.f, true, false});
	PilotCases.Add({14, TEXT("P4: 공간 100cm 극단"),          100.f,    0.f,  ERHIStimulusType::BrushSync, 20.f, true, false});
	PilotCases.Add({15, TEXT("P5: 시간 500ms"),                 0.f,  500.f,  ERHIStimulusType::BrushSync, 20.f, true, true });
	PilotCases.Add({16, TEXT("P6: 시간 1500ms"),                0.f, 1500.f,  ERHIStimulusType::BrushSync, 20.f, true, false});
	PilotCases.Add({17, TEXT("P7: 시간 3000ms 극단"),           0.f, 3000.f,  ERHIStimulusType::BrushSync, 20.f, true, false});

	// Main 7케이스 — 핵심 가설 + 극단 비교. 각 60s.
	MainCases.Add({1, TEXT("M1: baseline 동기화"),              0.f,    0.f,  ERHIStimulusType::BrushSync,    60.f, true, true });
	MainCases.Add({2, TEXT("M2: 공간 60cm 큰 어긋남"),         60.f,    0.f,  ERHIStimulusType::BrushSync,    60.f, true, false});
	MainCases.Add({3, TEXT("M3: 시간 1500ms 큰 지연"),          0.f, 1500.f,  ERHIStimulusType::BrushSync,    60.f, true, false});
	MainCases.Add({4, TEXT("M4: 촉각불일치 (시각우위)"),        0.f,    0.f,  ERHIStimulusType::BrushAsync,   60.f, true, true });
	MainCases.Add({5, TEXT("M5: 위협 망치 (시각만)"),           0.f,    0.f,  ERHIStimulusType::HammerThreat, 45.f, true, true });
	MainCases.Add({6, TEXT("M6: 공간+시간 동시"),              60.f, 1500.f,  ERHIStimulusType::BrushSync,    60.f, true, false});
	MainCases.Add({7, TEXT("M7: 극단 (100cm/3000ms)"),        100.f, 3000.f,  ERHIStimulusType::BrushSync,    60.f, true, false});
}

void UExperimentManagerComponent::ApplyModeCases()
{
	Cases = (CurrentMode == ERHIExperimentMode::Pilot) ? PilotCases : MainCases;
}

static void ParseCaseArray(const TArray<TSharedPtr<FJsonValue>>& Arr, TArray<FRHICaseSpec>& Out)
{
	Out.Reset();
	for (const TSharedPtr<FJsonValue>& V : Arr)
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

		Out.Add(C);
	}
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

	PilotCases.Reset();
	MainCases.Reset();

	// 새 포맷: { casesPilot:[...], casesMain:[...] }
	if (Root->Type == EJson::Object)
	{
		const TSharedPtr<FJsonObject> Obj = Root->AsObject();
		if (Obj->HasField(TEXT("casesPilot")))
		{
			ParseCaseArray(Obj->GetArrayField(TEXT("casesPilot")), PilotCases);
		}
		if (Obj->HasField(TEXT("casesMain")))
		{
			ParseCaseArray(Obj->GetArrayField(TEXT("casesMain")), MainCases);
		}
		// 구 포맷 호환: { cases:[...] } → MainCases에 적재.
		if (PilotCases.Num() == 0 && MainCases.Num() == 0 && Obj->HasField(TEXT("cases")))
		{
			ParseCaseArray(Obj->GetArrayField(TEXT("cases")), MainCases);
		}
	}
	else if (Root->Type == EJson::Array)
	{
		// 최구 포맷: top-level array → MainCases로.
		ParseCaseArray(Root->AsArray(), MainCases);
	}

	const bool bAnyLoaded = (PilotCases.Num() + MainCases.Num()) > 0;
	if (bAnyLoaded)
	{
		UE_LOG(LogTemp, Log, TEXT("[RHI] Cases.json loaded: pilot=%d main=%d"), PilotCases.Num(), MainCases.Num());
	}
	return bAnyLoaded;
}

ARubberHandPawn* UExperimentManagerComponent::GetOwnerPawn() const
{
	return Cast<ARubberHandPawn>(GetOwner());
}

void UExperimentManagerComponent::StartExperiment()
{
	if (bRunning) return;
	if (Cases.Num() == 0) { ApplyModeCases(); }
	if (Cases.Num() == 0) { InitDefaultCases(); ApplyModeCases(); }

	bRunning = true;
	ExperimentStartTime = GetWorld()->GetTimeSeconds();
	LogEvent(TEXT("ExperimentStart"),
		FString::Printf(TEXT("mode=%s cases=%d"),
			CurrentMode == ERHIExperimentMode::Pilot ? TEXT("Pilot") : TEXT("Main"),
			Cases.Num()));
	BeginCase(0);
}

void UExperimentManagerComponent::StopExperiment()
{
	if (!bRunning) return;
	DespawnStimulusActors();
	ClearSurveyWidget();
	LogEvent(TEXT("ExperimentStop"), TEXT(""));
	ReturnToIdle();
}

void UExperimentManagerComponent::SetExperimentMode(ERHIExperimentMode NewMode)
{
	if (CurrentMode == NewMode && Cases.Num() > 0) return;
	if (bRunning) { StopExperiment(); }
	CurrentMode = NewMode;
	ApplyModeCases();
	LogEvent(TEXT("ModeChange"),
		FString::Printf(TEXT("mode=%s count=%d"),
			NewMode == ERHIExperimentMode::Pilot ? TEXT("Pilot") : TEXT("Main"),
			Cases.Num()));
}

void UExperimentManagerComponent::ToggleExperimentMode()
{
	SetExperimentMode(CurrentMode == ERHIExperimentMode::Pilot
		? ERHIExperimentMode::Main
		: ERHIExperimentMode::Pilot);
}

void UExperimentManagerComponent::SelectCase(int32 CaseId)
{
	// 패널 버튼 매핑 (1~9):
	//   9 = Stop
	//   8 = Pilot/Main 모드 토글
	//   1~7 = 현재 모드 케이스 N번째 (Pilot은 1~7, Main은 1~5만 유효)
	if (CaseId == 9)
	{
		if (bRunning) { StopExperiment(); }
		LogEvent(TEXT("ManualStop"), TEXT(""));
		return;
	}
	if (CaseId == 8)
	{
		ToggleExperimentMode();
		return;
	}
	if (CaseId < 1 || CaseId > 7) return;

	if (Cases.Num() == 0) { ApplyModeCases(); }
	if (Cases.Num() == 0) { InitDefaultCases(); ApplyModeCases(); }
	const int32 Index = CaseId - 1;
	if (!Cases.IsValidIndex(Index))
	{
		UE_LOG(LogTemp, Warning, TEXT("[RHI] SelectCase %d out of range (mode has %d cases)"), CaseId, Cases.Num());
		return;
	}

	// 진행 중이면 정리 후 즉시 새 케이스 시작.
	if (bRunning)
	{
		if (UWorld* W = GetWorld()) { W->GetTimerManager().ClearTimer(CaseTimer); }
		DespawnStimulusActors();
		ClearSurveyWidget();
	}

	bRunning = true;
	if (ExperimentStartTime <= 0.0)
	{
		ExperimentStartTime = GetWorld()->GetTimeSeconds();
	}
	LogEvent(TEXT("ManualSelect"), FString::Printf(TEXT("mode=%s caseId=%d"),
		CurrentMode == ERHIExperimentMode::Pilot ? TEXT("Pilot") : TEXT("Main"),
		CaseId));
	BeginCase(Index);
}

FString UExperimentManagerComponent::GetLegendString() const
{
	const TCHAR* ModeTag = (CurrentMode == ERHIExperimentMode::Pilot) ? TEXT("PILOT") : TEXT("MAIN");
	FString Out = FString::Printf(TEXT("[%s MODE]   [8]: switch  [9]: stop\n"), ModeTag);
	Out += TEXT("--------------------------------\n");

	for (int32 i = 0; i < Cases.Num() && i < 7; ++i)
	{
		const FRHICaseSpec& C = Cases[i];
		const TCHAR* StimStr =
			C.Stimulus == ERHIStimulusType::BrushSync    ? TEXT("BrushSync")  :
			C.Stimulus == ERHIStimulusType::BrushAsync   ? TEXT("BrushAsync") :
			C.Stimulus == ERHIStimulusType::HammerThreat ? TEXT("Hammer")     : TEXT("None");
		Out += FString::Printf(
			TEXT("[%d] %4.0fcm  %5.0fms  %s\n     %s\n"),
			i + 1, C.SpatialErrorCm, C.TemporalDelayMs, StimStr, *C.Label);
	}
	return Out;
}

FString UExperimentManagerComponent::GetStatusString() const
{
	const TCHAR* ModeTag = (CurrentMode == ERHIExperimentMode::Pilot) ? TEXT("PILOT") : TEXT("MAIN");
	const int32 N = Cases.Num();

	// 누적 세션 시간 (1시간 budget 대비 진행률).
	float SessionMin = 0.f;
	if (ExperimentStartTime > 0.0 && GetWorld())
	{
		SessionMin = static_cast<float>((GetWorld()->GetTimeSeconds() - ExperimentStartTime) / 60.0);
	}

	if (!bRunning || !Cases.IsValidIndex(CurrentCaseIndex))
	{
		// IDLE 표시 — [8]=ModeToggle, [9]=Stop. 1~N은 현재 모드 케이스 수.
		return FString::Printf(
			TEXT("IDLE  [%s %d cases]\n[1]~[%d]:Case  [8]:Mode  [9]:Stop\nSession %.1f / 60 min"),
			ModeTag, N, FMath::Min(N, 7), SessionMin);
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
		TEXT("[%s] %d / %d  (%s)\n%.0fcm  %.0fms  %s\n%.1fs left   Session %.1f/60 min"),
		ModeTag, CurrentCaseIndex + 1, N, *C.Label,
		C.SpatialErrorCm, C.TemporalDelayMs, StimStr, Remain, SessionMin);
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

	// 자동 진행 OFF — 케이스 끝나면 IDLE로 돌아감. 사용자가 패널 버튼으로 다음 케이스 직접 선택.
	if (C.bShowSurveyOnEnd)
	{
		SpawnSurveyWidget();
	}
	else
	{
		// 설문 없이 그대로 IDLE.
		ReturnToIdle();
	}
}

void UExperimentManagerComponent::OnSurveySubmitted(const FRHISurveyResponse& Response)
{
	LogEvent(TEXT("Survey"),
		FString::Printf(TEXT("caseId=%d ownership=%d distort=%d visualDom=%d"),
			Response.CaseId, Response.BodyOwnership, Response.TactileDistort, Response.VisualDominance));
	ClearSurveyWidget();
	// 자동 진행 X — 설문 후에도 IDLE로 돌아감.
	ReturnToIdle();
}

void UExperimentManagerComponent::ReturnToIdle()
{
	if (UWorld* W = GetWorld()) { W->GetTimerManager().ClearTimer(CaseTimer); }
	bRunning = false;
	CurrentCaseIndex = INDEX_NONE;

	// 가상 손 효과 리셋 (다음 선택 전까지 동기화 상태로).
	if (ARubberHandPawn* P = GetOwnerPawn())
	{
		P->SetSpatialOffsetCm(0.f);
		P->SetTemporalDelayMs(0.f);
	}
	FlushCsv();
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
