class_name DraftStatsCertificationGateCore
extends RefCounted

## Final certification checks for stats snapshot promotion pipeline.

const StatsManifestScript := preload("res://scripts/tools/stats_manifest.gd")
const ValidatePickRecommendationsCoreScript := preload("res://scripts/tools/validate_pick_recommendations_core.gd")
const DraftAiConfigScript := preload("res://scripts/tools/draft_ai_config.gd")

const DEFAULT_PICK_REGRET_SMOKE_STATES: int = 10
const DEFAULT_PICK_REGRET_CEILING: float = 0.06
const DEFAULT_PICK_SMOKE_ROLLOUTS_PER_CANDIDATE: int = 5
const DEFAULT_PICK_SMOKE_BASE_SEED: int = 910000


static func evaluate(params: Dictionary) -> Dictionary:
	var candidate_dir: String = String(params.get("candidate_dir", ""))
	var baseline_dir: String = String(params.get("baseline_dir", ""))
	var prior_gates_pass: bool = bool(params.get("prior_gates_pass", false))
	var run_pick_smoke: bool = bool(params.get("run_pick_smoke", true))
	var pick_smoke_states: int = maxi(1, int(params.get("pick_smoke_states", DEFAULT_PICK_REGRET_SMOKE_STATES)))
	var pick_regret_ceiling: float = float(params.get("pick_regret_ceiling", DEFAULT_PICK_REGRET_CEILING))
	var config_path: String = String(params.get("config_path", DraftAiConfigScript.DEFAULT_CONFIG_PATH))

	var checks: Array[Dictionary] = []

	var candidate_manifest: Dictionary = StatsManifestScript.read_manifest(candidate_dir)
	var baseline_manifest: Dictionary = StatsManifestScript.read_manifest(baseline_dir)
	var candidate_snapshot_id: String = String(candidate_manifest.get("snapshot_id", ""))
	var baseline_snapshot_id: String = String(baseline_manifest.get("snapshot_id", ""))

	checks.append({
		"name": "candidate_snapshot_id_present",
		"pass": not candidate_snapshot_id.is_empty(),
		"detail": candidate_snapshot_id if not candidate_snapshot_id.is_empty() else "missing",
	})

	var snapshot_changed: bool = false
	if candidate_snapshot_id.is_empty():
		snapshot_changed = false
	elif baseline_snapshot_id.is_empty():
		snapshot_changed = true
	else:
		snapshot_changed = candidate_snapshot_id != baseline_snapshot_id
	checks.append({
		"name": "snapshot_id_differs_from_baseline",
		"pass": snapshot_changed,
		"detail": "candidate=%s baseline=%s" % [candidate_snapshot_id, baseline_snapshot_id],
	})

	checks.append({
		"name": "prior_gates_pass",
		"pass": prior_gates_pass,
		"detail": "ok" if prior_gates_pass else "one or more prior gates failed",
	})

	var mean_regret: float = 0.0
	if run_pick_smoke:
		var pick_result: Dictionary = ValidatePickRecommendationsCoreScript.run({
			"states": pick_smoke_states,
			"rollouts_per_candidate": DEFAULT_PICK_SMOKE_ROLLOUTS_PER_CANDIDATE,
			"draft_depth": 4,
			"base_seed": DEFAULT_PICK_SMOKE_BASE_SEED,
			"stats_dir": candidate_dir,
			"config_path": config_path,
			"write_csv": false,
		})
		var pick_pass: bool = pick_result.get("ok", false) and float(pick_result.get("mean_regret", INF)) <= pick_regret_ceiling
		mean_regret = float(pick_result.get("mean_regret", 0.0))
		checks.append({
			"name": "pick_recommendation_smoke_regret",
			"pass": pick_pass,
			"observed": mean_regret,
			"threshold": pick_regret_ceiling,
			"detail": pick_result.get("error", "%.4f regret (max %.4f)" % [mean_regret, pick_regret_ceiling]),
		})

	var overall_pass: bool = true
	for check in checks:
		if not check["pass"]:
			overall_pass = false
			break

	return {
		"pass": overall_pass,
		"checks": checks,
		"candidate_snapshot_id": candidate_snapshot_id,
		"baseline_snapshot_id": baseline_snapshot_id,
		"mean_pick_regret": mean_regret,
	}
