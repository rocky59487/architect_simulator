// Standalone gate for FrameCore — builds fixtures, solves, compares to closed-form
// analytic solutions, prints PASS/FAIL, exits 0 iff all pass.
#include "FrameCore/FrameSolver.h"
#include "FrameCore/ElasticAllowable.h"
#include "FrameCore/FiberSection.h"
#include "FrameCore/Pushover.h"
#include "FrameCore/EquivalentModeling.h"
#include "FrameCore/Connectivity.h"
#include "FrameCore/StaticCondensation.h"
#include "FrameCore/Grillage.h"
#include "FrameCore/DamageField.h"
#include "FrameCore/Construction.h"
#include "FrameTestFixtures.h"

#include <vector>
#include <utility>

#include <cstdio>
#include <cmath>
#include <string>
#include <algorithm>

using namespace frame;

static int g_fail = 0;

static double relErr(double got, double expect) {
    return std::fabs(got - expect) / std::max(std::fabs(expect), 1e-30);
}
static void checkClose(const char* tag, double got, double expect, double tol) {
    const double e = relErr(got, expect);
    const bool ok = std::isfinite(got) && e <= tol;
    if (!ok) ++g_fail;
    std::printf("  %s %-32s got=%-12.6g exp=%-12.6g rel=%.2e (tol=%.0e)\n",
                ok ? "[PASS]" : "[FAIL]", tag, got, expect, e, tol);
}
static void checkTrue(const char* tag, bool cond, const std::string& detail = "") {
    if (!cond) ++g_fail;
    std::printf("  %s %-32s %s\n", cond ? "[PASS]" : "[FAIL]", tag, detail.c_str());
}

