class_name TeamfightSimulationCoreReference
extends RefCounted

const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const MatchReplayInputScript := preload("res://scripts/simulation/match_replay_input.gd")
const MatchReplaySummaryScript := preload("res://scripts/simulation/match_replay_summary.gd")
const UnitReplaySummaryScript := preload("res://scripts/simulation/unit_replay_summary.gd")
const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")

const TEAM_PLAYER: StringName = &"player"
const TEAM_ENEMY: StringName = &"enemy"

const ACTION_AUTO: StringName = &"auto"
const ACTION_ABILITY: StringName = &"ability"
const ACTION_ULTIMATE: StringName = &"ultimate"
const ACTION_PASSIVE: StringName = &"passive"

const EFFECT_ON_ATTACK: StringName = &"on_attack"
const EFFECT_ON_DEFENSE: StringName = &"on_defense"
const EFFECT_ON_TICK: StringName = &"on_tick"
const EFFECT_POST_ATTACK: StringName = &"post_attack"
const EFFECT_POST_TAKE_DAMAGE: StringName = &"post_take_damage"

var _units: Array = []
var _projectiles: Array = []
var _rng: RandomNumberGenerator = RandomNumberGenerator.new()
var _time: float = 0.0
var _tick_rate: float = SimConstantsScript.DEFAULT_TICK_RATE
var _seed: int = 0
var _winner_team: StringName = &"draw"
var _record_events: bool = false
var _player_comp: Array[StringName] = []
var _enemy_comp: Array[StringName] = []
var _player_kills: int = 0
var _enemy_kills: int = 0

func is_ready() -> bool:
	return false

func clear() -> void:
	_units.clear()
	_projectiles.clear()
	_time = 0.0
	_tick_rate = SimConstantsScript.DEFAULT_TICK_RATE
	_seed = 0
	_winner_team = &"draw"
	_record_events = false
	_player_comp.clear()
	_enemy_comp.clear()
	_player_kills = 0
	_enemy_kills = 0

func run_matches(match_inputs: Array) -> Array:
	var results: Array = []
	results.resize(match_inputs.size())
	for index in range(match_inputs.size()):
		results[index] = run_match(match_inputs[index])
		clear()
	return results

func run_match_simulation_only(match_input: Variant) -> void:
	run_match(match_input)

func run_matches_simulation_only(match_inputs: Array) -> void:
	for index in range(match_inputs.size()):
		run_match_simulation_only(match_inputs[index])
		clear()

func run_match(match_input: Variant):
	clear()

	var input: Dictionary = _match_input_to_dict(match_input)
	if input.is_empty():
		return MatchReplaySummaryScript.new()

	_seed = int(input.get("seed", 0))
	_tick_rate = float(input.get("tick_rate", SimConstantsScript.DEFAULT_TICK_RATE))
	_record_events = bool(input.get("record_events", false))
	_rng.seed = _seed

	_player_comp = _spawn_team(Array(input.get("player_units", [])), TEAM_PLAYER, 1)
	var next_instance_id: int = 1 + _player_comp.size()
	_enemy_comp = _spawn_team(Array(input.get("enemy_units", [])), TEAM_ENEMY, next_instance_id)

	_simulate()
	return _build_summary()

func _match_input_to_dict(match_input: Variant) -> Dictionary:
	if match_input is Dictionary:
		return Dictionary(match_input)
	if match_input is Object and match_input.has_method("to_dict"):
		var converted: Variant = match_input.call("to_dict")
		if converted is Dictionary:
			return Dictionary(converted)
	push_error("TeamfightSimulationCore.run_match() expected MatchReplayInput or Dictionary.")
	return {}

func _spawn_team(spawns: Array, team: StringName, starting_id: int) -> Array[StringName]:
	var comp: Array[StringName] = []
	var instance_id: int = starting_id
	for item in spawns:
		var spawn_data: Dictionary = Dictionary(item)
		var archetype_id: StringName = StringName(String(spawn_data.get("archetype_id", "")))
		comp.append(archetype_id)
		var unit := _spawn_unit(spawn_data, team, instance_id)
		if not unit.is_empty():
			_units.append(unit)
			instance_id += 1
	return comp

func _spawn_unit(spawn_data: Dictionary, team: StringName, instance_id: int) -> Dictionary:
	var archetype_id: StringName = StringName(String(spawn_data.get("archetype_id", "")))
	var champion: Variant = ChampionCatalogScript.get_champion(archetype_id)
	if champion == null:
		push_error("Unknown champion archetype: %s" % String(archetype_id))
		return {}

	var champion_dict: Dictionary = champion.to_dict()
	var stats: Dictionary = _build_effective_stats(Dictionary(champion_dict.get("stats", {})))
	var role_config: Dictionary = _role_config_for(StringName(String(stats.get("role", ""))))
	var passive_effects: Dictionary = _build_passive_effects(champion_dict, role_config)
	var x: float = float(spawn_data.get("x", 0.0))
	var y: float = float(spawn_data.get("y", 0.0))
	var max_hp: float = float(stats.get("max_hp", 0.0))
	var max_mana: float = float(stats.get("max_mana", 0.0))

	return {
		"instance_id": instance_id,
		"archetype_id": archetype_id,
		"team": team,
		"stats": stats,
		"passive_effects": passive_effects,
		"ability_effect": champion_dict.get("ability", null),
		"ultimate_effect": champion_dict.get("ultimate", null),
		"spawn_x": x,
		"spawn_y": y,
		"x": x,
		"y": y,
		"hp": max_hp,
		"shield": 0.0,
		"mana": 0.0,
		"attack_cooldown": 0.0,
		"ability_cooldown": float(stats.get("ability_cd", 0.0)),
		"ultimate_cooldown": float(stats.get("ultimate_cd", 0.0)),
		"casting_remaining": 0.0,
		"casting_kind": &"",
		"casting_effect": null,
		"casting_target_id": 0,
		"casting_ally_target_id": 0,
		"cast_resolved_this_tick": false,
		"stun_remaining": 0.0,
		"respawn_timer": 0.0,
		"respawned_this_tick": false,
		"target_id": 0,
		"distance_to_target": 0.0,
		"in_range": false,
		"current_target_score": INF,
		"retarget_timer": 0.0,
		"target_switch_lock_timer": 0.0,
		"incoming_target_count": 0,
		"perceived_threat": 0.0,
		"alive": true,
		"attack_count": 0,
		"damage_dealt": 0.0,
		"damage_dealt_auto": 0.0,
		"damage_dealt_ability": 0.0,
		"damage_dealt_ultimate": 0.0,
		"damage_dealt_passive": 0.0,
		"damage_received": 0.0,
		"damage_mitigated": 0.0,
		"healing_done": 0.0,
		"healing_done_auto": 0.0,
		"healing_done_ability": 0.0,
		"healing_done_ultimate": 0.0,
		"healing_done_passive": 0.0,
		"shielding_done": 0.0,
		"shielding_done_auto": 0.0,
		"shielding_done_ability": 0.0,
		"shielding_done_ultimate": 0.0,
		"shielding_done_passive": 0.0,
		"auto_attacks": 0,
		"abilities": 0,
		"ultimates": 0,
		"stuns": 0,
		"kills": 0,
		"deaths": 0,
		"assists": 0,
		"taunt_target_id": 0,
		"taunt_remaining": 0.0,
		"damage_sources": {},
		"last_hit_time": 0.0,
		"regen_accumulator": 0.0,
	}

func _build_effective_stats(base_stats: Dictionary) -> Dictionary:
	var result: Dictionary = base_stats.duplicate(true)
	var role_config: Dictionary = _role_config_for(StringName(String(result.get("role", ""))))
	for key in role_config.get("stat_mods", {}).keys():
		result[key] = role_config["stat_mods"][key]
	return result

func _role_config_for(role: StringName) -> Dictionary:
	var role_config: Variant = ChampionCatalogScript.build_role_configs().get(role, null)
	if role_config == null:
		return {"stat_mods": {}, "passive_on_tick": null, "passive_post_take_damage": null}
	if role_config is Object and role_config.has_method("to_dict"):
		return Dictionary(role_config.call("to_dict"))
	if role_config is Dictionary:
		return Dictionary(role_config)
	return {"stat_mods": {}, "passive_on_tick": null, "passive_post_take_damage": null}

