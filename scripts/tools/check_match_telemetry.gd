extends SceneTree

const MatchReplayInputScript := preload("res://scripts/simulation/match_replay_input.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")
const HeadlessShutdownScript := preload("res://scripts/tools/headless_shutdown.gd")

func _init() -> void:
	call_deferred("_run_check")

func _run_check() -> void:
	await process_frame
	var backend: Object = NativeSimulationBackendScript.new()
	if not backend.is_available():
		push_error("Native simulation backend unavailable.")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	var players: Array[StringName] = [&"artillery"]
	var enemies: Array[StringName] = [&"guardian"]
	var match_input_obj = MatchReplayInputScript.build_match_input(
		0,
		players,
		enemies,
		SimConstantsScript.SIMULATION_TICK_RATE
	)
	var match_input: Variant = match_input_obj.to_dict()
	var summary_raw: Variant = backend.run_match(match_input)

	if summary_raw is not Dictionary:
		push_error("check_match_telemetry: run_match did not return Dictionary")
		if backend.has_method(&"clear"):
			backend.call(&"clear")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	# NativeSimulationBackend.run_match already deep-copies the summary dictionary.
	var summary_dict: Dictionary = summary_raw as Dictionary
	var rows: Array = Array(summary_dict.get("unit_stats", []))
	if rows.is_empty():
		push_error("check_match_telemetry: empty unit_stats")
		if backend.has_method(&"clear"):
			backend.call(&"clear")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	var schema_expected: String = SimConstantsScript.SIM_TELEMETRY_SCHEMA_V1
	for item in rows:
		if item is not Dictionary:
			push_error("check_match_telemetry: unit_stats row not Dictionary")
			if backend.has_method(&"clear"):
				backend.call(&"clear")
			await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
			return
		var ud := Dictionary(item)
		var tel_raw: Variant = ud.get("telemetry", {})
		if tel_raw is not Dictionary:
			push_error("check_match_telemetry: missing telemetry on unit %s" % ud.get("archetype", "?"))
			if backend.has_method(&"clear"):
				backend.call(&"clear")
			await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
			return
		var tel := Dictionary(tel_raw)
		if String(tel.get("schema", "")) != schema_expected:
			push_error(
				"check_match_telemetry: bad schema got=%s expected=%s"
				% [tel.get("schema", ""), schema_expected]
			)
			if backend.has_method(&"clear"):
				backend.call(&"clear")
			await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
			return
		var hard_cc: float = float(tel.get("hard_cc_seconds", -1.0))
		if hard_cc < 0.0:
			push_error("check_match_telemetry: hard_cc_seconds invalid %s" % hard_cc)
			if backend.has_method(&"clear"):
				backend.call(&"clear")
			await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
			return

	if backend.has_method(&"clear"):
		backend.call(&"clear")
	print("check_match_telemetry: OK (%d units)" % rows.size())
	await HeadlessShutdownScript.teardown_extension_then_quit(self, 0)
