extends SceneTree

## Reads native_draft_validation_analyzer.gd output (summary CSV + A/B report CSV)
## and checks quantitative regression thresholds. Writes a PASS/FAIL report.
##
## Usage:
##   godot --headless --script res://scripts/tools/native_draft_quantitative_gate.gd \
##     -- --summary=res://model_stats/native_draft_validation_summary.csv \
##        --ab-report=res://model_stats/native_draft_validation_ab_report.csv \
##        --output=res://logs/native_draft_quantitative_gate_report.md

const DEFAULT_THRESHOLDS: Dictionary = {
	"native_full_vs_random_blue_min": 0.94,
	"native_full_vs_random_overall_min": 0.89,
	"native_full_self_play_bias_max_pp": 30.0,
	"native_softmax_vs_random_blue_min": 0.85,
	"native_softmax_vs_random_overall_min": 0.85,
	"native_softmax_self_play_bias_max_pp": 12.0,
}

var _summary_path: String = ""
var _ab_report_path: String = ""
var _output_path: String = "res://logs/native_draft_quantitative_gate_report.md"
var _thresholds: Dictionary = {}


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
	_thresholds = DEFAULT_THRESHOLDS.duplicate()
	_override_thresholds()

	if _summary_path.is_empty():
		push_error("native_draft_quantitative_gate: --summary required")
		_write_report(false, ["--summary required"], [], {})
		quit(1)
		return

	var summary: Array[Dictionary] = _read_csv(_summary_path)
	if summary.is_empty():
		push_error("native_draft_quantitative_gate: summary empty or missing")
		_write_report(false, ["summary empty or missing"], [], {})
		quit(1)
		return

	var ab_report: Array[Dictionary] = []
	if not _ab_report_path.is_empty() and FileAccess.file_exists(ProjectSettings.globalize_path(_ab_report_path)):
		ab_report = _read_csv(_ab_report_path)

	var metrics: Dictionary = _compute_metrics(summary, ab_report)
	var checks: Array[Dictionary] = _run_checks(metrics)
	var overall_pass: bool = true
	for check in checks:
		if not check["pass"]:
			overall_pass = false
			break

	_write_report(overall_pass, [], checks, metrics)
	print("native_draft_quantitative_gate: %s" % ("PASS" if overall_pass else "FAIL"))
	quit(0 if overall_pass else 1)


func _override_thresholds() -> void:
	for key in _thresholds.keys():
		var val: String = _extract_argument("--%s=" % key, "")
		if not val.is_empty():
			_thresholds[key] = float(val)


func _read_csv(path: String) -> Array[Dictionary]:
	var out: Array[Dictionary] = []
	var f := FileAccess.open(ProjectSettings.globalize_path(path), FileAccess.READ)
	if f == null:
		return out
	var header: Array = _split_csv_line(f.get_line())
	var idx: Dictionary = _header_index(header)
	while not f.eof_reached():
		var line: String = f.get_line()
		if line.strip_edges().is_empty():
			continue
		var fields: Array = _split_csv_line(line)
		var row: Dictionary = {}
		for key in idx.keys():
			row[key] = fields[idx[key]]
		out.append(row)
	f.close()
	return out


func _header_index(header: Array) -> Dictionary:
	var idx: Dictionary = {}
	for i in range(header.size()):
		idx[header[i]] = i
	return idx


func _split_csv_line(line: String) -> Array:
	var out: Array = []
	for field in line.split(","):
		out.append(field.strip_edges())
	return out


func _compute_metrics(summary: Array[Dictionary], ab_report: Array[Dictionary]) -> Dictionary:
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


func _decisive_winrate(row: Dictionary) -> float:
	var wins: int = int(row.get("wins", 0))
	var losses: int = int(row.get("losses", 0))
	var total: int = wins + losses
	if total <= 0:
		return 0.0
	return float(wins) / float(total)


func _run_checks(metrics: Dictionary) -> Array[Dictionary]:
	var checks: Array[Dictionary] = []
	var definitions: Array[Dictionary] = [
		{"name": "native_full_vs_random_blue", "kind": "min", "threshold": _thresholds["native_full_vs_random_blue_min"]},
		{"name": "native_full_vs_random_overall", "kind": "min", "threshold": _thresholds["native_full_vs_random_overall_min"]},
		{"name": "native_full_self_play_bias_pp", "kind": "max", "threshold": _thresholds["native_full_self_play_bias_max_pp"]},
		{"name": "native_softmax_vs_random_blue", "kind": "min", "threshold": _thresholds["native_softmax_vs_random_blue_min"]},
		{"name": "native_softmax_vs_random_overall", "kind": "min", "threshold": _thresholds["native_softmax_vs_random_overall_min"]},
		{"name": "native_softmax_self_play_bias_pp", "kind": "max", "threshold": _thresholds["native_softmax_self_play_bias_max_pp"]},
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


func _write_report(overall_pass: bool, errors: Array[String], checks: Array[Dictionary] = [], metrics: Dictionary = {}) -> void:
	var lines: Array[String] = []
	lines.append("# Native Draft Quantitative Gate Report")
	lines.append("")
	lines.append("Generated: " + Time.get_datetime_string_from_system())
	lines.append("")
	if not errors.is_empty():
		lines.append("## Errors")
		for err in errors:
			lines.append("- " + err)
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
