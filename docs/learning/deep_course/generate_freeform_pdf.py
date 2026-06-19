from __future__ import annotations

import math
import re
from dataclasses import dataclass
from pathlib import Path

from reportlab.lib import colors
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfgen import canvas


ROOT = Path(__file__).resolve().parents[3]
COURSE = Path(__file__).resolve().parent
OUT_DIR = ROOT / "output" / "pdf" / "deep_course_freeform"
PDF_PATH = OUT_DIR / "framecore_v2_deep_course_freeform.pdf"
PNG_DIR = OUT_DIR / "png_pages"

# A large whiteboard canvas. Freeform can zoom; normal PDF viewers can still pan/zoom.
PAGE_W = 7200
PAGE_H = 4800

FONT = "MicrosoftYaHei"
FONT_HAND = "InkFree"
FONT_CODE = "Courier"


def register_fonts() -> None:
    pdfmetrics.registerFont(TTFont(FONT, r"C:\Windows\Fonts\msyh.ttc"))
    pdfmetrics.registerFont(TTFont(FONT_HAND, r"C:\Windows\Fonts\Inkfree.ttf"))


INK = colors.HexColor("#161616")
MUTED = colors.HexColor("#5D646B")
PAPER = colors.HexColor("#FBFAF6")
GRID = colors.HexColor("#EEE9DE")
BLUE = colors.HexColor("#36A4D8")
ORANGE = colors.HexColor("#F49A2E")
PINK = colors.HexColor("#E8529B")
GREEN = colors.HexColor("#39AA74")
PURPLE = colors.HexColor("#8465D6")
YELLOW = colors.HexColor("#F4D54A")
CYAN = colors.HexColor("#45C2C6")
RED = colors.HexColor("#DD4B4B")


PALETTES = [BLUE, ORANGE, PINK, GREEN, PURPLE, CYAN, RED]


def pale(c, amount=0.82):
    return colors.Color(c.red + (1 - c.red) * amount, c.green + (1 - c.green) * amount, c.blue + (1 - c.blue) * amount)


def tw(text: str, font: str, size: float) -> float:
    return pdfmetrics.stringWidth(text, font, size)


def setf(c: canvas.Canvas, font=FONT, size=32, fill=INK) -> None:
    c.setFont(font, size)
    c.setFillColor(fill)


def wrap_text(text: str, font: str, size: float, max_w: float) -> list[str]:
    text = text.replace("\t", "    ")
    out: list[str] = []
    for para in text.splitlines():
        if not para.strip():
            out.append("")
            continue
        line = ""
        for ch in para:
            cand = line + ch
            if tw(cand, font, size) <= max_w:
                line = cand
            else:
                if line:
                    out.append(line.rstrip())
                line = ch.strip()
        if line:
            out.append(line.rstrip())
    return out


def draw_wrapped(c, text, x, y, w, font=FONT, size=30, leading=None, fill=INK, max_lines=None) -> float:
    if leading is None:
        leading = size * 1.28
    setf(c, font, size, fill)
    lines = wrap_text(text, font, size, w)
    if max_lines is not None and len(lines) > max_lines:
        lines = lines[: max(0, max_lines - 1)] + ["..."]
    yy = y
    for line in lines:
        c.drawString(x, yy, line)
        yy -= leading
    return yy


def draw_bg(c: canvas.Canvas, footer: str) -> None:
    c.setFillColor(PAPER)
    c.rect(0, 0, PAGE_W, PAGE_H, fill=1, stroke=0)
    c.setStrokeColor(GRID)
    c.setLineWidth(1)
    for x in range(180, PAGE_W, 180):
        c.line(x, 120, x, PAGE_H - 120)
    for y in range(180, PAGE_H, 180):
        c.line(120, y, PAGE_W - 120, y)
    setf(c, FONT, 30, MUTED)
    c.drawString(170, PAGE_H - 82, "FrameCore v2 deep course / Freeform edition")
    c.drawRightString(PAGE_W - 170, PAGE_H - 82, footer)
    c.setStrokeColor(INK)
    c.setLineWidth(2)
    c.line(160, PAGE_H - 108, PAGE_W - 160, PAGE_H - 108)


def hand_line(c, x1, y1, x2, y2, col=INK, width=4):
    c.setStrokeColor(col)
    c.setLineWidth(width)
    p = c.beginPath()
    p.moveTo(x1, y1)
    p.curveTo((x1 + x2) / 2, y1 + (y2 - y1) * 0.2 + 25, (x1 + x2) / 2, y1 + (y2 - y1) * 0.8 - 25, x2, y2)
    c.drawPath(p)


