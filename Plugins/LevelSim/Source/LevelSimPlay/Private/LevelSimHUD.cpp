#include "LevelSimHUD.h"
#include "LevelSimPawn.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/PlayerController.h"

namespace
{
    const FLinearColor kInk(0.95f, 0.95f, 0.92f);
    const FLinearColor kDim(0.75f, 0.75f, 0.70f, 0.9f);
    const FLinearColor kGood(0.3f, 0.95f, 0.35f);
    const FLinearColor kWarn(1.0f, 0.75f, 0.1f);
    const FLinearColor kBad(1.0f, 0.25f, 0.2f);
    const FLinearColor kHair(0.05f, 0.05f, 0.05f);
    UFont* Big()  { return GEngine ? GEngine->GetLargeFont() : nullptr; }
}

void ALevelSimHUD::DrawCircle(float CX, float CY, float R, int32 Segs, const FLinearColor& C, float Thick)
{
    float PX = CX + R, PY = CY;
    for (int32 i = 1; i <= Segs; ++i)
    {
        const float A = 2.f * PI * i / Segs;
        const float X = CX + R * FMath::Cos(A), Y = CY + R * FMath::Sin(A);
        DrawLine(PX, PY, X, Y, C, Thick);
        PX = X; PY = Y;
    }
}

void ALevelSimHUD::DrawHUD()
{
    Super::DrawHUD();
    ALevelSimPawn* P = Cast<ALevelSimPawn>(GetOwningPawn());
    if (!P || !Canvas) return;

    switch (P->Phase)
    {
    case ELevelPhase::Overview:     DrawOverview(P);  break;
    case ELevelPhase::Leveling:     DrawLeveling(P);  break;
    case ELevelPhase::Telescope:    DrawTelescope(P); break;
    case ELevelPhase::Booking:      DrawBooking(P, false); break;
    case ELevelPhase::Done:         DrawBooking(P, true);  break;
    case ELevelPhase::RouteSummary: DrawRouteSummary(P);   break;
    }
    DrawMessage(P);
}

void ALevelSimHUD::DrawMessage(ALevelSimPawn* P)
{
    if (P->Msg.IsEmpty() || !GetWorld() || GetWorld()->GetTimeSeconds() > P->MsgUntil) return;
    const float W = Canvas->SizeX, H = Canvas->SizeY;
    DrawRect(FLinearColor(0, 0, 0, 0.55f), W * 0.5f - 430, H - 88, 860, 34);
    DrawText(P->Msg, kWarn, W * 0.5f - 420, H - 82, Big(), 1.25f);
}

void ALevelSimHUD::DrawOverview(ALevelSimPawn* P)
{
    const levelsim::BubbleState B = P->CurrentBubble();
    DrawRect(FLinearColor(0, 0, 0, 0.45f), 16, 16, 560, 132);

    const int32 NumLegs = FMath::Max(1, P->NumLegs());
    const FString BSName = P->RoutePoints.IsValidIndex(P->CurrentStationIdx)
                         ? P->RoutePoints[P->CurrentStationIdx].Name : FString(TEXT("BS"));
    const FString FSName = P->RoutePoints.IsValidIndex(P->CurrentStationIdx + 1)
                         ? P->RoutePoints[P->CurrentStationIdx + 1].Name : FString(TEXT("FS"));
    const FString Title  = NumLegs > 1
        ? FString::Printf(TEXT("LEVELLING ROUTE  Station %d / %d: %s -> %s"),
                          P->CurrentStationIdx + 1, NumLegs, *BSName, *FSName)
        : FString::Printf(TEXT("LEVELLING STATION  %s -> %s"), *BSName, *FSName);
    DrawText(Title, kInk, 28, 24, Big(), 1.6f);
    DrawText(TEXT("[L] level instrument   [T] telescope   [R] restart route"), kDim, 28, 56, Big(), 1.2f);
    DrawText(FString::Printf(TEXT("Bubble: %.2f div  %s"), B.magDiv,
             B.fineLevel ? TEXT("(FINE LEVEL OK)") : (B.roughLevel ? TEXT("(rough only)") : TEXT("(NOT level)"))),
             B.fineLevel ? kGood : (B.roughLevel ? kWarn : kBad), 28, 84, Big(), 1.2f);
    const FString JobTxt =
        P->Job == ELevelJob::ReadBS ? FString::Printf(TEXT("Task: read BACKSIGHT (BS) on the %s staff"), *BSName) :
        P->Job == ELevelJob::ReadFS ? FString::Printf(TEXT("Task: read FORESIGHT (FS) on the %s staff"), *FSName) :
                                      FString(TEXT("Task: fill the field book"));
    DrawText(JobTxt, kInk, 28, 112, Big(), 1.2f);
}

