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
const DRAFT_CHAMPION_SCROLL_TOP_PX: int = 500
const DRAFT_CHAMPION_TILE_PX: int = 200
const DRAFT_CHAMPION_TILE_MAX_PX: int = 160
const DRAFT_CHAMPION_FONT_SIZE_PX: int = 20
const DRAFT_CHAMPION_GAP_PX: int = 30

# Recommendation panel (draft testing only)
const DRAFT_RECOMMENDATION_HEIGHT_RATIO: float = 0.10
