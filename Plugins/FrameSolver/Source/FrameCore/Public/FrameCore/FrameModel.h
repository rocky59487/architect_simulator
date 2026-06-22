#pragma once
#include "FrameCore/Node.h"
#include "FrameCore/Member.h"
#include "FrameCore/Material.h"
#include "FrameCore/Section.h"
#include "FrameCore/Load.h"
#include "FrameCore/Shell.h"
#include "FrameCore/Hinge.h"
#include <vector>
#include <string>

namespace frame {

// Owns the whole structural model. Materials/Sections are referenced by INDEX
// (matIdx/secIdx) from Members and ShellQuads — never by pointer — so growing any
// vector can never dangle a reference; validate() range-checks the indices.
struct FrameModel {
    std::vector<Node>            nodes;
    std::vector<Member>          members;
    std::vector<ShellQuad>       shells;        // MITC4 flat-shell facets (parallel to members)
    std::vector<Material>        materials;   // optional storage to keep refs alive
    std::vector<Section>         sections;    // optional storage to keep refs alive
    std::vector<NodalLoad>       nodalLoads;
    std::vector<MemberUDL>       memberUDLs;
    std::vector<ShellPressure>   shellPressures;
    std::vector<PlasticHinge>    hinges;        // formed plastic hinges (stage 4a) -- model
                                                // STATE, honoured regardless of enableReleases;
                                                // a hinge on an INACTIVE member is inert

    int dofCount() const { return DOF_PER_NODE * static_cast<int>(nodes.size()); }

    // Index into nodes[] / members[] / shells[] for a given id, or -1 if not found.
    // v3.4 Phase 2: FRAMECORE_API added so the FrameCoreUE consumer module (separate
    // DLL) can resolve the engine id -> slot lookup needed for SolveResult marshal.
    // Pure facade annotation, no behaviour change.
    FRAMECORE_API int nodeIndex(NodeId id) const;
    FRAMECORE_API int memberIndex(MemberId id) const;
    FRAMECORE_API int shellIndex(int id) const;

    // Structural sanity (ids resolve, non-null mat/sec, positive A/I/E/L). On
    // failure returns false and fills `why`.
    // warpTol relaxes the non-coplanar shell-quad rejection (default 1e-6 = strict, today's gate;
    // SolveOptions::warpTolerance threads through for warped free-surface meshes).
    FRAMECORE_API bool validate(std::string& why, real warpTol = 1.0e-6) const;
};

} // namespace frame
