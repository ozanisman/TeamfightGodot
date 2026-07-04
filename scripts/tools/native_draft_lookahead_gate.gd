extends SceneTree

## Regression gate for native_lookahead_softmax (Workstream B.2).
##
## Usage:
##   godot --headless --path . --script res://scripts/tools/native_draft_lookahead_gate.gd \
##     -- --summary=res://model_stats/native_draft_lookahead_calibration_summary.csv \
##        --ab-report=res://model_stats/native_draft_lookahead_calibration_ab_report.csv \
##        --elo-ladder=res://model_stats/native_draft_lookahead_elo_ladder.csv \
##        --output=res://logs/native_draft_lookahead_gate_report.md

const DEFAULT_THRESHOLDS: Dictionary = {
	"native_lookahead_softmax_self_play_bias_max_pp": 15.0,
	"native_lookahead_softmax_vs_random_blue_min": 0.87,
	"native_lookahead_softmax_vs_random_overall_min": 0.87,
	"native_lookahead_softmax_elo_min_vs_native_softmax": -40.0,
}

const OPTIONAL_ELO_GAP_CHECK := "native_lookahead_softmax_elo_gap_vs_native_softmax"

var _summary_path: String = ""
var _ab_report_path: String = ""
var _elo_ladder_path: String = ""
var _output_path: String = "res://logs/native_draft_lookahead_gate_report.md"
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
	await process_frame
	_summary_path = _extract_argument("--summary=", "")
	_ab_report_path = _extract_argument("--ab-report=", "")
	_elo_ladder_path = _extract_argument("--elo-ladder=", "")
	_output_path = _extract_argument("--output=", "res://logs/native_draft_lookahead_gate_report.md")
	_thresholds = DEFAULT_THRESHOLDS.duplicate()
	_override_thresholds()

	if _summary_path.is_empty():
		push_error("native_draft_lookahead_gate: --summary required")
		_write_report(false, ["--summary required"], [], {})
		quit(1)
		return

	var summary: Array[Dictionary] = _read_csv(_summary_path)
	if summary.is_empty():
		push_error("native_draft_lookahead_gate: summary empty or missing")
		_write_report(false, ["summary empty or missing"], [], {})
		quit(1)
		return

	var ab_report: Array[Dictionary] = []
	if not _ab_report_path.is_empty() and FileAccess.file_exists(ProjectSettings.globalize_path(_ab_report_path)):
		ab_report = _read_csv(_ab_report_path)

	var metrics: Dictionary = _compute_metrics(summary, ab_report)
	var has_elo_ladder: bool = false
	if not _elo_ladder_path.is_empty() and FileAccess.file_exists(ProjectSettings.globalize_path(_elo_ladder_path)):
		_merge_elo_metrics(metrics, _read_csv(_elo_ladder_path))
		has_elo_ladder = true
	var checks: Array[Dictionary] = _run_checks(metrics, has_elo_ladder)
	var overall_pass: bool = true
	for check in checks:
		if not check["pass"]:
			overall_pass = false
			break

	_write_report(overall_pass, [], checks, metrics)
	print("native_draft_lookahead_gate: %s" % ("PASS" if overall_pass else "FAIL"))
	quit(0 if overall_pass else 1)


func _override_thresholds() -> void:
	for key in _thresholds.keys():
		var val: String = _extract_argument("--%s=" % key, "")
		if not val.is_empty() and val.is_valid_float():
			_thresholds[key] = float(val)


func _compute_metrics(summary: Array[Dictionary], ab_report: Array[Dictionary]) -> Dictionary:
	var metrics: Dictionary = {}
	for row in summary:
		var metric: String = String(row.get("metric", ""))
		var grouping: String = String(row.get("grouping", ""))
		if metric == "matchup":
			if grouping == "blue=native_lookahead_softmax red=native_lookahead_softmax":
				metrics["native_lookahead_softmax_self_play_decisive_blue"] = _decisive_winrate(row)
			elif grouping == "blue=native_lookahead_softmax red=random":
				metrics["native_lookahead_softmax_vs_random_blue"] = float(row.get("winrate", 0.0))
		elif metric == "native_winrate_by_opponent":
			if grouping == "native=native_lookahead_softmax opponent=random":
				metrics["native_lookahead_softmax_vs_random_overall"] = float(row.get("winrate", 0.0))

	for row in ab_report:
		if String(row.get("metric", "")) != "matchup_winrate_ci":
			continue
		var grouping: String = String(row.get("grouping", ""))
		if grouping == "blue=native_lookahead_softmax red=native_lookahead_softmax":
			metrics["native_lookahead_softmax_self_play_decisive_blue"] = float(row.get("decisive_winrate", 0.0))

	if metrics.has("native_lookahead_softmax_self_play_decisive_blue"):
		metrics["native_lookahead_softmax_self_play_bias_pp"] = absf(
			2.0 * float(metrics["native_lookahead_softmax_self_play_decisive_blue"]) - 1.0
		) * 100.0

	return metrics


