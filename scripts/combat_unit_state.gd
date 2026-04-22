extends RefCounted
class_name CombatUnitState

const CombatData = preload("res://scripts/combat_data.gd")
const CombatEffectContextScript = preload("res://scripts/combat_effect_context.gd")
const CombatEffectResult = preload("res://scripts/combat_effect_result.gd")
const CombatWorld = preload("res://scripts/combat_world.gd")

enum UnitState { MOVING, ATTACKING, CASTING, WAITING, KITING, STUNNED, DEAD }

var hero_id: String = ""
var display_name: String = ""
var role: String = ""
var team: String = ""
var color: Color = Color.WHITE
var passive_id: String = ""

var spawn_pos: Vector2 = Vector2.ZERO
var global_position: Vector2 = Vector2.ZERO
var velocity: Vector2 = Vector2.ZERO
var visible: bool = true
var draggable: bool = false
var selected: bool = false
var hovered: bool = false
var dragging: bool = false

var max_hp: float = 1.0
var hp: float = 1.0
var shield: float = 0.0
var magic_resist: float = 0.0
var armor: float = 0.0
var tenacity: float = 0.0
var life_steal: float = 0.0
var max_mana: float = 0.0
var mana_per_attack: float = 0.0
var ability_cd: float = 0.0
var ultimate_cd: float = 0.0
var respawn_time: float = CombatData.RESPAWN_TIME

var attack_damage: float = 0.0
var attack_range: float = 0.3
var attack_speed: float = 1.0
var move_speed: float = 1.0
var projectile_speed: float = CombatData.DEFAULT_PROJECTILE_SPEED
var projectile_radius: float = CombatData.DEFAULT_PROJECTILE_RADIUS
var combat_scale: float = 1.0
var combat_ranged_threshold: float = CombatData.RANGED_THRESHOLD
var combat_contact_buffer: float = CombatData.MELEE_CONTACT_BUFFER
var combat_bounds_min: Vector2 = Vector2.ZERO
var combat_bounds_max: Vector2 = Vector2.ZERO

var alive: bool = true
var current_state: int = UnitState.WAITING
var attack_cooldown_remaining: float = 0.0
var casting_remaining: float = 0.0
var _is_casting_ult: bool = false
var ability_timer: float = 0.0
var ultimate_timer: float = 0.0
var stun_timer: float = 0.0
var respawn_timer: float = 0.0
var retarget_timer: float = 0.0
var last_kite_timer: float = 0.0
var target_switch_lock_timer: float = 0.0

var taunt_timer: float = 0.0
var taunted_by: int = -1
var forced_target_timer: float = 0.0
var forced_target_id: int = -1
var forced_target_source_id: int = -1
var forced_target_kind: String = ""

var current_target: Object = null
var current_ally_target: Object = null
var instance_id: int = 0
var target_id: int = -1
var distance_to_target: float = -1.0
var in_range: bool = false
var last_target_reason: String = ""
var last_target_score: float = 0.0
var retargeted_this_tick: bool = false
var retarget_reason_this_tick: String = ""

var incoming_target_count: int = 0
var perceived_threat: float = 0.0
var damage_dealt: float = 0.0
var damage_dealt_auto: float = 0.0
var damage_dealt_ability: float = 0.0
var damage_dealt_ultimate: float = 0.0
var damage_received: float = 0.0
var damage_mitigated: float = 0.0
var healing_done: float = 0.0
var shielding_done: float = 0.0
var auto_attacks_done: int = 0
var abilities_used: int = 0
var ultimates_used: int = 0
var stuns_dealt: int = 0
var kills: int = 0
var deaths: int = 0
var assists: int = 0
var mana: float = 0.0
var attack_count: int = 0
var recent_damage_sources: Dictionary = {}
var recent_benefactors: Dictionary = {}
var _regen_accumulator: float = 0.0
var _cached_on_attack: Array = []
var _cached_on_defense: Array = []
var _cached_on_tick: Array = []
var _cached_post_attack: Array = []
var _cached_post_take_damage: Array = []
var _ability_effect = null
var _ultimate_effect = null
var _combat_world: Object = null
var _combat_registry = null
var _combat_metrics: Object = null
var _combat_ready: bool = true


func set_identity(new_hero_id: String, new_display_name: String, new_role: String, new_team: String, new_color: Color, new_draggable: bool, new_passive_id: String = "") -> void:
	hero_id = new_hero_id
	display_name = new_display_name
	role = new_role
	team = new_team
	color = new_color
	draggable = new_draggable
	passive_id = new_passive_id
	_refresh_effect_cache()


