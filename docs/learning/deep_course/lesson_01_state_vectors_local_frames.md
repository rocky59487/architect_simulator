# 力學引擎開發與理論實作課程表

> 目標：從零內化並複製一個結構力學 / 物理模擬核心。  
> 對齊專案：`ArchSim / Plugins / FrameSolver / Source / FrameCore`。  
> 主要語言：C++17。  
> 核心風格：POD public API、底層資料結構明確、數學不變量先行、測試驅動。  
> 單位約定：N、mm、MPa，其中 MPa = N/mm²。  
> 本文件版本：Lesson 1 完整擴充版。

---



## 0. 如何閱讀這份課程

這不是「使用物理引擎」課程，而是「複製物理 / 力學引擎內核」課程。

本課程採用三層並行：

1. **數學層**  
   向量空間、矩陣、微分方程、能量、虛功、有限元素、約束與非線性求解。

2. **資料結構層**  
   `Vec3`、`Mat3`、`Node`、`Member`、`Element`、`SparseMatrix`、`Solver`、`PreparedSystem`。

3. **工程驗證層**  
   每一個公式都落成可測試的 invariant。  
   例如：

\[
R^TR=I
\]

\[
q_l^TK_lq_l=q_g^TK_gq_g
\]

\[
R_{reaction}=Ku-F
\]

\[
K\phi=\omega^2M\phi
\]

\[
K_T=K+K_G
\]

你要把每一課都視為一個可獨立 commit 的 engine milestone。

---



## 1. 專案對齊：FrameCore 的核心邊界

本工作區目前核心專案是：

```text
ArchSim/
    Plugins/
        FrameSolver/
            Source/
                FrameCore/
                    Public/FrameCore/
                    Private/
                    Private/Tests/
            Standalone/
            Grasshopper/
```

其中 `FrameCore` 的公共 API 使用普通 C++ / POD 型別，私有實作可使用 Eigen。這是很重要的工程邊界：

```text
外部世界 / UE / CLI / Grasshopper
        |
        v
Public POD API
        |
        v
Private numerical backend
        |
        v
Eigen / sparse solve / element implementation
```

本課程的複製策略：

```text
第一階段：完全不用 Eigen，手寫最小線代核心
第二階段：建立有限元素資料流
第三階段：接入 sparse backend
第四階段：擴充梁柱、殼、動力、非線性、倒塌、最佳化
```

---



## 2. 完整課程 Roadmap

### Phase A — 數學與引擎最小地基

| 課次 | 主題 | 理論核心 | 實作核心 | 本課完成後你應該能寫出的東西 |
|---:|---|---|---|---|
| 1 | 向量、矩陣、局部座標、節點 / 元素狀態 | \(\mathbb{R}^3\)、內積、外積、正交基、DOF 編碼、虛功不變性 | `Vec3`、`Mat3`、`Node`、`Member`、`memberLocalFrame()`、`T=diag(R,R,R,R)` | 最小幾何與 DOF 核心 |
| 2 | 點、剛體、結構節點的運動學 | 位置、速度、加速度、小轉角、角速度、\(SO(3)\) | `ParticleState`、`RigidBodyState`、`StructuralState` | 能清楚區分 particle / rigid body / structural node |
| 3 | 微分方程與數值積分 | \(\dot{x}=v\)、\(M\ddot{q}=f\)、Euler、Symplectic、RK4、Newmark | `Integrator` interface | 從 ODE 到 time stepping |
| 4 | 質量、慣性、力與力矩累積 | \(p=mv\)、\(L=I\omega\)、平行軸定理、力矩 \(\tau=r\times f\) | `ForceAccumulator`、`MassProperties` | 可推導剛體與結構動力質量模型 |



### Phase B — 一般物理引擎核心

| 課次 | 主題 | 理論核心 | 實作核心 | 交付物 |
|---:|---|---|---|---|
| 5 | 幾何 primitive 與碰撞檢測 I | sphere、plane、AABB、segment、triangle | overlap / distance tests | 幾何查詢庫 |
| 6 | 碰撞檢測 II：Broadphase / Narrowphase | Sweep-and-Prune、BVH、SAT、GJK 概念 | broadphase candidates、narrowphase contact | contact generation |
| 7 | 衝量響應 | 線動量、角動量、恢復係數、摩擦錐 | `ContactConstraint`、impulse solve | 單點接觸 solver |
| 8 | 約束求解器 | Lagrange multiplier、KKT、LCP、PGS、XPBD | constraint rows、Jacobian、sequential impulse | 可擴展 constraint solver |



### Phase C — 結構 FEM 線性核心

| 課次 | 主題 | 理論核心 | 實作核心 | 交付物 |
|---:|---|---|---|---|
| 9 | FEM 總入口：虛功與能量 | \(\delta W_{int}=\delta W_{ext}\)、\(\Pi=U-W\) | `IElement` interface | 所有元素共同界面 |
| 10 | 3D Euler–Bernoulli 梁柱元素 | 12 DOF、軸向、扭轉、雙向彎曲 | `localStiffness12()` | 梁柱局部剛度 |
| 11 | 局部到全域與稀疏組裝 | \(K_g=T^TK_lT\)、scatter-add | global DOF map、assembler | 全域剛度矩陣 |
| 12 | 支承、邊界條件、機構判定 | DOF 消去、prescribed displacement、pivot guard | constrained solve | 能穩定解 \(Kq=f\) |
| 13 | 荷載、等效節點力、反力、桿端力 | fixed-end force、\(R=Ku-F\) | `LoadCase`、member force recovery | 結構線性分析完整回路 |
| 14 | 端部釋放與靜力凝聚 | Schur complement、released DOF condensation | `condenseReleases()` | hinge / truss / pin member |



### Phase D — 動力、模態、穩定

| 課次 | 主題 | 理論核心 | 實作核心 | 交付物 |
|---:|---|---|---|---|
| 15 | 質量矩陣與模態分析 | consistent mass、\(K\phi=\omega^2M\phi\) | `localMass12()`、modal solver | 頻率 / mode shape |
| 16 | Newmark 與 modal transient | \(M\ddot{q}+C\dot{q}+Kq=f(t)\) | Newmark-\(\beta\)、modal superposition | 線性時間歷程 |
| 17 | 幾何剛度、P-Delta、線性屈曲 | \(K_T=K+K_G\)、Euler load | `localGeometric12()`、buckling eigen | 二階與屈曲 |
| 18 | Timoshenko 梁 | 剪切變形、\(\Phi=12EI/(GA_sL^2)\) | `localStiffness12T()` | 剪切柔性梁 |



### Phase E — 殼元素與高階元素

| 課次 | 主題 | 理論核心 | 實作核心 | 交付物 |
|---:|---|---|---|---|
| 19 | 2D / plate / shell 基礎 | membrane、bending、shear、drilling DOF | shell local coordinates | shell 元素準備 |
| 20 | MITC4 Reissner–Mindlin shell | assumed shear、locking、Jacobian、Gauss integration | `MITC4ShellElement` clone | 24 DOF shell |
| 21 | Shell stress recovery 與 failure screen | \(N_{xx},M_{xx},Q_x\)、von Mises | stress resultants | shell D/C screen |



### Phase F — 非線性與倒塌

| 課次 | 主題 | 理論核心 | 實作核心 | 交付物 |
|---:|---|---|---|---|
| 22 | co-rotational beam | 大位移、小應變、元素跟隨座標 | corotational transform | 幾何非線性梁 |
| 23 | Newton、切線剛度、弧長法 | residual、tangent、snap-through | arc-length continuation | 極限點追蹤 |
| 24 | tension-only active set | complementarity、active-set iteration | cable / brace deactivation | 拉力構件 |
| 25 | 塑性鉸與 event-to-event collapse | 塑性事件、機構、順序線性分析 | hinge driver | progressive collapse |
| 26 | dynamic collapse 與碎片交接 | modal Newmark、momentum handoff、fragment inertia | `FragmentCluster` | 動態倒塌橋接物理引擎 |



### Phase G — 再分析、最佳化、產品化

