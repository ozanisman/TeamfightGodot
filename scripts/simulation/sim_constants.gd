class_name SimConstants
extends RefCounted

# ========================================
# VERSION
# ========================================
const SIMULATION_RULES_VERSION: int = 1

# ========================================
# WORLD & SIMULATION
# ========================================
const DEFAULT_TICK_RATE: float = 0.1
const RESPAWN_TIME: float = 5.0
const WORLD_SIZE: float = 10.0
const EPSILON: float = 0.000001
const CASTING_WINDUP: float = 0.5
const AOE_VISUAL_MAX_DURATION: float = 0.30
## Floor (screen px) for drawing small world radii (e.g. demolition 0.5) so sub-pixel rings are not dropped.
const VIEWER_AOE_MIN_RING_RADIUS_PX: float = 2.0

# ========================================
# ROLE MODIFIERS
# ========================================
const TANK_MANA_GAIN_DAMAGE_RATIO: float = 0.1
const MAGE_MANA_REGEN_TICK: float = 1.0

const STAT_MOD_TYPES: Dictionary = {
	&"tenacity": "add",
	&"life_steal": "add",
	&"ability_cd": "add",
	&"attack_speed": "multiply",
	&"move_speed": "multiply",
}

# ========================================
# ROLE COLORS
# ========================================
## Source of truth for role colors used across all UI components
## Color8 format (0-255 RGB values)
const ROLE_COLORS: Dictionary = {
	"tank": Color8(204, 51, 51),
	"fighter": Color8(210, 105, 30),
	"assassin": Color8(153, 50, 204),
	"marksman": Color8(34, 139, 34),
	"mage": Color8(76, 153, 204),
	"support": Color8(218, 165, 32),
}

# ========================================
# PROJECTILE DEFAULTS
# ========================================
const DEFAULT_PROJECTILE_SPEED: float = 5.0
const DEFAULT_PROJECTILE_RADIUS: float = 0.03

# ========================================
# VIEWER CONFIGURATION
# ========================================
const SIMULATION_VIEWER_MAX_TICKS_PER_FRAME: int = 48
const VIEWER_WORLD_GRID_DIVISIONS: int = 10

# ========================================
# VIEWER PROJECTILE SPREAD
# ========================================
const VIEWER_PROJECTILE_SPREAD_NUM_SLOTS: int = 16  # Number of radial positions
const VIEWER_PROJECTILE_SPREAD_BASE_RADIUS: float = 15.0  # Base spread radius in pixels
const VIEWER_PROJECTILE_SPREAD_RADIUS_VARIATION: float = 3.0  # Random radius variation
const VIEWER_PROJECTILE_SPREAD_ANGLE_VARIATION: float = 0.5  # Random angle variation in radians

# ========================================
# DRAFT & POSITIONING
# ========================================
const DRAFT_X_BASE: float = 0.9
const DRAFT_X_STEP: float = 0.9
const DRAFT_Y_BASE: float = 1.2
const DRAFT_Y_STEP: float = 1.8
const AI_X_TANK: float = 6.0
const AI_X_FIGHTER: float = 7.0
const AI_X_OTHER: float = 8.0
const AI_X_RANGED: float = 9.0
const AI_Y_BASE: float = 1.5
const AI_Y_STEP: float = 1.5
const PREP_PLAYER_X_MAX: float = 4.5
const PREP_Y_MIN: float = 0.5
const PREP_Y_MAX: float = 9.5
const PREP_SNAP_GRID: float = 2.0
const SCOREBOARD_Y_START: int = 150
const SCOREBOARD_Y_STEP: int = 25

# ========================================
# SIMULATION CONFIGURATION
# ========================================
const SIMULATION_TICK_RATE: float = 0.2
const SIMULATION_TEAM_SIZES: Array[int] = [1, 2, 3, 4, 5]
const SIMULATION_FORMATION_COLUMNS: int = 4
## Must match native summary [code]telemetry.schema[/code] for [member UnitReplaySummary.telemetry].
const SIM_TELEMETRY_SCHEMA_V1: String = "teamfight.telemetry.v1"

# ========================================
# HELPER FUNCTIONS
# ========================================

static func spawn_position(index: int, team: StringName) -> Vector2:
	var column: int = index / SIMULATION_FORMATION_COLUMNS
	var row: int = index % SIMULATION_FORMATION_COLUMNS
	var x: float
	if team == &"player":
		x = DRAFT_X_BASE + (float(column) * DRAFT_X_STEP)
	else:
		x = WORLD_SIZE - DRAFT_X_BASE - (float(column) * DRAFT_X_STEP)
	var y: float = DRAFT_Y_BASE + (float(row) * DRAFT_Y_STEP)
	return Vector2(x, y)


## Largest centered square in viewport; [0, WORLD_SIZE]^2 maps to that region (letterbox / pillarbox).
static func viewer_battle_square_side(vp: Vector2) -> float:
	return minf(vp.x, vp.y)


static func viewer_battle_square_offset(vp: Vector2) -> Vector2:
	var s: float = viewer_battle_square_side(vp)
	return Vector2((vp.x - s) * 0.5, (vp.y - s) * 0.5)