def arrow(c, x1, y1, x2, y2, col=INK, width=4):
    hand_line(c, x1, y1, x2, y2, col, width)
    ang = math.atan2(y2 - y1, x2 - x1)
    l = 42
    for s in (-0.48, 0.48):
        c.line(x2, y2, x2 - l * math.cos(ang + s), y2 - l * math.sin(ang + s))


def card(c, x, y, w, h, title, body, col, body_size=29, title_size=38, max_lines=18):
    c.setFillColor(colors.white)
    c.setStrokeColor(col)
    c.setLineWidth(4)
    c.roundRect(x, y, w, h, 10, fill=1, stroke=1)
    c.setFillColor(col)
    c.roundRect(x, y + h - 90, w, 90, 10, fill=1, stroke=0)
    setf(c, FONT, title_size, colors.white)
    c.drawString(x + 34, y + h - 60, title)
    yy = y + h - 128
    if isinstance(body, list):
        body = "\n".join(f"- {b}" for b in body)
    return draw_wrapped(c, body, x + 34, yy, w - 68, FONT, body_size, body_size * 1.34, INK, max_lines=max_lines)


def sticky(c, x, y, w, h, title, body, col=YELLOW, rot=-2):
    c.saveState()
    c.translate(x + w / 2, y + h / 2)
    c.rotate(rot)
    c.translate(-w / 2, -h / 2)
    c.setFillColor(pale(col, 0.35))
    c.setStrokeColor(col)
    c.setLineWidth(2)
    c.rect(0, 0, w, h, fill=1, stroke=1)
    setf(c, FONT, 27, INK)
    c.drawString(28, h - 44, title)
    draw_wrapped(c, body, 28, h - 86, w - 56, FONT, 23, 30, INK, max_lines=6)
    c.restoreState()


@dataclass
class LessonDoc:
    no: int
    file: Path
    title: str
    phase: str
    objective: str
    headings: list[str]
    formulas: list[str]
    sources: list[str]
    data: list[str]
    invariants: list[str]
    tests: list[str]
    pitfalls: list[str]
    exercise: str
    freeform: str


def section_between(text: str, start_pat: str, end_pat: str | None = None) -> str:
    s = re.search(start_pat, text, re.M)
    if not s:
        return ""
    start = s.end()
    if end_pat:
        e = re.search(end_pat, text[start:], re.M)
        if e:
            return text[start : start + e.start()]
    return text[start:]


def bullets_from(section: str, limit=10) -> list[str]:
    items = []
    for line in section.splitlines():
        m = re.match(r"\s*[-*]\s+(.+)", line)
        if m:
            item = re.sub(r"`", "", m.group(1)).strip()
            if item and item not in items:
                items.append(item)
        if len(items) >= limit:
            break
    return items


