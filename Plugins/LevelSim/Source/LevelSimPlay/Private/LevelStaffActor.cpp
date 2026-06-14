#include "LevelStaffActor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
    // Marks float this far (cm) in front of the board face to avoid z-fighting.
    constexpr float kProudCm  = 0.20f;
    constexpr float kMarkThk  = 0.10f;   // mark box thickness (cm)
    constexpr float kBoardThk = 2.0f;    // board thickness (cm); face at local x = -1
    constexpr float kBoardW   = 14.0f;   // board width (cm)

    UStaticMesh* CubeMesh()
    {
        return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    }
    UMaterialInterface* BaseMat()
    {
        return LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    }
    UMaterialInstanceDynamic* TintedMID(UObject* Outer, const FLinearColor& C)
    {
        UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat(), Outer);
        if (MID) MID->SetVectorParameterValue(TEXT("Color"), C);
        return MID;
    }
}

ALevelStaffActor::ALevelStaffActor()
{
    PrimaryActorTick.bCanEverTick = false;
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    Board = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Board"));
    Board->SetupAttachment(Root);
    Board->SetMobility(EComponentMobility::Movable);
    Board->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    BlackMarks = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BlackMarks"));
    RedMarks   = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RedMarks"));
    for (UInstancedStaticMeshComponent* ISM : { BlackMarks.Get(), RedMarks.Get() })
    {
        ISM->SetupAttachment(Root);
        ISM->SetMobility(EComponentMobility::Movable);
        ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        ISM->SetCastShadow(false);
    }
}

void ALevelStaffActor::AddBox(UInstancedStaticMeshComponent* ISM, float YCenterCm, float ZCenterCm,
                              float WidthCm, float HeightCm)
{
    // /Engine/BasicShapes/Cube is a centred 100 uu cube; 1 uu = 1 cm in this project.
    FTransform T;
    T.SetScale3D(FVector(kMarkThk / 100.f, WidthCm / 100.f, HeightCm / 100.f));
    T.SetLocation(FVector(-(kBoardThk * 0.5f + kProudCm), YCenterCm, ZCenterCm));
    ISM->AddInstance(T, /*bWorldSpace=*/false);
}

void ALevelStaffActor::AddSevenSegDigit(UInstancedStaticMeshComponent* ISM, int32 Digit,
                                        float YCenterCm, float ZCenterCm, float HCm, float WCm)
{
    //      A
    //    F   B        segment truth table, standard 7-seg
    //      G
    //    E   C
    //      D
    static const bool Seg[10][7] = {
        // A     B     C     D     E     F     G
        { true, true, true, true, true, true, false }, // 0
        { false,true, true, false,false,false,false }, // 1
        { true, true, false,true, true, false,true  }, // 2
        { true, true, true, true, false,false,true  }, // 3
        { false,true, true, false,false,true, true  }, // 4
        { true, false,true, true, false,true, true  }, // 5
        { true, false,true, true, true, true, true  }, // 6
        { true, true, true, false,false,false,false }, // 7
        { true, true, true, true, true, true, true  }, // 8
        { true, true, true, true, false,true, true  }, // 9
    };
    if (Digit < 0 || Digit > 9) return;
    const float t  = FMath::Max(0.30f, HCm * 0.12f); // stroke
    const float hh = HCm * 0.5f, hw = WCm * 0.5f;
    const float vLen = hh - t;                        // each vertical seg spans half height
    const bool* S = Seg[Digit];
    if (S[0]) AddBox(ISM, YCenterCm,        ZCenterCm + hh - t * 0.5f, WCm, t);          // A
    if (S[6]) AddBox(ISM, YCenterCm,        ZCenterCm,                 WCm, t);          // G
    if (S[3]) AddBox(ISM, YCenterCm,        ZCenterCm - hh + t * 0.5f, WCm, t);          // D
    if (S[1]) AddBox(ISM, YCenterCm + hw - t * 0.5f, ZCenterCm + vLen * 0.5f, t, vLen);  // B
    if (S[2]) AddBox(ISM, YCenterCm + hw - t * 0.5f, ZCenterCm - vLen * 0.5f, t, vLen);  // C
    if (S[5]) AddBox(ISM, YCenterCm - hw + t * 0.5f, ZCenterCm + vLen * 0.5f, t, vLen);  // F
    if (S[4]) AddBox(ISM, YCenterCm - hw + t * 0.5f, ZCenterCm - vLen * 0.5f, t, vLen);  // E
}

void ALevelStaffActor::BuildStaff(float LengthM)
{
    UStaticMesh* Cube = CubeMesh();
    if (!Cube) { UE_LOG(LogTemp, Error, TEXT("[LevelStaff] /Engine/BasicShapes/Cube missing")); return; }
    Board->SetStaticMesh(Cube);
    BlackMarks->SetStaticMesh(Cube);
    RedMarks->SetStaticMesh(Cube);

    Board->SetMaterial(0, TintedMID(this, FLinearColor(0.92f, 0.91f, 0.85f)));
    BlackMarks->SetMaterial(0, TintedMID(this, FLinearColor(0.01f, 0.01f, 0.01f)));
    RedMarks->SetMaterial(0, TintedMID(this, FLinearColor(0.55f, 0.02f, 0.02f)));

    const float LenCm = LengthM * 100.f;
    Board->SetRelativeScale3D(FVector(kBoardThk / 100.f, kBoardW / 100.f, LenCm / 100.f));
    Board->SetRelativeLocation(FVector(0, 0, LenCm * 0.5f));

    // Mark columns: two 4 cm-wide E columns (checkerboard) + digits column on the right.
    const float ColL = -4.0f, ColR = 0.0f, ColW = 4.0f, ColDig = 5.0f;

    const int32 NumBlocks = FMath::FloorToInt32(LenCm / 5.f); // 5 cm E blocks
    for (int32 b = 0; b < NumBlocks; ++b)
    {
        const float z0   = b * 5.f;
        const int32 m    = b / 20;                       // metre index (20 blocks per metre)
        UInstancedStaticMeshComponent* ISM = (m % 2 == 0) ? BlackMarks.Get() : RedMarks.Get();
        const float yc   = (b % 2 == 0) ? ColL : ColR;   // checkerboard columns
        // E: three 1 cm bars at +0..1, +2..3, +4..5 …
        AddBox(ISM, yc, z0 + 0.5f, ColW, 1.f);
        AddBox(ISM, yc, z0 + 2.5f, ColW, 1.f);
        AddBox(ISM, yc, z0 + 4.5f, ColW, 1.f);
        // … plus a 5 cm spine on the column's inner edge connecting them.
        AddBox(ISM, yc + ColW * 0.5f - 0.5f, z0 + 2.5f, 1.f, 5.f);
    }

    // dm numerals "<m><dm>" centred on each dm line (skip 0.0 — below it there is no room).
    const int32 NumDm = FMath::FloorToInt32(LenCm / 10.f);
    for (int32 d = 1; d < NumDm; ++d)
    {
        const int32 m  = d / 10, dm = d % 10;
        UInstancedStaticMeshComponent* ISM = (m % 2 == 0) ? BlackMarks.Get() : RedMarks.Get();
        const float z = d * 10.f;
        AddSevenSegDigit(ISM, m,  ColDig - 1.0f, z, 3.0f, 1.6f);
        AddSevenSegDigit(ISM, dm, ColDig + 1.0f, z, 3.0f, 1.6f);
    }
}
