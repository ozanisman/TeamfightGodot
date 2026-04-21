extends Node2D
class_name BattleUnit

const CombatData = preload("res://scripts/combat_data.gd")

enum UnitState { MOVING, ATTACKING, CASTING, WAITING, KITING, STUNNED, DEAD }

const BASE_RADIUS := 18.0

var hero_id: String = ""
var display_name: String = ""
var role: String = ""
var team: String = ""
var color: Color = Color.WHITE

var spawn_pos: Vector2 = Vector2.ZERO
var world_pos: Vector2 = Vector2.ZERO

var selected: bool = false
var hovered: bool = false
var dragging: bool = false
var draggable: bool = false

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

var alive: bool = true
var current_state: int = UnitState.WAITING
var attack_cooldown_remaining: float = 0.0
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
var forced_target_kind: String = ""

var current_target: Node2D = null
var current_ally_target: Node2D = null
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
var recent_damage_sources: Dictionary = {}
var recent_benefactors: Dictionary = {}
var _regen_accumulator: float = 0.0


func _ready() -> void:
	queue_redraw()


func set_identity(new_hero_id: String, new_display_name: String, new_role: String, new_team: String, new_color: Color, new_draggable: bool) -> void:
	hero_id = new_hero_id
	display_name = new_display_name
	role = new_role
	team = new_team
	color = new_color
	draggable = new_draggable
	queue_redraw()


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
	alive = true
	current_state = UnitState.WAITING
	attack_cooldown_remaining = 0.0
	ability_timer = ability_cd
	ultimate_timer = ultimate_cd
	respawn_timer = 0.0
	queue_redraw()


func set_spawn_position(pos: Vector2) -> void:
	spawn_pos = pos


func set_world_position(pos: Vector2) -> void:
	world_pos = pos


func set_focus(is_selected: bool, is_hovered: bool, is_dragging: bool) -> void:
	selected = is_selected
	hovered = is_hovered
	dragging = is_dragging
	queue_redraw()


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
		UnitState.KITING:
			return "KITING"
		UnitState.STUNNED:
			return "STUNNED"
		UnitState.DEAD:
			return "DEAD"
		_:
			return "WAITING"


func get_forced_target_id() -> int:
	if forced_target_timer > 0.0 and forced_target_id >= 0:
		return forced_target_id
	return -1


func apply_taunt(duration: float, source_id: int) -> void:
	forced_target_timer = maxf(forced_target_timer, duration)
	forced_target_id = source_id
	forced_target_kind = "taunt"
	taunt_timer = forced_target_timer
	taunted_by = source_id


func apply_forced_target(duration: float, source_id: int, target_id: int = -1, kind: String = "taunt") -> void:
	forced_target_timer = maxf(forced_target_timer, duration)
	forced_target_id = target_id if target_id >= 0 else source_id
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
	if source_id >= 0:
		var attacker: Node2D = world.call("get_unit_by_id", source_id)
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
	if source_id >= 0 and source_id != int(get("instance_id")):
		recent_benefactors[source_id] = float(world.get("time"))
	world.call("record_event", {
		"timestamp": float(world.get("time")),
		"attacker_id": source_id if source_id >= 0 else int(get("instance_id")),
		"attacker_name": display_name,
		"target_id": int(get("instance_id")),
		"target_name": display_name,
		"healing": amount,
		"distance": 0.0,
		"target_hp_after": hp,
		"target_shield_after": shield,
		"target_selection_reason": "Healing",
	})
	queue_redraw()


func take_damage(amount: float, world: Object, damage_type: String = "true", source_id: int = -1) -> Dictionary:
	var pre_res_damage := amount
	var final_damage := amount
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
	if hp <= 0.0:
		alive = false
		current_state = UnitState.DEAD
		respawn_timer = respawn_time
		current_target = null
		current_ally_target = null
		in_range = false
		distance_to_target = -1.0
		target_id = -1
		attack_cooldown_remaining = 0.0
	queue_redraw()
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
			forced_target_kind = ""
			taunt_timer = 0.0
			taunted_by = -1
	if stun_timer > 0.0:
		stun_timer = maxf(0.0, stun_timer - dt)