func set_combat_stats(
		new_max_hp: float,
		new_attack_damage: float,
		new_attack_range: float,
		new_attack_speed: float,
		new_move_speed: float,
		new_armor: float,
		new_projectile_speed: float,
		new_magic_resist: float = 0.0,
		new_tenacity: float = 0.0,
		new_life_steal: float = 0.0,
		new_respawn_time: float = CombatData.RESPAWN_TIME,
		new_mana_per_attack: float = 0.0,
		new_max_mana: float = 0.0,
		new_ability_cd: float = 0.0,
		new_ultimate_cd: float = 0.0,
		new_projectile_radius: float = CombatData.DEFAULT_PROJECTILE_RADIUS
	) -> void:
	max_hp = new_max_hp
	hp = new_max_hp
	shield = 0.0
	attack_damage = new_attack_damage
	attack_range = new_attack_range
	attack_speed = new_attack_speed
	move_speed = new_move_speed
	armor = new_armor
	projectile_speed = new_projectile_speed
	magic_resist = new_magic_resist
	tenacity = new_tenacity
	life_steal = new_life_steal
	respawn_time = new_respawn_time
	mana_per_attack = new_mana_per_attack
	max_mana = new_max_mana
	ability_cd = new_ability_cd
	ultimate_cd = new_ultimate_cd
	projectile_radius = new_projectile_radius
	mana = 0.0
	attack_count = 0
	alive = true
	current_state = UnitState.WAITING
	attack_cooldown_remaining = 0.0
	casting_remaining = 0.0
	_is_casting_ult = false
	ability_timer = ability_cd
	ultimate_timer = ultimate_cd
	respawn_timer = 0.0
	velocity = Vector2.ZERO


func set_spawn_position(pos: Vector2) -> void:
	spawn_pos = pos


func set_world_position(pos: Vector2) -> void:
	global_position = pos


func set_combat_world(world: Object) -> void:
	_combat_world = world


func set_mana(value: float) -> void:
	mana = value


func set_attack_count(value: int) -> void:
	attack_count = value


func add_damage_dealt(amount: float) -> void:
	damage_dealt += amount


func apply_combat_metrics(_metrics: Object) -> void:
	if _metrics == null:
		return
	_combat_metrics = _metrics
	_combat_ready = true
	move_speed = float(_metrics.call("scale_length", move_speed))
	attack_range = float(_metrics.call("scale_length", attack_range))
	projectile_speed = float(_metrics.call("scale_length", projectile_speed))
	projectile_radius = float(_metrics.call("scale_length", projectile_radius))
	combat_scale = float(_metrics.call("scale_length", 1.0))
	combat_ranged_threshold = float(_metrics.call("scale_length", CombatData.RANGED_THRESHOLD))
	combat_contact_buffer = float(_metrics.call("scale_length", CombatData.MELEE_CONTACT_BUFFER))
	var rect: Rect2 = _metrics.call("get_rect")
	combat_bounds_min = rect.position
	combat_bounds_max = rect.position + rect.size


func set_collision_enabled(_enabled: bool) -> void:
	pass


func set_combat_registry(registry: Object) -> void:
	_combat_registry = registry
	_refresh_effect_cache()


func set_focus(is_selected: bool, is_hovered: bool, is_dragging: bool) -> void:
	selected = is_selected
	hovered = is_hovered
	dragging = is_dragging


func step_motion(dt: float) -> void:
	if not _combat_ready or velocity == Vector2.ZERO or not is_alive() or current_state == UnitState.DEAD:
		return
	var previous_position := global_position
	global_position += velocity * dt
	if _combat_world != null:
		_combat_world.call("notify_unit_moved", self, previous_position)


func _refresh_effect_cache() -> void:
	if _combat_registry == null:
		_ability_effect = null
		_ultimate_effect = null
		_cached_on_attack = []
		_cached_on_defense = []
		_cached_on_tick = []
		_cached_post_attack = []
		_cached_post_take_damage = []
		return

	var keys: Array[String] = [hero_id, passive_id, role]
	_ability_effect = _combat_registry.call("get_ability", keys)
	_ultimate_effect = _combat_registry.call("get_ultimate", keys)
	_cached_on_attack = _combat_registry.call("get_passives", "on_attack", keys)
	_cached_on_defense = _combat_registry.call("get_passives", "on_defense", keys)
	_cached_on_tick = _combat_registry.call("get_passives", "on_tick", keys)
	_cached_post_attack = _combat_registry.call("get_passives", "post_attack", keys)
	_cached_post_take_damage = _combat_registry.call("get_passives", "post_take_damage", keys)


