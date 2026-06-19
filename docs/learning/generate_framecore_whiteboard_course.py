from __future__ import annotations

import math
import os
import random
from pathlib import Path

from reportlab.lib import colors
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfgen import canvas


ROOT = Path(__file__).resolve().parents[2]
OUT_DIR = ROOT / "output" / "pdf"
PNG_DIR = OUT_DIR / "framecore_v2_whiteboard_pages"
PDF_PATH = OUT_DIR / "framecore_v2_whiteboard_course.pdf"

PAGE_W = 4200
PAGE_H = 2600

FONT_TC = "NotoSansTC"
FONT_TC_BOLD = "NotoSansTC"
FONT_HAND = "InkFree"
FONT_CODE = "Courier"


def register_fonts() -> None:
    pdfmetrics.registerFont(TTFont(FONT_TC, r"C:\Windows\Fonts\NotoSansTC-VF.ttf"))
    pdfmetrics.registerFont(TTFont(FONT_HAND, r"C:\Windows\Fonts\Inkfree.ttf"))


PALETTE = {
    "ink": colors.HexColor("#111111"),
    "muted": colors.HexColor("#606060"),
    "paper": colors.HexColor("#FBFAF6"),
    "blue": colors.HexColor("#49A7D8"),
    "orange": colors.HexColor("#F6A23A"),
    "yellow": colors.HexColor("#F7D94C"),
    "pink": colors.HexColor("#ED5AA6"),
    "purple": colors.HexColor("#8E63CE"),
    "green": colors.HexColor("#45B37D"),
    "red": colors.HexColor("#E15656"),
    "cyan": colors.HexColor("#42C4C7"),
    "sand": colors.HexColor("#F3E7C0"),
    "gray": colors.HexColor("#E8E5DE"),
}


def lighten(color, amount=0.75):
    r, g, b = color.red, color.green, color.blue
    return colors.Color(r + (1 - r) * amount, g + (1 - g) * amount, b + (1 - b) * amount)


def set_font(c: canvas.Canvas, name: str, size: float, fill=PALETTE["ink"]) -> None:
    c.setFont(name, size)
    c.setFillColor(fill)


def text_width(text: str, font: str, size: float) -> float:
    return pdfmetrics.stringWidth(text, font, size)


def wrap_line(text: str, font: str, size: float, max_w: float) -> list[str]:
    chunks: list[str] = []
    line = ""
    break_chars = set(" /,.;:()[]{}+-=<>|")
    for ch in text:
        candidate = line + ch
        if text_width(candidate, font, size) <= max_w:
            line = candidate
            continue
        if line:
            chunks.append(line.rstrip())
        line = ch.lstrip()
    if line:
        chunks.append(line.rstrip())
    # Second pass for very long code-ish words.
    out: list[str] = []
    for chunk in chunks:
        if text_width(chunk, font, size) <= max_w:
            out.append(chunk)
            continue
        line = ""
        for ch in chunk:
            candidate = line + ch
            if text_width(candidate, font, size) <= max_w:
                line = candidate
            else:
                if line:
                    out.append(line)
                line = ch
        if line:
            out.append(line)
    return out


def draw_wrapped(
    c: canvas.Canvas,
    text: str,
    x: float,
    y: float,
    w: float,
    font: str = FONT_TC,
    size: float = 30,
    leading: float | None = None,
    fill=PALETTE["ink"],
    max_lines: int | None = None,
) -> float:
    if leading is None:
        leading = size * 1.28
    set_font(c, font, size, fill)
    lines: list[str] = []
    for raw in text.split("\n"):
        if raw.strip() == "":
            lines.append("")
        else:
            lines.extend(wrap_line(raw, font, size, w))
    if max_lines is not None:
        lines = lines[:max_lines]
    yy = y
    for line in lines:
        c.drawString(x, yy, line)
        yy -= leading
    return yy


def draw_hand_line(c: canvas.Canvas, x1, y1, x2, y2, color=PALETTE["ink"], width=3.0, wiggle=9):
    random.seed(int(x1 * 13 + y1 * 7 + x2 * 3 + y2))
    c.setStrokeColor(color)
    c.setLineWidth(width)
    p = c.beginPath()
    p.moveTo(x1, y1)
    mx1 = x1 + (x2 - x1) * 0.35 + random.uniform(-wiggle, wiggle)
    my1 = y1 + (y2 - y1) * 0.35 + random.uniform(-wiggle, wiggle)
    mx2 = x1 + (x2 - x1) * 0.70 + random.uniform(-wiggle, wiggle)
    my2 = y1 + (y2 - y1) * 0.70 + random.uniform(-wiggle, wiggle)
    p.curveTo(mx1, my1, mx2, my2, x2, y2)
    c.drawPath(p)


