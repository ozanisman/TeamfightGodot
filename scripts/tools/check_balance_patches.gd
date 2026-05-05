extends SceneTree
## Headless checks for native balance patches: stat overlays, inline ability overrides, kit_id presets,
## get_balance_patches round-trip, and empty patch list (vanilla effective stats for overlays).

const NativeClassName := "TeamfightSimulationCore"
const MatchReplayInputScript := preload("res://scripts/simulation/match_replay_input.gd")
const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const HeadlessShutdownScript := preload("res://scripts/tools/headless_shutdown.gd")

var _failures: PackedStringArray = PackedStringArray()


func _init() -> void:
	call_deferred("_run_suite")


func _fail(msg: String) -> void:
	_failures.append(msg)
	push_error("[balance_patch_suite] %s" % msg)


func _assert(cond: bool, msg: String) -> void:
	if not cond:
		_fail(msg)


func _load_native() -> Object:
	if not NativeSimulationBackendScript.ensure_gdextension_loaded():
		_fail("GDExtension load failed")
		return null
	if not ClassDB.class_exists(NativeClassName):
		_fail("%s not registered" % NativeClassName)
		return null
	var core: Object = ClassDB.instantiate(NativeClassName)
	if core == null:
		_fail("instantiate failed")
		return null
	if not core.has_method("set_balance_patches") or not core.has_method("get_balance_patches") or not core.has_method("run_match"):
		_fail("core missing balance/run_match API")
		return null
	return core


func _duel_artillery_guardian(seed: int) -> Dictionary:
	var players = [&"artillery"]
	var enemies = [&"guardian"]
	var match_input_obj = MatchReplayInputScript.build_match_input(
		seed,
		players,
		enemies,
		SimConstantsScript.DEFAULT_TICK_RATE,
		false,  # debug_stack_operations
		false      # debug_combat_trace
	).to_dict()
	return match_input_obj


func _stats_for_archetype(summary: Dictionary, archetype: String) -> Dictionary:
	for item in summary.get("unit_stats", []):
		if not item is Dictionary:
			continue
		var u: Dictionary = item
		if String(u.get("archetype", "")) == archetype:
			return u
	return {}


func _run_duel(core: Object, seed: int) -> Dictionary:
	return Dictionary(core.run_match(_duel_artillery_guardian(seed)))


func test_stat_multiplier_brackets_damage(core: Object) -> void:
	## Stronger attack_damage multiplier should yield strictly more damage dealt by artillery (same seed).
	var seed := 4242
	core.set_balance_patches([{
		"patch_id": "suite_high_ad",
		"targets": ["artillery"],
		"roles": [],
		"stat_multipliers": {"attack_damage": 2.0},
		"stat_additions": {},
	}])
	var hi_summary := _run_duel(core, seed)
	var hi_dmg: float = float(_stats_for_archetype(hi_summary, "artillery").get("damage_dealt", 0.0))

	core.set_balance_patches([{
		"patch_id": "suite_low_ad",
		"targets": ["artillery"],
		"roles": [],
		"stat_multipliers": {"attack_damage": 0.25},
		"stat_additions": {},
	}])
	var lo_summary := _run_duel(core, seed)
	var lo_dmg: float = float(_stats_for_archetype(lo_summary, "artillery").get("damage_dealt", 0.0))

	_assert(hi_dmg > lo_dmg + 1.0, "stat patch: expected high attack_damage patch to deal more damage than low (hi=%s lo=%s)" % [hi_dmg, lo_dmg])


func test_inline_ability_reduces_ability_damage(core: Object) -> void:
	## Replace artillery ability with a very weak projectile; ability damage share should drop vs baseline.
	var seed := 5151
	var weak_ability := {
		"kind": "projectile",
		"params": {
			"damage_multiplier": 0.05,
			"damage_type": "physical",
			"radius_override": null,
			"reason": "suite_inline_weak",
			"speed_override": null,
			"stun_duration": 0.8,
		},
	}

	core.set_balance_patches([])
	var base_summary := _run_duel(core, seed)
	var base_ab: float = float(_stats_for_archetype(base_summary, "artillery").get("damage_dealt_ability", 0.0))

	core.set_balance_patches([{
		"patch_id": "suite_inline_ability",
		"targets": ["artillery"],
		"roles": [],
		"stat_multipliers": {},
		"stat_additions": {},
		"ability": weak_ability,
	}])
	var weak_summary := _run_duel(core, seed)
	var weak_ab: float = float(_stats_for_archetype(weak_summary, "artillery").get("damage_dealt_ability", 0.0))

	_assert(weak_ab < base_ab * 0.85, "inline ability: expected weaker ability mult to reduce ability damage (base_ab=%s weak_ab=%s)" % [base_ab, weak_ab])


