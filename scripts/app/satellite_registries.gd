extends Node
## Global registries for satellite content builders, layout strategies, and style presets.

const SatelliteSpecScript := preload("res://scripts/app/satellite_spec.gd")

static var _content_builders: Dictionary = {}
static var _layout_strategies: Dictionary = {}
static var _style_presets: Dictionary = {}
static var _initialized: bool = false


## Initialize default builders, layouts, and styles.
static func _static_init() -> void:
	if _initialized:
		return
	_initialized = true
	
	# Register default content builders
	register_builder(&"minion", _build_minion_content)
	register_builder(&"status", _build_status_content)
	register_builder(&"modifier", _build_modifier_content)
	
	# Register default layout strategies
	register_layout(&"grid", _layout_grid)
	register_layout(&"radial", _layout_radial)
	register_layout(&"stack", _layout_stack)
	register_layout(&"custom", _layout_custom)
	
	# Register default style presets
	_register_style_presets()


## Register a content builder callable.
static func register_builder(type_key: StringName, builder: Callable) -> void:
	_content_builders[type_key] = builder


## Get a content builder callable by type key.
static func get_builder(type_key: StringName) -> Callable:
	return _content_builders.get(type_key, Callable())


## Register a layout strategy callable.
static func register_layout(strategy_key: StringName, layout: Callable) -> void:
	_layout_strategies[strategy_key] = layout


## Get a layout strategy callable by strategy key.
static func get_layout(strategy_key: StringName) -> Callable:
	return _layout_strategies.get(strategy_key, Callable())


## Register a style preset.
static func register_style_preset(preset_key: StringName, style_config: Dictionary) -> void:
	_style_presets[preset_key] = style_config


## Get a style preset by key.
static func get_style_preset(preset_key: StringName) -> Dictionary:
	return _style_presets.get(preset_key, {}).duplicate(true)


## Register default style presets.
static func _register_style_presets() -> void:
	# Minion style - neutral border, smaller font
	_style_presets[&"minion"] = {
		"border_color": Color(0.71, 0.71, 0.75, 1.0),
		"border_width": 1,
		"bg_color": Color(0.11, 0.11, 0.149, 1.0),
		"font_size": 18,
		"min_width": 400,
		"corner_radius": 3,
		"content_margin": 8
	}
	
	# Status style - effect-colored borders, compact
	_style_presets[&"status"] = {
		"border_color": Color(0.9, 0.4, 0.4, 1.0),  # Red default
		"border_width": 1,
		"bg_color": Color(0.11, 0.11, 0.149, 1.0),
		"font_size": 16,
		"min_width": 200,
		"corner_radius": 3,
		"content_margin": 6
	}
	
	# Modifier style - buff/nerf colored
	_style_presets[&"modifier"] = {
		"border_color": Color(0.4, 0.9, 0.4, 1.0),  # Green default
		"border_width": 1,
		"bg_color": Color(0.11, 0.11, 0.149, 1.0),
		"font_size": 16,
		"min_width": 250,
		"corner_radius": 3,
		"content_margin": 6
	}
	
	# Neutral style
	_style_presets[&"neutral"] = {
		"border_color": Color(0.71, 0.71, 0.75, 1.0),
		"border_width": 1,
		"bg_color": Color(0.11, 0.11, 0.149, 1.0),
		"font_size": 18,
		"min_width": 400,
		"corner_radius": 3,
		"content_margin": 8
	}


## Default minion content builder.
static func _build_minion_content(data: Dictionary, context) -> String:
	var minion_id: StringName = data.get("minion_id", &"")
	const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
	var minion = ChampionCatalogScript.get_minion(minion_id)
	if minion == null:
		return "Minion not found: %s" % minion_id
	
	var minion_data: Dictionary = minion.to_dict()
	var stats: Dictionary = minion_data.get("stats", {})
	var name: String = str(stats.get("name", minion_id))
	var hp: float = stats.get("max_hp", 0.0)
	var ad: float = stats.get("attack_damage", 0.0)
	var attack_speed: float = stats.get("attack_speed", 0.0)
	
	var lines: PackedStringArray = PackedStringArray()
	lines.append("[color=#b4b4b4]%s[/color]" % name)
	lines.append("HP: %.0f | AD: %.0f | AS: %.2f" % [hp, ad, attack_speed])
	
	return "\n".join(lines)


## Default status effect content builder.
static func _build_status_content(data: Dictionary, context) -> String:
	var effect_type: StringName = data.get("effect_type", &"")
	var unit_data: Dictionary = context.get_main_data(&"unit_data") if context else {}
	
	var duration: float = 0.0
	match effect_type:
		&"stun":
			duration = unit_data.get("stun_remaining", 0.0)
		&"silence":
			duration = unit_data.get("silence_remaining", 0.0)
		&"root":
			duration = unit_data.get("root_remaining", 0.0)
		&"taunt":
			duration = unit_data.get("taunt_remaining", 0.0)
		_:
			duration = 0.0
	
	if duration <= 0.0:
		return ""
	
	var color: String = "#e66666"  # Red default
	match effect_type:
		&"stun":
			color = "#e66666"
		&"silence":
			color = "#9966e6"
		&"root":
			color = "#e69966"
		&"taunt":
			color = "#e6e666"
	
	return "[color=%s]%s: %.1fs[/color]" % [color, str(effect_type).to_upper(), duration]


