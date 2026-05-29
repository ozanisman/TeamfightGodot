#include "sim_match_benchmark_host.hpp"

#include "../teamfight_simulation_core.hpp"

using namespace godot;

namespace sim::match::benchmark {

void GeneratedMatchHost::ensure_catalog_loaded(TeamfightSimulationCore *core) {
	core->_ensure_catalog_loaded();
}

sim::catalog::CatalogState &GeneratedMatchHost::catalog(TeamfightSimulationCore *core) {
	return core->_catalog;
}

void GeneratedMatchHost::reset_runtime_state(TeamfightSimulationCore *core) {
	core->_reset_runtime_state();
}

void GeneratedMatchHost::seed_generated_match(TeamfightSimulationCore *core, int64_t seed, double tick_rate) {
	core->_reset_runtime_state();
	core->_seed = seed;
	core->_tick_rate = tick_rate;
	core->_record_events = false;
	core->_debug_combat_trace = false;
	core->_trace_buffer.clear();
	core->_rng.seed_int64(seed);
}

CPythonRandom &GeneratedMatchHost::rng(TeamfightSimulationCore *core) {
	return core->_rng;
}

Array &GeneratedMatchHost::player_comp(TeamfightSimulationCore *core) {
	return core->_player_comp;
}

Array &GeneratedMatchHost::enemy_comp(TeamfightSimulationCore *core) {
	return core->_enemy_comp;
}

unit_builder::UnitBuilderHost GeneratedMatchHost::unit_builder_host(TeamfightSimulationCore *core) {
	return core->_unit_builder_host();
}

match_roster::MatchRosterState GeneratedMatchHost::match_roster_state(TeamfightSimulationCore *core) {
	return core->_match_roster_state();
}

void GeneratedMatchHost::build_role_strategy_cache(TeamfightSimulationCore *core) {
	core->_build_role_strategy_cache();
}

void GeneratedMatchHost::prepare_tick_context(TeamfightSimulationCore *core) {
	core->_prepare_tick_context();
}

match::MatchLoopState GeneratedMatchHost::match_loop_state(TeamfightSimulationCore *core) {
	return core->_match_loop_state();
}

match::MatchLoopHost GeneratedMatchHost::match_loop_host(TeamfightSimulationCore *core) {
	return core->_match_loop_host();
}

void GeneratedMatchHost::simulate_match(TeamfightSimulationCore *core) {
	match::MatchLoopState loop = core->_match_loop_state();
	match::MatchLoopHost host = core->_match_loop_host();
	match::simulate(loop, host);
}

std::vector<UnitState> &GeneratedMatchHost::units(TeamfightSimulationCore *core) {
	return CoordinatorHostAccess::units(core);
}

UnitStateCold &GeneratedMatchHost::unit_cold_at(TeamfightSimulationCore *core, size_t index) {
	return CoordinatorHostAccess::unit_cold_at(core, index);
}

UnitStateCold &GeneratedMatchHost::uc(TeamfightSimulationCore *core, UnitState &unit) {
	return CoordinatorHostAccess::uc(core, unit);
}

StringName &GeneratedMatchHost::winner_team(TeamfightSimulationCore *core) {
	return core->_winner_team;
}

int64_t &GeneratedMatchHost::seed(TeamfightSimulationCore *core) {
	return core->_seed;
}

double &GeneratedMatchHost::time(TeamfightSimulationCore *core) {
	return core->_time;
}

int64_t &GeneratedMatchHost::sudden_death_ticks(TeamfightSimulationCore *core) {
	return core->_sudden_death_ticks;
}

} // namespace sim::match::benchmark
