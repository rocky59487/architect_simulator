# Freeform 匯入與排版指南

這套課程現在的主產物是 Markdown 講義，不是上一版的簡略白板圖。

建議用法：

1. 先打開 `README.md` 當課程索引。
2. 每次只匯入一課，例如 `lesson_10_3d_euler_bernoulli_beam_column_局部剛度.md`。
3. 在 Freeform 中把每課拆成六個區塊：
   - 中央：核心公式與本課目標
   - 左上：數學推導
   - 右上：資料結構與 C++ 實作
   - 左下：手算例題
   - 右下：測試與 oracle
   - 外圈：Debug checklist 與錯誤案例
4. `framecore_v2_deep_course_combined.md` 是合併版，適合全文搜尋，不建議一次整份丟進 Freeform。

每課的 `H. Freeform 大畫布排版建議` 已經提供該課如何拆成大白板區塊。

若需要 PDF 版本，應從單課 Markdown 轉出，而不是整份合併版一次轉出；單課 PDF 比較適合 Freeform 分頁。