func sync_screen_position(viewport_size: Vector2, world_size: Vector2) -> void:
	if viewport_size.x <= 0.0 or viewport_size.y <= 0.0:
		return
	position = Vector2(
		clampf(world_pos.x / maxf(world_size.x, 0.001), 0.0, 1.0) * viewport_size.x,
		clampf(world_pos.y / maxf(world_size.y, 0.001), 0.0, 1.0) * viewport_size.y
	)


func contains_screen_point(screen_pos: Vector2) -> bool:
	return global_position.distance_to(screen_pos) <= BASE_RADIUS + 6.0


func update(dt: float, world: Object) -> void:
	if current_state == UnitState.DEAD:
		respawn_timer = maxf(0.0, respawn_timer - dt)
		if respawn_timer <= 0.0:
			respawn(world)
		return

	if not is_alive():
		current_state = UnitState.DEAD
		respawn_timer = respawn_time
		current_target = null
		current_ally_target = null
		return

	update_cooldowns(dt)

	var cutoff := float(world.get("time")) - CombatData.ASSIST_WINDOW
	recent_damage_sources = _prune_recent(recent_damage_sources, cutoff)
	recent_benefactors = _prune_recent(recent_benefactors, cutoff)

	if stun_timer > 0.0:
		current_state = UnitState.STUNNED
		return

	var targeting_system = world.call("get_targeting_system")
	var strategy: Dictionary = targeting_system.call("get_strategy_for", self)
	perceived_threat = maxf(0.0, perceived_threat - float(strategy.get("threat_decay_rate", CombatData.THREAT_DECAY_DEFAULT)) * dt)
	_apply_separation(world, dt)

	_regen_accumulator += dt
	if _regen_accumulator >= CombatData.REGEN_TICK_INTERVAL:
		_regen_accumulator -= CombatData.REGEN_TICK_INTERVAL

	var allies: Array[Node2D] = world.call("get_allies_for", self)
	var enemies: Array[Node2D] = world.call("get_enemies_for", self)

	if role == "support" or role == "tank":
		current_ally_target = targeting_system.call("select_ally", self, allies)
	else:
		current_ally_target = null

	var candidate: Dictionary = _retarget_if_needed(world, enemies, allies, targeting_system)
	if candidate.is_empty():
		current_state = UnitState.WAITING
		return

	var target: Node2D = candidate.get("target")
	if target == null or not bool(target.call("is_alive")):
		current_state = UnitState.WAITING
		_set_current_target(world, null)
		return

	_set_target_debug(target, float(candidate.get("distance", 0.0)), bool(candidate.get("in_range", false)))

	if in_range and attack_cooldown_remaining <= 0.0:
		_perform_auto_attack(world, String(candidate.get("reason", "target")))
		current_state = UnitState.ATTACKING
		return

	if bool(strategy.get("prefers_kiting", false)) and attack_cooldown_remaining > 0.0 and taunted_by < 0:
		if _kite_from_enemies(world, enemies, dt):
			current_state = UnitState.KITING
			return

	_move_toward(target, dt, world)
	current_state = UnitState.MOVING if not in_range else UnitState.WAITING