func is_alive() -> bool:
	return alive and hp > 0.0


func get_state_label() -> String:
	match current_state:
		UnitState.MOVING:
			return "MOVING"
		UnitState.ATTACKING:
			return "ATTACKING"
		UnitState.CASTING:
			return "CASTING"
		UnitState.WAITING:
			if current_target != null and in_range:
				return "ATTACKING"
			return "MOVING"
		UnitState.KITING:
			return "KITING"
		UnitState.STUNNED:
			return "STUNNED"
		UnitState.DEAD:
			return "DEAD"
		_:
			return "MOVING"


func get_forced_target_id() -> int:
	if forced_target_timer > 0.0 and forced_target_id >= 0:
		return forced_target_id
	return -1


func get_forced_target_source_id() -> int:
	if forced_target_timer > 0.0 and forced_target_source_id >= 0:
		return forced_target_source_id
	return -1


func apply_taunt(duration: float, source_id: int) -> void:
	forced_target_timer = maxf(forced_target_timer, duration)
	forced_target_id = source_id
	forced_target_source_id = source_id
	forced_target_kind = "taunt"
	taunt_timer = forced_target_timer
	taunted_by = source_id


func apply_forced_target(duration: float, source_id: int, target_id: int = -1, kind: String = "taunt") -> void:
	forced_target_timer = maxf(forced_target_timer, duration)
	forced_target_id = target_id if target_id >= 0 else source_id
	forced_target_source_id = source_id
	forced_target_kind = kind
	if kind == "taunt":
		taunt_timer = forced_target_timer
		taunted_by = source_id


func apply_stun(duration: float, world: Object, source_id: int = -1) -> void:
	var final_duration := maxf(0.0, duration * (1.0 - tenacity))
	if final_duration <= 0.0:
		return
	stun_timer = maxf(stun_timer, final_duration)
	current_state = UnitState.STUNNED
	if source_id >= 0 and world != null:
		var attacker: Object = world.call("get_unit_by_id", source_id)
		if attacker != null:
			attacker.set("stuns_dealt", int(attacker.get("stuns_dealt")) + 1)


func add_shield(amount: float, world: Object, source_id: int = -1) -> void:
	if not is_alive() or amount <= 0.0:
		return
	shield += amount
	if source_id >= 0:
		shielding_done += amount
		recent_benefactors[source_id] = float(world.get("time"))


func heal(amount: float, world: Object, source_id: int = -1) -> void:
	if not is_alive() or amount <= 0.0:
		return
	hp = minf(max_hp, hp + amount)
	healing_done += amount
	if source_id >= 0 and source_id != instance_id:
		recent_benefactors[source_id] = float(world.get("time"))
	var combat_world := world as CombatWorld
	if combat_world == null:
		return
	combat_world.record_event({
		"timestamp": combat_world.time,
		"attacker_id": source_id if source_id >= 0 else instance_id,
		"attacker_name": display_name,
		"target_id": instance_id,
		"target_name": display_name,
		"healing": amount,
		"distance": 0.0,
		"target_hp_after": hp,
		"target_shield_after": shield,
		"target_selection_reason": "Healing",
	})


func take_damage(amount: float, world: Object, damage_type: String = "true", source_id: int = -1) -> Dictionary:
	var pre_res_damage := amount
	var final_damage := amount
	var defense_context := CombatEffectContextScript.new(self, world as CombatWorld, current_target, current_ally_target, amount)
	var defense_mult := 1.0
	for effect in _cached_on_defense:
		defense_mult *= float(effect.apply_on_defense(defense_context))
	final_damage *= defense_mult
	if damage_type == "physical":
		final_damage *= (1.0 - armor)
	elif damage_type == "magic":
		final_damage *= (1.0 - magic_resist)
	damage_mitigated += pre_res_damage - final_damage
	if source_id >= 0:
		recent_damage_sources[source_id] = float(world.get("time"))
	var absorbed := 0.0
	if shield > 0.0:
		absorbed = minf(shield, final_damage)
		shield -= absorbed
		final_damage -= absorbed
	damage_received += final_damage
	hp = maxf(0.0, hp - final_damage)
	defense_context.damage = absorbed + final_damage
	for effect in _cached_post_take_damage:
		effect.apply_post_take_damage(defense_context)
	if hp <= 0.0:
		alive = false
		current_state = UnitState.DEAD
		visible = false
		respawn_timer = respawn_time
		velocity = Vector2.ZERO
		current_target = null
		current_ally_target = null
		in_range = false
		distance_to_target = -1.0
		target_id = -1
		attack_cooldown_remaining = 0.0
	return {"absorbed": absorbed, "hp_loss": final_damage}