def draw_arrow(c: canvas.Canvas, x1, y1, x2, y2, color=PALETTE["ink"], width=4):
    draw_hand_line(c, x1, y1, x2, y2, color, width)
    angle = math.atan2(y2 - y1, x2 - x1)
    length = 32
    spread = 0.45
    p1 = (x2 - length * math.cos(angle - spread), y2 - length * math.sin(angle - spread))
    p2 = (x2 - length * math.cos(angle + spread), y2 - length * math.sin(angle + spread))
    c.setStrokeColor(color)
    c.setLineWidth(width)
    c.line(x2, y2, p1[0], p1[1])
    c.line(x2, y2, p2[0], p2[1])


def draw_card(c, x, y, w, h, title, body, accent, footer=None, font_size=27):
    c.saveState()
    c.setFillColor(lighten(accent, 0.82))
    c.setStrokeColor(accent)
    c.setLineWidth(3)
    c.roundRect(x, y, w, h, 8, fill=1, stroke=1)
    c.setFillColor(accent)
    c.roundRect(x, y + h - 72, w, 72, 8, fill=1, stroke=0)
    set_font(c, FONT_TC, 34, colors.white)
    c.drawString(x + 34, y + h - 49, title)
    yy = y + h - 112
    for item in body:
        bullet = "- "
        yy = draw_wrapped(c, bullet + item, x + 36, yy, w - 72, FONT_TC, font_size, font_size * 1.28)
        yy -= 8
    if footer:
        c.setStrokeColor(accent)
        c.setLineWidth(2)
        c.line(x + 30, y + 75, x + w - 30, y + 75)
        draw_wrapped(c, footer, x + 36, y + 45, w - 72, FONT_TC, 23, 28, PALETTE["muted"], max_lines=2)
    c.restoreState()


def draw_sticky(c, x, y, w, h, title, body, fill_color, angle=0):
    c.saveState()
    c.translate(x + w / 2, y + h / 2)
    c.rotate(angle)
    c.translate(-w / 2, -h / 2)
    c.setFillColor(fill_color)
    c.setStrokeColor(colors.HexColor("#B9A94A"))
    c.setLineWidth(2)
    c.rect(0, 0, w, h, fill=1, stroke=1)
    set_font(c, FONT_TC, 24, PALETTE["ink"])
    c.drawString(24, h - 38, title)
    draw_wrapped(c, body, 24, h - 76, w - 48, FONT_TC, 21, 27, PALETTE["ink"], max_lines=5)
    c.restoreState()


def draw_center_node(c, level, title, subtitle, core):
    x, y, w, h = 1510, 1020, 1180, 620
    c.setFillColor(colors.white)
    c.setStrokeColor(PALETTE["ink"])
    c.setLineWidth(5)
    c.roundRect(x, y, w, h, 8, fill=1, stroke=1)
    c.setFillColor(lighten(PALETTE["blue"], 0.65))
    c.rect(x + 20, y + 20, w - 40, h - 40, fill=1, stroke=0)
    set_font(c, FONT_HAND, 66, PALETTE["ink"])
    c.drawString(x + 54, y + h - 86, f"Level {level:02d}")
    draw_hand_line(c, x + 50, y + h - 100, x + 420, y + h - 112, PALETTE["ink"], 3)
    draw_wrapped(c, title, x + 54, y + h - 160, w - 108, FONT_TC, 52, 66)
    draw_wrapped(c, subtitle, x + 58, y + h - 250, w - 116, FONT_TC, 28, 36, PALETTE["muted"], max_lines=2)
    c.setFillColor(colors.white)
    c.setStrokeColor(PALETTE["orange"])
    c.setLineWidth(3)
    c.roundRect(x + 60, y + 70, w - 120, 210, 6, fill=1, stroke=1)
    draw_wrapped(c, core, x + 92, y + 226, w - 184, FONT_CODE, 26, 34, PALETTE["ink"], max_lines=5)
    return x, y, w, h


def draw_page_background(c, page_no, total, module_name):
    c.setFillColor(PALETTE["paper"])
    c.rect(0, 0, PAGE_W, PAGE_H, fill=1, stroke=0)
    c.setStrokeColor(colors.HexColor("#EEEAE0"))
    c.setLineWidth(1)
    for x in range(180, PAGE_W, 180):
        c.line(x, 120, x, PAGE_H - 120)
    for y in range(140, PAGE_H, 160):
        c.line(120, y, PAGE_W - 120, y)
    set_font(c, FONT_TC, 26, PALETTE["muted"])
    c.drawString(140, PAGE_H - 92, "FrameCore v2 whiteboard course")
    c.drawRightString(PAGE_W - 140, PAGE_H - 92, f"{module_name}  |  page {page_no}/{total}")
    c.setStrokeColor(PALETTE["ink"])
    c.setLineWidth(2)
    c.line(140, PAGE_H - 112, PAGE_W - 140, PAGE_H - 112)


