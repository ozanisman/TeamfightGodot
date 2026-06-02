#include "sim_match_roster.hpp"

#include "sim_targeting.hpp"

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace sim {
namespace match_roster {
namespace {

inline const StringName &sn_player() {
	static const StringName s("player");
	return s;
}

std::vector<int64_t> &alive_indices_for_team(SimWorld &world, const StringName &team) {
	if (team == sn_player()) {
		return world.alive_player_indices;
	}
	return world.alive_enemy_indices;
}

} // namespace

void add_alive_index(SimWorld &world, MatchRosterState &state, const StringName &team, int64_t index) {
	std::vector<int64_t> &alive_indices = alive_indices_for_team(world, team);
	std::unordered_set<int64_t> &alive_indices_set =
			team == sn_player() ? state.alive_player_indices_set : state.alive_enemy_indices_set;

	if (alive_indices_set.count(index)) {
		return;
	}

	alive_indices.push_back(index);
	alive_indices_set.insert(index);
}

void remove_alive_index(SimWorld &world, MatchRosterState &state, const StringName &team, int64_t index) {
	std::vector<int64_t> &alive_indices = alive_indices_for_team(world, team);
	std::unordered_set<int64_t> &alive_indices_set =
			team == sn_player() ? state.alive_player_indices_set : state.alive_enemy_indices_set;

	alive_indices_set.erase(index);

	for (size_t i = 0; i < alive_indices.size(); ++i) {
		if (alive_indices[i] == index) {
			if (i != alive_indices.size() - 1) {
				alive_indices[i] = alive_indices.back();
			}
			alive_indices.pop_back();
			break;
		}
	}
}

int64_t register_built_unit(
		SimWorld &world,
		MatchRosterState &state,
		std::pair<UnitState, UnitStateCold> built,
		const StringName &team,
		int64_t instance_id) {
	if (built.first.instance_id == 0) {
		return -1;
	}

	const int64_t unit_index = int64_t(world.units.size());
	world.units.push_back(std::move(built.first));
	world.unit_cold.push_back(std::move(built.second));
	world.unit_index_map[instance_id] = unit_index;
	add_alive_index(world, state, team, unit_index);
	world.targeting_frame.push_back(targeting::make_targeting_frame_entry(world.units[static_cast<size_t>(unit_index)]));

	if (instance_id > state.max_instance_id) {
		state.max_instance_id = instance_id;
	}

	return unit_index;
}

void emit_spawn_passive_aoe_fx(ViewerHooks *viewer, SimWorld &world, int64_t unit_index) {
	if (viewer == nullptr || viewer->record_passive_aoe_fx == nullptr) {
		return;
	}
	UnitState &unit = world.units[static_cast<size_t>(unit_index)];
	const UnitStateCold &cold = world.unit_cold[static_cast<size_t>(unit_index)];
	for (const UnitStateCold::PassiveAoeInfo &aoe_info : cold.passive_aoe_info) {
		viewer->record_passive_aoe_fx(viewer->user_data, unit, aoe_info.radius, aoe_info.passive_id);
	}
}

void append_team_units(
		SimWorld &world,
		ViewerHooks *viewer,
		MatchRosterState &state,
		const unit_builder::UnitBuilderHost &builder,
		const Array &spawn_specs,
		const StringName &team,
		int64_t &next_instance_id,
		Array &team_comp,
		Dictionary (*coerce_spawn_spec)(void *user_data, const Variant &value),
		void *coerce_user_data) {
	for (int64_t index = 0; index < spawn_specs.size(); ++index) {
		Dictionary spawn_spec = coerce_spawn_spec(coerce_user_data, spawn_specs[index]);
		std::pair<UnitState, UnitStateCold> built = unit_builder::build_unit(builder, spawn_spec, team, next_instance_id);
		const int64_t unit_index = register_built_unit(world, state, std::move(built), team, next_instance_id);
		if (unit_index < 0) {
			continue;
		}

		team_comp.append(world.unit_cold[static_cast<size_t>(unit_index)].unit_id);
		emit_spawn_passive_aoe_fx(viewer, world, unit_index);
		++next_instance_id;
	}
}

void process_pending_spawns(
		SimWorld &world,
		MatchRosterState &state,
		const unit_builder::UnitBuilderHost &builder) {
	if (state.pending_spawns.empty()) {
		return;
	}

	int64_t next_instance_id = state.max_instance_id + 1;

	for (const PendingSpawn &pending : state.pending_spawns) {
		std::pair<UnitState, UnitStateCold> built =
				unit_builder::build_unit(builder, pending.spawn_spec, pending.team, next_instance_id);
		if (built.first.instance_id == 0) {
			UtilityFunctions::push_error(vformat("Pending spawn failed: build_unit returned instance_id=0"));
			continue;
		}

		const int64_t unit_index = register_built_unit(world, state, std::move(built), pending.team, next_instance_id);
		if (unit_index < 0) {
			continue;
		}

		world.units[static_cast<size_t>(unit_index)].summoner_instance_id = pending.summoner_instance_id;
		next_instance_id++;
	}

	state.pending_spawns.clear();
}

} // namespace match_roster
} // namespace sim