func update_cooldowns(dt: float) -> void:
	attack_cooldown_remaining = maxf(0.0, attack_cooldown_remaining - dt)
	ability_timer = maxf(0.0, ability_timer - dt)
	ultimate_timer = maxf(0.0, ultimate_timer - dt)
	retarget_timer = maxf(0.0, retarget_timer - dt)
	last_kite_timer = maxf(0.0, last_kite_timer - dt)
	target_switch_lock_timer = maxf(0.0, target_switch_lock_timer - dt)
	if forced_target_timer > 0.0:
		forced_target_timer = maxf(0.0, forced_target_timer - dt)
		if forced_target_timer <= 0.0:
			forced_target_id = -1
			forced_target_source_id = -1
			forced_target_kind = ""
			taunt_timer = 0.0
			taunted_by = -1
	if stun_timer > 0.0:
		stun_timer = maxf(0.0, stun_timer - dt)


func begin_cast(is_ult: bool) -> bool:
	if not is_alive():
		return false
	if is_ult:
		if max_mana <= 0.0 or mana < max_mana or ultimate_timer > 0.0:
			return false
		mana -= max_mana
		ultimate_timer = ultimate_cd
		ultimates_used += 1
	else:
		if ability_timer > 0.0:
			return false
		ability_timer = ability_cd
		abilities_used += 1
	casting_remaining = CombatData.CASTING_WINDUP
	_is_casting_ult = is_ult
	current_state = UnitState.CASTING
	return true


func update(dt: float, world: Object) -> void:
	if current_state == UnitState.DEAD:
		respawn_timer = maxf(0.0, respawn_timer - dt)
		if respawn_timer <= 0.0:
			respawn(world)
		return

	if not is_alive():
		current_state = UnitState.DEAD
		respawn_timer = respawn_time
		velocity = Vector2.ZERO
		current_target = null
		current_ally_target = null
		return

	update_cooldowns(dt)

	var cutoff := float(world.get("time")) - CombatData.ASSIST_WINDOW
	recent_damage_sources = _prune_recent(recent_damage_sources, cutoff)
	recent_benefactors = _prune_recent(recent_benefactors, cutoff)

	var targeting_system = world.call("get_targeting_system")
	var strategy: Dictionary = targeting_system.call("get_strategy_for", self)
	perceived_threat = maxf(0.0, perceived_threat - float(strategy.get("threat_decay_rate", CombatData.THREAT_DECAY_DEFAULT)) * dt)

	_regen_accumulator += dt
	if _regen_accumulator >= CombatData.REGEN_TICK_INTERVAL:
		_regen_accumulator -= CombatData.REGEN_TICK_INTERVAL
		var tick_context := CombatEffectContextScript.new(self, world as CombatWorld, current_target, current_ally_target)
		for effect in _cached_on_tick:
			effect.apply_on_tick(tick_context)

	if stun_timer > 0.0:
		current_state = UnitState.STUNNED
		velocity = Vector2.ZERO
		return

	if casting_remaining > 0.0:
		casting_remaining = maxf(0.0, casting_remaining - dt)
		velocity = Vector2.ZERO
		if casting_remaining <= 0.0:
			if _is_casting_ult:
				if not _execute_ultimate(world):
					mana = minf(max_mana, mana + max_mana)
					ultimate_timer = 0.0
			else:
				if not _execute_ability(world):
					ability_timer = 0.0
		current_state = UnitState.CASTING
		return

	var allies: Array = world.call("get_allies_for", self)
	var enemies: Array = world.call("get_enemies_for", self)
	var nearby_allies: Array = world.call("get_nearby_allies_for", self)
	var nearby_enemies: Array = world.call("get_nearby_enemies_for", self)
	var forced_target: CombatUnitState = world.get_unit_by_id(get_forced_target_id()) if get_forced_target_id() >= 0 else null

	var ally_candidates := _assemble_candidate_pool(nearby_allies, allies, [current_ally_target])
	if role == "support" or role == "tank":
		var ally_candidate: Object = targeting_system.call("select_ally", self, ally_candidates)
		current_ally_target = ally_candidate.target if ally_candidate != null else null
	else:
		current_ally_target = null

	var enemy_candidates := _assemble_candidate_pool(nearby_enemies, enemies, [current_target, forced_target])
	var kite_enemies := nearby_enemies if not nearby_enemies.is_empty() else enemy_candidates

	var candidate: Object = _retarget_if_needed(world, enemy_candidates, allies, targeting_system)
	if candidate == null:
		current_state = UnitState.WAITING
		velocity = Vector2.ZERO
		return

	var target: Object = candidate.target
	if target == null or not bool(target.call("is_alive")):
		current_state = UnitState.WAITING
		velocity = Vector2.ZERO
		_set_current_target(world, null)
		return

	_set_target_debug(target, float(candidate.distance), bool(candidate.in_range))

	if in_range:
		velocity = Vector2.ZERO
		if max_mana > 0.0 and ultimate_timer <= 0.0 and mana >= max_mana:
			if _begin_cast(world, true):
				current_state = UnitState.CASTING
				return
		if ability_timer <= 0.0:
			if _begin_cast(world, false):
				current_state = UnitState.CASTING
				return
		if attack_cooldown_remaining <= 0.0:
			_perform_auto_attack(world, String(candidate.reason))
			return

	if bool(strategy.get("prefers_kiting", false)) and attack_cooldown_remaining > 0.0 and taunted_by < 0:
		if _kite_from_enemies(world, kite_enemies, dt):
			current_state = UnitState.KITING
			return

	_move_toward(target, dt, world)
	current_state = UnitState.MOVING if not in_range else UnitState.WAITING