void ALevelSimHUD::DrawLeveling(ALevelSimPawn* P)
{
    const float W = Canvas->SizeX, H = Canvas->SizeY;
    const levelsim::BubbleState B = P->CurrentBubble();

    // circular vial close-up, right of centre; 14 px per division, housing = 8 div
    const float CX = W * 0.68f, CY = H * 0.46f, PxDiv = 14.f, RHouse = 8.f * PxDiv;
    DrawRect(FLinearColor(0.06f, 0.08f, 0.06f, 0.85f), CX - RHouse - 24, CY - RHouse - 24, 2 * (RHouse + 24), 2 * (RHouse + 24));
    DrawCircle(CX, CY, RHouse, 64, kDim, 2.f);
    for (int32 g = 2; g <= 6; g += 2)
        DrawCircle(CX, CY, g * PxDiv, 48, FLinearColor(0.4f, 0.45f, 0.4f, 0.5f), 1.f);
    DrawCircle(CX, CY, FMath::Max(4.f, (float)P->CoreParams.bubbleRoughDiv * PxDiv), 48, kWarn, 1.5f);

    // Bubble overlay must match the close-up camera orientation (yaw 0, pitched down,
    // viewer behind on -X): screen-right = world +Y (core offY), screen-up = world +X
    // (core offX). Otherwise the bubble disagrees with the 3D screw positions below it.
    const float BX = CX + FMath::Clamp((float)B.offY, -7.5f, 7.5f) * PxDiv;
    const float BY = CY - FMath::Clamp((float)B.offX, -7.5f, 7.5f) * PxDiv;
    const FLinearColor BubbleC = B.fineLevel ? kGood : (B.roughLevel ? kWarn : FLinearColor(0.7f, 0.95f, 0.6f));
    DrawRect(BubbleC, BX - 7, BY - 7, 14, 14);
    DrawCircle(BX, BY, 9, 24, BubbleC, 2.f);

    DrawRect(FLinearColor(0, 0, 0, 0.45f), 16, 16, 620, 158);
    DrawText(TEXT("PRECISE LEVELLING - foot screws"), kInk, 28, 24, Big(), 1.5f);
    DrawText(FString::Printf(TEXT("[1][2][3] choose screw (now: %d)   wheel = coarse   Shift+wheel = fine"), P->SelectedScrew + 1), kDim, 28, 54, Big(), 1.15f);
    DrawText(TEXT("Bubble runs to the HIGH side: lower that side / raise the other."), kDim, 28, 78, Big(), 1.1f);
    for (int32 i = 0; i < 3; ++i)
        DrawText(FString::Printf(TEXT("screw %d travel: %+.3f mm%s"), i + 1, P->ScrewTravel[i] * 1000.0,
                 i == P->SelectedScrew ? TEXT("   <--") : TEXT("")),
                 i == P->SelectedScrew ? kInk : kDim, 28, 102 + 22 * i, Big(), 1.1f);

    DrawText(FString::Printf(TEXT("offset %.2f div"), B.magDiv),
             B.fineLevel ? kGood : (B.roughLevel ? kWarn : kBad), CX - 50, CY + RHouse + 10, Big(), 1.3f);
    if (B.fineLevel)
        DrawText(TEXT("FINE LEVEL OK -> [T] telescope"), kGood, CX - 130, CY - RHouse - 50, Big(), 1.4f);
    DrawText(TEXT("[T] telescope   [Esc] step back"), kDim, 28, H - 40, Big(), 1.1f);
}