LESSONS = [
    {
        "module": "Foundation",
        "title": "狀態向量與座標契約",
        "subtitle": "把 FrameModel 轉成求解器能理解的 6N 維狀態。",
        "core": "q_node=[Ux Uy Uz Rx Ry Rz]^T\n"
        "gdof(i,d)=6*i+d\n"
        "q_l = T q_g,  T=diag(R,R,R,R)\n"
        "K_g = T^T K_l T",
        "theory": [
            "Node 是 6 DOF 邊界狀態，不是單純座標點。",
            "Member 是 [node i:6][node j:6] 的 12 DOF 元素。",
            "localAxes 的 row 是 local basis，所以 v_l=R v_g。",
        ],
        "code": ["FrameTypes.h: Dof enum, gdof()", "Node.h: fixed/prescribed", "Member.h: matIdx/secIdx/refVec", "ElementStiffness.cpp: localAxes(), transform12()"],
        "derive": ["L=||p_j-p_i||", "xhat=(p_j-p_i)/L", "zhat=normalize(xhat x ref)", "yhat=zhat x xhat"],
        "verify": ["手算 refVec=(0,0,1) 的 X 向梁", "檢查 global -Z 變成 local -Y", "不要只檢查 norm, 要檢查方向"],
        "task": "Clone task: 寫 Vec3, gdof, localAxes, transform12, 並建立一個 2-node cantilever model。",
    },
    {
        "module": "Linear Core",
        "title": "3-D 梁柱局部剛度矩陣",
        "subtitle": "把 axial, torsion, bending-y, bending-z 組成 12x12 K_l。",
        "core": "EA/L, GJ/L\n"
        "az=12 E Iz/L^3, bz=6 E Iz/L^2\n"
        "cz=4 E Iz/L, dz=2 E Iz/L\n"
        "ay/by/cy/dy 同理但對 Iy",
        "theory": ["軸向 block 只耦合 u_i,u_j。", "扭轉 block 只耦合 rx_i,rx_j。", "local z bending 用 v/rz；local y bending 用 w/ry。"],
        "code": ["ElementStiffness.cpp: localStiffness12()", "SolveOptions.useTimoshenko", "Section: A, Iy, Iz, J, Asy, Asz"],
        "derive": ["Euler-Bernoulli: v'''' = q/(EI)", "端力由 Hermite shape function 導出", "Timoshenko Phi=12EI/(G A_s L^2)"],
        "verify": ["cantilever tip: delta=PL^3/(3EI)", "torsion: theta=TL/(GJ)", "slender limit: Timoshenko -> EB"],
        "task": "Clone task: 先只實作 EB beam 的 axial + one bending plane，再擴成完整 12x12。",
    },
    {
        "module": "Linear Core",
        "title": "稀疏組裝與邊界條件",
        "subtitle": "把每個元素的 K_g scatter 到全域 K，再縮減成 K_ff。",
        "core": "K = sum_e A_e^T K_e A_e\n"
        "[Kff Kfc; Kcf Kcc][qf; qc]=[Ff; Fc]\n"
        "Kff qf = Ff - Kfc qc\n"
        "R = K q - F",
        "theory": ["assembly 是局部矩陣到全域矩陣的 index scatter。", "fixed flag 決定 free map；prescribed value 在 RHS 進場。", "reactions 不能從求解 RHS 猜，要用完整 Kq-F。"],
        "code": ["FrameSolver.cpp: assembleAndFactor()", "reduceFF(), fmap", "solveLoad(): Ff -= Kfc * prescribed", "SolveResult.reactions"],
        "derive": ["free/constrained partition", "Kff factorization", "backfill q_f and q_c"],
        "verify": ["fully constrained 要報 singular", "settlement q_c != 0 時 RHS 必須改變", "factorize-once 與 monolithic 解要一致"],
        "task": "Clone task: 寫 triplet assembly 和 free-map，不急著做 shells。",
    },
    {
        "module": "Verification",
        "title": "機構偵測與驗證紀律",
        "subtitle": "力學引擎不是產生數字，而是拒絕錯誤模型。",
        "core": "LDLT(Kff) -> D pivots\n"
        "pivotMargin = min(|D_i|)/max(|D_i|)\n"
        "singular if |D_i| < tol*maxD or D_i < 0",
        "theory": ["拓樸連通不等於剛度穩定。", "near-zero pivot 是機構或約束不足的證據。", "verification 要有獨立 oracle，不是重跑自己。"],
        "code": ["FrameSolver.cpp: pivot scan", "SolveOptions.pivotTol", "VERIFICATION.md: fixture map", "Standalone/main.cpp: analytic gates"],
        "derive": ["positive definite/semi-definite 的差異", "LDLT 的 D pivot 意義", "condition warning vs hard mechanism"],
        "verify": ["F3/F7 類機構模型必須拒絕", "旋轉不變性: u' = R u", "no-op flag 必須 bit-identical"],
        "task": "Clone task: 在 clone solver 裡加入 pivot guard，錯誤模型回傳 diagnostic。",
    },
    {
        "module": "Loads",
        "title": "等效節點力與端釋放",
        "subtitle": "把 distributed load 變成 Qf，並正確凝聚 released DOF。",
        "core": "Q = K_l q_l + Qf\n"
        "p_eq = -T^T Qf\n"
        "K* = Krr - Krc Kcc^-1 Kcr\n"
        "Qf* = Qfr - Krc Kcc^-1 Qfc",
        "theory": ["UDL 在 local frame 定義。", "fixed-end force 與 equivalent nodal load 符號相反。", "釋放 DOF 時 K 和 Qf 必須一起凝聚。"],
        "code": ["BeamColumnElement.cpp: Qf_ from MemberUDL", "condenseReleases()", "ReleasePreset::HingeI/HingeJ/TrussPin"],
        "derive": ["local y UDL: Vy=wL/2, Mz=wL^2/12", "local z UDL: Vz=wL/2, My=-wL^2/12", "Schur complement"],
        "verify": ["propped cantilever release oracle", "雙端 torsion release 要報 mechanism", "loaded hinge 不可出現 phantom moment"],
        "task": "Clone task: 先支援 HingeJ 的 Ry/Rz release，寫一個含 UDL 的回歸測試。",
    },
    {
        "module": "Strength",
        "title": "材料、截面與彈性強度篩選",
        "subtitle": "把 end forces 轉成 axial, bending, shear, torsion utilization。",
        "core": "sigma_N = N/A\n"
        "sigma_M = |My|/Wy + |Mz|/Wz  (rect)\n"
        "tau = k sqrt(Vy^2+Vz^2)/A\n"
        "risk = max(demand/capacity)",
        "theory": ["FrameCore 的 D/C 是 elastic allowable screen。", "compression-positive N 是引擎慣例。", "rectangle 用角點保守和；circle 用 resultant。"],
        "code": ["Material.h: E, G, rho, fy, Capacity", "Section.h: A, Iy, Iz, J, Wy(), Wz()", "ElasticAllowable.cpp"],
        "derive": ["W=I/c", "rectangle: Zy=d*b^2/4, Zz=b*d^2/4", "circle: J=2I"],
        "verify": ["biaxial D/C 手算", "torsion circular exact", "capacity=0 under demand -> infinity risk"],
        "task": "Clone task: 實作 checkSection(endForces, section, cap)，只回傳 risk 與 governing mode。",
    },
    {
        "module": "Dynamics",
        "title": "質量矩陣、模態與 Newmark",
        "subtitle": "從 Kq=F 進入 M qdd + C qd + K q = F(t)。",
        "core": "K phi = omega^2 M phi\n"
        "q(t)=Phi eta(t)\n"
        "eta'' + 2 zeta omega eta' + omega^2 eta = p(t)\n"
        "Newmark beta=1/4, gamma=1/2",
        "theory": ["consistent mass 保留 bending shape function 的質量分布。", "模態座標把多 DOF 系統拆成 SDOF。", "dynamic collapse 在 modal space 事件間積分。"],
        "code": ["localMass12()", "ModalAnalysis.h", "ModalDynamics.h", "DynamicCollapse.h"],
        "derive": ["rho kg/m^3 -> tonne/mm^3: *1e-12", "M-orthonormal: Phi^T M Phi=I", "C proportional damping"],
        "verify": ["cantilever omega_n analytic", "OpenSees eigen -cMass", "full-basis Ritz == pure modes"],
        "task": "Clone task: 產出 axial bar 的 lumped mass，再升級到 EB consistent mass。",
    },
    {
        "module": "Stability",
        "title": "線性屈曲與 P-Delta",
        "subtitle": "軸力改變 bending tangent，壓縮會削弱側向剛度。",
        "core": "K_T = K_e + K_g(P)\n"
        "P tension-positive; compression -> P=-N\n"
        "buckling: (-K_g) phi = lambda K phi\n"
        "P-Delta: K u_{k+1}=F - K_g u_k",
        "theory": ["幾何剛度是 stress stiffening。", "P-Delta 是 Theory-II small-sway 線性化。", "超過 Pcr 應報 diverged，不應回傳假解。"],
        "code": ["localGeometric12()", "BucklingAnalysis.cpp", "PDeltaAnalysis.cpp", "SolveOptions.shellGeometricStiffness"],
        "derive": ["beam-column differential equation", "Euler Pcr=pi^2 EI/L^2", "frozen pseudo-load iteration"],
        "verify": ["Euler column load", "P=0 bit-identical to linear", "0.95Pcr finite, 1.05Pcr diverged"],
        "task": "Clone task: 對單柱建立 K + Kg，掃描 P 並找 det(KT) 近零點。",
    },
    {
        "module": "Shell",
        "title": "MITC4 flat shell 基礎",
        "subtitle": "4-node facet, 24 DOF, membrane + bending + assumed shear + drilling。",
        "core": "K_shell = integral(Bm^T Dm Bm t)dA\n"
        "+ integral(Bb^T Db Bb)dA\n"
        "+ MITC shear + drilling penalty\n"
        "DOF per node: [u v w rx ry rz]",
        "theory": ["Reissner-Mindlin shell 允許 transverse shear。", "MITC tying points 避免薄板 shear locking。", "flat facet 不是曲面元素，曲面靠 refine 收斂。"],
        "code": ["MITC4ShellElement.cpp", "Shell.h: ShellQuad", "SolveResult.shellForces", "ShellPressure"],
        "derive": ["plane stress Dm", "plate bending Db", "gamma_xz, gamma_yz assumed shear"],
        "verify": ["membrane patch test", "clamped plate deflection", "OpenSees ShellMITC4 comparison"],
        "task": "Clone task: 先做 Q4 membrane patch，再加 plate bending，不急著做 MITC shear。",
    },
    {
        "module": "Shell",
        "title": "Shell 升級: QM6, DKQ, EICR",
        "subtitle": "解 locking, 薄板快路徑，以及大位移 facet rigid rotation。",
        "core": "QM6: incompatible membrane modes\n"
        "DKQ: discrete Kirchhoff thin plate\n"
        "EICR: remove rigid rotation, small strain\n"
        "default-off must be bit-identical",
        "theory": ["QM6 降低 in-plane bending locking。", "DKQ 是 thin plate path，沒有 transverse shear。", "EICR shell 移除剛體轉動，不消除 flat facet 誤差。"],
        "code": ["SolveOptions.useIncompatibleMembrane", "SolveOptions.useDKQPlate", "SolveOptions.shellCorotational", "docs/specs/S8_shell.md"],
        "derive": ["weak patch test", "Kirchhoff constraint", "corotational local frame update"],
        "verify": ["Cook membrane", "ShellDKGQ comparison", "rigid rotation invariance ~1e-14"],
        "task": "Clone task: 寫一個 default-off feature flag，證明 off 時 output bit-identical。",
    },
    {
        "module": "Reanalysis",
        "title": "PreparedSystem 與 ReSolve ladder",
        "subtitle": "互動式結構編輯靠重用 factorization，而不是每次重算一切。",
        "core": "PreparedSystem = K, free map, LDLT, prepared elements\n"
        "Tier1: Woodbury\n"
        "(A+U C V)^-1 = A^-1 - A^-1 U(... )V A^-1\n"
        "Tier2: stale-LDLT PCG; Tier3: rebaseline",
        "theory": ["幾何/拓樸/支承 flags 改變會使 factorization 失效。", "nodal loads 與 prescribed values 可重用 solveLoad。", "Tier-1 formula-exact 但不 bit-identical。"],
        "code": ["FrameSolver.h: PreparedSystem", "solveLoad reuse fingerprint", "Reanalysis.h/.cpp", "ModelHash.h"],
        "derive": ["Woodbury rank-k update", "PCG residual criterion", "fallback correctness"],
        "verify": ["solve() == assembleAndFactor()+solveLoad()", "fresh factor reference", "stale model fingerprint must reject"],
        "task": "Clone task: 實作 factorize-once solve-many，再加 model fingerprint guard。",
    },
    {
        "module": "Nonlinear Active Set",
        "title": "Tension-only active-set 求解",
        "subtitle": "纜索/細斜撐在壓縮下退出，拉伸時再回來。",
        "core": "if N_compression > 0: deactivate\n"
        "if axial elongation > 0: reactivate\n"
        "iterate active set until fixed point\n"
        "cycle guard -> monotone fallback",
        "theory": ["tension-only flag 不改普通 solve 行為。", "active set 是離散非線性，不是單次線性 solve。", "循環要被偵測，不能無限迭代。"],
        "code": ["Member.tensionOnly", "TensionOnly.cpp", "ReSolveSession inner solves", "transition-hash cycle guard"],
        "derive": ["axial elongation: (u_j-u_i).xhat", "compression-positive N", "finite termination bound"],
        "verify": ["converged == omit slack members", "monotone fallback <= nTO+1", "fingerprint includes tensionOnly"],
        "task": "Clone task: 對 X-brace 框架寫 active-set loop，印出每輪 active mask。",
    },
    {
        "module": "Collapse",
        "title": "Progressive collapse 與塑性鉸",
        "subtitle": "事件到事件的 sequential linear analysis，不是假裝材料非線性。",
        "core": "solve -> screen utilization -> remove/yield event\n"
        "hinge: release bending DOF + residual Mp\n"
        "Mp = fy Z\n"
        "Mp_eff(N)=Mp max(0,1-N/Ny)^2",
        "theory": ["LSP collapse 是線性事件序列。", "塑性鉸是 formed state，不是 fiber section。", "S10 N-M interaction 只是一軸 axial reduction。"],
        "code": ["Collapse.h/.cpp", "Hinge.h", "NMInteraction.h", "PlasticHinge records", "Member.active"],
        "derive": ["collapse load w*=16Mp/L^2", "hinge residual moment carry-over", "neutral axis shift for rectangle"],
        "verify": ["OpenSees formed hinge comparison", "0.99w* stable, 1.01w* collapsed", "off flag bit-identical"],
        "task": "Clone task: 寫 single-span beam 的 event-to-event hinge driver。",
    },
    {
        "module": "Dynamic Collapse",
        "title": "Dynamic collapse 與碎片交接",
        "subtitle": "FrameCore 算結構事件；Chaos 或剛體後端負責落下接觸。",
        "core": "M u'' + C u' + K u = F\n"
        "event removal -> fresh modal basis\n"
        "q' = Phi'^T M' u\n"
        "fragment vel=p/m, angVel=I^-1 L",
        "theory": ["事件間用 modal Newmark。", "跨事件要把 u,v 投影到新基底。", "FragmentCluster 是 rigid-body handoff，不是 contact solver。"],
        "code": ["DynamicCollapse.cpp", "FragmentMomentum.h", "Connectivity.cpp", "FragmentCluster mass/com/inertia"],
        "derive": ["linear momentum p=sum m_i v_i", "angular momentum L=sum r_i x m_i v_i", "M-orthonormal projection"],
        "verify": ["momentum closure", "full-system Newmark audit", "detached fragments id-sorted deterministic"],
        "task": "Clone task: 對斷裂後兩節點 fragment 算 mass center 與 linear velocity。",
    },
    {
        "module": "Large Displacement",
        "title": "Co-rotational 大位移 beam",
        "subtitle": "小應變、大旋轉：每根元素追蹤自己的剛體 frame。",
        "core": "R_e removes rigid rotation\n"
        "local deformation = current - rigid motion\n"
        "Newton: K_t delta u = residual\n"
        "arc-length: ||delta u||^2 + alpha delta lambda^2 = ds^2",
        "theory": ["co-rotational 不是 total Lagrangian 實作。", "SO(3) 有限轉角要正確 compose。", "snap-through 需要 arc-length path following。"],
        "code": ["CorotationalAnalysis.cpp", "runCorotational()", "useArcLength", "finite-difference tangent"],
        "derive": ["rotation vector / exponential map", "residual equilibrium", "Crisfield cylindrical constraint"],
        "verify": ["rigid rotation invariance", "Mattiasson elastica", "OpenSees Corotational comparison"],
        "task": "Clone task: 寫 planar corotational cantilever，先只處理 2D bending。",
    },
    {
        "module": "Optimization",
        "title": "FSD 尺寸最佳化與 BESO 拓樸",
        "subtitle": "把 solver 變成設計迭代器，但不偽稱全域最佳。",
        "core": "FSD: A_{k+1}=A_k * risk^eta\n"
        "sensitivity = element strain energy\n"
        "U_e = 1/2 u_e^T K_e u_e\n"
        "BESO: remove low sensitivity under volume target",
        "theory": ["FSD 對靜定 truss 最接近解析 optimum。", "固定端框架有 bending，所以會比 pin-joint literature 重。", "BESO 是 heuristic，需 mechanism guard。"],
        "code": ["SizeOpt.cpp", "Topology.cpp", "memberStrainEnergy()", "N2 collapse robustness option"],
        "derive": ["stress ratio resizing", "compliance derivative", "history averaging"],
        "verify": ["10-bar truss literature", "energy balance", "candidate topology re-solves non-singular"],
        "task": "Clone task: 對 3-member toy truss 做一輪 FSD，再用 energy 排序刪 member。",
    },
    {
        "module": "Bridge",
        "title": "CLI, C API 與外部客戶端",
        "subtitle": "把核心 solver 包成可被 Grasshopper/UE/工具鏈呼叫的穩定邊界。",
        "core": "public API: POD only\n"
        "frame_cli text protocol\n"
        "frame_capi_solve_text()\n"
        "daemon: many requests, one process",
        "theory": ["public boundary 不洩漏 Eigen 或 Unreal。", "文字協定適合黑箱 oracle 與跨語言。", "bridge 必須 bit-identical to core path。"],
        "code": ["Standalone/frame_cli.cpp", "frame_cli_core.cpp", "frame_capi.cpp", "docs/CLI_PROTOCOL.md", "Grasshopper/FrameCoreClient.cs"],
        "derive": ["serialization contract", "id-stamped result rows", "error diagnostic propagation"],
        "verify": ["CLI round-trip", "daemon == independent runs", "C API == CLI bit-identical"],
        "task": "Clone task: 寫最小 TEXT protocol: MAT, SEC, NODE, MEMBER, NLOAD, END。",
    },
    {
        "module": "Rigid Body Backend",
        "title": "碰撞、衝量與約束求解器補課",
        "subtitle": "這是碎片離開 FrameCore 後的世界：剛體、接觸、摩擦、約束。",
        "core": "impulse: J = -(1+e) v_rel / (n^T M_eff^-1 n)\n"
        "v' = v + M^-1 J^T lambda\n"
        "constraint: J v + b = 0\n"
        "PGS/SI solves lambda with bounds",
        "theory": ["碰撞檢測產生 contact manifold。", "衝量改速度，不直接改位置。", "joint/contact constraint 都可寫成 Jacobian row。"],
        "code": ["FrameCore handoff: FragmentCluster", "UE Chaos owns rigid fall/contact", "custom backend: broadphase, narrowphase, solver island"],
        "derive": ["relative velocity at contact", "effective mass", "Coulomb friction cone approximation"],
        "verify": ["momentum conservation when e=1", "no penetration drift with Baumgarte/split impulse", "stacking stability test"],
        "task": "Clone task: 寫 2D circle-plane impulse solver，再把 FragmentCluster 當 rigid body input。",
    },
]