| 課次 | 主題 | 理論核心 | 實作核心 | 交付物 |
|---:|---|---|---|---|
| 27 | 再分析與互動式求解 | Woodbury、stale factor PCG、rebaseline | `PreparedSystem` / reanalysis ladder | 即時編輯求解 |
| 28 | 稀疏求解器與 supernodal lane | Cholesky、LDLT、ordering、fill-in | sparse backend seam | 大規模 DOF |
| 29 | 尺寸最佳化 FSD | stress ratio、multi-load envelope | member sizing loop | section optimizer |
| 30 | BESO 拓樸最佳化 | strain energy sensitivity、hard kill | topology loop | topology optimizer |
| 31 | API / CLI / C API / Grasshopper | wire protocol、POD ABI、daemon | external bridge | 工具鏈介面 |
| 32 | Clone 專案總整合 | 分層重建、驗證矩陣、能力邊界 | MiniFrameCore | 可交接引擎核心 |

---



# Lesson 1 — 向量、矩陣、局部座標與結構狀態表示

## 1.1 本課定位

本課不是「線性代數複習」。

本課要建立的是一個力學引擎最底層的、不允許模糊的 convention layer。

在結構力學引擎中，只要下面任一點錯誤，後續所有結果都會出現表面正常但物理錯誤的災難：

1. DOF 順序錯。
2. 局部軸方向錯。
3. row-major / column-major convention 混亂。
4. \(R\) 與 \(R^T\) 用反。
5. \(K_g=T^TK_lT\) 寫成 \(TK_lT^T\)。
6. 彎矩正負號沒有對齊 local DOF。
7. 元素端部力 local / reaction global 混淆。

因此 Lesson 1 只做一件事：

> 把幾何、座標、DOF、元素狀態、局部 / 全域轉換徹底釘死。

---



## 1.2 本課輸出物

本課結束時，應該能從零寫出：

```text
Lesson1Core/
    CMakeLists.txt
    include/
        FrameTypes.hpp
        Mat3.hpp
        MemberFrame.hpp
        ModelTypes.hpp
        Transform12.hpp
        DenseSmall.hpp
    tests/
        test_lesson1.cpp
```

功能包括：

1. `Vec3`
2. `Mat3`
3. `Node`
4. `Material`
5. `Section`
6. `Member`
7. `memberLocalFrame(pi,pj,refVec)`
8. \(12\times12\) block transform
9. 能量不變性測試
10. row convention 測試
11. degenerate refVec fallback 測試

---



## 1.3 引擎級 convention

### 1.3.1 單位

本課採用：

\[
\text{Force}=\mathrm{N}
\]

\[
\text{Length}=\mathrm{mm}
\]

\[
\text{Stress}=\mathrm{MPa}=\mathrm{N/mm^2}
\]

對材料：

\[
E\in \mathrm{MPa}
\]

\[
G\in \mathrm{MPa}
\]

對截面：

\[
A\in \mathrm{mm^2}
\]

\[
I_y,I_z,J\in \mathrm{mm^4}
\]

軸向剛度：

\[
k_a=\frac{EA}{L}
\]

單位為：

\[
\frac{\mathrm{N/mm^2}\cdot\mathrm{mm^2}}{\mathrm{mm}}
=\mathrm{N/mm}
\]

彎曲剛度係數之一：

\[
k_b=\frac{12EI}{L^3}
\]

單位為：

\[
\frac{\mathrm{N/mm^2}\cdot\mathrm{mm^4}}{\mathrm{mm^3}}
=\mathrm{N/mm}
\]

轉角剛度係數之一：

\[
k_r=\frac{4EI}{L}
\]

單位為：

\[
\frac{\mathrm{N/mm^2}\cdot\mathrm{mm^4}}{\mathrm{mm}}
=\mathrm{N\cdot mm}
\]

因為轉角是 rad，工程上 rad 視為無因次，所以彎矩 / 轉角剛度為 \(\mathrm{N\cdot mm}\)。

---



### 1.3.2 節點自由度 DOF

每一個節點有 6 個 DOF：

\[
q_i=
\begin{bmatrix}
U_x\\
U_y\\
U_z\\
R_x\\
R_y\\
R_z
\end{bmatrix}_i
\]

其中：

\[
U_x,U_y,U_z
\]

是位移，單位為 mm。

\[
R_x,R_y,R_z
\]

是小轉角，單位為 rad。

固定 DOF 順序：

\[
[U_x,U_y,U_z,R_x,R_y,R_z]=[0,1,2,3,4,5]
\]

全域 DOF 編號：

\[
\operatorname{gdof}(i,d)=6i+d
\]

例如：節點 7 的 \(R_z\)：

\[
\operatorname{gdof}(7,5)=6\cdot7+5=47
\]

---



### 1.3.3 元素 DOF

一根二節點梁柱元素連接節點 \(i\) 與 \(j\)。

元素位移向量：

\[
q_e=
\begin{bmatrix}
q_i\\
q_j
\end{bmatrix}
\in\mathbb{R}^{12}
\]

完整展開：

\[
q_e=
\begin{bmatrix}
U_{xi}\\
U_{yi}\\
U_{zi}\\
R_{xi}\\
R_{yi}\\
R_{zi}\\
U_{xj}\\
U_{yj}\\
U_{zj}\\
R_{xj}\\
R_{yj}\\
R_{zj}
\end{bmatrix}
\]

索引表：

| local element index | DOF |
|---:|---|
| 0 | \(U_{xi}\) |
| 1 | \(U_{yi}\) |
| 2 | \(U_{zi}\) |
| 3 | \(R_{xi}\) |
| 4 | \(R_{yi}\) |
| 5 | \(R_{zi}\) |
| 6 | \(U_{xj}\) |
| 7 | \(U_{yj}\) |
| 8 | \(U_{zj}\) |
| 9 | \(R_{xj}\) |
| 10 | \(R_{yj}\) |
| 11 | \(R_{zj}\) |

這個順序與很多教科書的 block 排列一致：

\[
[u_i,v_i,w_i,\theta_{xi},\theta_{yi},\theta_{zi},u_j,v_j,w_j,\theta_{xj},\theta_{yj},\theta_{zj}]^T
\]

---



### 1.3.4 力與元素端力 convention

元素端力通常寫成：

\[
f_e=
\begin{bmatrix}
F_{xi}\\
F_{yi}\\
F_{zi}\\
M_{xi}\\
M_{yi}\\
M_{zi}\\
F_{xj}\\
F_{yj}\\
F_{zj}\\
M_{xj}\\
M_{yj}\\
M_{zj}
\end{bmatrix}
\]

結構分析常報告 local member end force：

\[
[N,V_y,V_z,T,M_y,M_z]_i
\]

\[
[N,V_y,V_z,T,M_y,M_z]_j
\]

其中：

- \(N\)：軸力。
- \(V_y,V_z\)：局部剪力。
- \(T\)：扭矩。
- \(M_y,M_z\)：局部彎矩。

此類引擎常約定：

```text
member end forces: local coordinates
reactions: global coordinates
```

這種分工很合理，因為：

- 桿件設計需要沿自身 local axis 讀取 \(N,V,M\)。
- 支承反力需要在全域座標下與外力平衡。

---



## 1.4 數學層：\(\mathbb{R}^3\) 向量

### 1.4.1 向量定義

\[
a=
\begin{bmatrix}
a_x\\a_y\\a_z
\end{bmatrix},
\qquad
b=
\begin{bmatrix}
b_x\\b_y\\b_z
\end{bmatrix}
\]

C++ POD：

```cpp
struct Vec3 {
    double x = 0;
    double y = 0;
    double z = 0;
};
```

這不是「小工具類」。  
這是整個引擎所有幾何、力、速度、力矩、軸向、shell normal、contact normal 的基礎資料單元。

---



### 1.4.2 線性運算

加法：

\[
a+b=
\begin{bmatrix}
a_x+b_x\\
a_y+b_y\\
a_z+b_z
\end{bmatrix}
\]

減法：

\[
a-b=
\begin{bmatrix}
a_x-b_x\\
a_y-b_y\\
a_z-b_z
\end{bmatrix}
\]

純量乘法：

\[
\lambda a=
\begin{bmatrix}
\lambda a_x\\
\lambda a_y\\
\lambda a_z
\end{bmatrix}
\]

向量的線性組合：

\[
v=\alpha a+\beta b+\gamma c
\]

這是 local axis 組成 global vector 的形式。

若 \(e_x,e_y,e_z\) 是一組基底，則：

\[
v_g=v_xe_x+v_ye_y+v_ze_z
\]

矩陣形式：

\[
v_g=
\begin{bmatrix}
e_x&e_y&e_z
\end{bmatrix}
\begin{bmatrix}
v_x\\v_y\\v_z
\end{bmatrix}
\]

