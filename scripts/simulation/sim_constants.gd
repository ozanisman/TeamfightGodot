class_name SimConstants
extends RefCounted

const SIMULATION_RULES_VERSION: int = 1

const MATCH_DURATION: float = 60.0
const DEFAULT_TICK_RATE: float = 0.1
const DEFAULT_MAX_SIM_TICKS: int = 4000
const RESPAWN_TIME: float = 10.0
const ASSIST_WINDOW: float = 5.0
const RETARGET_INTERVAL: float = 0.5
const WORLD_SIZE: float = 10.0
const WORLD_BOUNDARY_MIN: float = 0.2
const WORLD_BOUNDARY_MAX: float = 9.8
const WORLD_CENTER: float = WORLD_SIZE / 2.0
const EPSILON: float = 0.000001
const SPATIAL_GRID_CELL_SIZE: float = 1.0
const CASTING_WINDUP: float = 0.5
const AOE_VISUAL_DURATION: float = 0.3
## When native tick_fx omits r, use this for aoe_splash (matches demolition in champion_schema).
const VIEWER_AOE_FALLBACK_SPLASH_RADIUS_WORLD: float = 0.5
## AoE edge + fill stay readable a bit longer than floaters.
const AOE_VISUAL_MAX_DURATION: float = 0.55
const REGEN_TICK_INTERVAL: float = 1.0

const KITE_SPEED_MODIFIER: float = 0.5
const KITE_DURATION: float = 1.0
const NUDGE_SPEED_MODIFIER: float = 0.4
const BOUNDARY_DETECTION_MARGIN: float = 0.05
const RECOVERY_VELOCITY: float = 1.0
const SEPARATION_RADIUS_RANGED: float = 0.8
const SEPARATION_RADIUS_MELEE: float = 0.25
const SEPARATION_RANGE_THRESHOLD: float = 1.5
const KITE_DANGER_THRESHOLD: float = 0.9

const TARGET_SWITCH_THRESHOLD: float = 0.9
const TARGET_SWITCH_MARGIN: float = 0.75
const TARGET_BUCKET_MARGIN: float = 0.75
const TARGET_SWITCH_LOCK_DURATION: float = 0.3
const STICKINESS_DEFAULT: float = 2.0
const THREAT_DECAY_DEFAULT: float = 2.0
const HP_WEIGHT_DPS_DEFAULT: float = 0.5
const DISTANCE_WEIGHT_DEFAULT: float = 1.0
const PROJECTILE_TIME_WEIGHT_DEFAULT: float = 0.0
const TANK_PENALTY_DEFAULT: float = 2.0

const ALLY_CRITICAL_HP_RATIO: float = 0.35
const TARGET_WOUNDED_HP_RATIO: float = 0.4
const TARGET_EXECUTE_HP_RATIO: float = 0.25
const TARGET_CARRY_PEEL_WEIGHT: float = 60.0
const PREY_INCOMING_TARGET_SCALE: float = 0.75
const PREY_PERCEIVED_THREAT_SCALE: float = 0.35
const PREY_FRONTLINE_SCALE: float = 0.35
const AOE_DENSITY_RADIUS: float = 2.0
const BODYGUARD_RADIUS: float = 2.5
const OBSCURANCE_DISTANCE_FACTOR: float = 0.8

const SCORE_HP_WEIGHT_SCALE: float = 10.0
const SCORE_THREAT_WEIGHT_SCALE: float = 5.0
const SCORE_DISTANCE_WEIGHT_SCALE: float = 3.0
const SCORE_KITING_WEIGHT_SCALE: float = 1.5
const DISTANCE_EXPONENT: float = 1.5
const SPACING_EXPONENT: float = 1.5
const ROLE_PRIORITY_GLOBAL_SCALE: float = 0.85
const IN_RANGE_SCORE_BONUS: float = 0.6

