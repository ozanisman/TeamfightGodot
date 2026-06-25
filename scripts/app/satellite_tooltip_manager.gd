class_name SatelliteTooltipManager
extends Node
## Manages satellite tooltip lifecycle, panel pooling, and positioning.

const SatelliteSpecScript := preload("res://scripts/app/satellite_spec.gd")
const SatelliteContextScript := preload("res://scripts/app/satellite_context.gd")
const UiTokensScript := preload("res://scripts/ui/ui_tokens.gd")
var SatelliteRegistriesScript = null

var _ui_parent: Control
var _satellite_pool: Dictionary[StringName, PanelContainer] = {}  # spec_id -> PanelContainer
var _active_satellites: Dictionary = {}  # spec_id -> {spec, panel, data}
var _origin: Vector2 = Vector2.ZERO
var _context: SatelliteContext
var _tooltip_size: Vector2 = Vector2.ZERO

const VIEWPORT_MARGIN := 4.0


func _ready() -> void:
	process_mode = Node.PROCESS_MODE_INHERIT
	if SatelliteRegistriesScript == null:
		SatelliteRegistriesScript = load("res://scripts/app/satellite_registries.gd")
	SatelliteRegistriesScript._static_init()


## Setup the manager with a UI layer.
func setup(ui_layer: Control) -> void:
	_ui_parent = ui_layer


## Show satellites with given specs at the specified origin.
func show_satellites(specs: Array[SatelliteSpec], origin: Vector2, context: SatelliteContext, tooltip_size: Vector2 = Vector2.ZERO) -> void:
	_origin = origin
	_context = context
	_tooltip_size = tooltip_size
	
	# Hide existing satellites that are not in the new specs
	var new_spec_ids: Array[StringName] = []
	for spec in specs:
		new_spec_ids.append(spec.id)
	
	for spec_id in _active_satellites.keys():
		if not spec_id in new_spec_ids:
			_hide_satellite(spec_id)
	
	# Show or create satellites for new specs
	for spec in specs:
		_show_satellite(spec)
	
	# Position all satellites
	_position_satellites()


## Hide all satellites.
func hide_satellites() -> void:
	for spec_id in _active_satellites.keys():
		_hide_satellite(spec_id)
	_active_satellites.clear()


## Update satellites with fresh context.
func update_satellites(context: SatelliteContext) -> void:
	_context = context
	
	for spec_id in _active_satellites.keys():
		var satellite_data: Dictionary = _active_satellites[spec_id]
		var spec: SatelliteSpec = satellite_data.spec
		var panel: PanelContainer = satellite_data.panel
		
		# Get fresh data
		var data: Dictionary = spec.data_source.call(_context)
		
		# Build content
		var content = spec.content_builder.call(data, _context)
		
		# Update panel content
		_update_panel_content(panel, content)
		
		# Reset size and reposition
		panel.reset_size()
	
	_position_satellites()


## Update the origin point for satellite positioning.
func update_origin(new_origin: Vector2) -> void:
	_origin = new_origin
	_position_satellites()


## Show a single satellite (create or reuse panel).
func _show_satellite(spec: SatelliteSpec) -> void:
	var spec_id: StringName = spec.id
	var panel: PanelContainer
	
	# Check if satellite already exists
	if _active_satellites.has(spec_id):
		var satellite_data: Dictionary = _active_satellites[spec_id]
		panel = satellite_data.panel
		panel.z_index = UiTokensScript.Z_SATELLITE  # Below main tooltip
	else:
		# Get or create panel
		panel = _get_or_create_panel(spec)
	
	# Get initial data
	var data: Dictionary = spec.data_source.call(_context)
	
	# Build content
	var content = spec.content_builder.call(data, _context)
	
	# Apply style
	_apply_style_to_panel(panel, spec.style_config)
	
	# Set content
	_update_panel_content(panel, content)
	
	# Call on_show callback
	if spec.on_show.is_valid():
		spec.on_show.call(panel, data, _context)
	
	# Make visible
	panel.visible = true
	panel.reset_size()
	
	# Track as active
	_active_satellites[spec_id] = {
		"spec": spec,
		"panel": panel,
		"data": data
	}


## Hide a single satellite.
func _hide_satellite(spec_id: StringName) -> void:
	if not _active_satellites.has(spec_id):
		return
	
	var satellite_data: Dictionary = _active_satellites[spec_id]
	var spec: SatelliteSpec = satellite_data.spec
	var panel: PanelContainer = satellite_data.panel
	
	# Call on_hide callback
	if spec.on_hide.is_valid():
		spec.on_hide.call(panel, satellite_data.data, _context)
	
	# Hide panel
	panel.visible = false
	
	# Remove from active
	_active_satellites.erase(spec_id)


## Get or create a panel for a spec.
func _get_or_create_panel(spec: SatelliteSpec) -> PanelContainer:
	var spec_id: StringName = spec.id
	
	# Check pool
	if _satellite_pool.has(spec_id):
		var panel: PanelContainer = _satellite_pool[spec_id]
		if is_instance_valid(panel):
			return panel
	
	# Create new panel
	var panel := PanelContainer.new()
	panel.name = "Satellite_%s" % spec_id
	panel.mouse_filter = Control.MOUSE_FILTER_IGNORE
	panel.z_index = UiTokensScript.Z_SATELLITE  # Below main tooltip
	_ui_parent.add_child(panel)
	
	# Add to pool
	_satellite_pool[spec_id] = panel
	
	return panel


