from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import os
import re


ROOT = Path(__file__).resolve().parents[3]
OUT = Path(__file__).resolve().parent
SAMPLE_LESSON1 = Path(
    os.environ.get("SAMPLE_LESSON1", str(OUT / "lesson_01_state_vectors_local_frames.md"))
)


@dataclass
class Lesson:
    no: int
    title: str
    phase: str
    objective: str
    source: list[str]
    math: list[str]
    data: list[str]
    invariants: list[str]
    code: list[str]
    examples: list[str]
    pitfalls: list[str]
    tests: list[str]
    exercise: str
    next_title: str


LESSONS: list[Lesson] = [
    Lesson(2, "運動學、狀態方程與 engine state 分層", "Phase A - 數學與引擎最小地基",
           "把 particle、rigid body、structural node 三種狀態表示拆開，避免把剛體引擎和 FEM 節點狀態混成一團。",
           ["FrameCore/Node.h", "FrameCore/SolveResult.h", "ModalDynamics.h", "CorotationalAnalysis.h"],
           ["x_dot = v", "M q_ddot + C q_dot + K q = f(t)", "R_dot = [omega]_x R", "x = X + u", "theta is infinitesimal only in the linear core"],
           ["ParticleState{x,v,m}", "RigidBodyState{x,R,v,omega,m,I}", "StructuralState{X,u,theta,udot,thetadot}", "StateVector q in R^(6n)"],
           ["state type must reveal its kinematic assumptions", "linear structural rotation is not finite SO(3)", "all time derivatives have explicit units"],
           ["Define three POD state structs", "Implement derivative evaluation", "Add unit comments beside every field", "Write conversion notes but no implicit conversion operators"],
           ["Compare a falling particle and a vibrating cantilever tip", "Show why rigid body orientation cannot be stored as three linear rotations", "Map SolveResult.u into StructuralState"],
           ["using Rx/Ry/Rz as finite Euler angles in linear FEM", "hiding units in Vec3", "mixing current x and reference X"],
           ["dimension check for every derivative", "round-trip structural q -> node states -> q", "rigid identity orientation keeps local inertia unchanged"],
           "實作三種 state struct，並寫一個函式把 FrameCore 的 6N 位移向量映射成 StructuralState 陣列。", "數值積分器"),
    Lesson(3, "微分方程、時間步進與 Newmark/RK 家族", "Phase A - 數學與引擎最小地基",
           "從一階 ODE 到二階結構動力方程，建立 integrator interface，並釐清顯式、半隱式、隱式方法的穩定性。",
           ["ModalDynamics.cpp", "DynamicCollapse.cpp", "docs/VERIFICATION.md"],
           ["y_dot = f(t,y)", "x_{n+1}=x_n+h v_n", "v_{n+1}=v_n+h a(x_n)", "M a_{n+1}+C v_{n+1}+K x_{n+1}=F_{n+1}", "Newmark beta=1/4, gamma=1/2"],
           ["Integrator interface", "StepContext{dt,t}", "StateDerivative", "NewmarkState{x,v,a}"],
           ["dt is part of the numerical model", "energy behavior must be characterized, not guessed", "implicit solve must expose failure"],
           ["Implement explicit Euler", "Implement symplectic Euler", "Implement RK4 for first-order systems", "Implement Newmark average acceleration for M,C,K"],
           ["Harmonic oscillator phase error", "Free fall analytic comparison", "SDOF damped oscillator", "modal coordinate stepping"],
           ["using Euler for stiff structural modes", "updating position with stale acceleration in Newmark", "comparing only final norm"],
           ["x(t)=x0+v0t+0.5gt^2", "undamped oscillator energy drift plot", "Newmark SDOF against analytic solution"],
           "建立 Integrator.hpp，至少支援 symplectic Euler 與 Newmark，並用 SDOF oscillator 寫能量誤差測試。", "質量、慣性與動量"),
    Lesson(4, "質量、慣性、動量與力累加器", "Phase A - 數學與引擎最小地基",
           "把力、力矩、線動量、角動量與慣性張量明確化，為 dynamic collapse fragment handoff 和剛體後端鋪路。",
           ["FragmentMomentum.h", "Connectivity.cpp", "DynamicCollapse.cpp", "Material.h"],
           ["p = m v", "L = I omega", "tau = r x f", "v_c = v + omega x r", "I_world = R I_body R^T"],
           ["MassProperties{mass,com,inertia}", "ForceAccumulator{force,torque}", "FragmentCluster{mass,com,inertia,vel,angVel}"],
           ["mass must be positive", "inertia tensor symmetric positive definite", "force application point must name its frame"],
           ["Implement addForceAtPoint(f, worldPoint)", "Implement translate inertia by parallel-axis theorem", "Implement body/world inertia transform"],
           ["single point mass", "slender rod inertia", "two-member detached fragment", "force couple produces torque only"],
           ["forgetting parallel-axis term", "using kg/m^3 directly in N-mm system", "computing torque around world origin instead of COM"],
           ["momentum closure p=sum m_i v_i", "I symmetric test", "pure couple net force zero"],
           "寫 MassProperties 合併函式，把兩根 beam 的 mass/com/inertia 合成一個 FragmentCluster 近似。", "幾何 primitive 與距離測試"),
    Lesson(5, "幾何 primitive 與距離/投影測試", "Phase B - 一般物理引擎核心",
           "建立 sphere、plane、AABB、segment、triangle 的最小幾何庫；它是 collision 與 shell/mesh 幾何檢查的共同底層。",
           ["FrameModel.cpp shell validation", "Shell.h", "MITC4ShellElement.cpp"],
           ["point-plane distance d=n dot (x-p0)", "segment closest t=clamp(dot(p-a,b-a)/||b-a||^2)", "AABB overlap interval test", "triangle normal n=(b-a)x(c-a)", "barycentric coordinates"],
           ["Sphere{c,r}", "Plane{n,d}", "AABB{min,max}", "Segment{a,b}", "Triangle{a,b,c}"],
           ["normal vectors must be normalized or explicitly marked unnormalized", "degenerate primitives must be rejected", "distance sign convention must be documented"],
           ["Implement closestPointSegment", "Implement pointTriangleBarycentric", "Implement AABB overlap", "Implement shell quad planarity check"],
           ["point above plane", "segment endpoint clamp", "zero-area triangle", "AABB touching faces"],
           ["normalizing a zero vector", "using epsilon in model units without scale", "confusing signed distance with absolute distance"],
           ["degenerate triangle returns false", "AABB overlap symmetric", "quad warp tolerance scales with max edge"],
           "建立 GeometryPrimitives.hpp，複製 FrameModel.cpp 裡 shell validation 需要的幾何檢查。", "Broadphase / Narrowphase"),
    Lesson(6, "碰撞檢測 II：Broadphase、SAT、GJK", "Phase B - 一般物理引擎核心",
           "從候選對生成到精確接觸測試，建立 contact manifold 的輸入端；這部分屬於剛體後端，不是 FrameCore 線性 FEM 核心。",
           ["FragmentCluster handoff", "UE Chaos boundary note in README", "Geometry primitive lesson"],
           ["broadphase: reduce O(n^2)", "SAT: separated if exists axis a with intervals disjoint", "support_A-B(d)=support_A(d)-support_B(-d)", "GJK simplex contains origin", "EPA gives penetration depth"],
           ["Collider{type,transform}", "BroadphaseProxy{AABB,id}", "ContactCandidate", "ContactManifold{normal,points,depth}"],
           ["broadphase cannot decide contact response", "manifold normal orientation must be consistent", "convex support mapping must be deterministic"],
           ["Implement sweep-and-prune on one axis", "Implement SAT for OBB-lite boxes", "Write GJK support interface", "Return manifold skeleton"],
           ["sphere-sphere", "AABB-AABB", "box-plane", "convex hull support point"],
           ["treating broadphase pair as collision", "normal flips between frames", "no tolerance around touching contacts"],
           ["candidate count sanity", "SAT no-overlap axis test", "GJK distance on separated shapes"],
           "寫一個最小 broadphase + sphere/sphere narrowphase，輸出 ContactManifold 給下一課 impulse solver。", "碰撞衝量"),
    Lesson(7, "碰撞衝量、接觸速度與反彈", "Phase B - 一般物理引擎核心",
           "把 contact manifold 轉成速度層 impulse，推導 effective mass 與 restitution；這是碎片進入剛體後端後的第一個 solver。",
           ["FragmentMomentum.h", "README scope boundary: Chaos owns rigid-body fall", "DynamicCollapse fragment velocity"],
           ["v_c = v + omega x r", "v_rel = n dot (v_Bc - v_Ac)", "j = -(1+e)v_rel / (n^T M_eff^-1 n)", "v' = v + M^-1 J^T lambda", "omega' = omega + I^-1 (r x J)"],
           ["RigidBody", "ContactPoint", "ImpulseAccumulator", "MaterialContact{restitution,friction}"],
           ["normal impulse is non-negative", "restitution applies only when closing speed exceeds threshold", "impulse changes velocity, not position"],
           ["Implement normal impulse", "Add angular contribution", "Add restitution clamp", "Accumulate impulses for warm starting"],
           ["1D ball-wall impact", "two equal masses exchange velocity", "off-center hit creates angular velocity"],
           ["applying impulse twice per frame", "using penetration depth as impulse", "forgetting inverse inertia rotation"],
           ["momentum conservation e=1", "kinetic energy non-increase for e<=1", "no impulse for separating contact"],
           "以 circle-plane 或 sphere-plane 寫一個 contact impulse solver，列印碰撞前後線動量。", "約束求解器"),
    Lesson(8, "約束求解器：Jacobian、KKT、PGS 與 PBD 邊界", "Phase B - 一般物理引擎核心",
           "把接觸、joint、限制條件都表示成約束列，建立 sequential impulse/PGS 的核心形式。",
           ["Rigid body backend boundary", "DynamicCollapse handoff data", "Future integration with UE Chaos"],
           ["C(q)=0", "J v + b = 0", "J M^-1 J^T lambda = -(Jv+b)", "lambda_min <= lambda <= lambda_max", "PGS row solve with clamping"],
           ["ConstraintRow{J, rhs, lo, hi}", "SolverIsland", "WarmStartCache", "ContactConstraint"],
           ["constraint rows must be frame-consistent", "bounds encode contact/friction behavior", "iterations are part of quality setting"],
           ["Implement one scalar constraint row", "Implement PGS loop", "Add normal contact bounds", "Add two tangent friction rows"],
           ["distance joint", "contact normal", "box stack toy", "friction block on plane"],
           ["solving position and velocity constraints with same units", "missing Baumgarte scaling", "friction lambda not tied to normal lambda"],
           ["single row analytic comparison", "lambda clamping", "stack stability smoke test"],
           "寫 ConstraintRow 與 PGS solver，讓兩個 1D rigid bodies 透過距離約束保持固定距離。", "FEM 虛功入口"),
    Lesson(9, "FEM 入口：虛功、能量與元素抽象", "Phase C - 結構 FEM 線性核心",
           "把連續體力學的弱式轉成元素剛度、元素荷載與全域組裝接口。",
           ["IElement.h", "ElementFactory.h", "BeamColumnElement.cpp", "MITC4ShellElement.cpp"],
           ["delta W_int = delta W_ext", "U = 1/2 q^T K q", "f_int = partial U / partial q", "K = partial f_int / partial q", "K = integral B^T D B dOmega"],
           ["IElement", "ElementDofMap", "LocalStiffness", "EquivalentLoad", "RecoverResult"],
           ["element owns local mathematics", "solver owns assembly and boundary conditions", "recover uses the same convention as stiffness"],
           ["Define IElement prepare/assemble/recover", "Implement axial bar as first element", "Separate local and global element data"],
           ["one-bar spring", "two-element chain", "energy-derived stiffness", "force recovery"],
           ["putting boundary conditions inside element", "changing DOF order per element", "recovering forces in a different sign convention"],
           ["energy derivative finite difference", "symmetric K", "rigid-body modes where expected"],
           "寫一個 AxialBarElement，使用虛功/能量推導 K，並接到簡化 assembler。", "3D Euler-Bernoulli beam"),
    Lesson(10, "3D Euler-Bernoulli beam-column 局部剛度", "Phase C - 結構 FEM 線性核心",
           "完整拆解 FrameCore 的 localStiffness12：axial、torsion、兩個 bending plane。",
           ["ElementStiffness.cpp localStiffness12", "Section.h", "BeamColumnElement.cpp"],
           ["EA/L", "GJ/L", "12EI/L^3", "6EI/L^2", "4EI/L and 2EI/L"],
           ["Mat12", "Section{A,Iy,Iz,J}", "Material{E,G}", "BeamColumnElement::kl_"],
           ["local DOF order is fixed", "Iy pairs with local z deflection w/ry", "Iz pairs with local y deflection v/rz"],
           ["Build axial block", "Build torsion block", "Build z-bending block", "Build y-bending block with sign flips"],
           ["cantilever tip deflection", "torsion shaft twist", "fixed-fixed end rotations", "symmetry of K"],
           ["swapping Iy and Iz", "wrong sign in y-plane block", "using L^2 where L^3 belongs"],
           ["PL^3/3EI", "TL/GJ", "K symmetric", "rigid translations produce zero internal force"],
           "手寫 localStiffness12，不用 Eigen helper；用三個 closed-form fixture 驗證。", "全域座標與組裝"),
    Lesson(11, "局部到全域：K_g=T^T K_l T 與 sparse assembly", "Phase C - 結構 FEM 線性核心",
           "把元素局部矩陣轉回全域，並用 triplet scatter-add 組成 sparse K。",
           ["ElementStiffness.cpp transform12", "BeamColumnElement::assemble", "FrameSolver.cpp assembly"],
           ["q_l = T q_g", "f_g = T^T f_l", "K_g = T^T K_l T", "K = sum A_e^T K_e A_e", "energy invariance"],
           ["Triplet", "DofMap[12]", "SparseMatrix", "Assembler"],
           ["transform direction is an invariant, not a style choice", "assembly must add duplicate entries", "node id and node index are different"],
           ["Implement transformK", "Build dofs_[12]", "Scatter K_e into triplets", "Compress sparse matrix"],
           ["rotated cantilever", "two members sharing node", "duplicate triplets", "local/global roundtrip"],
           ["using TK_lT^T", "using NodeId as vector index", "dropping duplicate triplets"],
           ["q_l^T K_l q_l == q_g^T K_g q_g", "rotation equivariance", "same model reordered by ids"],
           "建立 sparse assembler，支援多根 beam 並用能量不變性抓 transform bug。", "邊界條件"),
    Lesson(12, "邊界條件、prescribed displacement 與 constrained solve", "Phase C - 結構 FEM 線性核心",
           "從完整 Kq=F 切成 free/constrained block，處理支承、settlement 與機構偵測。",
           ["FrameSolver.cpp free map/reduceFF/solveLoad", "Node.h", "SolveResult.h"],
           ["[Kff Kfc; Kcf Kcc][qf;qc]=[Ff;Fc]", "Kff qf = Ff - Kfc qc", "R=Kq-F", "pivotMargin=min|D|/max|D|", "qc=prescribed"],
           ["FreeMap", "ConstrainedDof", "PreparedSystem", "SolveResult{u,reactions,singular}"],
           ["fixed flag changes factorization", "prescribed value changes RHS only", "reactions are global"],
           ["Build free map", "Reduce Kff", "Move Kfc qc to RHS", "Backfill full q and reactions"],
           ["cantilever fixed root", "support settlement", "fully constrained model", "mechanism model"],
           ["dropping prescribed terms", "reporting internal forces on singular model", "reaction sign mismatch"],
           ["settlement vs OpenSees sp()", "near-zero pivot guard", "R=Kq-F check"],
           "實作 constrained solve；用支承沉陷測試證明 prescribed displacement 正確進 RHS。", "荷載與力回復"),
    Lesson(13, "荷載、fixed-end force、反力與 member force recovery", "Phase C - 結構 FEM 線性核心",
           "把 nodal load、UDL 與 element fixed-end force 放到同一個符號系統，並正確回復端力。",
           ["Load.h", "BeamColumnElement.cpp Qf_", "SolveResult.h", "FrameSolver.cpp reactions"],
           ["Q = K_l q_l + Qf", "p_eq = -T^T Qf", "R = Kq - F", "wL/2", "wL^2/12"],
           ["NodalLoad", "MemberUDL", "Vec12 Qf", "MemberForcePair"],
           ["UDL is local", "nodal load is global", "member end forces are local", "reactions are global"],
           ["Accumulate nodal loads", "Convert UDL to Qf", "Add equivalent nodal loads", "Recover Q after solve"],
           ["simply-supported UDL", "cantilever point load", "local y vs local z UDL", "reaction balance"],
           ["adding Qf instead of -Qf to F", "reporting end j axial with wrong sign", "mixing local/global loads"],
           ["sum reactions + loads = 0", "wL^2/8 moment", "local end force sign table"],
           "加入 MemberUDL，通過 simply-supported beam 的反力與最大彎矩 oracle。", "端釋放"),
    Lesson(14, "端釋放、Schur complement 與 hinge/truss 成員", "Phase C - 結構 FEM 線性核心",
           "把 released DOF 從元素層靜力凝聚出去，並同時處理剛度與 fixed-end force。",
           ["ElementStiffness.cpp condenseReleases", "Member.cpp makeRelease", "BeamColumnElement.cpp release handling"],
           ["K* = Krr - Krc Kcc^-1 Kcr", "Qf* = Qfr - Krc Kcc^-1 Qfc", "released force row = 0", "Schur complement", "rank(Kcc)=nc"],
           ["release[12]", "ReleasePreset", "CondensedElement", "FullPivLU rank guard"],
           ["condense K and Qf together", "singular released sub-block is a mechanism", "hinge releases bending rotations not translations"],
           ["Partition retained/released dofs", "Invert Kcc with rank check", "Apply dK and dQ", "Zero released rows/cols"],
           ["HingeJ on propped cantilever", "TrussPin rotations", "torsion both ends released", "loaded hinge phantom moment test"],
           ["only condensing K", "releasing axial DOF accidentally", "ignoring singular Kcc"],
           ["analytic propped cantilever", "free mechanism diagnostic", "fixed-end force condensation"],
           "實作 HingeI/HingeJ/TrussPin，並寫一個 loaded hinge 測試防止 phantom moment。", "質量與模態"),
    Lesson(15, "質量矩陣、模態分析與 mode shape", "Phase D - 動力、模態、穩定",
           "把 beam consistent mass 送入 generalized eigenproblem，建立 FrameCore 線性動力的基礎。",
           ["ElementStiffness.cpp localMass12", "ModalAnalysis.cpp", "ModalResult.h"],
           ["M local consistent mass", "K phi = omega^2 M phi", "Phi^T M Phi = I", "f_n = omega_n/(2pi)", "rho kg/m^3 * 1e-12"],
           ["MassMatrix", "ModalResult{omega,mode}", "ModeNormalization", "M-orthonormal basis"],
           ["mass unit bridge is mandatory", "mode sign is arbitrary", "frequency comparison must be sign-insensitive"],
           ["Implement localMass12", "Assemble M", "Reduce Mff", "Solve generalized eigenproblem"],
           ["cantilever first mode", "axial bar frequency", "mode normalization", "massless material failure"],
           ["kg/m^3 vs tonne/mm^3", "comparing mode vector signs directly", "using lumped mass while expecting consistent oracle"],
           ["analytic beam omega", "OpenSees eigen comparison", "Phi^T M Phi identity"],
           "為 clone solver 加入 consistent mass，求出 cantilever 第一頻率並與解析值比較。", "Newmark transient"),
    Lesson(16, "Newmark 與 modal transient", "Phase D - 動力、模態、穩定",
           "用模態疊加或完整系統 Newmark 解線性暫態，為 dynamic collapse 事件間積分做準備。",
           ["ModalDynamics.cpp", "DynamicCollapse.cpp", "docs/PROGRESS_S2.md"],
           ["M qdd + C qd + K q = F(t)", "q = Phi eta", "eta_dd + 2zeta omega eta_d + omega^2 eta = p(t)", "K_eff = K + a0 M + a1 C", "average acceleration is unconditionally stable for linear systems"],
           ["TransientState", "ModalCoordinate", "NewmarkCoefficients", "TimeHistoryFrame"],
           ["basis truncation creates residual", "damping model must be explicit", "event changes require projection"],
           ["Implement SDOF Newmark", "Lift to modal coordinates", "Store replay frames", "Report truncation residual"],
           ["step load oscillator", "impulse-like load", "modal full basis equivalence", "quiescence terminal"],
           ["forgetting initial acceleration", "using wrong gamma/beta constants", "not projecting velocity at events"],
           ["analytic SDOF", "full basis == direct", "energy decay under damping"],
           "建立 modal transient toy solver，讓一個 SDOF 在 step load 下與解析解比對。", "穩定與 P-Delta"),
    Lesson(17, "幾何剛度、線性屈曲與 P-Delta", "Phase D - 動力、模態、穩定",
           "從軸力導出 stress stiffening，處理 Euler buckling 與 Theory-II P-Delta。",
           ["ElementStiffness.cpp localGeometric12", "BucklingAnalysis.cpp", "PDeltaAnalysis.cpp", "docs/PROGRESS_S3.md"],
           ["K_T = K_e + K_g(P)", "P tension-positive", "P=-N for compression-positive result", "(-K_g) phi = lambda K phi", "Pcr = pi^2 EI/L^2"],
           ["GeometricStiffness", "BucklingResult", "PDeltaOptions", "DivergenceDetector"],
           ["compression softens bending", "P=0 must be bit-identical to linear", "past Pcr returns diverged"],
           ["Implement localGeometric12", "Build buckling eigenproblem", "Implement frozen pseudo-load P-Delta", "Add divergence criteria"],
           ["pinned column", "cantilever column", "P/Pcr sweep", "P=0 no-op"],
           ["wrong sign for compression", "silently returning post-critical displacement", "mixing first-order and tangent axial force"],
           ["Euler load", "KT reference solve", "P=0 bit identity", "1.05Pcr diverged"],
           "加入 localGeometric12，掃描單柱壓縮力，找出最接近 singular 的 P。", "Timoshenko beam"),
    Lesson(18, "Timoshenko beam、剪切柔度與 locking 觀念", "Phase D - 動力、模態、穩定",
           "在 Euler-Bernoulli 之外加入剪切變形，理解為什麼厚梁需要 Asy/Asz。",
           ["ElementStiffness.cpp localStiffness12T", "Section.h Asy/Asz", "SolveOptions.h useTimoshenko"],
           ["Phi = 12 E I/(G A_s L^2)", "k terms divided by 1+Phi", "delta = PL^3/(3EI) + PL/(G A_s)", "Phi -> 0 gives EB", "shear area pairs with bending plane"],
           ["Section{Asy,Asz}", "SolveOptions.useTimoshenko", "ShearFlexibleBeam"],
           ["Timoshenko is opt-in", "missing shear areas means EB path", "slender limit must match EB"],
           ["Implement Phiz/Phiy", "Modify bending coefficients", "Keep axial/torsion unchanged", "Add off-path bit identity test"],
           ["deep beam cantilever", "slender beam limit", "Asy=0 fallback", "two bending planes"],
           ["pairing Asy with Iy instead of Iz", "not guarding zero shear area", "changing default EB result"],
           ["PL^3/3EI + PL/GA_s", "Phi->0 convergence", "default-off no-op"],
           "把 localStiffness12T 加入 clone solver，做厚梁與細梁兩個對照測試。", "shell 基礎"),
    Lesson(19, "Plate/Shell 基礎：膜、板彎、剪力、drilling", "Phase E - 殼元素與高階元素",
           "建立 shell 元素的物理分解：membrane、bending、transverse shear 與 drilling rotation。",
           ["Shell.h", "MITC4ShellElement.h", "MITC4ShellElement.cpp"],
           ["membrane strain epsilon=[u_x, v_y, u_y+v_x]", "plate curvature kappa=[theta_x,x, theta_y,y, ...]", "Qx,Qy transverse shear", "plane stress D=E/(1-nu^2)", "24 DOF = 4 nodes * 6"],
           ["ShellQuad{n[4],matIdx,t}", "ShellLocalFrame", "GaussPoint", "ShellElementForces"],
           ["shell is a flat facet", "corner order must be CCW about normal", "nu in [0,0.5)"],
           ["Build quad local frame", "Compute bilinear shape functions", "Compute Jacobian", "Separate membrane/bending/shear blocks"],
           ["flat square plate", "warped quad rejection", "constant membrane strain", "rigid body rotation"],
           ["inverted Jacobian", "using center moment as design peak", "forgetting drilling DOF stabilization"],
           ["patch test", "zero rigid modes", "OpenSees ShellMITC4 flat comparison"],
           "先實作 Q4 membrane element，通過 constant strain patch，再規劃 bending/shear 加入點。", "MITC4"),
    Lesson(20, "MITC4 Reissner-Mindlin shell", "Phase E - 殼元素與高階元素",
           "深入 MITC4 assumed natural shear、2x2 Gauss integration 與 Hughes-Brezzi drilling penalty。",
           ["MITC4ShellElement.cpp", "docs/ARCHITECTURE.md MITC4 section", "docs/VERIFICATION.md F13-F16"],
           ["K = integral B^T D B dA", "gamma_xz, gamma_yz at tying points", "Db = E t^3/(12(1-nu^2))", "Dm = E t/(1-nu^2)", "drilling penalty alpha = G t"],
           ["MITC4ShellElement", "TyingPointShear", "ShellGaussData", "DrillingPenalty"],
           ["MITC shear defeats thin-limit locking", "drilling stiffness must not pollute patch test", "facet transform mirrors beam transform"],
           ["Implement shape functions", "Add membrane block", "Add bending block", "Add assumed shear", "Add drilling penalty"],
           ["clamped plate", "Scordelis-Lo roof", "pinched cylinder", "tilted plate invariance"],
           ["full integration shear locking", "over-large drilling penalty", "incorrect local frame normal"],
           ["Kirchhoff plate coefficient", "OpenSees ShellMITC4", "rigid rotation invariance"],
           "用 2x2 Gauss 寫一個簡化 MITC4 skeleton，至少輸出 membrane patch test。", "shell stress recovery"),
    Lesson(21, "Shell stress recovery 與 failure screen", "Phase E - 殼元素與高階元素",
           "把 shell 中心/角點 resultants 回復成可視化與 elastic failure screen 資料。",
           ["SolveResult.h ShellElementForces", "ShellFailureTest.cpp", "ElasticAllowable shell VM notes"],
           ["Nxx,Nyy,Nxy membrane force per width", "Mxx,Myy,Mxy moment per width", "sigma_top = N/t + 6M/t^2", "sigma_bottom = N/t - 6M/t^2", "von Mises plane stress"],
           ["ShellElementForces", "CornerMomentRecovery", "ShellDemand", "FailureMode"],
           ["center value is not design peak", "resultants live in shell local frame", "DKQ corner fields have different meaning"],
           ["Recover center resultants", "Recover corner bending values", "Compute surface stresses", "Compute von Mises risk"],
           ["constant curvature patch", "pressure plate", "membrane tension panel", "corner peak comparison"],
           ["mixing force/width with total force", "using center moment as peak", "wrong top/bottom sign"],
           ["hand VM values", "surface stress symmetry", "combine/envelope linear fields"],
           "寫 shellDemandAtSurface，輸入 N/M/t/cap.vm，輸出 top/bottom von Mises D/C。", "co-rotational beam"),
    Lesson(22, "Co-rotational beam：小應變、大旋轉", "Phase F - 非線性與倒塌",
           "把元素剛體運動拆出去，在局部 frame 中計算小應變 beam deformation。",
           ["CorotationalAnalysis.cpp", "CorotationalAnalysis.h", "docs/PROGRESS_S9.md"],
           ["current chord defines corot frame", "u_local = remove rigid motion", "SO(3) finite rotation composition", "residual = f_int - lambda f_ext", "K_t from finite difference or analytic tangent"],
           ["CorotNodeState", "CorotElementState", "RotationVector", "NonlinearResult"],
           ["rigid motion must produce zero strain", "small-displacement limit must match linear/P-Delta", "finite rotations are not linear Rx/Ry/Rz addition"],
           ["Build current local frame", "Extract deformational DOF", "Compute internal force", "Newton iterate residual"],
           ["large cantilever elastica", "rigid rotation", "pure torsion", "small displacement comparison"],
           ["counting rigid rotation as strain", "Euler angle singularity", "not updating tangent after geometry change"],
           ["Mattiasson elastica", "OpenSees Corotational", "rigid rotation residual zero"],
           "做一個 2D planar corotational beam toy，先驗證 rigid rotation 不產生內力。", "Newton/arc-length"),
    Lesson(23, "Newton-Raphson、非線性 residual 與 arc-length", "Phase F - 非線性與倒塌",
           "建立非線性求解器的主迴圈、收斂判據、切線剛度與 snap-through path following。",
           ["CorotationalAnalysis.cpp arc-length", "docs/PROGRESS_S9c.md", "docs/specs/S9c_arclength.md"],
           ["R(u,lambda)=f_int(u)-lambda f_ref", "K_t delta u = -R", "arc: ||delta u||^2 + alpha delta lambda^2 = ds^2", "predictor-corrector", "load control fails past limit point"],
           ["NonlinearOptions", "NewtonState", "ArcLengthState", "ConvergenceReport"],
           ["residual norm and increment norm are different", "divergence is a valid result", "arc-length follows a path, not a load target"],
           ["Implement load-control Newton", "Add line search or step split", "Add cylindrical arc-length", "Record iteration history"],
           ["scalar snap-through equation", "shallow arch", "limit point", "failed load-control case"],
           ["using stale tangent forever", "accepting NaN iterations", "wrong sign branch switch"],
           ["finite-difference tangent check", "OpenSees ArcLength limit load", "known scalar nonlinear root"],
           "實作一個 scalar arc-length solver，追蹤 f(u,lambda)=u^3-u+lambda 的 limit point。", "tension-only"),
    Lesson(24, "Tension-only active set", "Phase F - 非線性與倒塌",
           "把只受拉構件轉成 active-set 問題，處理壓縮退出、拉伸回復與循環保護。",
           ["TensionOnly.cpp", "Member.tensionOnly", "docs/PROGRESS_S4.md"],
           ["if N_compression>0 deactivate", "elongation = (u_j-u_i) dot xhat", "reactivate if elongation>0", "fixed point active set", "cycle -> monotone fallback"],
           ["TensionOnlyState", "ActiveMask", "TransitionHash", "ReSolveSession inner solve"],
           ["plain solve ignores tensionOnly", "driver owns active-set semantics", "finite termination requires guard"],
           ["Iterate solve/screen/update", "Hash active transitions", "Fallback deactivate-only", "Report iteration count"],
           ["X brace under lateral load", "slack cable", "reactivated cable", "cycle toy"],
           ["checking force sign after member deactivated", "no cycle detection", "stale factor after active mask change"],
           ["converged == omit slack member", "iteration bound", "reuse fingerprint includes tensionOnly"],
           "寫 X-brace active-set 測試，輸出每輪 active mask 與 governing axial sign。", "progressive collapse"),
    Lesson(25, "Progressive collapse、塑性鉸與 N-M interaction", "Phase F - 非線性與倒塌",
           "把 collapse driver 做成事件序列：solve、screen、remove/yield、re-solve。",
           ["Collapse.cpp", "Hinge.h", "NMInteraction.h", "docs/PROGRESS_S10.md"],
           ["utilization > threshold triggers event", "hinge releases bending DOF", "Mp=fy Z", "Mp_eff=Mp max(0,1-N/Ny)^2", "w*=16Mp/L^2"],
           ["CollapseStep", "PlasticHinge", "CollapseOptions", "ElementRemoval", "Connectivity cleanup"],
           ["collapse driver is sequential linear", "hinge is event-to-event", "N-M interaction is uniaxial reduction"],
           ["Screen worst utilization", "Apply deterministic tie-break", "Insert hinge or deactivate member", "Pin detached debris nodes"],
           ["plastic beam collapse", "single-member removal", "N-M on/off comparison", "stable vs collapsed terminal"],
           ["pretending this is fiber pushover", "non-deterministic tie break", "not removing debris loads"],
           ["OpenSees hinge state", "0.99/1.01 collapse bracket", "default-off bit identity"],
           "建立一個 two-span beam collapse toy，先支援 brittle removal，再加入 plastic hinge event。", "dynamic collapse"),
    Lesson(26, "Dynamic collapse 與 FragmentCluster handoff", "Phase F - 非線性與倒塌",
           "在線性動力事件間積分，事件後投影到新模態基底，並把脫離碎片交給剛體後端。",
           ["DynamicCollapse.cpp", "FragmentMomentum.h", "Connectivity.cpp", "docs/PROGRESS_S2.md"],
           ["q'=Phi'^T M' u", "qdot'=Phi'^T M' v", "p=sum m_i v_i", "L=sum r_i x m_i v_i", "omega=I^-1 L"],
           ["DynamicCollapseState", "ReplayFrame", "FragmentCluster", "ModalProjection"],
           ["event changes topology and basis", "fragment velocity comes from structural velocity field", "rigid fall is outside FrameCore"],
           ["Integrate modal step", "Detect event", "Rebuild basis", "Project u/v", "Compute fragment momentum"],
           ["axial member removal", "falling detached beam", "symmetry case", "full basis equivalence"],
           ["dropping velocity at event", "using static collapse for momentum", "wrong inertia origin"],
           ["momentum closure", "full-system Newmark audit", "fragment mass/com hand values"],
           "寫一個事件前後 modal projection toy，確認 u/v 在新 basis 下保持一致。", "ReSolve"),
    Lesson(27, "ReSolve ladder：Woodbury、stale PCG、rebaseline", "Phase G - 再分析、最佳化、產品化",
           "把互動式編輯的重算成本拆成三層：公式更新、近似迭代、完整重建。",
           ["Reanalysis.cpp", "PreparedSystemImpl.h", "ModelHash.h", "docs/PROGRESS_S1.md"],
           ["(A+UCV)^-1 = A^-1 - A^-1 U(C^-1+V A^-1 U)^-1 V A^-1", "PCG with stale LDLT preconditioner", "Tier3 fresh factor", "fingerprint guards reuse", "restore drift"],
           ["ReSolveSession", "RankUpdate", "PCGState", "ModelFingerprint"],
           ["Tier1 formula-exact not bit-identical", "Tier2 tolerance-grade", "Tier3 correctness anchor"],
           ["Capture baseline factor", "Build rank-k delta", "Attempt Woodbury", "Fallback PCG", "Fallback rebaseline"],
           ["member stiffness tweak", "shell facet removal", "restore original", "mechanism after update"],
           ["using update after topology incompatible change", "hiding PCG residual", "not preserving mechanism detection"],
           ["fresh factor reference", "restore drift", "mechanism guard"],
           "實作 rank-1 Woodbury solve for tiny dense SPD matrix，與 fresh inverse 比對。", "稀疏 direct solver"),
    Lesson(28, "稀疏求解器與 supernodal direct lane", "Phase G - 再分析、最佳化、產品化",
           "理解 SimplicialLDLT 預設路徑與 opt-in supernodal Cholesky 的工程邊界。",
           ["SnSolver.cpp", "SnSession.cpp", "FrameSnChol.h", "docs/PROGRESS_R_supernodal.md"],
           ["fill-in depends on ordering", "LL^T or LDL^T", "supernode groups adjacent columns", "BLAS3 dense panels", "fallback to LDLT on mechanism"],
           ["SparsePattern", "Ordering", "SnSession", "FactorizationResult"],
           ["default solver remains LDLT", "supernodal is explicit opt-in", "correctness checked vs LDLT, not residual only"],
           ["Analyze sparsity pattern", "Apply ordering", "Factor panels", "Solve many RHS", "Fallback on not-SPD"],
           ["frame grid", "settlement solve-many", "mechanism fallback", "large DOF benchmark"],
           ["changing default path silently", "trusting residual under high condition", "not guarding missing BLAS/METIS"],
           ["vs LDLT rel < tolerance", "disabled bit-exact", "mechanism diagnostic"],
           "建立一個小 SPD sparse matrix，試三種 ordering，觀察 fill-in 數量差異。", "FSD 尺寸最佳化"),
    Lesson(29, "FSD 尺寸最佳化與多載重 envelope", "Phase G - 再分析、最佳化、產品化",
           "把 strength screen 包成尺寸迭代，理解 FSD 的適用範圍與限制。",
           ["SizeOpt.cpp", "ElasticAllowable.cpp", "Combination.cpp", "docs/PROGRESS_S5.md"],
           ["risk = demand/capacity", "A_{k+1}=A_k risk^eta", "multi-load envelope max risk", "similar section scaling", "discrete table round-up"],
           ["SizeOptOptions", "SectionScale", "EnvelopeDemand", "OscillationGuard"],
           ["FSD is heuristic for indeterminate frames", "pin-joint literature differs from frame bending", "mechanism guard required after resizing"],
           ["Evaluate load cases", "Compute risk per member", "Scale section", "Round to table", "Detect oscillation"],
           ["10-bar truss", "fixed frame heavier than pin-joint", "multi-load envelope", "discrete table"],
           ["claiming global optimum", "shrinking below stiffness needs", "ignoring load combinations"],
           ["10-bar weight reference", "risk near 1", "no singular resized model"],
           "寫一輪 FSD 更新公式，用三根桿 toy model 驗證 risk 大的桿會放大截面。", "BESO 拓樸"),
    Lesson(30, "BESO 拓樸最佳化與 strain-energy sensitivity", "Phase G - 再分析、最佳化、產品化",
           "把 element active flag 變成拓樸設計變數，使用 strain energy 排序做 hard-kill。",
           ["Topology.cpp", "Member.active", "docs/PROGRESS_S7.md"],
           ["U_e = 1/2 u_e^T K_e u_e", "sensitivity = strain energy", "volume fraction target", "history averaging", "candidate must re-solve non-singular"],
           ["TopologyState", "ElementSensitivity", "ActiveSet", "ComplianceHistory"],
           ["BESO is heuristic", "hard-kill can create mechanism", "best feasible fallback is required"],
           ["Compute element energy", "Rank sensitivities", "Deactivate low sensitivity", "Re-solve and guard", "Apply history averaging"],
           ["ground structure", "single removal survival", "compliance increase", "N2 robustness screen"],
           ["removing supports", "not checking mechanism", "interpreting UDL energy as exact without caveat"],
           ["energy balance", "survives re-solve", "constraint topology vs unconstrained"],
           "建立 5-member ground structure，依 strain energy 刪 1 根並檢查是否 singular。", "API / CLI"),
    Lesson(31, "Public API、CLI、C API 與 Grasshopper bridge", "Phase G - 再分析、最佳化、產品化",
           "把 engine core 包成穩定 ABI/文字協定，使 UE、CLI、Grasshopper 都走同一條可信路徑。",
           ["FrameSolver.h", "Standalone/frame_cli_core.cpp", "frame_capi.cpp", "Grasshopper/FrameCoreClient.cs", "docs/CLI_PROTOCOL.md"],
           ["POD boundary", "text protocol is an ABI", "daemon amortizes startup", "C API returns explicit error", "id-stamped result rows"],
           ["FrameModel public API", "TextCommand", "CapiBuffer", "DaemonSession", "ClientAdapter"],
           ["public headers expose no Eigen", "wire protocol must be versioned", "errors propagate as diagnostics"],
           ["Parse MAT/SEC/NODE/MEMBER/NLOAD", "Call solve", "Serialize DISP/FORCE/REACTION", "Wrap in C API"],
           ["CLI cantilever", "daemon multiple requests", "ctypes call", "Grasshopper client round-trip"],
           ["leaking Eigen type", "implicit unit conversion", "not stamping inactive element ids"],
           ["CLI == core", "C API == CLI", "daemon == independent runs"],
           "實作最小 text protocol，讓 stdin 描述一根 cantilever 並輸出 tip displacement。", "Capstone"),
    Lesson(32, "Capstone：MiniFrameCore 複製計畫", "Phase G - 再分析、最佳化、產品化",
           "把前 31 課收斂成一個可測、可擴、可替換後端的 MiniFrameCore。",
           ["All FrameCore public headers", "Standalone/main.cpp", "docs/VERIFICATION.md", "Scripts/run_gate.ps1"],
           ["Kq=F", "M qdd + C qd + Kq=F(t)", "K_T=K+K_G", "q_l=Tq_g", "R=Kq-F"],
           ["MiniFrameModel", "MiniElement", "MiniAssembler", "MiniSolver", "MiniVerificationGate"],
           ["source code is final fact", "every capability needs oracle", "default path must stay stable"],
           ["Milestone 1 vector/frame", "Milestone 2 beam K", "Milestone 3 assembly solve", "Milestone 4 loads/releases", "Milestone 5 modal/stability"],
           ["cantilever", "simply-supported UDL", "release", "modal", "buckling", "CLI round-trip"],
           ["adding features before invariants", "refactoring without gate", "claiming capabilities without oracle"],
           ["ALL PASS local gate", "independent dense solver check", "rotation equivariance", "bit-identity no-op proofs"],
           "建立 MiniFrameCore repo skeleton，列出每個 milestone 的 API、公式、測試與 rollback 方法。", "回到專案源碼深讀"),
]


