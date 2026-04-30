extends SceneTree
## Headless: --parity-trace=<fixture_name> — prints one JSON line per tick (native get_tick_snapshot).

const NativeClassName := "TeamfightSimulationCore"
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const HeadlessShutdownScript := preload("res://scripts/tools/headless_shutdown.gd")


func _init() -> void:
	call_deferred("_run")


func _fixture_input_by_name(path: String, want: String) -> Dictionary:
	var file := FileAccess.open(path, FileAccess.READ)
	if file == null:
		push_error("open fail %s" % path)
		return {}
	var parsed: Variant = JSON.parse_string(file.get_as_text())
	if parsed == null or not (parsed is Dictionary):
		return {}
	for entry in Array(parsed.get("fixtures", [])):
		if not (entry is Dictionary):
			continue
		var d: Dictionary = entry
		if String(d.get("name", "")) != want:
			continue
		return Dictionary(d.get("input", {}))
	return {}


func _run() -> void:
	var fixture_name := ""
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--parity-trace="):
			fixture_name = arg.substr("--parity-trace=".length())
	if fixture_name.is_empty():
		push_error("missing --parity-trace=name")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 2)
		return
	if not NativeSimulationBackendScript.ensure_gdextension_loaded():
		push_error("extension load failed")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return
	var core: Object = ClassDB.instantiate(NativeClassName)
	var input_data: Dictionary = _fixture_input_by_name("res://fixtures/goldens/match_fixtures.json", fixture_name)
	if input_data.is_empty():
		push_error("fixture not found: %s" % fixture_name)
		core = null
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return
	core.begin_match(input_data)
	while not bool(core.match_ticks_exhausted()):
		core.advance_one_tick()
		var snap: Dictionary = core.get_tick_snapshot()
		var units: Array = Array(snap.get("units", []))
		for item in units:
			if not (item is Dictionary):
				continue
			var u: Dictionary = item
			u["x"] = snappedf(float(u.get("x", 0.0)), 1e-6)
			u["y"] = snappedf(float(u.get("y", 0.0)), 1e-6)
			u["hp"] = snappedf(float(u.get("hp", 0.0)), 1e-6)
			u["stun"] = snappedf(float(u.get("stun", 0.0)), 1e-6)
			u["acd"] = snappedf(float(u.get("acd", 0.0)), 1e-6)
			u["abi"] = snappedf(float(u.get("abi", 0.0)), 1e-6)
			u["ult"] = snappedf(float(u.get("ult", 0.0)), 1e-6)
		units.sort_custom(func(a, b): return int(a["id"]) < int(b["id"]))
		snap["units"] = units
		snap["time"] = snappedf(float(snap.get("time", 0.0)), 1e-6)
		print(JSON.stringify(snap, "", false))
	core = null
	await HeadlessShutdownScript.teardown_extension_then_quit(self, 0)