func _scale_priority_dict(base: Dictionary) -> Dictionary:
	var scaled: Dictionary = {}
	for key in base.keys():
		scaled[key] = float(base[key]) * SimConstantsScript.ROLE_PRIORITY_GLOBAL_SCALE
	return scaled

func _build_passive_effects(champion_dict: Dictionary, role_config: Dictionary) -> Dictionary:
	var passive_effects: Dictionary = {
		EFFECT_ON_ATTACK: [],
		EFFECT_ON_DEFENSE: [],
		EFFECT_ON_TICK: [],
		EFFECT_POST_ATTACK: [],
		EFFECT_POST_TAKE_DAMAGE: [],
	}
	var passive_ids: Array = Array(champion_dict.get("passive_ids", []))
	for passive_id_value in passive_ids:
		var passive_id: StringName = StringName(String(passive_id_value))
		var entry: Variant = ChampionCatalogScript.get_passive_entry(passive_id)
		if entry is Dictionary:
			_append_effects(passive_effects, Dictionary(entry))
	var role_tick: Variant = role_config.get("passive_on_tick", null)
	if role_tick != null:
		_append_effect(passive_effects, EFFECT_ON_TICK, role_tick)
	var role_take_damage: Variant = role_config.get("passive_post_take_damage", null)
	if role_take_damage != null:
		_append_effect(passive_effects, EFFECT_POST_TAKE_DAMAGE, role_take_damage)
	return passive_effects

func _append_effects(passive_effects: Dictionary, entry: Dictionary) -> void:
	for key in [EFFECT_ON_ATTACK, EFFECT_ON_DEFENSE, EFFECT_ON_TICK, EFFECT_POST_ATTACK, EFFECT_POST_TAKE_DAMAGE]:
		var effect_list: Array = Array(entry.get(key, []))
		for effect in effect_list:
			_append_effect(passive_effects, key, effect)

func _append_effect(passive_effects: Dictionary, kind: StringName, effect: Variant) -> void:
	var effect_list: Array = Array(passive_effects.get(kind, []))
	effect_list.append(effect)
	passive_effects[kind] = effect_list

func _simulate() -> void:
	var max_ticks: int = int(ceil(SimConstantsScript.MATCH_DURATION / maxf(_tick_rate, SimConstantsScript.EPSILON)))
	for tick_index in range(max_ticks):
		_step_tick()

	_winner_team = _determine_winner()

func _step_tick() -> void:
	_time += _tick_rate
	for unit in _units:
		unit["respawned_this_tick"] = false
		unit["cast_resolved_this_tick"] = false
		unit["retarget_timer"] = maxf(0.0, float(unit.get("retarget_timer", 0.0)) - _tick_rate)
		unit["target_switch_lock_timer"] = maxf(0.0, float(unit.get("target_switch_lock_timer", 0.0)) - _tick_rate)
	_refresh_target_pressure()
	_update_cooldowns_and_status()
	_update_projectiles()
	_process_actions()

func _refresh_target_pressure() -> void:
	for unit in _units:
		unit["incoming_target_count"] = 0
	for unit in _units:
		if not bool(unit.get("alive", false)):
			continue
		var target_id: int = int(unit.get("target_id", 0))
		if target_id == 0:
			continue
		var target: Dictionary = _unit_by_id(target_id)
		if not target.is_empty() and bool(target.get("alive", false)):
			target["incoming_target_count"] = int(target.get("incoming_target_count", 0)) + 1

func _update_cooldowns_and_status() -> void:
	for unit in _units:
		if not bool(unit.get("alive", false)):
			unit["respawn_timer"] = maxf(0.0, float(unit.get("respawn_timer", 0.0)) - _tick_rate)
			if float(unit.get("respawn_timer", 0.0)) <= 0.0:
				_respawn_unit(unit)
			continue
		unit["attack_cooldown"] = maxf(0.0, float(unit.get("attack_cooldown", 0.0)) - _tick_rate)
		unit["ability_cooldown"] = maxf(0.0, float(unit.get("ability_cooldown", 0.0)) - _tick_rate)
		unit["ultimate_cooldown"] = maxf(0.0, float(unit.get("ultimate_cooldown", 0.0)) - _tick_rate)
		unit["stun_remaining"] = maxf(0.0, float(unit.get("stun_remaining", 0.0)) - _tick_rate)
		unit["taunt_remaining"] = maxf(0.0, float(unit.get("taunt_remaining", 0.0)) - _tick_rate)
		if float(unit.get("stun_remaining", 0.0)) <= 0.0:
			unit["stun_remaining"] = 0.0
		if float(unit.get("taunt_remaining", 0.0)) <= 0.0:
			unit["taunt_remaining"] = 0.0
		if float(unit.get("casting_remaining", 0.0)) > 0.0:
			unit["casting_remaining"] = maxf(0.0, float(unit.get("casting_remaining", 0.0)) - _tick_rate)
			if float(unit.get("casting_remaining", 0.0)) <= 0.0:
				var failed_cast_kind: StringName = StringName(String(unit.get("casting_kind", "")))
				if not _resolve_cast(unit):
					if failed_cast_kind == ACTION_ABILITY:
						unit["ability_cooldown"] = 0.0
					elif failed_cast_kind == ACTION_ULTIMATE:
						unit["ultimate_cooldown"] = 0.0
						unit["mana"] = minf(float(Dictionary(unit.get("stats", {})).get("max_mana", 0.0)), float(unit.get("mana", 0.0)) + float(Dictionary(unit.get("stats", {})).get("max_mana", 0.0)))
				unit["cast_resolved_this_tick"] = true
		var regen_accumulator: float = float(unit.get("regen_accumulator", 0.0)) + _tick_rate
		while regen_accumulator >= SimConstantsScript.REGEN_TICK_INTERVAL:
			regen_accumulator -= SimConstantsScript.REGEN_TICK_INTERVAL
			var effects: Array = _collect_effects(unit, EFFECT_ON_TICK)
			for effect in effects:
				var context: Dictionary = _build_context(unit, {}, {}, 0.0, ACTION_PASSIVE)
				_execute_effect(effect, context)
		unit["regen_accumulator"] = regen_accumulator

func _process_actions() -> void:
	for unit in _units:
		if not bool(unit.get("alive", false)):
			continue
		if bool(unit.get("respawned_this_tick", false)):
			continue
		if bool(unit.get("cast_resolved_this_tick", false)):
			continue
		if float(unit.get("casting_remaining", 0.0)) > 0.0:
			continue
		if float(unit.get("stun_remaining", 0.0)) > 0.0:
			continue

		var target: Dictionary = _select_enemy_target(unit)
		if target.is_empty():
			unit["target_id"] = 0
			unit["distance_to_target"] = 0.0
			unit["in_range"] = false
			unit["current_target_score"] = INF
			continue

		var distance: float = _distance_between(unit, target)
		unit["target_id"] = int(target.get("instance_id", 0))
		unit["distance_to_target"] = distance
		unit["in_range"] = SimConstantsScript.is_melee_in_contact(distance, _attack_range(unit))
		unit["current_target_score"] = _score_enemy_target(unit, target, _strategy_for_unit(unit))
		if _try_cast_ultimate(unit, target, distance):
			continue
		if _try_cast_ability(unit, target, distance):
			continue
		if SimConstantsScript.is_melee_in_contact(distance, _attack_range(unit)):
			_perform_auto_attack(unit, target, distance)
		else:
			_move_toward_target(unit, target)

func _try_cast_ability(unit: Dictionary, target: Dictionary, distance: float) -> bool:
	if float(unit.get("ability_cooldown", 0.0)) > 0.0:
		return false
	return _start_cast(unit, target, distance, ACTION_ABILITY)

func _try_cast_ultimate(unit: Dictionary, target: Dictionary, distance: float) -> bool:
	var stats: Dictionary = Dictionary(unit.get("stats", {}))
	var max_mana: float = float(stats.get("max_mana", 0.0))
	if float(unit.get("ultimate_cooldown", 0.0)) > 0.0:
		return false
	if max_mana <= 0.0:
		return false
	if float(unit.get("mana", 0.0)) < max_mana:
		return false
	return _start_cast(unit, target, distance, ACTION_ULTIMATE)

