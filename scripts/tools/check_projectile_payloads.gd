extends SceneTree

const MatchReplayInputScript := preload("res://scripts/simulation/match_replay_input.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")
const HeadlessShutdownScript := preload("res://scripts/tools/headless_shutdown.gd")


func _init() -> void:
	call_deferred("_run_check")


func _run_check() -> void:
	await process_frame
	if not NativeSimulationBackendScript.ensure_gdextension_loaded():
		push_error("check_projectile_payloads: native simulation backend unavailable")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return
	var backend: Object = ClassDB.instantiate(NativeSimulationBackendScript.NativeClassName)
	if backend == null:
		push_error("check_projectile_payloads: native simulation backend unavailable")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	if not _check_projectile_multi_effect(backend):
		backend.clear()
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return
	backend.clear()
	backend = ClassDB.instantiate(NativeSimulationBackendScript.NativeClassName)
	if backend == null:
		push_error("check_projectile_payloads: native simulation backend unavailable")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	if not _check_ranged_auto_damage(backend):
		backend.clear()
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return
	backend.clear()

	print("check_projectile_payloads: OK")
	await HeadlessShutdownScript.teardown_extension_then_quit(self, 0)


func _run_match(backend: Object, seed: int, players: Array[StringName], enemies: Array[StringName]) -> Dictionary:
	var match_input: Object = MatchReplayInputScript.build_match_input(
		seed,
		players,
		enemies,
		SimConstantsScript.SIMULATION_TICK_RATE
	)
	var result: Variant = backend.run_match(match_input.to_dict())
	if result is Dictionary:
		return result
	if result is Object and result.has_method("to_dict"):
		return result.to_dict()
	return {}


func _stats_for(summary: Dictionary, archetype: String) -> Dictionary:
	for entry in Array(summary.get("unit_stats", [])):
		if entry is Dictionary and str((entry as Dictionary).get("archetype", "")) == archetype:
			return entry as Dictionary
	return {}


func _check_projectile_multi_effect(backend: Object) -> bool:
	var summary: Dictionary = _run_match(backend, 7001, [&"artillery"], [&"guardian"])
	var artillery: Dictionary = _stats_for(summary, "artillery")
	if artillery.is_empty():
		push_error("check_projectile_payloads: missing artillery stats")
		return false
	if float(artillery.get("damage_dealt_ability", 0.0)) <= 0.0:
		push_error("check_projectile_payloads: projectile damage payload did not deal ability damage")
		return false
	if int(artillery.get("stuns", 0)) <= 0:
		push_error("check_projectile_payloads: projectile multi_effect stun payload did not apply")
		return false
	return true


func _check_ranged_auto_damage(backend: Object) -> bool:
	var summary: Dictionary = _run_match(backend, 7002, [&"artillery"], [&"guardian"])
	var artillery: Dictionary = _stats_for(summary, "artillery")
	if artillery.is_empty():
		push_error("check_projectile_payloads: missing artillery stats")
		return false
	if float(artillery.get("damage_dealt_auto", 0.0)) <= 0.0:
		push_error("check_projectile_payloads: ranged auto projectile did not deal auto damage")
		return false
	return true