func _merge_elo_metrics(metrics: Dictionary, elo_rows: Array[Dictionary]) -> void:
	for row in elo_rows:
		var strategy: String = String(row.get("strategy", ""))
		if strategy == "native_lookahead_softmax":
			metrics["native_lookahead_softmax_elo"] = float(row.get("elo", 0.0))
		elif strategy == "native_softmax":
			metrics["native_softmax_elo"] = float(row.get("elo", 0.0))
	if metrics.has("native_lookahead_softmax_elo") and metrics.has("native_softmax_elo"):
		metrics["native_lookahead_softmax_elo_gap_vs_native_softmax"] = (
			float(metrics["native_lookahead_softmax_elo"]) - float(metrics["native_softmax_elo"])
		)


func _decisive_winrate(row: Dictionary) -> float:
	var wins: int = int(row.get("wins", 0))
	var losses: int = int(row.get("losses", 0))
	var total: int = wins + losses
	if total <= 0:
		return 0.0
	return float(wins) / float(total)


func _run_checks(metrics: Dictionary, has_elo_ladder: bool) -> Array[Dictionary]:
	var checks: Array[Dictionary] = []
	var definitions: Array[Dictionary] = [
		{"name": "native_lookahead_softmax_self_play_bias_pp", "kind": "max", "threshold": _thresholds["native_lookahead_softmax_self_play_bias_max_pp"]},
		{"name": "native_lookahead_softmax_vs_random_blue", "kind": "min", "threshold": _thresholds["native_lookahead_softmax_vs_random_blue_min"]},
		{"name": "native_lookahead_softmax_vs_random_overall", "kind": "min", "threshold": _thresholds["native_lookahead_softmax_vs_random_overall_min"]},
	]
	if has_elo_ladder:
		definitions.append({
			"name": OPTIONAL_ELO_GAP_CHECK,
			"kind": "min",
			"threshold": _thresholds["native_lookahead_softmax_elo_min_vs_native_softmax"],
		})
	for def in definitions:
		var name: String = def["name"]
		var value: float = float(metrics.get(name, NAN))
		var pass_check: bool = false
		if is_nan(value):
			pass_check = false
		elif def["kind"] == "max":
			pass_check = value <= float(def["threshold"])
		else:
			pass_check = value >= float(def["threshold"])
		checks.append({
			"name": name,
			"value": value,
			"threshold": def["threshold"],
			"kind": def["kind"],
			"pass": pass_check,
		})
	return checks


func _write_report(overall_pass: bool, errors: Array, checks: Array[Dictionary], metrics: Dictionary) -> void:
	var lines: Array[String] = []
	lines.append("# Native Draft Lookahead Gate Report")
	lines.append("")
	lines.append("STATUS: %s" % ("PASS" if overall_pass else "FAIL"))
	lines.append("Generated: %s" % Time.get_datetime_string_from_system())
	lines.append("Summary: `%s`" % _summary_path)
	lines.append("")
	if not errors.is_empty():
		lines.append("## Errors")
		for err in errors:
			lines.append("- %s" % err)
		lines.append("")
	lines.append("## Checks")
	for check in checks:
		lines.append("- %s: value=%s threshold=%s (%s) => %s" % [
			check["name"],
			check["value"],
			check["threshold"],
			check["kind"],
			"PASS" if check["pass"] else "FAIL",
		])
	lines.append("")
	lines.append("## Metrics")
	for key in metrics.keys():
		lines.append("- %s = %.4f" % [key, metrics[key]])

	var global_path: String = ProjectSettings.globalize_path(_output_path)
	_ensure_parent_dir(global_path)
	var f := FileAccess.open(global_path, FileAccess.WRITE)
	if f:
		f.store_string("\n".join(lines) + "\n")
		f.close()


func _ensure_parent_dir(path: String) -> void:
	var dir_path: String = path.get_base_dir()
	if not DirAccess.dir_exists_absolute(dir_path):
		DirAccess.make_dir_recursive_absolute(dir_path)


func _read_csv(path: String) -> Array[Dictionary]:
	var out: Array[Dictionary] = []
	var f := FileAccess.open(ProjectSettings.globalize_path(path), FileAccess.READ)
	if f == null:
		return out
	var header: Array = _split_csv_line(f.get_line())
	var idx: Dictionary = {}
	for i in range(header.size()):
		idx[header[i]] = i
	while not f.eof_reached():
		var line: String = f.get_line().strip_edges()
		if line.is_empty():
			continue
		var fields: Array = _split_csv_line(line)
		var row: Dictionary = {}
		for key in idx.keys():
			var col: int = int(idx[key])
			if col < fields.size():
				row[key] = fields[col]
		out.append(row)
	f.close()
	return out


func _split_csv_line(line: String) -> Array:
	var out: Array = []
	var current: String = ""
	var in_quotes: bool = false
	var i: int = 0
	while i < line.length():
		var c: String = line[i]
		if c == "\"":
			if in_quotes and i + 1 < line.length() and line[i + 1] == "\"":
				current += "\""
				i += 1
			else:
				in_quotes = not in_quotes
		elif c == "," and not in_quotes:
			out.append(current.strip_edges())
			current = ""
		else:
			current += c
		i += 1
	out.append(current.strip_edges())
	return out
