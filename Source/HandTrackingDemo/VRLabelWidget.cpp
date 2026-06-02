#include "VRLabelWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"

TSharedRef<SWidget> UVRLabelWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, FName(TEXT("WidgetTree")));
	}

	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		WidgetTree->RootWidget = RootCanvas;

		TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text"));

		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(RootCanvas->AddChild(TextBlock)))
		{
			CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			CanvasSlot->SetOffsets(FMargin(0.0f));
		}
	}

	if (TextBlock)
	{
		TextBlock->SetText(PendingText);
		FSlateFontInfo Font = TextBlock->GetFont();
		Font.Size = PendingSize;
		TextBlock->SetFont(Font);
		TextBlock->SetColorAndOpacity(FSlateColor(PendingColor));
		TextBlock->SetJustification(ETextJustify::Center);
		// 긴 한글 문항이 패널 폭을 넘으면 양옆으로 잘리지 않고 자동 줄바꿈되도록 함.
		TextBlock->SetAutoWrapText(true);
	}

	return Super::RebuildWidget();
}

void UVRLabelWidget::SetLabelText(const FText& InText)
{
	PendingText = InText;
	if (TextBlock) TextBlock->SetText(InText);
}

void UVRLabelWidget::SetLabelColor(FLinearColor InColor)
{
	PendingColor = InColor;
	if (TextBlock) TextBlock->SetColorAndOpacity(FSlateColor(InColor));
}

void UVRLabelWidget::SetLabelFontSize(float InSize)
{
	PendingSize = InSize;
	if (TextBlock)
	{
		FSlateFontInfo Font = TextBlock->GetFont();
		Font.Size = InSize;
		TextBlock->SetFont(Font);
	}
}