注意這裡 \(e_x,e_y,e_z\) 作為 column 時，該矩陣是從 local 到 global 的矩陣。

---



### 1.4.3 內積 dot product

定義：

\[
a\cdot b=a_xb_x+a_yb_y+a_zb_z
\]

矩陣形式：

\[
a\cdot b=a^Tb
\]

長度：

\[
\|a\|=\sqrt{a\cdot a}
\]

夾角關係：

\[
a\cdot b=\|a\|\|b\|\cos\theta
\]

所以：

\[
\cos\theta=\frac{a\cdot b}{\|a\|\|b\|}
\]

垂直判定：

\[
a\perp b\quad\Longleftrightarrow\quad a\cdot b=0
\]

在引擎中的用途：

1. 判斷 local axes 是否正交。
2. 將 global vector 投影到 local axis。
3. 計算 contact normal velocity。
4. 計算桿件軸向伸長。
5. 計算模態正交性。

---



### 1.4.4 外積 cross product

定義：

\[
a\times b=
\begin{bmatrix}
a_yb_z-a_zb_y\\
a_zb_x-a_xb_z\\
a_xb_y-a_yb_x
\end{bmatrix}
\]

性質：

\[
a\times b=-(b\times a)
\]

\[
a\cdot(a\times b)=0
\]

\[
b\cdot(a\times b)=0
\]

長度：

\[
\|a\times b\|=\|a\|\|b\|\sin\theta
\]

外積方向由右手定則決定。

在 local frame 中，外積決定第三軸方向。

若：

\[
e_x=(1,0,0)
\]

\[
e_y=(0,1,0)
\]

則：

\[
e_x\times e_y=e_z=(0,0,1)
\]

---



### 1.4.5 外積矩陣

定義：

\[
[a]_\times=
\begin{bmatrix}
0&-a_z&a_y\\
a_z&0&-a_x\\
-a_y&a_x&0
\end{bmatrix}
\]

則：

\[
a\times b=[a]_\times b
\]

證明：

\[
[a]_\times b=
\begin{bmatrix}
0&-a_z&a_y\\
a_z&0&-a_x\\
-a_y&a_x&0
\end{bmatrix}
\begin{bmatrix}
b_x\\b_y\\b_z
\end{bmatrix}
\]

\[
=
\begin{bmatrix}
-a_zb_y+a_yb_z\\
a_zb_x-a_xb_z\\
-a_yb_x+a_xb_y
\end{bmatrix}
\]

\[
=
\begin{bmatrix}
a_yb_z-a_zb_y\\
a_zb_x-a_xb_z\\
a_xb_y-a_yb_x
\end{bmatrix}
=a\times b
\]

反對稱性：

\[
[a]_\times^T=-[a]_\times
\]

小角度旋轉：

\[
r'\approx r+\theta\times r
\]

\[
\Delta r=\theta\times r=[\theta]_\times r
\]

也可寫成：

\[
\theta\times r=-r\times \theta=-[r]_\times\theta
\]

所以剛體 contact point 速度：

\[
v_c=v+\omega\times r
\]

\[
=v-[r]_\times\omega
\]

這是後續碰撞與約束 Jacobian 的核心。

---



## 1.5 數學層：矩陣與座標變換

### 1.5.1 基底與座標

令 global basis 為：

\[
E=
\{\hat{i},\hat{j},\hat{k}\}
\]

local basis 為：

\[
B=\{e_x,e_y,e_z\}
\]

其中 \(e_x,e_y,e_z\) 皆用 global coordinates 表示。

任一 global vector \(v_g\) 可投影到 local basis：

\[
v_l=
\begin{bmatrix}
v_g\cdot e_x\\
v_g\cdot e_y\\
v_g\cdot e_z
\end{bmatrix}
\]

因此定義：

\[
R=
\begin{bmatrix}
e_x^T\\
e_y^T\\
e_z^T
\end{bmatrix}
\]

則：

\[
v_l=Rv_g
\]

這個 convention 非常重要：

```text
R 的 row 是 local axis。
R * global = local。
R^T * local = global。
```

---



### 1.5.2 正交矩陣

若 local axes 是正交單位向量：

\[
e_x\cdot e_x=1
\]

\[
e_y\cdot e_y=1
\]

\[
e_z\cdot e_z=1
\]

\[
e_x\cdot e_y=e_y\cdot e_z=e_z\cdot e_x=0
\]

則：

\[
RR^T=I
\]

證明：

\[
RR^T=
\begin{bmatrix}
e_x^T\\
e_y^T\\
e_z^T
\end{bmatrix}
\begin{bmatrix}
e_x&e_y&e_z
\end{bmatrix}
\]

\[
=
\begin{bmatrix}
e_x^Te_x&e_x^Te_y&e_x^Te_z\\
e_y^Te_x&e_y^Te_y&e_y^Te_z\\
e_z^Te_x&e_z^Te_y&e_z^Te_z
\end{bmatrix}
\]

\[
=
\begin{bmatrix}
1&0&0\\
0&1&0\\
0&0&1
\end{bmatrix}=I
\]

對正交矩陣：

\[
R^{-1}=R^T
\]

如果還滿足右手座標：

\[
e_x\times e_y=e_z
\]

則：

\[
\det R=+1
\]

若 \(\det R=-1\)，代表你建立了左手座標系。這會讓彎矩、扭轉、殼 normal 或 contact tangent 出現鏡射錯誤。

---



### 1.5.3 row convention 與 column convention 的比較

本課採用 row convention：

\[
R_{row}=\begin{bmatrix}e_x^T\\e_y^T\\e_z^T\end{bmatrix}
\]

\[
v_l=R_{row}v_g
\]

\[
v_g=R_{row}^Tv_l
\]

另一種常見 convention 是 column convention：

\[
C=\begin{bmatrix}e_x&e_y&e_z\end{bmatrix}
\]

\[
v_g=Cv_l
\]

\[
v_l=C^Tv_g
\]

兩者關係：

\[
R_{row}=C^T
\]

\[
C=R_{row}^T
\]

不要混用。

工程規則：

```text
函式名稱一定要帶方向：

mulGlobalToLocal(v)  // R * v
mulLocalToGlobal(v)  // R^T * v

不要只叫 rotate(v)。
```

---



## 1.6 建立梁柱元素局部座標系

### 1.6.1 輸入

給定兩端節點位置：

\[
p_i,p_j\in\mathbb{R}^3
\]

給定參考向量：

\[
r\in\mathbb{R}^3
\]

參考向量的角色：

```text
它不是 local y 軸本身。
它只是用來定義 local x-y 平面的大致方向。
```

---



### 1.6.2 local x 軸

元素方向：

\[
d=p_j-p_i
\]

長度：

\[
L=\|d\|
\]

若：

\[
L<\varepsilon
\]

則元素退化，不可建立。

local x：

\[
e_x=\frac{d}{L}
\]

---



### 1.6.3 local z 軸

若 \(r\) 大致在 local \(x-y\) 平面中，則垂直於該平面的方向為：

\[
e_x\times r
\]

因此：

\[
e_z=\frac{e_x\times r}{\|e_x\times r\|}
\]

若：

\[
\|e_x\times r\|<\varepsilon
\]

表示：

\[
r\parallel e_x
\]

此時無法定義 local \(x-y\) 平面，需要 fallback。

典型 fallback：

\[
r=(0,1,0)
\]

若仍平行，再用：

\[
r=(1,0,0)
\]

---



### 1.6.4 local y 軸

為了得到右手座標：

\[
e_x\times e_y=e_z
\]

由外積反推：

\[
e_y=e_z\times e_x
\]

檢查：

\[
e_x\times(e_z\times e_x)
\]

使用向量三重積：

\[
a\times(b\times c)=b(a\cdot c)-c(a\cdot b)
\]

令 \(a=e_x,b=e_z,c=e_x\)：

\[
e_x\times(e_z\times e_x)=e_z(e_x\cdot e_x)-e_x(e_x\cdot e_z)
\]

因為：

\[
e_x\cdot e_x=1
\]

\[
e_x\cdot e_z=0
\]

所以：

\[
e_x\times(e_z\times e_x)=e_z
\]

因此 \(e_y=e_z\times e_x\) 的確滿足：

\[
e_x\times e_y=e_z
\]

---



### 1.6.5 局部座標建立演算法