func _start_cast(unit: Dictionary, target: Dictionary, distance: float, action_kind: StringName) -> bool:
	var effect: Variant = unit.get("ability_effect", null) if action_kind == ACTION_ABILITY else unit.get("ultimate_effect", null)
	if effect == null:
		return false

	var target_ally: Dictionary = _select_ally_target(unit)
	if action_kind == ACTION_ABILITY:
		unit["ability_cooldown"] = float(Dictionary(unit.get("stats", {})).get("ability_cd", 0.0))
		unit["abilities"] = int(unit.get("abilities", 0)) + 1
	else:
		unit["ultimate_cooldown"] = float(Dictionary(unit.get("stats", {})).get("ultimate_cd", 0.0))
		unit["ultimates"] = int(unit.get("ultimates", 0)) + 1
		var consumed_mana: float = float(Dictionary(unit.get("stats", {})).get("max_mana", 0.0))
		unit["mana"] = maxf(0.0, float(unit.get("mana", 0.0)) - consumed_mana)

	unit["casting_remaining"] = SimConstantsScript.CASTING_WINDUP
	unit["casting_kind"] = action_kind
	unit["casting_effect"] = effect
	unit["casting_target_id"] = int(target.get("instance_id", 0))
	unit["casting_ally_target_id"] = int(target_ally.get("instance_id", 0)) if not target_ally.is_empty() else 0
	return true

func _resolve_cast(unit: Dictionary) -> bool:
	var effect: Variant = unit.get("casting_effect", null)
	var action_kind: StringName = StringName(String(unit.get("casting_kind", "")))
	var target: Dictionary = _unit_by_id(int(unit.get("casting_target_id", 0)))
	var target_ally: Dictionary = _unit_by_id(int(unit.get("casting_ally_target_id", 0)))
	unit["casting_remaining"] = 0.0
	unit["casting_kind"] = &""
	unit["casting_effect"] = null
	unit["casting_target_id"] = 0
	unit["casting_ally_target_id"] = 0
	unit["cast_resolved_this_tick"] = false
	if effect == null:
		return false
	if action_kind == ACTION_ABILITY or action_kind == ACTION_ULTIMATE:
		if target.is_empty() and target_ally.is_empty():
			return false
	var context: Dictionary = _build_context(unit, target, target_ally, 0.0, action_kind)
	var result: Dictionary = _execute_effect(effect, context)
	if result.is_empty():
		return false
	return true

func _perform_auto_attack(unit: Dictionary, target: Dictionary, distance: float) -> void:
	var stats: Dictionary = Dictionary(unit.get("stats", {}))
	var attack_speed: float = maxf(0.0001, float(stats.get("attack_speed", 1.0)))
	unit["attack_cooldown"] = 1.0 / attack_speed
	unit["auto_attacks"] = int(unit.get("auto_attacks", 0)) + 1
	unit["attack_count"] = int(unit.get("attack_count", 0)) + 1
	var mana_gain: float = float(stats.get("mana_per_attack", 0.0))
	if mana_gain > 0.0:
		var max_mana: float = float(stats.get("max_mana", 0.0))
		unit["mana"] = minf(max_mana, float(unit.get("mana", 0.0)) + mana_gain)

	var damage: float = float(stats.get("attack_damage", 0.0))
	damage = _apply_attack_modifiers(unit, target, distance, damage)
	if float(stats.get("attack_range", 0.0)) > SimConstantsScript.RANGED_THRESHOLD:
		_projectiles.append({
			"source_id": int(unit.get("instance_id", 0)),
			"target_id": int(target.get("instance_id", 0)),
			"damage": damage,
			"damage_type": &"physical",
			"stun_duration": 0.0,
			"radius": float(stats.get("projectile_radius", SimConstantsScript.DEFAULT_PROJECTILE_RADIUS)),
			"time_remaining": distance / maxf(0.0001, float(stats.get("projectile_speed", SimConstantsScript.DEFAULT_PROJECTILE_SPEED))),
			"action_kind": ACTION_AUTO,
			"reason": "Auto Attack",
		})
		return
	var context: Dictionary = _build_context(unit, target, {}, damage, ACTION_AUTO)
	var dealt: float = _apply_damage(unit, target, damage, "physical", ACTION_AUTO, context)
	_run_post_attack_effects(unit, target, dealt, context)

func _move_toward_target(unit: Dictionary, target: Dictionary) -> void:
	var stats: Dictionary = Dictionary(unit.get("stats", {}))
	var speed: float = float(stats.get("move_speed", 0.0)) * _tick_rate
	if speed <= 0.0:
		return
	var origin: Vector2 = Vector2(float(unit.get("x", 0.0)), float(unit.get("y", 0.0)))
	var destination: Vector2 = Vector2(float(target.get("x", 0.0)), float(target.get("y", 0.0)))
	var direction: Vector2 = destination - origin
	var distance: float = direction.length()
	if distance <= SimConstantsScript.EPSILON:
		return
	var effective_range: float = _effective_attack_range(unit)
	var max_step: float = minf(speed, maxf(0.0, distance - effective_range))
	if max_step <= 0.0:
		return
	unit["x"] = origin.x + direction.normalized().x * max_step
	unit["y"] = origin.y + direction.normalized().y * max_step

func _update_projectiles() -> void:
	var next_projectiles: Array = []
	for projectile in _projectiles:
		var data: Dictionary = Dictionary(projectile)
		data["time_remaining"] = float(data.get("time_remaining", 0.0)) - _tick_rate
		if float(data.get("time_remaining", 0.0)) > 0.0:
			next_projectiles.append(data)
			continue
		_resolve_projectile(data)
	_projectiles = next_projectiles

func _resolve_projectile(projectile: Dictionary) -> void:
	var source: Dictionary = _unit_by_id(int(projectile.get("source_id", 0)))
	var target: Dictionary = _unit_by_id(int(projectile.get("target_id", 0)))
	if source.is_empty() or target.is_empty():
		return
	if not bool(source.get("alive", false)) or not bool(target.get("alive", false)):
		return

	var damage: float = float(projectile.get("damage", 0.0))
	var damage_type: StringName = StringName(String(projectile.get("damage_type", "physical")))
	var action_kind: StringName = StringName(String(projectile.get("action_kind", ACTION_AUTO)))
	var context: Dictionary = _build_context(source, target, {}, damage, action_kind)
	var dealt: float = _apply_damage(source, target, damage, damage_type, action_kind, context)
	if float(projectile.get("stun_duration", 0.0)) > 0.0 and bool(target.get("alive", false)):
		_apply_stun(source, target, float(projectile.get("stun_duration", 0.0)))
	if float(projectile.get("radius", 0.0)) > 0.0:
		_apply_splash_damage(source, target, dealt, float(projectile.get("radius", 0.0)), damage_type, action_kind, String(projectile.get("reason", "Projectile Splash")))
	_run_post_attack_effects(source, target, dealt, context)

func _build_context(source: Dictionary, target: Variant, target_ally: Variant, damage: float, action_kind: StringName) -> Dictionary:
	return {
		"unit": source,
		"target": target,
		"target_ally": target_ally,
		"damage": damage,
		"action_kind": action_kind,
		"distance": _distance_between(source, target) if not target.is_empty() else 0.0,
	}

func _collect_effects(unit: Dictionary, kind: StringName) -> Array:
	var effects: Array = []
	var passive_effects: Dictionary = Dictionary(unit.get("passive_effects", {}))
	effects.append_array(Array(passive_effects.get(kind, [])))
	return effects

func _apply_attack_modifiers(unit: Dictionary, target: Dictionary, distance: float, damage: float) -> float:
	var modified_damage: float = damage
	var context: Dictionary = _build_context(unit, target, {}, damage, ACTION_AUTO)
	var effects: Array = _collect_effects(unit, EFFECT_ON_ATTACK)
	for effect in effects:
		modified_damage *= _evaluate_multiplier_effect(effect, context, modified_damage)
	return modified_damage

