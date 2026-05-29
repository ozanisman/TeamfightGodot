#include "sim_effects_exec.hpp"
#include "sim_effects_exec_internal.hpp"

#include "sim_effect_kinds.inl.hpp"

#include "sim_channel.hpp"
#include "sim_constants.hpp"
#include "sim_movement.hpp"
#include "sim_spatial.hpp"
#include "sim_stats.hpp"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector2.hpp>

#define X(name, def, min_val, max_val) using sim::get_effective_##name;
STAT_LIST
#undef X

namespace sim::effects::execution {
namespace internal {

using namespace sim::effect_kinds;

Dictionary exec_status_channel(
		const EffectRecord &effect,
		EffectContext &context,
		SimWorld &world,
		SimHostCallbacks &host,
		const SimExecCallbacks &hooks,
		UnitState &source,
		UnitState *target) {
	switch (effect.opcode) {
		case EFFECT_OPCODE_CHANNEL: {
			Dictionary channel_result;
			channel_result["success"] = true;

			if (uc(world, source).is_channeling) {
				UtilityFunctions::push_error("Unit is already channeling");
				if (context.action_kind == sn_ability()) {
					source.ability_cooldown = get_effective_ability_cd(source);
				}
				channel_result["success"] = false;
				return channel_result;
			}

			UnitStateCold &cold = uc(world, source);
			cold.is_channeling = true;
			cold.channel_remaining_duration = effect.scalar0 + effect.scalar1;
			cold.channel_tick_interval = effect.scalar1;
			cold.channel_allow_movement = effect.int0 != 0;
			cold.channel_target_mode = effect.string0;
			cold.channel_reason = effect.reason;
			cold.channel_action_kind = context.action_kind;
			cold.channel_source_instance_id = source.instance_id;
			cold.channel_tick_accumulator = 0.0;
			cold.channel_accumulated_damage = 0.0;

			if (target != nullptr) {
				cold.channel_target_instance_id = target->instance_id;
			}

			if (!effect.children.empty()) {
				cold.channel_sub_effect = effect.children[0];
				if (cold.channel_sub_effect.opcode == 0) {
					String error_msg = "Channel sub-effect has invalid opcode. Channel reason: ";
					error_msg += cold.channel_reason;
					UtilityFunctions::push_error(error_msg);
					channel_result["success"] = false;
					return channel_result;
				}
			} else {
				String error_msg = "Channel missing required sub-effect. Channel reason: ";
				error_msg += cold.channel_reason;
				UtilityFunctions::push_error(error_msg);
				channel_result["success"] = false;
				return channel_result;
			}

			if (effect.sub_effects.size() > 0) {
				cold.channel_post_complete_effect = effect.sub_effects[0];
			}

			if (effect.sub_effects.size() > 1) {
				cold.channel_post_interrupt_effect = effect.sub_effects[1];
			}

			channel::ChannelHostHooks channel_hooks;
			channel_hooks.user_data = host.user_data;
			channel_hooks.debug_combat_trace = hooks.debug_combat_trace;
			channel::process_channel_tick(world, host, channel_hooks, source, cold.channel_tick_interval);

			channel_result["channel_started"] = true;
			return channel_result;
		}
		case EFFECT_OPCODE_SELF_DASH: {
			Dictionary dash_result;
			dash_result["success"] = true;
			dash_result["reached_target"] = false;
			if (source.root_remaining > 0.0) {
				dash_result["distance_traveled"] = 0.0;
				return dash_result;
			}

			double dash_distance = effect.scalar0;
			double dir_x = effect.scalar1;
			double dir_y = effect.scalar2;

			double current_x = source.pos_x;
			double current_y = source.pos_y;
			double new_x = current_x;
			double new_y = current_y;

			bool has_direction = (dir_x != 0.0 || dir_y != 0.0);

			if (has_direction) {
				new_x = current_x + dir_x * dash_distance;
				new_y = current_y + dir_y * dash_distance;
			} else if (target != nullptr) {
				int64_t target_id = target->instance_id;
				UnitState *target_unit = unit_by_id(world, target_id);
				if (target_unit != nullptr) {
					double target_x = target_unit->pos_x;
					double target_y = target_unit->pos_y;
					double dx = target_x - current_x;
					double dy = target_y - current_y;
					double dist = sim::distance_between_coords(current_x, current_y, target_x, target_y);
					if (dist > 0.0) {
						double move_dist = Math::min(dash_distance, dist);
						new_x = current_x + (dx / dist) * move_dist;
						new_y = current_y + (dy / dist) * move_dist;
					}
				}
			}

			int64_t source_id = source.instance_id;
			Vector2 valid_pos = sim::movement::find_valid_dash_position(world, current_x, current_y, new_x, new_y, dash_distance, source_id);
			new_x = valid_pos.x;
			new_y = valid_pos.y;

			new_x = Math::clamp(new_x, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
			new_y = Math::clamp(new_y, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);

			if (!has_direction && target != nullptr) {
				UnitState *target_unit = unit_by_id(world, target->instance_id);
				if (target_unit != nullptr) {
					double target_x = target_unit->pos_x;
					double target_y = target_unit->pos_y;
					double final_dist = sim::distance_between_coords(new_x, new_y, target_x, target_y);
					bool reached = (final_dist <= get_effective_attack_range(source) + 0.1);
					dash_result["reached_target"] = reached;
				}
			}

			source.pos_x = new_x;
			source.pos_y = new_y;
			on_unit_position_changed(world, source);
			host.sync_targeting_frame_unit(host.user_data, source);

			double actual_distance = sim::distance_between_coords(current_x, current_y, new_x, new_y);
			dash_result["distance_traveled"] = actual_distance;

			return dash_result;
		}
		default:
			break;
	}
	Dictionary fallback_result;
	fallback_result["success"] = false;
	return fallback_result;
}

} // namespace internal
} // namespace sim::effects::execution
