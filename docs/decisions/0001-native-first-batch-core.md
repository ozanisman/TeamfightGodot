# 0001: Native-First Batch Core

Status: Accepted

## Context

The batch simulation path is being refactored toward a native-first, data-oriented core that can scale to 100k+ matches. The goals are:

- move the hot simulation loop out of GDScript
- keep GDScript as a thin orchestration and fallback layer
- preserve deterministic output and chunked stats writing
- make the native runtime the default once a compiled extension exists

## Decision

1. Use `CMake` for the native batch library build.
2. Scope the first native build to `Windows` only.
3. Keep the current `.gdextension` manifest layout and DLL path expectations.
4. Use a silent fallback in normal batch runs, with warning behavior reserved for dev/debug usage.
5. Introduce a strict DTO for native roster and unit materialization instead of using `CombatUnitState` as the native-facing shape.
6. Target `snapshot parity plus summary parity` first.
7. Gate progress on all three performance concerns:
   - matches/sec
   - memory ceiling
   - no allocation growth across shard runs

## Consequences

- The native build has a clear path to become the default runtime path once the DLL exists.
- The native boundary stays data-only and avoids gameplay object coupling.
- GDScript fallback remains available for parity, editor runs, and recovery.
- The implementation sequence is explicit:
  1. make the native extension buildable
  2. move roster materialization behind the native seam
  3. replace fallback world stepping with contiguous native state
  4. add parity checks
  5. remove fallback-only shims

## Notes

- The DTO boundary should be narrow and typed.
- The native core should not depend on `CombatUnitState` directly.
- The launcher wrapper should keep a hard timeout so parse/load hangs do not block workflow.