def slug(s: str) -> str:
    x = re.sub(r"[^\w\u4e00-\u9fff]+", "_", s.lower()).strip("_")
    return x[:60]


def formula_block(formulas: list[str]) -> str:
    out = []
    for f in formulas:
        out += ["\\[", f, "\\]", ""]
    return "\n".join(out)


def bullet(items: list[str]) -> str:
    return "\n".join(f"- {x}" for x in items)


def code_skeleton(lesson: Lesson) -> str:
    class_name = re.sub(r"[^A-Za-z0-9]", "", lesson.title.split("：")[0].title()) or f"Lesson{lesson.no}"
    return f"""```cpp
// Lesson {lesson.no}: {lesson.title}
// Minimal C++17 skeleton. Keep it POD-first; do not leak Eigen/UE types here.

namespace mini {{

struct Lesson{lesson.no:02d}Context {{
    // Inputs are explicit. No hidden global unit conversion.
    double tolerance = 1.0e-9;
}};

struct Lesson{lesson.no:02d}Result {{
    bool ok = false;
    const char* diagnostic = "";
}};

inline Lesson{lesson.no:02d}Result runLesson{lesson.no:02d}Check(const Lesson{lesson.no:02d}Context& ctx) {{
    Lesson{lesson.no:02d}Result r;

    // TODO 1: encode the mathematical invariant for this lesson.
    // TODO 2: build the smallest data structure that can carry the invariant.
    // TODO 3: compare against an independent hand-computed oracle.
    // TODO 4: return diagnostic instead of silently producing nonsense.

    r.ok = ctx.tolerance > 0.0;
    r.diagnostic = r.ok ? "pass" : "invalid tolerance";
    return r;
}}

}} // namespace mini
```"""