def draw_lesson_page(c, idx, data, total_lessons):
    draw_page_background(c, idx, total_lessons, data["module"])
    cx, cy, cw, ch = draw_center_node(c, idx, data["title"], data["subtitle"], data["core"])

    # Learning path line.
    draw_arrow(c, 260, 1290, cx - 40, cy + ch / 2, PALETTE["purple"], 5)
    draw_arrow(c, cx + cw + 40, cy + ch / 2, PAGE_W - 280, 1290, PALETTE["purple"], 5)
    set_font(c, FONT_HAND, 42, PALETTE["purple"])
    c.drawString(205, 1340, "entry" if idx == 1 else "previous")
    c.drawRightString(PAGE_W - 205, 1340, "clone complete" if idx == total_lessons else "next unlock")

    cards = [
        (180, 1680, 1130, 650, "理論核", data["theory"], PALETTE["blue"], "先能手算，再談程式。"),
        (2880, 1665, 1140, 665, "程式落點", data["code"], PALETTE["orange"], "clone 時只保留必要接口。"),
        (190, 420, 1160, 700, "推導分支", data["derive"], PALETTE["green"], "把公式接到資料結構。"),
        (2860, 430, 1160, 690, "驗證關卡", data["verify"], PALETTE["pink"], "每個能力都要有 oracle。"),
    ]
    for x, y, w, h, title, body, accent, footer in cards:
        draw_card(c, x, y, w, h, title, body, accent, footer)

    # Arrows from center to cards.
    draw_arrow(c, cx + 70, cy + ch - 40, 1210, 1960, PALETTE["blue"], 4)
    draw_arrow(c, cx + cw - 70, cy + ch - 40, 2910, 1960, PALETTE["orange"], 4)
    draw_arrow(c, cx + 70, cy + 70, 1260, 850, PALETTE["green"], 4)
    draw_arrow(c, cx + cw - 70, cy + 70, 2910, 850, PALETTE["pink"], 4)

    # Clone mission strip.
    c.setFillColor(colors.white)
    c.setStrokeColor(PALETTE["ink"])
    c.setLineWidth(3)
    c.roundRect(1420, 250, 1360, 570, 8, fill=1, stroke=1)
    set_font(c, FONT_HAND, 48, PALETTE["ink"])
    c.drawString(1480, 745, "Clone Mission")
    draw_hand_line(c, 1478, 730, 1835, 716, PALETTE["ink"], 3)
    draw_wrapped(c, data["task"], 1480, 670, 1240, FONT_TC, 30, 40)

    # Sticky notes.
    draw_sticky(c, 1520, 1780, 390, 290, "手算", "本頁至少選一條公式完整展開到矩陣維度。", lighten(PALETTE["yellow"], 0.25), -3)
    draw_sticky(c, 2300, 1745, 420, 300, "禁止跳步", "任何黑箱 API 都要能對應回 K, M, F 或狀態向量。", lighten(PALETTE["cyan"], 0.35), 4)

    # Bottom progress rail.
    rail_x, rail_y, rail_w = 240, 160, PAGE_W - 480
    c.setStrokeColor(PALETTE["gray"])
    c.setLineWidth(8)
    c.line(rail_x, rail_y, rail_x + rail_w, rail_y)
    for k in range(1, total_lessons + 1):
        x = rail_x + rail_w * (k - 1) / (total_lessons - 1)
        c.setFillColor(PALETTE["purple"] if k <= idx else colors.white)
        c.setStrokeColor(PALETTE["purple"])
        c.circle(x, rail_y, 22, fill=1, stroke=1)
        if k in (1, idx, total_lessons):
            set_font(c, FONT_TC, 20, PALETTE["muted"])
            c.drawCentredString(x, rail_y - 55, str(k))


