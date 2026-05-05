extends SceneTree

## Headless CSV export for stats dashboard. Run via run_godot.ps1 --generate-stats (forwards args after --).

const StatsSimulationCsvGeneratorScript := preload("res://scripts/tools/stats_simulation_csv_generator.gd")
const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")


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


func _run() -> void:
	# Pre-initialize champion catalog static cache to avoid threading issues
	ChampionCatalogScript.build_catalog()
	
	var out_dir := _extract_argument("--out-dir=", "res://stats_output")
	var sizes_raw := _extract_argument("--team-sizes=", "1,2,3,4,5")
	var matches := int(_extract_argument("--matches-per-size=", "100"))
	var seed := int(_extract_argument("--base-seed=", "0"))
	var export_workers := int(_extract_argument("--export-workers=", "0"))
	var profile_enabled := _flag_enabled("--profile-stats")
	var arr: Array[int] = []
	for part in sizes_raw.split(","):
		var t: String = part.strip_edges()
		if t.is_empty():
			continue
		arr.append(int(t))
	if arr.is_empty():
		push_error("generate_simulation_stats: no team sizes")
		quit(1)
		return
	var gen := StatsSimulationCsvGeneratorScript.new()
	var err: Error = gen.run(out_dir, arr, matches, seed, export_workers, profile_enabled)
	if err != OK:
		push_error("generate_simulation_stats failed: %s" % error_string(err))
		quit(1)
		return
	print("generate_simulation_stats: wrote CSVs to ", out_dir)
	quit(0)
