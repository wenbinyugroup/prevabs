# Issue: Global `config` Singleton Couples Every Subsystem Invisibly

**Date:** 2026-02-14
**Severity:** Critical (design-review-20260213.md §13)
**Refactoring step:** Step 2 of §14

---

## Problem

`PConfig config` is a global struct with ~60 fields, defined in `main.cpp` and declared
`extern` in `globalVariables.hpp`. Every subsystem read it directly, creating coupling
invisible in function signatures:

```cpp
// Looks like it only needs a path:
void Segment::build(Message *pmessage)
// Actually reads: config.debug, config.tol, config.analysis_tool, ...
```

The following domain objects all read global `config`:

| File | Fields accessed |
|------|----------------|
| `src/geo/PDCEL.cpp` | `config.geo_tol` |
| `src/cs/CrossSection.cpp` | `config.debug` |
| `src/cs/PComponent.cpp` | `config.debug` |
| `src/cs/PBuildComponentLaminate.cpp` | `config.debug` |
| `src/cs/PSegment.cpp` | `config.debug` |
| `src/cs/PBuildSegmentAreas.cpp` | `config.tol`, `config.debug` |
| `src/cs/PArea.cpp` | `config.analysis_tool`, `config.debug` |
| `src/io/PModelIOSG.cpp` | `config.analysis_tool`, `config.dehomo`, `config.tool_ver`, `config.file_name_vsc` |

---

## Fix

Introduced two sub-structs in `include/globalVariables.hpp`:

```cpp
struct WriterConfig {
  int analysis_tool;         // 1: VABS, 2: SwiftComp
  bool dehomo;               // recovery analysis flag
  std::string tool_ver;      // tool version string (e.g. "2.2")
  std::string file_name_vsc; // path to the .sg file (read-back path)
};

struct BuilderConfig {
  bool debug;       // emit debug geometry plots
  int analysis_tool; // 1: VABS, 2: SwiftComp
  double tol;       // intersection parameter tolerance
  double geo_tol;   // geometric edge-length tolerance
};
```

Constructed once from global `config` at the pipeline entry points and passed
explicitly through the call chain.

### WriterConfig threading

- `PModel::writeSG(string, int, Message*)` → `PModel::writeSG(string, const WriterConfig&, Message*)`
- `writeSettingsVABS(FILE*, PModel*)` → `writeSettingsVABS(FILE*, PModel*, const WriterConfig&)`
- `writeSettingsSC(FILE*, PModel*)` → `writeSettingsSC(FILE*, PModel*, const WriterConfig&)`
- `readSG(const string&, PModel*, Message*)` → `readSG(const string&, PModel*, const WriterConfig&, Message*)`
- Call sites in `PModel.cpp` and `PModelIOPlot.cpp` construct `WriterConfig` from `config` before calling.

### BuilderConfig threading

- `CrossSection::build(Message*)` → `CrossSection::build(const BuilderConfig&, Message*)`
- `PComponent::build/buildDetails/buildLaminate` — all extended with `const BuilderConfig&`
- `Segment::build(Message*)` → `Segment::build(const BuilderConfig&, Message*)`
- `Segment::buildAreas(Message*)` → `Segment::buildAreas(const BuilderConfig&, Message*)`
- `PArea::buildLayers(Message*)` → `PArea::buildLayers(const BuilderConfig&, Message*)`
- `PDCEL::fixGeometry(Message*)` → `PDCEL::fixGeometry(const BuilderConfig&, Message*)`
- `PModel::build()` constructs `BuilderConfig bcfg{config.debug, config.analysis_tool, config.tol, config.geo_tol}` and passes it into the chain.

### Headers updated

Added `#include "globalVariables.hpp"` to: `PComponent.hpp`, `PSegment.hpp`, `PArea.hpp`,
`PDCEL.hpp` (all safe — `globalVariables.hpp` only includes system headers).

---

## Files Changed

- `include/globalVariables.hpp` — added `WriterConfig` and `BuilderConfig` structs
- `include/PModel.hpp` — updated `writeSG` signature
- `include/PModelIO.hpp` — updated `readSG`, `writeSettingsVABS`, `writeSettingsSC` signatures
- `include/CrossSection.hpp` — updated `build` signature
- `include/PComponent.hpp` — updated `build`, `buildDetails`, `buildLaminate` signatures; added `globalVariables.hpp`
- `include/PSegment.hpp` — updated `build`, `buildAreas` signatures; added `globalVariables.hpp`
- `include/PArea.hpp` — updated `buildLayers` signature; added `globalVariables.hpp`
- `include/PDCEL.hpp` — updated `fixGeometry` signature; added `globalVariables.hpp`
- `src/io/PModelIOSG.cpp` — updated implementations; replaced all `config.*` with `wcfg.*`
- `src/io/PModelIOPlot.cpp` — updated `readSG` call site
- `src/cs/PModel.cpp` — construct `WriterConfig` / `BuilderConfig` at entry points
- `src/cs/CrossSection.cpp` — replaced `config.debug` with `bcfg.debug`; threads `bcfg` down
- `src/cs/PComponent.cpp` — replaced `config.debug` with `bcfg.debug`; threads `bcfg` down
- `src/cs/PBuildComponentLaminate.cpp` — updated signature and propagation
- `src/cs/PSegment.cpp` — replaced `config.debug` with `bcfg.debug`
- `src/cs/PBuildSegmentAreas.cpp` — replaced `config.tol` and `config.debug` with `bcfg.*`
- `src/cs/PArea.cpp` — replaced `config.analysis_tool` and `config.debug` with `bcfg.*`
- `src/geo/PDCEL.cpp` — replaced `config.geo_tol` with `bcfg.geo_tol`

---

## Remaining Work

`PModel.cpp` itself still reads `config` directly for orchestration-level fields
(file paths, execution flags, etc.). This is intentional: the refactoring targets
domain objects that should be independent of the global config. Full removal of
`config` from `PModel` requires Steps 3–6 (see design-review-20260213.md §14).
