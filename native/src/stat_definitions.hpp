#ifndef STAT_DEFINITIONS_HPP
#define STAT_DEFINITIONS_HPP

#include <cstddef>

// Sentinel value for unbounded stats (no min/max clamping)
#define NO_BOUND -1e308

// Armor and magic resist are stored as integer points (22 = 22% reduction).
static constexpr double DEFENSE_PERCENT_SCALE = 100.0;

// Format: X(stat_name, default_value, min_value, max_value)
#define STAT_LIST \
    X(max_hp, 0.0, 1.0, NO_BOUND) \
    X(attack_damage, 0.0, 0.0, NO_BOUND) \
    X(attack_speed, 1.0, 0.0, NO_BOUND) \
    X(move_speed, 0.0, 0.0, NO_BOUND) \
    X(armor, 0.0, NO_BOUND, DEFENSE_PERCENT_SCALE) \
    X(magic_resist, 0.0, NO_BOUND, DEFENSE_PERCENT_SCALE) \
    X(tenacity, 0.0, NO_BOUND, 1.0) \
    X(life_steal, 0.0, 0.0, 10.0) \
    X(mana_cost, 50.0, 10.0, NO_BOUND) \
    X(mana_per_attack, 10.0, 0.0, NO_BOUND) \
    X(ability_cd, 5.0, 0.1, NO_BOUND) \
    X(projectile_speed, 5.0, 0.1, 50.0) \
    X(projectile_radius, 0.03, 0.01, 1.0) \
    X(respawn_time, 5.0, 0.1, 60.0) \
    X(attack_range, 0.3, 0.3, NO_BOUND) \
    X(cast_range, 0.0, 0.0, NO_BOUND)

// Generate enum for stat indexing
enum class StatIndex : int {
#define X(name, def, min_val, max_val) name,
    STAT_LIST
#undef X
    Count
};

// Generate array index helper
inline constexpr size_t stat_index(StatIndex idx) {
    return static_cast<size_t>(idx);
}

#endif // STAT_DEFINITIONS_HPP
