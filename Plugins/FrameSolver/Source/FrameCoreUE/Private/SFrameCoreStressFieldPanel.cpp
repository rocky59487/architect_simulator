#include "FrameCoreUE/SFrameCoreStressFieldPanel.h"

#if WITH_EDITOR

#include "FrameCoreUE/FrameCoreUELibrary.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

void SFrameCoreStressFieldPanel::Construct(const FArguments& /*InArgs*/)
{
    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8.f)
        [
            SNew(SButton)
            .Text(FText::FromString(TEXT("Compute Cantilever Stress Field")))
            .OnClicked(this, &SFrameCoreStressFieldPanel::OnComputeClicked)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8.f)
        [
            SAssignNew(ResultText, STextBlock)
            .Text(FText::FromString(TEXT("(no result yet -- click Compute)")))
        ]
        + SVerticalBox::Slot()
        .FillHeight(1.f)
        .Padding(8.f)
        [
            SAssignNew(SampleListView, SListView<TSharedPtr<FFrameStressFieldSample>>)
            .ListItemsSource(&ListItems)
            .OnGenerateRow(this, &SFrameCoreStressFieldPanel::OnGenerateSampleRow)
        ]
    ];
}

FReply SFrameCoreStressFieldPanel::OnComputeClicked()
{
    CurrentField = UFrameCoreStressFieldLibrary::ComputeCantileverFixture(1000.f, 2000.f, 11);

    if (ResultText.IsValid())
    {
        const FString Summary = FString::Printf(
            TEXT("Global max fiber sigma: %.4f MPa   Governing member id: %d   Members: %d"),
            CurrentField.GlobalMaxFiberSigma,
            CurrentField.GoverningMemberId,
            CurrentField.Members.Num());
        ResultText->SetText(FText::FromString(Summary));
    }

    ListItems.Reset();
    if (CurrentField.Members.Num() > 0)
    {
        for (const FFrameStressFieldSample& s : CurrentField.Members[0].Samples)
        {
            ListItems.Add(MakeShared<FFrameStressFieldSample>(s));
        }
    }
    if (SampleListView.IsValid()) { SampleListView->RequestListRefresh(); }

    return FReply::Handled();
}

TSharedRef<ITableRow> SFrameCoreStressFieldPanel::OnGenerateSampleRow(
    TSharedPtr<FFrameStressFieldSample> Item,
    const TSharedRef<STableViewBase>& OwnerTable)
{
    const FString Row = FString::Printf(
        TEXT("  x = %7.1f   sigComp = %10.3f   sigTens = %10.3f   tauShear = %10.3f   tauTorsion = %10.3f"),
        Item->X, Item->SigmaCompMax, Item->SigmaTensMax,
        Item->TauShear, Item->TauTorsion);
    return SNew(STableRow<TSharedPtr<FFrameStressFieldSample>>, OwnerTable)
        [
            SNew(STextBlock).Text(FText::FromString(Row))
        ];
}

#endif // WITH_EDITOR
