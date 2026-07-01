# 最小复现：开口 c-spar segment 薄首层切层崩溃

对应 issue：[local/issue-20260628-clipper2-open-layer1-collinear-hook.md](../../../local/issue-20260628-clipper2-open-layer1-collinear-hook.md)
（`local/` 不入库，本地查看）。

来源：`share/examples/ex_airfoil_dspar/cs_main.xml` 删去 trailing-edge（te）和
fill_aft 两个 **component**，只保留 d-spar 部分。

## 测的是什么

d-spar = **c-spar segment**（base = 翼型弧 `p4 → 前缘 → p3`，**开口**）+ **web
segment**（弦 `p3 → p4`）。铺层 = 薄石墨蒙皮（2 ply）+ 4 组 spar 层压，向内偏移。

首层（蒙皮）相对致密 base 极薄，其 trim 后的 offset 曲线在 `p4` 开口端形成一个
**近共线"尖刺"**（head tip `skin.v1` 处两条出边几乎同向，δ≈4.87e-15 rad）。
`splitFaceByPolyline` 的角序织入（`updateEdgeNeighbors`）在 δ→0 处插错边，
第一次 split 没把 segment face 一分为二，而是留下未切分的原 face + 两个伪面，
下游在损坏拓扑上操作 → 崩溃。

**raw vs trim 的关键发现**：raw offset（`makeRawOffsetCurve` 输出）头部是**干净
单调**的，首点就是 tip、未锚到 bound；**尖刺是 `trimLayerCurveEndsToCaps`
制造的**——它把 bound 落点 `skin.v0` prepend 到 tip 之前，造成回折。

**首层尖刺已由 (A) 修复**（`trimLayerCurveEndsToCaps` 反折尖点守卫，见 issue §8(A)）。
可视化见 [cspar_layer1_hook.svg](cspar_layer1_hook.svg)（历史，记录已修复的首层尖刺）：
- 面板 A：c-spar 整体 base / 首层 offset(post-trim) / shell / 两端 bound（base 与
  skin 在此尺度几乎重合，直观体现"首层极薄"）。
- 面板 B：`p4` head 的 **raw offset（trim 前）**——干净，无尖刺。
- 面板 C：`p4` head 的 **trimmed offset（trim 后）**——prepend 落点造出尖刺、δ≈5e-15。
- 面板 D：后果与修复方向说明。

## 当前失败点（(A) 之后，最后一层）

(A) 修好首层后，同一 case 推进到**最后一层**（`curves[last] → shell`）才崩，
**另一独立根因**：staircase 在头部 **outer(shell) 侧 stall**（base 3/4/5 都配
offset 3），step 4/5 的 connector 从同一个 shell 顶点 `outer[3]` 扇出，夹出近零面积
sliver，step 5 的 `splitFaceByPolyline` 返回两张坐标重合的退化面，污染 `remaining`，
step 6 在退化 sliver 上 split → `split_faces=0` → fatal。详见 issue §8.1。

可视化见 [cspar_layer1_final_stall.svg](cspar_layer1_final_stall.svg)：
- 面板 A：最后一层整体（inner=`curves[last]` / outer=`shell` + 全部 staircase
  connector，红 = outer-stall connector）。
- 面板 B：头部纵向 z 拉伸放大——connector 从单个 shell 顶点 `o3` 扇出，step4/step5
  夹出近零面积 sliver。
- 面板 C：机制文字（step 1 埋点实测，已回退）。

[cspar_layer1_layers.svg](cspar_layer1_layers.svg)：逐层 offset 曲线总览，
对比 SEQ（逐层 offset）vs DIR（直接累积 offset），量化偏差（max ≤ 8.8e-5，
< 层厚 0.75%）——**排除 offset 误差累积**，确认根因纯在 staircase 配对。

## 复现要点

- **web 必不可少**：去掉 web，第一次 split 会在任何 DCEL 修改**之前**干净失败并
  回退 legacy（通过）。web 的 join 改变了 segment 两端 bound 形状，把 split 从
  "干净空结果"推成"两个损坏面"。
- **te baselines / p7 / p8 保留**：虽无 component 直接使用，但它们切分 `ln_af`，
  决定 c-spar base 的离散化；删掉会改变顶点布局。

## 运行

```bash
cd test/debug/clipper2_dspar_layer1
../../../build_msvc/Release/prevabs.exe -i cspar_layer1.xml --hm
```

## 预期（当前 HEAD，开口 route-ii）

崩溃，但**已不在首层**（首层尖刺已由 (A) 修复，layer 1..n-1 正常切分、无 δ≈5e-15
近共线警告）。现崩在**最后一层** staircase：

```
xx fatal exception: ... layered offset[sgm_blk_1]: failed to split final layer
   into cells after the shared DCEL was already mutated ...
```

上方伴随
`layered offset[sgm_blk_1]: failed to split layer band cell at staircase step N`
（N 因 `rebuildBaseOffsetMapFromGeometry` 配对非确定而在 5/6 附近跳变，根因相同：
头部 outer-stall connector 扇 → step 5 退化分裂）。

## 对照（证明几何可网格化、缺陷在开口 route-ii）

把 base 闭合成单条闭合 segment（`<points>p4:p3,p4</points>`，去掉 web）：闭合多层
在 [PBuildSegmentAreas.cpp:1384](../../../src/cs/PBuildSegmentAreas.cpp#L1384)
早退到 **legacy** area/layer build → **通过**（EXIT=0）。完整对照见
`share/examples/ex_airfoil_dspar/cs_main_temp_2.xml`。

## 修复后预期

开口 route-ii 直接构建成功、不再回退 legacy；无 δ≈5e-15 的近共线警告；
正常输出 `.sg` 等文件。
