# Issue: `PModel*` Back-Pointers Couple Every Domain Object to the God Object

**Date:** 2026-02-14
**Severity:** Critical (design-review-20260213.md §13)
**Refactoring step:** Step 3 of §14

---

## Problem

`CrossSection`, `PComponent`, `Segment`, and `PArea` each held a `PModel *_pmodel`
member used inside their `build` / `buildDetails` / `buildLayers` methods.
The back-pointer had three distinct uses:

| Usage | Count | Method |
|-------|-------|--------|
| `_pmodel->dcel()->X()` — DCEL mutations | 65 | across all build methods |
| `_pmodel->plotGeoDebug(Message*)` — debug vis | 3 | CrossSection, PBuildComponentLaminate, PComponent |
| `_pmodel->getLayerTypeByMaterialAngle(...)` — material lookup | 1 | PBuildComponentFilling |

All three created an invisible dependency on `PModel` in domain objects that should be independent.

A `join.cpp` also had a stale `config.debug` read that was missed in Step 2.

---

## Fix

### New `IMaterialLookup` interface

Defined in `include/globalVariables.hpp`:

```cpp
struct IMaterialLookup {
  virtual LayerType* getLayerTypeByMaterialAngle(Material*, double) = 0;
  virtual ~IMaterialLookup() = default;
};
```

`PModel : public IMaterialLookup` — adds the inheritance declaration.

### Extended `BuilderConfig`

```cpp
struct BuilderConfig {
  bool debug;
  int analysis_tool;
  double tol;
  double geo_tol;
  PDCEL* dcel = nullptr;                     // replaces _pmodel->dcel()
  IMaterialLookup* materials = nullptr;       // replaces _pmodel->getLayerTypeByMaterialAngle()
  std::function<void(Message*)> plotDebug;   // replaces _pmodel->plotGeoDebug()
};
```

### Construction in `PModel::build()`

```cpp
BuilderConfig bcfg{
  config.debug, config.analysis_tool, config.tol, config.geo_tol,
  _dcel, this,
  [this](Message *m) { this->plotGeoDebug(m); }
};
```

### Private helper signatures updated in `PComponent`

`joinSegments` (both overloads) and `createSegmentFreeEnd` now receive
`const BuilderConfig &bcfg` before `Message*`. Calls propagated from
`buildLaminate(const BuilderConfig&, Message*)`.

### `buildFilling` updated

Signature changed to `buildFilling(const BuilderConfig&, Message*)`;
calls propagated from `build(const BuilderConfig&, Message*)`.

### `IBaselineLookup` — not applicable

Comprehensive grep found zero `_pmodel->getBaseline*()` calls in any
build method; baselines are stored directly in `Segment`/`Filling` at
construction time. No interface needed.

---

## Files Changed

- `include/globalVariables.hpp` — added `IMaterialLookup`; extended `BuilderConfig` with `dcel`, `materials`, `plotDebug`; added `<functional>` and forward decls
- `include/PModel.hpp` — `class PModel : public IMaterialLookup`; added `globalVariables.hpp` include
- `include/PComponent.hpp` — removed `_pmodel` member, `PComponent(PModel*)` and `PComponent(string, PModel*)` constructors, `pmodel()` getter; updated `createSegmentFreeEnd`, `joinSegments` (×2), `buildFilling` signatures
- `include/CrossSection.hpp` — removed `_pmodel` member, `CrossSection(string, PModel*)` constructor, `pmodel()` getter, `setPModel()`
- `include/PSegment.hpp` — removed `_pmodel` member, `Segment(PModel*, ...)` constructor, `pmodel()` getter, `setPModel()`
- `include/PArea.hpp` — removed `_pmodel` member, `PArea(PModel*, Segment*)` constructor, `pmodel()` getter; removed `PModel.hpp` include
- `src/cs/PModel.cpp` — extended `BuilderConfig` construction to include `_dcel`, `this`, lambda
- `src/cs/CrossSection.cpp` — replaced `_pmodel->dcel()->` → `bcfg.dcel->`; replaced `_pmodel->plotGeoDebug()` → `bcfg.plotDebug()`; removed `CrossSection(string, PModel*)` implementation
- `src/cs/PComponent.cpp` — replaced `_pmodel->plotGeoDebug()` → `bcfg.plotDebug()`; passes `bcfg` to `buildFilling`
- `src/cs/PBuildComponentLaminate.cpp` — replaced `_pmodel->dcel()->` → `bcfg.dcel->`; replaced `_pmodel->plotGeoDebug()` → `bcfg.plotDebug()`; updated all `joinSegments` / `createSegmentFreeEnd` call sites
- `src/cs/PBuildComponentFilling.cpp` — updated `buildFilling` signature; replaced all `_pmodel->dcel()->` → `bcfg.dcel->`; replaced `_pmodel->getLayerTypeByMaterialAngle` → `bcfg.materials->getLayerTypeByMaterialAngle`
- `src/cs/join.cpp` — updated all three method signatures; replaced all `_pmodel->dcel()->` → `bcfg.dcel->`; fixed missed `config.debug` → `bcfg.debug`
- `src/cs/PSegment.cpp` — replaced `_pmodel->dcel()->` → `bcfg.dcel->`
- `src/cs/PBuildSegmentAreas.cpp` — replaced `_pmodel->dcel()->` → `bcfg.dcel->`; changed `new PArea(_pmodel, this)` → `new PArea(this)` (×2)
- `src/cs/PArea.cpp` — replaced `_segment->pmodel()->dcel()->splitFace` → `bcfg.dcel->splitFace`; updated `PArea::PArea()` and `PArea::PArea(PModel*, Segment*)` constructors
- `src/io/PModelIOReadCrossSection.cpp` — `new CrossSection(csName, pmodel)` → `new CrossSection(csName)`
- `src/io/PModelIOReadComponent.cpp` — `new PComponent(pmodel)` → `new PComponent()`
- `src/io/PModelIOReadComponentLaminate.cpp` — removed `pmodel` arg from 4 `new Segment(...)` calls

---

## Remaining Work

`PModel.cpp` still reads `config` directly and `PModel` itself still owns the
DCEL, material maps, and baseline lists. Full decomposition of `PModel` requires
Steps 4–6 (see design-review-20260213.md §14).
