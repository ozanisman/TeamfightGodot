#include "teamfight_simulation_core.hpp"

#include "stat_definitions.hpp"
#include "simulation/sim_stats.inl.hpp"

#include "simulation/sim_constants.hpp"
#include "simulation/sim_match.hpp"
#include "simulation/sim_match_benchmark.hpp"
#include "simulation/sim_movement.hpp"
#include "simulation/sim_profile.hpp"
#include "simulation/sim_status.hpp"

#define X(name, def, min_val, max_val) using sim::get_effective_##name;
STAT_LIST
#undef X

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <vector>

static std::atomic<int64_t> s_benchmark_progress_done{0};
static std::atomic<bool> s_sim_profile_force_enabled{false};
static std::atomic<bool> s_targeting_profile_force_enabled{false};

sim::SimWorld TeamfightSimulationCore::_sim_world() const {
	TeamfightSimulationCore &self = const_cast<TeamfightSimulationCore &>(*this);
	return sim::SimWorld(
			self._units,
			self._unit_cold,
			self._unit_index_map,
			self._targeting_frame,
			self._tick_ctx,
			self._alive_player_indices,
			self._alive_enemy_indices,
			self._time,
			self._tick_rate,
			&self._spatial_buckets,
			&self._spatial_stamp,
			&self._spatial_generation,
			&self._spatial_fill_cache);
}

sim::match::MatchLoopState TeamfightSimulationCore::_match_loop_state() {
	sim::match::MatchLoopState state{
		_units,
		_unit_cold,
		_unit_index_map,
		_targeting_frame,
		_tick_ctx,
		_alive_player_indices,
		_alive_enemy_indices,
		_spatial_buckets,
		_spatial_stamp,
		_spatial_generation,
		_spatial_fill_cache,
		_tick,
		_time,
		_tick_rate,
		_sudden_death_ticks,
		_player_kills,
		_enemy_kills,
		_winner_team,
		_viewer_fx.events,
	};
	state.profile.ns_projectiles = &_sim_profile_counters.ns_projectiles;
	state.profile.ns_prepare_tick_ctx = &_sim_profile_counters.ns_prepare_tick_ctx;
	state.profile.ns_update_units = &_sim_profile_counters.ns_update_units;
	state.profile.tick_count = &_sim_profile_counters.tick_count;
	state.profile_active = &_sim_profile_runtime.sim_active;
	state.profile_targeting_active = &_sim_profile_runtime.targeting_active;
	return state;
}

sim::UnitStateCold &TeamfightSimulationCore::_uc(sim::UnitState &u) {
	return _unit_cold[static_cast<size_t>(&u - _units.data())];
}

const sim::UnitStateCold &TeamfightSimulationCore::_uc(const sim::UnitState &u) const {
	return _unit_cold[static_cast<size_t>(&u - _units.data())];
}

TeamfightSimulationCore::TeamfightSimulationCore() {
	_bind_sim_host();
	_refresh_match_context();
	_match_context_hosts_cached = true;

	_scratch_projectiles.reserve(32);
	_active_projectiles.reserve(32);
	for (auto &bucket : _spatial_buckets) {
		bucket.reserve(4);
	}
}

TeamfightSimulationCore::~TeamfightSimulationCore() {
}

