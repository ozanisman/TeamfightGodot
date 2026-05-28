#ifndef SIM_EFFECTS_EXEC_HPP
#define SIM_EFFECTS_EXEC_HPP

#include "sim_effects_host.hpp"
#include "sim_world.hpp"

namespace sim::effects::execution {

struct SimExecCallbacks {
	void *user_data = nullptr;
	void (*push_projectile)(void *user_data, const ProjectileState &projectile) = nullptr;
	std::vector<UnitState *> (*select_targets)(void *user_data, UnitState &source, UnitState *target, int64_t target_count, TargetSelectionStrategy strategy, bool include_source, ExcessTargetHandling excess_handling, const StringName &team_filter) = nullptr;
	bool (*debug_combat_trace)(void *user_data) = nullptr;
};

Dictionary execute(
		const EffectRecord &effect,
		EffectContext &context,
		SimWorld &world,
		SimHostCallbacks &host,
		const SimExecCallbacks &hooks,
		const effects::SimMatchHost &match_host);

} // namespace sim::effects::execution

#endif // SIM_EFFECTS_EXEC_HPP