const KITE_TARGET_WINDOW_MIN_FACTOR: float = 0.7
const KITE_TARGET_WINDOW_MAX_FACTOR: float = 1.3
const SWITCH_COMMIT_WINDOW_SECONDS: float = 0.18
const SWITCH_COMMIT_WINDOW_SWING_FRACTION: float = 0.25

const BODYGUARD_WEIGHT_TANK: float = 3.0
const BODYGUARD_WEIGHT_SUPPORT: float = 4.0
const OBSCURANCE_WEIGHT_DEFAULT: float = 4.5
const EXECUTE_BONUS_WEIGHT_DEFAULT: float = 50.0
const FLANKING_WEIGHT_ASSASSIN: float = 2.0
const OBSCURANCE_LINE_RADIUS: float = 0.35
const FLANKING_TEAM_CENTER_SCALE: float = 0.35

const RANGED_THRESHOLD: float = 1.0
const MELEE_CONTACT_BUFFER: float = 0.00001
const THREAT_BURST_THRESHOLD: float = 0.1
const THREAT_BURST_MULTIPLIER: float = 5.0
const TAUNT_SCORE_BONUS: float = -100.0

const STICKINESS_MARKSMAN: float = 5.0
const STICKINESS_SUPPORT: float = 1.0
const THREAT_DECAY_TANK: float = 4.0
const THREAT_DECAY_FIGHTER: float = 4.5
const THREAT_DECAY_FRAGILE: float = 1.0
const DISTANCE_WEIGHT_TANK: float = 1.5
const DISTANCE_WEIGHT_ASSASSIN: float = 0.15
const DISTANCE_WEIGHT_MAGE: float = 1.2
const DISTANCE_WEIGHT_SUPPORT: float = 1.5
const DISTANCE_WEIGHT_FIGHTER_CLOSE: float = 3.0

const HP_WEIGHT_ASSASSIN: float = 2.0
const HP_WEIGHT_MAGE: float = 2.5
const HP_WEIGHT_SUPPORT: float = 5.0
const CLUSTER_WEIGHT_TANK: float = 1.5
const CLUSTER_WEIGHT_MAGE: float = 3.0
const PREY_INSTINCT_WEIGHT_ASSASSIN: float = 2.0
const SPACING_WEIGHT_MARKSMAN: float = 2.5
const SPACING_WEIGHT_MAGE: float = 1.5
const SPACING_WEIGHT_SUPPORT: float = 1.0

const IN_RANGE_BONUS_TANK: float = 1.0
const IN_RANGE_BONUS_FIGHTER: float = 0.9
const IN_RANGE_BONUS_ASSASSIN: float = 0.6
const IN_RANGE_BONUS_MARKSMAN: float = 0.35
const IN_RANGE_BONUS_MAGE: float = 0.45
const IN_RANGE_BONUS_SUPPORT: float = 0.4

const TANK_PENALTY_TANK: float = 0.4
const TANK_PENALTY_FIGHTER: float = 1.0
const TANK_PENALTY_ASSASSIN: float = 7.0
const ASSASSIN_TANK_CONTEXT_PENALTY: float = 8.0
const TANK_PENALTY_MARKSMAN: float = 2.6
const TANK_PENALTY_MAGE: float = 2.3
const TANK_PENALTY_SUPPORT: float = 1.5

const THREAT_RESPONSE_TANK: float = 2.0
const THREAT_RESPONSE_FIGHTER: float = 1.8
const THREAT_RESPONSE_ASSASSIN: float = 0.8
const THREAT_RESPONSE_MARKSMAN: float = 1.0
const THREAT_RESPONSE_MAGE: float = 1.1
const THREAT_RESPONSE_SUPPORT: float = 1.7
const THREAT_RESPONSE_RANGE_FALLOFF: float = 0.6

const ROLE_PRIORITIES_TANK: Dictionary = {
	&"assassin": -5.0,
	&"fighter": -2.0,
}