```text
Input: pi, pj, refVec

1. d = pj - pi
2. L = ||d||
3. ex = d / L
4. ezRaw = ex × refVec
5. if ||ezRaw|| too small:
       refVec = global +Y
       ezRaw = ex × refVec
6. if still too small:
       refVec = global +X
       ezRaw = ex × refVec
7. ez = normalize(ezRaw)
8. ey = normalize(ez × ex)
9. R = rows(ex, ey, ez)
10. assert orthonormal and det(R)=+1
```

---



### 1.6.6 ASCII 幾何圖

```text
global space

             refVec r
                ↑
                |
                |
node i ------------------------> node j
                  local x = ex
```

計算：

```text
ex = unit(j - i)
ez = unit(ex × refVec)
ey = unit(ez × ex)
```

若你手繪：

1. 畫 \(i\rightarrow j\) 作為元素軸。
2. 畫參考向量 \(r\)。
3. 右手定則畫 \(e_x\times r\)，它是 \(e_z\)。
4. 再畫 \(e_z\times e_x\)，它是 \(e_y\)。

---



## 1.7 元素 12 DOF 轉換矩陣

### 1.7.1 三維向量轉換

對任一全域向量：

\[
v_g\in\mathbb{R}^3
\]

轉到 local：

\[
v_l=Rv_g
\]

反向：

\[
v_g=R^Tv_l
\]

位移向量與小轉角向量都用同一個 \(R\)：

\[
u_l=Ru_g
\]

\[
\theta_l=R\theta_g
\]

注意：這裡 \(\theta\) 是小轉角向量，不是 finite rotation matrix。

---



### 1.7.2 元素位移 block

全域元素 DOF：

\[
q_g=
\begin{bmatrix}
u_i^g\\
\theta_i^g\\
u_j^g\\
\theta_j^g
\end{bmatrix}
\]

局部元素 DOF：

\[
q_l=
\begin{bmatrix}
u_i^l\\
\theta_i^l\\
u_j^l\\
\theta_j^l
\end{bmatrix}
\]

其中每個 block 都是 \(3\times1\)。

因此：

\[
q_l=
\begin{bmatrix}
R&0&0&0\\
0&R&0&0\\
0&0&R&0\\
0&0&0&R
\end{bmatrix}
q_g
\]

定義：

\[
T=\operatorname{diag}(R,R,R,R)
\]

則：

\[
q_l=Tq_g
\]

反向：

\[
q_g=T^Tq_l
\]

因為：

\[
T^{-1}=T^T
\]

---



### 1.7.3 block index map

元素 DOF 12 entries 可以被視為 4 個 3-vector block：

| block | entries | meaning |
|---:|---|---|
| 0 | 0,1,2 | node i translation |
| 1 | 3,4,5 | node i rotation |
| 2 | 6,7,8 | node j translation |
| 3 | 9,10,11 | node j rotation |

因此程式中：

```cpp
int offset = 3 * block;
Vec3 b = { q[offset+0], q[offset+1], q[offset+2] };
```

---



## 1.8 虛功與剛度轉換推導

這是本課的核心公式來源。

不要背：

\[
K_g=T^TK_lT
\]

要從虛功推出來。

---



### 1.8.1 局部剛度關係

元素在 local coordinates 中：

\[
f_l=K_lq_l
\]

其中：

\[
K_l\in\mathbb{R}^{12\times12}
\]

\[
q_l\in\mathbb{R}^{12}
\]

\[
f_l\in\mathbb{R}^{12}
\]

---



### 1.8.2 位移轉換

\[
q_l=Tq_g
\]

虛位移同理：

\[
\delta q_l=T\delta q_g
\]

---



### 1.8.3 虛功不變性

同一個物理虛功不應該因座標改變而改變：

\[
\delta W=\delta q_l^Tf_l=\delta q_g^Tf_g
\]

將 \(\delta q_l=T\delta q_g\) 代入：

\[
\delta W=(T\delta q_g)^Tf_l
\]

\[
=\delta q_g^TT^Tf_l
\]

與：

\[
\delta W=\delta q_g^Tf_g
\]

比較得：

\[
f_g=T^Tf_l
\]

所以力的轉換方向與位移相反：

```text
q_l = T q_g
f_g = T^T f_l
```

這不是任意規則，而是虛功對偶性。

---



### 1.8.4 剛度轉換

由：

\[
f_l=K_lq_l
\]

\[
f_g=T^Tf_l
\]

代入：

\[
f_g=T^TK_lq_l
\]

又：

\[
q_l=Tq_g
\]

因此：

\[
f_g=T^TK_lTq_g
\]

所以：

\[
K_g=T^TK_lT
\]

---



### 1.8.5 能量不變性

局部應變能：

\[
U_l=\frac{1}{2}q_l^TK_lq_l
\]

代入 \(q_l=Tq_g\)：

\[
U_l=\frac{1}{2}(Tq_g)^TK_l(Tq_g)
\]

矩陣乘法轉置：

\[
(Tq_g)^T=q_g^TT^T
\]

所以：

\[
U_l=\frac{1}{2}q_g^TT^TK_lTq_g
\]

定義：

\[
K_g=T^TK_lT
\]

得：

\[
U_l=\frac{1}{2}q_g^TK_gq_g=U_g
\]

測試 invariant：

\[
q_l^TK_lq_l=q_g^TK_gq_g
\]

若這條不通過，通常只有以下幾種可能：

1. \(R\) row / column convention 錯。
2. `mul` / `mulT` 寫反。
3. \(T\) block 順序錯。
4. \(K_g\) 用成 \(TK_lT^T\)。
5. `Mat12` index 錯。

---



## 1.9 從數學到資料結構

### 1.9.1 `Vec3` 設計原則

`Vec3` 必須：

1. 是 plain data。
2. 沒有 virtual function。
3. 沒有 hidden heap allocation。
4. 不依賴 Unreal / Eigen。
5. 可直接出現在 public API。

```cpp
struct Vec3 {
    real x = 0;
    real y = 0;
    real z = 0;
};
```

不要在底層 `Vec3` 裡放：

```cpp
std::vector<double>
std::string
shared_ptr
virtual function
UE type
Eigen::Vector3d
```

---



### 1.9.2 `Mat3` 設計原則

本課只需要 \(3\times3\) 矩陣。

```cpp
struct Mat3 {
    real m[3][3];
};
```

row convention：

```text
m[0] = ex^T
m[1] = ey^T
m[2] = ez^T
```

所以：

```cpp
Vec3 globalToLocal(Vec3 v) const; // R * v
Vec3 localToGlobal(Vec3 v) const; // R^T * v
```

命名上避免：

```cpp
rotate(v)
transform(v)
```

因為它們沒有說明方向。

---



### 1.9.3 `Node`

結構節點保存參考構型位置：

\[
X_i=(X_i,Y_i,Z_i)
\]

線性靜力分析求解的是 displacement：

\[
u_i=(U_x,U_y,U_z)
\]

變形後位置：

\[
x_i=X_i+u_i
\]

節點本身不一定保存求解後 displacement；求解結果可以存在 `SolveResult`。

最小 `Node`：

```cpp
struct Node {
    NodeId id;
    Vec3 pos;
    std::array<bool, 6> fixed;
    std::array<real, 6> prescribed;
};
```

---



### 1.9.4 `Material`

線彈性材料：

\[
\sigma=E\varepsilon
\]

剪切：

\[
\tau=G\gamma
\]

若由 \(E\) 與 Poisson ratio \(\nu\) 推 \(G\)：

\[
G=\frac{E}{2(1+\nu)}
\]

最小資料：

```cpp
struct Material {
    real E;
    real G;
    real rho;
};
```

其中 `rho` 對靜力可暫時不用，動力與自重會用。

---



### 1.9.5 `Section`

梁柱截面：

```cpp
struct Section {
    real A;
    real Iy;
    real Iz;
    real J;
};
```

物理意義：

\[
A: \text{area}
\]

\[
I_y: \text{second moment about local y}
\]

\[
I_z: \text{second moment about local z}
\]

\[
J: \text{torsional constant / polar-like torsion property}
\]

---



### 1.9.6 `Member`

最小梁柱元素拓樸：

```cpp
struct Member {
    MemberId id;
    NodeId i;
    NodeId j;
    int matIdx;
    int secIdx;
    Vec3 refVec;
    std::array<bool, 12> release;
    bool active;
};
```

不要在 `Member` 裡直接保存 raw pointer：

```cpp
Material* mat;
Section* sec;
```

原因：

