#ifndef SIM_EFFECTS_EXEC_HPP
#define SIM_EFFECTS_EXEC_HPP

#include "sim_world.hpp"

namespace sim::effects::execution {

struct SimExecCallbacks {
	void *user_data = nullptr;

	void (*run_post_attack_effects)(void *user_data, UnitState &source, UnitState &target, double damage, const EffectContext &context) = nullptr;
	void (*run_post_heal_effects)(void *user_data, UnitState &source, UnitState &target, double heal_amount, double heal_gained, const EffectContext &context) = nullptr;
	void (*push_projectile)(void *user_data, const ProjectileState &projectile) = nullptr;
	int (*consume_stat_stacks)(void *user_data, UnitState &unit, StringName stat_name, const String &reason) = nullptr;
	void (*set_stat_stacks)(void *user_data, UnitState &unit, StringName stat_name, const String &reason, int stack_count, double duration, bool to_max, int fallback_max_stacks, double fallback_additive_per_stack, double fallback_multiplicative_per_stack) = nullptr;
	String (*get_stack_key)(void *user_data, StringName stat_name, const String &reason) = nullptr;
	void (*process_channel_tick)(void *user_data, UnitState &unit, double delta) = nullptr;
	std::vector<UnitState *> (*select_targets)(void *user_data, UnitState &source, UnitState *target, int64_t target_count, TargetSelectionStrategy strategy, bool include_source, ExcessTargetHandling excess_handling, const StringName &team_filter) = nullptr;
	Vector2 (*find_random_spawn_position_near_excluding_with_expansion)(void *user_data, double center_x, double center_y, double initial_radius, double max_radius, int64_t exclude_instance_id, const std::vector<Vector2> &pending_positions) = nullptr;
	void (*queue_pending_spawn)(void *user_data, const PendingSpawn &pending) = nullptr;
	int64_t (*get_max_instance_id)(void *user_data) = nullptr;
	void (*set_max_instance_id)(void *user_data, int64_t value) = nullptr;
	Dictionary (*get_minion_data)(void *user_data, const StringName &minion_id) = nullptr;
	Vector2 (*find_valid_dash_position)(void *user_data, double tx, double ty, double new_x, double new_y, double effective_distance, int64_t target_instance_id) = nullptr;
	void (*apply_stacked_stat_modifier)(void *user_data, UnitState &source, UnitState &target, StringName stat_name, double additive, double multiplicative, double duration, bool is_match_duration, int max_stacks, StackBehavior stack_behavior, const String &reason) = nullptr;
	void (*apply_simple_stat_modifier)(void *user_data, UnitState &source, UnitState &target, StringName stat_name, double additive, double multiplicative, double duration, bool is_match_duration, const String &reason) = nullptr;
	bool (*debug_combat_trace)(void *user_data) = nullptr;
};

Dictionary execute(const EffectRecord &effect, EffectContext &context, SimWorld &world, SimHostCallbacks &host, const SimExecCallbacks &hooks);

} // namespace sim::effects::execution

#endif // SIM_EFFECTS_EXEC_HPP
