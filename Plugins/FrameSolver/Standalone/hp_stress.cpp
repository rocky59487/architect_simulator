// hp_stress.cpp -- HP-FEM (solveLoadHP) vs LDLT (solveLoad) stress / boundary probe.
//
// Diagnostic standalone, NOT part of the five-leg gate. It drives the PRODUCTION public
// API across a wide case matrix to map, with data, where the opt-in HP lane (A1: serial
// matrix-free PCG + Jacobi preconditioner) is correct, where it must defer to the direct
// LDLT, and how its iteration count / wall cost scale vs the direct solve. This is the
// evidence for the "what can unify vs what must stay LDLT" decision of the unification work.
//
// Pass/fail contract (correctness only): a case PASSES if solveLoadHP either matches
// solveLoad to <= 1e-8 (u / reactions / member forces) or agrees on singularity. It is
// flagged SLOW (not a failure) when HP wall time exceeds 2x LDLT -- that is the expected
// A1 limitation the perf milestone (A2) addresses, surfaced as data, not hidden.
#include "FrameCore/FrameSolver.h"
#include "FrameCore/HpSolver.h"
#include "FrameCore/SelfWeight.h"
#include "FrameCore/Grillage.h"
#include "FrameTestFixtures.h"

#include <chrono>
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>

using namespace frame;

static int g_pass = 0, g_slow = 0, g_fail = 0;

static double relMax(const std::vector<real>& a, const std::vector<real>& b) {
    double n = 1e-30, d = 0;
    for (real v : b) n = std::max(n, std::fabs((double)v));
    const size_t m = std::min(a.size(), b.size());
    for (size_t i = 0; i < m; ++i) d = std::max(d, std::fabs((double)a[i] - (double)b[i]));
    return d / n;
}
static double mfRel(const SolveResult& a, const SolveResult& b) {
    double n = 1e-30, d = 0;
    const size_t m = std::min(a.memberForces.size(), b.memberForces.size());
    for (size_t e = 0; e < m; ++e) {
        const MemberEndForces ai[2] = { a.memberForces[e].endI, a.memberForces[e].endJ };
        const MemberEndForces bi[2] = { b.memberForces[e].endI, b.memberForces[e].endJ };
        for (int s = 0; s < 2; ++s) {
            const real av[6] = { ai[s].N, ai[s].Vy, ai[s].Vz, ai[s].T, ai[s].My, ai[s].Mz };
            const real bv[6] = { bi[s].N, bi[s].Vy, bi[s].Vz, bi[s].T, bi[s].My, bi[s].Mz };
            for (int k = 0; k < 6; ++k) { n = std::max(n, std::fabs((double)bv[k]));
                                          d = std::max(d, std::fabs((double)av[k] - (double)bv[k])); }
        }
    }
    return d / n;
}