## Apply style config to a panel.
func _apply_style_to_panel(panel: PanelContainer, style_config: Dictionary) -> void:
	var style := StyleBoxFlat.new()
	
	var border_color: Color = style_config.get("border_color", UiTokensScript.COLOR_SUBTLE)
	var border_width: int = style_config.get("border_width", 1)
	var bg_color: Color = style_config.get("bg_color", UiTokensScript.COLOR_PANEL)
	var corner_radius: int = style_config.get("corner_radius", 3)
	var content_margin: int = style_config.get("content_margin", 8)
	var font_size: int = style_config.get("font_size", 18)
	var min_width: int = style_config.get("min_width", 400)
	
	style.bg_color = bg_color
	style.set_border_width_all(border_width)
	style.border_color = border_color
	style.set_corner_radius_all(corner_radius)
	style.set_content_margin_all(content_margin)
	
	panel.set_meta("font_size", font_size)
	panel.set_meta("min_width", min_width)
	
	panel.add_theme_stylebox_override("panel", style)


## Update panel content (BBCode or Control node).
func _update_panel_content(panel: PanelContainer, content) -> void:
	# Clear existing children
	for child in panel.get_children():
		panel.remove_child(child)
		child.queue_free()
	
	if content is String:
		# BBCode content - create RichTextLabel
		var rich_text := RichTextLabel.new()
		rich_text.bbcode_enabled = true
		rich_text.scroll_active = false
		rich_text.fit_content = true
		rich_text.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		rich_text.text = content
		
		var font_size: int = panel.get_meta("font_size", 18)
		rich_text.add_theme_font_size_override("normal_font_size", font_size)
		rich_text.add_theme_font_size_override("bold_font_size", font_size)
		rich_text.add_theme_font_size_override("italics_font_size", font_size)
		
		var min_width: int = panel.get_meta("min_width", 400)
		rich_text.custom_minimum_size.x = min_width
		
		panel.add_child(rich_text)
	elif content is Control:
		# Custom Control node
		panel.add_child(content)
		content.set_anchors_preset(Control.PRESET_FULL_RECT)


## Position all satellites using their layout strategies.
func _position_satellites() -> void:
	if _active_satellites.is_empty():
		return
	
	# Group satellites by position hint
	var by_hint: Dictionary = {}
	for spec_id in _active_satellites.keys():
		var satellite_data: Dictionary = _active_satellites[spec_id]
		var spec: SatelliteSpec = satellite_data.spec
		var hint: StringName = spec.position_hint
		
		if not by_hint.has(hint):
			by_hint[hint] = []
		by_hint[hint].append(satellite_data)
	
	# Position each group
	for hint in by_hint.keys():
		var satellites: Array = by_hint[hint]
		var layout: Callable = SatelliteRegistriesScript.get_layout(hint)
		
		if not layout.is_valid():
			continue
		
		# Prepare satellite array for layout
		var satellite_array: Array = []
		for satellite_data in satellites:
			satellite_array.append({
				"panel": satellite_data.panel,
				"spec": satellite_data.spec
			})
		
		# Get viewport size for dynamic positioning
		var viewport := get_viewport()
		var viewport_rect: Rect2 = viewport.get_visible_rect() if viewport else Rect2(Vector2.ZERO, Vector2(1920, 1080))
		var viewport_size: Vector2 = viewport_rect.size
		
		# Calculate positions
		var positions: Array[Vector2] = layout.call(satellite_array, _origin, Vector2(10, 10), viewport_size, _tooltip_size)
		var clamped_positions: Array[Vector2] = _clamp_position_group_to_viewport(positions, satellites, viewport_rect)
		
		# Apply positions
		for i in range(satellites.size()):
			if i >= clamped_positions.size():
				break
			var satellite_data: Dictionary = satellites[i]
			var panel: PanelContainer = satellite_data.panel
			panel.global_position = clamped_positions[i]


func _clamp_position_group_to_viewport(positions: Array[Vector2], satellites: Array, viewport_rect: Rect2) -> Array[Vector2]:
	if positions.is_empty():
		return positions

	var has_bounds := false
	var min_pos := Vector2.ZERO
	var max_pos := Vector2.ZERO
	for i in range(mini(positions.size(), satellites.size())):
		var satellite_data: Dictionary = satellites[i]
		var panel: PanelContainer = satellite_data.panel
		if panel == null:
			continue
		var pos: Vector2 = positions[i]
		var size: Vector2 = panel.get_combined_minimum_size()
		var end_pos: Vector2 = pos + size
		if not has_bounds:
			min_pos = pos
			max_pos = end_pos
			has_bounds = true
		else:
			min_pos.x = minf(min_pos.x, pos.x)
			min_pos.y = minf(min_pos.y, pos.y)
			max_pos.x = maxf(max_pos.x, end_pos.x)
			max_pos.y = maxf(max_pos.y, end_pos.y)

	if not has_bounds:
		return positions

	var delta := Vector2.ZERO
	var left_bound: float = viewport_rect.position.x + VIEWPORT_MARGIN
	var top_bound: float = viewport_rect.position.y + VIEWPORT_MARGIN
	var right_bound: float = viewport_rect.end.x - VIEWPORT_MARGIN
	var bottom_bound: float = viewport_rect.end.y - VIEWPORT_MARGIN

	if max_pos.x > right_bound:
		delta.x = right_bound - max_pos.x
	if min_pos.x + delta.x < left_bound:
		delta.x = left_bound - min_pos.x

	if max_pos.y > bottom_bound:
		delta.y = bottom_bound - max_pos.y
	if min_pos.y + delta.y < top_bound:
		delta.y = top_bound - min_pos.y

	var clamped_positions: Array[Vector2] = []
	for pos in positions:
		clamped_positions.append(pos + delta)
	return clamped_positions