def expanded_source_reading(lesson: Lesson) -> str:
    out = ["## A. 源碼閱讀卡片\n"]
    for idx, src in enumerate(lesson.source, 1):
        out += [
            f"### A.{idx} `{src}`",
            "",
            "閱讀時不要只找函式名稱。要回答四個問題：",
            "",
            "1. 這個檔案位於 public API 還是 private backend？",
            "2. 它承載的是資料、演算法、還是驗證？",
            "3. 它依賴哪個 convention？",
            "4. 它失敗時應該回傳 diagnostic、assert、還是測試失敗？",
            "",
            "Freeform 卡片建議：",
            "",
            "```text",
            f"[{src}]",
            "    input     -> ?",
            "    output    -> ?",
            "    invariant -> ?",
            "    oracle    -> ?",
            "```",
            "",
        ]
    return "\n".join(out)


def expanded_formula_notes(lesson: Lesson) -> str:
    out = ["## B. 公式逐步拆解卡片\n"]
    for idx, f in enumerate(lesson.math, 1):
        out += [
            f"### B.{idx} 公式：`{f}`",
            "",
            "#### Step 1 - 先写数学对象",
            "",
            "\\[",
            f"{f}",
            "\\]",
            "",
            "不要直接写代码。先确认它描述的是标量、向量、矩阵、线性算子，还是约束列。",
            "",
            "#### Step 2 - 写出输入与输出",
            "",
            "```text",
            "input  : 本公式需要的最小物理量",
            "output : 本公式产生的数值对象",
            "frame  : global / local / material / body",
            "unit   : N, mm, MPa, tonne, rad",
            "```",
            "",
            "#### Step 3 - 单位检查",
            "",
            "把每个符号都替换成单位。如果单位不收敛，先修公式，不要修代码。",
            "",
            "```text",
            "left hand side unit  = ?",
            "right hand side unit = ?",
            "must match           = true",
            "```",
            "",
            "#### Step 4 - 程式映射",
            "",
            "```cpp",
            "// Pseudocode mapping for this formula",
            "auto input = read_model_or_state();",
            "auto value = compute_formula(input);",
            "assert(isfinite(value));",
            "```",
            "",
            "#### Step 5 - Oracle",
            "",
            "为这个公式建立最小测试：",
            "",
            "- 一个手算数值。",
            "- 一个极限情况。",
            "- 一个错误符号会失败的反例。",
            "",
        ]
    return "\n".join(out)