func _apply_damage(source: Dictionary, target: Dictionary, damage: float, damage_type: StringName, action_kind: StringName, context: Dictionary) -> float:
	if not bool(target.get("alive", false)):
		return 0.0

	var incoming: float = damage
	if damage_type != &"true":
		incoming *= _defense_multiplier(target, source, incoming, action_kind)
		incoming *= _damage_type_multiplier(target, damage_type)
	if incoming <= 0.0:
		return 0.0

	var shield_before: float = float(target.get("shield", 0.0))
	var absorbed: float = minf(shield_before, incoming)
	target["shield"] = maxf(0.0, shield_before - absorbed)
	var hp_loss: float = maxf(0.0, incoming - absorbed)
	target["hp"] = maxf(0.0, float(target.get("hp", 0.0)) - hp_loss)
	target["damage_received"] = float(target.get("damage_received", 0.0)) + incoming
	target["damage_mitigated"] = float(target.get("damage_mitigated", 0.0)) + maxf(0.0, damage - incoming) + absorbed

	source["damage_dealt"] = float(source.get("damage_dealt", 0.0)) + incoming
	match action_kind:
		ACTION_AUTO:
			source["damage_dealt_auto"] = float(source.get("damage_dealt_auto", 0.0)) + incoming
		ACTION_ABILITY:
			source["damage_dealt_ability"] = float(source.get("damage_dealt_ability", 0.0)) + incoming
		ACTION_ULTIMATE:
			source["damage_dealt_ultimate"] = float(source.get("damage_dealt_ultimate", 0.0)) + incoming
		ACTION_PASSIVE:
			source["damage_dealt_passive"] = float(source.get("damage_dealt_passive", 0.0)) + incoming

	if hp_loss > 0.0 or absorbed > 0.0:
		var source_damage: Dictionary = Dictionary(target.get("damage_sources", {}))
		var entry: Dictionary = Dictionary(source_damage.get(int(source.get("instance_id", 0)), {"damage": 0.0, "time": 0.0}))
		entry["damage"] = float(entry.get("damage", 0.0)) + incoming
		entry["time"] = _time
		source_damage[int(source.get("instance_id", 0))] = entry
		target["damage_sources"] = source_damage
		target["last_hit_time"] = _time
		if float(target.get("stats", {}).get("max_hp", 0.0)) > 0.0 and incoming > float(target.get("stats", {}).get("max_hp", 0.0)) * SimConstantsScript.THREAT_BURST_THRESHOLD:
			target["perceived_threat"] = float(target.get("perceived_threat", 0.0)) + ((incoming / float(target.get("stats", {}).get("max_hp", 1.0))) * SimConstantsScript.THREAT_BURST_MULTIPLIER)

	# Run post_take_damage effects with passive action kind
	var post_effects: Array = _collect_effects(target, EFFECT_POST_TAKE_DAMAGE)
	if not post_effects.is_empty():
		var post_context: Dictionary = _build_context(target, {}, {}, incoming, ACTION_PASSIVE)
		for effect in post_effects:
			_execute_effect(effect, post_context)

	if float(target.get("hp", 0.0)) <= 0.0:
		_handle_death(source, target)

	return incoming

func _defense_multiplier(target: Dictionary, source: Dictionary, damage: float, action_kind: StringName) -> float:
	var multiplier: float = 1.0
	var context: Dictionary = _build_context(source, target, {}, damage, action_kind)
	var effects: Array = _collect_effects(target, EFFECT_ON_DEFENSE)
	for effect in effects:
		multiplier *= _evaluate_multiplier_effect(effect, context, multiplier)
	return multiplier

func _damage_type_multiplier(target: Dictionary, damage_type: StringName) -> float:
	if damage_type == &"physical":
		var armor: float = float(Dictionary(target.get("stats", {})).get("armor", 0.0))
		return clampf(1.0 - armor, 0.05, 1.0)
	if damage_type == &"magic":
		var mr: float = float(Dictionary(target.get("stats", {})).get("magic_resist", 0.0))
		return clampf(1.0 - mr, 0.05, 1.0)
	return 1.0

func _evaluate_multiplier_effect(effect: Variant, context: Dictionary, current_value: float) -> float:
	var effect_dict: Dictionary = _effect_to_dict(effect)
	var kind: StringName = StringName(String(effect_dict.get("kind", "")))
	var params: Dictionary = Dictionary(effect_dict.get("params", {}))
	match kind:
		&"constant_multiplier":
			return float(params.get("multiplier", 1.0))
		&"target_hp_threshold_multiplier":
			var target: Dictionary = Dictionary(context.get("target", {}))
			if target.is_empty():
				return 1.0
			var hp_ratio_threshold: float = float(params.get("hp_ratio_threshold", 0.0))
			var target_hp: float = float(target.get("hp", 0.0))
			var target_max_hp: float = maxf(0.0001, float(Dictionary(target.get("stats", {})).get("max_hp", 1.0)))
			if target_hp / target_max_hp < hp_ratio_threshold:
				return float(params.get("multiplier", 1.0))
			return 1.0
		&"distance_threshold_multiplier":
			var min_distance: float = float(params.get("min_distance", 0.0))
			if float(context.get("distance", 0.0)) > min_distance:
				return float(params.get("multiplier", 1.0))
			return 1.0
		&"self_hp_threshold_multiplier":
			var source: Dictionary = Dictionary(context.get("unit", {}))
			var min_hp_ratio: float = float(params.get("min_hp_ratio", 0.0))
			var hp_ratio: float = float(source.get("hp", 0.0)) / maxf(0.0001, float(Dictionary(source.get("stats", {})).get("max_hp", 1.0)))
			if hp_ratio > min_hp_ratio:
				return float(params.get("multiplier", 1.0))
			return 1.0
		&"dodge":
			var dodge_chance: float = clampf(float(params.get("dodge_chance", 0.0)), 0.0, 1.0)
			if _rng.randf() < dodge_chance:
				return float(params.get("on_dodge_multiplier", 0.0))
			return float(params.get("on_hit_multiplier", 1.0))
		_:
			return 1.0

