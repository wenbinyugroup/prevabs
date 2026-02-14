# PreVABS Architecture Design Review — 2026-02-13

## Executive Summary

PreVABS implements genuinely complex algorithmic work — a Doubly-Connected Edge List (DCEL)
with sweep-line intersection, a two-phase laminate geometry construction, and a multi-format
structural analysis pipeline. The core algorithms are sound. The architectural problem is that
the system was grown incrementally from a small script into a 40-file application without
introducing abstraction boundaries between concerns. The result is a God object (`PModel`)
that acts simultaneously as a data repository, geometry builder, IO handler, solver launcher,
and post-processor, connected to every other subsystem through a web of raw back-pointers and
a global configuration singleton.

This document focuses on design and architecture. For bug-level findings (null dereferences,
memory leaks, specific line-level issues) see `review-20260213.md`.

---

## 1. Overall Dependency Graph

```
main.cpp
  ├── PModel  [GOD OBJECT — 60+ public methods, 464-line header]
  │     ├── PDCEL  [geometry kernel — reasonably clean]
  │     │     ├── PDCELVertex  ──► (Gmsh tags embedded)
  │     │     ├── PDCELHalfEdge ──► (Gmsh tags embedded)
  │     │     ├── PDCELFace  ──► PArea, Material, LayerType, (Gmsh tags + mesh control)
  │     │     └── PDCELHalfEdgeLoop
  │     │
  │     ├── CrossSection  ──► PModel*  [circular back-pointer]
  │     │     └── PComponent  ──► PModel*  [circular back-pointer]
  │     │           └── Segment  ──► PModel*  [circular back-pointer]
  │     │                 └── PArea  ──► PModel*  [circular back-pointer]
  │     │
  │     ├── Material / Lamina / LayerType / Layup / Ply / Layer
  │     ├── Baseline  ──► CrossSection*  [circular back-pointer]
  │     └── LocalState  [post-processing results]
  │
  ├── PModelIO  [free functions + PModel methods split across IO .cpp files]
  │     └── rapidxml
  ├── PModelBuildGmsh  [PModel methods in Gmsh .cpp files]
  │     └── Gmsh API
  ├── execu  [free functions; shell injection via std::system()]
  └── globalVariables  [PConfig singleton read by every translation unit]
```

The graph has no layers. Every subsystem can see and mutate every other subsystem's state
through the `PModel*` back-pointer or the global `config`.

---

## 2. The God Object: `PModel`

`PModel` is the single most critical architectural problem. It has ~60 public methods covering
five distinct concerns:

| Concern | Method examples |
|---|---|
| Repository (get/set/add) | `getMaterialByName()`, `addBaseline()`, `setCrossSection()` — 40+ methods |
| Build orchestration | `homogenize()`, `dehomogenize()`, `build()`, `initialize()`, `finalize()` |
| Gmsh meshing bridge | `buildGmsh()`, `createGmshVertices()`, `createGmshFaces()`, `writeGmsh()` |
| Solver output | `writeSG()`, `writeNodes()`, `writeElements()`, `writeGLB()`, `writeSupp()` |
| Post-processing | `plotDehomo()`, `plot()`, `readOutputDehomo()` |

A class responsible for five concerns cannot be tested for any of them in isolation. Every
unit test requires constructing the entire model to test even one lookup method.

**What `PModel` should become:**

```
MaterialRepository     owns and indexes Material, Lamina, LayerType, Layup
GeometryRepository     owns and indexes Baseline, PDCELVertex (named points)
CrossSectionModel      owns CrossSection → PComponent → Segment → PArea chain + DCEL
MeshData               owns node/element data after Gmsh run
PostProcessingData     owns LocalState results
PipelineController     holds all of the above; orchestrates the build pipeline
```

---

## 3. Circular Dependencies and `declarations.hpp`

The include graph has several circular dependencies:

- `PBaseLine.hpp` includes `CrossSection.hpp`; `CrossSection.hpp` includes `PBaseLine.hpp`
- `CrossSection` holds a `PModel*`; `PModel` holds a `CrossSection*`
- `PComponent`, `Segment`, `PArea` all hold `PModel*` back-pointers
- `PDCELFace` holds `PArea*`, `Material*`, `LayerType*`; those classes are defined in files
  that include DCEL headers

These are resolved in practice only by `declarations.hpp` — a catch-all header that includes
every major header in the codebase, has no `#pragma once`, and effectively creates a single
global translation unit for declarations. It is the architectural equivalent of duct tape.

**Root cause:** The back-pointers from child objects to `PModel*` exist so that child classes
can perform their own lookups (e.g., `Segment::build()` calls `_pmodel->getMaterialByName()`).
This design decision propagates coupling through the entire hierarchy.

**Fix direction:** Remove the `PModel*` back-pointers. Pass what each builder actually needs
as parameters or inject a narrow interface. For example:

```cpp
// Instead of: void Segment::build(Message *msg)
//                   { ... _pmodel->getMaterialByName(name) ... }

// Pass a narrow lookup interface:
class IMaterialLookup {
public:
  virtual Material* findByName(const std::string& name) const = 0;
};

void Segment::build(const IMaterialLookup& materials, Message* msg);
```

This breaks the circular dependency without requiring a full restructure at once.

---

## 4. Gmsh Concerns Embedded in the Geometry Kernel

`PDCELVertex`, `PDCELHalfEdge`, and `PDCELFace` each store Gmsh integer tags:

- `PDCELVertex::_gvertex_tag`, `_gbuild`
- `PDCELHalfEdge::_gedge_tag`
- `PDCELFace::_gface_tag`, `_embedded_gvertex_tags`, `_embedded_gedge_tags`,
  `_gmsh_physical_group_tag`, `_mesh_size`

`PDCELFace` is the worst offender: it conflates topology, material assignment, local fiber
orientation, Gmsh rendering state, mesh size control, and physical group assignment — five
distinct concerns in 108 lines.

**Consequence:** The DCEL geometry kernel cannot be used, tested, or reused without Gmsh.
Swapping the mesher would require modifying every geometry primitive class.

**Fix direction:** Move Gmsh tags out of DCEL nodes into an external map owned by the Gmsh
bridge:

```cpp
// In PModelBuildGmsh.cpp, not in PDCELVertex.hpp:
std::unordered_map<PDCELVertex*, int> _vertex_gmsh_tags;
std::unordered_map<PDCELFace*, int>   _face_gmsh_tags;
```

The Gmsh bridge reads pure DCEL topology and writes tags into its own map. The geometry
kernel becomes rendering-agnostic.

---

## 5. IO Architecture

IO is split across three different patterns simultaneously:

1. **Free functions in `PModelIO.*`** that take `PModel*` and mutate it (reading XML)
2. **Methods on `PModel`** implemented in separate IO `.cpp` files (`writeSG()`, `writeGLB()`)
3. **A method on `PModel`** that calls Gmsh API to get data then writes files (`writeGmsh()`)

The `PModelIO.hpp` header is the single largest coupling point in the codebase: it
transitively pulls in rapidxml, Material, PBaseLine, PDCELFace, PDCELVertex, PDataClasses,
PModel, declarations, utilities, and platform headers. Including this header in any translation
unit effectively includes the entire project.

`writeSG()` contains interleaved VABS and SwiftComp format logic controlled by
`config.analysis_tool == 1` / `config.analysis_tool == 2` conditionals. Adding a third output
format requires editing this method and every helper it calls.

**Fix direction:** Introduce output format interfaces:

```cpp
struct SGWriterOptions { /* format-independent settings */ };

class ISGWriter {
public:
  virtual void write(const CrossSectionModel&, const MeshData&,
                     const SGWriterOptions&, const std::string& path) = 0;
  virtual ~ISGWriter() = default;
};

class VABSWriter : public ISGWriter { ... };
class SwiftCompWriter : public ISGWriter { ... };
```

