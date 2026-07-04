class_name DraftStatsPromotion
extends RefCounted

## Copy a certified candidate stats snapshot into the production baseline directory.

const StatsManifestScript := preload("res://scripts/tools/stats_manifest.gd")
const DraftAiConfigScript := preload("res://scripts/tools/draft_ai_config.gd")


static func promote(
	candidate_dir: String,
	baseline_dir: String,
	config_path: String = DraftAiConfigScript.DEFAULT_CONFIG_PATH
) -> Dictionary:
	if candidate_dir.is_empty() or baseline_dir.is_empty():
		return {"ok": false, "error": "candidate_dir and baseline_dir required"}

	var manifest: Dictionary = StatsManifestScript.read_manifest(candidate_dir)
	if manifest.is_empty():
		return {"ok": false, "error": "candidate manifest missing or invalid"}

	var manifest_err: String = StatsManifestScript.validate_manifest(manifest, candidate_dir)
	if not manifest_err.is_empty():
		return {"ok": false, "error": "candidate manifest validation failed: %s" % manifest_err}

	var snapshot_id: String = String(manifest.get("snapshot_id", ""))
	if snapshot_id.is_empty():
		return {"ok": false, "error": "candidate snapshot_id missing"}

	if not _ensure_dir(baseline_dir):
		return {"ok": false, "error": "could not create baseline dir %s" % baseline_dir}

	for file_name in StatsManifestScript.get_canonical_csv_files():
		var src: String = candidate_dir.path_join(file_name)
		var dst: String = baseline_dir.path_join(file_name)
		if not FileAccess.file_exists(ProjectSettings.globalize_path(src)):
			return {"ok": false, "error": "candidate missing %s" % file_name}
		var copy_err: Error = DirAccess.copy_absolute(
			ProjectSettings.globalize_path(src),
			ProjectSettings.globalize_path(dst)
		)
		if copy_err != OK:
			return {"ok": false, "error": "copy failed for %s: %s" % [file_name, error_string(copy_err)]}

	var manifest_src: String = candidate_dir.path_join(StatsManifestScript.MANIFEST_FILENAME)
	var manifest_dst: String = baseline_dir.path_join(StatsManifestScript.MANIFEST_FILENAME)
	var manifest_copy_err: Error = DirAccess.copy_absolute(
		ProjectSettings.globalize_path(manifest_src),
		ProjectSettings.globalize_path(manifest_dst)
	)
	if manifest_copy_err != OK:
		return {"ok": false, "error": "copy failed for manifest: %s" % error_string(manifest_copy_err)}

	if not DraftAiConfigScript.write_certification_snapshot_id(config_path, snapshot_id):
		return {"ok": false, "error": "failed to write certification block to %s" % config_path}

	return {
		"ok": true,
		"snapshot_id": snapshot_id,
		"baseline_dir": baseline_dir,
		"config_path": config_path,
	}


static func _ensure_dir(path: String) -> bool:
	var global_path: String = ProjectSettings.globalize_path(path)
	if DirAccess.dir_exists_absolute(global_path):
		return true
	var err: Error = DirAccess.make_dir_recursive_absolute(global_path)
	return err == OK