def expanded_data_design(lesson: Lesson) -> str:
    out = ["## C. 资料结构设计卡片\n"]
    for idx, item in enumerate(lesson.data, 1):
        typename = re.sub(r"[^A-Za-z0-9]", "", item.title()) or f"Data{idx}"
        out += [
            f"### C.{idx} `{item}`",
            "",
            "这个资料结构必须回答：它保存 reference configuration、current state、solver cache，还是 result？",
            "",
            "```cpp",
            f"struct {typename} {{",
            "    // Write only fields that are necessary for this lesson.",
            "    // Do not store derived values unless caching is justified.",
            "    bool valid = false;",
            "};",
            "```",
            "",
            "字段设计检查：",
            "",
            "- 是否需要 id？",
            "- 是否需要 index？",
            "- 是否需要单位注释？",
            "- 是否应放在 public header？",
            "- 是否会被 solver cache fingerprint 使用？",
            "",
        ]
    return "\n".join(out)


def expanded_commit_plan(lesson: Lesson) -> str:
    out = [
        "## D. 建议实现顺序：可提交 milestone",
        "",
        "每一步都应该能独立编译、独立测试。不要一次写完再 debug。",
        "",
    ]
    for idx, item in enumerate(lesson.code, 1):
        out += [
            f"### D.{idx} Milestone - {item}",
            "",
            "交付物：",
            "",
            "```text",
            "source file      :",
            "public API       :",
            "private backend  :",
            "unit test        :",
            "expected failure :",
            "```",
            "",
            "验收条件：",
            "",
            "- 编译通过。",
            "- 至少一个正向案例。",
            "- 至少一个反向案例。",
            "- diagnostic 不为空。",
            "",
        ]
    return "\n".join(out)


