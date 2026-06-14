// Unity-include of the pure measurement core (zero-UE C++17) into the UE module.
// Single TU so the core needs no UBT module / export macros; the canonical sources stay
// in Plugins/LevelSim/Core and are gated independently by Standalone/build.bat (L1..L16).
#include "CoreMinimal.h"

THIRD_PARTY_INCLUDES_START
#include "LevelCore.cpp"
THIRD_PARTY_INCLUDES_END
