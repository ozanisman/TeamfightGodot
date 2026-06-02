#ifndef SIM_STATS_MODIFIERS_HPP
#define SIM_STATS_MODIFIERS_HPP

#include "simulation_types.hpp"

namespace sim {
namespace stats_modifiers {

void apply_stat_modifier(UnitState &source, UnitState &target, StringName stat_name, double additive, double multiplicative, double duration, bool is_match_duration);
void apply_simple_stat_modifier(UnitState &source, UnitState &target, StringName stat_name, double additive, double multiplicative, double duration, bool is_match_duration, const String &reason);
void apply_stacked_stat_modifier(UnitState &source, UnitState &target, StringName stat_name, double additive, double multiplicative, double duration, bool is_match_duration, int max_stacks, StackBehavior stack_behavior, const String &reason);
void clear_all_stat_modifiers(UnitState &unit);
void update_stat_modifier_durations(UnitState &unit, double delta);
void clear_expired_stat_modifiers(UnitState &unit);
String get_stack_key(StringName stat_name, const String &reason);
int consume_stat_stacks(UnitState &unit, StringName stat_name, const String &reason);
void set_stat_stacks(UnitState &unit, StringName stat_name, const String &reason, int stack_count, double duration, bool to_max, int fallback_max_stacks, double fallback_additive_per_stack, double fallback_multiplicative_per_stack);
void update_stacks(UnitState &unit, double delta, double current_time);
void cleanup_expired_stacks(UnitState &unit, double current_time);

} // namespace stats_modifiers
} // namespace sim

#endif // SIM_STATS_MODIFIERS_HPP
