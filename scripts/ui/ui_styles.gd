class_name UiStyles
extends RefCounted

const Tokens := preload("res://scripts/ui/ui_tokens.gd")


static func fill_for_role(role_color: Color) -> Color:
	var h := role_color.h
	var s := clampf(role_color.s * 0.90, 0.52, 0.82)
	var v := clampf(role_color.v * 0.88, 0.56, 0.82)
	var yellow_bias := -0.06 * cos(TAU * (h - 0.16))
	v = clampf(v + yellow_bias, 0.52, 0.84)
	var fill := Color.from_hsv(h, s, v, role_color.a)
	fill = Tokens.COLOR_ROLE_BLEND_BG.lerp(fill, 0.90)
	var max_lum := 0.30
	var lum := fill.get_luminance()
	if lum > max_lum:
		var excess := lum - max_lum
		fill = fill.darkened(clampf(excess * 1.4, 0.02, 0.16))
	return fill


static func panel(bg_color: Color, border_color: Color, border_width: int = 2) -> StyleBoxFlat:
	var style := StyleBoxFlat.new()
	style.bg_color = bg_color
	style.border_color = border_color
	style.set_border_width_all(border_width)
	return style


static func solid(bg_color: Color) -> StyleBoxFlat:
	var style := StyleBoxFlat.new()
	style.bg_color = bg_color
	return style


static func bordered(bg_color: Color, border_color: Color, border_width: int = 3) -> StyleBoxFlat:
	var style := StyleBoxFlat.new()
	style.bg_color = bg_color
	style.border_color = border_color
	style.set_border_width_all(border_width)
	return style
