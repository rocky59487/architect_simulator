#pragma once
//
// Structural fingerprint of a FrameModel — the hash assembleAndFactor() bakes into the
// PreparedSystem and solveLoad() / solveLoadHP() check before reusing a factorization.
// Moved out of FrameSolver.cpp's anonymous namespace so the HP-FEM opt-in lane
// (HpSolver.cpp) can run the SAME reuse-validity guard without duplicating it. Pure POD
// over FrameModel; no Eigen. Logic is byte-identical to the previous in-file definition.
//
#include "FrameCore/FrameModel.h"
#include "FrameCore/FrameTypes.h"

#include <cstdint>
#include <cstring>

namespace frame {

// Structural fingerprint: hashes everything solveLoad() must NOT change between an
// assembleAndFactor() and its reuse — node id/positions/support FLAGS, member
// id/connectivity/refVec/releases/matIdx/secIdx/active, shell id/connectivity/thickness/matIdx/active,
// the referenced material VALUES (E/G/nu/rho) and section VALUES (A/Iy/Iz/J/Asy/Asz), the
// baked distributed loads (member UDLs, shell pressures), and the plastic hinges (a hinge
// changes BOTH the condensed stiffness and the baked fixed-end forces). The factorization
// bakes in the stiffness those properties imply, so changing E, Iz, or which material/section
// an element points to would make a reused factorization a SILENT STALE SOLVE — they must be
// fingerprinted (the section/material values were the gap; see PROJECT.txt P1). It deliberately
// EXCLUDES nodal loads and prescribed VALUES, which solveLoad is allowed to vary (the
// interactive / settlement path), and the capacity-side fields fy/Zy/Zz/cap (post-processing
// screens only — they never enter K or the baked loads).
inline uint64_t fpMix(uint64_t h, uint64_t v) {
    return h ^ (v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
}
inline uint64_t fpBits(real d) {
    uint64_t u = 0; std::memcpy(&u, &d, sizeof(real)); return u;
}
inline uint64_t modelFingerprint(const FrameModel& m) {
    uint64_t h = 1469598103934665603ULL;
    h = fpMix(h, m.nodes.size());
    h = fpMix(h, m.members.size());
    h = fpMix(h, m.shells.size());
    h = fpMix(h, m.memberUDLs.size());
    h = fpMix(h, m.shellPressures.size());
    for (const auto& n : m.nodes) {
        h = fpMix(h, static_cast<uint64_t>(n.id));
        h = fpMix(h, fpBits(n.pos.x)); h = fpMix(h, fpBits(n.pos.y)); h = fpMix(h, fpBits(n.pos.z));
        uint64_t fb = 0; for (int d = 0; d < 6; ++d) if (n.fixed[d]) fb |= (1ull << d);
        h = fpMix(h, fb);
    }
    for (const auto& mem : m.members) {
        h = fpMix(h, static_cast<uint64_t>(mem.id));
        h = fpMix(h, static_cast<uint64_t>(mem.i)); h = fpMix(h, static_cast<uint64_t>(mem.j));
        h = fpMix(h, static_cast<uint64_t>(static_cast<int64_t>(mem.matIdx)));
        h = fpMix(h, static_cast<uint64_t>(static_cast<int64_t>(mem.secIdx)));
        h = fpMix(h, fpBits(mem.refVec.x)); h = fpMix(h, fpBits(mem.refVec.y)); h = fpMix(h, fpBits(mem.refVec.z));
        uint64_t rb = 0; for (int d = 0; d < 12; ++d) if (mem.release[d]) rb |= (1ull << d);
        h = fpMix(h, rb);
        h = fpMix(h, mem.active ? 1ull : 0ull);   // toggling active is a structural (remove/restore) change
        h = fpMix(h, mem.tensionOnly ? 1ull : 0ull);   // flipping tension-only changes runTensionOnly semantics
    }
    for (const auto& sh : m.shells) {
        h = fpMix(h, static_cast<uint64_t>(static_cast<int64_t>(sh.id)));
        for (int k = 0; k < 4; ++k) h = fpMix(h, static_cast<uint64_t>(sh.n[k]));
        h = fpMix(h, static_cast<uint64_t>(static_cast<int64_t>(sh.matIdx)));
        h = fpMix(h, fpBits(sh.t));
        h = fpMix(h, sh.active ? 1ull : 0ull);   // toggling active is a structural (remove/restore) change
    }
    // Material / section VALUES the elements reference by index. Changing E / Iz / etc. alters
    // the assembled K, so a reused factorization built on the OLD values is a stale solve. We
    // hash all entries (not just referenced ones) — editing an unused entry is harmless
    // over-conservatism, while missing a used one is a silent correctness bug.
    for (const auto& mat : m.materials) {
        h = fpMix(h, fpBits(mat.E)); h = fpMix(h, fpBits(mat.G));
        h = fpMix(h, fpBits(mat.nu)); h = fpMix(h, fpBits(mat.rho));
    }
    for (const auto& s : m.sections) {
        h = fpMix(h, fpBits(s.A));   h = fpMix(h, fpBits(s.Iy));  h = fpMix(h, fpBits(s.Iz));
        h = fpMix(h, fpBits(s.J));   h = fpMix(h, fpBits(s.Asy)); h = fpMix(h, fpBits(s.Asz));
    }
    for (const auto& u : m.memberUDLs) {
        h = fpMix(h, static_cast<uint64_t>(u.member));
        h = fpMix(h, fpBits(u.w_local.x)); h = fpMix(h, fpBits(u.w_local.y)); h = fpMix(h, fpBits(u.w_local.z));
    }
    for (const auto& sp : m.shellPressures) {
        h = fpMix(h, static_cast<uint64_t>(sp.shell)); h = fpMix(h, fpBits(sp.p));
    }
    for (const auto& ph : m.hinges) {   // a hinge alters K (release) AND the baked Qf (Mp)
        h = fpMix(h, static_cast<uint64_t>(ph.member));
        h = fpMix(h, static_cast<uint64_t>(static_cast<int64_t>(ph.dof)));
        h = fpMix(h, fpBits(ph.Mp));
    }
    return h;
}

}  // namespace frame