Select the writer at startup based on `config.analysis_tool`, then pass it through the
pipeline. The `config.analysis_tool == N` conditionals disappear from the write path.

The same pattern applies to the reader side — an `ICrossSectionReader` that returns a
domain model rather than mutating a `PModel*`.

---

## 6. The Build Pipeline

The two-phase build is an informal design that emerged by accretion:

**Phase 1 — `build()`:** Iterates components, calls `cmp->build()` which calls DCEL
`addEdge()` / `addFace()`. After each component, `CrossSection::build()` calls
`_pmodel->dcel()->removeTempLoops()` and `createTempLoops()` — the cross-section class
directly manages DCEL loop lifecycle.

**Phase 2 — `buildDetails()`:** Iterates components again, calls `cmp->buildDetails()` →
`sgm->buildAreas()` → `PArea::buildLayers()` which subdivides faces into per-layer regions.

These two passes over the same component list are not documented as a design decision. They
indicate that a single-pass design was insufficient and a second pass was bolted on. The DCEL
`createTempLoops()`/`removeTempLoops()` calls leaking out of DCEL into `CrossSection::build()`
are an abstraction violation — a higher-level class should not orchestrate the internal loop
lifecycle of the geometry kernel.

**Fix direction:** Introduce an explicit `GeometryBuilder` that owns the two-phase
construction:

```cpp
class GeometryBuilder {
public:
  void buildGrossGeometry(CrossSectionModel&);    // Phase 1
  void buildLayerGeometry(CrossSectionModel&);    // Phase 2
  void finalizeGeometry(PDCEL&);                  // fixGeometry
private:
  void processTempLoops(PDCEL&, PComponent&);     // loop lifecycle hidden here
};
```

---

## 7. Global Configuration as an Invisible Coupling

`PConfig config` is a global struct with ~60 fields, defined in `main.cpp` and declared
`extern` in `globalVariables.hpp`. Every subsystem reads it directly. This creates coupling
that is invisible in function signatures:

```cpp
// Looks like it only needs a path:
void PModel::writeSG(const std::string &file_sg)
// Actually reads: config.analysis_tool, config.model_type,
//                 config.analysis_model, config.format_float, ...
```

The function's actual dependencies are hidden from callers. This makes the code impossible
to unit test (cannot set config without setting every field) and impossible to reason about
without reading the entire implementation.

**Fix direction:** Make configuration dependencies explicit. At minimum, pass a sub-struct
to each subsystem:

```cpp
struct WriterConfig {
  int analysis_tool;
  int model_type;
  bool format_float;
};

void writeSG(const std::string& path, const WriterConfig& cfg);
```

Longer term, the `processConfigVariables()` function in `main.cpp` (which derives ~20
secondary fields from the raw CLI inputs) is the right place to construct these sub-structs
once and distribute them.

---

## 8. O(n) Name Lookups

`PModel` has seven `getXxxByName()` methods, all O(n) linear scans over `std::vector<T*>`:

```cpp
Material*   getMaterialByName(std::string name);     // scans _materials
Lamina*     getLaminaByName(std::string name);        // scans _laminas
LayerType*  getLayerTypeByMaterialAngle(...);         // scans _layertypes (double loop)
Layup*      getLayupByName(std::string name);         // scans _layups
// ... plus 3 more
```

Equivalent free-function versions exist in `utilities.hpp`. `CrossSection` also has its own
scan methods. These lookups are called from tight build loops (`PComponent::buildLaminate()`
calls `getMaterialByName()` for each segment of each component). For large cross-sections with
many layers and materials, this is quadratic in the number of lookups.

**Fix:** Use `std::unordered_map<std::string, T*>` (or `unique_ptr`) keyed by name for all
named-object repositories. Construction cost is identical; lookup becomes O(1).

---

## 9. Static Counters as Hidden Global State

Three classes expose mutable static counters initialized in a completely unrelated file:

