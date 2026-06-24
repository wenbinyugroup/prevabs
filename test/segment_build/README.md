# test/segment_build — "build a single laminate segment" 测试集

针对核心步骤 **build a single laminate-type segment**（`Segment::buildAreas` →
`buildLayeredOffsetAreas` 逐层 offset 切层，或 legacy 回退）的专用测试集。

集成测试（t1–t9）只给端到端的「跑通/失败」二值信号；很多「跑通」的算例其实
切层质量有问题（翻转面、sliver、缝隙、悄悄回退 legacy）却看不出来。本测试集
对**每个 segment** 直接检查切层结果并量化几何质量。

## 跑

```powershell
# 全部
pwsh -NoProfile -File test\segment_build\run-segment-tests.ps1
# 子集（名字含 arc）
pwsh -NoProfile -File test\segment_build\run-segment-tests.ps1 -Filter arc
```

需要先 build：`pwsh -NoProfile -File tools\build-msvc.ps1 fast`。

runner 对每个 `<case>.xml` 用
`PREVABS_LAYERED_OFFSET=1 PREVABS_SEGMENT_DUMP=1 prevabs -i <case>.xml --hm`
运行（带 per-case 超时，默认 30s）。

## 产出

每个 laminate segment：

| 文件 | 用途 |
|---|---|
| `<case>.<seg>.segment.svg` | **人读**：base(蓝)/offset·shell(橙)/逐层 tiled face（按层填色）；翻转面红、sliver 橙高亮 |
| `<case>.<seg>.segment.json` | **机读**：route、层数、face 数、每 face {面积, 最小角, 层, 材料}、inverted/sliver/degenerate 标志、`path` 分支轨迹 |
| `<case>.<seg>.segment.path.txt` | **分支轨迹**：该段在 `buildAreas`/`buildLayeredOffsetAreas` 各条件分支实际走的哪条路（layered/legacy、回退原因、单层/闭合/顶点不匹配/ambiguous-split）。比 debug log 干净 |
| `<case>.msh` | gmsh 网格（可视化/下游） |
| `<case>.dcel_dump.txt` | DCEL 文本转储 |

汇总：

| 文件 | 用途 |
|---|---|
| `report.md` | 人读总表：每 segment 一行 + PASS/WARN/FAIL，并附每段 **Build paths** 分支轨迹 |
| `report.csv` | 机读：每 segment 一行 |

状态判定：

- **FAIL** — 运行崩溃/超时，或有 inverted / degenerate face。
- **WARN** — 有 sliver face，或开放段意外回退 legacy。
- **PASS** — 其余。

SVG dump 由 `src/debug/segmentBuildDump.cpp` 生成，仅当 `PREVABS_SEGMENT_DUMP`
环境变量非空时启用；生产运行零开销、零影响。

## 当前用例

### 健康用例（构造的「应当跑通」算例，期望 PASS）

| case | 几何 | 铺层 | 考察点 |
|---|---|---|---|
| `line_1layer` | 直线 | 1 层 | 单层平凡 face |
| `line_4layer` | 直线 | 4 均匀层 | 基础多层切分 |
| `line_nonuniform` | 直线 | 3 层非均匀厚 (0.005/0.010/0.020) | route-ii 逐层不同厚度 offset |
| `line_thin_many` | 直线 | 8 薄层 (0.001) | 小厚度逐层嵌套 |
| `arc_coarse` | 90° 弧 (~9 段) | 3 层 | 多顶点弧 tiling |
| `arc_fine` | 90° 弧 (1°, ~90 段) | 4 层 | 细弧（曾因 min-chord 守卫超时，见 issue-20260624） |
| `arc_semicircle` | 180° 弧 | 3 层 | 大跨度弧 |
| `vbend_multibend` | Z 形开放折线 (4 点) | 3 层 | 开放 multi-bend 逐 connector 切 cell |
| `airfoil_skin` | MH104 上表面单段 | 3 层 | 真实翼型曲线段 |
| `circle_closed` | 闭合圆 | 3 层 | **边界**：闭合多层暂回退 legacy（route=legacy 预期） |

### 真实失败用例（从集成测试 t1–t9 原样提取，期望 WARN/FAIL）

下列用例是 t1–t9 集成算例里**当前 layered offset 路径处理不好的真实 laminate
segment**，逐字提取到本目录（几何/铺层/`general`/`adaptive_thickness` 等所有
segment 参数原样保留，仅把外部 `<include>` 的材料**内联**进 XML、并去掉与失败段
无关的兄弟 segment/component）。提取后逐项复核，`base_vertices` / `face_count` /
`sliver` / 回退路径与集成跑出来的完全一致。这些是 offset 重构的回归靶子。

| case | 提取自 (段) | 几何 | 现象 |
|---|---|---|---|
| `line_collinear_3pt` | t1_strip/strip_two_lines (sgm_1) | 直线 + 共线冗余中点 (lc,mc,rc) | 16 层对称铺层；shell/base 顶点数不匹配 (base=3,shell=2) → 回退 legacy |
| `vbend_offset_out` | t2_z/v_offset_out (sg_v) | 单 V 折 (3 点) | 铺层偏到**外凸**侧 (`direction="right"`)；顶点不匹配 (base=3,shell=4) → 回退 legacy |
| `ibeam_flanges` | t4_I/i_web (sgtop, sgbottom) | 两条共线-3 点翼缘 | 2 层显式铺层；两段均顶点不匹配 → 回退 legacy |
| `airfoil_te_adaptive` | t9_airfoil/mh104_te_only (sg_te) | MH104 后缘线段 + `adaptive_thickness` | 5 层（含 balsa）；段回退 legacy，**下游 gmsh 网格恢复失败 → 整体 FAIL**（与原集成算例同样崩溃） |
| `airfoil_skin_closed_mh104` | t9_airfoil/mh104_9webs (sg_le) | MH104 整张蒙皮（**闭合**环） | 3 层；闭合段回退 legacy，legacy tiling 残留 5 个 sliver |
| `airfoil_skin_closed_ah79k132` | t9_airfoil/airfoil_ah79k132_skin_only (sg_le) | AH 79-K-132 整张蒙皮（**闭合**环） | 单层各向同性；闭合回退 legacy，残留 15 个 sliver |

> 提取范围说明：另有 3 个失败段（`param_layup` 的 `sgm_blk_1_1`、
> `uh60a_arc_leading_web` 的 `sgm_*`、`main_cs_set1` 的 `seg_back_te`）是由
> 重叠 `begin/end` 铺层范围或翼型自动分段**生成**的子段，无法在不重算分段边界
> （即改变参数）的前提下化简成单个 `<segment>`，故未提取。

## 加用例

丢一个 `<name>.xml`（`<include><material>materials</material></include>` 引用共享
材料；几何/铺层照现有用例改）。重跑 runner 即自动收录，无需改脚本。翼型用例需把
`.dat` 放本目录并在 `<line type="airfoil">` 里引用。

## 与其它测试的关系

| | t0_offset_clipper2 | **segment_build（本）** | t1–t9 集成 |
|---|---|---|---|
| 走到哪一层 | 仅 offset+matcher 几何 | **完整 segment 切层 DCEL** | 全套 + homogenize |
| 入口 | C++/.dat | XML 单段截面 | XML + `--hm` |
| 失败口径 | 配对几何 | **切层 face 质量** | 下游 gmsh/homogenize |
| 产出 | SVG/CSV | SVG+JSON+gmsh+报告 | .sg/.msh |
