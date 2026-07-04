extends SceneTree

## Checks monotonic win-rate separation across difficulty tiers vs random.
##
## Usage:
##   godot --headless --path . --script res://scripts/tools/native_draft_tier_gate.gd \
##     -- --summary=res://model_stats/native_draft_tier_calibration_summary.csv
##
## Optional:
##     --draft-summary=res://model_stats/native_draft_tier_calibration_drafts.csv  (freshness check)
##     --min-gap=0.02
##     --self-test

const DraftAiConfigScript := preload("res://scripts/tools/draft_ai_config.gd")
const DraftValidationCsvScript := preload("res://scripts/tools/draft_validation_csv.gd")

const DEFAULT_MIN_GAP: float = 0.02

const TIER_STRATEGIES: Array[String] = [
	"native_softmax_easy",
	"native_softmax",
	"native_softmax_hard",
]

var _summary_path: String = ""
var _draft_summary_path: String = ""
var _output_path: String = "res://logs/native_draft_tier_gate_report.md"
var _min_gap: float = DEFAULT_MIN_GAP


func _extract_argument(prefix: String, default_value: String) -> String:
	for a in OS.get_cmdline_user_args():
		var arg: String = str(a)
		if arg.begins_with(prefix):
			return arg.substr(prefix.length())
	return default_value


func _init() -> void:
	call_deferred("_run")


func _run() -> void:
	if _flag_enabled("--self-test"):
		_run_self_test()
		return

	_summary_path = _extract_argument("--summary=", "")
	_draft_summary_path = _extract_argument("--draft-summary=", "")
	_output_path = _extract_argument("--output=", "res://logs/native_draft_tier_gate_report.md")
	_min_gap = _parse_float_arg("--min-gap=", str(DEFAULT_MIN_GAP))

	if _summary_path.is_empty():
		push_error("native_draft_tier_gate: --summary required")
		_write_report(false, ["--summary required"], {}, [])
		quit(1)
		return

	if not DraftAiConfigScript.run_tier_self_check():
		push_error("native_draft_tier_gate: DraftAiConfig tier self-check failed")
		_write_report(false, ["DraftAiConfig tier self-check failed"], {}, [])
		quit(1)
		return

	var freshness_errors: Array[String] = _check_summary_freshness()
	if not freshness_errors.is_empty():
		push_error("native_draft_tier_gate: stale or missing summary input")
		_write_report(false, freshness_errors, {}, [])
		quit(1)
		return

	var summary: Array[Dictionary] = _read_summary(_summary_path)
	if summary.is_empty():
		push_error("native_draft_tier_gate: summary empty or missing")
		_write_report(false, ["summary empty or missing"], {}, [])
		quit(1)
		return

	var metrics: Dictionary = _compute_metrics(summary)
	var checks: Array[Dictionary] = _run_checks(metrics)
	var overall_pass: bool = true
	for check in checks:
		if not check["pass"]:
			overall_pass = false
			break

	_write_report(overall_pass, [], metrics, checks)
	print("native_draft_tier_gate: %s" % ("PASS" if overall_pass else "FAIL"))
	quit(0 if overall_pass else 1)


func _run_self_test() -> void:
	if not DraftAiConfigScript.run_tier_self_check():
		push_error("native_draft_tier_gate self-test: DraftAiConfig tier self-check failed")
		quit(1)
		return
	print("native_draft_tier_gate self-test: OK")
	quit(0)


func _flag_enabled(flag: String) -> bool:
	for a in OS.get_cmdline_user_args():
		if str(a) == flag:
			return true
	return false


func _parse_float_arg(prefix: String, default_value: String) -> float:
	var raw: String = _extract_argument(prefix, default_value)
	if not raw.is_valid_float():
		push_warning("native_draft_tier_gate: invalid float for %s (%s), using default %s" % [prefix, raw, default_value])
		raw = default_value
	return float(raw)


func _check_summary_freshness() -> Array[String]:
	if _draft_summary_path.is_empty():
		return []
	if not FileAccess.file_exists(ProjectSettings.globalize_path(_summary_path)):
		return ["summary file missing"]
	if not FileAccess.file_exists(ProjectSettings.globalize_path(_draft_summary_path)):
		return ["draft summary file missing for freshness check"]
	var summary_mtime: int = FileAccess.get_modified_time(ProjectSettings.globalize_path(_summary_path))
	var drafts_mtime: int = FileAccess.get_modified_time(ProjectSettings.globalize_path(_draft_summary_path))
	if summary_mtime < drafts_mtime:
		return ["summary CSV older than draft summary (re-run analyzer)"]
	return []


