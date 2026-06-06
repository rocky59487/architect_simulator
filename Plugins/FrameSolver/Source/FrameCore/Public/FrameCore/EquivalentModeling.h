#pragma once
//
// #5 Equivalent modeling (seam 3: preprocessing fed to the solver). Pure helpers that
// fold higher-level construction (slabs, walls, T-beams) into the frame model: ACI
// effective flange width + composite T-section, tributary line loads, and an equivalent
// diagonal brace for a shear wall. Engine-agnostic (no Eigen/UE) so the standalone gate
// validates the same code the runtime uses.
//
#include "FrameCore/FrameTypes.h"
#include "FrameCore/Section.h"

namespace frame { namespace equiv {

// ACI 318-19 Table 6.3.2.1 effective flange width of a symmetric T-beam. Each side
// overhang is the least of 8*hf, sw/2 (half the clear distance to the adjacent web),
// and ln/8 (one-eighth of the clear span):  be = bw + 2*overhang.
FRAMECORE_API real effectiveFlangeWidthACI(real bw, real hf, real swClear, real lnClear);

// Composite T-section: flange (be x hf) on top of a web (bw x (h-hf)), total depth h.
// Fills a frame::Section about the COMPOSITE centroid: Iz = strong-axis 2nd moment,
// Iy = weak axis, cz = max extreme-fibre distance, J/shear-areas approximate (sum of
// rectangles / web-dominated). Bending in the vertical plane uses Iz.
FRAMECORE_API Section compositeTeeSection(real bw, real h, real be, real hf);

// Equivalent single-diagonal brace area reproducing a target lateral (racking) stiffness
// K for a bay of width L and height H. A pin-ended diagonal of length Ld=sqrt(L^2+H^2)
// at angle theta (cos theta = L/Ld) contributes lateral stiffness k = E*A*L^2/Ld^3, so
// A = K*Ld^3/(E*L^2). Use the result as a truss (axial-only) member.
FRAMECORE_API real equivalentBraceArea(real K, real E, real L, real H);

// Tributary line load (N/mm) on a beam from a slab area load (N/mm^2) over a tributary
// width (mm). The building block for folding slab loads into beam UDLs (load conserved:
// sum of tributary widths == panel width).
inline real tributaryLineLoad(real areaLoad, real tributaryWidth) { return areaLoad * tributaryWidth; }

}} // namespace frame::equiv
