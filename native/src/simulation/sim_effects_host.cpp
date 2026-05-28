#include "sim_effects_host.hpp"

#include "sim_effects_exec.hpp"

#include <godot_cpp/variant/dictionary.hpp>

namespace sim {
namespace effects {

SimWorld EffectExecBindings::make_world() const {
	return SimWorld(
			*units,
			*unit_cold,
			*unit_index_map,
			*targeting_frame,
			*tick_ctx,
			*alive_player_indices,
			*alive_enemy_indices,
			*time,
			*tick_rate,
			spatial_buckets,
			spatial_stamp,
			spatial_generation);
}

Dictionary host_execute_effect(SimHostCallbacks &host, const EffectRecord &effect, EffectContext &context) {
	if (host.effect_exec == nullptr || host.effect_exec->exec_callbacks == nullptr) {
		return Dictionary();
	}
	EffectExecBindings &bindings = *host.effect_exec;
	SimWorld world = bindings.make_world();
	return execution::execute(
			effect,
			context,
			world,
			host,
			*bindings.exec_callbacks,
			bindings.match_host);
}

} // namespace effects
} // namespace sim
