extends SceneTree

## Structural gate for native_draft_self_play_stats.gd output snapshots.
##
## Usage:
##   godot --headless --path . --script res://scripts/tools/native_draft_self_play_stats_gate.gd \
##     -- --output-dir=res://model_stats/stats_selfplay_smoke --min-matches=50

const StatsManifestScript := preload("res://scripts/tools/stats_manifest.gd")
const StatsDashboardLoaderScript := preload("res://scripts/tools/stats_dashboard_loader.gd")

const DEFAULT_MIN_MATCHES: int = 500

var _output_dir: String = ""
var _output_path: String = "res://logs/native_draft_self_play_stats_gate_report.md"
var _min_matches: int = DEFAULT_MIN_MATCHES


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

	_output_dir = _extract_argument("--output-dir=", "")
	_output_path = _extract_argument("--output=", "res://logs/native_draft_self_play_stats_gate_report.md")
	_min_matches = maxi(1, int(_extract_argument("--min-matches=", str(DEFAULT_MIN_MATCHES))))

	if _output_dir.is_empty():
		push_error("native_draft_self_play_stats_gate: --output-dir required")
		_write_report(false, ["--output-dir required"], [])
		quit(1)
		return

	var checks: Array[Dictionary] = _run_checks()
	var overall_pass: bool = true
	for check in checks:
		if not check["pass"]:
			overall_pass = false
			break

	_write_report(overall_pass, [], checks)
	print("native_draft_self_play_stats_gate: %s" % ("PASS" if overall_pass else "FAIL"))
	quit(0 if overall_pass else 1)


func _run_checks() -> Array[Dictionary]:
	var checks: Array[Dictionary] = []

	for file_name in StatsManifestScript.get_canonical_csv_files():
		var path: String = _output_dir.path_join(file_name)
		var exists: bool = FileAccess.file_exists(ProjectSettings.globalize_path(path))
		checks.append({
			"name": "file_%s" % file_name,
			"pass": exists,
			"detail": "present" if exists else "missing",
		})

	var manifest_path: String = _output_dir.path_join(StatsManifestScript.MANIFEST_FILENAME)
	var manifest_exists: bool = FileAccess.file_exists(ProjectSettings.globalize_path(manifest_path))
	checks.append({
		"name": "stats_manifest_json",
		"pass": manifest_exists,
		"detail": "present" if manifest_exists else "missing",
	})

	var match_count: int = 0
	if manifest_exists:
		var manifest: Dictionary = StatsManifestScript.read_manifest(_output_dir)
		var manifest_err: String = StatsManifestScript.validate_manifest(manifest, _output_dir)
		checks.append({
			"name": "manifest_validation",
			"pass": manifest_err.is_empty(),
			"detail": "ok" if manifest_err.is_empty() else manifest_err,
		})
		match_count = _count_matches_from_summary(_output_dir.path_join("summary_stats.csv"))
		checks.append({
			"name": "min_matches",
			"observed": match_count,
			"threshold": _min_matches,
			"pass": match_count >= _min_matches,
			"detail": "%d matches (min %d)" % [match_count, _min_matches],
		})
	else:
		checks.append({
			"name": "manifest_validation",
			"pass": false,
			"detail": "manifest missing",
		})
		checks.append({
			"name": "min_matches",
			"pass": false,
			"detail": "summary_stats.csv unreadable",
		})

	var combo_check: Dictionary = _check_role_combinations_content(
		_output_dir.path_join("role_combinations.csv")
	)
	checks.append({
		"name": "role_combinations_content",
		"pass": combo_check["pass"],
		"detail": combo_check["detail"],
	})

	var matchup_check: Dictionary = _check_matchup_content(_output_dir.path_join("matchup_vs.csv"))
	checks.append({
		"name": "matchup_vs_content",
		"pass": matchup_check["pass"],
		"detail": matchup_check["detail"],
	})

	var with_check: Dictionary = _check_matchup_content(_output_dir.path_join("matchup_with.csv"))
	checks.append({
		"name": "matchup_with_content",
		"pass": with_check["pass"],
		"detail": with_check["detail"],
	})

	var loader := StatsDashboardLoaderScript.new()
	var loader_err: Error = loader.load_from_dir(_output_dir)
	checks.append({
		"name": "dashboard_loader",
		"pass": loader_err == OK,
		"detail": "ok" if loader_err == OK else error_string(loader_err),
	})

	return checks


