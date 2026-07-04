class_name DraftStatsSnapshotChecks
extends RefCounted

## Structural checks for self-play stats snapshot directories.

const StatsManifestScript := preload("res://scripts/tools/stats_manifest.gd")
const StatsDashboardLoaderScript := preload("res://scripts/tools/stats_dashboard_loader.gd")

const DEFAULT_MIN_MATCHES: int = 500


static func run_checks(output_dir: String, min_matches: int = DEFAULT_MIN_MATCHES) -> Array[Dictionary]:
	var checks: Array[Dictionary] = []

	for file_name in StatsManifestScript.get_canonical_csv_files():
		var path: String = output_dir.path_join(file_name)
		var exists: bool = FileAccess.file_exists(ProjectSettings.globalize_path(path))
		checks.append({
			"name": "file_%s" % file_name,
			"pass": exists,
			"detail": "present" if exists else "missing",
		})

	var manifest_path: String = output_dir.path_join(StatsManifestScript.MANIFEST_FILENAME)
	var manifest_exists: bool = FileAccess.file_exists(ProjectSettings.globalize_path(manifest_path))
	checks.append({
		"name": "stats_manifest_json",
		"pass": manifest_exists,
		"detail": "present" if manifest_exists else "missing",
	})

	if manifest_exists:
		var manifest: Dictionary = StatsManifestScript.read_manifest(output_dir)
		var manifest_err: String = StatsManifestScript.validate_manifest(manifest, output_dir)
		checks.append({
			"name": "manifest_validation",
			"pass": manifest_err.is_empty(),
			"detail": "ok" if manifest_err.is_empty() else manifest_err,
		})
		var match_count: int = _count_matches_from_summary(output_dir.path_join("summary_stats.csv"))
		checks.append({
			"name": "min_matches",
			"observed": match_count,
			"threshold": min_matches,
			"pass": match_count >= min_matches,
			"detail": "%d matches (min %d)" % [match_count, min_matches],
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
		output_dir.path_join("role_combinations.csv")
	)
	checks.append({
		"name": "role_combinations_content",
		"pass": combo_check["pass"],
		"detail": combo_check["detail"],
	})

	var matchup_check: Dictionary = _check_matchup_content(output_dir.path_join("matchup_vs.csv"))
	checks.append({
		"name": "matchup_vs_content",
		"pass": matchup_check["pass"],
		"detail": matchup_check["detail"],
	})

	var with_check: Dictionary = _check_matchup_content(output_dir.path_join("matchup_with.csv"))
	checks.append({
		"name": "matchup_with_content",
		"pass": with_check["pass"],
		"detail": with_check["detail"],
	})

	var loader := StatsDashboardLoaderScript.new()
	var loader_err: Error = loader.load_from_dir(output_dir)
	checks.append({
		"name": "dashboard_loader",
		"pass": loader_err == OK,
		"detail": "ok" if loader_err == OK else error_string(loader_err),
	})

	return checks


static func overall_pass(checks: Array[Dictionary]) -> bool:
	for check in checks:
		if not check["pass"]:
			return false
	return true


static func _count_matches_from_summary(summary_path: String) -> int:
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


static func _check_role_combinations_content(combo_path: String) -> Dictionary:
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


static func _check_matchup_content(matchup_path: String) -> Dictionary:
	if not FileAccess.file_exists(ProjectSettings.globalize_path(matchup_path)):
		return {"pass": false, "detail": "matchup file missing"}

	var f := FileAccess.open(ProjectSettings.globalize_path(matchup_path), FileAccess.READ)
	if f == null:
		return {"pass": false, "detail": "matchup file unreadable"}

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