// Tiny dense linear solve (Gaussian elimination with partial pivoting). Deliberately
// independent of Eigen so the Schur-condensation check (F16) cross-validates the
// engine's own solver rather than re-using it. A (row-major n*n), b (n) -> x (n).
static std::vector<double> denseSolve(std::vector<double> A, std::vector<double> b, int n) {
    for (int col = 0; col < n; ++col) {
        int piv = col; double best = std::fabs(A[(size_t)col * n + col]);
        for (int r = col + 1; r < n; ++r) {
            const double v = std::fabs(A[(size_t)r * n + col]);
            if (v > best) { best = v; piv = r; }
        }
        if (piv != col) {
            for (int k = 0; k < n; ++k) std::swap(A[(size_t)col * n + k], A[(size_t)piv * n + k]);
            std::swap(b[(size_t)col], b[(size_t)piv]);
        }
        const double d = A[(size_t)col * n + col];
        for (int r = col + 1; r < n; ++r) {
            const double f = A[(size_t)r * n + col] / d;
            for (int k = col; k < n; ++k) A[(size_t)r * n + k] -= f * A[(size_t)col * n + k];
            b[(size_t)r] -= f * b[(size_t)col];
        }
    }
    std::vector<double> x((size_t)n, 0.0);
    for (int r = n - 1; r >= 0; --r) {
        double s = b[(size_t)r];
        for (int k = r + 1; k < n; ++k) s -= A[(size_t)r * n + k] * x[(size_t)k];
        x[(size_t)r] = s / A[(size_t)r * n + r];
    }
    return x;
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);  // unbuffered: output survives a crash
    const real E = 210000.0, G = 80769.0;     // steel, MPa
    const real side = 100.0;                   // mm, square section
    Section  sec = Section::Rectangular(side, side);
    Material mat(E, G, 7850.0);
    mat.cap = Capacity::make(300.0, 300.0, 180.0);

    std::printf("FrameCore standalone fixtures  (units: N, mm, MPa)\n");
    std::printf("  E=%.0f  G=%.0f  square=%.0f  A=%.0f  Iz=%.6g  Wz=%.6g\n\n",
                E, G, side, sec.A, sec.Iz, sec.Wz());

    // ---------- F1: cantilever, tip point load ----------
    {
        const real P = 1000.0, L = 2000.0;
        FrameModel m; fixtures::cantileverTipLoad(m, P, L, mat, sec);
        SolveResult r = solve(m);
        std::printf("[F1] cantilever tip load  P=%.0f L=%.0f\n", P, L);
        checkTrue("not singular", !r.singular, r.diagnostic);
        const real dExp = P * L * L * L / (3.0 * E * sec.Iz);
        checkClose("tip deflection |u_z(j)|", std::fabs(r.disp(1, Uz)), dExp, 1e-6);
        const MemberForcePair& mf = r.memberForces[0];
        const real Mroot = std::sqrt(mf.endI.My * mf.endI.My + mf.endI.Mz * mf.endI.Mz);
        checkClose("root moment |M_i|", Mroot, P * L, 1e-6);
        checkClose("reaction R_z(node0)", std::fabs(r.reaction(0, Uz)), P, 1e-9);
        ElasticAllowable strength;
        const DemandResult d = strength.checkSection(mf.endI, sec, mat.cap);
        checkTrue("checkSection mode != None", d.mode != FailMode::None);
        checkClose("checkSection risk", d.risk, (Mroot / sec.Wz()) / mat.cap.bend, 1e-6);
    }

    // ---------- F2: simply supported, UDL ----------
    {
        const real w = 5.0, L = 3000.0;
        FrameModel m; fixtures::simplySupportedUDL(m, w, L, mat, sec);
        SolveResult r = solve(m);
        std::printf("[F2] simply-supported UDL  w=%.1f L=%.0f\n", w, L);
        checkTrue("not singular", !r.singular, r.diagnostic);
        const real dExp = 5.0 * w * L * L * L * L / (384.0 * E * sec.Iz);
        checkClose("midspan deflection |u_z|", std::fabs(r.disp(1, Uz)), dExp, 1e-6);
        const MemberForcePair& m0 = r.memberForces[0];
        checkClose("midspan moment |M|", std::fabs(m0.endJ.Mz), w * L * L / 8.0, 1e-5);
        checkClose("reaction R_z(node0)", std::fabs(r.reaction(0, Uz)), w * L / 2.0, 1e-6);
        checkClose("reaction R_z(node2)", std::fabs(r.reaction(2, Uz)), w * L / 2.0, 1e-6);
    }

    // ---------- F3: mechanism (negative test) ----------
    {
        FrameModel m; fixtures::mechanism(m, mat, sec);
        SolveResult r = solve(m);
        std::printf("[F3] mechanism (under-constrained)\n");
        checkTrue("singular detected", r.singular, r.diagnostic);
        checkTrue("diagnostic non-empty", !r.diagnostic.empty(), r.diagnostic);
    }

    // ---------- F4: vertical column (axial sign + stiffness) ----------
    {
        const real P = 50000.0, h = 3000.0;
        FrameModel m; fixtures::axialColumn(m, P, h, mat, sec);
        SolveResult r = solve(m);
        std::printf("[F4] vertical column  P=%.0f h=%.0f\n", P, h);
        checkTrue("not singular", !r.singular, r.diagnostic);
        checkClose("axial shortening |u_z|", std::fabs(r.disp(1, Uz)), P * h / (E * sec.A), 1e-6);
        const real N = r.memberForces[0].endI.N;
        checkTrue("axial N > 0 (compression)", N > 0.0, "N=" + std::to_string(N));
    }

    // ---------- F5: checkSection pure torsion (units must be MPa) ----------
    {
        MemberEndForces f;            // pure torsion
        f.T = 1.0e6;                  // N*mm
        ElasticAllowable strength;
        const DemandResult d = strength.checkSection(f, sec, mat.cap);
        const real cTor    = std::hypot(sec.cy, sec.cz);
        const real sTorExp = 1.0e6 * cTor / sec.J;   // ~5.021 MPa, NOT |T|/J=0.071
        std::printf("[F5] checkSection pure torsion  T=1e6\n");
        checkClose("torsion stress sTor (MPa)", d.sTor, sTorExp, 1e-6);
        checkTrue("mode == Torsion", d.mode == FailMode::Torsion);
        checkClose("torsion risk", d.risk, sTorExp / mat.cap.tors, 1e-6);
    }

    // ---- F6: propped cantilever via member release (Qf condensation + analytic) ----
    {
        const real w = 5.0, L = 4000.0;
        FrameModel m; fixtures::proppedCantileverRelease(m, w, L, mat, sec);
        SolveOptions opt; opt.enableReleases = true;
        SolveResult r = solve(m, opt);
        std::printf("[F6] propped cantilever via release  w=%.1f L=%.0f\n", w, L);
        checkTrue("not singular", !r.singular, r.diagnostic);
        const real Mj = std::sqrt(r.memberForces[1].endJ.My * r.memberForces[1].endJ.My
                                + r.memberForces[1].endJ.Mz * r.memberForces[1].endJ.Mz);
        const real Mscale = w * L * L / 8.0;
        checkTrue("released-end moment ~ 0", Mj < 1e-6 * Mscale, "Mj=" + std::to_string(Mj));
        checkClose("fixed-end moment |M_i| = wL^2/8", std::fabs(r.memberForces[0].endI.Mz), Mscale, 1e-6);
        checkClose("reaction R_z(node0) = 5wL/8", std::fabs(r.reaction(0, Uz)), 5.0 * w * L / 8.0, 1e-6);
        checkClose("reaction R_z(node2) = 3wL/8", std::fabs(r.reaction(2, Uz)), 3.0 * w * L / 8.0, 1e-6);
    }

    // ---- F7: ill-posed release (both torsional ends) -> singular guard ----
    {
        FrameModel m; fixtures::torsionReleaseMechanism(m, mat, sec);
        SolveOptions opt; opt.enableReleases = true;
        SolveResult r = solve(m, opt);
        std::printf("[F7] torsional double-release mechanism\n");
        checkTrue("singular detected", r.singular, r.diagnostic);
        checkTrue("diagnostic mentions release",
                  r.diagnostic.find("release") != std::string::npos, r.diagnostic);
    }

    // ---- F8: FiberSection RC P-M capacity vs Whitney closed form ----
    {
        RCSectionParams rc; rc.fc = 28.0; rc.fy = 420.0; rc.Es = 200000.0; rc.cover = 60.0;
        rc.AsTop = 0.0; rc.AsBot = 1000.0;                   // singly reinforced
        Section rcs = Section::Rectangular(300.0, 500.0);    // b=300, d=500
        FiberSection fib(rc);
        const real dEff = 500.0 - 60.0;
        const real aW   = rc.AsBot * rc.fy / (0.85 * rc.fc * 300.0);
        const real MnW  = rc.AsBot * rc.fy * (dEff - aW / 2.0);   // Whitney Mn
        const Capacity dummy = Capacity::make(1, 1, 1);
        std::printf("[F8] FiberSection RC  MnW=%.5g  a=%.3g\n", MnW, aW);

        MemberEndForces fm; fm.Mz = MnW;
        const DemandResult d1 = fib.checkSection(fm, rcs, dummy);
        checkClose("D/C at M = MnW ~ 1.0", d1.risk, 1.0, 5e-3);
        checkTrue("mode == Bending", d1.mode == FailMode::Bending);

        MemberEndForces fh; fh.Mz = 0.5 * MnW;
        checkClose("D/C at M = MnW/2 ~ 0.5", fib.checkSection(fh, rcs, dummy).risk, 0.5, 5e-3);

        const real Ast = rc.AsTop + rc.AsBot;
        const real P0  = 0.85 * rc.fc * (300.0 * 500.0 - Ast) + rc.fy * Ast;
        MemberEndForces fa; fa.N = P0;
        const DemandResult d3 = fib.checkSection(fa, rcs, dummy);
        checkClose("D/C at N = P0 ~ 1.0", d3.risk, 1.0, 5e-3);
        checkTrue("mode == Crush", d3.mode == FailMode::Crush);
    }

    // ---- F9: plastic-hinge pushover collapse of a cantilever (tip load) ----
    {
        const real P = 1000.0, L = 2000.0;
        FrameModel mm; fixtures::cantileverTipLoad(mm, P, L, mat, sec);
        SolveOptions opt;
        const PushoverResult pr = pushover(mm, opt, 20);
        const real Mp = sec.Wz() * mat.cap.bend;
        const real expLam = Mp / (P * L);
        std::printf("[F9] pushover cantilever collapse  Mp=%.4g\n", Mp);
        checkTrue("pushover ok", pr.ok, pr.diagnostic);
        checkClose("collapse lambda = Mp/(P*L)", pr.collapseLambda, expLam, 1e-6);
        checkTrue("exactly 1 hinge -> mechanism", pr.steps.size() == 1,
                  "nsteps=" + std::to_string(pr.steps.size()));
    }

    // ---- F10: Timoshenko shear correction on a cantilever (deep vs slender) ----
    {
        const real P = 1000.0, L = 2000.0;
        FrameModel m; fixtures::cantileverTipLoad(m, P, L, mat, sec);
        SolveOptions opt; opt.useTimoshenko = true;
        SolveResult r = solve(m, opt);
        const real dEB    = P * L * L * L / (3.0 * E * sec.Iz);
        const real dShear = P * L / (G * sec.Asy);
        std::printf("[F10] Timoshenko cantilever  L=%.0f Asy=%.5g\n", L, sec.Asy);
        checkTrue("not singular", !r.singular, r.diagnostic);
        checkClose("tip deflection = PL^3/3EI + PL/GAs", std::fabs(r.disp(1, Uz)), dEB + dShear, 1e-6);

        // L/d sweep: slender -> Euler-Bernoulli; deep -> shear contribution matters.
        auto timoRatio = [&](real Ls) -> real {
            FrameModel ms; fixtures::cantileverTipLoad(ms, P, Ls, mat, sec);
            SolveOptions o; o.useTimoshenko = true;
            const real dt = std::fabs(solve(ms, o).disp(1, Uz));
            const real de = P * Ls * Ls * Ls / (3.0 * E * sec.Iz);
            return dt / de;
        };
        const real rDeep = timoRatio(300.0);     // L/d = 3
        const real rSlen = timoRatio(6000.0);    // L/d = 60
        std::printf("   ratio Timo/EB: deep(L/d=3)=%.5f  slender(L/d=60)=%.5f\n", rDeep, rSlen);
        checkTrue("deep-beam shear significant (>5%)", rDeep > 1.05, "rDeep=" + std::to_string(rDeep));
        checkTrue("slender converges to EB (<0.1%)", std::fabs(rSlen - 1.0) < 1e-3, "rSlen=" + std::to_string(rSlen));
        checkTrue("shear effect shrinks with slenderness", rSlen < rDeep);
    }

    // ---- F11: circular section properties + resultant biaxial bending ----
    {
        const real rad = 50.0;
        Section cir = Section::Circular(rad);
        const real PI = 3.14159265358979323846;
        const real Iexp = PI * rad * rad * rad * rad / 4.0;
        std::printf("[F11] circular section  r=%.0f  I=%.6g\n", rad, cir.Iz);
        checkClose("I = pi r^4 / 4", cir.Iz, Iexp, 1e-12);
        checkClose("Iy == Iz", cir.Iy, cir.Iz, 1e-12);
        checkClose("J = 2I (polar)", cir.J, 2.0 * Iexp, 1e-12);

        const real P = 1000.0, L = 2000.0;
        FrameModel m; fixtures::cantileverTipLoad(m, P, L, mat, cir);
        SolveResult r = solve(m);
        checkClose("circular cantilever tip = PL^3/3EI", std::fabs(r.disp(1, Uz)),
                   P * L * L * L / (3.0 * E * Iexp), 1e-6);

        // biaxial: round section uses the resultant sqrt(My^2+Mz^2)/W, which is LESS
        // than the rectangular conservative corner sum |My|/W + |Mz|/W.
        ElasticAllowable strength;
        const Capacity cap = Capacity::make(250.0, 250.0, 150.0);
        MemberEndForces f; f.My = 1.0e6; f.Mz = 1.0e6;
        const DemandResult dC = strength.checkSection(f, cir, cap);
        const real W = cir.Wz();
        checkClose("circular biaxial sComp = sqrt2*M/W", dC.sComp, std::sqrt(2.0) * 1.0e6 / W, 1e-6);
        checkTrue("round resultant < rectangular corner sum", dC.sComp < 2.0e6 / W - 1e-9,
                  "sComp=" + std::to_string(dC.sComp));
    }

    // ---- F12: piecewise-linear curved member (quarter-circle arch), convergence ----
    {
        const real R = 2000.0, P = 1000.0;
        const real PI = 3.14159265358979323846;
        const real dExp = (PI / 4.0) * P * R * R * R / (E * sec.Iz);
        std::printf("[F12] quarter-circle arch cantilever  R=%.0f  dExp=%.6g\n", R, dExp);
        const int Ns[4] = { 8, 16, 32, 64 };
        real prevErr = 1e30, lastVal = 0; bool monotone = true, nonsing = true;
        for (int i = 0; i < 4; ++i) {
            FrameModel m; fixtures::circularArchCantilever(m, R, Ns[i], P, mat, sec);
            SolveResult r = solve(m);
            if (r.singular) nonsing = false;
            const real d   = std::fabs(r.disp(Ns[i], Uy));
            const real err = std::fabs(d - dExp) / dExp;
            std::printf("   N=%2d  d=%.6g  err=%.3e\n", Ns[i], d, err);
            if (err > prevErr) monotone = false;
            prevErr = err; lastVal = d;
        }
        checkTrue("arch solves (non-singular)", nonsing);
        checkTrue("arch error shrinks as N grows", monotone);
        checkClose("arch tip (N=64) -> (pi/4)PR^3/EI", lastVal, dExp, 1.5e-2);
    }

    // ---- F13: equivalent modeling (ACI flange + composite T-section + brace + tributary) ----
    {
        using namespace frame::equiv;
        const real be = effectiveFlangeWidthACI(300.0, 100.0, 2000.0, 6000.0);  // 300 + 2*min(800,1000,750)
        std::printf("[F13] equivalent modeling  be=%.6g\n", be);
        checkClose("ACI effective flange be = 1800", be, 1800.0, 1e-12);

        const Section tee = compositeTeeSection(300.0, 600.0, be, 100.0);  // bw=300,h=600,hf=100
        checkClose("T-section area", tee.A, 330000.0, 1e-9);
        checkClose("T-section extreme fibre cz", tee.cz, 413.6364, 1e-4);
        checkClose("T-section Iz (composite)", tee.Iz, 1.0638636e10, 1e-4);
        checkTrue("composite I > bare web I", tee.Iz > Section::Rectangular(300.0, 600.0).Iz);

        const real Eb = 210000.0, Lb = 4000.0, Hb = 3000.0, Ktar = 5000.0;
        const real Abr = equivalentBraceArea(Ktar, Eb, Lb, Hb);
        const real Ld = std::sqrt(Lb * Lb + Hb * Hb);
        checkClose("equivalent brace K round-trip", Eb * Abr * Lb * Lb / (Ld * Ld * Ld), Ktar, 1e-9);

        const real q = 0.01, w1 = 2000.0, w2 = 4000.0;
        checkClose("tributary load conservation",
                   tributaryLineLoad(q, w1) + tributaryLineLoad(q, w2), q * (w1 + w2), 1e-12);
    }

    // ---- F14: collapse / connectivity (grounded mask + reversible journal) ----
    {
        using namespace frame::conn;
        const std::vector<int> ground = { 0 };
        const std::vector<std::pair<int,int>> full = { {0,1},{1,2},{2,3},{3,4} };
        const std::vector<bool> g0 = groundedMask(5, full, ground);
        std::printf("[F14] connectivity / reversible journal\n");
        checkTrue("full chain all grounded", g0[0] && g0[1] && g0[2] && g0[3] && g0[4]);

        const std::vector<std::pair<int,int>> cut = { {0,1},{1,2},{3,4} };   // (2,3) removed
        const std::vector<bool> g1 = groundedMask(5, cut, ground);
        checkTrue("cut: 0,1,2 grounded", g1[0] && g1[1] && g1[2]);
        checkTrue("cut: 3,4 detached", !g1[3] && !g1[4]);

        RollbackUnionFind uf(5);
        uf.unite(0, 1); uf.unite(1, 2);
        const int mk = uf.marker();
        uf.unite(2, 3); uf.unite(3, 4);
        checkTrue("after unites 0~4 connected", uf.connected(0, 4));
        uf.rollback(mk);
        checkTrue("rollback: 0~2 still connected", uf.connected(0, 2));
        checkTrue("rollback: 0 !~ 3 (undone)", !uf.connected(0, 3));
        checkTrue("rollback: 2 !~ 3 (undone)", !uf.connected(2, 3));
        checkTrue("rollback restores component count = 3", uf.componentCount() == 3);
    }

    // ---- F15: rotation-symmetry invariant (solver equivariance) ----
    {
        auto sq = [](real v) { return v * v; };
        const real P = 1000.0, L = 2000.0;
        FrameModel mb; fixtures::cantileverTipLoad(mb, P, L, mat, sec);
        const SolveResult rb = solve(mb);
        const real u0[3] = { rb.disp(1, Ux), rb.disp(1, Uy), rb.disp(1, Uz) };

        // Rodrigues rotation about a = unit(1,2,3), angle 0.6 rad
        real ax = 1, ay = 2, az = 3; const real an = std::sqrt(ax*ax + ay*ay + az*az);
        ax /= an; ay /= an; az /= an;
        const real ph = 0.6, c = std::cos(ph), s = std::sin(ph), t = 1.0 - c;
        const real R[3][3] = {
            { c + ax*ax*t,    ax*ay*t - az*s, ax*az*t + ay*s },
            { ay*ax*t + az*s, c + ay*ay*t,    ay*az*t - ax*s },
            { az*ax*t - ay*s, az*ay*t + ax*s, c + az*az*t }
        };
        auto rot = [&](real x, real y, real z, int i) { return R[i][0]*x + R[i][1]*y + R[i][2]*z; };

        FrameModel mr;
        mr.materials.reserve(1); mr.sections.reserve(1);
        mr.materials.push_back(mat); mr.sections.push_back(sec);
        const Material* pm = &mr.materials.back(); const Section* ps = &mr.sections.back();
        Node n0(0, 0, 0, 0); n0.fixAll();
        Node n1(1, rot(L,0,0,0), rot(L,0,0,1), rot(L,0,0,2));
        mr.nodes = { n0, n1 };
        Member mm(0, 0, 1, pm, ps);
        mm.refVec = Vec3(rot(0,0,1,0), rot(0,0,1,1), rot(0,0,1,2));   // rotate the local frame too
        mr.members = { mm };
        NodalLoad nl; nl.node = 1;
        nl.comp[Ux] = rot(0,0,P,0); nl.comp[Uy] = rot(0,0,P,1); nl.comp[Uz] = rot(0,0,P,2);
        mr.nodalLoads = { nl };
        const SolveResult rr = solve(mr);

        const real ex = rot(u0[0], u0[1], u0[2], 0), ey = rot(u0[0], u0[1], u0[2], 1), ez = rot(u0[0], u0[1], u0[2], 2);
        const real du  = std::sqrt(sq(rr.disp(1,Ux) - ex) + sq(rr.disp(1,Uy) - ey) + sq(rr.disp(1,Uz) - ez));
        const real nrm = std::sqrt(sq(u0[0]) + sq(u0[1]) + sq(u0[2]));
        std::printf("[F15] rotation invariance  |u0|=%.6g\n", nrm);
        checkTrue("rotated solve non-singular", !rr.singular, rr.diagnostic);
        checkClose("tip |u| preserved under rotation",
                   std::sqrt(sq(rr.disp(1,Ux)) + sq(rr.disp(1,Uy)) + sq(rr.disp(1,Uz))), nrm, 1e-9);
        checkTrue("u_rotated == R u_base (equivariance)", du < 1e-6 * nrm, "du=" + std::to_string(du));
    }

    // ---- F16: Schur static condensation reproduces the full static solution ----
    {
        const real P = 1000.0, L = 1000.0;
        FrameModel m; fixtures::condensationChain(m, P, L, mat, sec);
        const SolveResult full = solve(m);
        std::printf("[F16] Schur static condensation (4-node cantilever, internal load)\n");
        checkTrue("full solve non-singular", !full.singular, full.diagnostic);

        // boundary = node0 (6) + node3 (6); internal = nodes 1,2 folded away.
        std::vector<int> bdofs;
        for (int d = 0; d < 6; ++d) bdofs.push_back(gdof(0, d));
        for (int d = 0; d < 6; ++d) bdofs.push_back(gdof(3, d));
        const CondensationResult cr = condenseStatic(m, bdofs);
        checkTrue("condensation ok", cr.ok, cr.why);
        checkTrue("boundaryCount == 12", cr.boundaryCount == 12,
                  "nb=" + std::to_string(cr.boundaryCount));
        checkTrue("kappa_D finite & < 1e10",
                  std::isfinite(cr.conditionNumber) && cr.conditionNumber < 1e10,
                  "kappaD=" + std::to_string(cr.conditionNumber));

        // node0 is encastre (prescribed 0): the reduced boundary system is node3's 6x6
        // sub-block of S with RHS fEff[node3]. Solve it with an INDEPENDENT dense Gauss
        // routine; the result must match the full solve's node3 displacements.
        const int nb = cr.boundaryCount;                 // 12
        std::vector<double> A(36), bb(6);
        for (int i = 0; i < 6; ++i) {
            bb[(size_t)i] = cr.fEff[(size_t)(6 + i)];
            for (int j = 0; j < 6; ++j) A[(size_t)i * 6 + j] = cr.S[(size_t)(6 + i) * nb + (6 + j)];
        }
        const std::vector<double> x = denseSolve(A, bb, 6);   // u at node3 (Ux..Rz)
        const real uzFull = full.disp(3, Uz);
        real maxDiff = 0;
        for (int d = 0; d < 6; ++d) maxDiff = std::max(maxDiff, std::fabs(x[(size_t)d] - full.disp(3, d)));
        checkClose("condensed u_z(node3) == full", x[(size_t)Uz], uzFull, 1e-9);
        checkTrue("condensed u(node3) == full (all 6 DOF)",
                  maxDiff < 1e-7 * std::max(1.0, std::fabs(uzFull)),
                  "maxDiff=" + std::to_string(maxDiff));

        // guard A: empty boundary -> K_ii is the unconstrained K (6 rigid-body modes) -> reject.
        const CondensationResult bad = condenseStatic(m, {});
        checkTrue("empty-boundary condensation rejected", !bad.ok, bad.why);
        // guard B: out-of-range boundary DOF -> reject.
        const CondensationResult oor = condenseStatic(m, { 9999 });
        checkTrue("out-of-range boundary rejected", !oor.ok, oor.why);
    }

    // ---- F17: grillage idealization of a simply-supported isotropic plate ----
    {
        using namespace frame::grillage;
        const real Epl = 30000.0, nu = 0.3;            // concrete-like, nu = 0.3
        const real Gpl = Epl / (2.0 * (1.0 + nu));     // so the torsion recipe matches plate theory
        Material pmat(Epl, Gpl, 2400.0);

        PlateSpec sp; sp.a = 4000.0; sp.b = 4000.0; sp.t = 250.0; sp.q = 0.025;

        // Kirchhoff plate (simply supported square, uniform load): w_c = alpha q a^4 / D,
        // D = E t^3 / [12 (1-nu^2)], alpha = 0.00406 (Timoshenko table, nu = 0.3).
        const real D  = Epl * sp.t * sp.t * sp.t / (12.0 * (1.0 - nu * nu));
        const real wc = 0.00406 * sp.q * sp.a * sp.a * sp.a * sp.a / D;
        std::printf("[F17] grillage plate  D=%.6g  plate w_c=%.6g mm\n", D, wc);

        auto centerDeflection = [&](int n) -> real {
            FrameModel m; std::string why; PlateSpec s = sp; s.nx = n; s.ny = n;
            if (!buildGrillage(m, s, pmat, why)) return 1e30;
            const SolveResult r = solve(m);
            if (r.singular) return 1e30;
            return std::fabs(r.disp(gridNode(n / 2, n / 2, n), Uz));
        };

        // build + non-singular + load conservation (sum of vertical reactions == q a b)
        {
            FrameModel m; std::string why; PlateSpec s = sp; s.nx = 8; s.ny = 8;
            checkTrue("grillage builds", buildGrillage(m, s, pmat, why), why);
            const SolveResult r = solve(m);
            checkTrue("grillage non-singular", !r.singular, r.diagnostic);
            real sumRz = 0;
            for (size_t k = 0; k < m.nodes.size(); ++k) sumRz += r.reaction((int)k, Uz);
            checkClose("load conservation: sum Rz = q a b", std::fabs(sumRz), sp.q * sp.a * sp.b, 1e-6);
        }

        // The nu-matched grillage reproduces the plate's Dx=Dy=2H=D, so the center
        // deflection sits within a few % of plate theory at every mesh density and is
        // mesh-converged (it settles to a value ~2% off plate theory — the residual is
        // load-lumping + the moment-distribution mismatch, NOT a convergence-to-exact).
        const real d4 = centerDeflection(4), d8 = centerDeflection(8), d12 = centerDeflection(12);
        const real e4 = std::fabs(d4 - wc) / wc, e8 = std::fabs(d8 - wc) / wc, e12 = std::fabs(d12 - wc) / wc;
        std::printf("   N=4 w=%.5g (err %.2f%%)  N=8 w=%.5g (err %.2f%%)  N=12 w=%.5g (err %.2f%%)\n",
                    d4, 100 * e4, d8, 100 * e8, d12, 100 * e12);
        checkTrue("grillage deflection within 5% of plate theory (all meshes)",
                  e4 < 0.05 && e8 < 0.05 && e12 < 0.05,
                  "e4=" + std::to_string(e4) + " e8=" + std::to_string(e8) + " e12=" + std::to_string(e12));
        checkTrue("grillage mesh-converged (|d12-d8| < 2% d8)",
                  std::fabs(d12 - d8) < 0.02 * d8, "d8=" + std::to_string(d8) + " d12=" + std::to_string(d12));
    }

    // ---- F18: irreversible damage history field (monotonic + unloading no-heal) ----
    {
        using namespace frame::damage;
        DamageLaw law; law.kappa0 = 1.0; law.kappaf = 5.0;
        std::printf("[F18] irreversible damage history  kappa0=%.1f kappaf=%.1f\n", law.kappa0, law.kappaf);

        // closed-form linear-softening law anchors
        checkClose("d(kappa0) = 0", damageOf(1.0, law), 0.0, 1e-12);
        checkClose("d(kappaf) = 1", damageOf(5.0, law), 1.0, 1e-12);
        checkClose("d(2.0) = 0.625", damageOf(2.0, law), 0.625, 1e-12);
        checkTrue("d = 0 below onset", damageOf(0.5, law) == 0.0);
        checkTrue("d clamped to 1 above kappaf", damageOf(9.0, law) == 1.0);

        // a load / partial-unload / reload history; H must be the running max of psi.
        DamageState st;
        const real seq[7] = { 0.5, 1.5, 1.2, 2.0, 0.3, 4.0, 1.0 };
        real prevD = -1.0; bool monotone = true;
        real dPeak2 = 0, dUnload2 = 0, dPeak4 = 0, dUnload4 = 0;
        for (int k = 0; k < 7; ++k) {
            const real d = st.update(seq[k], law);
            if (d < prevD - 1e-15) monotone = false;
            prevD = d;
            if (k == 3) dPeak2  = d;   // psi=2.0 -> H=2.0
            if (k == 4) dUnload2 = d;  // psi=0.3 -> H stays 2.0 (no heal)
            if (k == 5) dPeak4  = d;   // psi=4.0 -> H=4.0
            if (k == 6) dUnload4 = d;  // psi=1.0 -> H stays 4.0 (no heal)
        }
        checkTrue("damage monotone non-decreasing", monotone);
        checkClose("H = running max (= 4.0)", st.H, 4.0, 1e-12);
        checkTrue("unloading does not heal (0.3 after 2.0)", dUnload2 == dPeak2, "d=" + std::to_string(dUnload2));
        checkTrue("unloading does not heal (1.0 after 4.0)", dUnload4 == dPeak4, "d=" + std::to_string(dUnload4));
        checkClose("peak damage d(H=4) = 0.9375", dPeak4, 0.9375, 1e-12);
        checkClose("secant factor (1-d)", st.secant(law), 1.0 - 0.9375, 1e-12);
    }

    // ---- F19: construction-discipline COLUMN (cantilever beam-column) ----
    {
        const real h = 3000.0, P = 50000.0, H = 2000.0;
        FrameModel m; construct::buildColumn(m, h, P, H, mat, sec);
        const SolveResult r = solve(m);
        std::printf("[F19] construct::buildColumn  h=%.0f P=%.0f H=%.0f\n", h, P, H);
        checkTrue("not singular", !r.singular, r.diagnostic);
        checkClose("lateral u_x(top) = H h^3/3EI", std::fabs(r.disp(1, Ux)), H * h * h * h / (3.0 * E * sec.Iz), 1e-6);
        checkClose("axial u_z(top) = P h/EA", std::fabs(r.disp(1, Uz)), P * h / (E * sec.A), 1e-6);
    }

    // ---- F20: construction-discipline BEAM (clamped-clamped UDL) ----
    {
        const real w = 5.0, L = 4000.0; const int nseg = 4;
        FrameModel m; construct::buildFixedBeam(m, L, w, nseg, mat, sec);
        const SolveResult r = solve(m);
        std::printf("[F20] construct::buildFixedBeam  w=%.1f L=%.0f nseg=%d\n", w, L, nseg);
        checkTrue("not singular", !r.singular, r.diagnostic);
        checkClose("fixed-end moment |M| = wL^2/12", std::fabs(r.memberForces[0].endI.Mz), w * L * L / 12.0, 1e-6);
        checkClose("midspan moment |M| = wL^2/24", std::fabs(r.memberForces[nseg / 2 - 1].endJ.Mz), w * L * L / 24.0, 1e-5);
        checkClose("midspan deflection = wL^4/384EI", std::fabs(r.disp(nseg / 2, Uz)), w * L * L * L * L / (384.0 * E * sec.Iz), 1e-6);
        checkClose("reaction R_z(node0) = wL/2", std::fabs(r.reaction(0, Uz)), w * L / 2.0, 1e-6);
    }

    // ---- F21: construction-discipline WALL (equivalent diagonal brace, K round-trip) ----
    {
        const real bay = 5000.0, hgt = 3000.0, Ktar = 1000.0, H = 10000.0;
        FrameModel m; construct::buildEquivalentBraceWall(m, bay, hgt, Ktar, H, mat);
        SolveOptions opt; opt.enableReleases = true;
        const SolveResult r = solve(m, opt);
        std::printf("[F21] construct::buildEquivalentBraceWall  K_target=%.1f\n", Ktar);
        checkTrue("not singular", !r.singular, r.diagnostic);
        const real ux = std::fabs(r.disp(1, Ux));
        checkClose("lateral stiffness round-trip K=H/u_x", H / ux, Ktar, 1e-6);
    }

    // ---- F22: construction-discipline SLAB (delegates to grillage) ----
    {
        using namespace frame::grillage;
        const real Epl = 30000.0, nu = 0.3; const real Gpl = Epl / (2.0 * (1.0 + nu));
        Material pmat(Epl, Gpl, 2400.0);
        PlateSpec sp; sp.a = 4000.0; sp.b = 4000.0; sp.t = 250.0; sp.q = 0.025; sp.nx = 8; sp.ny = 8;
        FrameModel m; std::string why;
        const bool ok = construct::buildSlab(m, sp, pmat, why);
        const SolveResult r = solve(m);
        const real D  = Epl * sp.t * sp.t * sp.t / (12.0 * (1.0 - nu * nu));
        const real wc = 0.00406 * sp.q * sp.a * sp.a * sp.a * sp.a / D;
        std::printf("[F22] construct::buildSlab (-> grillage)\n");
        checkTrue("slab builds", ok, why);
        checkTrue("slab non-singular", !r.singular, r.diagnostic);
        checkClose("slab center deflection ~ plate theory", std::fabs(r.disp(gridNode(4, 4, 8), Uz)), wc, 0.05);
    }

    std::printf("\n%s  (failures=%d)\n", g_fail == 0 ? "ALL PASS" : "FAILURES", g_fail);
    return g_fail == 0 ? 0 : 1;
}
