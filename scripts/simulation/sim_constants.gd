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
	"tank": Color8(215, 64, 64),
	"fighter": Color8(238, 132, 42),
	"assassin": Color8(180, 82, 220),
	"marksman": Color8(54, 168, 72),
	"mage": Color8(82, 170, 225),
	"support": Color8(226, 188, 66)
}
const ROLE_COLORS_OLD: Dictionary = {
	"tank": Color8(204, 51, 51),
	"fighter": Color8(210, 105, 30),
	"assassin": Color8(153, 50, 204),
	"marksman": Color8(34, 139, 34),
	"mage": Color8(76, 153, 204),
	"support": Color8(218, 165, 32),
}

# ========================================
# EFFECT METADATA
# ========================================
## Source of truth for effect metadata (color, category, description, etc.)
## Single dictionary for all effect properties for easy expansion
const EFFECT_METADATA: Dictionary = {
	"stun": {"color": "#ce88ee", "category": "CC", "description": "The target cannot move, attack, or cast abilities or ultimates."},
	"silence": {"color": "#ce88ee", "category": "CC", "description": "The target cannot cast abilities or ultimates. Affects both unless specified."},
	"root": {"color": "#ce88ee", "category": "CC", "description": "The target cannot move."},
	"taunt": {"color": "#ce88ee", "category": "CC", "description": "The target is forced to auto-attack the taunter and cannot cast abilities, ultimates, or kite."},
	"disarm": {"color": "#ce88ee", "category": "CC", "description": "The target cannot auto-attack."},
	"slow": {"color": "#ce88ee", "category": "CC", "description": "The target's movement speed is reduced by a percentage. Only the slow with the highest value is applied."},
	"knockback": {"color": "#ce88ee", "category": "CC", "description": "The target is pushed away from the source."},
	"reflect": {"color": "#ff9933", "category": "DEBUFF", "description": "A percentage of incoming damage is dealt back to the attacker."},
	"shield": {"color": "#66e666", "category": "UTILITY", "description": "Absorbs incoming damage before HP. Decays 1% of the current value per tick."},
	"stealth": {"color": "#66e666", "category": "UTILITY", "description": "The target cannot be targeted by enemies. Can ends prematurely when attacking or casting unless specified."},
	"heal": {"color": "#66e666", "category": "UTILITY", "description": "Restores HP to the target. Cannot increase a units HP over maximum unless specified."},
	"dodge": {"color": "#66e666", "category": "UTILITY", "description": "Chance to avoid incoming auto attacks, taking no damage."},
	"mana": {"color": "#66e666", "category": "UTILITY", "description": "Resource used to cast ultimates. By default 10 mana is generated per auto attack."},
	"dash": {"color": "#66e666", "category": "UTILITY", "description": "Rapidly moves the unit to a target location."},
	"summon": {"color": "#66e666", "category": "UTILITY", "description": "Creates allied units to fight alongside the caster. Their deaths do not count for score."},
	"physical damage": {"color": "#ff5555", "category": "DAMAGE", "description": "Reduced by the target's armor."},
	"magic damage": {"color": "#55aaff", "category": "DAMAGE", "description": "Reduced by the target's magic resist."},
	"true damage": {"color": "#eecfa1", "category": "DAMAGE", "description": "Ignores ALL of the target's defensive stats."}
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
# UNIT STATE STRINGS
# ========================================
# The 'state' field is a computed priority-based summary from native code.
# It represents the primary unit condition, prioritizing most restrictive effects.
# Priority order: DEAD > STUNNED > ROOTED > SILENCED > TAUNTED > REFLECTING > DISARMED > STEALTHED > SLOWED > CHANNELING > CASTING > KITING > ALIVE
const UNIT_STATE_STUNNED: String = "STUNNED"
const UNIT_STATE_DEAD: String = "DEAD"
const UNIT_STATE_KITING: String = "KITING"
const UNIT_STATE_ROOTED: String = "ROOTED"
const UNIT_STATE_SILENCED: String = "SILENCED"
const UNIT_STATE_TAUNTED: String = "TAUNTED"
const UNIT_STATE_REFLECTING: String = "REFLECTING"
const UNIT_STATE_DISARMED: String = "DISARMED"
const UNIT_STATE_STEALTHED: String = "STEALTHED"
const UNIT_STATE_SLOWED: String = "SLOWED"
const UNIT_STATE_CHANNELING: String = "CHANNELING"
const UNIT_STATE_CASTING: String = "CASTING"
const UNIT_STATE_ALIVE: String = "ALIVE"

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
const DRAFT_SEQUENCE: Array[String] = [
	"B_BAN", "R_BAN", "B_BAN", "R_BAN", "B_BAN", "R_BAN",
	"B_PICK", "R_PICK", "R_PICK", "B_PICK", "B_PICK", "R_PICK",
	"R_BAN", "B_BAN", "R_BAN", "B_BAN",
	"R_PICK", "B_PICK", "B_PICK", "R_PICK"
]
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

## Get all CC, DEBUFF, and UTILITY effect kinds from EFFECT_METADATA
static func get_effect_kinds() -> Array[StringName]:
	var kinds: Array[StringName] = []
	for key in EFFECT_METADATA:
		var metadata: Dictionary = EFFECT_METADATA[key]
		var category: String = metadata.get("category", "")
		if category in ["CC", "DEBUFF", "UTILITY"]:
			kinds.append(StringName(key))
	return kinds

## Get all damage types from EFFECT_METADATA (short names, e.g., "physical" not "physical damage")
static func get_damage_types() -> Array[StringName]:
	var types: Array[StringName] = []
	for key in EFFECT_METADATA:
		var metadata: Dictionary = EFFECT_METADATA[key]
		var category: String = metadata.get("category", "")
		if category == "DAMAGE":
			# Strip " damage" suffix to match existing usage
			var short_name: String = key.replace(" damage", "")
			types.append(StringName(short_name))
	return types

## Get base effect kind from AOE variant (e.g., "aoe_taunt" → "taunt")
static func get_base_effect_from_aoe(aoe_kind: StringName) -> StringName:
	if str(aoe_kind).begins_with("aoe_"):
		return StringName(str(aoe_kind).substr(4))
	return aoe_kind

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


## Map draft sequence index to global pick position (1-10) for snake draft order
## Returns 0 if the sequence index is a ban phase, otherwise returns 1-10 for picks
static func get_global_pick_position(sequence_index: int) -> int:
	if sequence_index < 0 or sequence_index >= DRAFT_SEQUENCE.size():
		return 0
	
	var turn: String = DRAFT_SEQUENCE[sequence_index]
	if not turn.ends_with("_PICK"):
		return 0  # Ban phase
	
	# Count picks before this index to determine global position
	var pick_count := 0
	for i in range(sequence_index):
		if DRAFT_SEQUENCE[i].ends_with("_PICK"):
			pick_count += 1
	
	return pick_count + 1  # 1-indexed