def expanded_examples(lesson: Lesson) -> str:
    out = ["## E. 手算例题展开模板\n"]
    for idx, ex in enumerate(lesson.examples, 1):
        out += [
            f"### E.{idx} 例题 - {ex}",
            "",
            "#### Given",
            "",
            "```text",
            "geometry/material/state/load = fill by hand",
            "unit system = N, mm, MPa unless explicitly stated",
            "frame = local or global",
            "```",
            "",
            "#### Derivation",
            "",
            "\\[",
            "\\text{definition} \\rightarrow \\text{substitution} \\rightarrow \\text{numeric result}",
            "\\]",
            "",
            "#### Implementation check",
            "",
            "```cpp",
            "// The test should compute the same scalar/vector/matrix.",
            "// Do not compare formatted strings. Compare numeric tolerance.",
            "```",
            "",
            "#### Failure case",
            "",
            "刻意翻转一个符号、交换一个 index、或切换 local/global frame，测试必须失败。",
            "",
        ]
    return "\n".join(out)


def expanded_debug(lesson: Lesson) -> str:
    out = ["## F. Debug 分页清单\n"]
    for idx, pitfall in enumerate(lesson.pitfalls, 1):
        out += [
            f"### F.{idx} 常见错误 - {pitfall}",
            "",
            "症状：",
            "",
            "```text",
            "displacement wrong / force sign wrong / matrix nonsymmetric / singular unexpectedly",
            "```",
            "",
            "定位顺序：",
            "",
            "1. 打印输入单位。",
            "2. 打印 local/global frame。",
            "3. 打印 DOF map。",
            "4. 打印最小矩阵 block。",
            "5. 比对手算 oracle。",
            "",
            "修复原则：",
            "",
            "- 不用调 tolerance 掩盖符号错误。",
            "- 不改测试迁就错误实现。",
            "- 先缩小到单元素模型。",
            "",
        ]
    return "\n".join(out)


