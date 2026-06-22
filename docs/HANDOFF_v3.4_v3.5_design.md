# HANDOFF — v3.4 + v3.5 設計交接(post-v3.3.0)

> 寫於 2026-06-22,v3.3.0 tag `dbdedb1` 發布之後。
> 目標:讓接續 session 拿到完整 v3.4 + v3.5 implementation map,不需重新設計。
> 主交接文 chain:`docs/HANDOFF_v3.3.0.md` → 本檔。

## 一頁總覽

| Release | 一句話 | 工時 | 動 engine? | 何時開始 |
|---|---|---|---|---|
| **v3.4.0** | Engine 數值面完整對 UE5 開放(Karamba3D-parity input + solve + inspect) | ~42 hr / 8-12 night-shift session | **0 行 target**(極少 `FRAMECORE_API` facade 加註可接受) | 任何 fresh session |
| **v3.5.0** | UE5 視覺面 + Chaos POD 整合 + 即時互動 GameInstanceSubsystem(完整建築模擬器遊戲 ready) | ~59 hr / 12-18 night-shift session | **0 行 hard target** | v3.4 ship 後 |

兩 release 加起來把 engine S1-S11 + v3 曲面線 + 全套線性/非線性分析的數值結果跟視覺結果都對 UE5 BP designer 開放。**v3.5 完了 = 對標 Karamba3D 11 個 tab 都有對應 BP 工具**。

## 規格文檔(authoritative)

- **v3.4 完整 spec**: [`docs/specs/UE5_engine_surface_map.md`](specs/UE5_engine_surface_map.md)
  - Phase 0-6 task tree
  - 每 phase USTRUCT / library / 測試清單
  - 鐵則對齊 + risk + done definition + honest boundaries
- **v3.5 完整 spec**: [`docs/specs/UE5_visual_surface_map.md`](specs/UE5_visual_surface_map.md)
  - Phase 0-10 task tree
  - 每 actor API 草稿
  - Chaos POD bridge + InteractiveSubsystem 設計
  - 鐵則對齊 + risk + done definition + honest boundaries
  - v3.4 → v3.5 cross-version contract

## 為什麼這麼切

### 不是「一個 release 全做」
- v3.4 + v3.5 合 ~100 hr,單 release 工時過長,中間沒 checkpoint 容易 derail。
- v3.4 (數值) 可獨立發布 + 給研究者用(僅 BP analysis,沒視覺也 ok)。
- v3.5 (視覺) 必須先有 v3.4 USTRUCT 才能消費,順序 forced。

### 不是「先 v3.5 再 v3.4」
- v3.5 actors 全部消費 v3.4 USTRUCT (`FFrameSolveResult`, `FFrameModalResult`,
  `FFrameDynCollapseResult` ...)。先做 v3.5 等於 mock USTRUCT 然後 v3.4 還要 rewrite。

### v3.4 為什麼是 6 phase 不是 1 個大 commit
- 每 phase 是 cohesive logical unit(Input / Output / Linear / Nonlinear / Shell-opts / Release-hardening)
- 每 phase 五腿綠 → checkpoint commit safe
- 鐵則 #5 commit 衛生:explicit `git add` per phase
- 模仿 v3.3 的 6-phase pattern,實證可運作

### v3.5 為什麼是 10 phase
- 每個 actor 是獨立 phase(可獨立測試 + commit)
- Phase 5 (Chaos POD bridge) 跟 Phase 7 (InteractiveSubsystem) 各自有 risk + 可獨立排程
- 模仿 Karamba3D 11 tab 結構,phase = tab

## 下 session 開工建議流程

### Fresh session 開 v3.4

1. **第一動**:讀 [`docs/specs/UE5_engine_surface_map.md`](specs/UE5_engine_surface_map.md) 整份
2. **第二動**:Phase 1 (Input USTRUCT)
   - 開始前 grep 確認 `frame::Node / Member / Material / Section / ShellQuad /
     NodalLoad / MemberUDL / ShellPressure / SolveOptions /
     {PDelta/Collapse/Corotational/ArcLength/DynCollapse/SizeOpt/BESO/
     ReSolve}Options` 全部存在於 `Plugins/FrameSolver/Source/FrameCore/Public/`
   - 每 struct 一一 mirror 進 USTRUCT
   - `UFrameModelBuilder` + `UFrameMaterialLibrary` + `UFrameSectionLibrary`
   - 3 個 test(BuildAndValidate / InvalidMemberRef / LibraryPresets)
   - 跑 standalone build 確認 engine source delta = 0
   - 跑 UE build 確認 module compile clean
   - 跑 run_gate -ExpectedUeTests 75 (72 base + 3 new) 確認綠
   - commit `feat(v3.4): Phase 1 -- Input USTRUCT + ModelBuilder + library presets`