func _retarget_if_needed(world: Object, enemies: Array[Node2D], allies: Array[Node2D], targeting_system: Object) -> Dictionary:
	var alive_enemies: Array[Node2D] = []
	for enemy in enemies:
		if bool(enemy.call("is_alive")):
			alive_enemies.append(enemy)
	if alive_enemies.is_empty():
		_set_current_target(world, null)
		last_target_reason = "no candidate found"
		last_target_score = 0.0
		return {}

	if get_forced_target_id() >= 0:
		var forced_candidate: Dictionary = targeting_system.call("select_target", self, alive_enemies, allies)
		if forced_candidate.is_empty():
			_set_current_target(world, null)
			last_target_reason = "no candidate found"
			last_target_score = 0.0
			return {}
		_set_current_target(world, forced_candidate.get("target"))
		last_target_score = float(forced_candidate.get("score", 0.0))
		last_target_reason = "forced target; %s" % String(forced_candidate.get("reason", "target"))
		retargeted_this_tick = true
		retarget_reason_this_tick = last_target_reason
		retarget_timer = 0.0
		return forced_candidate

	var strategy: Dictionary = targeting_system.call("get_strategy_for", self)
	var needs_evaluation := current_target == null or not bool(current_target.call("is_alive")) or retarget_timer <= 0.0
	if not needs_evaluation:
		return targeting_system.call("score_target", self, current_target, strategy, allies, alive_enemies)

	retarget_timer = CombatData.RETARGET_INTERVAL
	var candidate: Dictionary = targeting_system.call("select_target", self, alive_enemies, allies)
	if candidate.is_empty():
		_set_current_target(world, null)
		last_target_reason = "no candidate found"
		last_target_score = 0.0
		return {}

	if current_target == null or not bool(current_target.call("is_alive")):
		_set_current_target(world, candidate.get("target"))
		last_target_score = float(candidate.get("score", 0.0))
		last_target_reason = "periodic pick; %s" % String(candidate.get("reason", "target"))
		retargeted_this_tick = true
		retarget_reason_this_tick = last_target_reason
		return candidate

	var current_candidate: Dictionary = targeting_system.call("score_target", self, current_target, strategy, allies, alive_enemies)
	if int(candidate.get("target").get("instance_id")) == int(current_target.get("instance_id")):
		last_target_score = float(current_candidate.get("score", 0.0))
		last_target_reason = "kept target; %s" % String(current_candidate.get("reason", "target"))
		return current_candidate

	if targeting_system.call("should_switch", float(current_candidate.get("score", 0.0)), float(candidate.get("score", 0.0)), strategy, self):
		_set_current_target(world, candidate.get("target"))
		target_switch_lock_timer = CombatData.TARGET_SWITCH_LOCK_DURATION
		last_target_score = float(candidate.get("score", 0.0))
		last_target_reason = "switched; %s" % String(candidate.get("reason", "target"))
		retargeted_this_tick = true
		retarget_reason_this_tick = last_target_reason
		return candidate

	last_target_score = float(current_candidate.get("score", 0.0))
	last_target_reason = "kept target"
	return current_candidate


func _perform_auto_attack(world: Object, reason: String) -> void:
	current_state = UnitState.ATTACKING
	if not is_alive() or current_target == null or not bool(current_target.call("is_alive")):
		current_state = UnitState.WAITING
		return

	var attack_result: Dictionary = attack(current_target, world, reason)
	if max_mana > 0.0:
		mana = minf(max_mana, mana + mana_per_attack)
	_set_post_attack_cooldown()
	if not attack_result.is_empty():
		world.call("record_event", attack_result)


func attack(target: Node2D, world: Object, reason: String) -> Dictionary:
	if not is_alive() or not bool(target.call("is_alive")):
		return {}

	var damage_mult := 1.0
	var damage := attack_damage * damage_mult
	var distance: float = world.call("get_unit_by_id", int(target.get("instance_id"))).global_position.distance_to(global_position)

	auto_attacks_done += 1
	if attack_range > CombatData.RANGED_THRESHOLD:
		world.call("spawn_projectile", self, target, damage, "physical", reason, projectile_speed, projectile_radius, 0.0, false, false)
		return {}

	var damage_result: Dictionary = target.call("take_damage", damage, world, "physical", int(get("instance_id")))
	var absorbed := float(damage_result.get("absorbed", 0.0))
	var hp_loss := float(damage_result.get("hp_loss", 0.0))
	var total_dmg := absorbed + hp_loss
	damage_dealt_auto += total_dmg
	damage_dealt += total_dmg
	on_attack_hit(target, total_dmg, world, true)
	return {
		"timestamp": float(world.get("time")),
		"attacker_id": int(get("instance_id")),
		"attacker_name": display_name,
		"target_id": int(target.get("instance_id")),
		"target_name": String(target.get("display_name")),
		"damage": total_dmg,
		"shield_absorbed": absorbed,
		"distance": distance,
		"target_hp_after": float(target.get("hp")),
		"target_shield_after": float(target.get("shield")),
		"target_selection_reason": reason,
	}