def expanded_test_matrix(lesson: Lesson) -> str:
    out = ["## G. Test Matrix\n", "| Test | Purpose | Oracle | Expected failure |", "|---|---|---|---|"]
    for idx, t in enumerate(lesson.tests, 1):
        out.append(f"| G{idx} `{t}` | protect lesson invariant | hand / dense / external | wrong convention must fail |")
    out += [
        "",
        "### G.x 最小测试程式骨架",
        "",
        "```cpp",
        "static void assertNear(double a, double b, double eps) {",
        "    assert(std::abs(a - b) <= eps * std::max(1.0, std::abs(a)));",
        "}",
        "",
        "static void test_expected_failure_path() {",
        "    // Build a deliberately wrong input or wrong convention.",
        "    // The test must fail before the bug reaches a larger model.",
        "}",
        "```",
        "",
    ]
    return "\n".join(out)


def expanded_freeform_layout(lesson: Lesson) -> str:
    return f"""## H. Freeform 大畫布排版建議

每一課匯入 Freeform 後，建議拆成 6 個大區塊：

```text
┌──────────────────────────────┐
│ Lesson {lesson.no:02d}: {lesson.title}
├──────────────┬───────────────┤
│ 數學推導      │ 資料結構        │
├──────────────┼───────────────┤
│ C++ 實作      │ 測試 / oracle   │
├──────────────┴───────────────┤
│ Debug checklist + 課後練習     │
└──────────────────────────────┘
```

視覺規則：

- 中央只放本課核心公式。
- 左側放手算推導。
- 右側放 C++ struct / function。
- 下方放測試矩陣。
- 最外圈放錯誤案例與 debug checklist。

這樣你會得到「由中心慢慢擴散」的學習路徑，而不是一張裝飾圖。
"""


