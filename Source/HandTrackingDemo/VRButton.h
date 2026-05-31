#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VRButton.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UWidgetComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHTDButtonPressed, int32, ButtonId);

/**
 * AHTDButton — 3D hand-press button. A "HandTouch"-tagged primitive entering the box volume
 * triggers OnPressed with this button's ButtonId. Re-entry within RetriggerCooldown is ignored.
 *
 * Label is rendered through a UWidgetComponent (UMG) so the Slate Roboto composite font's CJK
 * fallback handles Korean — no separate font import needed.
 */
UCLASS()
class HANDTRACKINGDEMO_API AHTDButton : public AActor
{
	GENERATED_BODY()

public:
	AHTDButton();

	UPROPERTY(BlueprintAssignable, Category = "VRButton") FHTDButtonPressed OnPressed;

	UFUNCTION(BlueprintCallable, Category = "VRButton")
	void Configure(const FString& InLabel, int32 InButtonId, FLinearColor InColor);

	UFUNCTION(BlueprintCallable, Category = "VRButton")
	void SetEnabledState(bool bEnabled);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRButton") float RetriggerCooldown = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VRButton") UStaticMeshComponent* ButtonMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VRButton") UBoxComponent*        TriggerBox;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VRButton") UWidgetComponent*     LabelWidget;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	                   UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	                   bool bFromSweep, const FHitResult& SweepResult);

private:
	void ApplyLabel();

	int32        ButtonId        = 0;
	bool         bEnabled        = true;
	double       LastTriggerTime = -1000.0;
	FString      PendingLabelText;
	FLinearColor PendingLabelColor = FLinearColor::White;
};
