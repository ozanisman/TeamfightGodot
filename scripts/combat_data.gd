extends RefCounted
class_name CombatData

# =============================================================================
# 1. WORLD & ENGINE
# =============================================================================
const MATCH_DURATION := 60.0
const DEFAULT_WORLD_TICK_RATE := 0.1
const DEFAULT_MAX_SIM_TICKS := 4000
const RESPAWN_TIME := 10.0
const ASSIST_WINDOW := 5.0
const RETARGET_INTERVAL := 0.5
const WORLD_SIZE := 10.0
const WORLD_SIZE_VECTOR := Vector2(WORLD_SIZE, WORLD_SIZE)
const WORLD_BOUNDARY_MIN := 0.2
const WORLD_BOUNDARY_MAX := 9.8
const WORLD_CENTER := WORLD_SIZE / 2.0
const EPSILON := 0.000001
const SPATIAL_GRID_CELL_SIZE := 1.0
const CASTING_WINDUP := 0.5
const AOE_VISUAL_DURATION := 0.3
const REGEN_TICK_INTERVAL := 1.0

# =============================================================================
# 2. MOVEMENT & SPATIAL BEHAVIOR
# =============================================================================
const KITE_SPEED_MODIFIER := 0.5
const KITE_DURATION := 1.0
const NUDGE_SPEED_MODIFIER := 0.4
const BOUNDARY_DETECTION_MARGIN := 0.05
const ARENA_WALL_THICKNESS := 32.0
const RECOVERY_VELOCITY := 1.0
const SEPARATION_RADIUS_RANGED := 0.8
const SEPARATION_RADIUS_MELEE := 0.25
const SEPARATION_RANGE_THRESHOLD := 1.5
const KITE_DANGER_THRESHOLD := 0.9

# =============================================================================
# 3. GLOBAL TARGETING HEURISTICS
# =============================================================================
const TARGET_SWITCH_THRESHOLD := 0.9
const TARGET_SWITCH_MARGIN := 0.75
const TARGET_BUCKET_MARGIN := 0.75
const TARGET_SWITCH_LOCK_DURATION := 0.3
const STICKINESS_DEFAULT := 2.0
const THREAT_DECAY_DEFAULT := 2.0
const HP_WEIGHT_DPS_DEFAULT := 0.5
const DISTANCE_WEIGHT_DEFAULT := 1.0
const PROJECTILE_TIME_WEIGHT_DEFAULT := 0.0
const TANK_PENALTY_DEFAULT := 2.0

const ALLY_CRITICAL_HP_RATIO := 0.35
const TARGET_WOUNDED_HP_RATIO := 0.4
const TARGET_EXECUTE_HP_RATIO := 0.25
const TARGET_CARRY_PEEL_WEIGHT := 60.0
const PREY_INCOMING_TARGET_SCALE := 0.75
const PREY_PERCEIVED_THREAT_SCALE := 0.35
const PREY_FRONTLINE_SCALE := 0.35
const AOE_DENSITY_RADIUS := 2.0
const BODYGUARD_RADIUS := 2.5
const OBSCURANCE_DISTANCE_FACTOR := 0.8

const SCORE_HP_WEIGHT_SCALE := 10.0
const SCORE_THREAT_WEIGHT_SCALE := 5.0
const SCORE_DISTANCE_WEIGHT_SCALE := 3.0
const SCORE_KITING_WEIGHT_SCALE := 1.5
const DISTANCE_EXPONENT := 1.5
const SPACING_EXPONENT := 1.5
const ROLE_PRIORITY_GLOBAL_SCALE := 0.85
const IN_RANGE_SCORE_BONUS := 0.6

const KITE_TARGET_WINDOW_MIN_FACTOR := 0.7
const KITE_TARGET_WINDOW_MAX_FACTOR := 1.3

const SWITCH_COMMIT_WINDOW_SECONDS := 0.18
const SWITCH_COMMIT_WINDOW_SWING_FRACTION := 0.25

# --- Global Weights ---
const BODYGUARD_WEIGHT_TANK := 3.0
const BODYGUARD_WEIGHT_SUPPORT := 4.0
const OBSCURANCE_WEIGHT_DEFAULT := 4.5
const EXECUTE_BONUS_WEIGHT_DEFAULT := 50.0
const FLANKING_WEIGHT_ASSASSIN := 2.0
const OBSCURANCE_LINE_RADIUS := 0.35
const FLANKING_TEAM_CENTER_SCALE := 0.35