func _retarget_if_needed(world: Object, enemies: Array, allies: Array, targeting_system: Object) -> Object:
	var alive_enemies: Array = []
	for enemy in enemies:
		if bool(enemy.call("is_alive")):
			alive_enemies.append(enemy)
	if alive_enemies.is_empty():
		_set_current_target(world, null)
		last_target_reason = "no candidate found"
		last_target_score = 0.0
		return null

	if get_forced_target_id() >= 0:
		var forced_candidate: Object = targeting_system.call("select_target", self, alive_enemies, allies)
		if forced_candidate == null:
			_set_current_target(world, null)
			last_target_reason = "no candidate found"
			last_target_score = 0.0
			return null
		_set_current_target(world, forced_candidate.target)
		last_target_score = float(forced_candidate.score)
		last_target_reason = "forced target; %s" % String(forced_candidate.reason if forced_candidate.reason != "" else "target")
		retargeted_this_tick = true
		retarget_reason_this_tick = last_target_reason
		retarget_timer = 0.0
		return forced_candidate

	var frontline_roles: Array[String] = ["tank", "fighter"]
	var backline_roles: Array[String] = ["marksman", "mage", "support"]
	var assassin_pressuring_frontline: bool = (
		String(role) == "assassin"
		and current_target != null
		and bool(current_target.call("is_alive"))
		and String(current_target.get("role")) in frontline_roles
		and _has_alive_role(alive_enemies, backline_roles)
	)

	var strategy: Dictionary = targeting_system.call("get_strategy_for", self)
	var needs_evaluation: bool = current_target == null or not bool(current_target.call("is_alive")) or retarget_timer <= 0.0 or assassin_pressuring_frontline
	if not needs_evaluation:
		return targeting_system.call("score_target", self, current_target, strategy, allies, alive_enemies)

	retarget_timer = CombatData.RETARGET_INTERVAL
	var candidate: Object = targeting_system.call("select_target", self, alive_enemies, allies)
	if candidate == null:
		_set_current_target(world, null)
		last_target_reason = "no candidate found"
		last_target_score = 0.0
		return null

	if current_target == null or not bool(current_target.call("is_alive")):
		_set_current_target(world, candidate.target)
		last_target_score = float(candidate.score)
		last_target_reason = "periodic pick; %s" % String(candidate.reason if candidate.reason != "" else "target")
		retargeted_this_tick = true
		retarget_reason_this_tick = last_target_reason
		return candidate

	var current_candidate: Object = targeting_system.call("score_target", self, current_target, strategy, allies, alive_enemies, false)
	var candidate_target_state := candidate.target as CombatUnitState
	if candidate_target_state != null and int(candidate_target_state.instance_id) == int(current_target.get("instance_id")):
		last_target_score = float(current_candidate.score)
		last_target_reason = "kept target; %s" % String(current_candidate.reason if current_candidate.reason != "" else "target")
		return current_candidate

	var candidate_target: CombatUnitState = candidate.target as CombatUnitState
	if assassin_pressuring_frontline and candidate_target != null and candidate_target.role in backline_roles:
		_set_current_target(world, candidate.target)
		target_switch_lock_timer = 0.0
		last_target_score = float(candidate.score)
		last_target_reason = "assassin breaks frontline lock; %s" % String(candidate.reason if candidate.reason != "" else "target")
		retargeted_this_tick = true
		retarget_reason_this_tick = last_target_reason
		return candidate

	if targeting_system.call("should_switch", float(current_candidate.score), float(candidate.score), strategy, self):
		_set_current_target(world, candidate.target)
		target_switch_lock_timer = CombatData.TARGET_SWITCH_LOCK_DURATION
		last_target_score = float(candidate.score)
		last_target_reason = "switched; %s" % String(candidate.reason if candidate.reason != "" else "target")
		retargeted_this_tick = true
		retarget_reason_this_tick = last_target_reason
		return candidate

	last_target_score = float(current_candidate.score)
	last_target_reason = "kept target"
	return current_candidate


