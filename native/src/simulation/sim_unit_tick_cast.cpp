#include "sim_unit_tick.hpp"
#include "sim_unit_tick_internal.hpp"

#include "sim_combat.hpp"

#include <godot_cpp/core/math.hpp>

namespace sim {
namespace unit_tick {

using internal::SimProfileAccScope;

bool casting(
		SimWorld &world,
		UnitState &unit,
		SimHostCallbacks &host,
		const combat::CombatHostHooks &combat_hooks,
		UnitTickProfileCounters &profile) {
	const bool profile_sim = profile.profile_sim;

	bool should_return_casting = false;
	if (unit.casting_remaining >= 0.0 && unit.has_casting_effect) {
		SimProfileAccScope _uu_cast(profile_sim, profile.uu_casting);
		unit.casting_remaining = Math::max(0.0, unit.casting_remaining - world.tick_rate);
		if (unit.casting_remaining <= 0.0) {
			combat::resolve_cast(world, host, combat_hooks, unit);
			unit.cast_resolved_this_tick = true;
		}
		should_return_casting = true;
	}
	return should_return_casting;
}

} // namespace unit_tick
} // namespace sim
