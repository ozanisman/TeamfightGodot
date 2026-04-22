extends CharacterBody2D
class_name BattleUnit

const CombatData = preload("res://scripts/combat_data.gd")
const CombatUnitStateScript = preload("res://scripts/combat_unit_state.gd")

const BASE_RADIUS_WORLD := 0.24
const FALLBACK_RADIUS := 21.0

@onready var collision_shape: CollisionShape2D = $CollisionShape2D

var combat_state: CombatUnitState = CombatUnitStateScript.new()
var _combat_metrics: Object = null
var _combat_ready: bool = false

var hero_id: String = ""
var display_name: String = ""
var role: String = ""
var team: String = ""
var color: Color = Color.WHITE
var passive_id: String = ""
var spawn_pos: Vector2 = Vector2.ZERO
var selected: bool = false
var hovered: bool = false
var dragging: bool = false
var draggable: bool = false
var visual_scale: float = 1.0

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
var current_state: int = 0
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


func _ready() -> void:
	motion_mode = CharacterBody2D.MOTION_MODE_FLOATING
	if collision_shape != null and collision_shape.shape is CircleShape2D:
		(collision_shape.shape as CircleShape2D).radius = FALLBACK_RADIUS
	queue_redraw()


func _sync_from_state() -> void:
	hero_id = combat_state.hero_id
	display_name = combat_state.display_name
	role = combat_state.role
	team = combat_state.team
	color = combat_state.color
	passive_id = combat_state.passive_id
	spawn_pos = combat_state.spawn_pos
	selected = combat_state.selected
	hovered = combat_state.hovered
	dragging = combat_state.dragging
	draggable = combat_state.draggable
	max_hp = combat_state.max_hp
	hp = combat_state.hp
	shield = combat_state.shield
	magic_resist = combat_state.magic_resist
	armor = combat_state.armor
	tenacity = combat_state.tenacity
	life_steal = combat_state.life_steal
	max_mana = combat_state.max_mana
	mana_per_attack = combat_state.mana_per_attack
	ability_cd = combat_state.ability_cd
	ultimate_cd = combat_state.ultimate_cd
	respawn_time = combat_state.respawn_time
	attack_damage = combat_state.attack_damage
	attack_range = combat_state.attack_range
	attack_speed = combat_state.attack_speed
	move_speed = combat_state.move_speed
	projectile_speed = combat_state.projectile_speed
	projectile_radius = combat_state.projectile_radius
	combat_scale = combat_state.combat_scale
	combat_ranged_threshold = combat_state.combat_ranged_threshold
	combat_contact_buffer = combat_state.combat_contact_buffer
	combat_bounds_min = combat_state.combat_bounds_min
	combat_bounds_max = combat_state.combat_bounds_max
	alive = combat_state.alive
	current_state = combat_state.current_state
	attack_cooldown_remaining = combat_state.attack_cooldown_remaining
	casting_remaining = combat_state.casting_remaining
	_is_casting_ult = combat_state._is_casting_ult
	ability_timer = combat_state.ability_timer
	ultimate_timer = combat_state.ultimate_timer
	stun_timer = combat_state.stun_timer
	respawn_timer = combat_state.respawn_timer
	retarget_timer = combat_state.retarget_timer
	last_kite_timer = combat_state.last_kite_timer
	target_switch_lock_timer = combat_state.target_switch_lock_timer
	taunt_timer = combat_state.taunt_timer
	taunted_by = combat_state.taunted_by
	forced_target_timer = combat_state.forced_target_timer
	forced_target_id = combat_state.forced_target_id
	forced_target_source_id = combat_state.forced_target_source_id
	forced_target_kind = combat_state.forced_target_kind
	current_target = combat_state.current_target
	current_ally_target = combat_state.current_ally_target
	instance_id = combat_state.instance_id
	target_id = combat_state.target_id
	distance_to_target = combat_state.distance_to_target
	in_range = combat_state.in_range
	last_target_reason = combat_state.last_target_reason
	last_target_score = combat_state.last_target_score
	retargeted_this_tick = combat_state.retargeted_this_tick
	retarget_reason_this_tick = combat_state.retarget_reason_this_tick
	incoming_target_count = combat_state.incoming_target_count
	perceived_threat = combat_state.perceived_threat
	damage_dealt = combat_state.damage_dealt
	damage_dealt_auto = combat_state.damage_dealt_auto
	damage_dealt_ability = combat_state.damage_dealt_ability
	damage_dealt_ultimate = combat_state.damage_dealt_ultimate
	damage_received = combat_state.damage_received
	damage_mitigated = combat_state.damage_mitigated
	healing_done = combat_state.healing_done
	shielding_done = combat_state.shielding_done
	auto_attacks_done = combat_state.auto_attacks_done
	abilities_used = combat_state.abilities_used
	ultimates_used = combat_state.ultimates_used
	stuns_dealt = combat_state.stuns_dealt
	kills = combat_state.kills
	deaths = combat_state.deaths
	assists = combat_state.assists
	mana = combat_state.mana
	attack_count = combat_state.attack_count
	recent_damage_sources = combat_state.recent_damage_sources
	recent_benefactors = combat_state.recent_benefactors
	velocity = combat_state.velocity
	global_position = combat_state.global_position


