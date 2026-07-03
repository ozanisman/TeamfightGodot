class_name StatsManifest
extends RefCounted

## Lightweight provenance manifest for stats snapshots.
## Written alongside the CSV files during generation and validated when loaded.

const MANIFEST_FILENAME := "stats_manifest.json"
const SCHEMA_VERSION := 1

const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")

const _CANONICAL_CSV_FILES: Array[String] = [
	"summary_stats.csv",
	"combat_stats.csv",
	"role_stats.csv",
	"role_combinations.csv",
	"matchup_vs.csv",
	"matchup_with.csv",
]


static func _manifest_path(stats_dir: String) -> String:
	return stats_dir.path_join(MANIFEST_FILENAME)


static func _now_iso8601() -> String:
	return Time.get_datetime_string_from_system(true)


static func _compute_file_hash(path: String) -> String:
	var f := FileAccess.open(path, FileAccess.READ)
	if f == null:
		return ""
	var text := f.get_as_text()
	f.close()
	return str(text.hash())


static func _get_written_files(stats_dir: String) -> PackedStringArray:
	var files: PackedStringArray = []
	var dir := DirAccess.open(stats_dir)
	if dir == null:
		return files
	dir.list_dir_begin()
	var file_name := dir.get_next()
	while not file_name.is_empty():
		if not dir.current_is_dir() and file_name != MANIFEST_FILENAME and file_name in _CANONICAL_CSV_FILES:
			files.append(file_name)
		file_name = dir.get_next()
	dir.list_dir_end()
	files.sort()
	return files


static func build_manifest(
	stats_dir: String,
	generator_name: String,
	generator_args: Array = [],
	extra_metadata: Dictionary = {}
) -> Dictionary:
	var files: Dictionary = {}
	for file_name in _get_written_files(stats_dir):
		var file_path := stats_dir.path_join(file_name)
		files[file_name] = _compute_file_hash(file_path)

	var manifest := {
		"schema_version": SCHEMA_VERSION,
		"snapshot_id": _generate_snapshot_id(stats_dir),
		"generated_at": _now_iso8601(),
		"catalog_version": ChampionCatalogScript.get_catalog_version(),
		"generator_name": generator_name,
		"generator_args": generator_args.duplicate(),
		"files": files,
	}
	for key in extra_metadata:
		manifest[key] = extra_metadata[key]
	return manifest


static func _generate_snapshot_id(stats_dir: String) -> String:
	# Deterministic seed: catalog version + sorted file hashes. The exact timestamp is intentionally
	# excluded so regenerating identical stats from the same catalog produces the same id.
	var files: Dictionary = {}
	for file_name in _get_written_files(stats_dir):
		files[file_name] = _compute_file_hash(stats_dir.path_join(file_name))
	var keys: Array = files.keys()
	keys.sort()
	var parts: PackedStringArray = []
	parts.append(ChampionCatalogScript.get_catalog_version())
	for key in keys:
		parts.append("%s=%s" % [key, files[key]])
	var seed := "|".join(parts)
	return str(seed.hash())


static func write_manifest(stats_dir: String, manifest: Dictionary) -> bool:
	var path := _manifest_path(stats_dir)
	var f := FileAccess.open(path, FileAccess.WRITE)
	if f == null:
		push_error("StatsManifest: could not write %s" % path)
		return false
	f.store_string(JSON.stringify(manifest, "\t"))
	f.close()
	return true


static func read_manifest(stats_dir: String) -> Dictionary:
	var path := _manifest_path(stats_dir)
	if not FileAccess.file_exists(path):
		return {}
	var f := FileAccess.open(path, FileAccess.READ)
	if f == null:
		push_warning("StatsManifest: could not open %s" % path)
		return {}
	var text := f.get_as_text()
	f.close()
	var parsed: Variant = JSON.parse_string(text)
	if parsed == null or not parsed is Dictionary:
		push_warning("StatsManifest: %s is not valid JSON" % path)
		return {}
	return Dictionary(parsed)


static func validate_manifest(manifest: Dictionary, stats_dir: String) -> String:
	if manifest.is_empty():
		return "missing manifest"
	var schema_version: Variant = manifest.get("schema_version", 0)
	if schema_version != SCHEMA_VERSION:
		return "unsupported schema_version %s (expected %s)" % [schema_version, SCHEMA_VERSION]
	if not manifest.has("snapshot_id"):
		return "missing snapshot_id"
	if not manifest.has("files"):
		return "missing files"
	var files: Dictionary = manifest.get("files", {})
	if files.is_empty():
		return "manifest has no files"
	for file_name in files:
		var file_path := stats_dir.path_join(file_name)
		if not FileAccess.file_exists(file_path):
			return "manifest references missing file: %s" % file_name
		var expected_hash: String = str(files[file_name])
		var actual_hash := _compute_file_hash(file_path)
		if expected_hash != actual_hash:
			return "hash mismatch for %s (expected %s, got %s)" % [file_name, expected_hash, actual_hash]
	return ""