func _execute_effect(effect: Variant, context: Dictionary) -> Dictionary:
	var effect_dict: Dictionary = _effect_to_dict(effect)
	var kind: StringName = StringName(String(effect_dict.get("kind", "")))
	var params: Dictionary = Dictionary(effect_dict.get("params", {}))
	var source: Dictionary = Dictionary(context.get("unit", {}))
	var target: Dictionary = Dictionary(context.get("target", {}))
	var target_ally: Dictionary = Dictionary(context.get("target_ally", {}))

	match kind:
		&"multi":
			var combined: Dictionary = {}
			for child in Array(params.get("effects", [])):
				var child_result: Dictionary = _execute_effect(child, context)
				_merge_result(combined, child_result)
			return combined
		&"damage":
			if target.is_empty():
				return {}
			var damage_multiplier: float = float(params.get("damage_multiplier", 1.0))
			var damage_type: StringName = StringName(String(params.get("damage_type", "physical")))
			var damage: float = float(source.get("stats", {}).get("attack_damage", 0.0)) * damage_multiplier
			var dealt: float = _apply_damage(source, target, damage, damage_type, StringName(String(context.get("action_kind"))), context)
			if bool(params.get("trigger_on_hit", true)):
				_run_post_attack_effects(source, target, dealt, context)
			return {"damage": dealt, "reason": String(params.get("reason", ""))}
		&"projectile":
			if target.is_empty():
				return {}
			var projectile_speed: float = float(params.get("speed_override", float(Dictionary(source.get("stats", {})).get("projectile_speed", SimConstantsScript.DEFAULT_PROJECTILE_SPEED))))
			var radius: float = float(params.get("radius_override", 0.0))
			var damage_multiplier: float = float(params.get("damage_multiplier", 1.0))
			var damage_type: StringName = StringName(String(params.get("damage_type", "physical")))
			var stun_duration: float = float(params.get("stun_duration", 0.0))
			var damage: float = float(Dictionary(source.get("stats", {})).get("attack_damage", 0.0)) * damage_multiplier
			var distance: float = _distance_between(source, target)
			var time_remaining: float = distance / maxf(0.0001, projectile_speed)
			_projectiles.append({
				"source_id": int(source.get("instance_id", 0)),
				"target_id": int(target.get("instance_id", 0)),
				"damage": damage,
				"damage_type": damage_type,
				"stun_duration": stun_duration,
				"radius": radius,
				"time_remaining": time_remaining,
				"action_kind": context.get("action_kind"),
				"reason": String(params.get("reason", "")),
			})
			return {"damage": damage, "reason": String(params.get("reason", "")), "use_projectile": true}
		&"stun":
			if not target.is_empty():
				var duration: float = float(params.get("duration", 0.0))
				_apply_stun(source, target, duration)
			return {"stun": float(params.get("duration", 0.0)), "reason": String(params.get("reason", ""))}
		&"shield":
			var shield_target: Dictionary = target_ally if not target_ally.is_empty() else source
			var amount: float = float(source.get("stats", {}).get("max_hp", 0.0)) * float(params.get("max_hp_ratio", 0.0))
			_add_shield(source, shield_target, amount, StringName(String(context.get("action_kind"))))
			return {"shield_added": amount, "reason": String(params.get("reason", ""))}
		&"heal":
			var heal_target: Dictionary = target_ally if not target_ally.is_empty() else source
			var heal_amount: float = (
				float(source.get("stats", {}).get("max_hp", 0.0)) * float(params.get("max_hp_ratio", 0.0))
				+ float(heal_target.get("hp", 0.0)) * float(params.get("current_hp_ratio", 0.0))
				+ float(params.get("flat_amount", 0.0))
			)
			_heal_unit(source, heal_target, heal_amount, StringName(String(context.get("action_kind"))))
			return {"healing": heal_amount, "reason": String(params.get("reason", ""))}
		&"self_damage":
			var self_damage: float = float(source.get("stats", {}).get("max_hp", 0.0)) * float(params.get("damage_ratio", 0.0))
			_apply_damage(source, source, self_damage, StringName(String(params.get("damage_type", "true"))), StringName(String(context.get("action_kind"))), context)
			return {"damage": self_damage, "reason": String(params.get("reason", ""))}
		&"self_shield":
			var self_shield: float = float(source.get("stats", {}).get("max_hp", 0.0)) * float(params.get("max_hp_ratio", 0.0))
			_add_shield(source, source, self_shield, StringName(String(context.get("action_kind"))))
			return {"shield_added": self_shield, "reason": String(params.get("reason", ""))}
		&"self_aoe_taunt":
			var radius: float = float(params.get("radius", 0.0))
			var duration: float = float(params.get("duration", 0.0))
			_apply_aoe_taunt(source, radius, duration)
			return {"taunt": duration, "reason": String(params.get("reason", ""))}
		&"self_aoe_damage":
			var radius_damage: float = float(params.get("radius", 0.0))
			var aoe_multiplier: float = float(params.get("damage_multiplier", 1.0))
			var aoe_type: StringName = StringName(String(params.get("damage_type", "physical")))
			var aoe_damage: float
			if params.has("flat_amount"):
				aoe_damage = float(params.get("flat_amount", 0.0))
			else:
				aoe_damage = float(source.get("stats", {}).get("attack_damage", 0.0)) * aoe_multiplier
			var total_damage: float = _apply_aoe_damage(source, source, aoe_damage, radius_damage, aoe_type, String(params.get("reason", "")), StringName(String(context.get("action_kind"))))
			return {"damage": total_damage, "reason": String(params.get("reason", ""))}
		&"splash_damage":
			var splash_radius: float = float(params.get("radius", 0.0))
			var splash_ratio: float = float(params.get("ratio", 0.0))
			var splash_damage_type: StringName = StringName(String(params.get("damage_type", "physical")))
			var splash_reason: String = String(params.get("reason", "Splash"))
			_apply_splash_damage(source, target, float(context.get("damage", 0.0)), splash_radius, splash_damage_type, StringName(String(context.get("action_kind"))), splash_reason, splash_ratio)
			return {"damage": float(context.get("damage", 0.0)), "reason": splash_reason}
		&"threshold_splash_damage":
			var threshold_multiplier: float = float(params.get("threshold_multiplier", 1.0))
			if float(context.get("damage", 0.0)) > float(Dictionary(source.get("stats", {})).get("attack_damage", 0.0)) * threshold_multiplier:
				var nested: Variant = params.get("splash", null)
				if nested != null:
					return _execute_effect(nested, context)
			return {}
		&"mana_regen":
			var mana_amount: float = float(params.get("flat_amount", 0.0)) + (float(source.get("stats", {}).get("max_mana", 0.0)) * float(params.get("max_mana_ratio", 0.0)))
			_restore_mana(source, source, mana_amount)
			return {"mana_restored": mana_amount}
		&"post_damage_mana_gain":
			var gain: float = float(context.get("damage", 0.0)) * float(params.get("damage_ratio", 0.0))
			_restore_mana(source, source, gain)
			return {"mana_restored": gain}
		&"damage_based_heal":
			var heal_ratio: float = float(params.get("heal_ratio", 0.0))
			var heal_amount: float = float(context.get("damage", 0.0)) * heal_ratio
			_heal_unit(source, source, heal_amount, StringName(String(context.get("action_kind"))))
			return {"healing": heal_amount}
		&"mana_restore_on_hit":
			var restore_amount: float = float(params.get("flat_amount", 0.0))
			_restore_mana(source, source, restore_amount)
			return {"mana_restored": restore_amount}
		&"drain_target_mana_on_hit":
			if target.is_empty():
				return {}
			var drain_amount: float = float(params.get("flat_amount", 0.0))
			target["mana"] = maxf(0.0, float(target.get("mana", 0.0)) - drain_amount)
			return {"mana_drained": drain_amount}
		&"every_n_attacks_stun":
			var every_n: int = int(params.get("every_n", 0))
			if every_n > 0:
				var attack_count: int = int(source.get("attack_count", 0))
				if attack_count % every_n == 0 and not target.is_empty():
					_apply_stun(source, target, float(params.get("stun_duration", 0.0)))
			return {"reason": String(params.get("reason", ""))}
		&"self_dash":
			var dash_distance: float = float(params.get("distance", 1.0))
			var dash_direction: Dictionary = Dictionary(params.get("direction", {}))
			var target_id: int = int(params.get("target_id", 0))
			
			var current_x: float = float(source.get("x", 0.0))
			var current_y: float = float(source.get("y", 0.0))
			
			var new_x: float = current_x
			var new_y: float = current_y
			
			if target_id != 0:
				var target_unit: Dictionary = _unit_by_id(target_id)
				if not target_unit.is_empty():
					var target_x: float = float(target_unit.get("x", 0.0))
					var target_y: float = float(target_unit.get("y", 0.0))
					var dx: float = target_x - current_x
					var dy: float = target_y - current_y
					var dist: float = sqrt(dx*dx + dy*dy)
					if dist > 0.0:
						var move_dist: float = minf(dash_distance, dist)
						new_x = current_x + (dx / dist) * move_dist
						new_y = current_y + (dy / dist) * move_dist
			else:
				new_x = current_x + float(dash_direction.get("x", 0.0)) * dash_distance
				new_y = current_y + float(dash_direction.get("y", 0.0)) * dash_distance
			
			new_x = clampf(new_x, 0.0, _map_width)
			new_y = clampf(new_y, 0.0, _map_height)
			
			source["x"] = new_x
			source["y"] = new_y
			return {"dash_distance": dash_distance}
		&"dodge":
			return {}
		&"constant_multiplier":
			return {}
		&"target_hp_threshold_multiplier":
			return {}
		&"distance_threshold_multiplier":
			return {}
		&"self_hp_threshold_multiplier":
			return {}
		_:
			return {}

func _merge_result(target_result: Dictionary, source_result: Dictionary) -> void:
	for key in source_result.keys():
		var source_value: Variant = source_result[key]
		if target_result.has(key) and source_value is float and target_result[key] is float:
			target_result[key] = float(target_result[key]) + float(source_value)
		elif target_result.has(key) and source_value is int and target_result[key] is int:
			target_result[key] = int(target_result[key]) + int(source_value)
		else:
			target_result[key] = source_value

func _run_post_attack_effects(source: Dictionary, target: Dictionary, damage: float, context: Dictionary) -> void:
	var post_attack_effects: Array = _collect_effects(source, EFFECT_POST_ATTACK)
	if post_attack_effects.is_empty():
		return
	var effect_context: Dictionary = context.duplicate(true)
	effect_context["damage"] = damage
	for effect in post_attack_effects:
		_execute_effect(effect, effect_context)

func _apply_stun(source: Dictionary, target: Dictionary, duration: float) -> void:
	if duration <= 0.0 or target.is_empty():
		return
	target["stun_remaining"] = maxf(float(target.get("stun_remaining", 0.0)), duration)
	source["stuns"] = int(source.get("stuns", 0)) + 1

func _add_shield(source: Dictionary, target: Dictionary, amount: float, action_kind: StringName) -> void:
	if amount <= 0.0 or target.is_empty():
		return
	target["shield"] = float(target.get("shield", 0.0)) + amount
	source["shielding_done"] = float(source.get("shielding_done", 0.0)) + amount
	match action_kind:
		ACTION_AUTO:
			source["shielding_done_auto"] = float(source.get("shielding_done_auto", 0.0)) + amount
		ACTION_ABILITY:
			source["shielding_done_ability"] = float(source.get("shielding_done_ability", 0.0)) + amount
		ACTION_ULTIMATE:
			source["shielding_done_ultimate"] = float(source.get("shielding_done_ultimate", 0.0)) + amount
		ACTION_PASSIVE:
			source["shielding_done_passive"] = float(source.get("shielding_done_passive", 0.0)) + amount

