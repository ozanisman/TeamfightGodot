#include "../teamfight_simulation_core.hpp"

#include "sim_constants.hpp"
#include "sim_coordinator_host.hpp"
#include "sim_targeting.hpp"
#include "sim_targeting_strategies.hpp"

using namespace sim;

sim::TargetingFrameEntry TeamfightSimulationCore::_make_targeting_frame_entry(const sim::UnitState &unit) const {
	return sim::targeting::make_targeting_frame_entry(unit);
}

void TeamfightSimulationCore::_sync_targeting_frame_index(int64_t index, const sim::UnitState &unit) {
	if (_sim_profile_runtime.targeting_active) {
		_sim_profile_counters.tgt_frame_syncs += 1;
	}
	sim::SimWorld w = _sim_world();
	sim::targeting::sync_targeting_frame_index(w, index, unit);
}

void TeamfightSimulationCore::_sync_targeting_frame_unit(const sim::UnitState &unit) {
	sim::SimWorld w = _sim_world();
	_sync_targeting_frame_index(sim::targeting::unit_index_by_id(w, unit.instance_id), unit);
}

void TeamfightSimulationCore::_build_role_strategy_cache() {
	sim::targeting::build_role_strategy_cache(_role_strategy_cache_by_slot, _default_strategy);
}

const sim::UnitStrategy &TeamfightSimulationCore::_strategy_for_unit(const sim::UnitState &unit) const {
	return sim::targeting::strategy_for_unit(unit, _role_strategy_cache_by_slot, _default_strategy);
}

void TeamfightSimulationCore::_prepare_tick_context() {
	sim::SimWorld w = _sim_world();
	sim::targeting::prepare_tick_context(w, _sim_host_callbacks);
}

sim::targeting::CoordinatorTargetingState TeamfightSimulationCore::_targeting_coordinator_state(bool profile_score_enemy) {
	sim::targeting::CoordinatorTargetingState state;
	state.profile_targeting_active = _sim_profile_runtime.targeting_active;
	state.profile_score_enemy = profile_score_enemy;
	state.debug_targeting_scoring = _debug_targeting_scoring;
	state.debug_user_data = const_cast<TeamfightSimulationCore *>(this);
	state.debug_archetype_for_unit = &sim_host_archetype_for_unit;
	state.debug_print_line = &sim_host_print_line;
	state.debug_print_score_breakdown = &sim_host_print_score_breakdown;
	state.tgt_retarget_keeps = &_sim_profile_counters.tgt_retarget_keeps;
	state.tgt_enemy_early_keeps = &_sim_profile_counters.tgt_enemy_early_keeps;
	state.tgt_enemy_early_keep_rejects = &_sim_profile_counters.tgt_enemy_early_keep_rejects;
	state.tgt_enemy_scans = &_sim_profile_counters.tgt_enemy_scans;
	state.tgt_candidates_scored = &_sim_profile_counters.tgt_candidates_scored;
	state.tgt_candidates_prefix_pruned = &_sim_profile_counters.tgt_candidates_prefix_pruned;
	state.tgt_ties_adjusted = &_sim_profile_counters.tgt_ties_adjusted;
	state.tgt_ties_distance = &_sim_profile_counters.tgt_ties_distance;
	state.tgt_ties_instance = &_sim_profile_counters.tgt_ties_instance;
	state.tgt_ally_scans = &_sim_profile_counters.tgt_ally_scans;
	state.profile_se_base = &_sim_profile_counters.se_base;
	state.profile_se_calls = &_sim_profile_counters.se_calls;
	return state;
}

sim::UnitState *TeamfightSimulationCore::_select_enemy_target(sim::UnitState &unit, bool profile_sim) {
	sim::SimWorld w = _sim_world();
	return sim::targeting::select_enemy_target_coordinator(
			w,
			unit,
			_strategy_for_unit(unit),
			&_sim_host_callbacks,
			_targeting_coordinator_state(profile_sim));
}

sim::UnitState *TeamfightSimulationCore::_select_ally_target(sim::UnitState &unit) {
	sim::SimWorld w = _sim_world();
	return sim::targeting::select_ally_target_coordinator(
			w, unit, _strategy_for_unit(unit), _targeting_coordinator_state(false));
}

void TeamfightSimulationCore::_prune_assist_window(sim::UnitState &unit) {
	const double cutoff = _time - sim::ASSIST_WINDOW;
	sim::UnitStateRare &r = _ur(unit);
	std::vector<int64_t> remove_ids;
	remove_ids.reserve(r.damage_sources.size());
	for (const auto &entry : r.damage_sources) {
		if (entry.second.last_time <= cutoff) {
			remove_ids.push_back(entry.first);
		}
	}
	for (int64_t id : remove_ids) {
		r.damage_sources.erase(id);
	}
	remove_ids.clear();
	for (const auto &entry : r.recent_benefactors) {
		if (entry.second <= cutoff) {
			remove_ids.push_back(entry.first);
		}
	}
	for (int64_t id : remove_ids) {
		r.recent_benefactors.erase(id);
	}
}