func _sync_to_state() -> void:
	combat_state.hero_id = hero_id
	combat_state.display_name = display_name
	combat_state.role = role
	combat_state.team = team
	combat_state.color = color
	combat_state.passive_id = passive_id
	combat_state.spawn_pos = spawn_pos
	combat_state.selected = selected
	combat_state.hovered = hovered
	combat_state.dragging = dragging
	combat_state.draggable = draggable
	combat_state.max_hp = max_hp
	combat_state.hp = hp
	combat_state.shield = shield
	combat_state.magic_resist = magic_resist
	combat_state.armor = armor
	combat_state.tenacity = tenacity
	combat_state.life_steal = life_steal
	combat_state.max_mana = max_mana
	combat_state.mana_per_attack = mana_per_attack
	combat_state.ability_cd = ability_cd
	combat_state.ultimate_cd = ultimate_cd
	combat_state.respawn_time = respawn_time
	combat_state.attack_damage = attack_damage
	combat_state.attack_range = attack_range
	combat_state.attack_speed = attack_speed
	combat_state.move_speed = move_speed
	combat_state.projectile_speed = projectile_speed
	combat_state.projectile_radius = projectile_radius
	combat_state.combat_scale = combat_scale
	combat_state.combat_ranged_threshold = combat_ranged_threshold
	combat_state.combat_contact_buffer = combat_contact_buffer
	combat_state.combat_bounds_min = combat_bounds_min
	combat_state.combat_bounds_max = combat_bounds_max
	combat_state.alive = alive
	combat_state.current_state = current_state
	combat_state.attack_cooldown_remaining = attack_cooldown_remaining
	combat_state.casting_remaining = casting_remaining
	combat_state._is_casting_ult = _is_casting_ult
	combat_state.ability_timer = ability_timer
	combat_state.ultimate_timer = ultimate_timer
	combat_state.stun_timer = stun_timer
	combat_state.respawn_timer = respawn_timer
	combat_state.retarget_timer = retarget_timer
	combat_state.last_kite_timer = last_kite_timer
	combat_state.target_switch_lock_timer = target_switch_lock_timer
	combat_state.taunt_timer = taunt_timer
	combat_state.taunted_by = taunted_by
	combat_state.forced_target_timer = forced_target_timer
	combat_state.forced_target_id = forced_target_id
	combat_state.forced_target_source_id = forced_target_source_id
	combat_state.forced_target_kind = forced_target_kind
	combat_state.current_target = current_target
	combat_state.current_ally_target = current_ally_target
	combat_state.instance_id = instance_id
	combat_state.target_id = target_id
	combat_state.distance_to_target = distance_to_target
	combat_state.in_range = in_range
	combat_state.last_target_reason = last_target_reason
	combat_state.last_target_score = last_target_score
	combat_state.retargeted_this_tick = retargeted_this_tick
	combat_state.retarget_reason_this_tick = retarget_reason_this_tick
	combat_state.incoming_target_count = incoming_target_count
	combat_state.perceived_threat = perceived_threat
	combat_state.damage_dealt = damage_dealt
	combat_state.damage_dealt_auto = damage_dealt_auto
	combat_state.damage_dealt_ability = damage_dealt_ability
	combat_state.damage_dealt_ultimate = damage_dealt_ultimate
	combat_state.damage_received = damage_received
	combat_state.damage_mitigated = damage_mitigated
	combat_state.healing_done = healing_done
	combat_state.shielding_done = shielding_done
	combat_state.auto_attacks_done = auto_attacks_done
	combat_state.abilities_used = abilities_used
	combat_state.ultimates_used = ultimates_used
	combat_state.stuns_dealt = stuns_dealt
	combat_state.kills = kills
	combat_state.deaths = deaths
	combat_state.assists = assists
	combat_state.mana = mana
	combat_state.attack_count = attack_count
	combat_state.recent_damage_sources = recent_damage_sources
	combat_state.recent_benefactors = recent_benefactors
	combat_state.velocity = velocity
	combat_state.global_position = global_position


