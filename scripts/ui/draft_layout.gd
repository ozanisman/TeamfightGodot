class_name DraftLayout
extends RefCounted

const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const Tokens := preload("res://scripts/ui/ui_tokens.gd")


static func apply_full_rect(control: Control) -> void:
	control.set_anchors_preset(Control.PRESET_FULL_RECT)
	control.offset_left = 0.0
	control.offset_top = 0.0
	control.offset_right = 0.0
	control.offset_bottom = 0.0


static func apply_title_layout(label: Label) -> void:
	label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	label.set_anchors_preset(Control.PRESET_TOP_WIDE)
	label.offset_left = 0.0
	label.offset_top = Tokens.DRAFT_TOP_MARGIN_PX
	label.offset_right = 0.0
	label.offset_bottom = Tokens.DRAFT_TOP_MARGIN_PX + 28.0


static func apply_turn_layout(label: Label) -> void:
	label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	label.set_anchors_preset(Control.PRESET_TOP_WIDE)
	label.offset_left = 0.0
	label.offset_top = Tokens.DRAFT_TOP_MARGIN_PX + 32.0
	label.offset_right = 0.0
	label.offset_bottom = Tokens.DRAFT_TOP_MARGIN_PX + 58.0


static func apply_back_button_layout(button: Button) -> void:
	button.custom_minimum_size = Vector2(120, 40)
	button.set_anchors_preset(Control.PRESET_TOP_RIGHT)
	button.offset_left = -150.0
	button.offset_top = Tokens.DRAFT_TOP_MARGIN_PX
	button.offset_right = -Tokens.DRAFT_EDGE_MARGIN_PX
	button.offset_bottom = Tokens.DRAFT_TOP_MARGIN_PX + 40.0


static func apply_debug_label_layout(label: Label) -> void:
	label.set_anchors_preset(Control.PRESET_TOP_RIGHT)
	label.offset_left = -320.0
	label.offset_top = Tokens.DRAFT_TOP_MARGIN_PX + 48.0
	label.offset_right = -Tokens.DRAFT_EDGE_MARGIN_PX
	label.offset_bottom = Tokens.DRAFT_TOP_MARGIN_PX + 72.0


static func apply_section_panel_layout(panel: Control, side: StringName, slot: int) -> void:
	var top := float(Tokens.DRAFT_SECTION_TOP_PX)
	if side == &"left":
		panel.set_anchors_preset(Control.PRESET_TOP_LEFT)
		panel.offset_left = Tokens.DRAFT_EDGE_MARGIN_PX + slot * (Tokens.DRAFT_SECTION_W_PX + Tokens.DRAFT_SECTION_GAP_PX)
		panel.offset_right = panel.offset_left + Tokens.DRAFT_SECTION_W_PX
	else:
		panel.set_anchors_preset(Control.PRESET_TOP_RIGHT)
		panel.offset_right = -Tokens.DRAFT_EDGE_MARGIN_PX - (1 - slot) * (Tokens.DRAFT_SECTION_W_PX + Tokens.DRAFT_SECTION_GAP_PX)
		panel.offset_left = panel.offset_right - Tokens.DRAFT_SECTION_W_PX
	panel.offset_top = top
	panel.offset_bottom = top + Tokens.DRAFT_SECTION_H_PX


static func apply_section_header_layout(label: Label) -> void:
	label.set_anchors_preset(Control.PRESET_TOP_WIDE)
	label.offset_left = 10.0
	label.offset_top = 8.0
	label.offset_right = -10.0
	label.offset_bottom = 34.0


static func apply_section_list_layout(list: Control) -> void:
	list.set_anchors_preset(Control.PRESET_FULL_RECT)
	list.offset_left = 10.0
	list.offset_top = 44.0
	list.offset_right = -10.0
	list.offset_bottom = -10.0


