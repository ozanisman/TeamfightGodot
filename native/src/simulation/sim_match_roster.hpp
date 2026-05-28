#ifndef SIM_MATCH_ROSTER_HPP
#define SIM_MATCH_ROSTER_HPP

#include "sim_unit_builder.hpp"
#include "sim_viewer.hpp"
#include "sim_world.hpp"

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/variant.hpp>

#include <cstdint>
#include <unordered_set>
#include <vector>

namespace sim {
namespace match_roster {

struct MatchRosterState {
	int64_t &max_instance_id;
	std::unordered_set<int64_t> &alive_player_indices_set;
	std::unordered_set<int64_t> &alive_enemy_indices_set;
	std::vector<PendingSpawn> &pending_spawns;
};

void add_alive_index(SimWorld &world, MatchRosterState &state, const StringName &team, int64_t index);
void remove_alive_index(SimWorld &world, MatchRosterState &state, const StringName &team, int64_t index);

int64_t register_built_unit(
		SimWorld &world,
		MatchRosterState &state,
		std::pair<UnitState, UnitStateCold> built,
		const StringName &team,
		int64_t instance_id);

void emit_spawn_passive_aoe_fx(ViewerHooks *viewer, SimWorld &world, int64_t unit_index);

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
		void *coerce_user_data);

void process_pending_spawns(
		SimWorld &world,
		MatchRosterState &state,
		const unit_builder::UnitBuilderHost &builder);

} // namespace match_roster
} // namespace sim

#endif // SIM_MATCH_ROSTER_HPP
