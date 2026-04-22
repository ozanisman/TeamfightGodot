extends RefCounted
class_name BattleMetrics

const CombatData = preload("res://scripts/combat_data.gd")

var arena_size: Vector2 = Vector2.ZERO
var arena_origin: Vector2 = Vector2.ZERO
var pixels_per_unit: float = 1.0


func setup(viewport_size: Vector2) -> void:
	setup_for_viewport(viewport_size)


func setup_for_viewport(viewport_size: Vector2) -> void:
	var arena_side := minf(viewport_size.x, viewport_size.y)
	setup_from_bounds(
		Vector2(
			(viewport_size.x - arena_side) * 0.5,
			(viewport_size.y - arena_side) * 0.5
		),
		Vector2(arena_side, arena_side),
		arena_side / maxf(CombatData.WORLD_SIZE_VECTOR.x, CombatData.EPSILON)
	)


func setup_from_bounds(new_arena_origin: Vector2, new_arena_size: Vector2, new_pixels_per_unit: float = 1.0) -> void:
	arena_origin = new_arena_origin
	arena_size = new_arena_size
	pixels_per_unit = maxf(new_pixels_per_unit, CombatData.EPSILON)


func setup_for_world(world_size: Vector2, pixels_per_unit_value: float = 1.0) -> void:
	setup_from_bounds(Vector2.ZERO, world_size, pixels_per_unit_value)


func world_to_combat(pos: Vector2) -> Vector2:
	return arena_origin + pos * pixels_per_unit


func combat_to_world(pos: Vector2) -> Vector2:
	return (pos - arena_origin) / maxf(pixels_per_unit, CombatData.EPSILON)


func scale_length(value: float) -> float:
	return value * pixels_per_unit


func clamp_position(pos: Vector2) -> Vector2:
	var min_pos := arena_origin
	var max_pos := arena_origin + arena_size
	return Vector2(
		clampf(pos.x, min_pos.x, max_pos.x),
		clampf(pos.y, min_pos.y, max_pos.y)
	)


func get_rect() -> Rect2:
	return Rect2(arena_origin, arena_size)