void ALevelSimHUD::DrawTelescope(ALevelSimPawn* P)
{
    const float W = Canvas->SizeX, H = Canvas->SizeY;
    const float CX = W * 0.5f, CY = H * 0.5f;

    // round-ish eyepiece mask (letterbox to a centred square, then corner fills)
    const float Hole = H * 0.98f;
    DrawRect(FLinearColor::Black, 0, 0, (W - Hole) * 0.5f, H);
    DrawRect(FLinearColor::Black, W - (W - Hole) * 0.5f, 0, (W - Hole) * 0.5f, H);

    const levelsim::SightState S = P->CurrentSight();
    if (!S.readable)
    {
        DrawRect(FLinearColor(0.05f, 0.05f, 0.05f, 0.88f), 0, 0, W, H);
        DrawText(TEXT("IMAGE UNSTABLE - COMPENSATOR OUT OF RANGE"), kBad, CX - 280, CY - 40, Big(), 1.7f);
        DrawText(TEXT("Re-level the instrument: [Esc] then [L]"), kInk, CX - 200, CY + 6, Big(), 1.3f);
        return;
    }

    // crosshair + stadia hairs. Vertical pixel scale comes from the camera projection:
    // px = tan(angle) / tan(vFov/2) * H/2  (small angles, exact enough at 0.3 deg).
    const float HFov = ALevelSimPawn::TelescopeHFovDeg;
    const float VTan = FMath::Tan(FMath::DegreesToRadians(HFov * 0.5f)) * (H / W);
    const double Alpha = FMath::Atan(1.0 / (2.0 * P->CoreParams.stadiaK));
    const float StadiaPx = (float)(FMath::Tan(Alpha) / VTan) * (H * 0.5f);

    // 視差 stub: unresolved parallax wobbles the apparent hair position (read it wrong
    // and the score shows it). parallaxJitter() is metres of reading jitter at the staff.
    const FStaffTarget* Sighted = P->SightedTarget();
    float JitterPx = 0.f;
    if (Sighted)
    {
        const double D = Sighted->DistM();
        const double JitterM = levelsim::parallaxJitter(P->CoreParams, P->MakeSetup(), D);
        const double JitterAng = FMath::Atan(JitterM / FMath::Max(D, 1.0));
        const float  MaxPx = (float)(FMath::Tan(JitterAng) / VTan) * (H * 0.5f);
        JitterPx = MaxPx * FMath::Sin(GetWorld()->GetTimeSeconds() * 5.0f);
    }

    const float HairY = CY + JitterPx;
    DrawLine(0, HairY, W, HairY, kHair, 2.0f);                       // mid hair
    DrawLine(CX, 0, CX, H, kHair, 2.0f);                              // vertical hair
    DrawLine(CX - 60, HairY - StadiaPx, CX + 60, HairY - StadiaPx, kHair, 1.5f); // upper stadia
    DrawLine(CX - 60, HairY + StadiaPx, CX + 60, HairY + StadiaPx, kHair, 1.5f); // lower stadia

    DrawRect(FLinearColor(0, 0, 0, 0.5f), 16, 16, 540, 116);
    DrawText(TEXT("TELESCOPE"), kInk, 28, 22, Big(), 1.4f);
    DrawText(FString::Printf(TEXT("brake: %s   focus: %.1f m   [C] brake  [F]+wheel focus"),
             P->bBrakeOn ? TEXT("ON") : TEXT("off"), P->FocusM), kDim, 28, 50, Big(), 1.1f);
    DrawText(P->bBrakeOn ? TEXT("wheel = tangent screw (Shift = finer)   [Enter] read")
                         : TEXT("mouse = coarse aim   then [C] to brake"), kDim, 28, 74, Big(), 1.1f);
    DrawText(Sighted ? FString::Printf(TEXT("sighted: %s"), *Sighted->Name) : FString(TEXT("sighted: (nothing)")),
             Sighted ? kGood : kDim, 28, 98, Big(), 1.1f);

    if (P->bTyping) DrawTypingBox(P, TEXT("READING [m]"));
}

void ALevelSimHUD::DrawTypingBox(ALevelSimPawn* P, const FString& Label)
{
    const float W = Canvas->SizeX, H = Canvas->SizeY;
    DrawRect(FLinearColor(0, 0, 0, 0.8f), W * 0.5f - 220, H * 0.72f, 440, 64);
    DrawText(FString::Printf(TEXT("%s: %s_"), *Label, *P->TypeBuf), kInk, W * 0.5f - 200, H * 0.72f + 16, Big(), 1.7f);
}