func _process(_delta: float) -> void:
	_sync_from_state()
	queue_redraw()


func set_identity(new_hero_id: String, new_display_name: String, new_role: String, new_team: String, new_color: Color, new_draggable: bool, new_passive_id: String = "") -> void:
	combat_state.set_identity(new_hero_id, new_display_name, new_role, new_team, new_color, new_draggable, new_passive_id)
	_sync_from_state()


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
	combat_state.set_combat_stats(
		new_max_hp,
		new_attack_damage,
		new_attack_range,
		new_attack_speed,
		new_move_speed,
		new_armor,
		new_projectile_speed,
		new_magic_resist,
		new_tenacity,
		new_life_steal,
		new_respawn_time,
		new_mana_per_attack,
		new_max_mana,
		new_ability_cd,
		new_ultimate_cd,
		new_projectile_radius
	)
	_sync_from_state()


func set_spawn_position(pos: Vector2) -> void:
	combat_state.set_spawn_position(pos)
	_sync_from_state()


func set_visual_scale(scale: float) -> void:
	visual_scale = maxf(scale, CombatData.EPSILON)
	if collision_shape != null and collision_shape.shape is CircleShape2D:
		(collision_shape.shape as CircleShape2D).radius = BASE_RADIUS_WORLD * visual_scale
	queue_redraw()


func set_world_position(pos: Vector2) -> void:
	combat_state.set_world_position(pos)
	_sync_from_state()


func set_combat_world(world: Object) -> void:
	combat_state.set_combat_world(world)


func apply_combat_metrics(metrics: Object) -> void:
	_combat_metrics = metrics
	_combat_ready = metrics != null
	combat_state.apply_combat_metrics(metrics)
	_sync_from_state()


func set_collision_enabled(enabled: bool) -> void:
	collision_layer = 1 if enabled else 0
	collision_mask = 1 if enabled else 0
	if not enabled:
		velocity = Vector2.ZERO
	combat_state.set_collision_enabled(enabled)


func set_combat_registry(registry: Object) -> void:
	combat_state.set_combat_registry(registry)


func set_focus(is_selected: bool, is_hovered: bool, is_dragging: bool) -> void:
	_sync_to_state()
	combat_state.set_focus(is_selected, is_hovered, is_dragging)
	_sync_from_state()
	queue_redraw()


