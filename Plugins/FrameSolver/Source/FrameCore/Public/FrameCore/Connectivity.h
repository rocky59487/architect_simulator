#pragma once
#include "FrameCore/FrameModel.h"
#include <vector>

namespace frame {

// A rigid debris chunk cut loose from the grounded structure: the node/member/shell id
// lists plus aggregate mass properties for the physics-engine (UE5 Chaos) handoff.
//
// Units follow the engine's N-mm-tonne-s system: mass in tonnes (Material.rho is kg/m^3,
// bridged by rho*1e-12 like addSelfWeight / the mass matrix), com in mm (global), inertia
// in tonne*mm^2. Mass model (honest scope): STRUCTURAL self-mass only — members as slender
// rods (rho*A*L, zero inertia about their own axis), shells as thin laminae (rho*t*Area,
// exact two-triangle closed form). Applied loads carry no mass (same scope as the modal
// mass matrix). Initial velocities: the STATIC driver (runProgressiveCollapse) leaves vel/
// angVel ZERO -- a static engine cannot estimate separation speed, so the physics layer would
// start the fragment from rest under gravity. The DYNAMIC driver (runDynamicCollapse, S2) fills
// them from the consistent-mass momentum captured at the detach instant (see vel/angVel below).
struct FragmentCluster {
    std::vector<NodeId>   nodes;     // ascending id (deterministic output)
    std::vector<MemberId> members;   // active members fully inside the component, ascending
    std::vector<int>      shells;    // active shell ids fully inside the component, ascending

    real mass = 0;                   // tonnes (0 when every piece has rho == 0)
    Vec3 com;                        // centre of mass (mm, global). Falls back to the mean
                                     // node position when mass == 0 (inertia stays zero).
    // Inertia TENSOR about com, GLOBAL axes, stored { Ixx, Iyy, Izz, Ixy, Ixz, Iyz }.
    // The off-diagonals are the tensor MATRIX ENTRIES (i.e. Ixy = -integral(x*y dm)), so
    // I = [[Ixx,Ixy,Ixz],[Ixy,Iyy,Iyz],[Ixz,Iyz,Izz]] with NO extra sign flip on the
    // consumer side — read them straight into a 3x3.
    real inertia[6] = { 0, 0, 0, 0, 0, 0 };

    // Rigid-body velocity of the fragment at the detach instant, GLOBAL axes. ZERO from the
    // static driver (old behaviour, POD default). The dynamic driver (S2) fills them from the
    // fragment's consistent-mass momentum at separation: vel = p / mass, angVel = I^{-1} L_com
    // (I = the inertia tensor above). Hand straight to the physics engine as the chunk's
    // initial linear/angular velocity.
    Vec3 vel;       // centre-of-mass linear velocity (mm/s, global)
    Vec3 angVel;    // angular velocity (rad/s, global) = I^{-1} L_com
};

// Connected-component analysis of the ACTIVE element graph: vertices = nodes, edges =
// active members (i-j) + active shell perimeter edges. A component is GROUNDED iff any of
// its nodes has any fixed[d] = true (a prescribed-displacement support still grounds).
// Components are formed over nodes attached to at least one active element; bare nodes are
// reported separately in looseNodes.
struct ConnectivityResult {
    // Ungrounded element-carrying components, ordered by smallest contained node id. These
    // are the debris chunks: nothing connects them to a support, so the collapse driver
    // deactivates them wholesale and hands them to the physics layer.
    std::vector<FragmentCluster> detached;

    // Nodes attached to NO active element that still have at least one free DOF. Their free
    // DOFs carry zero stiffness (the solver would flag a mechanism), so the collapse driver
    // pins them. Fully fixed bare nodes are harmless and are not listed. Ascending id.
    std::vector<NodeId> looseNodes;

    int  groundedComponents = 0;     // element-carrying components that reach a support
    bool valid = false;              // false when the model fails validate()
};
FRAMECORE_API ConnectivityResult analyzeConnectivity(const FrameModel& model);

} // namespace frame
