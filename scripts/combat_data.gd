extends RefCounted
class_name CombatData

const MATCH_DURATION := 60.0
const DEFAULT_WORLD_TICK_RATE := 0.1
const ASSIST_WINDOW := 5.0
const RETARGET_INTERVAL := 0.5
const WORLD_SIZE := Vector2(10.0, 10.0)
const WORLD_BOUNDARY_MIN := 0.2
const WORLD_BOUNDARY_MAX := 9.8
const WORLD_CENTER := 5.0
const EPSILON := 0.000001
const CASTING_WINDUP := 0.5
const REGEN_TICK_INTERVAL := 1.0

const KITE_SPEED_MODIFIER := 0.5
const KITE_DURATION := 1.0
const NUDGE_SPEED_MODIFIER := 0.4
const BOUNDARY_DETECTION_MARGIN := 0.05
const RECOVERY_VELOCITY := 1.0
const SEPARATION_RADIUS_RANGED := 0.8
const SEPARATION_RADIUS_MELEE := 0.25
const SEPARATION_RANGE_THRESHOLD := 1.5
const KITE_DANGER_THRESHOLD := 0.9

const TARGET_SWITCH_MARGIN := 0.75
const TARGET_SWITCH_LOCK_DURATION := 0.3
const STICKINESS_DEFAULT := 2.0
const THREAT_DECAY_DEFAULT := 2.0
const HP_WEIGHT_DPS_DEFAULT := 0.5
const DISTANCE_WEIGHT_DEFAULT := 1.0
const IN_RANGE_SCORE_BONUS := 0.6
const TANK_PENALTY_DEFAULT := 2.0
const TARGET_WOUNDED_HP_RATIO := 0.4
const TARGET_EXECUTE_HP_RATIO := 0.25
const SUPPORT_PEEL_THREAT_THRESHOLD := 2.0
const SUPPORT_PEEL_BOOST := 2.2
const TAUNT_SCORE_BONUS := -100.0

const RANGED_THRESHOLD := 1.0
const MELEE_CONTACT_BUFFER := 0.00001

const DEFAULT_PROJECTILE_SPEED := 5.0
const DEFAULT_PROJECTILE_RADIUS := 0.03
const DEFAULT_PROJECTILE_STUN_DURATION := 0.0

const RESPAWN_TIME := 10.0

static func effective_attack_range(attack_range: float) -> float:
	if attack_range <= RANGED_THRESHOLD:
		return attack_range + MELEE_CONTACT_BUFFER
	return attack_range


static func is_melee_in_contact(distance: float, attack_range: float) -> bool:
	if attack_range > RANGED_THRESHOLD:
		return distance <= attack_range
	var effective_range := effective_attack_range(attack_range)
	return distance <= effective_range or is_equal_approx(distance, effective_range)