func test_kit_id_reduces_ability_damage(core: Object) -> void:
	## Named kit in fixtures/goldens/champion_kits.json — same idea as inline but exercises kit_id resolution.
	var seed := 6161
	core.set_balance_patches([])
	var base_summary := _run_duel(core, seed)
	var base_ab: float = float(_stats_for_archetype(base_summary, "artillery").get("damage_dealt_ability", 0.0))

	core.set_balance_patches([{
		"patch_id": "suite_kit_id",
		"targets": ["artillery"],
		"roles": [],
		"stat_multipliers": {},
		"stat_additions": {},
		"kit_id": "balance_suite_weak_artillery_ability",
	}])
	var kit_summary := _run_duel(core, seed)
	var kit_ab: float = float(_stats_for_archetype(kit_summary, "artillery").get("damage_dealt_ability", 0.0))

	_assert(kit_ab < base_ab * 0.85, "kit_id: expected kit weak ability to reduce ability damage (base_ab=%s kit_ab=%s)" % [base_ab, kit_ab])


func test_get_balance_patches_round_trip(core: Object) -> void:
	var original := [{
		"patch_id": "suite_roundtrip",
		"targets": ["artillery", "guardian"],
		"roles": ["marksman"],
		"stat_multipliers": {"attack_damage": 0.9},
		"stat_additions": {"max_hp": 5.0},
		"kit_id": "balance_suite_weak_artillery_ability",
		"ability": {"kind": "heal", "params": {"max_hp_ratio": 0.1, "reason": "rt"}},
		"passive_ids": ["bastion"],
	}]
	core.set_balance_patches(original)
	var back: Array = Array(core.get_balance_patches())
	_assert(back.size() == 1, "round-trip: expected 1 patch, got %d" % back.size())
	var p0: Dictionary = back[0]
	_assert(float(p0.get("stat_multipliers", {}).get("attack_damage", 0.0)) == 0.9, "round-trip: stat_multipliers")
	_assert(float(p0.get("stat_additions", {}).get("max_hp", 0.0)) == 5.0, "round-trip: stat_additions")
	_assert(String(p0.get("kit_id", "")) == "balance_suite_weak_artillery_ability", "round-trip: kit_id")
	_assert(p0.has("ability"), "round-trip: ability present")
	_assert(Array(p0.get("passive_ids", [])) == ["bastion"], "round-trip: passive_ids")


func test_empty_patches_clears_file_overlays(core: Object) -> void:
	## After load, file-based patches are replaced by []; artillery should match schema attack_damage (no file 0.95).
	## We only assert internal consistency: [] then explicit 0.95 mult matches file-default behavior when re-applied.
	var seed := 7171
	core.set_balance_patches([])
	var s0 := _run_duel(core, seed)
	var d0: float = float(_stats_for_archetype(s0, "artillery").get("damage_dealt", 0.0))

	core.set_balance_patches([{
		"patch_id": "file_like",
		"targets": ["artillery"],
		"roles": [],
		"stat_multipliers": {"attack_damage": 0.95},
		"stat_additions": {},
	}])
	var s1 := _run_duel(core, seed)
	var d1: float = float(_stats_for_archetype(s1, "artillery").get("damage_dealt", 0.0))

	_assert(d0 > d1 + 0.01, "empty vs 0.95: unpatched artillery should deal more than 0.95 mult (d0=%s d1=%s)" % [d0, d1])


func _run_suite() -> void:
	print("balance_patch_suite: starting")
	var core := _load_native()
	if core == null:
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	test_stat_multiplier_brackets_damage(core)
	test_inline_ability_reduces_ability_damage(core)
	test_kit_id_reduces_ability_damage(core)
	test_get_balance_patches_round_trip(core)
	test_empty_patches_clears_file_overlays(core)

	if core.has_method("clear"):
		core.clear()
	core = null

	if _failures.size() > 0:
		print("balance_patch_suite: FAILED (%d)" % _failures.size())
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	print("balance_patch_suite: all tests passed")
	await HeadlessShutdownScript.teardown_extension_then_quit(self, 0)
