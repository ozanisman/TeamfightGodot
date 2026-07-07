# Draft AI validation gate

Quantitative regression pipeline for native draft AI and stats snapshots. Run after draft AI, strategy, or stats changes.

**Prerequisites:** Native build, `--check-only`, and core [validation gate](../../README.md#validation-gate) passing. Godot at `C:\Godot\godot.exe` (or on `PATH`). Script examples below use direct `godot`; equivalent wrapper form is `.\run_godot.ps1 -- --script <script> -- <args>`. Outputs under `model_stats/` and `logs/` are gitignored unless noted.

**Threshold baselines:** Section 6 of [draft_ai_improvement_plan.md](draft_ai_improvement_plan.md). Override any quantitative threshold via CLI (e.g. `--native_softmax_self_play_bias_max_pp=15.0`).

## Quick path (suite only)

After running the individual steps below (or their smoke equivalents), aggregate reports:

```powershell
godot --headless --path . --script res://scripts/tools/run_draft_ai_validation_suite.gd
```

Reads `logs/*_gate_report.md` and related artifacts; writes `logs/draft_ai_validation_suite_report.md`.

## Step 1 — Full-draft harness

Generates per-step validation CSV + draft-summary CSV (slowest step; scale `--trials` / `--sims-per-draft` as needed).

```powershell
godot --headless --path . --script res://scripts/tools/native_draft_validation_harness.gd `
  -- --trials=50 --sims-per-draft=25 `
     --blue-strategies=native_full,native_softmax,random,base_power_only `
     --red-strategies=native_full,native_softmax,random,base_power_only `
     --output=res://model_stats/native_draft_validation.csv `
     --draft-summary-output=res://model_stats/native_draft_validation_drafts.csv
```

**Smoke:** `--trials=2 --sims-per-draft=1` with a single strategy pairing.

## Step 2 — Analyzer (Wilson CIs / A/B)

```powershell
godot --headless --path . --script res://scripts/tools/native_draft_validation_analyzer.gd `
  -- --input=res://model_stats/native_draft_validation.csv `
     --draft-summary=res://model_stats/native_draft_validation_drafts.csv `
     --output=res://model_stats/native_draft_validation_summary.csv `
     --ab-output=res://model_stats/native_draft_validation_ab_report.csv `
     --native-strategy-names=native_full,native_softmax
```

## Step 2b — Elo ladder + ordering gate

Run **ladder then gate sequentially** (gate rejects stale ladder CSV vs draft-summary mtime).

```powershell
godot --headless --path . --script res://scripts/tools/native_draft_elo_ladder.gd `
  -- --draft-summary=res://model_stats/native_draft_validation_drafts.csv `
     --strategies=native_full,native_softmax,random,base_power_only

godot --headless --path . --script res://scripts/tools/native_draft_elo_gate.gd `
  -- --input=res://model_stats/native_draft_elo_ladder.csv `
     --draft-summary=res://model_stats/native_draft_validation_drafts.csv
```

## Step 3 — Quantitative gate

```powershell
godot --headless --path . --script res://scripts/tools/native_draft_quantitative_gate.gd `
  -- --summary=res://model_stats/native_draft_validation_summary.csv `
     --ab-report=res://model_stats/native_draft_validation_ab_report.csv `
     --output=res://logs/native_draft_quantitative_gate_report.md
```

## Step 3b — Difficulty tier calibration + gate

```powershell
godot --headless --path . --script res://scripts/tools/native_draft_validation_harness.gd `
  -- --trials=50 --sims-per-draft=25 `
     --blue-strategies=native_softmax_easy,native_softmax,native_softmax_hard `
     --red-strategies=random `
     --output=res://model_stats/native_draft_tier_calibration.csv `
     --draft-summary-output=res://model_stats/native_draft_tier_calibration_drafts.csv

godot --headless --path . --script res://scripts/tools/native_draft_validation_analyzer.gd `
  -- --input=res://model_stats/native_draft_tier_calibration.csv `
     --draft-summary=res://model_stats/native_draft_tier_calibration_drafts.csv `
     --output=res://model_stats/native_draft_tier_calibration_summary.csv `
     --ab-output=res://model_stats/native_draft_tier_calibration_ab_report.csv `
     --native-strategy-names=native_softmax,native_softmax_easy,native_softmax_hard

godot --headless --path . --script res://scripts/tools/native_draft_tier_gate.gd `
  -- --summary=res://model_stats/native_draft_tier_calibration_summary.csv `
     --draft-summary=res://model_stats/native_draft_tier_calibration_drafts.csv
```

## Step 3c — Self-play stats generation (optional)

Policy-driven full drafts → production stats CSVs + `stats_manifest.json` via `StatsCsvAggregator`. Writes a **new snapshot directory**; does not mutate `--stats-dir`.

```powershell
godot --headless --path . --script res://scripts/tools/native_draft_self_play_stats.gd `
  -- --drafts=1000 --sims-per-draft=1 `
     --blue-strategies=native_softmax --red-strategies=native_softmax `
     --stats-dir=res://model_stats/stats_output_100k `
     --output-dir=res://model_stats/stats_selfplay_native_softmax
```

Optional draft-state training rows:

```powershell
godot --headless --path . --script res://scripts/tools/native_draft_self_play_stats.gd `
  -- --drafts=50 --sims-per-draft=1 `
     --blue-strategies=native_softmax --red-strategies=native_softmax `
     --output-dir=res://model_stats/stats_selfplay_decision_rows_smoke `
     --decision-output=res://model_stats/draft_state_training_rows_smoke.csv

godot --headless --path . --script res://scripts/tools/native_draft_decision_rows_gate.gd `
  -- --input=res://model_stats/draft_state_training_rows_smoke.csv `
     --drafts=50 `
     --blue-strategies=native_softmax `
     --red-strategies=native_softmax
```

Validation-only learned scorer experiment over those rows:

```powershell
godot --headless --path . --script res://scripts/tools/native_draft_state_scorer_experiment.gd `
  -- --input=res://model_stats/draft_state_training_rows_smoke.csv `
     --output-dir=res://model_stats/draft_state_scorer_experiments/smoke `
     --min-rows=200 `
     --calibration-bins=10 `
     --split-repeats=3
```

The scorer experiment writes `draft_state_scorer_report.md`, metrics JSON, model JSON, per-row predictions, calibration bins, and repeated-split summaries. `STATUS: PASS` only means the offline learned scorer cleared the validation gate against the native `total_score` baseline; it does not promote or wire runtime policy.

Larger scorer validation:

```powershell
godot --headless --path . --script res://scripts/tools/native_draft_self_play_stats.gd `
  -- --drafts=200 --sims-per-draft=3 `
     --blue-strategies=native_softmax --red-strategies=native_softmax `
     --output-dir=res://model_stats/stats_selfplay_scorer_validation `
     --decision-output=res://model_stats/draft_state_scorer_validation.csv

godot --headless --path . --script res://scripts/tools/native_draft_decision_rows_gate.gd `
  -- --input=res://model_stats/draft_state_scorer_validation.csv `
     --drafts=200 `
     --blue-strategies=native_softmax `
     --red-strategies=native_softmax `
     --output=res://logs/native_draft_decision_rows_gate_scorer_validation.md

godot --headless --path . --script res://scripts/tools/native_draft_state_scorer_experiment.gd `
  -- --input=res://model_stats/draft_state_scorer_validation.csv `
     --output-dir=res://model_stats/draft_state_scorer_experiments/validation_native_softmax_200x3 `
     --min-rows=4000 `
     --calibration-bins=10 `
     --split-repeats=5
```

**Smoke:**

```powershell
godot --headless --path . --script res://scripts/tools/native_draft_self_play_stats.gd `
  -- --drafts=50 --sims-per-draft=1 `
     --blue-strategies=native_softmax --red-strategies=random `
     --output-dir=res://model_stats/stats_selfplay_smoke/

godot --headless --path . --script res://scripts/tools/native_draft_self_play_stats_gate.gd `
  -- --output-dir=res://model_stats/stats_selfplay_smoke/ --min-matches=50
```

Gate checks canonical CSVs, manifest hashes, min match count, non-empty `role_combinations` / matchup data, and `StatsDashboardLoader` load.

**Promotion:** Use the stats certification pipeline (step 3e) with explicit `--promote` after all gates pass. Without `--promote`, the candidate snapshot is validated but production stats are not overwritten.

## Step 3e — Stats certification pipeline (D.2)

Single headless command: self-play generation → structural gate → harness on candidate `--stats-dir` → analyzer → quantitative + Elo gates → certification gate. Optional `--promote` copies canonical CSVs + manifest to `--baseline-stats-dir` and writes `certification.stats_snapshot_id` to `model_stats/draft_ai_config.json`.

**Smoke:**

```powershell
godot --headless --path . --script res://scripts/tools/native_draft_stats_certification.gd `
  -- --drafts=50 --sims-per-draft=1 `
     --blue-strategies=native_softmax --red-strategies=native_softmax `
     --baseline-stats-dir=res://model_stats/stats_output_100k `
     --output-dir=res://model_stats/stats_selfplay_candidate `
     --trials=2 --harness-sims-per-draft=25 --min-matches=50
```

**Full (pre-promotion calibration):** `--drafts=1000 --trials=50 --harness-sims-per-draft=25`.

**Promote (explicit only):** add `--promote` to the smoke/full command above after a PASS report.

Report: `logs/native_draft_stats_certification_report.md` (`STATUS: PASS/FAIL`). Default harness with `--trials` ≤ 2 uses **smoke** calibration (relaxed quantitative floors, Elo min-gap 5.0); override with `--production` or force smoke with `--smoke`. Full pre-promotion calibration should use `--trials=50 --production`.

## Step 3d — Lookahead calibration + gate (optional)

Symmetric harness including `native_lookahead_softmax` (validation-only; not wired to gameplay):

```powershell
godot --headless --path . --script res://scripts/tools/native_draft_validation_harness.gd `
  -- --trials=50 --sims-per-draft=25 `
     --blue-strategies=native_lookahead_softmax,native_softmax,native_full,random `
     --red-strategies=native_lookahead_softmax,native_softmax,native_full,random `
     --output=res://model_stats/native_draft_lookahead_calibration.csv `
     --draft-summary-output=res://model_stats/native_draft_lookahead_calibration_drafts.csv

godot --headless --path . --script res://scripts/tools/native_draft_validation_analyzer.gd `
  -- --input=res://model_stats/native_draft_lookahead_calibration.csv `
     --draft-summary=res://model_stats/native_draft_lookahead_calibration_drafts.csv `
     --output=res://model_stats/native_draft_lookahead_calibration_summary.csv `
     --ab-output=res://model_stats/native_draft_lookahead_calibration_ab_report.csv `
     --native-strategy-names=native_lookahead_softmax,native_softmax,native_full

godot --headless --path . --script res://scripts/tools/native_draft_elo_ladder.gd `
  -- --draft-summary=res://model_stats/native_draft_lookahead_calibration_drafts.csv `
     --strategies=native_lookahead_softmax,native_softmax,native_full,random `
     --output-csv=res://model_stats/native_draft_lookahead_elo_ladder.csv

godot --headless --path . --script res://scripts/tools/native_draft_lookahead_gate.gd `
  -- --summary=res://model_stats/native_draft_lookahead_calibration_summary.csv `
     --ab-report=res://model_stats/native_draft_lookahead_calibration_ab_report.csv `
     --elo-ladder=res://model_stats/native_draft_lookahead_elo_ladder.csv `
     --output=res://logs/native_draft_lookahead_gate_report.md
```

**B.1 baseline** (legacy `native_lookahead` with `opponent_model=top1` config): same harness recipe with `native_lookahead` in the strategy list; report via `native_draft_lookahead_baseline_report.gd`. Per-step diagnostic: `native_draft_lookahead_diagnostic.gd` (`--config-mode=legacy`, `--turn-table`, `--self-test` validates C++ softmax expectation vs GDScript). Lookahead JSON presets are committed under `res://fixtures/draft_ai/`.

**Calibrated thresholds (2026-07-04, n=50):** self-play bias ≤15pp (observed 13.9pp); vs-random blue/overall ≥0.87 (observed 0.90/0.89); Elo gap vs `native_softmax` ≥−40 (observed −26; strength still below greedy softmax).

## Step 3f — Risk/persona promotion gate (validation-only)

Symmetric harness for the config-only persona experiments. The gate compares each persona against `native_softmax` on Elo, self-play side-bias, and A/B regression. Until realism metrics are available, otherwise passing personas are reported as `VALIDATION_ONLY`, not `PROMOTE`.

```powershell
godot --headless --path . --script res://scripts/tools/native_draft_validation_harness.gd `
  -- --trials=50 --sims-per-draft=25 `
     --blue-strategies=native_softmax,native_softmax_safe,native_softmax_ceiling,native_softmax_counter_heavy `
     --red-strategies=native_softmax,native_softmax_safe,native_softmax_ceiling,native_softmax_counter_heavy `
     --output=res://model_stats/native_draft_persona_validation.csv `
     --draft-summary-output=res://model_stats/native_draft_persona_validation_drafts.csv

godot --headless --path . --script res://scripts/tools/native_draft_validation_analyzer.gd `
  -- --input=res://model_stats/native_draft_persona_validation.csv `
     --draft-summary=res://model_stats/native_draft_persona_validation_drafts.csv `
     --output=res://model_stats/native_draft_persona_validation_summary.csv `
     --ab-output=res://model_stats/native_draft_persona_validation_ab_report.csv `
     --native-strategy-names=native_softmax,native_softmax_safe,native_softmax_ceiling,native_softmax_counter_heavy

godot --headless --path . --script res://scripts/tools/native_draft_elo_ladder.gd `
  -- --draft-summary=res://model_stats/native_draft_persona_validation_drafts.csv `
     --strategies=native_softmax,native_softmax_safe,native_softmax_ceiling,native_softmax_counter_heavy `
     --output-csv=res://model_stats/native_draft_persona_elo_ladder.csv

godot --headless --path . --script res://scripts/tools/native_draft_persona_realism_metrics.gd `
  -- --draft-summary=res://model_stats/native_draft_persona_validation_drafts.csv `
     --output=res://model_stats/native_draft_persona_realism_metrics.csv `
     --report=res://logs/native_draft_persona_realism_metrics_report.md

godot --headless --path . --script res://scripts/tools/native_draft_persona_gate.gd `
  -- --summary=res://model_stats/native_draft_persona_validation_summary.csv `
     --ab-report=res://model_stats/native_draft_persona_validation_ab_report.csv `
     --elo-ladder=res://model_stats/native_draft_persona_elo_ladder.csv `
     --draft-summary=res://model_stats/native_draft_persona_validation_drafts.csv `
     --realism-metrics=res://model_stats/native_draft_persona_realism_metrics.csv `
     --output=res://logs/native_draft_persona_gate_report.md
```

Gate defaults: persona Elo gap vs `native_softmax` must be ≥0; persona self-play side-bias must be at most `native_softmax` bias + 3pp; side-balanced direct A/B vs `native_softmax` must not show a significant regression at p ≤0.05; required realism metrics must stay within 3pp of `native_softmax`. Missing A/B or realism metrics produce `VALIDATION_ONLY`.

The analyzer emits per-matchup CI rows for every pairing; the persona gate derives each A/B check from the two direct cross-matchups (`persona` blue vs `native_softmax` red, plus `native_softmax` blue vs `persona` red).

Realism metrics are computed from the draft-summary CSV by aggregating each strategy's blue and red appearances. Unique pick/ban rates are normalized against the observed champion pool in that metrics run, and repeated openers are counted only within the same side/opponent context. Counter-pick rate is reported as `NOT_EVALUATED` for summary-only input and does not block the realism result.

Report `STATUS: FAIL` means at least one persona was rejected or the inputs were invalid. `VALIDATION_ONLY` rows are non-promotable but do not fail the gate by themselves.

## Stats manifest and certification

Stats snapshots include `stats_manifest.json` (schema v1): `snapshot_id`, `generated_at`, `catalog_version`, `generator_name`, `generator_args`, and per-file hashes (`String.hash()` 32-bit). Native `DraftStatsDatabase` warns on missing manifest, unsupported schema, invalid types, or hash mismatch.

Do not pass secrets in stats-generation CLI args (`generator_args` is recorded).

Optional hard fail — add to `model_stats/draft_ai_config.json`:

```json
{
  "certification": {
    "stats_snapshot_id": "<snapshot_id_from_manifest>"
  }
}
```

When set, native pick/ban calls return empty on snapshot ID mismatch. Harness and quantitative gate inherit this via the same native recommendation API.

## Script reference

| Script | Role |
|--------|------|
| `native_draft_validation_harness.gd` | Full-draft tournaments + sim outcomes |
| `native_draft_validation_analyzer.gd` | Wilson CIs, A/B z-test, summaries |
| `native_draft_elo_ladder.gd` | Elo from draft-summary CSV |
| `native_draft_elo_gate.gd` | Relative Elo ordering |
| `native_draft_quantitative_gate.gd` | Win-rate floors, side-bias ceilings |
| `native_draft_tier_gate.gd` | Tier monotonic separation |
| `native_draft_self_play_stats.gd` | Policy drafts → stats CSVs + manifest |
| `native_draft_self_play_stats_gate.gd` | Structural self-play snapshot gate |
| `native_draft_decision_rows_gate.gd` | Structural gate for optional draft-state training rows |
| `native_draft_lookahead_diagnostic.gd` | Per-step lookahead CSV + softmax self-test |
| `native_draft_lookahead_baseline_report.gd` | Markdown baseline from analyzer summary |
| `native_draft_lookahead_gate.gd` | `native_lookahead_softmax` bias/strength gate |
| `native_draft_persona_gate.gd` | Risk/persona Elo, side-bias, A/B promotion gate |
| `native_draft_persona_realism_metrics.gd` | Persona entropy, diversity, concentration, opener metrics |
| `native_draft_stats_certification.gd` | Full-chain stats regen + certification (D.2) |
| `native_draft_stats_certification_gate.gd` | Final certification gate (snapshot id, pick smoke) |
| `run_draft_ai_validation_suite.gd` | Aggregate PASS/FAIL reports |
| `draft_harness_core.gd` | Shared draft/sim helpers (library, not CLI) |

Wired `run_godot.ps1` flags: [command_reference.md](command_reference.md#draft-validation-and-research).

## Related

- [native_draft_ai.md](native_draft_ai.md) — architecture and scoring
- [draft_ai_improvement_plan.md](draft_ai_improvement_plan.md) — workstreams and baselines
- [native_draft_ai.md#validation-suite](native_draft_ai.md#validation-suite) — older file-based suite checks (`full_draft_validation`, audits)