# =============================================================================
# 4. COMBAT MECHANICS & THRESHOLDS
# =============================================================================
const RANGED_THRESHOLD := 1.0
const MELEE_CONTACT_BUFFER := 0.00001
const THREAT_BURST_THRESHOLD := 0.10
const THREAT_BURST_MULTIPLIER := 5.0
const TAUNT_SCORE_BONUS := -100.0

# =============================================================================
# 5. ROLE-SPECIFIC TARGETING WEIGHTS
# =============================================================================
const STICKINESS_MARKSMAN := 5.0
const STICKINESS_SUPPORT := 1.0
const THREAT_DECAY_TANK := 4.0
const THREAT_DECAY_FIGHTER := 4.5
const THREAT_DECAY_FRAGILE := 1.0
const DISTANCE_WEIGHT_TANK := 1.5
const DISTANCE_WEIGHT_ASSASSIN := 0.15
const DISTANCE_WEIGHT_MAGE := 1.2
const DISTANCE_WEIGHT_SUPPORT := 1.5
const DISTANCE_WEIGHT_FIGHTER_CLOSE := 3.0

const HP_WEIGHT_ASSASSIN := 2.0
const HP_WEIGHT_MAGE := 2.5
const HP_WEIGHT_SUPPORT := 5.0
const CLUSTER_WEIGHT_TANK := 1.5
const CLUSTER_WEIGHT_MAGE := 3.0
const PREY_INSTINCT_WEIGHT_ASSASSIN := 2.0
const SPACING_WEIGHT_MARKSMAN := 2.5
const SPACING_WEIGHT_MAGE := 1.5
const SPACING_WEIGHT_SUPPORT := 1.0

# Role-aware targeting behavior knobs
const IN_RANGE_BONUS_TANK := 1.0
const IN_RANGE_BONUS_FIGHTER := 0.9
const IN_RANGE_BONUS_ASSASSIN := 0.6
const IN_RANGE_BONUS_MARKSMAN := 0.35
const IN_RANGE_BONUS_MAGE := 0.45
const IN_RANGE_BONUS_SUPPORT := 0.4

const TANK_PENALTY_TANK := 0.4
const TANK_PENALTY_FIGHTER := 1.0
const TANK_PENALTY_ASSASSIN := 7.0
const ASSASSIN_TANK_CONTEXT_PENALTY := 8.0
const TANK_PENALTY_MARKSMAN := 2.6
const TANK_PENALTY_MAGE := 2.3
const TANK_PENALTY_SUPPORT := 1.5

const THREAT_RESPONSE_TANK := 2.0
const THREAT_RESPONSE_FIGHTER := 1.8
const THREAT_RESPONSE_ASSASSIN := 0.8
const THREAT_RESPONSE_MARKSMAN := 1.0
const THREAT_RESPONSE_MAGE := 1.1
const THREAT_RESPONSE_SUPPORT := 1.7
const THREAT_RESPONSE_RANGE_FALLOFF := 0.6

# Supportive & Peeling Heuristics
const SUPPORT_PEEL_BOOST := 2.2
const SUPPORT_PEEL_THREAT_THRESHOLD := 2.0

# Projectile & Travel Time Weights
const PROJECTILE_TIME_WEIGHT_MARKSMAN := 0.35
const PROJECTILE_TIME_WEIGHT_MAGE := 0.45
const PROJECTILE_TIME_WEIGHT_SUPPORT := 0.3

# =============================================================================
# 6. ROLE PRIORITY MATRICES
# =============================================================================
const ROLE_PRIORITIES_TANK := {"assassin": -5.0, "fighter": -2.0}
const ROLE_PRIORITIES_ASSASSIN := {"marksman": -15.0, "mage": -15.0, "support": -10.0, "fighter": 10.0}
const ROLE_PRIORITIES_FIGHTER := {"marksman": -1.0, "mage": -1.0}
const ROLE_PRIORITIES_MAGE := {"marksman": -4.0, "support": -2.0}
const ROLE_PRIORITIES_SUPPORT := {"assassin": -8.0, "fighter": -4.0}

const ALLY_ROLE_PRIORITIES_TANK := {"marksman": -5.0, "mage": -5.0, "support": -3.0}
const ALLY_ROLE_PRIORITIES_SUPPORT := {"marksman": -5.0, "mage": -5.0, "fighter": -2.0}

