class_name UiTokens
extends RefCounted

# === Color Palette ===
const COLOR_BG := Color(0.078, 0.078, 0.102, 1.0)
const COLOR_PANEL := Color(0.11, 0.11, 0.149, 1.0)
const COLOR_GRID := Color(0.157, 0.157, 0.204, 1.0)
const COLOR_PLAYER := Color(0.275, 0.51, 1.0)
const COLOR_ENEMY := Color(0.882, 0.314, 0.314, 1.0)
const COLOR_TEXT := Color(0.902, 0.902, 0.902, 1.0)
const COLOR_SUBTLE := Color(0.706, 0.706, 0.745, 1.0)
const COLOR_BUTTON := Color(0.275, 0.51, 0.863, 1.0)
const COLOR_BUTTON_TEXT := Color(0.941, 0.941, 0.941, 1.0)
const COLOR_WARNING := Color(0.922, 0.667, 0.392, 1.0)
const COLOR_SUCCESS := Color(0.196, 0.902, 0.196, 1.0)
const COLOR_HIGHLIGHT := Color(0.471, 0.863, 0.549, 1.0)
const COLOR_SECTION_BG := Color(0.133, 0.133, 0.18, 1.0)
const COLOR_SECTION_BORDER := Color(0.275, 0.275, 0.353, 1.0)

# Bars
const COLOR_HP_BG := Color(0.18, 0.18, 0.2, 1.0)
const COLOR_HP_FILL := Color(0.31, 0.78, 0.31, 1.0)
const COLOR_MN_BG := Color(0.12, 0.14, 0.2, 1.0)
const COLOR_MN_FILL := Color(0.28, 0.48, 0.9, 1.0)
const COLOR_SHIELD_FILL := Color(1.0, 1.0, 1.0, 0.9)

# Stat diffs
const COLOR_STAT_BUFF := Color(0.4, 0.9, 0.4, 1.0)
const COLOR_STAT_NERF := Color(0.9, 0.4, 0.4, 1.0)

# Balance bar
const COLOR_DRAW := Color(0.55, 0.55, 0.55)
const COLOR_TRACK := Color(0.12, 0.12, 0.14, 1.0)

# Viewer-specific colors
const COLOR_ZONE_P1 := Color(0.2, 0.4, 0.8, 1.0)
const COLOR_ZONE_P2 := Color(0.8, 0.2, 0.2, 1.0)
const COLOR_TARGET_LINE := Color(0.824, 0.824, 0.431, 1.0)
const COLOR_BUTTON_DISABLED := Color(0.275, 0.275, 0.314, 1.0)
const COLOR_OVERTIME_BORDER := Color(1.0, 0.2, 0.2, 0.9)
const COLOR_MATCH_OVERLAY_BG := Color(0.08, 0.08, 0.1, 0.86)
const COLOR_OVERTIME_TEXT := Color(1.0, 0.3, 0.3)
const COLOR_TIMER_SHADOW := Color(0, 0, 0, 0.5)

# Damage type colors
const COLOR_DAMAGE_PHYSICAL := Color(0.9, 0.45, 0.45)
const COLOR_DAMAGE_MAGIC := Color(0.3, 0.6, 1.0)
const COLOR_DAMAGE_TRUE := Color(1.0, 1.0, 1.0)
const COLOR_SHIELD_TEXT := Color(0.55, 0.75, 1.0)

# AOE / FX colors
const COLOR_PROJECTILE_CORE := Color(0.95, 0.9, 0.45, 0.95)
const COLOR_MELEE_SLASH := Color(1.0, 0.9, 0.45, 0.85)
const COLOR_AOE_TAUNT := Color(0.72, 0.42, 0.95, 0.62)
const COLOR_AOE_SPLASH := Color(1.0, 0.78, 0.2, 0.75)
const COLOR_AOE_HOT := Color(0.2, 0.9, 0.3, 0.7)
const COLOR_AOE_PLAYER := Color(0.35, 0.55, 1.0, 0.58)
const COLOR_AOE_ENEMY := Color(1.0, 0.4, 0.35, 0.58)
const COLOR_AOE_NEUTRAL := Color(0.95, 0.5, 0.35, 0.5)
const COLOR_HOT_RING := Color(0.2, 0.9, 0.3, 0.85)

