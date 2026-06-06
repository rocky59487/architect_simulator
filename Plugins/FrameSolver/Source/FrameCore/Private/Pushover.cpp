#include "FrameCore/Pushover.h"
#include "FrameCore/FrameSolver.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

namespace frame {

namespace {

// Deep-copy the model and re-point each member's Material*/Section* into THIS copy's
// vectors (a plain copy would leave them dangling at the source). std::vector move on
// return preserves the buffer, so the rebound pointers stay valid.
FrameModel rebindCopy(const FrameModel& src) {
    FrameModel m = src;
    if (!src.materials.empty() && !src.sections.empty()) {
        for (auto& mem : m.members) {
            const std::size_t mi = static_cast<std::size_t>(mem.mat - src.materials.data());
            const std::size_t si = static_cast<std::size_t>(mem.sec - src.sections.data());
            mem.mat = m.materials.data() + mi;
            mem.sec = m.sections.data() + si;
        }
    }
    return m;
}

} // namespace

PushoverResult pushover(const FrameModel& model, const SolveOptions& baseOpts, int maxSteps) {
    PushoverResult R;

    FrameModel m = rebindCopy(model);
    SolveOptions opts = baseOpts;
    opts.enableReleases = true;            // we insert hinges as member-end releases

    const std::size_t ne = m.members.size();
    std::vector<real> Mp(ne, 0);
    for (std::size_t e = 0; e < ne; ++e) {
        Mp[e] = m.members[e].sec->Wz() * m.members[e].mat->cap.bend;   // plastic-moment proxy
    }

    std::vector<real> Mi(ne, 0), Mj(ne, 0);    // accumulated local Mz at end i / j
    std::vector<bool> hi(ne, false), hj(ne, false);
    std::vector<real> uAcc;
    real lambda = 0;

    for (int step = 0; step < std::max(1, maxSteps); ++step) {
        const SolveResult sr = solve(m, opts);
        if (sr.singular) {
            R.ok = true;
            R.collapseLambda = lambda;
            R.diagnostic = "collapse mechanism after " + std::to_string(R.steps.size()) + " hinge(s)";
            return R;
        }
        if (uAcc.empty()) uAcc.assign(sr.u.size(), 0.0);

        // smallest positive load increment to drive some un-hinged end to +/-Mp
        real bestD = std::numeric_limits<real>::infinity();
        int  bestE = -1, bestEnd = -1;
        for (std::size_t e = 0; e < ne; ++e) {
            for (int end = 0; end < 2; ++end) {
                if (end == 0 ? hi[e] : hj[e]) continue;
                const real Mref = (end == 0) ? sr.memberForces[e].endI.Mz : sr.memberForces[e].endJ.Mz;
                if (std::abs(Mref) < 1e-9) continue;
                const real Macc   = (end == 0) ? Mi[e] : Mj[e];
                const real target = (Mref > 0) ? Mp[e] : -Mp[e];
                const real d = (target - Macc) / Mref;
                if (d > 1e-12 && d < bestD) { bestD = d; bestE = (int)e; bestEnd = end; }
            }
        }
        if (bestE < 0) {
            R.ok = true; R.collapseLambda = lambda;
            R.diagnostic = "no further hinge can form (stable below proxy capacity)";
            return R;
        }

        lambda += bestD;
        for (std::size_t e = 0; e < ne; ++e) {
            Mi[e] += bestD * sr.memberForces[e].endI.Mz;
            Mj[e] += bestD * sr.memberForces[e].endJ.Mz;
        }
        for (std::size_t k = 0; k < uAcc.size(); ++k) uAcc[k] += bestD * sr.u[k];

        PushoverStep ps;
        ps.lambda = lambda; ps.u = uAcc; ps.hingeMember = bestE; ps.hingeEnd = bestEnd;
        R.steps.push_back(ps);

        // insert the plastic hinge: release the two bending rotations at that end
        if (bestEnd == 0) { m.members[bestE].release[4] = m.members[bestE].release[5] = true;  hi[bestE] = true; }
        else              { m.members[bestE].release[10] = m.members[bestE].release[11] = true; hj[bestE] = true; }
    }

    R.ok = true;
    R.collapseLambda = lambda;
    R.diagnostic = "max pushover steps reached without a full mechanism";
    return R;
}

} // namespace frame
