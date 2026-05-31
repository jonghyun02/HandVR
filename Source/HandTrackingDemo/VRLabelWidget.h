#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VRLabelWidget.generated.h"

class UTextBlock;

/**
 * UVRLabelWidget — Slate UMG 기반 라벨. UE5의 UnrealHeaderTool은 전 엔진+프로젝트 통틀어
 * 헤더 파일명 유니크를 요구함. 엔진의 `Plugins/Experimental/EditorDataStorage/.../LabelWidget.h`와
 * 충돌해서 `VRLabelWidget`으로 명명함.
 *
 * UTextRenderComponent의 한글 글리프 누락 문제를 우회하기 위해 도입.
 * Slate 기본 Roboto 컴포지트 폰트는 CJK fallback 서브-타입페이스를 포함하므로
 * UMG TextBlock으로 띄우면 별도 폰트 임포트 없이 한글 출력됨.
 *
 * 위젯 블루프린트 없이 순수 C++로 WidgetTree를 구성한다.
 */
UCLASS()
class HANDTRACKINGDEMO_API UVRLabelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Label") void SetLabelText(const FText& InText);
	UFUNCTION(BlueprintCallable, Category = "Label") void SetLabelColor(FLinearColor InColor);
	UFUNCTION(BlueprintCallable, Category = "Label") void SetLabelFontSize(float InSize);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY() UTextBlock* TextBlock = nullptr;

	FText        PendingText  = FText::FromString(TEXT(""));
	FLinearColor PendingColor = FLinearColor::White;
	float        PendingSize  = 48.0f;
};
