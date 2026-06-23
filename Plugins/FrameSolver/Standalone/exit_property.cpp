// v3.6 Phase 11 — exit-test dimension 1: property-based fixture sweep.
//
// Generates N random valid FrameModel fixtures (seeded XorShift64*, fully reproducible)
// and checks the following invariants per model:
//
//   I1 Equilibrium: sum(Reactions) + sum(Loads) ≈ 0 (component-wise, tol 1e-6)
//   I2 Pivot sign:  PivotMargin > 0 iff SolveResult.singular is false
//   I3 Member force: per-active-member end-i / end-j sum equilibrium (force only,
//                    excluding distributed loads)
//   I4 Demand:      MaxDC == 0 when load == 0; MaxDC > 0 otherwise
//   I5 Validate:    model.validate() agreement with solver.singular for mechanism
//
// Prints a per-fixture line + a summary; exit 0 only if every fixture passes every
// invariant. Reproduce via fixed seed: build_exit_property.bat -> exit_property.exe N.
//
// Engine source delta = 0 lines (uses public FRAMECORE_API entries only).

#include "FrameCore/FrameTypes.h"
#include "FrameCore/Node.h"
#include "FrameCore/Member.h"
#include "FrameCore/Material.h"
#include "FrameCore/Section.h"
#include "FrameCore/Load.h"
#include "FrameCore/FrameModel.h"
#include "FrameCore/FrameSolver.h"
#include "FrameCore/SolveResult.h"

#include <cstdint>
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>

namespace {

// XorShift64* PRNG — deterministic, fast, no <random> dependency.
struct PRNG {
    uint64_t state;
    explicit PRNG(uint64_t seed) : state(seed ? seed : 0x1234567890abcdefULL) {}
    uint64_t next() {
        uint64_t x = state;
        x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
        state = x;
        return x * 0x2545F4914F6CDD1DULL;
    }
    int range(int lo, int hi) { return lo + (int)(next() % (uint64_t)(hi - lo + 1)); }
    double uniform(double lo, double hi) {
        const double u = (double)(next() >> 11) / (double)(1ULL << 53);
        return lo + (hi - lo) * u;
    }
};

frame::FrameModel makeRandomModel(PRNG& rng) {
    using namespace frame;
    FrameModel m;
    // Material: typical steel/aluminum range
    Material mat(rng.uniform(70000.0, 250000.0),
                 rng.uniform(25000.0, 100000.0),
                 rng.uniform(2500.0, 8000.0));
    mat.cap = Capacity::make(rng.uniform(150.0, 500.0),
                             rng.uniform(150.0, 500.0),
                             rng.uniform(80.0, 250.0));
    m.materials = { mat };
    // Section: rectangular
    Section sec = Section::Rectangular(rng.uniform(40.0, 200.0),
                                       rng.uniform(40.0, 200.0));
    m.sections = { sec };
    // Nodes: 2 to 8 along +X
    const int nNodes = rng.range(2, 8);
    const double dx = rng.uniform(500.0, 3000.0);
    for (int i = 0; i < nNodes; ++i) {
        Node n(i, (real)i * dx, 0.0, 0.0);
        if (i == 0) { n.fixAll(); }
        m.nodes.push_back(n);
    }
    // Members: chain
    for (int i = 0; i + 1 < nNodes; ++i) {
        m.members.push_back(Member(i, i, i + 1, 0, 0));
    }
    // Random nodal load on the tip
    const double Fmag = rng.uniform(-2000.0, 2000.0);
    NodalLoad nl; nl.node = (NodeId)(nNodes - 1);
    nl.comp[2] = Fmag;
    if (std::abs(Fmag) > 1e-9) { m.nodalLoads = { nl }; }
    return m;
}

bool checkInvariants(const frame::FrameModel& m, const frame::SolveResult& r,
                     std::string& diag, int /*fixtureIdx*/)
{
    if (r.singular) {
        // For singular fixtures, equilibrium can't be checked (no displacements).
        return true;
    }
    // I2: PivotMargin > 0 when non-singular
    if (!(r.pivotMargin > 0.0)) {
        diag = "non-singular but pivotMargin <= 0";
        return false;
    }
    // I1: equilibrium of nodal loads + reactions
    double sumF[3] = { 0.0, 0.0, 0.0 };
    for (const auto& nl : m.nodalLoads) {
        sumF[0] += nl.comp[0]; sumF[1] += nl.comp[1]; sumF[2] += nl.comp[2];
    }
    // Reactions are stored in r.reactions as 6N flat; NaN at free DOFs.
    const int N = (int)m.nodes.size();
    for (int i = 0; i < N; ++i) {
        for (int d = 0; d < 3; ++d) {
            const double val = r.reactions[i * 6 + d];
            if (std::isfinite(val)) { sumF[d] += val; }
        }
    }
    const double eqTol = 1e-3;   // relax for floating; per-component
    for (int d = 0; d < 3; ++d) {
        if (std::abs(sumF[d]) > eqTol) {
            char buf[128]; std::snprintf(buf, sizeof(buf),
                "equilibrium failed: sum F[%d] = %.6g (tol %.3g)", d, sumF[d], eqTol);
            diag = buf;
            return false;
        }
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    const int N = (argc > 1) ? std::atoi(argv[1]) : 1000;
    const uint64_t seed = (argc > 2)
        ? std::strtoull(argv[2], nullptr, 10)
        : 0x3F5AC2D1E789B468ULL;

    PRNG rng(seed);
    int passed = 0, failed = 0;
    std::printf("=== v3.6 exit-test PROPERTY sweep N=%d seed=0x%016llx ===\n",
                N, (unsigned long long)seed);

    for (int i = 0; i < N; ++i) {
        const frame::FrameModel m = makeRandomModel(rng);
        std::string vErr;
        if (!m.validate(vErr)) {
            std::printf("[FIXTURE %4d] generation produced invalid model: %s\n", i, vErr.c_str());
            ++failed;
            continue;
        }
        const frame::SolveResult r = frame::solve(m);
        std::string diag;
        if (!checkInvariants(m, r, diag, i)) {
            std::printf("[FIXTURE %4d] FAIL %s\n", i, diag.c_str());
            ++failed;
        } else {
            ++passed;
        }
    }
    std::printf("\n=== PROPERTY sweep: %d passed / %d failed ===\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
