extends SceneTree

const MatchReplayInputScript := preload("res://scripts/simulation/match_replay_input.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")
const HeadlessShutdownScript := preload("res://scripts/tools/headless_shutdown.gd")

const TICK_LIMIT := 900


func _init() -> void:
	call_deferred("_run_check")


func _run_check() -> void:
	await process_frame
	if not NativeSimulationBackendScript.ensure_gdextension_loaded():
		push_error("check_large_projectile_damage: native simulation backend unavailable")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return
	var backend: Object = ClassDB.instantiate(NativeSimulationBackendScript.NativeClassName)
	if backend == null:
		push_error("check_large_projectile_damage: native simulation backend unavailable")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return
	backend.set_balance_patches([{
		"patch_id": "large_projectile_damage_regression",
		"targets": ["archer"],
		"roles": [],
		"stat_multipliers": {},
		"stat_additions": {},
		"ability": {
			"kind": "projectile",
			"params": {
				"reason": "large_projectile_damage_regression",
				"on_hit": {
					"kind": "damage",
					"params": {
						"damage_ratio": 1000.5,
						"damage_type": "physical",
						"trigger_on_hit": true,
						"reason": "large_projectile_damage_regression"
					}
				}
			}
		},
	}])

	var players: Array[StringName] = [&"guardian"]
	var enemies: Array[StringName] = [&"archer"]
	var match_input_obj = MatchReplayInputScript.build_match_input(
		6161,
		players,
		enemies,
		SimConstantsScript.SIMULATION_TICK_RATE
	)
	backend.begin_match(match_input_obj.to_dict())

	var saw_projectile := false
	var saw_kill := false
	for _i in range(TICK_LIMIT):
		backend.advance_one_tick()
		var snapshot: Dictionary = backend.get_tick_snapshot()
		if not snapshot.is_empty():
			if Array(snapshot.get("projectiles", [])).size() > 0:
				saw_projectile = true
			if int(snapshot.get("player_kills", 0)) + int(snapshot.get("enemy_kills", 0)) > 0:
				saw_kill = true
				break

	backend.clear()

	if not saw_projectile:
		push_error("check_large_projectile_damage: no projectile observed")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return
	if not saw_kill:
		push_error("check_large_projectile_damage: large projectile did not resolve to a kill")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	print("check_large_projectile_damage: OK")
	await HeadlessShutdownScript.teardown_extension_then_quit(self, 0)
