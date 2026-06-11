// UE automation mirror of the standalone F45/F46/F47 -- BESO topology optimization + N2 robustness.
// (1) the BESO sensitivity is the exact element strain energy, so sum(alpha) == 1/2 F.u (energy
// balance); (2) evolutionary removal on a ground structure trims material and the best topology still
// factors (mechanism guard); (3) the N2 collapse-robustness constraint keeps the topology able to
// survive any single-member removal (and locks the protective bars), where the unconstrained sister
// topology cannot. NB: 'IN'/'OUT' are Windows SAL macros (via CoreMinimal.h) -- do not name constants IN.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "FrameCore/FrameSolver.h"
#include "FrameCore/Topology.h"
#include "FrameCore/Collapse.h"
#include <cmath>
#include <vector>

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreTopologyTest,
    "FrameCore.Topology.BESO",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreTopologyTest::RunTest(const FString& /*Parameters*/)
{
    using namespace frame;
    const double E = 210000.0, G = 80769.0;
    Material mat(E, G, 7850.0); mat.cap = Capacity::make(300.0, 300.0, 180.0);
    const Section sec = Section::Rectangular(80.0, 80.0);

    // (1) sensitivity = element strain energy -> energy balance sum(alpha) == 1/2 F.u (3D L-frame).
    {
        FrameModel m; m.materials.push_back(mat); m.sections.push_back(sec);
        m.nodes.push_back(Node(0, 0, 0, 0)); m.nodes[0].fixAll();
        m.nodes.push_back(Node(1, 1500, 0, 0));
        m.nodes.push_back(Node(2, 1500, 0, 1200));
        { Member a(0, 0, 1, 0, 0); a.refVec = { 0, 0, 1 }; m.members.push_back(a); }
        { Member b(1, 1, 2, 0, 0); b.refVec = { 1, 0, 0 }; m.members.push_back(b); }
        NodalLoad l; l.node = 2; l.comp[Ux] = 8000.0; l.comp[Uz] = -5000.0; l.comp[Ry] = 3.0e6;
        m.nodalLoads.push_back(l);
        const SolveResult r = solve(m);
        double sumA = 0;
        for (int e = 0; e < (int)m.members.size(); ++e) sumA += memberStrainEnergy(m, r, e);
        double C = 0;
        for (const NodalLoad& L : m.nodalLoads) {
            const int ni = m.nodeIndex(L.node);
            for (int d = 0; d < 6; ++d) C += L.comp[d] * r.u[(size_t)gdof(ni, d)];
        }
        TestTrue(TEXT("energy balance sum(alpha) == 1/2 F.u"),
                 !r.singular && FMath::Abs(sumA - 0.5 * C) / FMath::Max(1.0, FMath::Abs(0.5 * C)) < 1e-9);
    }

    // (2) evolutionary removal on a small ground structure; the best topology must still factor.
    {
        const int NX = 5, NZ = 3; const double SP = 500.0;
        auto nid = [&](int i, int k) { return k * (NX + 1) + i; };
        FrameModel m; m.materials.push_back(mat); m.sections.push_back(sec);
        for (int k = 0; k <= NZ; ++k)
            for (int i = 0; i <= NX; ++i) {
                Node n(nid(i, k), i * SP, 0.0, k * SP);
                if (i == 0) n.fixAll();
                else { n.fixed[Uy] = true; n.fixed[Rx] = true; n.fixed[Rz] = true; }
                m.nodes.push_back(n);
            }
        auto add = [&](int a, int b) {
            Member mem((int)m.members.size(), a, b, 0, 0);
            const Vec3 d = m.nodes[(size_t)b].pos - m.nodes[(size_t)a].pos;
            mem.refVec = (FMath::Abs(d.z) > FMath::Abs(d.x)) ? Vec3{ 1, 0, 0 } : Vec3{ 0, 0, 1 };
            m.members.push_back(mem);
        };
        for (int k = 0; k <= NZ; ++k) for (int i = 0; i < NX; ++i) add(nid(i, k), nid(i + 1, k));
        for (int k = 0; k < NZ; ++k) for (int i = 0; i <= NX; ++i) add(nid(i, k), nid(i, k + 1));
        for (int k = 0; k < NZ; ++k) for (int i = 0; i < NX; ++i) { add(nid(i, k), nid(i + 1, k + 1)); add(nid(i + 1, k), nid(i, k + 1)); }
        NodalLoad l; l.node = nid(NX, NZ / 2); l.comp[Uz] = -1.0e4; m.nodalLoads.push_back(l);
        const int active0 = (int)m.members.size();
        BESOOptions o; o.targetVolFrac = 0.5; o.evolRate = 0.06; o.maxIter = 80;
        const BESOResult R = runBESO(m, o);
        int activeF = 0; for (char a : R.finalActive) if (a) ++activeF;
        TestTrue(TEXT("BESO removed material"), activeF < active0 && R.reason != BESOStop::Invalid);
        FrameModel mb = m;
        for (int e = 0; e < (int)mb.members.size(); ++e) mb.members[(size_t)e].active = R.bestActive[(size_t)e] != 0;
        const SolveResult rb = solve(mb);
        TestTrue(TEXT("best topology non-singular (load path intact)"), !rb.singular);
    }

    // (3) N2: robust BESO topology survives every single-member removal + locks bars; plain one cannot.
    {
        FrameModel m; m.materials.push_back(mat);
        m.sections.push_back(sec);
        m.sections.push_back(Section::Rectangular(50.0, 50.0));
        m.nodes.push_back(Node(0, 0, 0, 0));     m.nodes[0].fixAll();   // support A
        m.nodes.push_back(Node(1, 0, 0, 2000));  m.nodes[1].fixAll();   // support B
        m.nodes.push_back(Node(2, 1500, 0, 0));                         // relay M (free)
        m.nodes.push_back(Node(3, 3000, 0, 1000));                      // loaded C (free)
        m.members.push_back(Member(0, 0, 2, 0, 0));   // g (main)
        m.members.push_back(Member(1, 2, 3, 0, 0));   // m (main)
        m.members.push_back(Member(2, 1, 3, 0, 1));   // b (small backup)
        NodalLoad l; l.node = 3; l.comp[Uz] = -2000.0; m.nodalLoads.push_back(l);
        auto fragile = [&](const std::vector<char>& act) -> bool {
            FrameModel mm = m;
            for (int e = 0; e < (int)mm.members.size(); ++e) mm.members[(size_t)e].active = act[(size_t)e] != 0;
            for (int e = 0; e < (int)mm.members.size(); ++e) {
                if (!mm.members[(size_t)e].active) continue;
                CollapseOptions co; co.dlf = 1.0; co.removeThreshold = 1.0;
                co.initialRemovals = { mm.members[(size_t)e].id };
                if (runProgressiveCollapse(mm, co).outcome == CollapseOutcome::Collapsed) return true;
            }
            return false;
        };
        BESOOptions ou; ou.targetVolFrac = 0.6; ou.evolRate = 0.34; ou.maxIter = 20;
        const BESOResult Ru = runBESO(m, ou);
        BESOOptions orb = ou; orb.redundancyCheckEvery = 1; orb.redundancySamples = 0;
        orb.redundancy.dlf = 1.0; orb.redundancy.removeThreshold = 1.0;
        const BESOResult Rr = runBESO(m, orb);
        TestTrue(TEXT("unconstrained topology is fragile (single removal collapses)"), fragile(Ru.finalActive));
        TestTrue(TEXT("robust topology survives every single removal + locks bars"),
                 !fragile(Rr.finalActive) && !Rr.protectedMembers.empty());
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