func _heal_unit(source: Dictionary, target: Dictionary, amount: float, action_kind: StringName) -> void:
	if amount <= 0.0 or target.is_empty():
		return
	var stats: Dictionary = Dictionary(target.get("stats", {}))
	var max_hp: float = float(stats.get("max_hp", 0.0))
	var new_hp: float = minf(max_hp, float(target.get("hp", 0.0)) + amount)
	target["hp"] = new_hp
	source["healing_done"] = float(source.get("healing_done", 0.0)) + amount
	match action_kind:
		ACTION_AUTO:
			source["healing_done_auto"] = float(source.get("healing_done_auto", 0.0)) + amount
		ACTION_ABILITY:
			source["healing_done_ability"] = float(source.get("healing_done_ability", 0.0)) + amount
		ACTION_ULTIMATE:
			source["healing_done_ultimate"] = float(source.get("healing_done_ultimate", 0.0)) + amount
		ACTION_PASSIVE:
			source["healing_done_passive"] = float(source.get("healing_done_passive", 0.0)) + amount

func _restore_mana(source: Dictionary, target: Dictionary, amount: float) -> void:
	if amount <= 0.0 or target.is_empty():
		return
	var max_mana: float = float(Dictionary(target.get("stats", {})).get("max_mana", 0.0))
	target["mana"] = minf(max_mana, float(target.get("mana", 0.0)) + amount)

func _apply_splash_damage(source: Dictionary, target: Dictionary, damage: float, radius: float, damage_type: StringName, action_kind: StringName, reason: String, splash_ratio: float = 0.5) -> void:
	if target.is_empty() or radius <= 0.0:
		return
	for unit in _units:
		if not bool(unit.get("alive", false)):
			continue
		if StringName(String(unit.get("team", ""))) == StringName(String(source.get("team", ""))):
			continue
		if int(unit.get("instance_id", 0)) == int(target.get("instance_id", 0)):
			continue
		if _distance_between_dicts(target, unit) <= radius:
			var splash_damage: float = damage * splash_ratio
			var context: Dictionary = _build_context(source, unit, {}, splash_damage, action_kind)
			_apply_damage(source, unit, splash_damage, damage_type, action_kind, context)

func _apply_aoe_taunt(source: Dictionary, radius: float, duration: float) -> void:
	for unit in _units:
		if not bool(unit.get("alive", false)):
			continue
		if StringName(String(unit.get("team", ""))) == StringName(String(source.get("team", ""))):
			continue
		if _distance_between_dicts(source, unit) <= radius:
			unit["taunt_target_id"] = int(source.get("instance_id", 0))
			unit["taunt_remaining"] = maxf(float(unit.get("taunt_remaining", 0.0)), duration)

func _apply_aoe_damage(source: Dictionary, center_source: Dictionary, damage: float, radius: float, damage_type: StringName, reason: String, action_kind: StringName) -> float:
	var total_damage: float = 0.0
	for unit in _units:
		if not bool(unit.get("alive", false)):
			continue
		if StringName(String(unit.get("team", ""))) == StringName(String(source.get("team", ""))):
			continue
		if _distance_between_dicts(center_source, unit) <= radius:
			var context: Dictionary = _build_context(source, unit, {}, damage, action_kind)
			var dealt: float = _apply_damage(source, unit, damage, damage_type, action_kind, context)
			total_damage += dealt
	return total_damage

func _select_enemy_target(unit: Dictionary) -> Dictionary:
	var taunt_id: int = int(unit.get("taunt_target_id", 0))
	if taunt_id != 0 and float(unit.get("taunt_remaining", 0.0)) > 0.0:
		var taunt_target: Dictionary = _unit_by_id(taunt_id)
		if not taunt_target.is_empty() and bool(taunt_target.get("alive", false)):
			return taunt_target

	var strategy: Dictionary = _strategy_for_unit(unit)
	var current_target: Dictionary = _unit_by_id(int(unit.get("target_id", 0)))
	if not current_target.is_empty() and bool(current_target.get("alive", false)) and StringName(String(current_target.get("team", ""))) != StringName(String(unit.get("team", ""))):
		if float(unit.get("target_switch_lock_timer", 0.0)) > 0.0:
			return current_target
		if float(unit.get("retarget_timer", 0.0)) > 0.0:
			return current_target
	unit["retarget_timer"] = SimConstantsScript.RETARGET_INTERVAL

	var best: Dictionary = {}
	var best_score: float = INF
	for candidate in _units:
		if not bool(candidate.get("alive", false)):
			continue
		if StringName(String(candidate.get("team", ""))) == StringName(String(unit.get("team", ""))):
			continue
		var candidate_score: float = _score_enemy_target(unit, candidate, strategy)
		if candidate_score < best_score - SimConstantsScript.EPSILON:
			best_score = candidate_score
			best = candidate
		elif is_equal_approx(candidate_score, best_score) and not best.is_empty():
			if int(candidate.get("instance_id", 0)) < int(best.get("instance_id", 0)):
				best = candidate
	if not current_target.is_empty() and bool(current_target.get("alive", false)) and StringName(String(current_target.get("team", ""))) != StringName(String(unit.get("team", ""))):
		var current_score: float = _score_enemy_target(unit, current_target, strategy)
		if best.is_empty() or current_target.get("instance_id", 0) == best.get("instance_id", 0):
			unit["current_target_score"] = current_score
			return current_target
		if not _should_switch(unit, current_score, best_score, strategy):
			unit["current_target_score"] = current_score
			return current_target
		unit["target_switch_lock_timer"] = SimConstantsScript.TARGET_SWITCH_LOCK_DURATION
		unit["retarget_timer"] = SimConstantsScript.RETARGET_INTERVAL
	return best

func _select_ally_target(unit: Dictionary) -> Dictionary:
	var alive_allies: Array = []
	for candidate in _units:
		if not bool(candidate.get("alive", false)):
			continue
		if StringName(String(candidate.get("team", ""))) != StringName(String(unit.get("team", ""))):
			continue
		alive_allies.append(candidate)
	if alive_allies.is_empty():
		return {}

	var critical_allies: Array = []
	for candidate in alive_allies:
		var candidate_stats: Dictionary = Dictionary(candidate.get("stats", {}))
		var candidate_hp_ratio: float = float(candidate.get("hp", 0.0)) / maxf(0.0001, float(candidate_stats.get("max_hp", 1.0)))
		if candidate_hp_ratio <= SimConstantsScript.ALLY_CRITICAL_HP_RATIO:
			critical_allies.append(candidate)

	var pool: Array = critical_allies if not critical_allies.is_empty() else alive_allies
	var strategy: Dictionary = _strategy_for_unit(unit)
	var best: Dictionary = {}
	var best_score: float = INF
	for candidate in pool:
		var score: float = _score_ally_target(unit, candidate, strategy)
		if best.is_empty() or score < best_score - SimConstantsScript.EPSILON or (is_equal_approx(score, best_score) and int(candidate.get("instance_id", 0)) < int(best.get("instance_id", 0))):
			best_score = score
			best = candidate
	return best

func _score_ally_target(unit: Dictionary, ally: Dictionary, strategy: Dictionary) -> float:
	var stats: Dictionary = Dictionary(ally.get("stats", {}))
	var dist: float = _distance_between(unit, ally)
	var hp_ratio: float = float(ally.get("hp", 0.0)) / maxf(0.0001, float(stats.get("max_hp", 1.0)))
	var score: float = dist * float(strategy.get("ally_distance_weight", 1.0))
	score += hp_ratio * float(strategy.get("ally_hp_weight", 0.0)) * SimConstantsScript.SCORE_HP_WEIGHT_SCALE
	score -= float(ally.get("perceived_threat", 0.0)) * float(strategy.get("ally_threat_weight", 0.0))
	score += float(strategy.get("ally_role_priorities", {}).get(StringName(String(stats.get("role", ""))), 0.0))
	return score

