#include "FrameCore/Member.h"

namespace frame {

static_assert(sizeof(Member) > 0, "Member must be a complete type");

std::array<bool, 12> makeRelease(ReleasePreset p) {
    std::array<bool, 12> r{};   // all false (Rigid)
    switch (p) {
    case ReleasePreset::TrussPin:
        // all rotations both ends: Rx,Ry,Rz at i (3,4,5) and j (9,10,11)
        r[3] = r[4] = r[5] = r[9] = r[10] = r[11] = true; break;
    case ReleasePreset::HingeI:
        r[4] = r[5] = true;   break;   // Ry_i, Rz_i (bending) at end i
    case ReleasePreset::HingeJ:
        r[10] = r[11] = true; break;   // Ry_j, Rz_j at end j
    case ReleasePreset::Rigid:
    default:
        break;
    }
    return r;
}

} // namespace frame
