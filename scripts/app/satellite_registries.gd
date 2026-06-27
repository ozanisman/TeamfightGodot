extends Node
## Global registries for satellite content builders, layout strategies, and style presets.

const SatelliteSpecScript := preload("res://scripts/app/satellite_spec.gd")
const SimConstants := preload("res://scripts/simulation/sim_constants.gd")
const UiTokensScript := preload("res://scripts/ui/ui_tokens.gd")

static var _content_builders: Dictionary = {}
static var _layout_strategies: Dictionary = {}
static var _style_presets: Dictionary = {}
static var _initialized: bool = false
static var _compiled_regexes: Dictionary[String, RegEx] = {}


static func _escape_bbcode_plain(text: String) -> String:
	return str(text).replace("[", "[lb]").replace("]", "[rb]")


## Color keywords in text using BBCode tags.
## Case-insensitive, matches full words containing keywords.
static func _color_keywords_in_text(text: String) -> String:
	var result: String = _escape_bbcode_plain(text)
	
	# Lazily compile regexes on first call
	if _compiled_regexes.is_empty():
		for keyword in SimConstants.EFFECT_METADATA:
			var regex := RegEx.new()
			regex.compile("(?i)\\b([a-zA-Z]*%s[a-zA-Z]*)\\b" % keyword)
			_compiled_regexes[keyword] = regex
	
	for keyword in SimConstants.EFFECT_METADATA:
		var metadata: Dictionary = SimConstants.EFFECT_METADATA[keyword]
		var color: String = metadata.get("color", SimConstants.EFFECT_COLOR_UNKNOWN)
		var regex: RegEx = _compiled_regexes[keyword]
		result = regex.sub(result, "[color=%s]$1[/color]" % color, true)
	
	return result