## Default modifier content builder.
static func _build_modifier_content(data: Dictionary, context) -> String:
	var stat_name: StringName = data.get("stat_name", &"")
	var value: float = data.get("value", 0.0)
	var is_buff: bool = data.get("is_buff", true)
	
	var color: String = "#66e666" if is_buff else "#e66666"
	var sign: String = "+" if value > 0 and is_buff else ""
	
	return "[color=%s]%s: %s%.2f[/color]" % [color, str(stat_name).to_upper(), sign, value]


## Default grid layout strategy.
static func _layout_grid(satellites: Array, origin: Vector2, spacing: Vector2 = Vector2(10, 10)) -> Array[Vector2]:
	var positions: Array[Vector2] = []
	var current_x: float = origin.x
	var current_y: float = origin.y + 200.0  # Offset below main tooltip
	var max_width: float = 600.0
	var row_width: float = 0.0
	
	for satellite in satellites:
		var panel: PanelContainer = satellite.get("panel")
		if panel == null:
			continue
		
		var size: Vector2 = panel.get_combined_minimum_size()
		
		# Check if we need to wrap to next row
		if row_width + size.x > max_width and row_width > 0:
			current_x = origin.x
			current_y += size.y + spacing.y
			row_width = 0.0
		
		positions.append(Vector2(current_x, current_y))
		current_x += size.x + spacing.x
		row_width += size.x + spacing.x
	
	return positions


## Default radial layout strategy.
static func _layout_radial(satellites: Array, origin: Vector2, radius: float = 200.0) -> Array[Vector2]:
	var positions: Array[Vector2] = []
	var count: int = satellites.size()
	if count == 0:
		return positions
	
	var angle_step: float = (2.0 * PI) / float(count)
	var start_angle: float = -PI / 2.0  # Start from top
	
	for i in range(count):
		var angle: float = start_angle + (angle_step * float(i))
		var x: float = origin.x + (radius * cos(angle))
		var y: float = origin.y + (radius * sin(angle))
		positions.append(Vector2(x, y))
	
	return positions


## Default stack layout strategy.
static func _layout_stack(satellites: Array, origin: Vector2, direction: Vector2 = Vector2(0, 1), spacing: float = 10.0) -> Array[Vector2]:
	var positions: Array[Vector2] = []
	var current_pos: Vector2 = origin + (direction * 200.0)  # Offset from main tooltip
	
	for satellite in satellites:
		positions.append(current_pos)
		current_pos += direction * spacing
	
	return positions


## Default custom layout strategy (user must provide their own).
static func _layout_custom(satellites: Array, origin: Vector2) -> Array[Vector2]:
	# This is a placeholder - users should register their own custom layout
	return []


## Create a minion satellite spec with default styling.
static func minion(
	minion_id: StringName,
	p_id: StringName = &"",
	p_position_hint: StringName = &"grid"
):
	_static_init()
	var spec_id := p_id if not p_id.is_empty() else StringName("minion_%s" % minion_id)
	var data_source := func(context) -> Dictionary:
		return {"minion_id": minion_id}
	var content_builder := func(data: Dictionary, context):
		return get_builder(&"minion").call(data, context)
	var style_config := get_style_preset(&"minion")
	return SatelliteSpecScript.new(
		spec_id,
		content_builder,
		data_source,
		style_config,
		p_position_hint
	)


## Create a status effect satellite spec with default styling.
static func status(
	effect_type: StringName,
	p_id: StringName = &"",
	p_position_hint: StringName = &"grid"
):
	_static_init()
	var spec_id := p_id if not p_id.is_empty() else StringName("status_%s" % effect_type)
	var data_source := func(context) -> Dictionary:
		return {"effect_type": effect_type}
	var content_builder := func(data: Dictionary, context):
		return get_builder(&"status").call(data, context)
	var style_config := get_style_preset(&"status")
	return SatelliteSpecScript.new(
		spec_id,
		content_builder,
		data_source,
		style_config,
		p_position_hint
	)


## Create a stat modifier satellite spec with default styling.
static func modifier(
	modifier_data: Dictionary,
	p_id: StringName = &"",
	p_position_hint: StringName = &"grid"
):
	_static_init()
	var spec_id := p_id if not p_id.is_empty() else StringName("modifier_%s" % modifier_data.get("stat_name", "unknown"))
	var data_source := func(context) -> Dictionary:
		return modifier_data
	var content_builder := func(data: Dictionary, context):
		return get_builder(&"modifier").call(data, context)
	var style_config := get_style_preset(&"modifier")
	return SatelliteSpecScript.new(
		spec_id,
		content_builder,
		data_source,
		style_config,
		p_position_hint
	)


## Create a custom satellite spec with full configuration.
static func custom(
	p_id: StringName,
	p_content_builder: Callable,
	p_data_source: Callable,
	p_style_config: Dictionary = {},
	p_position_hint: StringName = &"grid",
	p_on_show: Callable = Callable(),
	p_on_hide: Callable = Callable()
):
	return SatelliteSpecScript.new(
		p_id,
		p_content_builder,
		p_data_source,
		p_style_config,
		p_position_hint,
		p_on_show,
		p_on_hide
	)