func _assemble_candidate_pool(primary: Array, fallback: Array, preserve: Array = []) -> Array:
	var pool: Array = primary if not primary.is_empty() else fallback
	var result: Array = pool.duplicate()
	for unit in preserve:
		var unit_state := unit as CombatUnitState
		if unit_state != null and unit_state.is_alive() and not _contains_unit_id(result, unit_state.instance_id):
			result.append(unit)
	return result


func _perform_auto_attack(world: Object, reason: String) -> void:
	current_state = UnitState.ATTACKING
	var combat_world := world as CombatWorld
	if combat_world == null or not is_alive() or current_target == null or not bool(current_target.call("is_alive")):
		velocity = Vector2.ZERO
		current_state = UnitState.WAITING
		return

	var attack_result: Dictionary = attack(current_target, world, reason)
	if max_mana > 0.0:
		mana = minf(max_mana, mana + mana_per_attack)
	_set_post_attack_cooldown()
	if not attack_result.is_empty():
		combat_world.record_event(attack_result)


func _begin_cast(world: Object, is_ult: bool) -> bool:
	if not is_alive():
		return false

	if is_ult:
		if max_mana <= 0.0 or mana < max_mana or ultimate_timer > 0.0:
			return false
		mana -= max_mana
		ultimate_timer = ultimate_cd
		ultimates_used += 1
	else:
		if ability_timer > 0.0:
			return false
		ability_timer = ability_cd
		abilities_used += 1

	casting_remaining = CombatData.CASTING_WINDUP
	_is_casting_ult = is_ult
	current_state = UnitState.CASTING
	return true


func _execute_ability(world: Object) -> bool:
	var effect = _ability_effect
	if effect == null:
		return false
	var combat_world := world as CombatWorld
	if combat_world == null:
		return false
	var context := CombatEffectContextScript.new(self, combat_world, current_target, current_ally_target)
	var result: CombatEffectResult = effect.execute_active(context)
	if result.is_empty():
		return false
	if result.use_projectile:
		if current_target == null or not bool(current_target.call("is_alive")):
			return false
		combat_world.spawn_projectile(self, current_target, result.damage, result.damage_type, String(result.reason if not result.reason.is_empty() else "Ability"), result.projectile_speed, result.projectile_radius, result.stun_duration, true, false, result.splash_radius, result.splash_ratio)
		return true
	var target: Object = result.event_target
	if target == null:
		target = current_target
	if result.healing > 0.0 or result.shield_added > 0.0:
		target = current_ally_target if current_ally_target != null else self
	if target != null:
		velocity = Vector2.ZERO
		var active_damage := result.damage
		if active_damage > 0.0:
			damage_dealt += active_damage
			if _is_casting_ult:
				damage_dealt_ultimate += active_damage
			else:
				damage_dealt_ability += active_damage
		_record_effect_event(combat_world, target, result, "Ability")
		return true
	return false


func _execute_ultimate(world: Object) -> bool:
	var effect = _ultimate_effect
	if effect == null:
		return false
	var combat_world := world as CombatWorld
	if combat_world == null:
		return false
	var context := CombatEffectContextScript.new(self, combat_world, current_target, current_ally_target)
	var result: CombatEffectResult = effect.execute_active(context)
	if result.is_empty():
		return false
	if result.use_projectile:
		if current_target == null or not bool(current_target.call("is_alive")):
			return false
		combat_world.spawn_projectile(self, current_target, result.damage, result.damage_type, String(result.reason if not result.reason.is_empty() else "Ultimate"), result.projectile_speed, result.projectile_radius, result.stun_duration, false, true, result.splash_radius, result.splash_ratio)
		return true
	var target: Object = result.event_target
	if target == null:
		target = current_target
	if result.healing > 0.0 or result.shield_added > 0.0:
		target = current_ally_target if current_ally_target != null else self
	if target != null:
		velocity = Vector2.ZERO
		var active_damage := result.damage
		if active_damage > 0.0:
			damage_dealt += active_damage
			if _is_casting_ult:
				damage_dealt_ultimate += active_damage
			else:
				damage_dealt_ability += active_damage
		_record_effect_event(combat_world, target, result, "Ultimate")
		return true
	return false