## Initialize default builders, layouts, and styles.
static func _static_init() -> void:
	if _initialized:
		return
	_initialized = true
	
	# Register default content builders
	register_builder(&"minion", _build_minion_content)
	register_builder(&"status", _build_status_content)
	register_builder(&"modifier", _build_modifier_content)
	register_builder(&"damage_type", _build_damage_type_content)
	
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
		"border_color": UiTokensScript.COLOR_SUBTLE,
		"border_width": 1,
		"bg_color": UiTokensScript.COLOR_PANEL,
		"font_size": 18,
		"min_width": 475,
		"corner_radius": 3,
		"content_margin": 8
	}
	
	# Status style - effect-colored borders, compact
	_style_presets[&"status"] = {
		"border_color": UiTokensScript.COLOR_STAT_NERF,
		"border_width": 1,
		"bg_color": UiTokensScript.COLOR_PANEL,
		"font_size": 16,
		"min_width": 200,
		"corner_radius": 3,
		"content_margin": 6
	}
	
	# Modifier style - buff/nerf colored
	_style_presets[&"modifier"] = {
		"border_color": UiTokensScript.COLOR_STAT_BUFF,
		"border_width": 1,
		"bg_color": UiTokensScript.COLOR_PANEL,
		"font_size": 16,
		"min_width": 250,
		"corner_radius": 3,
		"content_margin": 6
	}
	
	# Neutral style
	_style_presets[&"neutral"] = {
		"border_color": UiTokensScript.COLOR_SUBTLE,
		"border_width": 1,
		"bg_color": UiTokensScript.COLOR_PANEL,
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
	
	var lines: PackedStringArray = PackedStringArray()
	lines.append("[color=%s]%s (MINION)[/color]" % [SimConstants.MINION_TITLE_COLOR, name])
	
	# Description
	var description: String = str(minion_data.get("description", ""))
	if not description.is_empty():
		lines.append(_color_keywords_in_text(description))
		lines.append("")  # Line break
	
	# Core stats
	var hp: float = stats.get("max_hp", 0.0)
	var ad: float = stats.get("attack_damage", 0.0)
	var attack_speed: float = stats.get("attack_speed", 0.0)
	var range: float = stats.get("attack_range", 0.0)
	var ms: float = stats.get("move_speed", 0.0)
	lines.append("HP: %.0f | AD: %.0f | AS: %.2f | Range: %.1f | MS: %.1f" % [hp, ad, attack_speed, range, ms])
	
	# Defensive stats
	var armor: float = stats.get("armor", 0.0)
	var ward: float = stats.get("ward", 0.0)
	var tenacity: float = stats.get("tenacity", 0.0) * 100
	var lifesteal: float = stats.get("life_steal", 0.0) * 100
	lines.append("Armor: %.0f%% | Ward: %.0f%% | Tenacity: %.0f%% | Lifesteal: %.0f%%" % [armor, ward, tenacity, lifesteal])
	
	# Passive
	var passive_name: String = str(minion_data.get("passive_name", ""))
	var passive_desc: String = str(minion_data.get("passive_desc", ""))
	if not passive_name.is_empty() or not passive_desc.is_empty():
		lines.append("")
		lines.append("%s (passive): %s" % [passive_name if not passive_name.is_empty() else "Passive", _color_keywords_in_text(passive_desc)])
	
	# Ability
	var ability_name: String = str(minion_data.get("ability_name", ""))
	var ability_desc: String = str(minion_data.get("ability_desc", ""))
	var ability_cd: float = stats.get("ability_cd", 0.0)
	if not ability_name.is_empty() or not ability_desc.is_empty():
		lines.append("")
		var cd_str: String = "%.1fs" % ability_cd if ability_cd > 0 else ""
		lines.append("%s (%s): %s" % [ability_name if not ability_name.is_empty() else "Ability", cd_str, _color_keywords_in_text(ability_desc)])
	
	# Ultimate
	var ultimate_name: String = str(minion_data.get("ultimate_name", ""))
	var ultimate_desc: String = str(minion_data.get("ultimate_desc", ""))
	var mana_cost: float = stats.get("mana_cost", 0.0)
	if not ultimate_name.is_empty() or not ultimate_desc.is_empty():
		lines.append("")
		var mana_str: String = "%.0f" % mana_cost if mana_cost > 0 else ""
		lines.append("%s (%s mana): %s" % [ultimate_name if not ultimate_name.is_empty() else "Ultimate", mana_str, _color_keywords_in_text(ultimate_desc)])
	
	return "\n".join(lines)


## Default status effect content builder.
static func _build_status_content(data: Dictionary, context) -> String:
	var effect_type: StringName = data.get("effect_type", &"")
	var unit_data: Dictionary = context.get_main_data(&"unit_data") if context else {}
	
	# Use SimConstants.EFFECT_METADATA for color, category, and description
	var metadata: Dictionary = SimConstants.EFFECT_METADATA.get(String(effect_type), {"color": SimConstants.EFFECT_COLOR_ERROR, "category": "EFFECT", "description": ""})
	var description: String = metadata.get("description", "")
	
	var duration: float = 0.0
	var has_duration: bool = false
	
	# Try to get duration from unit_data (in-game)
	if not unit_data.is_empty():
		match effect_type:
			&"stun":
				duration = unit_data.get("stun_remaining", 0.0)
			&"silence":
				duration = unit_data.get("silence_remaining", 0.0)
			&"root":
				duration = unit_data.get("root_remaining", 0.0)
			&"taunt":
				duration = unit_data.get("taunt_remaining", 0.0)
			&"reflect":
				duration = unit_data.get("reflect_remaining", 0.0)
			&"disarm":
				duration = unit_data.get("disarm_remaining", 0.0)
			&"slow":
				duration = unit_data.get("slow_remaining", 0.0)
			&"stealth":
				duration = unit_data.get("stealth_remaining", 0.0)
			&"shield":
				duration = unit_data.get("shield_remaining", 0.0)
			_:
				duration = 0.0
		has_duration = duration > 0.0
	
	# In draft phase, show CC effect without duration
	# In-game, only show if duration > 0
	if not unit_data.is_empty() and not has_duration:
		return ""
	
	# Get color and category from metadata (already declared above)
	var color: String = metadata.get("color", SimConstants.EFFECT_COLOR_ERROR)
	var effect_category: String = metadata.get("category", "EFFECT")
	# Capitalize to title case, CC becomes "Crowd Control"
	if effect_category == "CC":
		effect_category = "Crowd Control"
	else:
		effect_category = effect_category.capitalize()
	
	var lines: PackedStringArray = []
	if has_duration:
		lines.append("[color=%s]%s (%s): %.1fs[/color]" % [color, str(effect_type).to_upper(), effect_category, duration])
	else:
		lines.append("[color=%s]%s (%s)[/color]" % [color, str(effect_type).to_upper(), effect_category])
	
	if not description.is_empty():
		lines.append(_color_keywords_in_text(description))
	
	return "\n".join(lines)


## Default modifier content builder.
static func _build_modifier_content(data: Dictionary, context) -> String:
	var stat_name: StringName = data.get("stat_name", &"")
	var value: float = data.get("value", 0.0)
	var is_buff: bool = data.get("is_buff", true)
	
	var color: String = UiTokensScript.COLOR_STAT_BUFF.to_html(false) if is_buff else UiTokensScript.COLOR_STAT_NERF.to_html(false)
	var sign: String = "+" if value > 0 and is_buff else ""
	
	return "[color=%s]%s: %s%.2f[/color]" % [color, str(stat_name).to_upper(), sign, value]


## Default damage type content builder.
static func _build_damage_type_content(data: Dictionary, context) -> String:
	var damage_type: StringName = data.get("damage_type", &"")
	
	# Use SimConstants.EFFECT_METADATA for color, category, and description
	var metadata: Dictionary = SimConstants.EFFECT_METADATA.get(String(damage_type), {"color": SimConstants.EFFECT_COLOR_UNKNOWN, "category": "DAMAGE", "description": ""})
	var color: String = metadata.get("color", SimConstants.EFFECT_COLOR_UNKNOWN)
	var effect_category: String = metadata.get("category", "DAMAGE")
	# Capitalize to title case
	effect_category = effect_category.capitalize()
	var description: String = metadata.get("description", "")
	
	var type_name: String = str(damage_type).to_upper()
	
	var lines: PackedStringArray = []
	lines.append("[color=%s]%s (%s)[/color]" % [color, type_name, effect_category])
	if not description.is_empty():
		lines.append(_color_keywords_in_text(description))
	
	return "\n".join(lines)


## Default grid layout strategy.
static func _layout_grid(satellites: Array, origin: Vector2, spacing: Vector2 = Vector2(10, 10), viewport_size: Vector2 = Vector2(1920, 1080), tooltip_size: Vector2 = Vector2.ZERO) -> Array[Vector2]:
	var positions: Array[Vector2] = []
	
	# Get satellite panel width for positioning
	var satellite_width: float = 0.0
	if not satellites.is_empty():
		var first_panel: PanelContainer = satellites[0].get("panel")
		if first_panel != null:
			satellite_width = first_panel.get_combined_minimum_size().x
	
	# Determine offset direction based on mouse position
	# If mouse is on right half, position satellites to the left of tooltip
	# If mouse is on left half, position satellites to the right of tooltip
	var gap: float = 10.0  # Gap between tooltip and satellites
	var offset_x: float
	if origin.x > viewport_size.x / 2:
		# Position to the left: satellites end at tooltip start
		# So satellites start at: origin.x - satellite_width - gap
		offset_x = -(satellite_width + gap) if tooltip_size.x > 0 else -450.0
	else:
		# Position to the right: satellites start at tooltip end
		# So satellites start at: origin.x + tooltip_width + gap
		offset_x = tooltip_size.x + gap if tooltip_size.x > 0 else 450.0
	
	var current_x: float = origin.x + offset_x
	var current_y: float = origin.y
	
	for satellite in satellites:
		var panel: PanelContainer = satellite.get("panel")
		if panel == null:
			continue
		
		var size: Vector2 = panel.get_combined_minimum_size()
		
		positions.append(Vector2(current_x, current_y))
		current_y += size.y + spacing.y
	
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
## Returns null if the minion has no content (empty description, passive, ability, and ultimate).
static func minion(
	minion_id: StringName,
	p_id: StringName = &"",
	p_position_hint: StringName = &"grid"
):
	_static_init()
	const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
	var minion = ChampionCatalogScript.get_minion(minion_id)
	if minion == null:
		return null
	
	var minion_data: Dictionary = minion.to_dict()
	
	# Check if minion has any meaningful content
	var has_description: bool = not str(minion_data.get("description", "")).is_empty()
	var has_passive: bool = not str(minion_data.get("passive_name", "")).is_empty() or not str(minion_data.get("passive_desc", "")).is_empty()
	var has_ability: bool = not str(minion_data.get("ability_name", "")).is_empty() or not str(minion_data.get("ability_desc", "")).is_empty()
	var has_ultimate: bool = not str(minion_data.get("ultimate_name", "")).is_empty() or not str(minion_data.get("ultimate_desc", "")).is_empty()
	
	# Skip if minion has no content (stats alone don't justify a tooltip)
	if not has_description and not has_passive and not has_ability and not has_ultimate:
		return null
	
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


## Create a damage type satellite spec with default styling.
static func damage_type(
	damage_type: StringName,
	p_id: StringName = &"",
	p_position_hint: StringName = &"grid"
):
	_static_init()
	var spec_id := p_id if not p_id.is_empty() else StringName("damage_type_%s" % damage_type)
	var data_source := func(context) -> Dictionary:
		return {"damage_type": damage_type}
	var content_builder := func(data: Dictionary, context):
		return get_builder(&"damage_type").call(data, context)
	var style_config := get_style_preset(&"status")  # Reuse status style
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
