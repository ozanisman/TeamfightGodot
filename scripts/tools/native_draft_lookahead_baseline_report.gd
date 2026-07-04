extends SceneTree

## Build markdown baseline report for quarantined lookahead from analyzer summary CSV.
##
## Usage:
##   godot --headless --path . --script res://scripts/tools/native_draft_lookahead_baseline_report.gd \
##     -- --summary=res://model_stats/native_draft_lookahead_baseline_summary.csv \
##        --ab-report=res://model_stats/native_draft_lookahead_baseline_ab_report.csv \
##        --output=res://logs/native_draft_lookahead_baseline_report.md

const STRATEGY_PREFIXES: Array[String] = [
	"native_lookahead",
	"native_softmax",
	"native_full",
]

var _summary_path: String = ""
var _ab_report_path: String = ""
var _output_path: String = "res://logs/native_draft_lookahead_baseline_report.md"


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
	_output_path = _extract_argument("--output=", "res://logs/native_draft_lookahead_baseline_report.md")

	if _summary_path.is_empty():
		push_error("native_draft_lookahead_baseline_report: --summary required")
		quit(1)
		return

	var summary: Array[Dictionary] = _read_csv(_summary_path)
	if summary.is_empty():
		push_error("native_draft_lookahead_baseline_report: summary empty")
		quit(1)
		return

	var ab_report: Array[Dictionary] = []
	if not _ab_report_path.is_empty() and FileAccess.file_exists(ProjectSettings.globalize_path(_ab_report_path)):
		ab_report = _read_csv(_ab_report_path)

	var metrics: Dictionary = _compute_metrics(summary, ab_report)
	_write_report(metrics)
	print("native_draft_lookahead_baseline_report: wrote %s" % _output_path)
	quit(0)


func _compute_metrics(summary: Array[Dictionary], ab_report: Array[Dictionary]) -> Dictionary:
	var metrics: Dictionary = {}
	for row in summary:
		var metric: String = String(row.get("metric", ""))
		var grouping: String = String(row.get("grouping", ""))
		if metric == "matchup":
			for prefix in STRATEGY_PREFIXES:
				if grouping == "blue=%s red=%s" % [prefix, prefix]:
					metrics["%s_self_play_blue" % prefix] = float(row.get("winrate", 0.0))
					metrics["%s_self_play_decisive_blue" % prefix] = _decisive_winrate(row)
				elif grouping == "blue=%s red=random" % prefix:
					metrics["%s_vs_random_blue" % prefix] = float(row.get("winrate", 0.0))
		elif metric == "native_winrate_by_opponent":
			for prefix in STRATEGY_PREFIXES:
				if grouping == "native=%s opponent=random" % prefix:
					metrics["%s_vs_random_overall" % prefix] = float(row.get("winrate", 0.0))

	for row in ab_report:
		var metric: String = String(row.get("metric", ""))
		var grouping: String = String(row.get("grouping", ""))
		if metric != "matchup_winrate_ci":
			continue
		for prefix in STRATEGY_PREFIXES:
			if grouping == "blue=%s red=%s" % [prefix, prefix]:
				metrics["%s_self_play_decisive_blue" % prefix] = float(row.get("decisive_winrate", 0.0))

	for prefix in STRATEGY_PREFIXES:
		var key: String = "%s_self_play_decisive_blue" % prefix
		if metrics.has(key):
			metrics["%s_self_play_bias_pp" % prefix] = absf(2.0 * float(metrics[key]) - 1.0) * 100.0

	return metrics


func _decisive_winrate(row: Dictionary) -> float:
	var wins: int = int(row.get("wins", 0))
	var losses: int = int(row.get("losses", 0))
	var total: int = wins + losses
	if total <= 0:
		return 0.0
	return float(wins) / float(total)


func _write_report(metrics: Dictionary) -> void:
	var lines: Array[String] = []
	lines.append("# Native Draft Lookahead Baseline Report")
	lines.append("")
	lines.append("Generated: %s" % Time.get_datetime_string_from_system())
	lines.append("Summary: `%s`" % _summary_path)
	lines.append("")
	lines.append("## Self-play side bias (decisive blue deviation pp)")
	for prefix in STRATEGY_PREFIXES:
		var key: String = "%s_self_play_bias_pp" % prefix
		if metrics.has(key):
			lines.append("- %s: %.1f pp" % [prefix, metrics[key]])
	lines.append("")
	lines.append("## Vs random")
	for prefix in STRATEGY_PREFIXES:
		var blue_key: String = "%s_vs_random_blue" % prefix
		var overall_key: String = "%s_vs_random_overall" % prefix
		if metrics.has(blue_key) or metrics.has(overall_key):
			lines.append(
				"- %s: blue=%.1f%% overall=%.1f%%"
				% [
					prefix,
					float(metrics.get(blue_key, 0.0)) * 100.0,
					float(metrics.get(overall_key, 0.0)) * 100.0,
				]
			)
	lines.append("")
	lines.append("## Raw metrics")
	for key in metrics.keys():
		lines.append("- %s = %.4f" % [key, metrics[key]])

	var global_path: String = ProjectSettings.globalize_path(_output_path)
	_ensure_parent_dir(global_path)
	var f := FileAccess.open(global_path, FileAccess.WRITE)
	if f == null:
		push_error("native_draft_lookahead_baseline_report: could not write %s" % _output_path)
		return
	f.store_string("\n".join(lines) + "\n")
	f.close()


func _ensure_parent_dir(path: String) -> void:
	var dir_path: String = path.get_base_dir()
	if not DirAccess.dir_exists_absolute(dir_path):
		DirAccess.make_dir_recursive_absolute(dir_path)


func _read_csv(path: String) -> Array[Dictionary]:
	var out: Array[Dictionary] = []
	if not FileAccess.file_exists(ProjectSettings.globalize_path(path)):
		return out
	var f := FileAccess.open(ProjectSettings.globalize_path(path), FileAccess.READ)
	if f == null:
		return out
	var header_line: String = f.get_line()
	if header_line.is_empty():
		f.close()
		return out
	var header: Array = _split_csv_line(header_line.strip_edges())
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