func attack(target: Object, world: Object, reason: String) -> Dictionary:
	var combat_world := world as CombatWorld
	var target_state := target as CombatUnitState
	if combat_world == null or not is_alive() or (target_state == null and not bool(target.call("is_alive"))) or (target_state != null and not target_state.is_alive()):
		return {}

	var damage_mult := 1.0
	var context := CombatEffectContextScript.new(self, combat_world, target, current_ally_target)
	for effect in _cached_on_attack:
		damage_mult *= float(effect.apply_on_attack(context))
	var damage := attack_damage * damage_mult
	var target_position: Vector2 = target_state.global_position if target_state != null else target.global_position
	var distance: float = global_position.distance_to(target_position)

	auto_attacks_done += 1
	if attack_range > combat_ranged_threshold:
		velocity = Vector2.ZERO
		combat_world.spawn_projectile(self, target, damage, "physical", reason, projectile_speed, projectile_radius, 0.0, false, false)
		return {}

	var damage_result: Dictionary = target_state.take_damage(damage, combat_world, "physical", instance_id) if target_state != null else target.call("take_damage", damage, world, "physical", instance_id)
	var absorbed := float(damage_result.get("absorbed", 0.0))
	var hp_loss := float(damage_result.get("hp_loss", 0.0))
	var total_dmg := absorbed + hp_loss
	damage_dealt_auto += total_dmg
	damage_dealt += total_dmg
	on_attack_hit(target, total_dmg, world, true)
	return {
		"timestamp": combat_world.time,
		"attacker_id": instance_id,
		"attacker_name": display_name,
		"target_id": target_state.instance_id if target_state != null else int(target.get("instance_id")),
		"target_name": target_state.display_name if target_state != null else String(target.get("display_name")),
		"damage": total_dmg,
		"shield_absorbed": absorbed,
		"distance": distance,
		"target_hp_after": target_state.hp if target_state != null else float(target.get("hp")),
		"target_shield_after": target_state.shield if target_state != null else float(target.get("shield")),
		"target_selection_reason": reason,
	}


func on_attack_hit(target: Object, damage: float, world: Object, is_auto: bool = true) -> void:
	var context := CombatEffectContextScript.new(self, world as CombatWorld, target, current_ally_target, damage)
	for effect in _cached_post_attack:
		effect.apply_post_attack(context)
	if is_auto and life_steal > 0.0 and damage > 0.0:
		heal(damage * life_steal, world, instance_id)


func respawn(world: Object = null) -> void:
	hp = max_hp
	shield = 0.0
	mana = 0.0
	attack_count = 0
	perceived_threat = 0.0
	current_state = UnitState.WAITING
	alive = true
	velocity = Vector2.ZERO
	attack_cooldown_remaining = 0.0
	casting_remaining = 0.0
	_is_casting_ult = false
	ability_timer = ability_cd
	ultimate_timer = ultimate_cd
	stun_timer = 0.0
	respawn_timer = 0.0
	retarget_timer = 0.0
	last_kite_timer = 0.0
	target_switch_lock_timer = 0.0
	taunt_timer = 0.0
	taunted_by = -1
	forced_target_timer = 0.0
	forced_target_id = -1
	forced_target_source_id = -1
	forced_target_kind = ""
	current_target = null
	current_ally_target = null
	target_id = -1
	distance_to_target = -1.0
	in_range = false
	regen_reset()
	recent_damage_sources.clear()
	recent_benefactors.clear()
	attack_count = 0
	global_position = spawn_pos


func regen_reset() -> void:
	_regen_accumulator = 0.0


