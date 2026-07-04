extends SceneTree

## Reads the per-step CSV from native_draft_validation_harness.gd (or a draft
## summary CSV) and emits grouped summary metrics.
##
## Usage:
##   godot --headless --script res://scripts/tools/native_draft_validation_analyzer.gd \
##     -- --draft-summary=res://model_stats/native_draft_validation_drafts.csv \
##     --output=res://model_stats/native_draft_validation_summary.csv

const DraftValidationAnalyzerCoreScript := preload("res://scripts/tools/draft_validation_analyzer_core.gd")


func _extract_argument(prefix: String, default_value: String) -> String:
	for a in OS.get_cmdline_user_args():
		var arg: String = str(a)
		if arg.begins_with(prefix):
			return arg.substr(prefix.length())
	return default_value


func _parse_float_arg(prefix: String, default_value: String, min_val: float, max_val: float) -> float:
	var raw: String = _extract_argument(prefix, default_value)
	if not raw.is_valid_float():
		push_warning("native_draft_validation_analyzer: invalid float for %s (%s), using default %s" % [prefix, raw, default_value])
		raw = default_value
	return clampf(float(raw), min_val, max_val)


func _init() -> void:
	call_deferred("_run")


func _run() -> void:
	var result: Dictionary = DraftValidationAnalyzerCoreScript.run({
		"input_path": _extract_argument("--input=", ""),
		"draft_summary_path": _extract_argument("--draft-summary=", ""),
		"output_path": _extract_argument("--output=", "res://model_stats/native_draft_validation_summary.csv"),
		"ab_output_path": _extract_argument("--ab-output=", "res://model_stats/native_draft_validation_ab_report.csv"),
		"native_strategy_names": _extract_argument("--native-strategy-names=", "native_full,native_softmax"),
		"ab_control_blue": _extract_argument("--ab-control-blue=", ""),
		"ab_control_red": _extract_argument("--ab-control-red=", ""),
		"ab_treatment_blue": _extract_argument("--ab-treatment-blue=", ""),
		"ab_treatment_red": _extract_argument("--ab-treatment-red=", ""),
		"ab_mde": _parse_float_arg("--ab-mde=", "0.05", 0.001, INF),
		"ab_alpha": _parse_float_arg("--ab-alpha=", "0.05", 0.001, 0.5),
		"ab_power": _parse_float_arg("--ab-power=", "0.80", 0.5, 0.999),
	})

	if not result.get("ok", false):
		push_error("native_draft_validation_analyzer: %s" % result.get("error", "failed"))
		quit(1)
		return

	print("native_draft_validation_analyzer: wrote %s (%d rows)" % [
		result.get("output_path", ""), int(result.get("summary_row_count", 0))
	])
	if FileAccess.file_exists(ProjectSettings.globalize_path(String(result.get("ab_output_path", "")))):
		print("native_draft_validation_analyzer: wrote A/B report %s" % result.get("ab_output_path", ""))
	quit(0)