func _strategy_for_unit(unit: Dictionary) -> Dictionary:
	var role: StringName = StringName(String(Dictionary(unit.get("stats", {})).get("role", "")))
	match role:
		&"tank":
			return {
				"name": "Protector",
				"distance_weight": SimConstantsScript.DISTANCE_WEIGHT_TANK,
				"hp_weight": 0.0,
				"ally_distance_weight": 1.0,
				"ally_hp_weight": 0.0,
				"ally_threat_weight": SimConstantsScript.SCORE_THREAT_WEIGHT_SCALE * SimConstantsScript.SCORE_THREAT_WEIGHT_SCALE,
				"ally_role_priorities": _scale_priority_dict(SimConstantsScript.ALLY_ROLE_PRIORITIES_TANK),
				"stickiness_bonus": SimConstantsScript.STICKINESS_DEFAULT,
				"in_range_bonus": SimConstantsScript.IN_RANGE_BONUS_TANK,
				"tank_penalty": SimConstantsScript.TANK_PENALTY_TANK,
				"threat_response_weight": SimConstantsScript.THREAT_RESPONSE_TANK,
				"execute_bonus_weight": 0.0,
				"role_priorities": _scale_priority_dict(SimConstantsScript.ROLE_PRIORITIES_TANK),
			}
		&"assassin":
			return {
				"name": "Diver",
				"distance_weight": SimConstantsScript.DISTANCE_WEIGHT_ASSASSIN,
				"hp_weight": SimConstantsScript.HP_WEIGHT_ASSASSIN,
				"stickiness_bonus": SimConstantsScript.STICKINESS_DEFAULT,
				"in_range_bonus": SimConstantsScript.IN_RANGE_BONUS_ASSASSIN,
				"tank_penalty": SimConstantsScript.TANK_PENALTY_ASSASSIN,
				"threat_response_weight": SimConstantsScript.THREAT_RESPONSE_ASSASSIN,
				"execute_bonus_weight": SimConstantsScript.EXECUTE_BONUS_WEIGHT_DEFAULT,
				"role_priorities": _scale_priority_dict(SimConstantsScript.ROLE_PRIORITIES_ASSASSIN),
			}
		&"marksman":
			return {
				"name": "Kiter",
				"distance_weight": SimConstantsScript.DISTANCE_WEIGHT_DEFAULT,
				"hp_weight": SimConstantsScript.HP_WEIGHT_DPS_DEFAULT,
				"stickiness_bonus": SimConstantsScript.STICKINESS_MARKSMAN,
				"in_range_bonus": SimConstantsScript.IN_RANGE_BONUS_MARKSMAN,
				"tank_penalty": SimConstantsScript.TANK_PENALTY_MARKSMAN,
				"threat_response_weight": SimConstantsScript.THREAT_RESPONSE_MARKSMAN,
				"execute_bonus_weight": SimConstantsScript.EXECUTE_BONUS_WEIGHT_DEFAULT,
				"role_priorities": {},
			}
		&"fighter":
			return {
				"name": "Brawler",
				"distance_weight": SimConstantsScript.DISTANCE_WEIGHT_FIGHTER_CLOSE,
				"hp_weight": SimConstantsScript.HP_WEIGHT_DPS_DEFAULT,
				"stickiness_bonus": SimConstantsScript.STICKINESS_DEFAULT,
				"in_range_bonus": SimConstantsScript.IN_RANGE_BONUS_FIGHTER,
				"tank_penalty": SimConstantsScript.TANK_PENALTY_FIGHTER,
				"threat_response_weight": SimConstantsScript.THREAT_RESPONSE_FIGHTER,
				"execute_bonus_weight": SimConstantsScript.EXECUTE_BONUS_WEIGHT_DEFAULT,
				"role_priorities": _scale_priority_dict(SimConstantsScript.ROLE_PRIORITIES_FIGHTER),
			}
		&"mage":
			return {
				"name": "Spellcaster",
				"distance_weight": SimConstantsScript.DISTANCE_WEIGHT_MAGE,
				"hp_weight": SimConstantsScript.HP_WEIGHT_MAGE,
				"stickiness_bonus": SimConstantsScript.STICKINESS_DEFAULT,
				"in_range_bonus": SimConstantsScript.IN_RANGE_BONUS_MAGE,
				"tank_penalty": SimConstantsScript.TANK_PENALTY_MAGE,
				"threat_response_weight": SimConstantsScript.THREAT_RESPONSE_MAGE,
				"execute_bonus_weight": SimConstantsScript.EXECUTE_BONUS_WEIGHT_DEFAULT,
				"role_priorities": _scale_priority_dict(SimConstantsScript.ROLE_PRIORITIES_MAGE),
			}
		&"support":
			return {
				"name": "Enchanter",
				"distance_weight": SimConstantsScript.DISTANCE_WEIGHT_SUPPORT,
				"hp_weight": 0.0,
				"ally_distance_weight": 1.0,
				"ally_hp_weight": SimConstantsScript.HP_WEIGHT_SUPPORT,
				"ally_threat_weight": SimConstantsScript.SCORE_THREAT_WEIGHT_SCALE * SimConstantsScript.SCORE_THREAT_WEIGHT_SCALE,
				"ally_role_priorities": _scale_priority_dict(SimConstantsScript.ALLY_ROLE_PRIORITIES_SUPPORT),
				"stickiness_bonus": SimConstantsScript.STICKINESS_SUPPORT,
				"in_range_bonus": SimConstantsScript.IN_RANGE_BONUS_SUPPORT,
				"tank_penalty": SimConstantsScript.TANK_PENALTY_SUPPORT,
				"threat_response_weight": SimConstantsScript.THREAT_RESPONSE_SUPPORT,
				"execute_bonus_weight": 0.0,
				"role_priorities": _scale_priority_dict(SimConstantsScript.ROLE_PRIORITIES_SUPPORT),
			}
		_:
			return {"name": "Default", "distance_weight": 1.0, "hp_weight": 0.0, "ally_distance_weight": 1.0, "ally_hp_weight": 0.0, "ally_threat_weight": 0.0, "ally_role_priorities": {}, "stickiness_bonus": 2.0, "in_range_bonus": 0.6, "tank_penalty": 2.0, "threat_response_weight": 0.0, "execute_bonus_weight": 0.0, "role_priorities": {}}

func _score_enemy_target(attacker: Dictionary, enemy: Dictionary, strategy: Dictionary) -> float:
	var dist: float = _distance_between(attacker, enemy)
	var attack_range: float = _attack_range(attacker)
	var effective_range: float = _effective_attack_range(attacker)
	var in_range: bool = SimConstantsScript.is_melee_in_contact(dist, attack_range)
	var hp_ratio: float = float(enemy.get("hp", 0.0)) / maxf(0.0001, float(Dictionary(enemy.get("stats", {})).get("max_hp", 1.0)))
	var score: float = 0.0
	var range_gap: float = maxf(0.0, dist - maxf(effective_range, SimConstantsScript.EPSILON))
	var norm_gap: float = range_gap / maxf(effective_range, SimConstantsScript.EPSILON)
	score += pow(norm_gap, SimConstantsScript.DISTANCE_EXPONENT) * float(strategy.get("distance_weight", 1.0)) * SimConstantsScript.SCORE_DISTANCE_WEIGHT_SCALE
	if in_range:
		score -= float(strategy.get("in_range_bonus", 0.0))
	score += hp_ratio * float(strategy.get("hp_weight", 0.0)) * SimConstantsScript.SCORE_HP_WEIGHT_SCALE
	if float(strategy.get("execute_bonus_weight", 0.0)) > 0.0 and in_range and hp_ratio <= SimConstantsScript.TARGET_EXECUTE_HP_RATIO:
		score -= float(strategy.get("execute_bonus_weight", 0.0))
	score += float(strategy.get("role_priorities", {}).get(StringName(String(Dictionary(enemy.get("stats", {})).get("role", ""))), 0.0))
	if StringName(String(Dictionary(enemy.get("stats", {})).get("role", ""))) == &"tank":
		score += float(strategy.get("tank_penalty", 0.0))
	if int(enemy.get("target_id", 0)) == int(attacker.get("instance_id", 0)):
		var falloff: float = 1.0 / (1.0 + maxf(0.0, dist - attack_range) * SimConstantsScript.THREAT_RESPONSE_RANGE_FALLOFF)
		score -= float(strategy.get("threat_response_weight", 0.0)) * falloff
	var enemy_incoming: float = float(enemy.get("incoming_target_count", 0))
	var prey_focus: float = enemy_incoming * SimConstantsScript.PREY_INCOMING_TARGET_SCALE + float(enemy.get("perceived_threat", 0.0)) * SimConstantsScript.PREY_PERCEIVED_THREAT_SCALE
	if StringName(String(Dictionary(enemy.get("stats", {})).get("role", ""))) in [&"tank", &"fighter"]:
		prey_focus *= SimConstantsScript.PREY_FRONTLINE_SCALE
	if float(strategy.get("execute_bonus_weight", 0.0)) > 0.0:
		score -= prey_focus * 0.0
	if int(attacker.get("target_id", 0)) == int(enemy.get("instance_id", 0)):
		score -= maxf(1.0, float(strategy.get("distance_weight", 1.0))) * float(strategy.get("stickiness_bonus", 0.0))
	return score

