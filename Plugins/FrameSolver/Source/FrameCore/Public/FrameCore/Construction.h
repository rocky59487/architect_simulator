#pragma once
#include "FrameCore/FrameModel.h"
#include "FrameCore/Grillage.h"
#include <string>

namespace frame { namespace construct {

// #12 construction-discipline builders — "construction vocabulary == structural
// vocabulary". Each building element (column / beam / slab / wall) maps to its own
// structural idealization and emits a frame::FrameModel ready for solve(). Engine-pure
// (units mm / N / MPa); the runtime construction tools wrap these (UE types stay above).

// COLUMN -> vertical cantilever (fixed base at origin, free top at (0,0,height)). Axial
// gravity P downward + lateral shear H in +X. Idealization = cantilever beam-column.
// Oracle: u_x(top) = H*height^3/(3 E I),  u_z(top) = -P*height/(E A). Also exercises the
// vertical-member refVec degeneracy fallback.
FRAMECORE_API void buildColumn(FrameModel& m, real height, real axialP, real lateralH,
                               const Material& mat, const Section& sec);

// BEAM -> clamped-clamped (moment-connected) beam along +X under downward UDL w, split
// into nSeg segments (even -> midspan is a node). Idealization = fixed-fixed beam.
// Oracle: |M_end| = w L^2/12,  |M_mid| = w L^2/24,  midspan deflection = w L^4/(384 E Iz),
// each support reaction = w L/2.
FRAMECORE_API void buildFixedBeam(FrameModel& m, real span, real w, int nSeg,
                                  const Material& mat, const Section& sec);

// SLAB -> isotropic floor slab idealized as a grillage (delegates to buildGrillage). The
// slab tool is just the plate idealization under the construction vocabulary.
FRAMECORE_API bool buildSlab(FrameModel& m, const grillage::PlateSpec& spec,
                             const Material& mat, std::string& why);

// WALL -> shear wall replaced by an equivalent diagonal brace sized to a target lateral
// stiffness K (equivalentBraceArea). A single truss-pin brace runs from the fixed base
// to a top node that may sway only in +X; lateral load H is applied there. Idealization =
// equivalent brace. Oracle: H / u_x(top) == Ktarget (the sizing round-trip). Solve with
// SolveOptions.enableReleases = true (the brace is a 2-force member). bay = horizontal
// (x) projection, height = vertical (y) projection of the brace.
FRAMECORE_API void buildEquivalentBraceWall(FrameModel& m, real bay, real height,
                                            real Ktarget, real lateralH, const Material& mat);

}} // namespace frame::construct