def draw_overview(c):
    draw_page_background(c, 0, len(LESSONS), "Course Map")
    set_font(c, FONT_HAND, 90, PALETTE["ink"])
    c.drawString(210, PAGE_H - 255, "FrameCore Clone Quest")
    draw_hand_line(c, 210, PAGE_H - 275, 1190, PAGE_H - 300, PALETTE["ink"], 4)
    draw_wrapped(
        c,
        "目標：從資料結構、矩陣推導、稀疏求解、驗證紀律，一路擴散到崩塌、非線性、最佳化、外部橋接與剛體後端。",
        220,
        PAGE_H - 360,
        1800,
        FONT_TC,
        34,
        46,
        PALETTE["ink"],
    )

    # Central target.
    c.setFillColor(lighten(PALETTE["blue"], 0.58))
    c.setStrokeColor(PALETTE["ink"])
    c.setLineWidth(5)
    c.roundRect(1600, 980, 1000, 560, 8, fill=1, stroke=1)
    draw_wrapped(c, "Clone FrameCore v2", 1700, 1375, 800, FONT_TC, 62, 74)
    draw_wrapped(c, "C++17 + Eigen\nDirect-stiffness FEM\nVerified structural engine", 1710, 1265, 780, FONT_CODE, 30, 38)

    cluster_defs = [
        ("Foundation", [1, 2, 3, 4, 5, 6], PALETTE["blue"], 520, 1540),
        ("Dynamics + Stability", [7, 8], PALETTE["green"], 2760, 1650),
        ("Shell", [9, 10], PALETTE["orange"], 2860, 850),
        ("Nonlinear + Collapse", [11, 12, 13, 14, 15], PALETTE["pink"], 560, 650),
        ("Optimization + Bridge", [16, 17, 18], PALETTE["purple"], 1810, 340),
    ]

    for label, nums, col, x0, y0 in cluster_defs:
        c.setFillColor(colors.white)
        c.setStrokeColor(col)
        c.setLineWidth(4)
        c.roundRect(x0 - 80, y0 - 80, 980, 520, 8, fill=1, stroke=1)
        set_font(c, FONT_HAND, 48, col)
        c.drawString(x0, y0 + 370, label)
        draw_hand_line(c, x0, y0 + 354, x0 + 430, y0 + 340, col, 3)
        prev = None
        for i, num in enumerate(nums):
            px = x0 + 115 + (i % 3) * 245
            py = y0 + 245 - (i // 3) * 150
            c.setFillColor(lighten(col, 0.65))
            c.setStrokeColor(col)
            c.setLineWidth(3)
            c.circle(px, py, 58, fill=1, stroke=1)
            set_font(c, FONT_TC, 28, PALETTE["ink"])
            c.drawCentredString(px, py - 9, str(num))
            if prev:
                draw_arrow(c, prev[0] + 58, prev[1], px - 58, py, col, 3)
            prev = (px, py)
        draw_arrow(c, x0 + 400, y0 + 130, 2050, 1260, col, 3)

    draw_sticky(c, 2800, 360, 520, 360, "Freeform 用法", "匯入 PDF 後，每頁就是一張大白板。先沿主線走，再把每個分支往外手寫擴張。", lighten(PALETTE["yellow"], 0.2), -2)
    draw_sticky(c, 3420, 360, 500, 360, "讀法", "每一關只問三件事：數學是什麼？程式接口在哪？oracle 怎麼證明？", lighten(PALETTE["cyan"], 0.35), 3)


def render_pdf_to_pngs(pdf_path: Path, out_dir: Path) -> None:
    import fitz

    out_dir.mkdir(parents=True, exist_ok=True)
    doc = fitz.open(str(pdf_path))
    matrix = fitz.Matrix(0.45, 0.45)
    for i, page in enumerate(doc):
        pix = page.get_pixmap(matrix=matrix, alpha=False)
        pix.save(str(out_dir / f"page_{i:02d}.png"))


def build_pdf() -> None:
    register_fonts()
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    c = canvas.Canvas(str(PDF_PATH), pagesize=(PAGE_W, PAGE_H))
    c.setTitle("FrameCore v2 Whiteboard Course")
    c.setAuthor("Codex")
    draw_overview(c)
    c.showPage()
    for idx, lesson in enumerate(LESSONS, start=1):
        draw_lesson_page(c, idx, lesson, len(LESSONS))
        c.showPage()
    c.save()
    render_pdf_to_pngs(PDF_PATH, PNG_DIR)


if __name__ == "__main__":
    build_pdf()
    print(PDF_PATH)
    print(PNG_DIR)