func _move_toward(target: Object, dt: float, world: Object) -> void:
	if not _combat_ready:
		velocity = Vector2.ZERO
		return
	var unit_pos := global_position
	var target_pos: Vector2 = target.global_position
	var to_target := target_pos - unit_pos
	var target_distance := unit_pos.distance_to(target_pos)
	if target_distance <= CombatData.EPSILON or to_target.length() <= CombatData.EPSILON:
		velocity = Vector2.ZERO
		return
	var speed := move_speed
	if last_kite_timer > 0.0:
		speed *= CombatData.KITE_SPEED_MODIFIER
	var max_step := speed * dt
	var desired_step := 0.0
	if not CombatData.is_melee_in_contact(target_distance, attack_range, combat_ranged_threshold, combat_contact_buffer):
		desired_step = maxf(0.0, target_distance - CombatData.effective_attack_range(attack_range, combat_ranged_threshold, combat_contact_buffer))
	var step := minf(max_step, desired_step)
	if step <= 0.0:
		velocity = Vector2.ZERO
		return
	velocity = to_target.normalized() * (step / maxf(dt, CombatData.EPSILON))


func _kite_from_enemies(_world: Object, enemies: Array, dt: float) -> bool:
	var repulsion := Vector2.ZERO
	var count := 0
	var danger_radius := attack_range * CombatData.KITE_DANGER_THRESHOLD
	for enemy in enemies:
		var enemy_pos: Vector2 = enemy.global_position
		var dist_sq: float = global_position.distance_squared_to(enemy_pos)
		if dist_sq > 0.0 and dist_sq < danger_radius * danger_radius:
			var dist: float = sqrt(dist_sq)
			if dist > CombatData.EPSILON:
				var weight := 1.0 / dist
				var diff: Vector2 = (global_position - enemy_pos) * (weight / dist)
				repulsion += diff
				count += 1
	if count <= 0 or repulsion == Vector2.ZERO:
		velocity = Vector2.ZERO
		return false
	var velocity_dir := repulsion.normalized()
	if velocity_dir.x == 0.0 and velocity_dir.y == 0.0:
		velocity_dir = Vector2.ONE.normalized()
	last_kite_timer = CombatData.KITE_DURATION
	velocity = velocity_dir * move_speed * CombatData.KITE_SPEED_MODIFIER
	return true


func _clamp_velocity_to_combat_bounds(desired_velocity: Vector2, dt: float) -> Vector2:
	if dt <= CombatData.EPSILON:
		return desired_velocity
	return desired_velocity


func _set_current_target(world: Object, target: Object) -> void:
	if current_target == target:
		return
	var old_target: Object = current_target
	current_target = target
	var combat_world := world as CombatWorld
	if combat_world != null:
		combat_world.adjust_target_pressure(old_target, target)
	target_id = -1 if target == null else int(target.get("instance_id"))


func _contains_unit_id(units: Array, unit_id: int) -> bool:
	for unit in units:
		var unit_state := unit as CombatUnitState
		if unit_state != null and unit_state.is_alive() and unit_state.instance_id == unit_id:
			return true
	return false


func _has_alive_role(units: Array, roles: Array[String]) -> bool:
	for unit in units:
		var unit_state := unit as CombatUnitState
		if unit_state == null or not unit_state.is_alive():
			continue
		if unit_state.role in roles:
			return true
	return false


func _set_target_debug(target: Object, distance: float, is_in_range: bool) -> void:
	target_id = -1 if target == null else int(target.get("instance_id"))
	distance_to_target = distance
	in_range = is_in_range


func _set_post_attack_cooldown() -> void:
	if attack_speed <= 0.0:
		attack_cooldown_remaining = INF
		return
	attack_cooldown_remaining = 1.0 / attack_speed


func _prune_recent(source: Dictionary, cutoff: float) -> Dictionary:
	var result: Dictionary = {}
	for key in source.keys():
		var timestamp := float(source[key])
		if timestamp > cutoff:
			result[key] = timestamp
	return result


func _record_effect_event(world: Object, target: Object, result: CombatEffectResult, action_reason: String) -> void:
	var combat_world := world as CombatWorld
	if combat_world == null or target == null:
		return
	var target_state := target as CombatUnitState
	combat_world.record_event({
		"timestamp": combat_world.time,
		"attacker_id": instance_id,
		"attacker_name": display_name,
		"target_id": target_state.instance_id if target_state != null else int(target.get("instance_id")),
		"target_name": target_state.display_name if target_state != null else String(target.get("display_name")),
		"damage": result.damage,
		"healing": result.healing,
		"shield_absorbed": result.shield_absorbed,
		"shield_added": result.shield_added,
		"distance": global_position.distance_to(target_state.global_position if target_state != null else target.global_position),
		"target_hp_after": target_state.hp if target_state != null else float(target.get("hp")),
		"target_shield_after": target_state.shield if target_state != null else float(target.get("shield")),
		"target_selection_reason": result.reason if not result.reason.is_empty() else action_reason,
	})