```text
vector push_back 可能造成 reallocation。
raw pointer 可能 dangling。
```

用 index 是更穩定的 public model representation：

```cpp
int matIdx;
int secIdx;
```

---



## 1.10 Mermaid：Lesson 1 資料流

```mermaid
flowchart LR
    A[Node positions Xi] --> B[Member i-j]
    B --> C[Compute d = Xj - Xi]
    C --> D[ex = d / ||d||]
    D --> E[ez = normalize ex cross refVec]
    E --> F[ey = normalize ez cross ex]
    F --> G[R rows = ex ey ez]
    G --> H[T = diag R R R R]
    H --> I[q_l = T q_g]
    H --> J[K_g = T^T K_l T]
    J --> K[Energy invariant test]
```

---



# 2. Lesson 1 完整 C++17 實作

## 2.1 專案目錄

```text
Lesson1Core/
    CMakeLists.txt
    include/
        FrameTypes.hpp
        DenseSmall.hpp
        Mat3.hpp
        MemberFrame.hpp
        ModelTypes.hpp
        Transform12.hpp
    tests/
        test_lesson1.cpp
```

---



## 2.2 `FrameTypes.hpp`

```cpp
#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace mech {

using real = double;

constexpr real EPS = 1e-12;
constexpr real AXIS_EPS = 1e-8;

enum Dof : int {
    Ux = 0,
    Uy = 1,
    Uz = 2,
    Rx = 3,
    Ry = 4,
    Rz = 5
};

constexpr int DOF_PER_NODE = 6;
constexpr int ELEM_DOF = 12;

using NodeId = int;
using MemberId = int;

inline int gdof(int nodeIndex, int localDof) {
    return DOF_PER_NODE * nodeIndex + localDof;
}

struct Vec3 {
    real x = 0;
    real y = 0;
    real z = 0;

    Vec3() = default;

    Vec3(real x_, real y_, real z_)
        : x(x_), y(y_), z(z_) {}
};

inline Vec3 operator+(const Vec3& a, const Vec3& b) {
    return Vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

inline Vec3 operator-(const Vec3& a, const Vec3& b) {
    return Vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

inline Vec3 operator-(const Vec3& a) {
    return Vec3(-a.x, -a.y, -a.z);
}

inline Vec3 operator*(const Vec3& a, real s) {
    return Vec3(a.x * s, a.y * s, a.z * s);
}

inline Vec3 operator*(real s, const Vec3& a) {
    return a * s;
}

inline Vec3 operator/(const Vec3& a, real s) {
    if (std::abs(s) < EPS) {
        throw std::runtime_error("Vec3 division by near-zero scalar");
    }
    return Vec3(a.x / s, a.y / s, a.z / s);
}

inline real dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return Vec3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

inline real norm2(const Vec3& a) {
    return dot(a, a);
}

inline real norm(const Vec3& a) {
    return std::sqrt(norm2(a));
}

inline Vec3 normalized(const Vec3& a) {
    const real n = norm(a);
    if (n < EPS) {
        throw std::runtime_error("Cannot normalize near-zero vector");
    }
    return a / n;
}

inline bool near(real a, real b, real eps = 1e-9) {
    return std::abs(a - b) <= eps;
}

inline bool nearVec(const Vec3& a, const Vec3& b, real eps = 1e-9) {
    return near(a.x, b.x, eps)
        && near(a.y, b.y, eps)
        && near(a.z, b.z, eps);
}

} // namespace mech
```

---



## 2.3 `DenseSmall.hpp`

本課不需要通用大矩陣，只需要固定大小容器。

```cpp
#pragma once

#include "FrameTypes.hpp"
#include <array>

namespace mech {

template <int N>
using VecN = std::array<real, N>;

template <int R, int C>
using MatRC = std::array<std::array<real, C>, R>;

using Vec12 = VecN<12>;
using Mat12 = MatRC<12, 12>;

template <int N>
inline VecN<N> zeroVec() {
    VecN<N> v{};
    for (int i = 0; i < N; ++i) {
        v[i] = 0;
    }
    return v;
}

template <int R, int C>
inline MatRC<R, C> zeroMat() {
    MatRC<R, C> A{};
    for (int i = 0; i < R; ++i) {
        for (int j = 0; j < C; ++j) {
            A[i][j] = 0;
        }
    }
    return A;
}

template <int N>
inline real dotN(const VecN<N>& a, const VecN<N>& b) {
    real s = 0;
    for (int i = 0; i < N; ++i) {
        s += a[i] * b[i];
    }
    return s;
}

template <int R, int C>
inline VecN<R> mul(const MatRC<R, C>& A, const VecN<C>& x) {
    VecN<R> y = zeroVec<R>();
    for (int i = 0; i < R; ++i) {
        real s = 0;
        for (int j = 0; j < C; ++j) {
            s += A[i][j] * x[j];
        }
        y[i] = s;
    }
    return y;
}

template <int N>
inline real quadraticForm(const VecN<N>& q, const MatRC<N, N>& K) {
    real result = 0;
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            result += q[i] * K[i][j] * q[j];
        }
    }
    return result;
}

template <int N>
inline bool nearVecN(const VecN<N>& a, const VecN<N>& b, real eps = 1e-9) {
    for (int i = 0; i < N; ++i) {
        if (!near(a[i], b[i], eps)) {
            return false;
        }
    }
    return true;
}

} // namespace mech
```

---



## 2.4 `Mat3.hpp`

```cpp
#pragma once

#include "FrameTypes.hpp"

namespace mech {

struct Mat3 {
    real m[3][3] = {
        {0, 0, 0},
        {0, 0, 0},
        {0, 0, 0}
    };

    static Mat3 identity() {
        Mat3 I;
        I.m[0][0] = 1;
        I.m[1][1] = 1;
        I.m[2][2] = 1;
        return I;
    }

    static Mat3 fromRows(const Vec3& r0, const Vec3& r1, const Vec3& r2) {
        Mat3 R;
        R.m[0][0] = r0.x; R.m[0][1] = r0.y; R.m[0][2] = r0.z;
        R.m[1][0] = r1.x; R.m[1][1] = r1.y; R.m[1][2] = r1.z;
        R.m[2][0] = r2.x; R.m[2][1] = r2.y; R.m[2][2] = r2.z;
        return R;
    }

    Vec3 row(int i) const {
        return Vec3(m[i][0], m[i][1], m[i][2]);
    }

    Vec3 col(int j) const {
        return Vec3(m[0][j], m[1][j], m[2][j]);
    }

    // R * v : global vector -> local vector
    Vec3 globalToLocal(const Vec3& v) const {
        return Vec3(
            m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
            m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
            m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z
        );
    }

    // R^T * v : local vector -> global vector
    Vec3 localToGlobal(const Vec3& v) const {
        return Vec3(
            m[0][0] * v.x + m[1][0] * v.y + m[2][0] * v.z,
            m[0][1] * v.x + m[1][1] * v.y + m[2][1] * v.z,
            m[0][2] * v.x + m[1][2] * v.y + m[2][2] * v.z
        );
    }

    real det() const {
        const real a = m[0][0], b = m[0][1], c = m[0][2];
        const real d = m[1][0], e = m[1][1], f = m[1][2];
        const real g = m[2][0], h = m[2][1], i = m[2][2];

        return a * (e * i - f * h)
             - b * (d * i - f * g)
             + c * (d * h - e * g);
    }
};

inline bool isOrthonormal(const Mat3& R, real eps = 1e-8) {
    const Vec3 r0 = R.row(0);
    const Vec3 r1 = R.row(1);
    const Vec3 r2 = R.row(2);

    return near(dot(r0, r0), 1.0, eps)
        && near(dot(r1, r1), 1.0, eps)
        && near(dot(r2, r2), 1.0, eps)
        && near(dot(r0, r1), 0.0, eps)
        && near(dot(r1, r2), 0.0, eps)
        && near(dot(r2, r0), 0.0, eps)
        && near(R.det(), 1.0, eps);
}

} // namespace mech
```

---



## 2.5 `MemberFrame.hpp`

