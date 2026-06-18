#pragma once
#include "FrameCore/FrameSolver.h"      // PreparedSystem (opaque PIMPL)
#include "FrameCore/FrameModel.h"
#include "FrameCore/SolveResult.h"
#include "FrameCore/SnSolveOptions.h"

namespace frame {

// Opt-in self-built supernodal Cholesky solve lane (research-validated; see docs/PROGRESS_R_supernodal.md).
// Computes the SAME SolveResult as solveLoad() -- identical RHS
// assembly, reactions (K*u - F) and element force recovery -- but replaces the LDLT forward/back
// substitution with the self-built supernodal factor (METIS nested-dissection ordering + BLAS3 dense
// panels via OpenBLAS, level-set parallel). Research benchmark: ~on par with MKL-CHOLMOD (vsCH
// 1.15-1.26x, 17k best ~1.03x; not a guaranteed win over CHOLMOD).
//
// Stateless (one-shot): each call does analyze + factorize + solve. It falls back to the
// PreparedSystem's LDLT (the oracle) whenever the lane is disabled, the factor is not SPD/finite, or
// the supernodal lane is not compiled in (FRAMECORE_SUPERNODAL=0, e.g. when the conda OpenBLAS/METIS
// env is absent) -- so the numeric result never deviates from the direct solve. With
// opts.enabled == false it is a drop-in equal to solveLoad. The factor-REUSE speedup (the real
// production value for a fixed structure with many load cases) lives in SnSession.
//
// The signature is always declared (so callers/tests link in any build); the supernodal body is
// what FRAMECORE_SUPERNODAL gates inside SnSolver.cpp.
//
// Parallel entry point: solve(), solveLoad() and assembleAndFactor() are NOT changed, and the public
// boundary stays Eigen-free (POD SolveResult).
//
// THREAD SAFETY (R2 audit SLV-01): solveLoadSupernodal calls into OpenBLAS through the
// supernodal factor, which uses `openblas_set_num_threads(...)` as process-global state.
// **Do not call solveLoadSupernodal concurrently from multiple threads on the same process**
// (the OpenBLAS thread-count mutation races between calls; the dense panel kernels themselves
// are thread-safe under a single global thread-count setting). Serialise supernodal calls or
// fence them with an external lock. The default LDLT lane (`solveLoad`) has no such constraint.
FRAMECORE_API SolveResult solveLoadSupernodal(const PreparedSystem& prepared,
                                              const FrameModel& model,
                                              const SnSolveOptions& opts = {});

}  // namespace frame