// One case: assemble once, compare solveLoadHP vs solveLoad, time both (repeated to beat
// noise), classify. Prints one machine-greppable line.
static void runCase(const char* cat, const std::string& name, const FrameModel& m,
                    const SolveOptions& so = {}) {
    HpSolveOptions hp; hp.enabled = true;
    PreparedSystem ps = assembleAndFactor(m, so);
    const SolveResult ref = solveLoad(ps, m);
    const SolveResult got = solveLoadHP(ps, m, hp);

    const int ndof = (int)ref.u.size();
    const bool fellBack = got.diagnostic.find("LDLT (") != std::string::npos;
    double v = 0.0;
    std::string status;
    if (ref.singular || got.singular) {
        v = 0.0;
        if (ref.singular == got.singular) { status = "PASS-SING"; ++g_pass; }
        else { status = "FAIL-SING"; ++g_fail; }
    } else {
        v = std::max({ relMax(got.u, ref.u), relMax(got.reactions, ref.reactions), mfRel(got, ref) });
        if (v > 1e-8) { status = "FAIL"; ++g_fail; }
        else { status = "PASS"; ++g_pass; }
    }

    double ldltMs = 0, hpMs = 0;
    if (!ref.singular && !got.singular) {
        const int K = ndof > 4000 ? 20 : 200;
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < K; ++i) { const SolveResult r = solveLoad(ps, m); (void)r.u[0]; }
        auto t1 = std::chrono::steady_clock::now();
        for (int i = 0; i < K; ++i) { const SolveResult r = solveLoadHP(ps, m, hp); (void)r.u[0]; }
        auto t2 = std::chrono::steady_clock::now();
        ldltMs = std::chrono::duration<double, std::milli>(t1 - t0).count() / K;
        hpMs   = std::chrono::duration<double, std::milli>(t2 - t1).count() / K;
        if (status == "PASS" && hpMs > 2.0 * ldltMs) { status = "SLOW"; --g_pass; ++g_slow; }
    }

    std::printf("[hp_stress] cat=%-9s case=%-26s ndof=%-6d sing=%d fellBack=%d vsLdlt=%.2e "
                "ldltMs=%.4f hpMs=%.4f ratio=%6.2f %-9s diag=\"%s\"\n",
                cat, name.c_str(), ndof, ref.singular ? 1 : 0, fellBack ? 1 : 0, v,
                ldltMs, hpMs, ldltMs > 1e-12 ? hpMs / ldltMs : 0.0, status.c_str(),
                got.diagnostic.c_str());
}