func _should_switch(unit: Dictionary, current_score: float, new_score: float, strategy: Dictionary) -> bool:
	if float(unit.get("target_switch_lock_timer", 0.0)) > 0.0:
		return false
	var margin: float = float(strategy.get("switch_margin", SimConstantsScript.TARGET_SWITCH_MARGIN))
	return new_score <= (current_score - margin)

func _distance_between(left: Dictionary, right: Dictionary) -> float:
	return _distance_between_dicts(left, right)

func _distance_between_dicts(left: Dictionary, right: Dictionary) -> float:
	var left_position: Vector2 = Vector2(float(left.get("x", 0.0)), float(left.get("y", 0.0)))
	var right_position: Vector2 = Vector2(float(right.get("x", 0.0)), float(right.get("y", 0.0)))
	return left_position.distance_to(right_position)

func _attack_range(unit: Dictionary) -> float:
	return float(Dictionary(unit.get("stats", {})).get("attack_range", 0.0))

func _effective_attack_range(unit: Dictionary) -> float:
	return float(SimConstantsScript.effective_attack_range(_attack_range(unit)))

func _unit_by_id(instance_id: int) -> Dictionary:
	for unit in _units:
		if int(unit.get("instance_id", 0)) == instance_id:
			return unit
	return {}

func _handle_death(killer: Dictionary, target: Dictionary) -> void:
	if not bool(target.get("alive", false)):
		return
	target["alive"] = false
	target["respawn_timer"] = float(Dictionary(target.get("stats", {})).get("respawn_time", SimConstantsScript.RESPAWN_TIME))
	target["deaths"] = int(target.get("deaths", 0)) + 1

	var damage_sources: Dictionary = Dictionary(target.get("damage_sources", {}))
	var killer_id: int = 0
	var killer_damage: float = -1.0
	for key in damage_sources.keys():
		var source_id: int = int(key)
		var entry: Dictionary = Dictionary(damage_sources[key])
		var dealt: float = float(entry.get("damage", 0.0))
		var hit_time: float = float(entry.get("time", 0.0))
		if _time - hit_time > SimConstantsScript.ASSIST_WINDOW:
			continue
		if dealt > killer_damage or (is_equal_approx(dealt, killer_damage) and source_id < killer_id):
			killer_damage = dealt
			killer_id = source_id

	var killer_unit: Dictionary = _unit_by_id(killer_id)
	if not killer_unit.is_empty():
		killer_unit["kills"] = int(killer_unit.get("kills", 0)) + 1
		if StringName(String(killer_unit.get("team", ""))) == TEAM_PLAYER:
			_player_kills += 1
		elif StringName(String(killer_unit.get("team", ""))) == TEAM_ENEMY:
			_enemy_kills += 1

	for key in damage_sources.keys():
		var source_id: int = int(key)
		if source_id == killer_id:
			continue
		var entry: Dictionary = Dictionary(damage_sources[key])
		if _time - float(entry.get("time", 0.0)) <= SimConstantsScript.ASSIST_WINDOW:
			var assist_unit: Dictionary = _unit_by_id(source_id)
			if not assist_unit.is_empty():
				assist_unit["assists"] = int(assist_unit.get("assists", 0)) + 1

func _determine_winner() -> StringName:
	if _player_kills > _enemy_kills:
		return TEAM_PLAYER
	if _enemy_kills > _player_kills:
		return TEAM_ENEMY
	return &"draw"

func _respawn_unit(unit: Dictionary) -> void:
	if bool(unit.get("alive", true)):
		return
	unit["alive"] = true
	unit["hp"] = float(Dictionary(unit.get("stats", {})).get("max_hp", 0.0))
	unit["mana"] = 0.0
	unit["shield"] = 0.0
	unit["ability_cooldown"] = float(Dictionary(unit.get("stats", {})).get("ability_cd", 0.0))
	unit["stun_remaining"] = 0.0
	unit["taunt_remaining"] = 0.0
	unit["respawn_timer"] = 0.0
	unit["damage_sources"] = {}
	unit["last_hit_time"] = 0.0
	unit["respawned_this_tick"] = true
	unit["casting_remaining"] = 0.0
	unit["casting_kind"] = &""
	unit["casting_effect"] = null
	unit["casting_target_id"] = 0
	unit["casting_ally_target_id"] = 0
	unit["taunt_target_id"] = 0
	if StringName(String(unit.get("team", ""))) == TEAM_PLAYER:
		unit["x"] = float(SimConstantsScript.DRAFT_X_BASE)
		unit["y"] = float(unit.get("spawn_y", unit.get("y", 0.0)))
	else:
		unit["x"] = float(SimConstantsScript.WORLD_SIZE - SimConstantsScript.DRAFT_X_BASE)
		unit["y"] = float(unit.get("spawn_y", unit.get("y", 0.0)))

func _build_summary():
	var summary := MatchReplaySummaryScript.new()
	summary.seed = _seed
	summary.winner_team = _winner_team
	summary.duration = _time
	summary.player_comp = _player_comp.duplicate()
	summary.enemy_comp = _enemy_comp.duplicate()
	for unit in _units:
		var unit_summary := UnitReplaySummaryScript.new()
		unit_summary.instance_id = int(unit.get("instance_id", 0))
		unit_summary.archetype_id = StringName(String(unit.get("archetype_id", "")))
		unit_summary.team = StringName(String(unit.get("team", "")))
		unit_summary.won = _winner_team != &"" and unit_summary.team == _winner_team
		unit_summary.damage_dealt = float(unit.get("damage_dealt", 0.0))
		unit_summary.damage_dealt_auto = float(unit.get("damage_dealt_auto", 0.0))
		unit_summary.damage_dealt_ability = float(unit.get("damage_dealt_ability", 0.0))
		unit_summary.damage_dealt_ultimate = float(unit.get("damage_dealt_ultimate", 0.0))
		unit_summary.damage_dealt_passive = float(unit.get("damage_dealt_passive", 0.0))
		unit_summary.damage_received = float(unit.get("damage_received", 0.0))
		unit_summary.damage_mitigated = float(unit.get("damage_mitigated", 0.0))
		unit_summary.healing_done = float(unit.get("healing_done", 0.0))
		unit_summary.healing_done_auto = float(unit.get("healing_done_auto", 0.0))
		unit_summary.healing_done_ability = float(unit.get("healing_done_ability", 0.0))
		unit_summary.healing_done_ultimate = float(unit.get("healing_done_ultimate", 0.0))
		unit_summary.healing_done_passive = float(unit.get("healing_done_passive", 0.0))
		unit_summary.shielding_done = float(unit.get("shielding_done", 0.0))
		unit_summary.shielding_done_auto = float(unit.get("shielding_done_auto", 0.0))
		unit_summary.shielding_done_ability = float(unit.get("shielding_done_ability", 0.0))
		unit_summary.shielding_done_ultimate = float(unit.get("shielding_done_ultimate", 0.0))
		unit_summary.shielding_done_passive = float(unit.get("shielding_done_passive", 0.0))
		unit_summary.auto_attacks = int(unit.get("auto_attacks", 0))
		unit_summary.abilities = int(unit.get("abilities", 0))
		unit_summary.ultimates = int(unit.get("ultimates", 0))
		unit_summary.stuns = int(unit.get("stuns", 0))
		unit_summary.kills = int(unit.get("kills", 0))
		unit_summary.deaths = int(unit.get("deaths", 0))
		unit_summary.assists = int(unit.get("assists", 0))
		summary.unit_stats.append(unit_summary)
	return summary

func _effect_to_dict(effect: Variant) -> Dictionary:
	if effect is Dictionary:
		return Dictionary(effect)
	if effect is Object and effect.has_method("to_dict"):
		var converted: Variant = effect.call("to_dict")
		if converted is Dictionary:
			return Dictionary(converted)
	return {}
