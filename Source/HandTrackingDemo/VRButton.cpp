#include "VRButton.h"

#include "VRLabelWidget.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"

AHTDButton::AHTDButton()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	ButtonMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ButtonMesh"));
	ButtonMesh->SetupAttachment(Root);
	// Engine basic cube is 100×100×100 cm. Button: 14 × 8 × 4 cm.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeAsset.Succeeded()) ButtonMesh->SetStaticMesh(CubeAsset.Object);
	ButtonMesh->SetRelativeScale3D(FVector(0.14f, 0.08f, 0.04f));
	ButtonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetupAttachment(Root);
	TriggerBox->SetBoxExtent(FVector(9.0f, 5.5f, 4.0f));
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AHTDButton::HandleOverlap);

	LabelWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("LabelWidget"));
	LabelWidget->SetupAttachment(Root);
	// Label plane floats ABOVE the cube (not on its face), facing the player (pawn is at -X looking +X).
	// The cube is 14×8×4 cm (half-height 2 cm in Z), so centering the ~22×11 cm label at Z=+9 cm puts its whole
	// footprint above the button face — multi-line Korean ("실험1\n고무손") no longer collides with / is clipped by
	// the button body (fixes the "글자가 버튼에 잘림" report; user asked to lift the text up).
	LabelWidget->SetRelativeLocation(FVector(-3.0f, 0.0f, 9.0f));
	LabelWidget->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	LabelWidget->SetWidgetSpace(EWidgetSpace::World);
	LabelWidget->SetDrawSize(FVector2D(440.0f, 220.0f));
	LabelWidget->SetWidgetClass(UVRLabelWidget::StaticClass());
	LabelWidget->SetWorldScale3D(FVector(0.05f)); // 440 × 220 px × 0.05 ≈ 22 × 11 cm plane, floating above the cube
	LabelWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AHTDButton::BeginPlay()
{
	Super::BeginPlay();
	ApplyLabel(); // widget instance is now initialized — push the cached text.
}

void AHTDButton::Configure(const FString& InLabel, int32 InButtonId, FLinearColor InColor)
{
	ButtonId          = InButtonId;
	PendingLabelText  = InLabel;
	PendingLabelColor = InColor;
	ApplyLabel();

	if (ButtonMesh)
	{
		if (UMaterialInstanceDynamic* MID = ButtonMesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), InColor);
		}
	}
}

void AHTDButton::ApplyLabel()
{
	if (!LabelWidget) return;
	if (UVRLabelWidget* W = Cast<UVRLabelWidget>(LabelWidget->GetUserWidgetObject()))
	{
		W->SetLabelText(FText::FromString(PendingLabelText));
		W->SetLabelColor(FLinearColor::White);
	}
}

void AHTDButton::SetEnabledState(bool bInEnabled)
{
	bEnabled = bInEnabled;
	SetActorHiddenInGame(!bInEnabled);
	SetActorEnableCollision(bInEnabled);
}

void AHTDButton::HandleOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* /*OtherActor*/,
                              UPrimitiveComponent* OtherComp, int32 /*OtherBodyIndex*/,
                              bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	if (!bEnabled || !OtherComp) return;
	if (!OtherComp->ComponentHasTag(TEXT("HandTouch"))) return;

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (Now - LastTriggerTime < RetriggerCooldown) return;
	LastTriggerTime = Now;

	OnPressed.Broadcast(ButtonId);
}