```cpp
#pragma once

#include "FrameTypes.hpp"
#include "Mat3.hpp"

namespace mech {

struct MemberFrame {
    Vec3 ex;
    Vec3 ey;
    Vec3 ez;
    Mat3 R;
    real L = 0;
};

inline MemberFrame makeMemberFrame(
    const Vec3& pi,
    const Vec3& pj,
    const Vec3& refVec = Vec3(0, 0, 1)
) {
    const Vec3 d = pj - pi;
    const real L = norm(d);

    if (L < EPS) {
        throw std::runtime_error("Degenerate member: zero length");
    }

    const Vec3 ex = d / L;

    Vec3 ref = refVec;
    Vec3 ezRaw = cross(ex, ref);

    if (norm(ezRaw) < AXIS_EPS) {
        ref = Vec3(0, 1, 0);
        ezRaw = cross(ex, ref);
    }

    if (norm(ezRaw) < AXIS_EPS) {
        ref = Vec3(1, 0, 0);
        ezRaw = cross(ex, ref);
    }

    if (norm(ezRaw) < AXIS_EPS) {
        throw std::runtime_error("Cannot build member frame: fallback refs failed");
    }

    const Vec3 ez = normalized(ezRaw);
    const Vec3 ey = normalized(cross(ez, ex));

    const Mat3 R = Mat3::fromRows(ex, ey, ez);

    if (!isOrthonormal(R)) {
        throw std::runtime_error("Internal error: member frame is not orthonormal");
    }

    MemberFrame f;
    f.ex = ex;
    f.ey = ey;
    f.ez = ez;
    f.R = R;
    f.L = L;
    return f;
}

} // namespace mech
```

---



## 2.6 `ModelTypes.hpp`

```cpp
#pragma once

#include "FrameTypes.hpp"
#include <array>

namespace mech {

struct Node {
    NodeId id = 0;
    Vec3 pos;

    std::array<bool, DOF_PER_NODE> fixed {
        false, false, false, false, false, false
    };

    std::array<real, DOF_PER_NODE> prescribed {
        0, 0, 0, 0, 0, 0
    };

    Node() = default;

    Node(NodeId id_, real x, real y, real z)
        : id(id_), pos(x, y, z) {}

    void fixAll() {
        fixed = { true, true, true, true, true, true };
    }

    void pinTranslations() {
        fixed = { true, true, true, false, false, false };
    }
};

struct Material {
    real E = 0;
    real G = 0;
    real rho = 0;
};

struct Section {
    real A = 0;
    real Iy = 0;
    real Iz = 0;
    real J = 0;
};

enum class ReleasePreset {
    Rigid,
    TrussPin,
    HingeI,
    HingeJ
};

inline std::array<bool, ELEM_DOF> makeRelease(ReleasePreset p) {
    std::array<bool, ELEM_DOF> r{};
    for (bool& b : r) {
        b = false;
    }

    switch (p) {
        case ReleasePreset::Rigid:
            break;

        case ReleasePreset::TrussPin:
            r[3] = r[4] = r[5] = true;
            r[9] = r[10] = r[11] = true;
            break;

        case ReleasePreset::HingeI:
            r[4] = true;
            r[5] = true;
            break;

        case ReleasePreset::HingeJ:
            r[10] = true;
            r[11] = true;
            break;
    }

    return r;
}

struct Member {
    MemberId id = 0;
    NodeId i = 0;
    NodeId j = 0;

    int matIdx = -1;
    int secIdx = -1;

    Vec3 refVec = Vec3(0, 0, 1);

    std::array<bool, ELEM_DOF> release{};

    bool active = true;
    bool tensionOnly = false;

    Member() = default;

    Member(MemberId id_, NodeId i_, NodeId j_, int matIdx_, int secIdx_)
        : id(id_), i(i_), j(j_), matIdx(matIdx_), secIdx(secIdx_) {}
};

} // namespace mech
```

---



## 2.7 `Transform12.hpp`

```cpp
#pragma once

#include "DenseSmall.hpp"
#include "Mat3.hpp"

namespace mech {

inline Vec3 getBlock3(const Vec12& v, int block) {
    const int o = 3 * block;
    return Vec3(v[o + 0], v[o + 1], v[o + 2]);
}

inline void setBlock3(Vec12& v, int block, const Vec3& x) {
    const int o = 3 * block;
    v[o + 0] = x.x;
    v[o + 1] = x.y;
    v[o + 2] = x.z;
}

// q_l = T q_g
inline Vec12 toLocal12(const Mat3& R, const Vec12& qg) {
    Vec12 ql = zeroVec<12>();

    for (int block = 0; block < 4; ++block) {
        const Vec3 vg = getBlock3(qg, block);
        const Vec3 vl = R.globalToLocal(vg);
        setBlock3(ql, block, vl);
    }

    return ql;
}

// q_g = T^T q_l
inline Vec12 toGlobal12(const Mat3& R, const Vec12& ql) {
    Vec12 qg = zeroVec<12>();

    for (int block = 0; block < 4; ++block) {
        const Vec3 vl = getBlock3(ql, block);
        const Vec3 vg = R.localToGlobal(vl);
        setBlock3(qg, block, vg);
    }

    return qg;
}

// K_g = T^T K_l T
inline Mat12 transformKLocalToGlobal(const Mat3& R, const Mat12& Kl) {
    Mat12 Kg = zeroMat<12, 12>();

    for (int br = 0; br < 4; ++br) {
        for (int bc = 0; bc < 4; ++bc) {
            for (int ga = 0; ga < 3; ++ga) {
                for (int gb = 0; gb < 3; ++gb) {
                    real sum = 0;

                    for (int la = 0; la < 3; ++la) {
                        for (int lb = 0; lb < 3; ++lb) {
                            const int rowL = 3 * br + la;
                            const int colL = 3 * bc + lb;

                            // T maps global to local: local_index la receives R[la][ga] * global_ga.
                            sum += R.m[la][ga] * Kl[rowL][colL] * R.m[lb][gb];
                        }
                    }

                    const int rowG = 3 * br + ga;
                    const int colG = 3 * bc + gb;
                    Kg[rowG][colG] = sum;
                }
            }
        }
    }

    return Kg;
}

inline bool isSymmetric12(const Mat12& A, real eps = 1e-9) {
    for (int i = 0; i < 12; ++i) {
        for (int j = i + 1; j < 12; ++j) {
            if (!near(A[i][j], A[j][i], eps)) {
                return false;
            }
        }
    }
    return true;
}

} // namespace mech
```

---



## 2.8 `test_lesson1.cpp`