func _read_summary(path: String) -> Array[Dictionary]:
	var out: Array[Dictionary] = []
	var f := FileAccess.open(ProjectSettings.globalize_path(path), FileAccess.READ)
	if f == null:
		return out
	var header: Array = DraftValidationCsvScript.split_csv_line(f.get_line())
	var idx: Dictionary = DraftValidationCsvScript.header_index(header)
	while not f.eof_reached():
		var line: String = f.get_line()
		if line.strip_edges().is_empty():
			continue
		var fields: Array = DraftValidationCsvScript.split_csv_line(line)
		var row: Dictionary = {}
		for key in idx.keys():
			var col_index: int = int(idx[key])
			if col_index < fields.size():
				row[key] = fields[col_index]
		if not row.is_empty():
			out.append(row)
	f.close()
	return out


func _compute_metrics(summary: Array[Dictionary]) -> Dictionary:
	var metrics: Dictionary = {}
	for row in summary:
		if row.get("metric", "") != "matchup":
			continue
		var grouping: String = String(row.get("grouping", ""))
		for strategy_name in TIER_STRATEGIES:
			var expected: String = "blue=%s red=random" % strategy_name
			if grouping == expected:
				metrics[strategy_name] = float(row.get("winrate", 0.0))
	return metrics


func _run_checks(metrics: Dictionary) -> Array[Dictionary]:
	var checks: Array[Dictionary] = []
	for strategy_name in TIER_STRATEGIES:
		var observed: float = metrics.get(strategy_name, -1.0)
		checks.append({
			"name": "%s_vs_random_blue" % strategy_name,
			"observed": observed,
			"pass": observed >= 0.0,
			"detail": "missing" if observed < 0.0 else "present",
		})

	for i in range(TIER_STRATEGIES.size() - 1):
		var lower_name: String = TIER_STRATEGIES[i]
		var higher_name: String = TIER_STRATEGIES[i + 1]
		var lower_val: float = metrics.get(lower_name, -1.0)
		var higher_val: float = metrics.get(higher_name, -1.0)
		var gap: float = higher_val - lower_val
		var check_pass: bool = lower_val >= 0.0 and higher_val >= 0.0 and gap >= _min_gap
		checks.append({
			"name": "%s_lt_%s" % [lower_name, higher_name],
			"observed": gap,
			"threshold": _min_gap,
			"pass": check_pass,
			"detail": "%.1f%% < %.1f%% (gap %.1f pp)" % [
				lower_val * 100.0, higher_val * 100.0, gap * 100.0
			] if lower_val >= 0.0 and higher_val >= 0.0 else "missing matchup row",
		})
	return checks


func _write_report(overall_pass: bool, errors: Array[String], metrics: Dictionary, checks: Array[Dictionary]) -> void:
	var lines: Array[String] = []
	lines.append("# Native Draft Tier Gate Report")
	lines.append("")
	lines.append("Summary: `%s`" % _summary_path)
	if not _draft_summary_path.is_empty():
		lines.append("Draft summary: `%s`" % _draft_summary_path)
	lines.append("Min gap: %.1f pp" % (_min_gap * 100.0))
	lines.append("")
	if not errors.is_empty():
		lines.append("## Errors")
		for err in errors:
			lines.append("- %s" % err)
		lines.append("")
	lines.append("## Metrics")
	for strategy_name in TIER_STRATEGIES:
		if metrics.has(strategy_name):
			lines.append("- `%s` vs random (blue): **%.1f%%**" % [strategy_name, float(metrics[strategy_name]) * 100.0])
		else:
			lines.append("- `%s` vs random (blue): missing" % strategy_name)
	lines.append("")
	lines.append("## Checks")
	for check in checks:
		var status: String = "PASS" if check["pass"] else "FAIL"
		if check.has("threshold"):
			lines.append("- %s: %s (observed gap %.1f pp, min %.1f pp)" % [
				check["name"], status, float(check["observed"]) * 100.0, float(check["threshold"]) * 100.0
			])
		else:
			lines.append("- %s: %s (%s)" % [check["name"], status, check.get("detail", "")])
	lines.append("")
	lines.append("STATUS: %s" % ("PASS" if overall_pass else "FAIL"))

	var f := FileAccess.open(ProjectSettings.globalize_path(_output_path), FileAccess.WRITE)
	if f == null:
		push_error("native_draft_tier_gate: could not write %s" % _output_path)
		quit(1)
		return
	f.store_string("\n".join(lines))
	f.close()
	print("native_draft_tier_gate: wrote %s" % _output_path)
