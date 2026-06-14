#ifndef SIM_MATCH_BENCHMARK_HOST_HPP
#define SIM_MATCH_BENCHMARK_HOST_HPP

#include "sim_coordinator_host.hpp"

#include "sim_catalog.hpp"
#include "sim_match_loop.hpp"
#include "sim_match_roster.hpp"
#include "sim_unit_builder.hpp"

#include "python_random.hpp"

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <vector>

class TeamfightSimulationCore;

namespace sim::match::benchmark {

struct GeneratedMatchHost {
	static void ensure_catalog_loaded(TeamfightSimulationCore *core);
	static sim::catalog::CatalogState &catalog(TeamfightSimulationCore *core);
	static void reset_runtime_state(TeamfightSimulationCore *core);
	static void seed_generated_match(TeamfightSimulationCore *core, int64_t seed, double tick_rate);
	static CPythonRandom &rng(TeamfightSimulationCore *core);
	static godot::Array &player_comp(TeamfightSimulationCore *core);
	static godot::Array &enemy_comp(TeamfightSimulationCore *core);
	static unit_builder::UnitBuilderHost unit_builder_host(TeamfightSimulationCore *core);
	static match_roster::MatchRosterState match_roster_state(TeamfightSimulationCore *core);
	static void build_role_strategy_cache(TeamfightSimulationCore *core);
	static void prepare_tick_context(TeamfightSimulationCore *core);
	static match::MatchLoopState match_loop_state(TeamfightSimulationCore *core);
	static match::MatchLoopHost match_loop_host(TeamfightSimulationCore *core);
	static void simulate_match(TeamfightSimulationCore *core);
	static std::vector<UnitState> &units(TeamfightSimulationCore *core);
	static UnitStateCold &unit_cold_at(TeamfightSimulationCore *core, size_t index);
	static UnitStateCold &uc(TeamfightSimulationCore *core, UnitState &unit);
	static UnitStateRare &unit_rare_at(TeamfightSimulationCore *core, size_t index);
	static UnitStateRare &ur(TeamfightSimulationCore *core, UnitState &unit);
	static godot::StringName &winner_team(TeamfightSimulationCore *core);
	static int64_t &seed(TeamfightSimulationCore *core);
	static double &time(TeamfightSimulationCore *core);
	static int64_t &sudden_death_ticks(TeamfightSimulationCore *core);
	static SimWorld sim_world(TeamfightSimulationCore *core) {
		return CoordinatorHostAccess::sim_world(core);
	}
};

} // namespace sim::match::benchmark

#endif // SIM_MATCH_BENCHMARK_HOST_HPP