void TeamfightSimulationCore::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_ready"), &TeamfightSimulationCore::is_ready);
	ClassDB::bind_method(D_METHOD("clear"), &TeamfightSimulationCore::clear);
	ClassDB::bind_method(D_METHOD("run_match", "match_input"), &TeamfightSimulationCore::run_match);
	ClassDB::bind_method(D_METHOD("run_match_stats", "match_input"), &TeamfightSimulationCore::run_match_stats);
	ClassDB::bind_method(D_METHOD("run_matches", "match_inputs"), &TeamfightSimulationCore::run_matches);
	ClassDB::bind_method(D_METHOD("run_matches_stats", "match_inputs"), &TeamfightSimulationCore::run_matches_stats);
	ClassDB::bind_method(D_METHOD("run_match_simulation_only", "match_input"), &TeamfightSimulationCore::run_match_simulation_only);
	ClassDB::bind_method(D_METHOD("run_matches_simulation_only", "match_inputs"), &TeamfightSimulationCore::run_matches_simulation_only);
	ClassDB::bind_method(D_METHOD("run_generated_matches_simulation_only", "base_seed", "batch_count", "team_size"), &TeamfightSimulationCore::run_generated_matches_simulation_only);
	ClassDB::bind_method(D_METHOD("run_generated_matches_stats_partial", "base_seed", "batch_count", "team_size", "include_match_log", "tick_rate"), &TeamfightSimulationCore::run_generated_matches_stats_partial);
	ClassDB::bind_method(D_METHOD("begin_match", "match_input"), &TeamfightSimulationCore::begin_match);
	ClassDB::bind_method(D_METHOD("advance_one_tick"), &TeamfightSimulationCore::advance_one_tick);
	ClassDB::bind_method(D_METHOD("match_ticks_exhausted"), &TeamfightSimulationCore::match_ticks_exhausted);
	ClassDB::bind_method(D_METHOD("finish_and_summarize"), &TeamfightSimulationCore::finish_and_summarize);
	ClassDB::bind_method(D_METHOD("get_tick_snapshot"), &TeamfightSimulationCore::get_tick_snapshot);
	ClassDB::bind_method(D_METHOD("get_trace_events"), &TeamfightSimulationCore::get_trace_events);
	ClassDB::bind_method(D_METHOD("set_balance_patches", "patches"), &TeamfightSimulationCore::set_balance_patches);
	ClassDB::bind_method(D_METHOD("get_balance_patches"), &TeamfightSimulationCore::get_balance_patches);
	ClassDB::bind_method(D_METHOD("effective_champion_for", "unit_id"), &TeamfightSimulationCore::effective_champion_for);
	ClassDB::bind_method(D_METHOD("flush_stdio"), &TeamfightSimulationCore::flush_stdio);
	ClassDB::bind_method(D_METHOD("benchmark_progress_reset", "total_matches"), &TeamfightSimulationCore::benchmark_progress_reset);
	ClassDB::bind_method(D_METHOD("benchmark_progress_add", "delta_matches"), &TeamfightSimulationCore::benchmark_progress_add);
	ClassDB::bind_method(D_METHOD("benchmark_progress_read"), &TeamfightSimulationCore::benchmark_progress_read);
	ClassDB::bind_method(D_METHOD("sim_profile_set_enabled", "enabled"), &TeamfightSimulationCore::sim_profile_set_enabled);
	ClassDB::bind_method(D_METHOD("targeting_profile_set_enabled", "enabled"), &TeamfightSimulationCore::targeting_profile_set_enabled);
	ClassDB::bind_method(D_METHOD("debug_print_draft_recommendations", "allies", "enemies", "available", "top_n", "stats_dir", "base_weight", "synergy_weight", "counter_weight", "debug_mode"), &TeamfightSimulationCore::debug_print_draft_recommendations, DEFVAL(5), DEFVAL("res://stats_output"), DEFVAL(0.50), DEFVAL(0.25), DEFVAL(0.25), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("run_debug_draft_evaluation_batch", "allies", "enemies", "available", "num_runs", "stats_dir", "base_weight", "synergy_weight", "counter_weight"), &TeamfightSimulationCore::run_debug_draft_evaluation_batch, DEFVAL(50), DEFVAL("res://stats_output"), DEFVAL(0.50), DEFVAL(0.25), DEFVAL(0.25));
	ClassDB::bind_method(D_METHOD("get_draft_recommendation_names", "allies", "enemies", "available", "top_n", "stats_dir", "base_weight", "synergy_weight", "counter_weight"), &TeamfightSimulationCore::get_draft_recommendation_names, DEFVAL(3), DEFVAL("res://stats_output"), DEFVAL(0.50), DEFVAL(0.25), DEFVAL(0.25));
}

void TeamfightSimulationCore::flush_stdio() {
	std::fflush(stdout);
	std::fflush(stderr);
}

void TeamfightSimulationCore::benchmark_progress_reset(int64_t p_total_matches) {
	(void)p_total_matches;
	s_benchmark_progress_done.store(0, std::memory_order_relaxed);
}

void TeamfightSimulationCore::benchmark_progress_add(int64_t delta_matches) {
	if (delta_matches > 0) {
		s_benchmark_progress_done.fetch_add(delta_matches, std::memory_order_relaxed);
	}
}

int64_t TeamfightSimulationCore::benchmark_progress_read() {
	return s_benchmark_progress_done.load(std::memory_order_relaxed);
}

void TeamfightSimulationCore::sim_profile_set_enabled(bool enabled) {
	s_sim_profile_force_enabled.store(enabled, std::memory_order_relaxed);
}

void TeamfightSimulationCore::targeting_profile_set_enabled(bool enabled) {
	s_targeting_profile_force_enabled.store(enabled, std::memory_order_relaxed);
}