# Stats bar colors
const COLOR_SCALE_MID_TICK := Color(55.0 / 255.0, 55.0 / 255.0, 68.0 / 255.0)
const COLOR_DRAW_SEGMENT := Color(0.5, 0.5, 0.52, 1.0)
const COLOR_BAR_OUTLINE := Color(0.18, 0.18, 0.22, 1.0)
const COLOR_BAR_TRACK := Color(0.12, 0.12, 0.15, 1.0)
const COLOR_CI_LINE := Color(0.9, 0.9, 0.9)

# Unit node colors
const COLOR_UNIT_PLAYER := Color(0.275, 0.51, 1.0, 1.0)
const COLOR_UNIT_ENEMY := Color(0.882, 0.314, 0.314, 1.0)
const COLOR_UNIT_STUN_RING := Color(0.9, 0.65, 0.2, 0.9)
const COLOR_CASTING_ULT := Color(1.0, 0.86, 0.4)
const COLOR_CASTING_ABILITY := Color(0.47, 0.86, 0.55)
const COLOR_HP_BAR_BG := Color(0.31, 0.31, 0.31, 1.0)
const COLOR_HP_BAR_FILL := Color(0.31, 0.82, 0.31, 1.0)
const COLOR_MANA_BAR_FILL := Color(0.3, 0.55, 0.9, 0.9)
const COLOR_UNIT_LABEL := Color(0.95, 0.95, 0.95)

# Roster card colors
const COLOR_NAME_NEUTRAL := Color(0.82, 0.84, 0.88, 1.0)
const COLOR_KDA_TEXT := Color(0.78, 0.78, 0.8, 1.0)
const COLOR_BEHAVIOR_TEXT := Color(0.6, 0.8, 0.95, 1.0)
const COLOR_CASTING_TEXT := Color(0.9, 0.8, 0.4, 1.0)
const COLOR_ABILITY_READY := Color(0.4, 0.9, 0.4, 1.0)
const COLOR_ABILITY_TEXT := Color(0.78, 0.78, 0.8, 1.0)
const COLOR_RESPAWN_TEXT := Color(0.9, 0.72, 0.45, 1.0)
const COLOR_KDA_DEAD := Color(0.55, 0.5, 0.5, 1.0)
const COLOR_UNIT_DEAD_MODULATE := Color(0.75, 0.75, 0.8, 0.9)
const COLOR_ROLE_BLEND_BG := Color("#171821")

# Z-index layers
const Z_BACKGROUND := -1
const Z_AOE_FX := 20
const Z_ROSTER_TOOLTIP_CATCHER := 20
const Z_OVERTIME_BORDER := 40
const Z_MATCH_OVERLAY := 50
const Z_MELEE_SLASH := 5
const Z_CHART_TOOLTIP := 80
const Z_SATELLITE := 150
const Z_FLOATING_TEXT := 200
const Z_CATALOG_TOOLTIP := 200

# === Draft Screen Layout ===
# Overall margins
const DRAFT_EDGE_MARGIN_PX: int = 48
const DRAFT_TOP_MARGIN_PX: int = 16

# Section panels (player/bans panels on left and right sides)
const DRAFT_SECTION_TOP_PX: int = 82
const DRAFT_SECTION_W_PX: int = 150
const DRAFT_SECTION_H_PX: int = 260
const DRAFT_SECTION_GAP_PX: int = 16

# Action buttons (Random Draft / Start Match)
const DRAFT_ACTION_TOP_PX: int = 108
const DRAFT_ACTION_W_PX: int = 420
const DRAFT_ACTION_H_PX: int = 56
const DRAFT_ACTION_GAP_PX: int = 12

# Role filter buttons
const DRAFT_ROLE_TOP_PX: int = 400
const DRAFT_ROLE_W_PX: int = 250
const DRAFT_ROLE_H_PX: int = 75

# Champion grid (scroll container and tile buttons)
const DRAFT_CHAMPION_SCROLL_TOP_PX: int = 550
const DRAFT_CHAMPION_TILE_PX: int = 160
const DRAFT_CHAMPION_TILE_MAX_PX: int = 200
const DRAFT_CHAMPION_FONT_SIZE_PX: int = 20
const DRAFT_CHAMPION_GAP_PX: int = 30
const DRAFT_CHAMPION_MAX_COLUMNS: int = 9

# Recommendation panel (draft testing only)
const DRAFT_RECOMMENDATION_HEIGHT_RATIO: float = 0.32
