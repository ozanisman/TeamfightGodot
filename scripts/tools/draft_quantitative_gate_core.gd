class_name DraftQuantitativeGateCore
extends RefCounted

## Threshold evaluation for native draft validation summaries.

const DraftValidationCsvScript := preload("res://scripts/tools/draft_validation_csv.gd")

const DEFAULT_THRESHOLDS: Dictionary = {
	"native_full_vs_random_blue_min": 0.91,
	"native_full_vs_random_overall_min": 0.91,
	"native_full_self_play_bias_max_pp": 32.0,
	"native_softmax_vs_random_blue_min": 0.88,
	"native_softmax_vs_random_overall_min": 0.88,
	"native_softmax_self_play_bias_max_pp": 12.0,
}

# Relaxed floors for certification smoke (trials=2, harness on candidate snapshot).
const SMOKE_THRESHOLDS: Dictionary = {
	"native_full_vs_random_blue_min": 0.55,
	"native_full_vs_random_overall_min": 0.55,
	"native_full_self_play_bias_max_pp": 80.0,
	"native_softmax_vs_random_blue_min": 0.65,
	"native_softmax_vs_random_overall_min": 0.45,
	"native_softmax_self_play_bias_max_pp": 12.0,
}


static func smoke_thresholds() -> Dictionary:
	return SMOKE_THRESHOLDS.duplicate()


static func evaluate(
	summary: Array[Dictionary],
	ab_report: Array[Dictionary],
	threshold_overrides: Dictionary = {}
) -> Dictionary:
	var thresholds: Dictionary = DEFAULT_THRESHOLDS.duplicate()
	for key in threshold_overrides.keys():
		if thresholds.has(key):
			thresholds[key] = float(threshold_overrides[key])

	var metrics: Dictionary = _compute_metrics(summary, ab_report)
	var checks: Array[Dictionary] = _run_checks(metrics, thresholds)
	var overall_pass: bool = true
	for check in checks:
		if not check["pass"]:
			overall_pass = false
			break

	return {
		"pass": overall_pass,
		"checks": checks,
		"metrics": metrics,
		"thresholds": thresholds,
	}


static func evaluate_from_paths(
	summary_path: String,
	ab_report_path: String,
	threshold_overrides: Dictionary = {}
) -> Dictionary:
	var summary: Array[Dictionary] = _read_csv(summary_path)
	if summary.is_empty():
		return {"pass": false, "checks": [], "metrics": {}, "errors": ["summary empty or missing"]}

	var ab_report: Array[Dictionary] = []
	if not ab_report_path.is_empty() and FileAccess.file_exists(ProjectSettings.globalize_path(ab_report_path)):
		ab_report = _read_csv(ab_report_path)

	var result: Dictionary = evaluate(summary, ab_report, threshold_overrides)
	result["errors"] = []
	return result


static func _read_csv(path: String) -> Array[Dictionary]:
	return DraftValidationCsvScript.load_metric_rows(path)


static func _compute_metrics(summary: Array[Dictionary], ab_report: Array[Dictionary]) -> Dictionary:
	var metrics: Dictionary = {}
	for row in summary:
		var metric: String = row.get("metric", "")
		var grouping: String = row.get("grouping", "")
		if metric == "matchup":
			if grouping == "blue=native_full red=random":
				metrics["native_full_vs_random_blue"] = float(row["winrate"])
			elif grouping == "blue=native_full red=native_full":
				metrics["native_full_self_play_blue"] = float(row["winrate"])
				metrics["native_full_self_play_decisive_blue"] = _decisive_winrate(row)
			elif grouping == "blue=native_softmax red=random":
				metrics["native_softmax_vs_random_blue"] = float(row["winrate"])
			elif grouping == "blue=native_softmax red=native_softmax":
				metrics["native_softmax_self_play_blue"] = float(row["winrate"])
				metrics["native_softmax_self_play_decisive_blue"] = _decisive_winrate(row)
		elif metric == "native_winrate_by_opponent":
			if grouping == "native=native_full opponent=random":
				metrics["native_full_vs_random_overall"] = float(row["winrate"])
			elif grouping == "native=native_softmax opponent=random":
				metrics["native_softmax_vs_random_overall"] = float(row["winrate"])

	for row in ab_report:
		var metric: String = row.get("metric", "")
		var grouping: String = row.get("grouping", "")
		if metric == "matchup_winrate_ci":
			if grouping == "blue=native_full red=native_full":
				metrics["native_full_self_play_decisive_blue"] = float(row["decisive_winrate"])
			elif grouping == "blue=native_softmax red=native_softmax":
				metrics["native_softmax_self_play_decisive_blue"] = float(row["decisive_winrate"])

	if metrics.has("native_full_self_play_decisive_blue"):
		metrics["native_full_self_play_bias_pp"] = absf(2.0 * metrics["native_full_self_play_decisive_blue"] - 1.0) * 100.0
	if metrics.has("native_softmax_self_play_decisive_blue"):
		metrics["native_softmax_self_play_bias_pp"] = absf(2.0 * metrics["native_softmax_self_play_decisive_blue"] - 1.0) * 100.0

	return metrics


static func _decisive_winrate(row: Dictionary) -> float:
	var wins: int = int(row.get("wins", 0))
	var losses: int = int(row.get("losses", 0))
	var total: int = wins + losses
	if total <= 0:
		return 0.0
	return float(wins) / float(total)


static func _run_checks(metrics: Dictionary, thresholds: Dictionary) -> Array[Dictionary]:
	var checks: Array[Dictionary] = []
	var definitions: Array[Dictionary] = [
		{"name": "native_full_vs_random_blue", "kind": "min", "threshold": thresholds["native_full_vs_random_blue_min"]},
		{"name": "native_full_vs_random_overall", "kind": "min", "threshold": thresholds["native_full_vs_random_overall_min"]},
		{"name": "native_full_self_play_bias_pp", "kind": "max", "threshold": thresholds["native_full_self_play_bias_max_pp"]},
		{"name": "native_softmax_vs_random_blue", "kind": "min", "threshold": thresholds["native_softmax_vs_random_blue_min"]},
		{"name": "native_softmax_vs_random_overall", "kind": "min", "threshold": thresholds["native_softmax_vs_random_overall_min"]},
		{"name": "native_softmax_self_play_bias_pp", "kind": "max", "threshold": thresholds["native_softmax_self_play_bias_max_pp"]},
	]
	for def in definitions:
		var name: String = def["name"]
		var present: bool = metrics.has(name)
		var value: float = metrics.get(name, 0.0)
		var pass_: bool = false
		if present:
			if def["kind"] == "min":
				pass_ = value >= def["threshold"]
			else:
				pass_ = value <= def["threshold"]
		checks.append({
			"name": name,
			"present": present,
			"value": value,
			"threshold": def["threshold"],
			"kind": def["kind"],
			"pass": present and pass_,
			"reason": "missing" if not present else ("value below min" if def["kind"] == "min" and not pass_ else ("value above max" if def["kind"] == "max" and not pass_ else "ok"))
		})
	return checks