namespace {

std::vector<StringName> array_to_string_names(const Array &values) {
	std::vector<StringName> out;
	out.reserve(static_cast<size_t>(values.size()));
	for (int64_t i = 0; i < values.size(); ++i) {
		Variant value = values[i];
		if (value.get_type() == Variant::STRING_NAME) {
			out.push_back(StringName(value));
		} else if (value.get_type() == Variant::STRING) {
			out.push_back(StringName(String(value)));
		}
	}
	return out;
}

}

void TeamfightSimulationCore::debug_print_draft_recommendations(
		const Array &allies,
		const Array &enemies,
		const Array &available,
		int64_t top_n,
		const String &stats_dir,
		double base_weight,
		double synergy_weight,
		double counter_weight,
		bool debug_mode) {
	sim::draft::DraftStatsDatabase database;
	if (!database.load_from_dir(stats_dir)) {
		UtilityFunctions::push_error(database.last_error());
		return;
	}

	sim::draft::DraftScoreWeights weights;
	weights.base = base_weight;
	weights.synergy = synergy_weight;
	weights.counter = counter_weight;

	sim::draft::DraftEvaluator evaluator(database, weights);
	sim::draft::DraftRecommender recommender(evaluator, debug_mode);
	std::vector<sim::draft::DraftEvaluation> ranked = recommender.recommend(
			array_to_string_names(allies),
			array_to_string_names(enemies),
			array_to_string_names(available));
	recommender.print_top(ranked, top_n);
	std::fflush(stdout);
}

void TeamfightSimulationCore::run_debug_draft_evaluation_batch(
		const Array &allies,
		const Array &enemies,
		const Array &available,
		int64_t num_runs,
		const String &stats_dir,
		double base_weight,
		double synergy_weight,
		double counter_weight) {
	sim::draft::DraftStatsDatabase database;
	if (!database.load_from_dir(stats_dir)) {
		UtilityFunctions::push_error(database.last_error());
		return;
	}

	sim::draft::DraftScoreWeights weights;
	weights.base = base_weight;
	weights.synergy = synergy_weight;
	weights.counter = counter_weight;

	sim::draft::DraftEvaluator evaluator(database, weights);
	sim::draft::DraftRecommender recommender(evaluator, true);
	recommender.run_debug_evaluation_batch(
			array_to_string_names(allies),
			array_to_string_names(enemies),
			array_to_string_names(available),
			num_runs);
	std::fflush(stdout);
}

Array TeamfightSimulationCore::get_draft_recommendation_names(
		const Array &allies,
		const Array &enemies,
		const Array &available,
		int64_t top_n,
		const String &stats_dir,
		double base_weight,
		double synergy_weight,
		double counter_weight) {
	sim::draft::DraftStatsDatabase database;
	if (!database.load_from_dir(stats_dir)) {
		UtilityFunctions::push_error(database.last_error());
		return Array();
	}

	sim::draft::DraftScoreWeights weights;
	weights.base = base_weight;
	weights.synergy = synergy_weight;
	weights.counter = counter_weight;

	sim::draft::DraftEvaluator evaluator(database, weights);
	sim::draft::DraftRecommender recommender(evaluator, false);
	std::vector<sim::draft::DraftEvaluation> ranked = recommender.recommend(
			array_to_string_names(allies),
			array_to_string_names(enemies),
			array_to_string_names(available));

	Array result;
	int64_t count = std::min(top_n, static_cast<int64_t>(ranked.size()));
	for (int64_t i = 0; i < count; ++i) {
		result.push_back(String(ranked[static_cast<size_t>(i)].champion));
	}
	return result;
}

double TeamfightSimulationCore::_randf() {
	return _rng.random_random();
}

bool TeamfightSimulationCore::is_ready() const {
	return true;
}

void TeamfightSimulationCore::clear() {
	_reset_runtime_state();
}

Vector2 TeamfightSimulationCore::_resolve_aoe_direction(const sim::UnitState &source, const sim::AoShapeParams &params, const sim::UnitState *target_override) const {
	sim::SimWorld w = _sim_world();
	return sim::status::resolve_aoe_direction(w, source, params, target_override);
}

bool TeamfightSimulationCore::_position_collides_with_unit(double x, double y, int64_t exclude_instance_id) const {
	return sim::movement::position_collides_with_unit(_sim_world(), x, y, exclude_instance_id);
}

Vector2 TeamfightSimulationCore::_get_random_spawn_position(const StringName &team, bool is_respawn) {
	(void)is_respawn;
	double x_base = (team == StringName("player")) ? sim::PLAYER_SPAWN_X_BASE : sim::ENEMY_SPAWN_X_BASE;

	static const std::vector<double> spawn_points = {3.0, 3.5, 4.0, 4.5, 5.0, 5.5, 6.0, 6.5, 7.0};

	int max_index = int(spawn_points.size() - 1);
	int index = (_rng.genrand_uint32() % (max_index + 1));

	return Vector2(x_base, spawn_points[index]);
}

