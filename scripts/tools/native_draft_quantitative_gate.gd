extends SceneTree

## Reads native_draft_validation_analyzer.gd output (summary CSV + A/B report CSV)
## and checks quantitative regression thresholds. Writes a PASS/FAIL report.
##
## Usage:
##   godot --headless --script res://scripts/tools/native_draft_quantitative_gate.gd \
##     -- --summary=res://model_stats/native_draft_validation_summary.csv \
##        --ab-report=res://model_stats/native_draft_validation_ab_report.csv \
##        --output=res://logs/native_draft_quantitative_gate_report.md

const DraftQuantitativeGateCoreScript := preload("res://scripts/tools/draft_quantitative_gate_core.gd")

var _summary_path: String = ""
var _ab_report_path: String = ""
var _output_path: String = "res://logs/native_draft_quantitative_gate_report.md"
var _threshold_overrides: Dictionary = {}


func _extract_argument(prefix: String, default_value: String) -> String:
	for a in OS.get_cmdline_user_args():
		var arg: String = str(a)
		if arg.begins_with(prefix):
			return arg.substr(prefix.length())
	return default_value


func _init() -> void:
	call_deferred("_run")


func _run() -> void:
	_summary_path = _extract_argument("--summary=", "")
	_ab_report_path = _extract_argument("--ab-report=", "")
	_output_path = _extract_argument("--output=", "res://logs/native_draft_quantitative_gate_report.md")
	_threshold_overrides = _parse_threshold_overrides()

	if _summary_path.is_empty():
		push_error("native_draft_quantitative_gate: --summary required")
		_write_report(false, ["--summary required"], [], {})
		quit(1)
		return

	var result: Dictionary = DraftQuantitativeGateCoreScript.evaluate_from_paths(
		_summary_path, _ab_report_path, _threshold_overrides
	)
	if not result.get("errors", []).is_empty():
		push_error("native_draft_quantitative_gate: %s" % str(result["errors"][0]))

	var overall_pass: bool = bool(result.get("pass", false))
	_write_report(overall_pass, result.get("errors", []), result.get("checks", []), result.get("metrics", {}))
	print("native_draft_quantitative_gate: %s" % ("PASS" if overall_pass else "FAIL"))
	quit(0 if overall_pass else 1)


func _parse_threshold_overrides() -> Dictionary:
	var overrides: Dictionary = {}
	for key in DraftQuantitativeGateCoreScript.DEFAULT_THRESHOLDS.keys():
		var val: String = _extract_argument("--%s=" % key, "")
		if not val.is_empty():
			if not val.is_valid_float():
				push_warning("native_draft_quantitative_gate: invalid float for %s (%s), ignoring override" % [key, val])
				continue
			overrides[key] = float(val)
	return overrides


func _write_report(overall_pass: bool, errors: Array, checks: Array[Dictionary] = [], metrics: Dictionary = {}) -> void:
	var lines: Array[String] = []
	lines.append("# Native Draft Quantitative Gate Report")
	lines.append("")
	lines.append("Generated: " + Time.get_datetime_string_from_system())
	lines.append("")
	if not errors.is_empty():
		lines.append("## Errors")
		for err in errors:
			lines.append("- " + str(err))
		lines.append("")
	lines.append("## Metrics")
	for key in metrics.keys():
		lines.append("- %s = %.4f" % [key, metrics[key]])
	if metrics.is_empty():
		lines.append("- (none computed)")
	lines.append("")
	lines.append("## Threshold Checks")
	for check in checks:
		var status: String = "PASS" if check["pass"] else "FAIL"
		var op: String = ">=" if check["kind"] == "min" else "<="
		lines.append("- %s: %s (%.4f %s %.4f) [%s]" % [check["name"], status, check["value"], op, check["threshold"], check["reason"]])
	if checks.is_empty():
		lines.append("- (no checks)")
	lines.append("")
	lines.append("## Overall")
	lines.append("STATUS: %s" % ("PASS" if overall_pass else "FAIL"))

	var f := FileAccess.open(ProjectSettings.globalize_path(_output_path), FileAccess.WRITE)
	if f == null:
		push_error("native_draft_quantitative_gate: could not write report %s" % _output_path)
		return
	f.store_string("\n".join(lines) + "\n")
	f.close()
	print("native_draft_quantitative_gate: wrote %s" % _output_path)
