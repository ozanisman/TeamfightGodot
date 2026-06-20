#include "sim_targeting.hpp"

#include "sim_spatial.hpp"
#include "sim_targeting_internal.hpp"

namespace sim {
namespace targeting {

using namespace internal;

void request_immediate_retarget_eval(UnitState &unit, bool bypass_stickiness) {
	unit.retarget_timer = 0.0;
	if (bypass_stickiness) {
		unit.retarget_priority_eval = true;
		unit.target_switch_lock_timer = 0.0;
	}
}

void request_retarget_eval_for_targeters(SimWorld &world, int64_t victim_id, bool bypass_stickiness, const SimHostCallbacks *host) {
	if (victim_id == 0) {
		return;
	}
	for (UnitState &unit : world.units) {
		if (!unit.alive || unit.target_id != victim_id) {
			continue;
		}
		const int64_t old_target_id = unit.target_id;
		adjust_target_pressure(world, old_target_id, 0, host);
		unit.target_id = 0;
		unit.target_index = -1;
		sync_frame_unit(world, unit, host);
		request_immediate_retarget_eval(unit, bypass_stickiness);
	}
}

void request_retarget_eval_for_opposing_team(SimWorld &world, const UnitState &victim, bool bypass_stickiness) {
	if (!victim.alive) {
		return;
	}
	const StringName &opposing_team =
			victim.team == player_team_name() ? enemy_team_name() : player_team_name();
	const std::vector<int64_t> &indices = alive_indices_for_team(world, opposing_team);
	for (int64_t index : indices) {
		if (index < 0 || index >= int64_t(world.units.size())) {
			continue;
		}
		UnitState &unit = world.units[static_cast<size_t>(index)];
		if (!unit.alive) {
			continue;
		}
		request_immediate_retarget_eval(unit, bypass_stickiness);
	}
}

} // namespace targeting
} // namespace sim