def expanded_appendix(lesson: Lesson) -> str:
    return f"""## I. 本課完整複製任務書

### I.1 Scope

本課只實作和 `{lesson.title}` 直接相關的最小功能。不得提前實作後面課程的 solver。

### I.2 Source References

{bullet(lesson.source)}

### I.3 Data Contract

{bullet(lesson.data)}

### I.4 Invariants

{bullet(lesson.invariants)}

### I.5 Rollback

如果本課實作導致後續測試混亂，rollback 方式是：

```text
1. 保留測試檔。
2. 移除本課新增實作。
3. 保留 public API 討論記錄。
4. 重新從最小 oracle 開始。
```

### I.6 Completion Definition

```text
[ ] 數學公式已手算
[ ] C++17 skeleton 已實作
[ ] 正向測試通過
[ ] 反向測試會失敗
[ ] Debug checklist 已跑過
[ ] Freeform 大畫布已整理
```
"""


def lesson_markdown(lesson: Lesson) -> str:
    return f"""# Lesson {lesson.no:02d} - {lesson.title}

> Phase: {lesson.phase}  
> Objective: {lesson.objective}  
> Style: 数学不变量先行 -> POD 资料结构 -> C++17 实作 -> oracle 验证。

---



## 0. 本课在整套课程的位置

这一课不是孤立知识点。它必须接在前面的不变量之后，并为后面的 solver 或分析模块提供一个可测试的接口。

你读本课时，只问三件事：

1. 数学对象是什么？
2. 资料结构如何承载它？
3. 哪一个 invariant 可以证明实现没有偏离？

---



## 1. 专案源码对齐

本课优先对齐下列 FrameCore 源码或文件：

{bullet(lesson.source)}

阅读顺序：

1. 先读 public header，确认 API 边界。
2. 再读 private implementation，确认数学如何落地。
3. 最后读 tests 或 verification map，确认 oracle。

---



## 2. 数学层：核心公式

以下公式不是装饰；每一个都必须能写成测试。

{formula_block(lesson.math)}

### 2.1 维度检查

写任何 solver 前，先做单位与维度检查：

```text
force      : N
length     : mm
stress     : N/mm^2 = MPa
rotation   : rad, dimensionless in linearized equations
stiffness  : translational N/mm, rotational N*mm
```

如果一个公式无法通过单位检查，不准进入实现。

### 2.2 从连续形式到离散形式

工程实现只处理离散资料结构。你必须能说明：

```text
continuous field / equation
        |
        v
finite-dimensional vector or matrix
        |
        v
POD data structure
        |
        v
testable invariant
```

---



## 3. 资料结构层

本课至少需要下列资料结构或概念：

{bullet(lesson.data)}

设计原则：

- public API 只放普通 C++ 型别。
- index 与 id 分离。
- 单位写进注释，不藏在命名习惯里。
- 可变状态和不可变模型资料分开。

### 3.1 最小资料流

```mermaid
flowchart TD
    A["输入: 模型/状态/参数"] --> B["建立数学对象"]
    B --> C["映射到 POD 资料结构"]
    C --> D["执行核心算法"]
    D --> E["检查 invariant"]
    E --> F["输出 result + diagnostic"]
```

---



## 4. 必守不变量

{bullet(lesson.invariants)}

把这些不变量写成测试，而不是写在 README 里相信自己。

### 4.1 Invariant template

```cpp
static void test_lesson_{lesson.no:02d}_invariant() {{
    // Arrange: build the smallest model that activates the formula.
    // Act: run the implementation.
    // Assert: compare against an independent oracle.
}}
```

---



## 5. C++17 实作骨架

{code_skeleton(lesson)}

### 5.1 实作输出物

本课结束时，你应该能提交：

{bullet(lesson.code)}

### 5.2 文件组织建议

```text
Lesson{lesson.no:02d}/
    include/
        Lesson{lesson.no:02d}Types.hpp
        Lesson{lesson.no:02d}Core.hpp
    tests/
        test_lesson{lesson.no:02d}.cpp
    CMakeLists.txt
```

---



## 6. 手算例题

本课至少手算以下例题：

{bullet(lesson.examples)}

### 6.1 手算格式

每题固定写成：

```text
Given:
    已知量、单位、坐标系

Derive:
    从定义开始，不跳步

Compute:
    代入数值

Check:
    单位、符号、极限情况
```

### 6.2 例题模板

\\[
\\text{{input}} \\rightarrow \\text{{mathematical object}} \\rightarrow \\text{{matrix/vector}} \\rightarrow \\text{{oracle}}
\\]

---



## 7. Debug Checklist

优先检查这些错误：

{bullet(lesson.pitfalls)}

### 7.1 Debug 顺序

1. 检查单位。
2. 检查 index。
3. 检查 local/global convention。
4. 检查符号。
5. 检查 invariant。
6. 再检查数值误差。

---



## 8. 验证关卡

本课必须至少通过以下测试：

{bullet(lesson.tests)}

### 8.1 Oracle 分类

优先级：

1. closed-form analytic solution
2. independent small dense implementation
3. external reference such as OpenSees
4. invariance test
5. regression test

不要用「同一段代码跑两次」当 oracle。

---



## 9. 课后硬核练习

### 题目 1：理论手算题

选择本课第 2 节的一个公式，给定具体数值，完整算到最后一行。要求写出单位。

<details>
<summary>解析方向</summary>

1. 先写定义。
2. 再代入数值。
3. 最后做单位检查。
4. 如果结果与直觉相反，先检查符号 convention。

</details>


### 题目 2：矩阵不变量题

构造一个最小矩阵或向量例子，验证本课 invariant。

<details>
<summary>解析方向</summary>

把 invariant 写成：

\\[
\\left|a-b\\right| < \\epsilon
\\]

或：

\\[
\\frac{{\\|a-b\\|}}{{\\max(1,\\|a\\|)}} < \\epsilon
\\]

不要只比较打印出来的小数位。

</details>


### 题目 3：核心程式码实作题

{lesson.exercise}

要求：

- C++17。
- 不使用高层物理库。
- public header 不暴露 Eigen/UE。
- 至少一个失败测试能证明错误实现会被抓到。

---



## 10. 本课必背

{bullet(lesson.invariants + lesson.math[:3])}

---



## 11. 下一课

下一课：**{lesson.next_title}**

你进入下一课前，应确认：

```text
[ ] 本课公式能手算
[ ] 本课资料结构能从零写出
[ ] 本课 invariant 有测试
[ ] 本课错误案例会失败
```

---



{expanded_source_reading(lesson)}

---



{expanded_formula_notes(lesson)}

---



{expanded_data_design(lesson)}

---



{expanded_commit_plan(lesson)}

---



{expanded_examples(lesson)}

---



{expanded_debug(lesson)}

---



{expanded_test_matrix(lesson)}

---



{expanded_freeform_layout(lesson)}

---



{expanded_appendix(lesson)}
"""