```cpp
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

#include "FrameTypes.hpp"
#include "Mat3.hpp"
#include "MemberFrame.hpp"
#include "ModelTypes.hpp"
#include "Transform12.hpp"

using namespace mech;

static Mat12 makeSymmetricPositiveTestMatrix() {
    Mat12 K = zeroMat<12, 12>();

    for (int i = 0; i < 12; ++i) {
        for (int j = i; j < 12; ++j) {
            real value = 0.01 * static_cast<real>((i + 1) * (j + 2));
            if (i == j) {
                value += 10.0;
            }
            K[i][j] = value;
            K[j][i] = value;
        }
    }

    return K;
}

static Vec12 makeTestVec12() {
    Vec12 q = zeroVec<12>();
    for (int i = 0; i < 12; ++i) {
        q[i] = 0.1 * static_cast<real>(i + 1);
    }
    return q;
}

static void test_vec3_basic() {
    Vec3 a(1, 2, 3);
    Vec3 b(4, -5, 6);

    assert(near(dot(a, b), 1 * 4 + 2 * (-5) + 3 * 6));

    Vec3 c = cross(a, b);
    assert(nearVec(c, Vec3(27, 6, -13)));
    assert(near(dot(a, c), 0.0));
    assert(near(dot(b, c), 0.0));
}

static void test_member_frame_axis_aligned() {
    Vec3 pi(0, 0, 0);
    Vec3 pj(3000, 0, 0);
    Vec3 ref(0, 0, 1);

    MemberFrame f = makeMemberFrame(pi, pj, ref);

    assert(near(f.L, 3000.0));
    assert(nearVec(f.ex, Vec3(1, 0, 0)));
    assert(nearVec(f.ey, Vec3(0, 0, 1)));
    assert(nearVec(f.ez, Vec3(0, -1, 0)));
    assert(isOrthonormal(f.R));
}

static void test_member_frame_roundtrip() {
    Vec3 pi(0, 0, 0);
    Vec3 pj(3, 4, 5);
    Vec3 ref(0, 0, 1);

    MemberFrame f = makeMemberFrame(pi, pj, ref);

    Vec3 vg(7, -2, 5);
    Vec3 vl = f.R.globalToLocal(vg);
    Vec3 vg2 = f.R.localToGlobal(vl);

    assert(nearVec(vg, vg2, 1e-9));
}

static void test_fallback_refvec_parallel() {
    Vec3 pi(0, 0, 0);
    Vec3 pj(0, 0, 10);
    Vec3 ref(0, 0, 1);

    MemberFrame f = makeMemberFrame(pi, pj, ref);

    assert(nearVec(f.ex, Vec3(0, 0, 1)));
    assert(isOrthonormal(f.R));
    assert(near(f.R.det(), 1.0, 1e-9));
}

static void test_12_vector_roundtrip() {
    Vec3 pi(0, 0, 0);
    Vec3 pj(3, 4, 5);
    MemberFrame f = makeMemberFrame(pi, pj, Vec3(0, 0, 1));

    Vec12 qg = makeTestVec12();
    Vec12 ql = toLocal12(f.R, qg);
    Vec12 qg2 = toGlobal12(f.R, ql);

    assert(nearVecN<12>(qg, qg2, 1e-9));
}

static void test_energy_invariance() {
    Vec3 pi(0, 0, 0);
    Vec3 pj(3, 4, 5);
    Vec3 ref(0, 0, 1);

    MemberFrame f = makeMemberFrame(pi, pj, ref);

    Mat12 Kl = makeSymmetricPositiveTestMatrix();
    Mat12 Kg = transformKLocalToGlobal(f.R, Kl);

    assert(isSymmetric12(Kl));
    assert(isSymmetric12(Kg, 1e-8));

    Vec12 qg = makeTestVec12();
    Vec12 ql = toLocal12(f.R, qg);

    real eLocal = quadraticForm<12>(ql, Kl);
    real eGlobal = quadraticForm<12>(qg, Kg);

    assert(std::abs(eLocal - eGlobal) < 1e-8);
}

static void test_gdof() {
    assert(gdof(0, Ux) == 0);
    assert(gdof(0, Rz) == 5);
    assert(gdof(3, Ry) == 22);
    assert(gdof(7, Rz) == 47);
}

int main() {
    test_vec3_basic();
    test_gdof();
    test_member_frame_axis_aligned();
    test_member_frame_roundtrip();
    test_fallback_refvec_parallel();
    test_12_vector_roundtrip();
    test_energy_invariance();

    std::cout << "Lesson 1 tests passed.\n";
    return 0;
}
```

---