```cpp
// Defined in PModelIOReadCrossSection.cpp:52-55 — an IO file:
int CrossSection::used_material_index = 0;
int PComponent::count_tmp = 0;
int Segment::count_tmp = 0;
```

These counters are used to assign sequential IDs during XML parsing. Because they are static
class members initialized once at program start, calling `readCrossSection()` a second time
(which does happen: both `homogenize()` and `dehomogenize()` call `readInputMain()`) will
produce IDs that continue from where the first call left off, rather than starting fresh.

This is a non-reentrant design: the program cannot correctly process more than one
cross-section per session.

**Fix:** Move these counters to local variables in the read functions, threaded through the
call stack or stored in a parse-context struct:

```cpp
struct ParseContext {
  int material_index = 0;
  int component_count = 0;
  int segment_count = 0;
};
```

---

## 10. Memory Ownership Model

The intended hierarchy is: `PModel` → `CrossSection` → `PComponent` → `Segment` → `PArea`.
The actual ownership is entirely informal — all pointers are raw, no destructor in the chain
performs cleanup, and the lifetime of every object is effectively `main()`-scoped.

```
Owner           Holds                        Freed?
──────────────  ───────────────────────────  ──────
PModel          vector<Material*>            No
PModel          vector<Baseline*>            No
PModel          vector<LayerType*>           No
PModel          CrossSection*                No
PModel          PMesh*                       No
PModel          vector<LocalState*>          No
CrossSection    list<PComponent*>            No
PComponent      vector<Segment*>             No
Segment         vector<PArea*>               No
PArea           list<PDCELFace*>             N/A (DCEL owns)
PDCEL           vector<PDCELVertex*>         Yes ✓
PDCEL           vector<PDCELHalfEdge*>       Yes ✓
PDCEL           vector<PDCELFace*>           Yes ✓
PDCEL           vector<PDCELHalfEdgeLoop*>   No ✗
PMesh           vector<PNode*>               No
```

The DCEL destructor is the only one that performs meaningful cleanup. Everything above it in
the hierarchy leaks.

**Fix:** Convert the ownership chain to `unique_ptr` from top to bottom, starting with the
most impactful:
1. `main.cpp`: `unique_ptr<PModel>`, `unique_ptr<Message>`
2. `PModel`: `unique_ptr<CrossSection>`, `vector<unique_ptr<Material>>`, etc.
3. `CrossSection`: `list<unique_ptr<PComponent>>`
4. And so on down the chain.

Since objects are created in IO code and handed to `PModel`, the pattern should be:

```cpp
auto cs = std::make_unique<CrossSection>(...);
pmodel->setCrossSection(std::move(cs));  // PModel takes ownership
```

---

## 11. Segment: A Class That Does Too Much

`Segment` has 20+ member variables. They fall into two categories:

**Permanent domain data** (should stay on Segment):
- `_pbaseline`, `_layup`, `_layup_side`, `_level`, `_u_start`, `_u_end`

**Temporary build state** (should move to a build context):
- `_inner_bounds`, `_inner_bounds_tt`, `_inner_bounds_dc` — three lists of boundary data
- `_index_offset_to_base`, `_index_base_to_offset` — index arrays for topology linking
- `_link_offset_to_base_index` — offset vertex link indices
- `_bounding_vertices`, `_bounding_vertices_index` — transient bounding data

After `buildDetails()` completes, these temporary members serve no further purpose but remain
allocated for the lifetime of the process. This is an accretion of build-time scratch state
onto a domain class.

**Fix:** Extract a `SegmentBuildContext` struct:
```cpp
struct SegmentBuildContext {
  std::vector<std::vector<PDCELVertex*>> inner_bounds;
  std::vector<int> index_offset_to_base;
  // ...
};
```
Pass it through the build methods and discard it when the build phase completes.

---

## 12. Test Architecture

