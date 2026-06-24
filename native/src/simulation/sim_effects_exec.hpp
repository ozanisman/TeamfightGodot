#ifndef SIM_EFFECTS_EXEC_HPP
#define SIM_EFFECTS_EXEC_HPP

#include "sim_effects_host.hpp"
#include "sim_world.hpp"

namespace sim::effects::execution {

struct SimExecCallbacks {
	bool (*debug_combat_trace)(void *user_data) = nullptr;
};

Dictionary execute(
		const EffectRecord &effect,
		EffectContext &context,
		SimWorld &world,
		SimHostCallbacks &host,
		const SimExecCallbacks &hooks,
		effects::SimMatchHost match_host);

void resume_deferred_multi_effect(SimWorld &world, SimHostCallbacks &host, UnitState &source);

void resume_deferred_multi_target(SimWorld &world, SimHostCallbacks &host, UnitState &source);

void try_complete_deferred_projectile_chains(SimWorld &world, SimHostCallbacks &host, UnitState &source);

void abandon_deferred_projectile(SimWorld &world, SimHostCallbacks &host, const ProjectileState &projectile);

} // namespace sim::effects::execution

#endif // SIM_EFFECTS_EXEC_HPP
