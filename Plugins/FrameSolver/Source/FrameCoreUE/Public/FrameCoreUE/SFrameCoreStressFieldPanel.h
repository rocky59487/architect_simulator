#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "FrameCoreUE/FrameCoreUETypes.h"

class STextBlock;
template <typename ItemType> class SListView;
class STableViewBase;
class ITableRow;

// Minimal editor utility panel that runs FrameCoreUE::ComputeCantileverFixture and
// renders the worst-element summary + the 11-sample table. Dev-only validation
// surface for the visualisation lane — production renderers (spline mesh / Niagara
// / colour-band shader) consume the same USTRUCT in v3.3 and beyond.
class FRAMECOREUE_API SFrameCoreStressFieldPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SFrameCoreStressFieldPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    // Public so the editor smoke test can drive the compute path without simulating
    // a real Slate click event.
    FReply OnComputeClicked();

private:
    TSharedRef<ITableRow> OnGenerateSampleRow(
        TSharedPtr<FFrameStressFieldSample> Item,
        const TSharedRef<STableViewBase>& OwnerTable);

    FFrameStressField CurrentField;
    TArray<TSharedPtr<FFrameStressFieldSample>> ListItems;
    TSharedPtr<STextBlock> ResultText;
    TSharedPtr<SListView<TSharedPtr<FFrameStressFieldSample>>> SampleListView;
};

#endif // WITH_EDITOR
