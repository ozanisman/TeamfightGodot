# Performance status

> **Agents:** validation commands and benchmark numbers only. Code layout → [native_agent_guide.md](native_agent_guide.md).

**Last updated:** 2026-05-28

## Benchmark (canonical gate)

Release build, **2000 matches**, **5v5**, **`--bench-skip-summaries`**, **workers=1**:

| Metric | Value |
|--------|-------|
| Matches/sec | **142.4** |
| `duration_sec` | 14.04 |
| Fixtures | **7/7** |

Recorded after native refactor + hygiene pass on a quiet host. Re-run the gate before merge or when claiming a perf change; numbers vary with background load.

**Regression compare (optional):** [`scripts/tools/run_perf_iteration_gate.ps1`](../../scripts/tools/run_perf_iteration_gate.ps1) vs [`scripts/tools/perf_gate_baseline.json`](../../scripts/tools/perf_gate_baseline.json) (2026-05-28 capture: w=1 median **146.2** m/s, w=8 median **628.7** m/s).

## Profiling snapshot (2026-05-28)

50 matches with `--sim-profile --targeting-profile`: `ns_update_units` dominates; within unit tick — `uu_targeting`, `uu_cooldowns_cc`, `uu_separation`; enemy scoring (`se_calls`); many ticks skip full retarget (`tgt_retarget_keeps` high).

Further ideas (not scheduled): [algorithmic_optimization_analysis.md](algorithmic_optimization_analysis.md).

## Validation gate

```text
cmake --build native/build --config Release
.\run_godot.ps1 -- --check-only
.\run_godot.ps1 -- --check-native-load
.\run_godot.ps1 -- --check-match-telemetry
.\run_godot.ps1 -- --fixture-file=res://fixtures/goldens/match_fixtures.json
.\run_godot.ps1 -- --check-benchmark --batch-count=2000 --team-size=5 --bench-skip-summaries --workers=1
```

Confirm no lingering Godot processes after runs.