void ALevelSimHUD::DrawBooking(ALevelSimPawn* P, bool bFinal)
{
    const float W = Canvas->SizeX;
    const float X = W * 0.5f - 330, Y = 90;
    DrawRect(FLinearColor(0.02f, 0.02f, 0.05f, 0.88f), X - 20, Y - 24, 700, bFinal ? 330 : 250);
    DrawText(TEXT("FIELD BOOK - height of instrument method"), kInk, X, Y - 12, Big(), 1.5f);

    auto Row = [&](int32 i, const FString& K, const FString& V, const FLinearColor& C)
    { DrawText(K, kDim, X, Y + 28 + i * 26, Big(), 1.2f); DrawText(V, C, X + 320, Y + 28 + i * 26, Big(), 1.2f); };

    const FString BSName = P->RoutePoints.IsValidIndex(P->CurrentStationIdx)
                         ? P->RoutePoints[P->CurrentStationIdx].Name : FString(TEXT("BM"));
    const FString FSName = P->RoutePoints.IsValidIndex(P->CurrentStationIdx + 1)
                         ? P->RoutePoints[P->CurrentStationIdx + 1].Name : FString(TEXT("P1"));

    Row(0, FString::Printf(TEXT("%s elevation (known)"), *BSName),
        FString::Printf(TEXT("%.3f m"), P->CurrentKnownElevM), kInk);
    Row(1, FString::Printf(TEXT("BS on %s"), *BSName), FString::Printf(TEXT("%.3f m"), P->PlayerBS), kInk);
    Row(2, FString::Printf(TEXT("H.I. = %s + BS"), *BSName),
        P->Job == ELevelJob::FillHI ? TEXT("?  (type it)") : FString::Printf(TEXT("%.3f m"), P->PlayerHI),
        P->Job == ELevelJob::FillHI ? kWarn : (P->bHIok ? kGood : kBad));
    Row(3, FString::Printf(TEXT("FS on %s"), *FSName), FString::Printf(TEXT("%.3f m"), P->PlayerFS), kInk);
    Row(4, FString::Printf(TEXT("%s elev = H.I. - FS"), *FSName),
        P->Job == ELevelJob::FillElev ? TEXT("?  (type it)") :
        (P->Job == ELevelJob::FillHI ? TEXT("-") : FString::Printf(TEXT("%.3f m"), P->PlayerElev)),
        P->Job == ELevelJob::FillElev ? kWarn : (P->bElevOk ? kGood : kBad));

    if (!bFinal)
    {
        if (P->bTyping)
            DrawTypingBox(P, P->Job == ELevelJob::FillHI ? TEXT("H.I. [m]") : TEXT("P1 elev [m]"));
    }
    else
    {
        const float SY = Y + 170;
        DrawText(TEXT("------- RESULT -------"), kInk, X, SY, Big(), 1.3f);
        DrawText(FString::Printf(TEXT("BS reading: %.0f/30   (true %.4f, err %+.1f mm)"),
                 P->ScoreBS * 30.0, P->TruthBS, (P->PlayerBS - P->TruthBS) * 1000.0), kDim, X, SY + 26, Big(), 1.15f);
        DrawText(FString::Printf(TEXT("FS reading: %.0f/30   (true %.4f, err %+.1f mm)"),
                 P->ScoreFS * 30.0, P->TruthFS, (P->PlayerFS - P->TruthFS) * 1000.0), kDim, X, SY + 50, Big(), 1.15f);
        DrawText(FString::Printf(TEXT("H.I. arithmetic: %s   elevation arithmetic: %s"),
                 P->bHIok ? TEXT("20/20") : TEXT("0/20"), P->bElevOk ? TEXT("20/20") : TEXT("0/20")), kDim, X, SY + 74, Big(), 1.15f);
        const bool bPass = P->TotalScore >= 60;
        DrawText(FString::Printf(TEXT("TOTAL %d / 100  -  %s   (instrument truth: %s elev %.4f m)"),
                 P->TotalScore, bPass ? TEXT("PASS") : TEXT("FAIL"), *FSName, P->TrueElevM()),
                 bPass ? kGood : kBad, X, SY + 102, Big(), 1.35f);
        DrawText(TEXT("[R] new station"), kInk, X, SY + 130, Big(), 1.2f);
    }
}