static void addTipUz(FrameModel& m, int node, real P) {
    NodalLoad nl; nl.node = node; nl.comp[Uz] = -P; m.nodalLoads.push_back(nl);
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("# HP-FEM stress/boundary probe (solveLoadHP vs LDLT) | %s %s\n", __DATE__, __TIME__);
    const real E = 210000.0, G = 80769.0;
    Section sec = Section::Rectangular(100.0, 100.0);
    Material mat(E, G, 7850.0);
    mat.cap = Capacity::make(300.0, 300.0, 180.0);

    // ---- A. correctness on the standard fixtures ----
    { FrameModel m; fixtures::cantileverTipLoad(m, 1000.0, 2000.0, mat, sec); runCase("correct", "cantilever_tip", m); }
    { FrameModel m; fixtures::simplySupportedUDL(m, 5.0, 3000.0, mat, sec);  runCase("correct", "ss_udl", m); }
    { FrameModel m; fixtures::axialColumn(m, 50000.0, 3000.0, mat, sec);     runCase("correct", "axial_column", m); }
    { FrameModel m; fixtures::clampedSettlement(m, 3000.0, 5.0, mat, sec);   runCase("correct", "settlement_prescribed", m); }
    { FrameModel m; fixtures::proppedCantileverRelease(m, 5.0, 3000.0, mat, sec);
      SolveOptions so; so.enableReleases = true;                            runCase("correct", "release_enabled", m, so); }
    { FrameModel m; fixtures::twoSpanContinuous(m, 3000.0, mat, sec);
      NodalLoad nl; nl.node = 1; nl.comp[Uz] = -8000.0; m.nodalLoads.push_back(nl); runCase("correct", "two_span", m); }
    { FrameModel m; fixtures::circularArchCantilever(m, 1000.0, 16, 3000.0, mat, sec); runCase("correct", "arch_curved16", m); }

    // ---- B. boundary / singular: HP must defer to the LDLT pivot guard or fall back ----
    { FrameModel m; fixtures::mechanism(m, mat, sec);                        runCase("boundary", "mechanism_singular", m); }
    { FrameModel m; fixtures::torsionReleaseMechanism(m, mat, sec);
      SolveOptions so; so.enableReleases = true;                            runCase("boundary", "torsion_release_mech", m, so); }
    { FrameModel m; fixtures::clampedPlateShell(m, 2000.0, 20.0, 4, 0.01, mat); runCase("boundary", "shell_only_fallback", m); }
    { FrameModel m;  // fully constrained: nf=0 -> singular ("fully constrained")
      m.materials = { mat }; m.sections = { sec };
      Node a(0,0,0,0); a.fixAll(); Node b(1,1000,0,0); b.fixAll();
      m.nodes = { a, b }; m.members = { Member(0,0,1,0,0) };                runCase("boundary", "fully_constrained", m); }
    { FrameModel m;  // single element, tip load
      m.materials = { mat }; m.sections = { sec };
      Node a(0,0,0,0); a.fixAll(); m.nodes = { a, Node(1,2000,0,0) };
      m.members = { Member(0,0,1,0,0) }; addTipUz(m, 1, 1000.0);            runCase("boundary", "single_element", m); }

    // ---- B2. coverage gaps surfaced by the adversarial review ----
    // prescribed displacement + external load together (mixed RHS direction)
    { FrameModel m; fixtures::clampedSettlement(m, 3000.0, 5.0, mat, sec);
      NodalLoad nl; nl.node = 1; nl.comp[Uz] = -6000.0; m.nodalLoads.push_back(nl);
      runCase("coverage", "settlement_plus_load", m); }
    // multi-material (steel + aluminium spans -> per-element diagonal stiffness ratio)
    { FrameModel m; m.materials = { mat, Material(70000.0, 26000.0, 2700.0) }; m.sections = { sec };
      Node a(0, 0, 0, 0); a.fixAll();
      m.nodes = { a, Node(1, 2000, 0, 0), Node(2, 4000, 0, 0) };
      m.members = { Member(0, 0, 1, 0, 0), Member(1, 1, 2, 1, 0) };   // member 1 uses matIdx=1
      addTipUz(m, 2, 1000.0); runCase("coverage", "multi_material", m); }
    // inactive (element-removed) member: two parallel members, one deactivated
    { FrameModel m; m.materials = { mat }; m.sections = { sec };
      Node a(0, 0, 0, 0); a.fixAll(); m.nodes = { a, Node(1, 2000, 0, 0) };
      Member m0(0, 0, 1, 0, 0); Member m1(1, 0, 1, 0, 0); m1.active = false;
      m.members = { m0, m1 }; addTipUz(m, 1, 1000.0);
      runCase("coverage", "inactive_member", m); }
    // self-weight equivalent nodal loads (addEquivalentNodalLoads path)
    { FrameModel m; fixtures::cantileverBeamN(m, 8, 4000.0, mat, sec); addSelfWeight(m);
      runCase("coverage", "self_weight", m); }
    // grillage (woven beam grid: ill-conditioned mixed bending/torsion stiffness)
    { FrameModel m; grillage::PlateSpec spec; spec.nx = 4; spec.ny = 4; std::string why;
      if (grillage::buildGrillage(m, spec, mat, why)) runCase("coverage", "grillage_4x4", m);
      else std::printf("[hp_stress] grillage build failed: %s\n", why.c_str()); }
    // truss-pin: all-rotation release -> 2-force members (axial chain, transverse restrained)
    { FrameModel m; m.materials = { mat }; m.sections = { sec };
      Node a(0, 0, 0, 0); a.fixAll();
      Node b(1, 2000, 0, 0); b.fixed[Uy] = b.fixed[Uz] = b.fixed[Rx] = b.fixed[Ry] = b.fixed[Rz] = true;
      Node c(2, 4000, 0, 0); c.fixed[Uy] = c.fixed[Uz] = c.fixed[Rx] = c.fixed[Ry] = c.fixed[Rz] = true;
      m.nodes = { a, b, c };
      Member tm0(0, 0, 1, 0, 0); tm0.release = makeRelease(ReleasePreset::TrussPin);
      Member tm1(1, 1, 2, 0, 0); tm1.release = makeRelease(ReleasePreset::TrussPin);
      m.members = { tm0, tm1 };
      NodalLoad nl; nl.node = 2; nl.comp[Ux] = 5000.0; m.nodalLoads.push_back(nl);
      runCase("coverage", "truss_pin_axial", m); }

    // ---- C. mesh-size scaling: how A1's Jacobi PCG iteration count / cost grows with nf ----
    for (int n : {4, 8, 16, 32, 64, 128, 256}) {
        FrameModel m; fixtures::cantileverBeamN(m, n, 4000.0, mat, sec); addTipUz(m, n, 1000.0);
        runCase("meshscale", "cantileverN_" + std::to_string(n), m);
    }

    // ---- D. conditioning: slenderness (vary section side at fixed length) ----
    for (real side : {400.0, 100.0, 25.0, 6.0}) {
        Section s2 = Section::Rectangular(side, side);
        FrameModel m; fixtures::cantileverBeamN(m, 16, 4000.0, mat, s2); addTipUz(m, 16, 1000.0);
        runCase("slender", "side_" + std::to_string((int)side), m);
    }

    // ---- E. unit scale (geometry x1000) and Young's modulus range ----
    for (real L : {20.0, 2000.0, 200000.0}) {
        FrameModel m; fixtures::cantileverBeamN(m, 16, L, mat, sec); addTipUz(m, 16, 1000.0);
        runCase("unitscale", "L_" + std::to_string((long)L), m);
    }
    for (real Eval : {100.0, 1.0e5, 1.0e8}) {
        Material m2(Eval, Eval * 0.385, 7850.0);
        FrameModel m; fixtures::cantileverBeamN(m, 16, 4000.0, m2, sec); addTipUz(m, 16, 1000.0);
        runCase("emod", "E_" + std::to_string((long)Eval), m);
    }

    // ---- F. repeated low-dim loads (game frame loop): A1 has no seeded recycling, so each
    //         frame re-solves from scratch -- correct but NOT faster than reused LDLT. This is
    //         exactly the regime the seeded HP win (A2) targets; A1 surfaces the gap as data. ----
    {
        FrameModel m; fixtures::cantileverBeamN(m, 16, 4000.0, mat, sec); addTipUz(m, 16, 1000.0);
        HpSolveOptions hp; hp.enabled = true;
        PreparedSystem ps = assembleAndFactor(m);
        const int frames = 200;
        double maxV = 0.0, ldltMs = 0, hpMs = 0;
        auto t0 = std::chrono::steady_clock::now();
        for (int f = 0; f < frames; ++f) {
            FrameModel mf = m; mf.nodalLoads.clear();
            addTipUz(mf, 16, 1000.0 * (1.0 + 0.3 * std::sin(0.1 * f)));   // low-dim varying load
            const SolveResult r = solveLoad(ps, mf); (void)r.u[0];
        }
        auto t1 = std::chrono::steady_clock::now();
        for (int f = 0; f < frames; ++f) {
            FrameModel mf = m; mf.nodalLoads.clear();
            addTipUz(mf, 16, 1000.0 * (1.0 + 0.3 * std::sin(0.1 * f)));
            const SolveResult rh = solveLoadHP(ps, mf, hp);
            FrameModel mr = m; mr.nodalLoads.clear();
            addTipUz(mr, 16, 1000.0 * (1.0 + 0.3 * std::sin(0.1 * f)));
            const SolveResult rl = solveLoad(ps, mr);
            maxV = std::max(maxV, relMax(rh.u, rl.u));
        }
        auto t2 = std::chrono::steady_clock::now();
        ldltMs = std::chrono::duration<double, std::milli>(t1 - t0).count() / frames;
        hpMs   = std::chrono::duration<double, std::milli>(t2 - t1).count() / (frames * 2);
        const char* st = maxV <= 1e-8 ? "PASS" : "FAIL";
        if (maxV <= 1e-8) ++g_pass; else ++g_fail;
        std::printf("[hp_stress] cat=repeated  case=%-26s frames=%d maxVsLdlt=%.2e ldltMs=%.4f hpMs=%.4f %s\n",
                    "cantileverN16_200frames", frames, maxV, ldltMs, hpMs, st);
    }

    std::printf("\n[hp_stress_summary] pass=%d slow=%d fail=%d\n", g_pass, g_slow, g_fail);
    return g_fail == 0 ? 0 : 1;
}
