# t0_offset_clipper2 — Clipper2 offset → staircase 构建实验台

## 目的

独立、最小依赖的程序，专门用来 **rapid-iterate** Clipper2 offset 后的
**staircase base-offset map 构建**。

集成测试主管线（`prevabs -i x.xml --hm`）跑全套 DCEL + gmsh +
homogenize，回路 ≥ 30 s/case 且失败原因复杂；本程序剥掉所有下游，
**只跑 offset() 几何 + matcher**，每 case 毫秒级，便于：

- 验证 base ↔ offset 配对是否合理（segment / nearest / DP 三 algo 对照）
- 实验 Stage E pre-trim 不同 α 的 thin run 检测
- 添加新的几何病态 case（cusp / pinch / 短开放段 / etc.）零成本
- 不引入 PreVABS PDCEL / spdlog / gmsh / cpp-terminal 任何依赖

未来若发现稳健构建方法，可直接把改动反推回
[`src/geo/offset_clipper2.cpp`](../../../src/geo/offset_clipper2.cpp) /
[`offset_clipper2_bridge.cpp`](../../../src/geo/offset_clipper2_bridge.cpp)
——本程序**直接 link 这两个 TU**，因此实验改动 = 主代码改动，无须重写。

## 名词解释

### 几何主体

- **base** — 输入折线（蓝色 polyline）。**closed**=true 表示首尾通过隐
  式 wrap 段闭合；**open**=false 表示两端是独立端点（offset 用 Butt
  cap 收尾）。
- **offset / offset polygon** — Clipper2 `InflatePaths` 输出（橙色），
  距离 base 一个 `dist` 的偏移多边形 / 折线。
- **inset** = **inward offset / 内偏移**。Clipper2 `InflatePaths`
  的 `delta < 0` 路径——闭合 polygon **向内**收缩 `|delta|`。PreVABS
  闭合 CCW + `side = -1` ⇒ Clipper2 `delta = -dist` ⇒ **inset**。
  - "outward offset / 外偏移" 反之：`delta > 0`，polygon 向外扩张。
  - 开放折线没有"内 / 外"之分；Clipper2 给 `EndType::Butt` 双侧
    wrap 把开放折线**两侧**都 inflate，本程序 + PreVABS 通过 `side`
    后处理过滤掉不要的那一侧。
  - **inset 几何病态**：当 base 的局部 half-thickness `h_i < dist`
    时（cusp 顶端 / thin TE），向内 offset 会"穿透"对岸——Clipper2
    把穿过去的部分 union 掉，结果是 base 顶点被合并 / 吃掉
    （**M ≪ N**）。这是 mh104 TE 失败的根因，见
    [issue-20260521-mh104-te-root-cause-not-pairing.md §6.1](../../../../rnd/notes/dev/prevabs/dev-notes/archive/issue-20260521-mh104-te-root-cause-not-pairing.md)
    的厚度 sweep 实证。
- **side** — `+1` / `-1`。开口折线侧向选择（左/右）；闭合 CCW
  polygon `+1`=外偏、`-1`=**inset**（内偏）。PreVABS 约定的 side 与
  Clipper2 内部 δ 符号在闭合分支会做翻转。
- **dist** — offset 距离的**幅值** `|δ|`（一定 > 0）。

### 顶点计数

- **N** — base 顶点数（distinct，不含 closed polyline 末尾的 trailing
  duplicate）。
- **M** — Clipper2 offset 多边形输出的顶点数（distinct）。
- **M/N ratio** — 经验 mesh-able 阈值 ≈ 0.7（见
  [issue-20260521-mh104-te-root-cause-not-pairing.md §6.1](../../../../rnd/notes/dev/prevabs/dev-notes/archive/issue-20260521-mh104-te-root-cause-not-pairing.md)
  的 mh104 厚度 sweep 实证）。M/N 低于阈值时下游 gmsh edge-recovery
  常失败。

### Staircase 对应关系

- **pair** = `BaseOffsetPair{base, offset}` — 单条 base 顶点 ↔ offset
  顶点的对应关系。SVG 里**灰色细线**就是 pair 连线。