func on_attack_hit(target: Node2D, damage: float, world: Object, is_auto: bool = true) -> void:
	if is_auto and life_steal > 0.0 and damage > 0.0:
		heal(damage * life_steal, world, int(get("instance_id")))


func respawn(world: Object = null) -> void:
	hp = max_hp
	shield = 0.0
	mana = 0.0
	perceived_threat = 0.0
	current_state = UnitState.WAITING
	alive = true
	attack_cooldown_remaining = 0.0
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
	forced_target_kind = ""
	current_target = null
	current_ally_target = null
	target_id = -1
	distance_to_target = -1.0
	in_range = false
	regen_reset()
	recent_damage_sources.clear()
	recent_benefactors.clear()
	set_world_position(spawn_pos)
	queue_redraw()


func regen_reset() -> void:
	_regen_accumulator = 0.0


func _move_toward(target: Node2D, dt: float, world: Object) -> void:
	var unit_pos: Vector2 = world_pos
	var target_pos: Vector2 = target.get("world_pos")
	var direction: Vector2 = target_pos - unit_pos
	var distance: float = direction.length()
	if distance <= CombatData.EPSILON:
		return

	var speed := move_speed
	if last_kite_timer > 0.0:
		speed *= CombatData.KITE_SPEED_MODIFIER
	var max_step := speed * dt
	var desired_step := 0.0
	if not CombatData.is_melee_in_contact(distance, attack_range):
		desired_step = maxf(0.0, distance - CombatData.effective_attack_range(attack_range))
	var step := minf(max_step, desired_step)
	if step <= 0.0:
		return

	var old_pos := unit_pos
	set_world_position(unit_pos + direction * (step / distance))
	_clamp_position_to_world()
	world.call("notify_unit_moved", self, old_pos)


func _apply_separation(world: Object, dt: float) -> void:
	if move_speed <= 0.0:
		return

	var radius := CombatData.SEPARATION_RADIUS_RANGED if attack_range > CombatData.SEPARATION_RANGE_THRESHOLD else CombatData.SEPARATION_RADIUS_MELEE
	var allies: Array[Node2D] = world.call("nearby_units", world_pos, radius, team, int(get("instance_id")))
	var separation := Vector2.ZERO

	for ally in allies:
		var ally_pos: Vector2 = ally.get("world_pos")
		var dist_sq: float = world_pos.distance_squared_to(ally_pos)
		if dist_sq > 0.0 and dist_sq < radius * radius:
			var dist: float = sqrt(dist_sq)
			if dist > CombatData.EPSILON:
				var force := (radius - dist) / radius
				var diff: Vector2 = (world_pos - ally_pos) * (force / dist)
				separation += diff

	if separation != Vector2.ZERO:
		var old_pos := world_pos
		set_world_position(world_pos + separation * move_speed * CombatData.NUDGE_SPEED_MODIFIER * dt)
		_clamp_position_to_world()
		world.call("notify_unit_moved", self, old_pos)