def build_readme(paths: list[Path]) -> str:
    rows = []
    for p in paths:
        title = p.stem.replace("_", " ")
        rows.append(f"- [{title}]({p.name})")
    return (
        "# FrameCore v2 深度課程包\n\n"
        "這一版不是白板摘要，而是按完整講義密度分課輸出。\n\n"
        "- [Freeform 匯入與排版指南](FREEFORM_IMPORT_GUIDE.md)\n\n"
        + "\n".join(rows)
        + "\n"
    )


def normalize_lesson1() -> str:
    if SAMPLE_LESSON1.exists():
        text = SAMPLE_LESSON1.read_text(encoding="utf-8")
        text = text.replace("architect--2.0.0 / Plugins / FrameSolver / Source / FrameCore",
                            "ArchSim / Plugins / FrameSolver / Source / FrameCore")
        text = text.replace("從上傳壓縮包可見，核心專案是：", "本工作區目前核心專案是：")
        text = text.replace("architect--2.0.0/", "ArchSim/")
        return text
    # Fallback if the user-provided file is unavailable.
    return lesson_markdown(Lesson(
        1, "向量、矩陣、局部座標與結構狀態表示", "Phase A - 數學與引擎最小地基",
        "建立所有後續 FEM 與物理求解器共享的座標、DOF 與矩陣 convention。",
        ["FrameTypes.h", "Node.h", "Member.h", "ElementStiffness.cpp"],
        ["gdof(i,d)=6i+d", "R=[ex^T;ey^T;ez^T]", "T=diag(R,R,R,R)", "K_g=T^T K_l T"],
        ["Vec3", "Mat3", "Node", "Member", "Transform12"],
        ["R R^T=I", "det(R)=1", "q_l^T K_l q_l=q_g^T K_g q_g"],
        ["Vec3 operations", "Mat3 rows", "memberLocalFrame", "transformK"],
        ["axis-aligned beam", "skew beam", "energy invariance"],
        ["wrong cross order", "R vs R^T", "NodeId vs index"],
        ["orthonormal frame", "roundtrip vector", "energy invariant"],
        "實作 Vec3/Mat3/memberLocalFrame/Transform12。", "運動學、狀態方程與 engine state 分層"))


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    paths: list[Path] = []

    p1 = OUT / "lesson_01_state_vectors_local_frames.md"
    p1.write_text(normalize_lesson1(), encoding="utf-8")
    paths.append(p1)

    for lesson in LESSONS:
        p = OUT / f"lesson_{lesson.no:02d}_{slug(lesson.title)}.md"
        p.write_text(lesson_markdown(lesson), encoding="utf-8")
        paths.append(p)

    (OUT / "README.md").write_text(build_readme(paths), encoding="utf-8")
    combined = ["# FrameCore v2 深度課程合併版\n"]
    for p in paths:
        combined.append(f"\n\n<!-- pagebreak: {p.name} -->\n\n")
        combined.append(p.read_text(encoding="utf-8"))
    (OUT / "framecore_v2_deep_course_combined.md").write_text("\n".join(combined), encoding="utf-8")

    print(OUT)
    print(len(paths))


if __name__ == "__main__":
    main()