def parse_lesson(path: Path) -> LessonDoc:
    text = path.read_text(encoding="utf-8")
    h1s = [l for l in text.splitlines() if l.startswith("# ")]
    title_line = next((l for l in h1s if re.match(r"#\s*Lesson\s+\d+", l)), h1s[0] if h1s else path.stem)
    title = title_line[2:].strip()
    m = re.match(r"Lesson\s+(\d+)\s*[-—]\s*(.+)", title)
    no = int(m.group(1)) if m else int(re.search(r"lesson_(\d+)", path.name).group(1))
    clean_title = m.group(2).strip() if m else title
    phase = ""
    objective = ""
    for line in text.splitlines()[:25]:
        if "Phase:" in line:
            phase = line.split("Phase:", 1)[1].strip(" >")
        if "Objective:" in line:
            objective = line.split("Objective:", 1)[1].strip(" >")
        if "目標：" in line and not objective:
            objective = line.split("目標：", 1)[1].strip(" >")
    headings = [re.sub(r"^#+\s*", "", l).strip() for l in text.splitlines() if l.startswith("## ")][:20]
    formulas = []
    for block in re.findall(r"\\\[(.*?)\\\]", text, flags=re.S):
        f = " ".join(block.strip().split())
        if f and len(f) < 90 and f not in formulas:
            formulas.append(f)
        if len(formulas) >= 12:
            break
    sources = bullets_from(section_between(text, r"^## 1\. .*源碼|^## 1\. .*源码|^## 1\. 專案對齊", r"^---"), 12)
    data = bullets_from(section_between(text, r"^## 3\. .*資料|^## 3\. .*资料", r"^---"), 12)
    invariants = bullets_from(section_between(text, r"^## 4\. .*不變量|^## 4\. .*不变量", r"^---"), 12)
    tests = bullets_from(section_between(text, r"^## 8\. .*驗證|^## 8\. .*验证", r"^---"), 12)
    pitfalls = bullets_from(section_between(text, r"^## 7\. Debug", r"^---"), 10)
    ex_sec = section_between(text, r"### 題目 3|## 題目 3|### 题目 3|## 題目 3", r"---")
    exercise = re.sub(r"\s+", " ", ex_sec.strip())[:420] if ex_sec else "本課核心實作題，詳見 Markdown 原文。"
    ff = section_between(text, r"^## H\. Freeform", r"^---")
    freeform = re.sub(r"\s+", " ", ff.strip())[:520] if ff else "中央放公式，左側放推導，右側放程式，下方放測試與 Debug。"
    if no == 1:
        phase = "Phase A - 數學與引擎最小地基"
        objective = "建立 DOF、向量/矩陣、局部座標、Transform12 與 K_g=T^T K_l T 的 convention layer。"
        sources = [
            "FrameTypes.h / gdof / Vec3",
            "Node.h / fixed / prescribed",
            "Member.h / refVec / release[12]",
            "ElementStiffness.cpp / localAxes / transform12",
            "BeamColumnElement.cpp / T^T K_l T assembly",
        ]
        data = [
            "Vec3",
            "Mat3",
            "Node",
            "Member",
            "MemberFrame",
            "Vec12 / Mat12",
            "Transform12",
            "DOF map",
        ]
        invariants = [
            "gdof(i,d)=6i+d",
            "R R^T = I",
            "det(R)=+1",
            "q_l = T q_g",
            "K_g = T^T K_l T",
            "q_l^T K_l q_l = q_g^T K_g q_g",
        ]
        tests = [
            "Vec3 dot/cross orthogonality",
            "member frame axis-aligned hand case",
            "refVec parallel fallback",
            "12-vector local/global roundtrip",
            "energy invariance test",
            "wrong transform should fail",
        ]
        pitfalls = [
            "row convention 與 column convention 混用",
            "ey = cross(ex, ez) 導致 det(R)=-1",
            "把 NodeId 當 vector index",
            "使用 T K_l T^T 而不是 T^T K_l T",
            "local force 和 global reaction 混用",
        ]
        exercise = (
            "重構 Mat3，不提供模糊的 rotate(v)；只保留 globalToLocal(v)=R v 與 "
            "localToGlobal(v)=R^T v。替 transformKLocalToGlobal 寫 energy invariance 與 wrong-transform failure test。"
        )
        freeform = (
            "中央放 R、T、K_g 公式；左側展開 Vec3/dot/cross；右側放 Mat3/MemberFrame/Transform12；"
            "下方放 axis-aligned、skew member、energy invariance 三組手算與測試。"
        )
    return LessonDoc(no, path, clean_title, phase, objective, headings, formulas, sources, data, invariants, tests, pitfalls, exercise, freeform)


def load_lessons() -> list[LessonDoc]:
    return [parse_lesson(p) for p in sorted(COURSE.glob("lesson_*.md"))]