There are effectively no automated unit tests:
- `test/unittest/unittest.cpp` contains three test functions; two are commented out.
- The single active test covers one geometric utility (`findParamPointOnPolyline`).
- No tests exist for DCEL operations, material parsing, XML reading, SG writing, or
  any geometry build step.

The root cause is architectural: the God object, global config, and pervasive raw back-pointers
make the codebase structurally untestable. You cannot construct a `Segment` without a `PModel`.
You cannot test `readCrossSection()` without a `PConfig` global. You cannot test `writeSG()`
without running Gmsh.

Each of the refactoring directions described above (narrow interfaces, dependency injection,
separation of build state) also makes the code testable. The two goals are inseparable.

---

## 13. Summary of Architectural Issues by Severity

| Severity | Issue |
|---|---|
| **Critical** | `PModel` God object — 60 public methods across 5 concerns |
| **Critical** | Global `config` singleton couples every subsystem invisibly |
| **Critical** | Back-pointers from every domain object to `PModel*` |
| **Critical** | `declarations.hpp` has no `#pragma once` and is a catch-all aggregator |
| **High** | Gmsh tags embedded in DCEL geometry primitives |
| **High** | IO is three different patterns simultaneously (free functions, PModel methods, Gmsh-coupled methods) |
| **High** | VABS/SwiftComp selection via `config.analysis_tool == N` conditionals throughout write path |
| **High** | No abstract interfaces — system is impossible to mock or extend |
| **High** | Ownership model entirely informal; pervasive raw pointer leaks |
| **Medium** | `CrossSection::build()` directly manages DCEL loop lifecycle (abstraction leak) |
| **Medium** | Two-phase `build()` / `buildDetails()` undocumented; rationale invisible |
| **Medium** | O(n) linear scans for all named-object lookups |
| **Medium** | Static counters on domain classes initialized in unrelated IO file; non-reentrant |
| **Medium** | `Segment` accumulates 20+ members; temporary build state not separated |
| **Medium** | No unit test infrastructure; codebase is structurally untestable as-is |
| **Low** | `Material.hpp` contains 8 classes; natural to split into material and layup headers |
| **Low** | `utilities.cpp` mixes geometry math, string parsing, XML helpers, and object lookups |

---

## 14. Recommended Refactoring Sequence

These are ordered to deliver maximum value with minimum disruption. Each step leaves the
system in a working state.

**Step 1 — Immediate (no design change required):**
- Add `#pragma once` to `declarations.hpp`
- Replace raw `new` in `main.cpp` with `unique_ptr`
- Add `~PDCEL` loop cleanup; add `~PMesh` node/element cleanup

**Step 2 — Break global config dependency:**
- Introduce per-subsystem config sub-structs (`WriterConfig`, `BuilderConfig`)
- Pass them explicitly; stop reading `config` inside DCEL, CrossSection, Segment, PArea

**Step 3 — Remove `PModel*` back-pointers from domain objects:**
- Introduce narrow lookup interfaces (`IMaterialLookup`, `IBaselineLookup`)
- Pass them as parameters to `build()` methods
- This also breaks the circular includes that `declarations.hpp` was papering over

**Step 4 — Extract Gmsh tags from DCEL primitives:**
- Move tags to `unordered_map` in `PModelBuildGmsh`
- DCEL becomes rendering-agnostic; unit tests for DCEL topology become possible

**Step 5 — Introduce IO interfaces:**
- `ISGWriter` with `VABSWriter` and `SwiftCompWriter` implementations
- Remove `config.analysis_tool == N` branching from write path

**Step 6 — Split `PModel`:**
- Extract `MaterialRepository`, `GeometryRepository`, `MeshData`, `PostProcessingData`
- `PipelineController` coordinates them
- `PModel` becomes a thin compatibility shim that can be removed incrementally

**Step 7 — Add unit tests:**
- Once steps 3–4 are done, DCEL tests become possible without constructing `PModel`
- Once step 5 is done, writer tests become possible with mock mesh data
- Once step 6 is done, repository tests become possible in isolation
