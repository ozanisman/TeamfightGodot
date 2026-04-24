# Performance Improvement Implementation Plan - Executive Summary

## Overview

A phased optimization strategy has been created to improve the Teamfight Godot simulation throughput from **290 m/s** to **330-340 m/s** (13-20% improvement) by eliminating redundant calculations in the targeting system.

**Implementation Strategy:** Two-phase approach with sequential validation
- ✅ **Phase 1:** Memoize enemy team centers (5-10% improvement, 1 day)
- ✅ **Phase 2:** Cache bodyguard adjacency matrix (8-12% improvement, 1 day)
- 🚀 **Phase 3 (Stretch):** Early exit on stickiness (3-8%, deferred for later)

---

## Phase 1: Memoize Enemy Team Centers

### What & Why
- **Problem:** Flanking weight calculation recomputes enemy team center **50 times per 5v5 tick** even though it only changes between ticks
- **Solution:** Compute team centers once in `_prepare_tick_context()`, cache in `TickContext`, reuse in scoring
- **Expected Gain:** 5-10% throughput improvement (~15-30 m/s)

### Implementation
- Add team center caching to `TickContext` struct
- Precompute player and enemy team centers with validity flags
- Replace redundant calculations in flanking weight section
- **Files modified:** `native/src/teamfight_simulation_core.cpp`, `.hpp`

### Validation
```powershell
.\run_godot.ps1 -- --check-only                          # Syntax
.\run_godot.ps1 -- --fixture-file=res://fixtures/...     # Parity
.\run_godot.ps1 -- --check-benchmark --batch-count=2000  # Throughput (3× runs)
```

### Expected Results
- ✅ No compilation errors
- ✅ All 22 golden fixtures pass
- ✅ Median throughput: 305-320 m/s (vs 270 baseline)

---

## Phase 2: Cache Bodyguard Adjacency Matrix

### What & Why
- **Problem:** Bodyguard bonus checks iterate **all carries for every enemy candidate** during scoring (75+ redundant checks per tick)
- **Solution:** Precompute which enemies threaten which carries, cache distance values, use lookup instead of iteration
- **Expected Gain:** 8-12% throughput improvement (~25-35 m/s)

### Implementation
- Add `enemy_nearby_carries` map and `bodyguard_distance_cache` to `TickContext`
- Precompute adjacency matrix in `_prepare_tick_context()` using spatial distance check
- Replace bodyguard iteration loop with precomputed matrix lookup
- **Files modified:** `native/src/teamfight_simulation_core.cpp`, `.hpp`

### Validation
```powershell
.\run_godot.ps1 -- --check-only                          # Syntax
.\run_godot.ps1 -- --fixture-file=res://fixtures/...     # Parity
.\run_godot.ps1 -- --check-benchmark --batch-count=2000  # Throughput (3× runs)
```

### Expected Results
- ✅ No compilation errors
- ✅ All 22 golden fixtures still pass
- ✅ Median throughput: 330-350 m/s (cumulative 13-20% improvement)
- ✅ Additional 8-12% gain over Phase 1

---

## Cumulative Outcome (Both Phases)

### Performance Metrics

| Metric | Before | Target | Expected |
|--------|--------|--------|----------|
| 5v5 Throughput (m/s) | 290 | 330-340 | 330-350 |
| Targeting time (% of tick) | 87% | <75% | 75-80% |
| Per-match time (300 ticks) | 3.45 ms | 2.86 ms | 2.75-3.0 ms |

### Behavior Changes
- ✅ **ZERO** — All improvements are computational only, no behavior changes
- ✅ **Determinism maintained** — Same calculations, same results, just precomputed
- ✅ **Fixtures guaranteed to pass** — Golden validation built into each phase

### Code Quality
- ✅ Follows existing patterns (TickContext, RAII profiling guards, const-correctness)
- ✅ Maintains Godot best practices (bounds checking, null safety, early returns)
- ✅ Incremental validation enables rapid debugging if issues arise
- ✅ Two separate commits for clear code review and Git history

---

## Risk Assessment

### Very Low Risk (Mitigated)
- **Determinism broken:** Golden fixtures validate bit-exact results after each phase
- **Index out of bounds:** All accesses check `0 <= idx < size()`
- **Null/invalid data:** All cached values have validity flags (`has_player_center`, `has_enemy_center`)

### Low Risk (Pre-planned)
- **Performance regression:** Benchmark 3× on same machine before proceeding
- **Bodyguard matrix misses:** Bounds checking and cache key encoding prevents collisions
- **Team center edge cases:** Zero count handled with validity flags

