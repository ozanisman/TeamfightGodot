# Command reference

All Godot runs should go through [`run_godot.ps1`](../../run_godot.ps1) (logging to `logs/godot.log`, timeouts). Pass flags after `--` unless noted.

Canonical validation gate: [README#validation-gate](../../README.md#validation-gate).

## Core gate

| Flag | Script | Purpose | Timeout (s) |
|------|--------|---------|-------------|
| `--check-only` | `check_gdscript_preload.gd` (+ Python struct check) | GDScript preload/compile gate | 15 |
| `--check-native-load` | `check_native_load.gd` | GDExtension load smoke | 15 |
| `--check-native-simulation-tests` | `check_native_simulation_tests.gd` | Focused native effects/combat module tests | 120 |
| `--check-match-telemetry` | `check_match_telemetry.gd` | Match telemetry schema check | 15 |
| `--fixture-file=res://fixtures/goldens/match_fixtures.json` | `headless_bootstrap.gd` → `headless_runner.gd` | Real-champion golden parity (15 fixtures) | 120 |
| `--check-benchmark` | `check_benchmark.gd` | Throughput benchmark | 180 |
| `--check-benchmark-sharded` | `run_benchmark_sharded.ps1` | Sharded benchmark driver | 300 |

## UI smoke

| Flag | Script | Purpose | Timeout (s) |
|------|--------|---------|-------------|
| `--check-main-menu` | `check_main_menu_load.gd` | Main menu scene load | 30 |
| `--check-stats-dashboard` | `check_stats_dashboard_load.gd` | Stats dashboard loader | 30 |
| `--check-draft-ui` | `check_draft_ui_load.gd` | Draft testing UI load | 30 |

## Simulation checks

| Flag | Script | Purpose | Timeout (s) |
|------|--------|---------|-------------|
| `--check-determinism` | `check_determinism.gd` | Replay determinism probe | 30 |
| `--check-balance-patches` | `check_balance_patches.gd` | Balance patch + kit resolution | 120 |
| `--check-projectile-payloads` | `check_projectile_payloads.gd` | Projectile payload regression | 15 |
| `--check-large-projectile-damage` | `check_large_projectile_damage.gd` | Large projectile damage check | 15 |
| `--check-stats-aggregator` | `check_stats_aggregator_roundtrip.gd` | Stats CSV aggregator roundtrip | 120 |
| `--check-stats-csv-determinism` | `check_stats_csv_determinism.gd` | Stats CSV determinism | 240 |

## Draft validation and research

| Flag | Script | Purpose | Timeout (s) |
|------|--------|---------|-------------|
| `--generate-stats` | `generate_simulation_stats.gd` | Generate draft model CSVs under `model_stats/` | 120 |
| `--validate-native-strategy` | `file_based_validation.gd` | Native draft strategy validation | 30 |
| `--validate-full-draft` | `full_draft_validation.gd` | Full draft validation | 60 |
| `--full-draft-ab-test` | `full_draft_ab_test.gd` | Full draft A/B test | 600 |
| `--full-draft-ablation-test` | `full_draft_ablation_test.gd` | Full draft ablation test | 600 |
| `--full-draft-ban-diagnostic` | `full_draft_ban_diagnostic.gd` | Ban diagnostic | 300 |
| `--test-partial-comp-scoring` | `test_partial_comp_scoring.gd` | Partial comp scoring test | 60 |
| `--ab-test-draft-strategies` | `ab_test_draft_strategies.gd` | Strategy A/B comparison | 7200 |
| `--measure-draft-ceiling` | `measure_draft_ceiling.gd` | Draft ceiling measurement | 900 |
| `--verify-pairwise-signal` | `verify_pairwise_signal.gd` | Pairwise signal verification | 300 |
| `--generate-draft-probe-signals` | `generate_draft_probe_signals.gd` | Draft probe signal generation | 900 |
| `--validate-pick-recommendations` | `validate_pick_recommendations.gd` | Pick recommendation validation | 900 |
| `--generate-draft-aware-training-data` | `generate_draft_aware_training_data.gd` | Draft-aware training data | 900 |
| `--verify-draft-aware-signal` | `verify_draft_aware_signal.gd` | Draft-aware signal verification | 300 |
| `--validate-rollout-convergence` | `validate_rollout_convergence.gd` | Rollout convergence validation | 600 |

Unwired scripts (invoke Godot directly; see [draft_ai_validation_gate.md](draft_ai_validation_gate.md)):

| Script | Purpose |
|--------|---------|
| `native_draft_validation_harness.gd` | Full-draft round-robin tournaments with simulated match outcomes |
| `native_draft_validation_analyzer.gd` | Wilson CIs, A/B z-test, summary CSV |
| `native_draft_elo_ladder.gd` | Elo ratings from draft-summary CSV (`--self-test` for unit checks) |
| `native_draft_elo_gate.gd` | Relative Elo ordering gate (`--ordering=`, `--draft-summary=` freshness, `--min-gap=`) |
| `native_draft_quantitative_gate.gd` | Win-rate and side-bias regression gate |
| `native_draft_tier_gate.gd` | Difficulty tier monotonic separation gate (`--min-gap=`, `--self-test`) |
| `native_draft_self_play_stats.gd` | Policy-draft matches → production stats CSVs + manifest |
| `native_draft_self_play_stats_gate.gd` | Structural gate for self-play stats snapshots (`--min-matches=`) |
| `native_draft_lookahead_diagnostic.gd` | Per-step lookahead CSV; `--self-test` for C++ softmax expectation |
| `native_draft_lookahead_baseline_report.gd` | Markdown report from lookahead baseline analyzer CSV |
| `native_draft_lookahead_gate.gd` | Regression gate for `native_lookahead_softmax` |
| `run_draft_ai_validation_suite.gd` | Aggregate PASS/FAIL reports |

## Interactive UI

These flags omit `--headless` and maximize the window.

| Flag | Purpose |
|------|---------|
| `--main-menu` | Main menu hub (viewer / stats / draft) |
| `--simulation-viewer` | Simulation viewer directly |
| `--simulation-viewer --stats-dashboard` | Stats dashboard directly |

## Perf gate helpers (PowerShell)

Run from repo root; not routed through Godot directly.

| Script | Purpose |
|--------|---------|
| [`scripts/tools/run_perf_full_gate.ps1`](../../scripts/tools/run_perf_full_gate.ps1) | Build, check-only, fixtures, 5× bench w=1 and w=8 vs baseline |
| [`scripts/tools/run_perf_iteration_gate.ps1`](../../scripts/tools/run_perf_iteration_gate.ps1) | Single perf iteration compare/update vs `perf_gate_baseline.json` |

## Unwired scripts

Some tools (e.g. `run_draft_ai_validation_suite.gd`) are not routed in `run_godot.ps1`. Invoke Godot directly:

```powershell
godot --headless --path . --script res://scripts/tools/run_draft_ai_validation_suite.gd
```
