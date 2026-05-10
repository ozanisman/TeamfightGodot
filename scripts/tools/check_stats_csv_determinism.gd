extends SceneTree

## Runs [StatsSimulationCsvGenerator] twice with identical args and compares canonical CSV payloads.[br][br][code]run_godot.ps1 --check-stats-csv-determinism --[/code] forwards optional [--matches-per-size=] [--team-sizes=] [--export-workers=] [--base-seed=].

const StatsSimulationCsvGeneratorScript := preload("res://scripts/tools/stats_simulation_csv_generator.gd")
const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const HeadlessShutdownScript := preload("res://scripts/tools/headless_shutdown.gd")

const _DIR_A := "user://stats_csv_determinism_a"
const _DIR_B := "user://stats_csv_determinism_b"
const _DIR_FALLBACK := "user://stats_csv_determinism_fallback"

const _COMPARE_FILES: Array[String] = [
	"summary_stats.csv",
	"combat_stats.csv",
	"role_stats.csv",
	"hero_combinations.csv",
	"matchup_vs.csv",
	"matchup_with.csv",
]


func _extract_argument(prefix: String, default_value: String) -> String:
	for a in OS.get_cmdline_user_args():
		var arg: String = str(a)
		if arg.begins_with(prefix):
			return arg.substr(prefix.length())
	return default_value


func _flag_enabled(prefix: String) -> bool:
	for a in OS.get_cmdline_user_args():
		var arg: String = str(a)
		if arg == prefix:
			return true
		if arg.begins_with(prefix + "="):
			var tail: String = arg.substr(prefix.length() + 1)
			return tail != "0" and tail.to_lower() != "false"
	return false


func _init() -> void:
	call_deferred("_run")


func _run_once(
	directory: String,
	matches_per_size: int,
	sizes: Array[int],
	base_seed: int,
	export_workers: int,
	with_log: bool,
	use_native_generated_stats: bool = true
) -> Error:
	var gen := StatsSimulationCsvGeneratorScript.new()
	var err := gen.run(
		directory,
		sizes,
		matches_per_size,
		base_seed,
		export_workers,
		false,
		with_log,
		true,
		{},
		use_native_generated_stats
	)
	return err


func _same_payload(left_base: String, right_base: String, name: String) -> bool:
	var pa := "%s/%s" % [left_base.rstrip("/"), name]
	var pb := "%s/%s" % [right_base.rstrip("/"), name]
	if not FileAccess.file_exists(pa):
		push_error("check_stats_csv_determinism: missing %s" % pa)
		return false
	if not FileAccess.file_exists(pb):
		push_error("check_stats_csv_determinism: missing %s" % pb)
		return false
	var fa := FileAccess.open(pa, FileAccess.READ)
	var fb := FileAccess.open(pb, FileAccess.READ)
	if fa == null or fb == null:
		return false
	var ta := fa.get_as_text()
	var tb := fb.get_as_text()
	fa.close()
	fb.close()
	return ta == tb


func _run() -> void:
	await process_frame

	if not NativeSimulationBackendScript.ensure_gdextension_loaded():
		push_error("check_stats_csv_determinism: native extension unavailable")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	var matches := int(_extract_argument("--matches-per-size=", "8"))
	var sizes_raw := _extract_argument("--team-sizes=", "3,5")
	var base_seed := int(_extract_argument("--base-seed=", "12345"))
	var export_workers := int(_extract_argument("--export-workers=", "2"))

	var sz: Array[int] = []
	for part in sizes_raw.split(","):
		var t := part.strip_edges()
		if t.is_empty():
			continue
		sz.append(int(t))
	if sz.is_empty():
		push_error("check_stats_csv_determinism: no team sizes parsed")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	var write_log := _flag_enabled("--write-match-log")

	OS.set_environment("TEAMFIGHT_STATS_EXPORT_MINIMAL", "1")
	ChampionCatalogScript.build_catalog()
	ChampionCatalogScript.build_role_configs()
	ChampionCatalogScript.build_passive_registry()
	ChampionCatalogScript.freeze_built_specs_for_worker_reuse()

	var ea := _run_once(_DIR_A, matches, sz, base_seed, export_workers, write_log)
	if ea != OK:
		push_error("check_stats_csv_determinism: first run failed %s" % error_string(ea))
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	var eb := _run_once(_DIR_B, matches, sz, base_seed, export_workers, write_log)
	if eb != OK:
		push_error("check_stats_csv_determinism: second run failed %s" % error_string(eb))
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	var fallback_enabled := not _flag_enabled("--skip-fallback-compare")
	if fallback_enabled:
		var ef := _run_once(_DIR_FALLBACK, matches, sz, base_seed, export_workers, write_log, false)
		if ef != OK:
			push_error("check_stats_csv_determinism: fallback run failed %s" % error_string(ef))
			await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
			return

	var files_to_check: Array[String] = []
	files_to_check.assign(_COMPARE_FILES)
	if write_log:
		files_to_check.append("match_log.csv")

	for fname in files_to_check:
		if not _same_payload(_DIR_A, _DIR_B, fname):
			push_error(
				"check_stats_csv_determinism: mismatch in %s (compare %s vs %s)"
				% [fname, _DIR_A, _DIR_B]
			)
			await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
			return
		if fallback_enabled and not _same_payload(_DIR_A, _DIR_FALLBACK, fname):
			push_error(
				"check_stats_csv_determinism: fallback mismatch in %s (compare %s vs %s)"
				% [fname, _DIR_A, _DIR_FALLBACK]
			)
			await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
			return

	print("check_stats_csv_determinism: OK (team_sizes=%s, matches_per_size=%s, fallback=%s)" % [sizes_raw, str(matches), str(fallback_enabled)])
	await HeadlessShutdownScript.teardown_extension_then_quit(self, 0)