- **id_pairs / BaseOffsetMap / staircase** — 一组有序 pair，构成
  base ↔ offset 的全局对应表。staircase 不变量：相邻 pair 的
  `base` / `offset` 索引各自 +1 步增（不回退、不跳跃 > 1）。详见
  [include/geo_types.hpp:27](../../../include/geo_types.hpp#L27)。
- **wrap pair** — closed polyline 的 staircase 末尾的 `(N, M)`
  对，让下游可以正确接通尾部到首部的环。Open 路径**没有** wrap pair。

### Matcher（配对算法）

把 Clipper2 输出的 offset 顶点序列**反向匹配**到 base 段，构造
staircase。三种实现，每种有不同的 attribution 语义：

- **segment** = `planReverseMatch` — 用 `OffsetVertexSource::base_seg` +
  `attributeOne` 的 segment-midpoint round。Legacy 算法、PreVABS
  生产默认。
- **nearest** = `planReverseMatchByNearest` — point-to-point 最近邻顶
  点 + forward greedy walk。绕开 segment-midpoint 处的阈值跳变。
- **dp** = `planReverseMatchByDP` — min-sum dynamic programming（DTW
  风格），四项几何代价（距离、弧长进度、切线对齐、法向对齐）。全
  局最优、但带 O(N·M) 开销。

详细差异见
[issue-20260522-dp-matcher-phase-d.md](../../../../rnd/notes/dev/prevabs/dev-notes/archive/issue-20260522-dp-matcher-phase-d.md)
§2 三模式集成对比。

### Drop（丢失对应）

- **drop / dropped base** — 某个 base 顶点在 Clipper2 inset 后**没
  有对应的 offset 顶点**（局部 half-thickness < dist，offset 直接吃
  穿了这段 base）。SVG 里**红色圆圈**高亮。
- **dropped_base_ranges_lo/hi** — 连续 dropped base 段，存为
  `[lo, hi]` inclusive 区间列表。Stage C `buildIdPairs` 会在 walked
  序列出现跳跃时自动记一段 dropped；下游 PSegment 用 dropped 跳过
  area 构造。

### Stage E pre-trim

[plan-20260522-stage-e-pretrim.md](../../../../rnd/notes/dev/prevabs/dev-notes/archive/issue-20260522-stage-e-pretrim-phase-c.md)
的"输入侧"思路：在调 Clipper2 **之前**就把薄顶点剔除，避免 inset
阶段折角合并。

- **h_i / signedHalfThickness** — 从 `base[i]` 沿 inward 法线（朝
  polyline 内部方向）射出的射线到**最近的非邻 base 段**的距离。
  cusp apex 处 inward ray 通常击中"对岸"段；valley apex 处射线指向
  空旷处、返回 INF。
- **α (pretrim_alpha)** — 阈值倍数。`h_i < α·|dist|` 的 base 顶点视
  为 **thin**。Phase A 标定 0.8，Phase B 调到 1.2（覆盖 borderline
  桥端点）。
- **thin run** = `ThinRun{lo, hi}` — 一段连续的 thin base 顶点。
  本 harness 只处理**单一 strictly-interior**（`lo > 0 && hi < N-1`）
  thin run；多段不相交或贴边的 thin run 走 fallback 不 pre-trim。
- **trimmed input** — 把 `base[lo..hi]` 直接删掉后剩下的折线：
  `base[0..lo-1] ++ base[hi+1..N-1]`。**紫色虚线**在 SVG 里显示。
- **bridge / bridge chord** — trimmed input 中 `base[lo-1]` 与
  `base[hi+1]` 之间的隐式直线段。这条段没有原始 base 对应，
  Clipper2 输出落在 bridge 上的 offset 顶点经 `remapBaseSegToOriginal`
  改 `base_seg = -1`，被 Stage C 当 join-point 处理。
- **INF-邻居规则** — interior INF（cusp apex）只有当**至少一个邻居
  finite-thin** 时才标 thin。否则健康短开放段（每个 interior 都 INF
  的 web）也会被误判 trim 成裸 bridge chord。

### Open vs Closed Clipper2 输出细节

- **pre_resample_points** — open 路径，Clipper2 raw 输出（含 Butt cap）。
  `nearest` / `dp` matcher 默认走这条 raw 序列。
- **resampled `points`** — `extractOpenRuns` 把 raw 重采样到 N 顶点
  对齐 base。`segment` matcher 走这条 resampled 序列。
- 这就是为什么 healthy `open_straight` 在表里 `segment M=4` 但
  `nearest/dp M=2` —— 不是 bug，是 source-of-truth 选择不同。

### raw 对照组 (`+raw`)

`offsetWithClipper2` 第 7 个参数 `resample_open`（默认 `true`）控制
`extractOpenRuns` **第 5 步 resample** 是否执行：

| `resample_open` | open 路径 `points` 内容 | matcher 看到的 M |
|---|---|---|
| `true`（默认/生产） | foot-of-perpendicular 投影到 N base 顶点 | M ≈ N（covered subset） |
| `false`（**raw 对照组**） | 原 side-filtered Clipper2 顶点（含 corner / bevel join 点） | M = Clipper2 raw 顶点数 |

闭合输入下此 flag **无效**——闭合 polygon 本来就没 resample 步骤，所
以闭合 case 的 `<scenario>_raw.svg` 与 `<scenario>.svg` 像素级一致（仅
作 sanity 对照）。

#### 实测对比（segment matcher、开口 case）

| Scenario | segment（resample） | segment+raw |
|---|---|---|
| open_straight | M=4，无 drop | **M=2**，drop [1..2] |
| open_v_cusp_shallow | M=8 drop [6..7] | **M=6**，drop [1..2],[4..4],[7..8]（3 段） |
| open_v_cusp_sharp | M=11 无 drop | M=11 无 drop（resample 没起作用） |
| open_te_cusp_like | M=9 drop [1..3],[10..12] | **M=8** drop [1..4],[10..13] |
| airfoil_mh104_te_open | M=3 drop [2..16] | M=3 drop [2..16]（Clipper2 已 collapse cusp） |

观察：

- `segment+raw` 暴露出 **resample 在干什么**——给 base[i] 找 foot-of-
  perpendicular 投影、合成新顶点对齐到 N。关掉它后 `points` 直接是
  Clipper2 raw 顶点序列，`segment` matcher 的 M < N 比较常见
- `nearest+raw` / `dp+raw` ≈ 默认 `nearest` / `dp`——本来就走
  `pre_resample_points`（与 resample=false 时的 `points` 等价），所以
  raw 对照组对这两个 matcher **无变化**（实测完全一致）
- **mh104 TE cusp 失败的 M=3 与 resample 无关**——Clipper2 在 inset
  阶段就吃穿了 cusp，无论 resample 与否 raw 都只剩 3 个顶点。这进一
  步缩小 mh104 失败的根因：**纯属 Clipper2 inset 几何病态**，与下游
  resample / matcher 选择都无关

#### 实验目的

raw 对照组要回答的问题："vertex-match 是否能直接吃 raw Clipper2
顶点，不依赖 resample？"——

- **回答（理论上的 yes）**：raw 顶点 + nearest / dp matcher 本来就工作
  得很好（默认就走 raw）。问题是 segment matcher 的 attributeOne 用
  `(seg, u-round)` 假设输入顶点 1:1 对齐 base，raw 序列不满足这个假
  设 → drop ranges 增多。
- **生产侧含义**：若想全面去掉 resample（plan-20260520
  "skip-resample" 备选路径），必须配合 segment matcher 重写**或者**
  把默认 matcher 换成 nearest/dp（DP matcher 工作即 Phase A-D 已完成）。
- **mh104 / thin-TE 群**：raw 对照组确认它们的 M=3 / M≪N 现象不是
  resample 引入的伪 drop——是真 Clipper2 inset 失败，需要正交工作
  （per-layer offset 或代替 inset 算法）。

### 其它字段

- **ok** = `plan.ok` — Stage C 反向匹配产出的 plan 是否通过
  `staircaseValid`（顶点全 ≥ 0、相邻步进 ∈ {0, 1}）。
- **pairs** = `id_pairs.size()` — 总 pair 数。closed 路径含 wrap
  pair 所以一般是 N + 1。
- **drps** / **drops** = dropped range 列表的可读字符串
  `"[lo..hi],[lo..hi]"`。

### JoinType（Clipper2 corner-join 算法）

`offsetWithClipper2` 在每个 base 顶点（"corner"）需要决定怎么连接前
后两段的 offset 边——四种算法（Clipper2 `JoinType`）：

| Join | Clipper2 行为 | 顶点产出（每 corner） |
|---|---|---|
| **miter** | 延长两边交于尖角；超过 `miter_limit·delta` 时退化为 bevel | 通常 **1**（尖角点） |
| **square** | 用 **2 个** 顶点 + 一段 90°/平直边把尖角"切方" | **2** |
| **bevel** | 直接用最短 chord 跨过 corner，去掉尖头 | **2** |
| **round** | 用 `arc_tolerance` 控制精度的弧（弧上插许多顶点近似） | **多**（视 `arc_tolerance`，默认 0.25 → 每 corner 可几十顶点） |

PreVABS 生产默认 `Miter + miter_limit=2.0`（见
[issue-20260516-clipper2-offset-phase-e.md](../../../../rnd/notes/dev/prevabs/dev-notes/archive/issue-20260516-clipper2-offset-phase-e.md)
+ [issue-20260515-clipper2-airfoil-a0.md](../../../../rnd/notes/dev/prevabs/dev-notes/archive/issue-20260515-clipper2-airfoil-a0.md)
的 A0 evidence）。本 harness 把 JoinType 暴露为 `offsetWithClipper2`
的可选参数（生产代码默认仍 Miter+2.0、零行为变化）。

#### 当前实测（segment matcher + dist 同）：

| Scenario | Miter | Square | Bevel | Round |
|---|---|---|---|---|
| closed_square | M=4 | M=8 | M=8 | **M=5316** |
| open_straight | M=4 | M=4 | M=4 | M=4 |
| open_v_cusp_shallow | M=8 drop [6..7] | M=8 drop [1..2] | M=6 drop [1..2,6..7] | M=6 drop [1..2,6..7] |
| open_te_cusp_like | M=9 drop [1..3,10..12] | M=9 同 | M=7 drop [1..4,9..12] | M=7 同 bevel |
| closed_thin_te | M=11 | M=20 | M=20 | **M=3473** |
| airfoil_mh104_closed | M=67 | M=94 | M=94 | **M=2651** |
| airfoil_mh104_te_open | M=3（cusp 仍吃穿） | M=3 | M=3 | M=3 |

观察：

- **Round 在 arc_tolerance=默认（0.25）下 M 爆炸**（square 4 顶点 →
  5316）——除非把 `arc_tolerance` 调大、否则 Round 不实用做 staircase
- **Square / Bevel 在闭合凸 polygon 上 M ≈ 2N**（每 corner 多 1 顶点）
- **TE-cusp 类失败 case**（airfoil_mh104_te_open）JoinType **无影响**——
  M=3 与 join 选择无关，cusp 还是被 Clipper2 吃穿。这进一步印证
  [Stage E pre-trim Phase C 负面结果](../../../../rnd/notes/dev/prevabs/dev-notes/archive/issue-20260522-stage-e-pretrim-phase-c.md)
  的判断：上游 inset 几何缺陷不在 JoinType 层面可修
- **open_v_cusp_shallow 上 4 种 JoinType drop range 各不相同**——浅
  cusp 场景对 JoinType 敏感，bevel/round 给"对称"的双侧 drop，
  miter/square 给"非对称"

#### 实验入口

`runOnce()` 第 4 个参数是 `JoinTypeChoice` 默认 `Miter`，第 5 个是
`miter_limit` 默认 2.0。`main()` 里对每 scenario 已经 sweep 全 4 种
JoinType（segment matcher）写到 `<scenario>_joins.svg`。

要 sweep `miter_limit`（影响 Miter join 的尖角"延伸度"上限）或者
其它 dist 值，直接在 `main()` 加 `runOnce(s, algo, false, jt, ml)`
调用即可。

## 依赖

- C++17
- Clipper2 1.4.0（CMake FetchContent，与主仓库同 pin）
- `include/offset_clipper2.hpp` + `include/geo_types.hpp` + `include/linalg.h`
- `src/geo/offset_clipper2.cpp` + `src/geo/offset_clipper2_bridge.cpp`

**零依赖**：PDCELVertex、PSegment、PModel、gmsh、spdlog、cpp-terminal、
Catch2、第三方 SVG 库。SVG 写文件是手撸字符串。

## 构建 + 运行

Windows MSVC：

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File build.ps1 fast -Run
```

或手工：

```powershell
mkdir build_msvc; cd build_msvc
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release
.\Release\test_offset_map.exe
```

## 输出

`build_msvc/out/` 下：

- `<scenario>.svg` — **matcher 对比** SVG（segment / nearest / dp
  / pretrim 各一栏），固定 JoinType=Miter + 默认 resample，每栏标注
  `ok`/`M`/`pairs`/`drops`
- `<scenario>_joins.svg` — **JoinType 对比** SVG（miter / square /
  bevel / round 各一栏），固定 matcher=segment + 默认 resample
- `<scenario>_raw.svg` — **resample 对照组** SVG（segment+raw /
  nearest+raw / dp+raw [+ pretrim+raw] 各一栏），关掉 open 路径
  `extractOpenRuns` 第 5 步 resample。详见 §"raw 对照组"
- `<scenario>_compare.svg` — **resample vs raw 配对 A/B** SVG。
  把 `<scenario>.svg` 与 `<scenario>_raw.svg` 的同名 matcher 行
  **交替排列**（segment / segment+raw / nearest / nearest+raw / ...），
  相邻面板直接显示"开/关 resample"对同一 matcher 的效果——这是看
  raw 对照组的最快路径
- `summary.csv` — 一行/scenario × (matcher 或 JoinType 或 +raw)，列：
  N, M, pairs, ok, n_drops, drops, pretrimmed, dist, pretrim_alpha

控制台同步打印汇总表（matcher 块 + JoinType 块 + raw 块）。

### SVG 图例

| 颜色 | 含义 |
|---|---|
| 蓝色 polyline + 点 | base（每点标号在第一栏） |
| 橙色 polyline + 点 | offset polygon |
| 灰色细线 | base ↔ offset 配对 |
| 红色圆圈 | dropped base 顶点（落进 dropped_base_ranges） |
| 紫色虚线 | pre-trim 后的 trimmed input（pretrim 栏） |

## 添加新场景

[test_offset_map.cpp](test_offset_map.cpp) 顶部 `make*` 函数批量加：

```cpp
Scenario makeMyCase() {
  return {"my_case",
          {SPoint2(...), ...},
          /*closed*/ false, /*side*/ +1, /*dist*/ 0.05,
          /*pretrim_alpha*/ 1.2};
}
```

然后在 `main()` 里 `scenarios.push_back(makeMyCase());`。重 build 即可。

### 加载 airfoil .dat

Selig 格式（首行 header，后面 `x y` 对）：

```cpp
auto pts = loadAirfoilDat(std::string(T0_DATA_DIR) + "/yourfoil.dat");
```

`T0_DATA_DIR` 由 CMake 注入，指向本目录。

## 几何/算法快速实验工作流

1. 改 [test_offset_map.cpp](test_offset_map.cpp) 加新 case 或调 α
2. `build.ps1 fast -Run`（~5 s）
3. 看 `build_msvc/out/<scenario>.svg` + 终端表
4. 若发现新构建方法：编辑
   [`src/geo/offset_clipper2_bridge.cpp`](../../../src/geo/offset_clipper2_bridge.cpp)
   ——本程序自动重 build 链入
5. 验证 OK 后跑主仓库 unit + integration 测试

## 与主集成测试的关系

| 维度 | t0_offset_clipper2（本程序） | t1..t9（主仓库 ctest） |
|---|---|---|
| 入口 | C++ 硬编码 / .dat 文件 | XML + `--hm` |
| 走过的 PreVABS 代码 | 仅 bridge 2 个 TU | 全套（DCEL + Segment + gmsh + homogenize） |
| 单 case 时间 | 毫秒 | 秒~10秒 |
| 失败口径 | offset / matcher 几何错 | 通常是下游 gmsh mesh 失败 |
| 用途 | **构建方法迭代** | 端到端回归 |
| CTest 集成 | 暂无（standalone） | `ctest --test-dir test/integration/build_msvc` |

本程序**不参与** `test\run-integration-tests.ps1` 主流程——它只是放在
同一目录方便编辑。未来若觉得需要把它纳入 CI，写一行 CTest 注册即可。

## 当前内置场景

| # | scenario | closed | N | dist | α | 预期 |
|---|---|---|---|---|---|---|
| 1 | closed_square | yes | 4 | 0.1 | — | 健康，三 matcher 等价 |
| 2 | open_straight | no | 4 | 0.05 | — | 健康，三 matcher 等价 |
| 3 | open_v_cusp_shallow | no | 11 | 0.05 | — | 浅 cusp，不需要 pre-trim |
| 4 | open_v_cusp_sharp | no | 11 | 0.05 | 1.2 | 深 cusp，pre-trim 触发 |
| 5 | closed_thin_te | yes | 10 | 0.04 | — | 闭合 TE 薄区，Stage C dropped range |
| 6 | airfoil_mh104_closed | yes | 67 | 0.02 | 1.2 | 真实 mh104（closed） |
| 7 | airfoil_mh104_te_open | no | 18 | 0.02 | 1.2 | mh104 TE 开口子段 |

## 相关文档

- [`include/offset_clipper2.hpp`](../../../include/offset_clipper2.hpp)
  — 三个 matcher + Stage E pre-trim helper 的契约
- [issue-20260522-stage-e-pretrim-phase-c.md](../../../../rnd/notes/dev/prevabs/dev-notes/archive/issue-20260522-stage-e-pretrim-phase-c.md)
  — Stage E pre-trim 负面结果（mh104 还差 1 个 M）
- [issue-20260522-dp-matcher-phase-d.md](../../../../rnd/notes/dev/prevabs/dev-notes/archive/issue-20260522-dp-matcher-phase-d.md)
  — DP matcher 三模式集成对比
- [issue-20260521-mh104-te-root-cause-not-pairing.md](../../../../rnd/notes/dev/prevabs/dev-notes/archive/issue-20260521-mh104-te-root-cause-not-pairing.md)
  — mh104 失败根因 + M/N 经验阈值 0.7
- [test/geo_library/clipper2/](../../geo_library/clipper2/) — 同
  philosophy 的更底层 Clipper2 独立测试