func _count_matches_from_summary(summary_path: String) -> int:
	if not FileAccess.file_exists(ProjectSettings.globalize_path(summary_path)):
		return 0
	var f := FileAccess.open(ProjectSettings.globalize_path(summary_path), FileAccess.READ)
	if f == null:
		return 0
	var header_line: String = f.get_line()
	var header: PackedStringArray = header_line.strip_edges().split(",")
	var total_idx: int = -1
	for i in range(header.size()):
		var col: String = header[i].strip_edges()
		if col == "total_rounds" or col == "total":
			total_idx = i
			break
	if total_idx < 0:
		f.close()
		return 0
	var total_matches: int = 0
	while not f.eof_reached():
		var line: String = f.get_line().strip_edges()
		if line.is_empty():
			continue
		var fields: PackedStringArray = line.split(",")
		if total_idx < fields.size() and fields[total_idx].is_valid_int():
			total_matches += int(fields[total_idx])
	f.close()
	return total_matches


func _check_role_combinations_content(combo_path: String) -> Dictionary:
	if not FileAccess.file_exists(ProjectSettings.globalize_path(combo_path)):
		return {"pass": false, "detail": "role_combinations.csv missing"}

	var f := FileAccess.open(ProjectSettings.globalize_path(combo_path), FileAccess.READ)
	if f == null:
		return {"pass": false, "detail": "role_combinations.csv unreadable"}

	var header_line: String = f.get_line()
	var header: PackedStringArray = header_line.strip_edges().split(",")
	var fingerprint_idx: int = -1
	for i in range(header.size()):
		if header[i].strip_edges() == "role_fingerprint":
			fingerprint_idx = i
			break
	if fingerprint_idx < 0:
		f.close()
		return {"pass": false, "detail": "role_fingerprint column missing"}

	var non_empty_fingerprints: int = 0
	while not f.eof_reached():
		var line: String = f.get_line().strip_edges()
		if line.is_empty():
			continue
		var fields: PackedStringArray = line.split(",")
		if fingerprint_idx < fields.size() and not fields[fingerprint_idx].strip_edges().is_empty():
			non_empty_fingerprints += 1
	f.close()

	if non_empty_fingerprints == 0:
		return {"pass": false, "detail": "no non-empty role_fingerprint rows"}
	return {"pass": true, "detail": "%d fingerprints" % non_empty_fingerprints}


func _check_matchup_content(matchup_path: String) -> Dictionary:
	if not FileAccess.file_exists(ProjectSettings.globalize_path(matchup_path)):
		return {"pass": false, "detail": "matchup_vs.csv missing"}

	var f := FileAccess.open(ProjectSettings.globalize_path(matchup_path), FileAccess.READ)
	if f == null:
		return {"pass": false, "detail": "matchup_vs.csv unreadable"}

	var header_line: String = f.get_line()
	var header: PackedStringArray = header_line.strip_edges().split(",")
	var wins_idx: int = -1
	var losses_idx: int = -1
	for i in range(header.size()):
		var col: String = header[i].strip_edges()
		if col == "wins":
			wins_idx = i
		elif col == "losses":
			losses_idx = i
	if wins_idx < 0 or losses_idx < 0:
		f.close()
		return {"pass": false, "detail": "wins/losses columns missing"}

	var populated_pairs: int = 0
	while not f.eof_reached():
		var line: String = f.get_line().strip_edges()
		if line.is_empty():
			continue
		var fields: PackedStringArray = line.split(",")
		if wins_idx >= fields.size() or losses_idx >= fields.size():
			continue
		var wins_str: String = fields[wins_idx].strip_edges()
		var losses_str: String = fields[losses_idx].strip_edges()
		if not wins_str.is_valid_int() or not losses_str.is_valid_int():
			continue
		if int(wins_str) + int(losses_str) > 0:
			populated_pairs += 1
	f.close()

	if populated_pairs == 0:
		return {"pass": false, "detail": "no matchup pairs with wins+losses > 0"}
	return {"pass": true, "detail": "%d populated pairs" % populated_pairs}


func _write_report(overall_pass: bool, errors: Array[String], checks: Array[Dictionary]) -> void:
	var lines: Array[String] = []
	lines.append("# Native Draft Self-Play Stats Gate Report")
	lines.append("")
	lines.append("Output dir: `%s`" % _output_dir)
	lines.append("Min matches: %d" % _min_matches)
	lines.append("")
	if not errors.is_empty():
		lines.append("## Errors")
		for err in errors:
			lines.append("- %s" % err)
		lines.append("")
	lines.append("## Checks")
	for check in checks:
		var status: String = "PASS" if check["pass"] else "FAIL"
		if check.has("threshold"):
			lines.append("- %s: %s (%s)" % [check["name"], status, check.get("detail", "")])
		else:
			lines.append("- %s: %s (%s)" % [check["name"], status, check.get("detail", "")])
	lines.append("")
	lines.append("STATUS: %s" % ("PASS" if overall_pass else "FAIL"))

	var f := FileAccess.open(ProjectSettings.globalize_path(_output_path), FileAccess.WRITE)
	if f == null:
		push_error("native_draft_self_play_stats_gate: could not write %s" % _output_path)
		quit(1)
		return
	f.store_string("\n".join(lines))
	f.close()
	print("native_draft_self_play_stats_gate: wrote %s" % _output_path)