func _kite_from_enemies(world: Object, enemies: Array[Node2D], dt: float) -> bool:
	var repulsion := Vector2.ZERO
	var count := 0
	var danger_radius := attack_range * CombatData.KITE_DANGER_THRESHOLD
	for enemy in enemies:
		var enemy_pos: Vector2 = enemy.get("world_pos")
		var dist_sq: float = world_pos.distance_squared_to(enemy_pos)
		if dist_sq > 0.0 and dist_sq < danger_radius * danger_radius:
			var dist: float = sqrt(dist_sq)
			if dist > CombatData.EPSILON:
				var weight := 1.0 / dist
				var diff: Vector2 = (world_pos - enemy_pos) * (weight / dist)
				repulsion += diff
				count += 1

	if count <= 0 or repulsion == Vector2.ZERO:
		return false

	var velocity := repulsion.normalized()
	if velocity.x == 0.0 and velocity.y == 0.0:
		velocity = Vector2(CombatData.RECOVERY_VELOCITY if world_pos.x < CombatData.WORLD_CENTER else -CombatData.RECOVERY_VELOCITY, CombatData.RECOVERY_VELOCITY if world_pos.y < CombatData.WORLD_CENTER else -CombatData.RECOVERY_VELOCITY).normalized()
	last_kite_timer = CombatData.KITE_DURATION
	var old_pos := world_pos
	set_world_position(world_pos + velocity * move_speed * CombatData.KITE_SPEED_MODIFIER * dt)
	_clamp_position_to_world()
	world.call("notify_unit_moved", self, old_pos)
	return true


func _clamp_position_to_world() -> void:
	world_pos.x = clampf(world_pos.x, CombatData.WORLD_BOUNDARY_MIN, CombatData.WORLD_BOUNDARY_MAX)
	world_pos.y = clampf(world_pos.y, CombatData.WORLD_BOUNDARY_MIN, CombatData.WORLD_BOUNDARY_MAX)


func _set_current_target(world: Object, target: Node2D) -> void:
	if current_target == target:
		return
	var old_target: Node2D = current_target
	current_target = target
	world.call("adjust_target_pressure", old_target, target)
	target_id = -1 if target == null else int(target.get("instance_id"))


func _set_target_debug(target: Node2D, distance: float, is_in_range: bool) -> void:
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


func _draw() -> void:
	var ring_color := Color(0.95, 0.85, 0.35) if selected or dragging else Color(0.6, 0.7, 1.0) if hovered else color.lightened(0.15)
	var fill_color := color
	var outline_width := 4.0 if selected or dragging else 2.0 if hovered else 1.0

	draw_circle(Vector2.ZERO, BASE_RADIUS, fill_color)
	draw_arc(Vector2.ZERO, BASE_RADIUS + 2.0, 0.0, TAU, 48, ring_color, outline_width)

	var hp_ratio := 0.0 if max_hp <= 0.0 else hp / max_hp
	draw_rect(Rect2(Vector2(-BASE_RADIUS, BASE_RADIUS + 6.0), Vector2(BASE_RADIUS * 2.0, 4.0)), Color(0.18, 0.18, 0.18), true)
	draw_rect(Rect2(Vector2(-BASE_RADIUS, BASE_RADIUS + 6.0), Vector2(BASE_RADIUS * 2.0 * hp_ratio, 4.0)), Color(0.20, 0.85, 0.30), true)
	if shield > 0.0 and max_hp > 0.0:
		var shield_ratio := clampf(shield / max_hp, 0.0, 1.0)
		draw_rect(Rect2(Vector2(-BASE_RADIUS, BASE_RADIUS + 11.0), Vector2(BASE_RADIUS * 2.0 * shield_ratio, 3.0)), Color(0.35, 0.65, 1.0), true)

	var font := ThemeDB.fallback_font
	var font_size := 12
	if font:
		var name_size := font.get_string_size(display_name, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size)
		draw_string(font, Vector2(-name_size.x * 0.5, -BASE_RADIUS - 10.0), display_name, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, Color.WHITE)

		var state := get_state_label()
		var state_size := font.get_string_size(state, HORIZONTAL_ALIGNMENT_LEFT, -1, 10)
		draw_string(font, Vector2(-state_size.x * 0.5, BASE_RADIUS + 16.0), state, HORIZONTAL_ALIGNMENT_LEFT, -1, 10, Color(0.9, 0.9, 0.95))
