#ifndef SIM_CONSTANTS_HPP
#define SIM_CONSTANTS_HPP

#include <cstdint>

namespace sim {

static constexpr double MATCH_DURATION = 60.0;
static constexpr double DEFAULT_TICK_RATE = 0.1;
static constexpr int64_t SUDDEN_DEATH_MAX_TICKS = 10000;
static constexpr double EPSILON = 0.000001;
static constexpr double RESPAWN_TIME = 10.0;
static constexpr double ASSIST_WINDOW = 5.0;
static constexpr double RETARGET_INTERVAL = 0.5;
static constexpr double SHIELD_DECAY_RATE = 0.1;
static constexpr double OVERTIME_DAMAGE_BASE_RATE = 0.0001;
// Increase rate currently unused.
static constexpr double OVERTIME_DAMAGE_INCREASE_RATE = 0.0001;
static constexpr double TARGET_SWITCH_LOCK_DURATION = 0.3;
static constexpr double TARGET_STICKINESS_THRESHOLD = 5.0;
static constexpr double STICKINESS_RETARGET_BONUS = 0.5;
static constexpr double REGEN_TICK_INTERVAL = 1.0;
static constexpr double CASTING_WINDUP = 0.5;
static constexpr double RANGED_THRESHOLD = 1.0;
static constexpr double MELEE_CONTACT_BUFFER = 0.1;
static constexpr double RANGED_CONTACT_BUFFER = 0.1;
static constexpr double DEFAULT_PROJECTILE_SPEED = 5.0;
static constexpr double DEFAULT_PROJECTILE_RADIUS = 0.03;
static constexpr double DEFAULT_PROJECTILE_STUN_DURATION = 0.0;
static constexpr int64_t SIMULATION_RULES_VERSION = 1;
static constexpr double WORLD_SIZE = 10.0;
static constexpr double WORLD_BOUNDARY_MIN = 0.2;
static constexpr double WORLD_BOUNDARY_MAX = 9.8;
static constexpr double BOUNDARY_DETECTION_MARGIN = 0.05;
static constexpr double RECOVERY_VELOCITY = 1.0;
static constexpr double SEPARATION_RADIUS_RANGED = 0.8;
static constexpr double SEPARATION_RADIUS_MELEE = 0.25;
static constexpr double SEPARATION_RANGE_THRESHOLD = 1.5;
static constexpr double NUDGE_SPEED_MODIFIER = 0.4;
static constexpr double KITE_SPEED_MODIFIER = 0.5;
static constexpr double KITE_DURATION = 1.0;
static constexpr double KITE_DANGER_THRESHOLD = 0.9;
/// Minimum effective slow multiplier so movement math stays stable at extreme slow percentages.
static constexpr double SLOW_MOVEMENT_MULTIPLIER_MIN = 0.05;
static constexpr double DRAFT_X_BASE = 0.9;
// Positions are stored as IEEE 754 double for deterministic arithmetic.
static constexpr double ALLY_CRITICAL_HP_RATIO = 0.35;
static constexpr double REACTIVE_PEEL_BONUS = 25.0;
static constexpr double ROLE_PRIORITY_GLOBAL_SCALE = 1.0;
static constexpr double SCORE_HP_WEIGHT_SCALE = 10.0;
static constexpr double SCORE_THREAT_WEIGHT_SCALE = 5.0;
static constexpr double SCORE_DISTANCE_WEIGHT_SCALE = 10.0;
static constexpr double SCORE_KITING_WEIGHT_SCALE = 1.5;
static constexpr double DISTANCE_EXPONENT = 1.5;
static constexpr double SPACING_EXPONENT = 1.5;
static constexpr double THREAT_RESPONSE_RANGE_FALLOFF = 0.6;
static constexpr double THREAT_BURST_THRESHOLD = 0.1;
static constexpr double THREAT_BURST_MULTIPLIER = 5.0;
static constexpr double THREAT_DECAY_DEFAULT = 2.0;
static constexpr double TARGET_SWITCH_MARGIN = 2.0;

// Random vertical spawning constants
static constexpr double SPAWN_Y_MIN = 3.0;
static constexpr double SPAWN_Y_MAX = 7.0;
static constexpr double RESPAWN_Y_MIN = 3.0; // Same as initial spawn
static constexpr double RESPAWN_Y_MAX = 7.0; // Same as initial spawn
static constexpr double PLAYER_SPAWN_X_BASE = 1.0;
static constexpr double ENEMY_SPAWN_X_BASE = 9.0;
static constexpr double SPAWN_Y_STEP = 0.5;
static constexpr double ASSASSIN_TANK_CONTEXT_PENALTY = 15.0;
static constexpr double EXECUTE_BONUS_WEIGHT_DEFAULT = 20.0;
static constexpr double PREY_INCOMING_TARGET_SCALE = 0.75;
static constexpr double PREY_PERCEIVED_THREAT_SCALE = 0.35;
static constexpr double PREY_FRONTLINE_SCALE = 0.0;
static constexpr double AOE_DENSITY_RADIUS = 2.0;
static constexpr double KITE_TARGET_WINDOW_MIN_FACTOR = 0.7;
static constexpr double KITE_TARGET_WINDOW_MAX_FACTOR = 1.3;
static constexpr double SWITCH_COMMIT_WINDOW_SECONDS = 0.18;
static constexpr double SWITCH_COMMIT_WINDOW_SWING_FRACTION = 0.25;
static constexpr double SUPPORT_PEEL_BOOST = 2.2;
static constexpr double SUPPORT_PEEL_THREAT_THRESHOLD = 2.0;
static constexpr double TARGET_EXECUTE_HP_RATIO = 0.25;
static constexpr double IN_RANGE_BONUS_DEFAULT = 0.6;
static constexpr double THREAT_RESPONSE_RANGE_DEFAULT = 0.0;
static constexpr double UNIT_COLLISION_RADIUS = 0.15;

static constexpr int SPATIAL_GRID_DIM = 8;
/// Broad-phase for targeting/density/kite only when a team has this many **alive** units (4+). Standard 5v5 can enter the spatial path under the current benchmark contract.
static constexpr int SPATIAL_BROAD_PHASE_TEAM_THRESHOLD = 4;
/// Separation ally scan uses a grid only at this team alive count or above (custom large teams); 5v5 uses brute O(n) with tiny n.
static constexpr int SPATIAL_SEPARATION_TEAM_THRESHOLD = 6;

static constexpr const char *CHAMPION_SCHEMA_PATH = "res://fixtures/goldens/champion_schema.json";
static constexpr const char *MINION_SCHEMA_PATH = "res://fixtures/goldens/minion_schema.json";
static constexpr const char *BALANCE_PATCHES_PATH = "res://fixtures/goldens/balance_patches.json";
static constexpr const char *CHAMPION_KITS_PATH = "res://fixtures/goldens/champion_kits.json";

} // namespace sim

#endif // SIM_CONSTANTS_HPP