const _THREAT_WEIGHT_SQUARED := SCORE_THREAT_WEIGHT_SCALE * SCORE_THREAT_WEIGHT_SCALE
const ALLY_THREAT_WEIGHT_TANK := _THREAT_WEIGHT_SQUARED
const ALLY_THREAT_WEIGHT_SUPPORT := _THREAT_WEIGHT_SQUARED

# =============================================================================
# 7. ROLE SYSTEMIC STAT MODIFIERS
# =============================================================================
const TANK_TENACITY_MOD := 0.20
const TANK_MANA_GAIN_DAMAGE_RATIO := 0.1
const FIGHTER_LIFESTEAL_MOD := 0.10
const FIGHTER_TENACITY_MOD := 0.10
const MARKSMAN_AS_MOD := 1.2
const ASSASSIN_MS_MOD := 1.5
const MAGE_MANA_REGEN_TICK := 1.0
const SUPPORT_ABILITY_CD_FLAT := 4.0

# =============================================================================
# 8. COMBAT DEFAULTS & ASSETS
# =============================================================================
const DEFAULT_PROJECTILE_SPEED := 5.0
const DEFAULT_PROJECTILE_RADIUS := 0.03
const DEFAULT_PROJECTILE_STUN_DURATION := 0.0
const DEFAULT_VISUAL_EFFECT_COLOR := [255, 255, 255]

# =============================================================================
# 9. UI & VISUALIZATION
# =============================================================================
const VIEWER_WIDTH := 1000
const VIEWER_HEIGHT := 800
const VIEWER_SIDEBAR_WIDTH := 280
const VIEWER_TARGET_FPS := 120
const VIEWER_WORLD_GRID_DIVISIONS := 10
const MELEE_HIT_EFFECT_RADIUS := 0.15
const MELEE_HIT_EFFECT_DURATION := 0.1
const PROJECTILE_CORE_RADIUS := 2
const DRAFT_X_BASE := 0.9
const DRAFT_X_STEP := 0.9
const DRAFT_Y_BASE := 1.2
const DRAFT_Y_STEP := 1.8
const AI_X_TANK := 6.0
const AI_X_FIGHTER := 7.0
const AI_X_OTHER := 8.0
const AI_X_RANGED := 9.0
const AI_Y_BASE := 1.5
const AI_Y_STEP := 1.5
const PREP_PLAYER_X_MAX := 4.5
const PREP_Y_MIN := 0.5
const PREP_Y_MAX := 9.5
const PREP_SNAP_GRID := 2.0
const SCOREBOARD_Y_START := 150
const SCOREBOARD_Y_STEP := 25

# =============================================================================
# 10. SIMULATION BATCH DEFAULTS
# =============================================================================
const SIMULATION_BASE_SEED := 42
const DEFAULT_SIMULATION_ROUNDS := 10000
const SIMULATION_MIN_SYNERGY_GAMES := 5
const SIMULATION_STATS_DIR := "stats_output"
const SIMULATION_TICK_RATE := 0.2
const SIMULATION_TEAM_SIZES := [1, 2, 3, 4, 5]
const SIMULATION_FORMATION_COLUMNS := 4
const SIMULATION_SEED_TEAM_OFFSET := 10000
const SIMULATION_PROGRESS_INTERVAL := 1000
const SIMULATION_MAP_CHUNKSIZE := 100
const CI_Z_95 := 1.96
const KDA_DEATHS_FLOOR := 0.1

# =============================================================================
# 11. DATA MODELS & LOADERS
# =============================================================================
const VALID_ROLES := ["tank", "fighter", "assassin", "marksman", "mage", "support"]

static func effective_attack_range(attack_range: float, ranged_threshold: float = RANGED_THRESHOLD, contact_buffer: float = MELEE_CONTACT_BUFFER) -> float:
	if attack_range <= ranged_threshold:
		return attack_range + contact_buffer
	return attack_range


static func is_melee_in_contact(distance: float, attack_range: float, ranged_threshold: float = RANGED_THRESHOLD, contact_buffer: float = MELEE_CONTACT_BUFFER) -> bool:
	if attack_range > ranged_threshold:
		return distance <= attack_range
	var effective_range := effective_attack_range(attack_range, ranged_threshold, contact_buffer)
	return distance <= effective_range or is_equal_approx(distance, effective_range)