3. **第三動**:Phase 2 (Output USTRUCT) — 同樣 pattern
4. ...各 phase 依 spec 跑
5. **最後**:Phase 6 release-hardening 統一 bump version + 3-agent audit + commit + tag + push + release

預期 8-12 個 night-shift session 完成 v3.4。

### Fresh session 開 v3.5(v3.4 ship 後)

1. **第一動**:讀 [`docs/specs/UE5_visual_surface_map.md`](specs/UE5_visual_surface_map.md) 整份
2. **第二動**:Phase 1 USTRUCT validation — quick sanity 跑 v3.4 USTRUCT 跟 spec 對齊(避免 actor 寫到一半發現 USTRUCT shape 不對)
3. **第三動**:Phase 1 Deformed Shape Actor(最簡單,先 land 建立 actor pattern)
4. ...順 phase 跑
5. **Phase 9** (Demo Map) 需 UE Editor 手動工作,planning 排在前面預留時間
6. **Phase 10** release-hardening 同 v3.4 pattern

預期 12-18 個 night-shift session 完成 v3.5。

## 跨 release engineering 共識

- **每 phase 必跑五腿綠才 commit**(CLAUDE.md 鐵則 #2)
- **每 phase 用 explicit `git add` 個別檔**(鐵則 #5,絕不 `-A`)
- **engine source delta target 鎖 0 行**(v3.4 容許極少 `FRAMECORE_API` facade additive;v3.5 hard zero)
- **3-agent audit 在每 release 末跑**(parallel general-purpose A correctness / B build-gate / C docs-schema,從 v3.0 起的固定 pattern)
- **bundle zip 內容固定**:`frame_capi.dll` + `frame_capi_v2.dll` + `frame_cli.exe` + `frametest.exe` + `openblas.dll` + `LICENSE` + `README.txt`
- **README + VERIFICATION + ARCHITECTURE + new HANDOFF + new RELEASE notes 為每 release lockstep update**
- **CLAUDE.md line 13 anchor 為 out-of-tree maintenance,每 release 手動修**

## 不在 scope 的東西(誠實邊界)

v3.4 + v3.5 對 UE5 開放後,以下 engine 限制依然成立(UE wrapper 解不掉,不要對使用者承諾):

- D/C utilization 是彈性快篩,**不是** RC ultimate-state check。
- DynamicCollapse 是 LSP-level sequential linear,文獻 ±30% 保守。
- Modal / Buckling / ResponseSpectrum 全 linear,**不是** 真實非線性動力。
- Plastic hinge 是 event-to-event,**沒有** unloading / reversal。
- 沒有 fiber section / pushover / 真彈塑性(使用者刻意排除)。
- MITC4 是 flat-facet,曲面誤差 O(1/N²)。
- Shell 殼 co-rotational 是 small-strain(linear `kl_` 黑盒)。
- Chaos POD destruction (v3.5 Phase 5) **是視覺效果非物理模擬** — engine 只告訴 Chaos 哪些 member 脫離,chunks 怎麼掉 Chaos 自己決定。
- Real-time interactive 60 fps 在 10K-DOF 有 benchmark,更大規模需更低 fps 或更 thin patch。

每個都會在對應 phase 的 RELEASE notes "Honest boundaries" 段重述。

## 不要做的東西(scope creep 警示)

接 v3.4/v3.5 session 容易爆 scope 的方向,**遇到要踩煞車**:

- 把 PMC 換成 DynamicMesh — v3.6 refactor,別在 v3.5 動。
- 重 design FrameModel 為 UObject (而非 USTRUCT) — 等 v3.5 Phase 7 真有 lifetime 需求才動。
- 加新 engine algorithm — engine S1-S10 完成,**v3.4/v3.5 不加算法,只暴露**。
- Karamba3D 沒有但 Rhino 有的 fancy feature(RC pushover、纖維斷面、真彈塑性)— **使用者刻意排除**,別動。
- DemoMap 過度 art polish — Phase 9 是 functional 不是 portfolio piece;留給後續設計工作。

## 跟 v3.3 一致的決策

v3.3 通過 7 phase + 9 task 完成,**v3.4/v3.5 沿用同 pattern**:

| v3.3 pattern | v3.4/v3.5 沿用 |
|---|---|
| Phase 0 spec doc 寫 LOCK | ✅ (本檔 + 兩 spec) |
| Phase N 完成 in-progress → completed → next | ✅ |
| Phase 末 五腿綠 才 commit | ✅ |
| 3-agent audit @ release Phase | ✅ |
| explicit `git add` per file | ✅ |
| RELEASE_v3.X.md 模板 | ✅(可複製 v3.3.0 結構) |
| HANDOFF_v3.X.0.md § 5 deferred + § 7 candidates | ✅ |
| CLAUDE.md line 13 manual sync | ✅ |
| bundle zip 同 6 件套 | ✅ |
| gh release with --notes-file RELEASE_v3.X.md | ✅ |

## 對 Karamba3D 11-tab 對標自評

| Karamba3D Tab | v3.4 phase | v3.5 phase | 對齊狀態 |
|---|---|---|---|
| Setup | Phase 1 (ModelBuilder) | — | ✅ |
| Material | Phase 1 (MaterialLibrary) | — | ✅ |
| Section | Phase 1 (SectionLibrary) | — | ✅ |
| Element | Phase 1 (Member/ShellQuad USTRUCT) | — | ✅ |
| Load | Phase 1 (NodalLoad/MemberUDL/ShellPressure USTRUCT) | — | ✅ |
| Analyze | Phase 3 + Phase 4 (linear + nonlinear lib) | — | ✅ |
| Inspect | Phase 2 (Output USTRUCT) | — | ✅ |
| Display | — | Phase 1-6, 8 (renderer actors) | ✅ |
| Algorithms (BESO/FSD) | Phase 4 | — | ✅ |
| Advanced (PD/Tens/Corot/Arc) | Phase 4 | — | ✅ |
| Interactive | Phase 3 (ReanalysisSolve library form) | Phase 7 (InteractiveSubsystem) | ✅ |
| Utility | Phase 1 (libraries) | — | ✅ |

**全 tab cover ✅**。差別只在 Karamba 走 Rhino/Grasshopper UI,v3.4+v3.5 走 UE5 BP 跟 Slate。

## 待用戶決定的 design 開放問題

下 session 動工前,如果遇到下列 design choice,需 ASK user 而非自己決定:

1. **FrameModel 用 `USTRUCT` (`FFrameModelDef`) 還是 `UObject` (`UFrameModel`)?**
   - USTRUCT:輕量,BP 友好,但 InteractiveSubsystem 持有 lifetime 不自然
   - UObject:lifetime 自然,但 BP `Make/Break` 不適用
   - **建議**:USTRUCT 走 v3.4,v3.5 Phase 7 InteractiveSubsystem 內 wrap 成 UObject
2. **DynCollapse `live frames` 走 callback 還是 polling?**
   - Callback (`onFrameEmitted`):engine 已有(v2.7),但要 BP delegate / event dispatcher
   - Polling (`GetFrameAtTime` / `Tick read`):BP 更習慣,但效率次
   - **建議**:走 BP delegate,跟 UE 慣例對齊
3. **Chaos POD chunk 用 procedural geometry 還是 designer-provided GeometryCollection?**
   - Procedural:engine drive 全自動,但視覺 generic
   - Designer-provided:藝術 polished,但 BP 要 pass GeometryCollection asset
   - **建議**:`UPROPERTY UGeometryCollection* ChunkTemplate = nullptr` 可選,nullptr 時用 procedural fallback
4. **v3.6 起的 roadmap?**
   - 預設:engine 進入 "stable mode",只接 audit-driven minor patches
   - 候選擴展:RC pushover (使用者刻意排除,unlikely)、SaaS dispatcher (B6)、Rhino 8 .gha真實作 (B7)、新 element type (S11 MITC9i 高階殼;memory 提的「殿後」)
   - **建議**:v3.5 ship 後跟用戶 review,no commitment now

## 最後一句

**v3.3 把 visualization 一條窄面(stress field)開了。v3.4 + v3.5 把整個 engine 對 UE5 開到滿。** 兩個 release 加起來大約是 v3.0-v3.3 累計工時的 2 倍;但因為 engine 已 frozen + spec 已 lock,實作上應該比早期 v3.x 順手 — 沒新算法、沒 schema 風暴、沒 release-hardening 時才發現 audit blocker。

**Engine 從 v3.4 起進入「stable mode」假設**,理論上 v3.6+ 只接 bug fix。如果 v3.5 ship 後使用者有新方向(例如 Rhino 整合 / SaaS 化 / MITC9i 高階殼),那是另一條 roadmap,跟本 spec 無關。