## 2.9 `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.16)
project(Lesson1Core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(test_lesson1
    tests/test_lesson1.cpp
)

target_include_directories(test_lesson1 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if (MSVC)
    target_compile_options(test_lesson1 PRIVATE /W4 /permissive-)
else()
    target_compile_options(test_lesson1 PRIVATE -Wall -Wextra -Wpedantic)
endif()
```

---



## 2.10 編譯方式

```bash
mkdir build
cd build
cmake ..
cmake --build .
./test_lesson1
```

期望輸出：

```text
Lesson 1 tests passed.
```

---



# 3. Lesson 1 手算推導與例題

## 3.1 例題 A：軸向元素的 local frame

給定：

\[
p_i=(0,0,0)
\]

\[
p_j=(3000,0,0)
\]

\[
r=(0,0,1)
\]

計算：

\[
d=p_j-p_i=(3000,0,0)
\]

\[
L=\|d\|=3000
\]

\[
e_x=\frac{d}{L}=(1,0,0)
\]

\[
e_z=\frac{e_x\times r}{\|e_x\times r\|}
\]

\[
e_x\times r=(1,0,0)\times(0,0,1)
\]

\[
=
\begin{bmatrix}
0\cdot1-0\cdot0\\
0\cdot0-1\cdot1\\
1\cdot0-0\cdot0
\end{bmatrix}
=
\begin{bmatrix}
0\\-1\\0
\end{bmatrix}
\]

所以：

\[
e_z=(0,-1,0)
\]

\[
e_y=e_z\times e_x
\]

\[
=(0,-1,0)\times(1,0,0)
\]

\[
=
\begin{bmatrix}
(-1)\cdot0-0\cdot0\\
0\cdot1-0\cdot0\\
0\cdot0-(-1)\cdot1
\end{bmatrix}
=
\begin{bmatrix}
0\\0\\1
\end{bmatrix}
\]

因此：

\[
R=
\begin{bmatrix}
1&0&0\\
0&0&1\\
0&-1&0
\end{bmatrix}
\]

---



## 3.2 例題 B：全域力轉局部力

令全域力：

\[
f_g=(0,-12,0)
\]

用上題：

\[
R=
\begin{bmatrix}
1&0&0\\
0&0&1\\
0&-1&0
\end{bmatrix}
\]

則：

\[
f_l=Rf_g
\]

\[
=
\begin{bmatrix}
1&0&0\\
0&0&1\\
0&-1&0
\end{bmatrix}
\begin{bmatrix}
0\\-12\\0
\end{bmatrix}
\]

\[
=
\begin{bmatrix}
0\\0\\12
\end{bmatrix}
\]

結論：

```text
global -Y force = local +Z force
```

---



## 3.3 例題 C：一般斜桿 local frame

給定：

\[
p_i=(0,0,0)
\]

\[
p_j=(3,4,0)
\]

\[
r=(0,0,1)
\]

計算：

\[
d=(3,4,0)
\]

\[
L=\sqrt{3^2+4^2}=5
\]

\[
e_x=\left(\frac{3}{5},\frac{4}{5},0\right)
\]

\[
e_z=\frac{e_x\times r}{\|e_x\times r\|}
\]

\[
e_x\times r=
\left(\frac{3}{5},\frac{4}{5},0\right)
\times(0,0,1)
\]

\[
=
\begin{bmatrix}
\frac{4}{5}\cdot1-0\cdot0\\
0\cdot0-\frac{3}{5}\cdot1\\
\frac{3}{5}\cdot0-\frac{4}{5}\cdot0
\end{bmatrix}
=
\begin{bmatrix}
\frac{4}{5}\\
-\frac{3}{5}\\
0
\end{bmatrix}
\]

長度：

\[
\sqrt{\left(\frac{4}{5}\right)^2+\left(-\frac{3}{5}\right)^2}=1
\]

所以：

\[
e_z=\left(\frac{4}{5},-\frac{3}{5},0\right)
\]

\[
e_y=e_z\times e_x
\]

\[
=\left(\frac{4}{5},-\frac{3}{5},0\right)
\times
\left(\frac{3}{5},\frac{4}{5},0\right)
\]

\[
=
\begin{bmatrix}
0\\0\\
\frac{4}{5}\cdot\frac{4}{5}-\left(-\frac{3}{5}\right)\cdot\frac{3}{5}
\end{bmatrix}
=
\begin{bmatrix}
0\\0\\1
\end{bmatrix}
\]

因此：

\[
R=
\begin{bmatrix}
3/5&4/5&0\\
0&0&1\\
4/5&-3/5&0
\end{bmatrix}
\]

---



## 3.4 例題 D：2D axial bar 剛度從能量推出

雖然本課主軸是 3D frame，但 2D axial bar 是最純粹的剛度推導模型。

軸向單位向量：

\[
n=\begin{bmatrix}c\\s\end{bmatrix}
\]

節點位移：

\[
u_i=\begin{bmatrix}u_{xi}\\u_{yi}\end{bmatrix},
\qquad
u_j=\begin{bmatrix}u_{xj}\\u_{yj}\end{bmatrix}
\]

相對位移：

\[
u_j-u_i
\]

軸向伸長：

\[
\Delta=n^T(u_j-u_i)
\]

展開：

\[
\Delta=
\begin{bmatrix}c&s\end{bmatrix}
\left(
\begin{bmatrix}u_{xj}\\u_{yj}\end{bmatrix}
-
\begin{bmatrix}u_{xi}\\u_{yi}\end{bmatrix}
\right)
\]

\[
=-cu_{xi}-su_{yi}+cu_{xj}+su_{yj}
\]

定義：

\[
q=\begin{bmatrix}u_{xi}\\u_{yi}\\u_{xj}\\u_{yj}\end{bmatrix}
\]

\[
B=\begin{bmatrix}-c&-s&c&s\end{bmatrix}
\]

則：

\[
\Delta=Bq
\]

軸向能量：

\[
U=\frac{1}{2}k\Delta^2
\]

\[
=\frac{1}{2}k(Bq)^T(Bq)
\]

因為 \(Bq\) 是 scalar：

\[
(Bq)^T(Bq)=q^TB^TBq
\]

所以：

\[
U=\frac{1}{2}q^T(kB^TB)q
\]

剛度矩陣：

\[
K=kB^TB
\]

展開：

\[
K=k
\begin{bmatrix}
 c^2& cs&-c^2&-cs\\
 cs& s^2&-cs&-s^2\\
-c^2&-cs& c^2& cs\\
-cs&-s^2& cs& s^2
\end{bmatrix}
\]

這個推導會在 Lesson 10 變成 3D beam-column 的能量推導。

---



# 4. Debug Checklist

## 4.1 local frame 錯誤檢查

每建立一根 member frame，都應檢查：

\[
\|e_x\|=1
\]

\[
\|e_y\|=1
\]

\[
\|e_z\|=1
\]

\[
e_x\cdot e_y=0
\]

\[
e_y\cdot e_z=0
\]

\[
e_z\cdot e_x=0
\]

\[
e_x\times e_y=e_z
\]

\[
\det R=1
\]

若 \(\det R=-1\)，通常是：

```cpp
ey = cross(ex, ez); // wrong for this convention
```

正確是：

```cpp
ey = cross(ez, ex);
```

---



## 4.2 轉換方向錯誤檢查

檢查：

\[
v_g=R^T(Rv_g)
\]

程式：

```cpp
Vec3 vl = R.globalToLocal(vg);
Vec3 vg2 = R.localToGlobal(vl);
assert(nearVec(vg, vg2));
```

---



## 4.3 剛度轉換錯誤檢查

取任意 symmetric \(K_l\)，任意 \(q_g\)：

\[
q_l=Tq_g
\]

\[
K_g=T^TK_lT
\]

檢查：

\[
q_l^TK_lq_l=q_g^TK_gq_g
\]

這是最硬的 invariant。

---



## 4.4 單位錯誤檢查

若使用：

\[
E=200000\ \mathrm{MPa}
\]

\[
A=10000\ \mathrm{mm^2}
\]

\[
L=3000\ \mathrm{mm}
\]

軸向剛度：

\[
\frac{EA}{L}=\frac{200000\cdot10000}{3000}=666666.6667\ \mathrm{N/mm}
\]

若你得到 \(666.7\)、\(6.67e8\)、或 \(6.67e-4\)，通常是 m / mm 單位混用。

---



# 5. 課後硬核練習題

## 題目 1：local frame 與 force transform 手算

給定：

\[
p_i=(0,0,0)
\]

\[
p_j=(0,4000,0)
\]

\[
r=(0,0,1)
\]

全域力：

\[
f_g=(10,20,30)
\]

請求：

1. \(L\)
2. \(e_x,e_y,e_z\)
3. \(R\)
4. \(f_l=Rf_g\)
5. 驗證 \(\det R=1\)

<details>
<summary>解析答案</summary>

\[
d=(0,4000,0)
\]

\[
L=4000
\]

\[
e_x=(0,1,0)
\]

\[
e_z=\frac{e_x\times r}{\|e_x\times r\|}
=(0,1,0)\times(0,0,1)
\]

\[
=
\begin{bmatrix}
1\cdot1-0\cdot0\\
0\cdot0-0\cdot1\\
0\cdot0-1\cdot0
\end{bmatrix}
=
(1,0,0)
\]

\[
e_y=e_z\times e_x=(1,0,0)\times(0,1,0)=(0,0,1)
\]

所以：

\[
R=
\begin{bmatrix}
0&1&0\\
0&0&1\\
1&0&0
\end{bmatrix}
\]

\[
f_l=Rf_g=
\begin{bmatrix}
0&1&0\\
0&0&1\\
1&0&0
\end{bmatrix}
\begin{bmatrix}
10\\20\\30
\end{bmatrix}
=
\begin{bmatrix}
20\\30\\10
\end{bmatrix}
\]

\[
\det R=1
\]

因為該矩陣是 cyclic permutation matrix，且方向為右手系。

</details>

---



## 題目 2：能量不變性手算簡化版

考慮 2D axial bar。

\[
c=\frac{3}{5},\qquad s=\frac{4}{5}
\]

\[
k=1000
\]

\[
q=\begin{bmatrix}1\\2\\4\\6\end{bmatrix}
\]

1. 求 axial elongation \(\Delta\)。
2. 求 \(U=\frac{1}{2}k\Delta^2\)。
3. 用 \(K=kB^TB\) 驗證 \(U=\frac{1}{2}q^TKq\)。

<details>
<summary>解析答案</summary>

\[
B=\begin{bmatrix}-c&-s&c&s\end{bmatrix}
=
\begin{bmatrix}-3/5&-4/5&3/5&4/5\end{bmatrix}
\]

\[
q=\begin{bmatrix}1\\2\\4\\6\end{bmatrix}
\]

\[
\Delta=Bq
\]

\[
=-\frac{3}{5}(1)-\frac{4}{5}(2)+\frac{3}{5}(4)+\frac{4}{5}(6)
\]

\[
=-\frac{3}{5}-\frac{8}{5}+\frac{12}{5}+\frac{24}{5}
\]

\[
=\frac{25}{5}=5
\]

能量：

\[
U=\frac{1}{2}k\Delta^2
\]

\[
=\frac{1}{2}\cdot1000\cdot25=12500
\]

因為：

\[
K=kB^TB
\]

所以：

\[
q^TKq=q^T(kB^TB)q
\]

\[
=k(Bq)^T(Bq)
\]

\[
=k\Delta^2
\]

\[
=1000\cdot25=25000
\]

因此：

\[
\frac{1}{2}q^TKq=12500
\]

與直接能量一致。

</details>

---



## 題目 3：核心程式碼重構 / 實作題

把本課程式碼重構為「方向顯式」API。

要求：

1. `Mat3` 不允許出現 `mul()`。
2. 必須改為：

```cpp
Vec3 globalToLocal(const Vec3& v) const;
Vec3 localToGlobal(const Vec3& v) const;
```

3. `Transform12.hpp` 不允許直接手寫 `R.m` 轉向量；必須通過 `globalToLocal()` 與 `localToGlobal()` 完成向量轉換。
4. `transformKLocalToGlobal()` 內保留矩陣 index 展開，但要用註解寫出：

\[
K_g[a,b]=\sum_{\alpha,\beta}T_{\alpha a}K_l[\alpha,\beta]T_{\beta b}
\]

5. 加入以下測試：

```cpp
static void test_wrong_transform_should_fail_energy();
```

這個測試故意構造：

\[
K_{wrong}=TK_lT^T
\]

並驗證它通常不滿足：

\[
q_l^TK_lq_l=q_g^TK_{wrong}q_g
\]

目的：證明你不是只靠「結果看起來差不多」通過，而是靠 invariant 抓錯。

---



# 6. Lesson 1 必背不變量

\[
\operatorname{gdof}(i,d)=6i+d
\]

\[
e_x=\frac{p_j-p_i}{\|p_j-p_i\|}
\]

\[
e_z=\frac{e_x\times r}{\|e_x\times r\|}
\]

\[
e_y=e_z\times e_x
\]

\[
R=
\begin{bmatrix}
e_x^T\\e_y^T\\e_z^T
\end{bmatrix}
\]

\[
v_l=Rv_g
\]

\[
v_g=R^Tv_l
\]

\[
T=\operatorname{diag}(R,R,R,R)
\]

\[
q_l=Tq_g
\]

\[
f_g=T^Tf_l
\]

\[
K_g=T^TK_lT
\]

\[
q_l^TK_lq_l=q_g^TK_gq_g
\]

---



# 7. Lesson 2 預告：運動學與狀態方程

下一課會把「座標與 DOF」推進到「時間演化」。

我們會嚴格區分：

## 7.1 Particle state

\[
x,v,m
\]

\[
\dot{x}=v
\]

\[
m\dot{v}=f
\]

---



## 7.2 Rigid body state

\[
x,R,v,\omega,m,I
\]

\[
\dot{x}=v
\]

\[
\dot{R}=[\omega]_\times R
\]

\[
M\dot{v}=f
\]

\[
I\dot{\omega}+\omega\times(I\omega)=\tau
\]

---



## 7.3 Structural node state

\[
X,u,\theta,\dot{u},\dot{\theta}
\]

\[
x=X+u
\]

整體系統：

\[
M\ddot{q}+C\dot{q}+Kq=f(t)
\]

這會把一般物理引擎與結構 FEM 引擎正式接起來。