const ROLE_PRIORITIES_ASSASSIN: Dictionary = {
	&"marksman": -15.0,
	&"mage": -15.0,
	&"support": -10.0,
	&"fighter": 10.0,
}

const ROLE_PRIORITIES_FIGHTER: Dictionary = {
	&"marksman": -1.0,
	&"mage": -1.0,
}

const ROLE_PRIORITIES_MAGE: Dictionary = {
	&"marksman": -4.0,
	&"support": -2.0,
}

const ROLE_PRIORITIES_SUPPORT: Dictionary = {
	&"assassin": -8.0,
	&"fighter": -4.0,
}

const ALLY_ROLE_PRIORITIES_TANK: Dictionary = {
	&"marksman": -5.0,
	&"mage": -5.0,
	&"support": -3.0,
}

const ALLY_ROLE_PRIORITIES_SUPPORT: Dictionary = {
	&"marksman": -5.0,
	&"mage": -5.0,
	&"fighter": -2.0,
}

const SUPPORT_PEEL_BOOST: float = 2.2
const SUPPORT_PEEL_THREAT_THRESHOLD: float = 2.0

const PROJECTILE_TIME_WEIGHT_MARKSMAN: float = 0.35
const PROJECTILE_TIME_WEIGHT_MAGE: float = 0.45
const PROJECTILE_TIME_WEIGHT_SUPPORT: float = 0.3

const TANK_TENACITY_MOD: float = 0.2
const TANK_MANA_GAIN_DAMAGE_RATIO: float = 0.1
const FIGHTER_LIFESTEAL_MOD: float = 0.1
const FIGHTER_TENACITY_MOD: float = 0.1
const MARKSMAN_AS_MOD: float = 1.2
const ASSASSIN_MS_MOD: float = 1.5
const MAGE_MANA_REGEN_TICK: float = 1.0
const SUPPORT_ABILITY_CD_FLAT: float = 4.0

const DEFAULT_PROJECTILE_SPEED: float = 5.0
const DEFAULT_PROJECTILE_RADIUS: float = 0.03
const DEFAULT_PROJECTILE_STUN_DURATION: float = 0.0
const DEFAULT_VISUAL_EFFECT_COLOR: Color = Color(1.0, 1.0, 1.0)

const VIEWER_WIDTH: int = 1000
const VIEWER_HEIGHT: int = 800
const VIEWER_SIDEBAR_WIDTH: int = 280
const VIEWER_TARGET_FPS: int = 120
const VIEWER_WORLD_GRID_DIVISIONS: int = 10
const MELEE_HIT_EFFECT_RADIUS: float = 0.15
const MELEE_HIT_EFFECT_DURATION: float = 0.1
const PROJECTILE_CORE_RADIUS: int = 2
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

const SIMULATION_BASE_SEED: int = 42
const DEFAULT_SIMULATION_ROUNDS: int = 10000
const SIMULATION_MIN_SYNERGY_GAMES: int = 5
const SIMULATION_STATS_DIR: StringName = &"stats_output"
const SIMULATION_TICK_RATE: float = 0.2
const SIMULATION_TEAM_SIZES: Array[int] = [1, 2, 3, 4, 5]
const SIMULATION_FORMATION_COLUMNS: int = 4
const SIMULATION_SEED_TEAM_OFFSET: int = 10000
const SIMULATION_PROGRESS_INTERVAL: int = 1000
const SIMULATION_MAP_CHUNKSIZE: int = 100
const CI_Z_95: float = 1.96
const KDA_DEATHS_FLOOR: float = 0.1

static func effective_attack_range(attack_range: float) -> float:
	if attack_range <= RANGED_THRESHOLD:
		return attack_range + MELEE_CONTACT_BUFFER
	return attack_range

static func is_melee_in_contact(distance: float, attack_range: float) -> bool:
	if attack_range > RANGED_THRESHOLD:
		return distance <= attack_range

	var effective_range: float = effective_attack_range(attack_range)
	return distance <= effective_range or is_equal_approx(distance, effective_range)

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
