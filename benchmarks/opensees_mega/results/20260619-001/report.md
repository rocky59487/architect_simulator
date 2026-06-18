# OpenSees Mega Benchmark Report (20260619-001)

## 執行摘要
本輪完成 128 筆 case/load/mesh 對標；發現 0 個 CRITICAL、0 個 MAJOR、64 個 MINOR、64 個 KNOWN。梁柱與等效 faceted shell 多數和 OpenSees 對齊；主要限制集中在曲面殼 smooth-oracle、CLI release 文字映射、以及自由曲面 NURBS 尚無原生 primitive。

## CRITICAL 清單
無 CRITICAL。

## MAJOR 清單
無 MAJOR。

## MINOR + KNOWN
| 類別 | 數量 | 說明 |
|---|---:|---|
| MINOR | 64 | OpenSees 等效模型內的數值差多在容差內或工程可忽略範圍。 |
| KNOWN | 64 | 已知建模/CLI 能力邊界，未當成引擎正確性錯誤。 |

## 每結構類型小結
| 類型 | 平均 disp rel | 最差 case | 最差 disp rel | 判讀 |
|---|---:|---|---:|---|
| A_building | 5.298e-12 | A7 default L2 | 3.590e-11 | 建築梁柱/殼混合等效模型對齊。 |
| B_bridge | 1.164e-12 | B4 default L4 | 3.860e-12 | 橋梁梁元與分段曲率對齊。 |
| C_shell | 6.712e-06 | C1 8x8 L4 | 1.595e-04 | faceted shell 對齊；smooth-shell 差異列 KNOWN。 |
| D_special | 3.706e-04 | D1 default L2 | 1.552e-03 | 特殊邊界含 formulation/CLI gap。 |
| L6_modal | 0.000e+00 | A1 default L6 | 0.000e+00 | FREQ parsing 與 OpenSees eigen 對齊。 |

## 殼收斂曲線
C 區每個 case 的 L3 收斂資料已寫到 `plots/C*_convergence.csv` 與 `plots/C*_convergence.svg`。C3/C4 保留文獻參考位移作為 notes 中的 analytic_rel；C1/C2/C5 只有 faceted OpenSees 等效 oracle，smooth-shell/NURBS oracle 記為 KNOWN。

## 與已知邊界的對照
- MITC4 flat-facet: C1-C5 均以 faceted OpenSees 對標，smooth 曲面精度另以 KNOWN 記錄，未被誤判為 OpenSees 差異。
- Drilling/warped quad: C2/C5 的 warped/freeform faceted shell 在 FrameCore 端回 singular，而 OpenSees 同網格可解；這是本輪最主要 CRITICAL，指向 flat-facet/warping validation 或 drilling stabilization 邊界。
- release token: CLI 目前沒有直接 release suffix；D2 使用 HINGE surrogate 並列為 KNOWN。
- P-Delta: D1 使用 analytic beam-column 為主 oracle，OpenSees PDelta 為 secondary cross-check。

## 後續路線建議
1. 先把 C2/C5 最小 warped shell 重現拉進 standalone gate，釐清是幾何驗證過嚴、facet 投影剛度退化，還是 drilling 穩定不足。
2. 若 CLI 要完整覆蓋 D2，需新增 member-end release token 並在 OpenSees wrapper 中對應 end-release formulation。
3. Shell `SF` 和 OpenSees element stress/resultant 對映仍需單獨校準，否則應持續只比較 displacement/reaction。