def draw_overview(c, lessons: list[LessonDoc]):
    draw_bg(c, "overview")
    setf(c, FONT_HAND, 104, INK)
    c.drawString(230, PAGE_H - 260, "FrameCore v2 Deep Course")
    hand_line(c, 230, PAGE_H - 290, 1450, PAGE_H - 326, INK, 5)
    draw_wrapped(
        c,
        "這是按完整講義密度生成的 Freeform 版本。每課兩頁：Map 是擴散學習路線，Lecture 是密集講義索引。",
        240,
        PAGE_H - 390,
        2350,
        FONT,
        42,
        58,
        INK,
        max_lines=3,
    )
    phases: dict[str, list[LessonDoc]] = {}
    for l in lessons:
        phase = l.phase or "Course"
        phases.setdefault(phase, []).append(l)
    positions = [
        (260, 2820, BLUE),
        (2680, 3050, GREEN),
        (4960, 2880, ORANGE),
        (430, 1160, PINK),
        (2870, 1080, PURPLE),
        (5120, 1220, CYAN),
    ]
    for (phase, group), (x, y, col) in zip(phases.items(), positions):
        c.setStrokeColor(col)
        c.setFillColor(colors.white)
        c.setLineWidth(5)
        c.roundRect(x, y, 1880, 1010, 12, fill=1, stroke=1)
        setf(c, FONT_HAND, 54, col)
        c.drawString(x + 60, y + 915, phase[:42])
        hand_line(c, x + 60, y + 895, x + 720, y + 880, col, 3)
        gx, gy = x + 170, y + 720
        for i, lesson in enumerate(group):
            px = gx + (i % 5) * 310
            py = gy - (i // 5) * 220
            c.setFillColor(pale(col, 0.58))
            c.setStrokeColor(col)
            c.setLineWidth(3)
            c.circle(px, py, 64, fill=1, stroke=1)
            setf(c, FONT, 30, INK)
            c.drawCentredString(px, py - 11, str(lesson.no))
            if i > 0:
                prevx = gx + ((i - 1) % 5) * 310
                prevy = gy - ((i - 1) // 5) * 220
                arrow(c, prevx + 64, prevy, px - 64, py, col, 2.5)
    card(c, 2460, 2100, 2250, 610, "讀法", [
        "先看每課 Map 頁，建立路線。",
        "再看 Lecture 頁，把公式、程式、測試拆到 Freeform。",
        "最後回到 Markdown 原文做完整推導與實作。",
    ], YELLOW, body_size=37, max_lines=8)
    card(c, 2440, 400, 2300, 500, "輸出來源", [
        "docs/learning/deep_course/lesson_*.md",
        "每課約 1100 行以上，Lesson 1 保留完整範本。",
        "本 PDF 是視覺索引，不取代 Markdown 全文。",
    ], BLUE, body_size=34, max_lines=8)


def draw_lesson_map(c, lesson: LessonDoc, total: int):
    col = PALETTES[(lesson.no - 1) % len(PALETTES)]
    draw_bg(c, f"Lesson {lesson.no:02d} map / {lesson.file.name}")
    # Center.
    cx, cy, cw, ch = 2500, 1780, 2200, 1180
    c.setFillColor(pale(col, 0.62))
    c.setStrokeColor(INK)
    c.setLineWidth(6)
    c.roundRect(cx, cy, cw, ch, 14, fill=1, stroke=1)
    setf(c, FONT_HAND, 74, INK)
    c.drawString(cx + 80, cy + ch - 105, f"Level {lesson.no:02d}")
    draw_wrapped(c, lesson.title, cx + 80, cy + ch - 190, cw - 160, FONT, 56, 72, INK, max_lines=2)
    draw_wrapped(c, lesson.objective or lesson.phase, cx + 90, cy + ch - 360, cw - 180, FONT, 34, 46, MUTED, max_lines=4)
    c.setFillColor(colors.white)
    c.setStrokeColor(col)
    c.setLineWidth(4)
    c.roundRect(cx + 85, cy + 90, cw - 170, 385, 10, fill=1, stroke=1)
    formula_text = "\n".join(lesson.formulas[:6]) if lesson.formulas else "核心公式見 Markdown 原文"
    draw_wrapped(c, formula_text, cx + 120, cy + 405, cw - 240, FONT_CODE, 42, 56, INK, max_lines=6)

    panels = [
        (230, 3050, 1900, 1100, "源碼錨點", lesson.sources or ["詳見 Markdown"], BLUE),
        (5030, 3050, 1900, 1100, "資料結構", lesson.data or ["詳見 Markdown"], ORANGE),
        (210, 1350, 1900, 1180, "不變量", lesson.invariants or ["詳見 Markdown"], GREEN),
        (5080, 1350, 1900, 1180, "驗證關卡", lesson.tests or ["詳見 Markdown"], PINK),
        (680, 330, 2600, 720, "Debug 分支", lesson.pitfalls or ["詳見 Markdown"], PURPLE),
        (3920, 330, 2600, 720, "實作任務", lesson.exercise, CYAN),
    ]
    for x, y, w, h, title, body, pcol in panels:
        card(c, x, y, w, h, title, body, pcol, body_size=42, max_lines=11)
    # arrows.
    arrow(c, cx, cy + ch * 0.7, 2130, 3600, BLUE, 4)
    arrow(c, cx + cw, cy + ch * 0.7, 5030, 3600, ORANGE, 4)
    arrow(c, cx, cy + 300, 2110, 2000, GREEN, 4)
    arrow(c, cx + cw, cy + 300, 5080, 2000, PINK, 4)
    arrow(c, cx + 600, cy, 2600, 1050, PURPLE, 4)
    arrow(c, cx + 1600, cy, 4550, 1050, CYAN, 4)
    sticky(c, 2840, 3080, 660, 420, "Freeform", lesson.freeform, YELLOW, -3)
    sticky(c, 3700, 3100, 680, 420, "原文", f"完整講義：{lesson.file.name}", CYAN, 2)
    # progress rail.
    rx, ry, rw = 360, 185, PAGE_W - 720
    c.setStrokeColor(colors.HexColor("#DDD8CC"))
    c.setLineWidth(8)
    c.line(rx, ry, rx + rw, ry)
    for i in range(1, total + 1):
        x = rx + rw * (i - 1) / (total - 1)
        c.setFillColor(col if i <= lesson.no else colors.white)
        c.setStrokeColor(col)
        c.circle(x, ry, 18, fill=1, stroke=1)


def draw_lesson_lecture(c, lesson: LessonDoc):
    col = PALETTES[(lesson.no - 1) % len(PALETTES)]
    draw_bg(c, f"Lesson {lesson.no:02d} lecture / {lesson.file.name}")
    setf(c, FONT_HAND, 72, INK)
    c.drawString(220, PAGE_H - 250, f"Lesson {lesson.no:02d} Lecture Sheet")
    draw_wrapped(c, lesson.title, 220, PAGE_H - 340, 2800, FONT, 50, 64, INK, max_lines=2)
    draw_wrapped(c, lesson.objective or lesson.phase, 220, PAGE_H - 485, 3150, FONT, 32, 42, MUTED, max_lines=3)
    # Main lecture grid: 3 columns x 2 rows.
    x0s = [220, 2550, 4880]
    y_top = 2960
    w, h = 2100, 1180
    card(c, x0s[0], y_top, w, h, "1. 章節路線", lesson.headings[:12], col, body_size=39, max_lines=14)
    card(c, x0s[1], y_top, w, h, "2. 核心公式", lesson.formulas[:10], BLUE, body_size=40, max_lines=14)
    card(c, x0s[2], y_top, w, h, "3. 源碼 + 資料", (lesson.sources[:5] + lesson.data[:5]) or ["詳見 Markdown"], ORANGE, body_size=39, max_lines=14)
    card(c, x0s[0], 1510, w, h, "4. 不變量 + 測試", (lesson.invariants[:5] + lesson.tests[:5]) or ["詳見 Markdown"], GREEN, body_size=39, max_lines=14)
    card(c, x0s[1], 1510, w, h, "5. Debug", lesson.pitfalls[:10] or ["詳見 Markdown"], PINK, body_size=39, max_lines=14)
    card(c, x0s[2], 1510, w, h, "6. 實作題", lesson.exercise, PURPLE, body_size=38, max_lines=13)
    # Bottom dense notes.
    card(c, 220, 310, 3230, 880, "手寫展開區", [
        "把本課公式至少選一條完整展開。",
        "將每個符號標註單位與座標系。",
        "在旁邊寫出 C++ 欄位名稱。",
        "最後寫出會失敗的反例。",
    ], YELLOW, body_size=35, max_lines=10)
    card(c, 3750, 310, 3230, 880, "提交前檢查", [
        "正向測試、反向測試、oracle 都存在。",
        "public header 不暴露 Eigen / UE。",
        "錯誤模型回傳 diagnostic。",
        "Freeform 中保留外圈 debug 分支。",
    ], CYAN, body_size=35, max_lines=10)


def render_pngs(pdf: Path, out_dir: Path, max_pages: int | None = None) -> None:
    import fitz

    out_dir.mkdir(parents=True, exist_ok=True)
    doc = fitz.open(str(pdf))
    mat = fitz.Matrix(0.35, 0.35)
    for i, page in enumerate(doc):
        if max_pages is not None and i >= max_pages:
            break
        pix = page.get_pixmap(matrix=mat, alpha=False)
        pix.save(str(out_dir / f"page_{i:03d}.png"))


def main() -> None:
    register_fonts()
    lessons = load_lessons()
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    c = canvas.Canvas(str(PDF_PATH), pagesize=(PAGE_W, PAGE_H))
    c.setTitle("FrameCore v2 Deep Course - Freeform Edition")
    c.setAuthor("Codex")
    draw_overview(c, lessons)
    c.showPage()
    for lesson in lessons:
        draw_lesson_map(c, lesson, len(lessons))
        c.showPage()
        draw_lesson_lecture(c, lesson)
        c.showPage()
    c.save()
    render_pngs(PDF_PATH, PNG_DIR)
    print(PDF_PATH)
    print(PNG_DIR)
    print(1 + len(lessons) * 2)


if __name__ == "__main__":
    main()