func set_mana(value: float) -> void:
	_sync_to_state()
	combat_state.set_mana(value)
	_sync_from_state()


func set_attack_count(value: int) -> void:
	_sync_to_state()
	combat_state.set_attack_count(value)
	_sync_from_state()


func add_damage_dealt(amount: float) -> void:
	_sync_to_state()
	combat_state.add_damage_dealt(amount)
	_sync_from_state()


func is_alive() -> bool:
	return combat_state.is_alive()


func get_state_label() -> String:
	return combat_state.get_state_label()


func get_forced_target_id() -> int:
	return combat_state.get_forced_target_id()


func get_forced_target_source_id() -> int:
	return combat_state.get_forced_target_source_id()


func apply_taunt(duration: float, source_id: int) -> void:
	_sync_to_state()
	combat_state.apply_taunt(duration, source_id)
	_sync_from_state()


func apply_forced_target(duration: float, source_id: int, target_id: int = -1, kind: String = "taunt") -> void:
	_sync_to_state()
	combat_state.apply_forced_target(duration, source_id, target_id, kind)
	_sync_from_state()


func apply_stun(duration: float, world: Object, source_id: int = -1) -> void:
	_sync_to_state()
	combat_state.apply_stun(duration, world, source_id)
	_sync_from_state()


func add_shield(amount: float, world: Object, source_id: int = -1) -> void:
	_sync_to_state()
	combat_state.add_shield(amount, world, source_id)
	_sync_from_state()


func heal(amount: float, world: Object, source_id: int = -1) -> void:
	_sync_to_state()
	combat_state.heal(amount, world, source_id)
	_sync_from_state()


func take_damage(amount: float, world: Object, damage_type: String = "true", source_id: int = -1) -> Dictionary:
	_sync_to_state()
	var result: Dictionary = combat_state.take_damage(amount, world, damage_type, source_id)
	_sync_from_state()
	return result


func update_cooldowns(dt: float) -> void:
	_sync_to_state()
	combat_state.update_cooldowns(dt)
	_sync_from_state()


func _begin_cast(_world: Object, is_ult: bool) -> bool:
	_sync_to_state()
	var result: bool = combat_state.begin_cast(is_ult)
	_sync_from_state()
	return result


func update(dt: float, world: Object) -> void:
	_sync_to_state()
	combat_state.update(dt, world)
	_sync_from_state()


func attack(target: Object, world: Object, reason: String) -> Dictionary:
	_sync_to_state()
	var result: Dictionary = combat_state.attack(target, world, reason)
	_sync_from_state()
	return result


func on_attack_hit(target: Object, damage: float, world: Object, is_auto: bool = true) -> void:
	_sync_to_state()
	combat_state.on_attack_hit(target, damage, world, is_auto)
	_sync_from_state()


func respawn(world: Object = null) -> void:
	_sync_to_state()
	combat_state.respawn(world)
	_sync_from_state()
	queue_redraw()


func step_motion(dt: float) -> void:
	_sync_to_state()
	combat_state.step_motion(dt)
	_sync_from_state()


func contains_screen_point(screen_pos: Vector2) -> bool:
	return global_position.distance_to(screen_pos) <= _radius_px() + 6.0


func _radius_px() -> float:
	return maxf(FALLBACK_RADIUS * visual_scale, 12.0)


func _draw() -> void:
	var radius := _radius_px()
	var fill_color := color
	if not is_alive():
		fill_color = Color(0.35, 0.35, 0.35)
	elif dragging:
		fill_color = Color(1.0, 1.0, 1.0)
	elif hovered:
		fill_color = Color(minf(fill_color.r + 0.2, 1.0), minf(fill_color.g + 0.2, 1.0), minf(fill_color.b + 0.2, 1.0))

	draw_circle(Vector2.ZERO, radius, fill_color)
	draw_circle(Vector2.ZERO, radius, Color(0.1, 0.1, 0.12), false, 2.0)