bool TeamfightSimulationCore::_sim_profile_env_enabled() {
	if (s_sim_profile_force_enabled.load(std::memory_order_relaxed)) {
		return true;
	}
	const char *v = std::getenv("TEAMFIGHT_SIM_PROFILE");
	if (v == nullptr || v[0] == '\0') {
		return false;
	}
	return !(v[0] == '0' && v[1] == '\0');
}

bool TeamfightSimulationCore::targeting_profile_env_enabled() {
	if (s_targeting_profile_force_enabled.load(std::memory_order_relaxed)) {
		return true;
	}
	const char *v = std::getenv("TEAMFIGHT_TARGETING_PROFILE");
	if (v == nullptr || v[0] == '\0') {
		return false;
	}
	return !(v[0] == '0' && v[1] == '\0');
}

void TeamfightSimulationCore::_sim_profile_reset() {
	sim::profile::reset(*this);
}

void TeamfightSimulationCore::_sim_profile_emit_json_stderr() const {
	sim::profile::emit_json_stderr(*this);
}

sim::match::MatchSnapshot TeamfightSimulationCore::_match_snapshot() const {
	sim::match::MatchSnapshot snap;
	snap.seed = _seed;
	snap.winner_team = _winner_team;
	snap.time = _time;
	snap.sudden_death_ticks = _sudden_death_ticks;
	snap.player_kills = _player_kills;
	snap.enemy_kills = _enemy_kills;
	snap.player_comp = _player_comp;
	snap.enemy_comp = _enemy_comp;
	return snap;
}

Dictionary TeamfightSimulationCore::_build_summary() {
	return sim::match::build_summary(_match_snapshot(), _units, _unit_cold, _summary_cache, _summary_unit_stats);
}

Dictionary TeamfightSimulationCore::_build_stats_summary() {
	return sim::match::build_stats_summary(_match_snapshot(), _units, _unit_cold);
}

void TeamfightSimulationCore::run_generated_matches_simulation_only(int64_t base_seed, int64_t batch_count, int64_t team_size) {
	sim::match::benchmark::run_generated_matches_simulation_only(*this, base_seed, batch_count, team_size);
}

Dictionary TeamfightSimulationCore::run_generated_matches_stats_partial(
		int64_t base_seed,
		int64_t batch_count,
		int64_t team_size,
		bool include_match_log,
		double tick_rate) {
	return sim::match::benchmark::run_generated_matches_stats_partial(
			*this, base_seed, batch_count, team_size, include_match_log, tick_rate);
}

void TeamfightSimulationCore::_log_sudden_death_draw() {
	String player_team = _join_team_names(_player_comp);
	String enemy_team = _join_team_names(_enemy_comp);
	UtilityFunctions::push_error("==============================================");
	UtilityFunctions::push_error("!!! SUDDEN DEATH SAFETY LIMIT REACHED !!!");
	UtilityFunctions::push_error("!!! MATCH ENDED IN DRAW AFTER MAX TICKS !!!");
	UtilityFunctions::push_error(vformat("!!! PLAYER TEAM: %s !!!", player_team));
	UtilityFunctions::push_error(vformat("!!! ENEMY TEAM: %s !!!", enemy_team));
	UtilityFunctions::push_error("!!! THIS MATCHUP CREATED A STALEMATE !!!");
	UtilityFunctions::push_error("==============================================");
}

void TeamfightSimulationCore::_debug_print_stack_state(const sim::UnitState &unit) const {
	if (!_debug_combat_trace) {
		return;
	}

	String debug_output = vformat("=== STACK STATE FOR UNIT %d ===\n", unit.instance_id);
	debug_output += vformat("Total stacks: %d\n", unit.stat_stacks.size());

	Array stack_keys = unit.stat_stacks.keys();
	for (int i = 0; i < stack_keys.size(); i++) {
		Variant key_var = stack_keys[i];
		if (key_var.get_type() != Variant::STRING) {
			continue;
		}

		String key = key_var;
		Dictionary stack_dict = unit.stat_stacks[key];

		debug_output += vformat("Stack[%s]: stacks=%d/%d, duration=%.2f, reason=%s\n",
				key,
				stack_dict.get("current_stacks", 0),
				stack_dict.get("max_stacks", 1),
				stack_dict.get("duration", 0.0),
				stack_dict.get("reason", "unknown"));
	}

	UtilityFunctions::push_warning(debug_output);
}