### Handled
- **Determinism check:** `--check-determinism` validates parity
- **1v1 guardrail:** Run separate 1v1 benchmark (target ~850 m/s)
- **Trace consistency:** Compare combat events between old and new code

---

## Implementation Timeline

| Phase | Task | Duration | Total |
|-------|------|----------|-------|
| **1** | Code changes | 30 min | 1 day |
| | Validation | 30 min | |
| **2** | Code changes | 2 hr | 1 day |
| | Validation | 1 hr | |
| **Cumulative** | Final testing & regression | 2 hr | 2 hr |
| | | | **2-3 days total** |

---

## Success Criteria (Required)

- ✅ **Code compiles** without warnings on Release build
- ✅ **All 22 golden fixtures pass** after each phase
- ✅ **Median throughput improves** as planned (≥5-10% Phase 1, ≥8-12% Phase 2)
- ✅ **1v1 guardrail maintained** (~790 m/s, no regression)
- ✅ **Git commits created** with clear messages explaining each phase
- ✅ **Determinism maintained** (same seed = same results)

---

## Key Files Affected

### Modified
- `native/src/teamfight_simulation_core.hpp` — TickContext struct additions
- `native/src/teamfight_simulation_core.cpp` — Precomputation and usage

### Not Modified (No Breaking Changes)
- Python simulation code (reference implementation)
- GDScript harness code
- Godot scene/UI files
- Golden fixture files (binary validation only)
- Public API (same method signatures)

---

## Stretch Goal: Phase 3 (Deferred)

**Implementation:** Early exit on stickiness (skip candidate evaluation if current target "sticky enough")
- **Status:** Deferred until Phase 1-2 validated
- **Benefit:** Additional 3-8% improvement (350+ m/s if all 3 phases combined)
- **Risk:** Medium (early exit logic requires careful validation)
- **Decision:** Add only if Phase 1-2 deliver expected results and user wants additional optimization

---

## Git Workflow

```powershell
# 1. Create feature branch
git checkout -b optimization/targeting-memoization

# 2. Phase 1
# ... edit files ...
# ... validate ...
git add native/src/teamfight_simulation_core.cpp
git commit -m "Optimization: Memoize enemy team centers in TickContext ..."

# 3. Phase 2
# ... edit files ...
# ... validate ...
git add native/src/teamfight_simulation_core.cpp native/src/teamfight_simulation_core.hpp
git commit -m "Optimization: Cache bodyguard adjacency matrix in TickContext ..."

# 4. Merge to main
git checkout main && git merge optimization/targeting-memoization
```

---

## Validation Commands Reference

### Syntax Check
```powershell
.\run_godot.ps1 -- --check-only
```

### Golden Fixture Parity
```powershell
.\run_godot.ps1 -- --fixture-file=res://fixtures/goldens/match_fixtures.json
```

### Benchmark (3 runs, take median)
```powershell
.\run_godot.ps1 -- --check-benchmark --batch-count=2000 --team-size=5 --bench-skip-summaries --workers=1
```

### 1v1 Guardrail
```powershell
.\run_godot.ps1 -- --check-benchmark --batch-count=1000 --team-size=1 --workers=16
```

### Profile Breakdown
```powershell
.\run_godot.ps1 -- --check-benchmark --batch-count=500 --team-size=5 --bench-skip-summaries --sim-profile
```

---

## Next Steps

1. **Read the full plan** at `C:\Users\ozani\.claude\plans\cozy-nibbling-candy.md`
2. **Create feature branch** `optimization/targeting-memoization`
3. **Implement Phase 1** using code examples from plan
4. **Validate Phase 1** (fixtures + benchmarks)
5. **Commit Phase 1** with provided message template
6. **Implement Phase 2** (if Phase 1 succeeds)
7. **Validate Phase 2** (fixtures + cumulative benchmarks)
8. **Commit Phase 2** with provided message template
9. **Merge to main** when both phases validated

---

## Q&A

**Q: Why only two phases initially?**  
A: Phases 1-2 are lower complexity and provide 13-20% improvement with very high confidence. Phase 3 (early exit) is more complex and can be added later without architectural changes.

**Q: What if a phase doesn't improve performance as expected?**  
A: Each phase is committed independently. If Phase 1 regresses, we have git history to revert. Golden fixtures ensure correctness. Profiling data identifies where improvements did/didn't happen.

**Q: Can phases run in parallel?**  
A: No — each depends on prior validation. However, Phase 2 could be coded while Phase 1 is validating.

**Q: How long does validation take?**  
A: Fixtures (~2 min), benchmarks (~10 min each × 3 runs = 30 min), total ~40 min per phase.

**Q: Is determinism guaranteed?**  
A: Yes — we cache and reuse the same calculations, not change them. Golden fixtures validate this automatically.