static func apply_role_filter_layout(row: HBoxContainer) -> void:
	row.set_anchors_preset(Control.PRESET_TOP_WIDE)
	row.offset_left = Tokens.DRAFT_EDGE_MARGIN_PX
	row.offset_top = Tokens.DRAFT_ROLE_TOP_PX
	row.offset_right = -Tokens.DRAFT_EDGE_MARGIN_PX
	row.offset_bottom = Tokens.DRAFT_ROLE_TOP_PX + Tokens.DRAFT_ROLE_H_PX
	row.alignment = BoxContainer.ALIGNMENT_CENTER
	row.add_theme_constant_override("separation", Tokens.DRAFT_CHAMPION_GAP_PX)


static func apply_action_box_layout(box: VBoxContainer, screen_size: Vector2) -> void:
	var action_margin: float = maxf(Tokens.DRAFT_EDGE_MARGIN_PX, (screen_size.x - Tokens.DRAFT_ACTION_W_PX) * 0.5)
	box.set_anchors_preset(Control.PRESET_TOP_WIDE)
	box.offset_left = action_margin
	box.offset_top = Tokens.DRAFT_ACTION_TOP_PX
	box.offset_right = -action_margin
	box.offset_bottom = Tokens.DRAFT_ACTION_TOP_PX + Tokens.DRAFT_ACTION_H_PX * 2 + Tokens.DRAFT_ACTION_GAP_PX
	box.add_theme_constant_override("separation", Tokens.DRAFT_ACTION_GAP_PX)


static func apply_champion_scroll_layout(scroll: ScrollContainer, screen_size: Vector2, has_recommendation_panel: bool = false) -> void:
	scroll.set_anchors_preset(Control.PRESET_FULL_RECT)
	scroll.offset_left = Tokens.DRAFT_EDGE_MARGIN_PX
	scroll.offset_top = Tokens.DRAFT_CHAMPION_SCROLL_TOP_PX
	scroll.offset_right = -Tokens.DRAFT_EDGE_MARGIN_PX
	if has_recommendation_panel:
		scroll.offset_bottom = -screen_size.y * Tokens.DRAFT_RECOMMENDATION_HEIGHT_RATIO - 12.0
	else:
		scroll.offset_bottom = -12.0
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED


static func apply_recommendation_panel_layout(panel: Control, screen_size: Vector2) -> void:
	panel.set_anchors_preset(Control.PRESET_BOTTOM_WIDE)
	panel.offset_left = 0.0
	panel.offset_top = -screen_size.y * Tokens.DRAFT_RECOMMENDATION_HEIGHT_RATIO
	panel.offset_right = 0.0
	panel.offset_bottom = 0.0


static func calculate_champion_tile_size(champion_ids: Array[StringName]) -> int:
	var font: Font = ThemeDB.fallback_font
	var max_text_w: float = 0.0
	for champion_id in champion_ids:
		var champion: Variant = ChampionCatalogScript.get_champion(champion_id)
		var champion_name: String = String(champion_id)
		if champion != null:
			var champion_dict: Dictionary = champion.to_dict()
			var stats_dict: Dictionary = champion_dict.get("stats", {})
			champion_name = String(stats_dict.get("name", String(champion_id)))
		var text_w: float = font.get_string_size(
			champion_name,
			HORIZONTAL_ALIGNMENT_CENTER,
			-1.0,
			Tokens.DRAFT_CHAMPION_FONT_SIZE_PX
		).x
		max_text_w = maxf(max_text_w, text_w)
	var content_px: int = ceili(max_text_w) + 24
	return clampi(maxi(Tokens.DRAFT_CHAMPION_TILE_PX, content_px), Tokens.DRAFT_CHAMPION_TILE_PX, Tokens.DRAFT_CHAMPION_TILE_MAX_PX)


static func calculate_grid_columns(view_width: float, tile_size: int, max_columns: int = Tokens.DRAFT_CHAMPION_MAX_COLUMNS) -> int:
	var available_width: float = maxf(tile_size, view_width - float(Tokens.DRAFT_EDGE_MARGIN_PX * 2))
	return mini(max_columns, maxi(1, int((available_width + Tokens.DRAFT_CHAMPION_GAP_PX) / (tile_size + Tokens.DRAFT_CHAMPION_GAP_PX))))