void ALevelSimHUD::DrawRouteSummary(ALevelSimPawn* P)
{
    const float W = Canvas->SizeX;
    const float X = W * 0.5f - 360, Y = 60;
    const int32 N = P->LegRecords.Num();
    const float PanelH = 130.f + 26.f * N + 200.f;
    DrawRect(FLinearColor(0.02f, 0.02f, 0.05f, 0.92f), X - 20, Y - 24, 760, PanelH);
    DrawText(FString::Printf(TEXT("ROUTE CLOSURE SUMMARY  (%d legs)"), N), kInk, X, Y - 12, Big(), 1.55f);

    // Per-leg header.
    auto Row2 = [&](int32 i, const FString& C0, const FString& C1, const FString& C2,
                    const FString& C3, const FString& C4, const FLinearColor& Tint)
    {
        const float YY = Y + 28 + i * 26;
        DrawText(C0, kDim, X,        YY, Big(), 1.15f);
        DrawText(C1, kDim, X + 80,   YY, Big(), 1.15f);
        DrawText(C2, kInk, X + 280,  YY, Big(), 1.15f);
        DrawText(C3, kInk, X + 430,  YY, Big(), 1.15f);
        DrawText(C4, Tint, X + 600,  YY, Big(), 1.15f);
    };
    Row2(0, TEXT("#"), TEXT("BS -> FS"), TEXT("dH (m)"), TEXT("adj dH (m)"), TEXT("score/100"), kInk);

    for (int32 i = 0; i < N; ++i)
    {
        const FLegRecord& LR = P->LegRecords[i];
        const FString BSn = P->RoutePoints.IsValidIndex(i)     ? P->RoutePoints[i].Name     : FString(TEXT("?"));
        const FString FSn = P->RoutePoints.IsValidIndex(i + 1) ? P->RoutePoints[i + 1].Name : FString(TEXT("?"));
        const double dH = LR.TruthBS - LR.TruthFS;
        const double AdjdH = P->CachedLoop.adjustedDh.size() > (size_t)i ? P->CachedLoop.adjustedDh[i] : dH;
        const int32 LegScore = FMath::RoundToInt32(
            30.0 * LR.ScoreBS + 30.0 * LR.ScoreFS
            + (LR.bHIok ? 20.0 : 0.0) + (LR.bElevOk ? 20.0 : 0.0));
        const FLinearColor Tint = LegScore >= 60 ? kGood : (LegScore >= 40 ? kWarn : kBad);
        Row2(i + 1,
             FString::Printf(TEXT("%d"), i + 1),
             FString::Printf(TEXT("%s -> %s"), *BSn, *FSn),
             FString::Printf(TEXT("%+.4f"), dH),
             FString::Printf(TEXT("%+.4f"), AdjdH),
             FString::Printf(TEXT("%d"), LegScore), Tint);
    }

    // Closure rows.
    const float CY = Y + 28 + (N + 2) * 26;
    const bool bWithinDist = P->CachedLoop.valid     && P->CachedLoop.withinTolerance;
    const bool bWithinSta  = P->CachedLoopBySta.valid && P->CachedLoopBySta.withinTolerance;
    const FLinearColor& MisclosureC = (P->bByDistance ? bWithinDist : bWithinSta) ? kGood : kBad;
    DrawText(FString::Printf(TEXT("Misclosure: %+.2f mm   [%s tolerance]"),
                              P->CachedLoop.misclosureM * 1000.0,
                              (P->bByDistance ? bWithinDist : bWithinSta) ? TEXT("WITHIN") : TEXT("EXCEEDS")),
             MisclosureC, X, CY, Big(), 1.25f);

    DrawText(FString::Printf(TEXT("  C*sqrt(L)  allowable = %.2f mm   [by-distance]"), P->CachedLoop.allowableMm),
             P->bByDistance ? kInk : kDim, X, CY + 26, Big(), 1.1f);
    DrawText(FString::Printf(TEXT("  C*sqrt(n)  allowable = %.2f mm   [by-station]"), P->CachedLoopBySta.allowableMm),
             P->bByDistance ? kDim : kInk, X, CY + 50, Big(), 1.1f);

    if (!P->CachedLoop.valid)
        DrawText(TEXT("  (note: degenerate weights -> equal-share fallback)"), kWarn, X, CY + 76, Big(), 1.05f);

    const float TY = CY + 110;
    const bool bPass = P->RouteTotal >= 60;
    DrawText(FString::Printf(TEXT("ROUTE SCORE: %d / 100  -  %s"),
                              P->RouteTotal, bPass ? TEXT("PASS") : TEXT("FAIL")),
             bPass ? kGood : kBad, X, TY, Big(), 1.4f);
    DrawText(TEXT("[R] restart route"), kInk, X, TY + 30, Big(), 1.15f);
}
