extends Control

var max_hp: float = 1.0
var hp: float = 0.0
var shield: float = 0.0

const COLOR_HP_FILL := Color(0.31, 0.78, 0.31, 1.0)
const COLOR_SHIELD_FILL := Color(1.0, 1.0, 1.0, 0.9)

func set_data(p_max_hp: float, p_hp: float, p_shield: float) -> void:
	max_hp = maxf(1.0, p_max_hp)
	hp = p_hp
	shield = p_shield
	queue_redraw()

func _draw() -> void:
	var size := get_size()
	if size.x < 1.0 or size.y < 1.0:
		return
	
	# Calculate HP and shield ratios for display
	var total_effective: float = hp + shield
	var hp_ratio: float
	var shield_ratio: float
	
	if total_effective > max_hp:
		# When HP + Shield > Max HP, show ratio of HP to total effective health
		hp_ratio = clampf(hp / total_effective, 0.0, 1.0)
		shield_ratio = clampf(shield / total_effective, 0.0, 1.0)
	else:
		# When HP + Shield <= Max HP, show ratios relative to Max HP
		hp_ratio = clampf(hp / max_hp, 0.0, 1.0)
		shield_ratio = clampf(shield / max_hp, 0.0, 1.0)
	
	# Draw HP bar (green)
	var hp_width: float = size.x * hp_ratio
	if hp_width > 0.0:
		draw_rect(Rect2(Vector2.ZERO, Vector2(hp_width, size.y)), COLOR_HP_FILL)
	
	# Draw shield bar (white) layered on top, to the right of HP
	if shield > 0.0:
		var shield_x: float = hp_width
		var shield_width: float = size.x * shield_ratio
		if shield_width > 0.0:
			draw_rect(Rect2(Vector2(shield_x, 0.0), Vector2(shield_width, size.y)), COLOR_SHIELD_FILL)
	
	# Draw tick marks on top of everything for visibility
	_draw_hp_ticks(size, max_hp, hp, shield)


func _draw_hp_ticks(size: Vector2, max_hp: float, hp: float, shield: float) -> void:
	var total_effective: float = hp + shield
	var scale: float = size.x / maxf(max_hp, total_effective)
	var tick_color := Color(0.15, 0.15, 0.15, 0.9)
	
	# Use total_effective as the upper bound for tick drawing when it exceeds max_hp
	var tick_upper_bound: float = maxf(max_hp, total_effective)
	
	# Draw ticks every 10 HP (top to middle)
	var tick_10_interval := 10.0
	var num_10_ticks := int(tick_upper_bound / tick_10_interval)
	for i in range(1, num_10_ticks + 1):
		var tick_hp := i * tick_10_interval
		if tick_hp > total_effective:
			break
		var tick_x := tick_hp * scale
		var tick_height := size.y * 0.5
		draw_line(Vector2(tick_x, 0.0), Vector2(tick_x, tick_height), tick_color, 2.0)
	
	# Draw ticks every 100 HP (top to bottom)
	var tick_100_interval := 100.0
	var num_100_ticks := int(tick_upper_bound / tick_100_interval)
	for i in range(1, num_100_ticks + 1):
		var tick_hp := i * tick_100_interval
		if tick_hp > total_effective:
			break
		var tick_x := tick_hp * scale
		draw_line(Vector2(tick_x, 0.0), Vector2(tick_x, size.y), tick_color, 2.0)
