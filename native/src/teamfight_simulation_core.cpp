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
	ClassDB::bind_method(D_METHOD("debug_print_draft_recommendations", "allies", "enemies", "available", "top_n", "stats_dir", "base_weight", "synergy_weight", "counter_weight", "debug_mode", "synergy_amplification", "matchup_amplification"), &TeamfightSimulationCore::debug_print_draft_recommendations, DEFVAL(5), DEFVAL("res://stats_output"), DEFVAL(0.50), DEFVAL(0.25), DEFVAL(0.25), DEFVAL(false), DEFVAL(1.2), DEFVAL(1.2));
	ClassDB::bind_method(D_METHOD("run_debug_draft_evaluation_batch", "allies", "enemies", "available", "num_runs", "stats_dir", "base_weight", "synergy_weight", "counter_weight", "synergy_amplification", "matchup_amplification"), &TeamfightSimulationCore::run_debug_draft_evaluation_batch, DEFVAL(50), DEFVAL("res://stats_output"), DEFVAL(0.50), DEFVAL(0.25), DEFVAL(0.25), DEFVAL(1.2), DEFVAL(1.2));
	ClassDB::bind_method(D_METHOD("get_draft_recommendation_names", "allies", "enemies", "available", "top_n", "stats_dir", "base_weight", "synergy_weight", "counter_weight", "synergy_amplification", "matchup_amplification"), &TeamfightSimulationCore::get_draft_recommendation_names, DEFVAL(3), DEFVAL("res://stats_output"), DEFVAL(0.50), DEFVAL(0.25), DEFVAL(0.25), DEFVAL(1.2), DEFVAL(1.2));
	ClassDB::bind_method(D_METHOD("get_draft_recommendations_with_breakdowns", "allies", "enemies", "available", "top_n", "stats_dir", "base_weight", "synergy_weight", "counter_weight", "synergy_amplification", "matchup_amplification"), &TeamfightSimulationCore::get_draft_recommendations_with_breakdowns, DEFVAL(3), DEFVAL("res://stats_output"), DEFVAL(0.50), DEFVAL(0.25), DEFVAL(0.25), DEFVAL(1.2), DEFVAL(1.2));
	ClassDB::bind_method(D_METHOD("predict_draft_winner", "team1", "team2", "stats_dir", "base_weight", "synergy_weight", "counter_weight", "matchup_weight", "composition_weight", "logistic_k", "include_breakdown", "synergy_amplification", "matchup_amplification", "logit_sharpness", "score_sharpness", "interaction_weight", "scoring_mode", "variance_weight", "cc_weight", "mobility_weight", "sustain_weight", "best_counter_weight", "worst_counter_weight", "best_synergy_weight", "worst_synergy_weight", "synergy_aggregation", "counter_aggregation", "use_decorrelated_scoring", "draft_position", "early_pick_base_weight", "late_pick_counter_weight"), &TeamfightSimulationCore::predict_draft_winner, DEFVAL("res://stats_output"), DEFVAL(0.50), DEFVAL(0.25), DEFVAL(0.25), DEFVAL(0.25), DEFVAL(0.0), DEFVAL(10.0), DEFVAL(false), DEFVAL(1.2), DEFVAL(1.2), DEFVAL(1.5), DEFVAL(1.0), DEFVAL(0.0), DEFVAL(static_cast<int>(sim::draft::ScoringMode::CERTIFIED_PAIRWISE_PROBABILITY)), DEFVAL(0.0), DEFVAL(0.0), DEFVAL(0.0), DEFVAL(0.0), DEFVAL(0.0), DEFVAL(0.0), DEFVAL(0.0), DEFVAL(0.0), DEFVAL(static_cast<int>(sim::draft::AggregationMode::FLAT_AVERAGE)), DEFVAL(static_cast<int>(sim::draft::AggregationMode::FLAT_AVERAGE)), DEFVAL(false), DEFVAL(0), DEFVAL(0.7), DEFVAL(0.4));
	ClassDB::bind_method(D_METHOD("predict_draft_winner_hybrid", "team1", "team2", "stats_dir"), &TeamfightSimulationCore::predict_draft_winner_hybrid, DEFVAL("res://stats_output"));
	ClassDB::bind_method(D_METHOD("analyze_draft_signal_influence", "candidate", "allies", "enemies", "stats_dir", "base_weight", "synergy_weight", "matchup_weight", "synergy_amplification", "matchup_amplification"), &TeamfightSimulationCore::analyze_draft_signal_influence, DEFVAL("res://stats_output"), DEFVAL(0.50), DEFVAL(0.25), DEFVAL(0.25), DEFVAL(1.2), DEFVAL(1.2));
	ClassDB::bind_method(D_METHOD("run_controlled_draft_evaluation", "allies", "enemies", "available", "stats_dir", "base_weight", "synergy_weight", "matchup_weight", "synergy_amplification", "matchup_amplification"), &TeamfightSimulationCore::run_controlled_draft_evaluation, DEFVAL("res://stats_output"), DEFVAL(0.50), DEFVAL(0.25), DEFVAL(0.25), DEFVAL(1.2), DEFVAL(1.2));
	ClassDB::bind_method(D_METHOD("run_stress_test", "allies", "enemies", "available", "stats_dir", "num_iterations", "base_weight", "synergy_weight", "matchup_weight", "synergy_amplification", "matchup_amplification"), &TeamfightSimulationCore::run_stress_test, DEFVAL("res://stats_output"), DEFVAL(50), DEFVAL(0.50), DEFVAL(0.25), DEFVAL(0.25), DEFVAL(1.2), DEFVAL(1.2));
	ClassDB::bind_method(D_METHOD("run_stress_test_with_perturbations", "allies", "enemies", "available", "stats_dir", "seed", "scenario_count", "base_weight", "synergy_weight", "matchup_weight", "synergy_amplification", "matchup_amplification", "scoring_mode", "logit_sharpness", "smoothing_mode"), &TeamfightSimulationCore::run_stress_test_with_perturbations, DEFVAL("res://stats_output"), DEFVAL(42), DEFVAL(30), DEFVAL(0.50), DEFVAL(0.25), DEFVAL(0.25), DEFVAL(1.2), DEFVAL(1.2), DEFVAL(0), DEFVAL(1.0), DEFVAL(0));
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
		bool debug_mode,
		double synergy_amplification,
		double matchup_amplification) {
	sim::draft::DraftStatsDatabase database;
	if (!database.load_from_dir(stats_dir)) {
		UtilityFunctions::push_error(database.last_error());
		return;
	}

	sim::draft::PredictionConfig config;
	config.base_weight = base_weight;
	config.synergy_weight = synergy_weight;
	config.matchup_weight = counter_weight;
	config.synergy_amplification = synergy_amplification;
	config.matchup_amplification = matchup_amplification;

	sim::draft::DraftEvaluator evaluator(database, config);
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
		double counter_weight,
		double synergy_amplification,
		double matchup_amplification) {
	sim::draft::DraftStatsDatabase database;
	if (!database.load_from_dir(stats_dir)) {
		UtilityFunctions::push_error(database.last_error());
		return;
	}

	sim::draft::PredictionConfig config;
	config.base_weight = base_weight;
	config.synergy_weight = synergy_weight;
	config.matchup_weight = counter_weight;
	config.synergy_amplification = synergy_amplification;
	config.matchup_amplification = matchup_amplification;

	sim::draft::DraftEvaluator evaluator(database, config);
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
		double counter_weight,
		double synergy_amplification,
		double matchup_amplification) {
	sim::draft::DraftStatsDatabase database;
	if (!database.load_from_dir(stats_dir)) {
		UtilityFunctions::push_error(database.last_error());
		return Array();
	}

	sim::draft::PredictionConfig config;
	config.base_weight = base_weight;
	config.synergy_weight = synergy_weight;
	config.matchup_weight = counter_weight;
	config.synergy_amplification = synergy_amplification;
	config.matchup_amplification = matchup_amplification;

	sim::draft::DraftEvaluator evaluator(database, config);
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

Array TeamfightSimulationCore::get_draft_recommendations_with_breakdowns(
		const Array &allies,
		const Array &enemies,
		const Array &available,
		int64_t top_n,
		const String &stats_dir,
		double base_weight,
		double synergy_weight,
		double counter_weight,
		double synergy_amplification,
		double matchup_amplification) {
	sim::draft::DraftStatsDatabase database;
	if (!database.load_from_dir(stats_dir)) {
		UtilityFunctions::push_error(database.last_error());
		return Array();
	}

	sim::draft::PredictionConfig config;
	config.base_weight = base_weight;
	config.synergy_weight = synergy_weight;
	config.matchup_weight = counter_weight;
	config.synergy_amplification = synergy_amplification;
	config.matchup_amplification = matchup_amplification;

	sim::draft::DraftEvaluator evaluator(database, config);
	sim::draft::DraftRecommender recommender(evaluator, false);
	std::vector<sim::draft::DraftEvaluation> ranked = recommender.recommend(
			array_to_string_names(allies),
			array_to_string_names(enemies),
			array_to_string_names(available));

	Array result;
	int64_t count = std::min(top_n, static_cast<int64_t>(ranked.size()));
	for (int64_t i = 0; i < count; ++i) {
		const sim::draft::DraftEvaluation &eval = ranked[static_cast<size_t>(i)];
		Dictionary breakdown;
		breakdown["champion"] = String(eval.champion);
		breakdown["base"] = eval.base_winrate;
		breakdown["synergy"] = eval.avg_synergy;
		breakdown["synergy_variance"] = eval.synergy_variance;
		breakdown["counter"] = eval.avg_counter;
		breakdown["counter_variance"] = eval.counter_variance;
		breakdown["final"] = eval.score;
		result.push_back(breakdown);
	}
	return result;
}

Dictionary TeamfightSimulationCore::predict_draft_winner(
		const Array &team1,
		const Array &team2,
		const String &stats_dir,
		double base_weight,
		double synergy_weight,
		double counter_weight,
		double matchup_weight,
		double composition_weight,
		double logistic_k,
		bool include_breakdown,
		double synergy_amplification,
		double matchup_amplification,
		double logit_sharpness,
		double score_sharpness,
		double interaction_weight,
		int scoring_mode,
		double variance_weight,
		double cc_weight,
		double mobility_weight,
		double sustain_weight,
		double best_counter_weight,
		double worst_counter_weight,
		double best_synergy_weight,
		double worst_synergy_weight,
		int synergy_aggregation,
		int counter_aggregation,
		bool use_decorrelated_scoring,
		int draft_position,
		double early_pick_base_weight,
		double late_pick_counter_weight) {
	// load_from_dir() re-reads and re-parses every stats CSV from disk, which is expensive and
	// becomes a major bottleneck when this is called many times in a row for the same stats_dir
	// (e.g. draft prediction-accuracy evaluation calls this once per match). Cache the loaded
	// database per-thread, keyed by stats_dir, and only reload when the directory changes.
	static thread_local String s_cached_stats_dir;
	static thread_local sim::draft::DraftStatsDatabase s_cached_database;
	static thread_local bool s_cache_loaded = false;
	if (!s_cache_loaded || s_cached_stats_dir != stats_dir) {
		sim::draft::DraftStatsDatabase fresh;
		if (!fresh.load_from_dir(stats_dir)) {
			UtilityFunctions::push_error(fresh.last_error());
			Dictionary result;
			result["team1_prob"] = 0.5;
			result["team2_prob"] = 0.5;
			return result;
		}
		s_cached_database = std::move(fresh);
		s_cached_stats_dir = stats_dir;
		s_cache_loaded = true;
	}
	const sim::draft::DraftStatsDatabase &database = s_cached_database;

	sim::draft::PredictionConfig config;
	config.base_weight = base_weight;
	config.synergy_weight = synergy_weight;
	config.matchup_weight = matchup_weight;
	config.composition_weight = composition_weight;
	config.logistic_k = logistic_k;
	config.synergy_amplification = synergy_amplification;
	config.matchup_amplification = matchup_amplification;
	config.logit_sharpness = logit_sharpness;
	config.score_sharpness = score_sharpness;
	config.interaction_weight = interaction_weight;
	config.scoring_mode = static_cast<sim::draft::ScoringMode>(scoring_mode);
	config.variance_weight = variance_weight;
	config.cc_weight = cc_weight;
	config.mobility_weight = mobility_weight;
	config.sustain_weight = sustain_weight;
	config.best_counter_weight = best_counter_weight;
	config.worst_counter_weight = worst_counter_weight;
	config.best_synergy_weight = best_synergy_weight;
	config.worst_synergy_weight = worst_synergy_weight;
	config.synergy_aggregation = static_cast<sim::draft::AggregationMode>(synergy_aggregation);
	config.counter_aggregation = static_cast<sim::draft::AggregationMode>(counter_aggregation);
	config.use_decorrelated_scoring = use_decorrelated_scoring;
	config.draft_position = draft_position;
	config.early_pick_base_weight = early_pick_base_weight;
	config.late_pick_counter_weight = late_pick_counter_weight;

	double team1_prob = 0.0;
	double team2_prob = 0.0;
	sim::draft::DraftStatsDatabase::TeamScoreBreakdown team1_breakdown;
	sim::draft::DraftStatsDatabase::TeamScoreBreakdown team2_breakdown;
	database.calculate_win_probability(
			array_to_string_names(team1),
			array_to_string_names(team2),
			config,
			team1_prob,
			team2_prob,
			team1_breakdown,
			team2_breakdown);

	Dictionary result;
	result["team1_prob"] = team1_prob;
	result["team2_prob"] = team2_prob;
	result["scoring_mode"] = scoring_mode;

	if (include_breakdown) {
		Dictionary team1_dict;
		team1_dict["base"] = team1_breakdown.base;
		team1_dict["synergy"] = team1_breakdown.synergy;
		team1_dict["matchup"] = team1_breakdown.matchup;
		team1_dict["composition"] = team1_breakdown.composition;
		team1_dict["final"] = team1_breakdown.final;
		result["team1_breakdown"] = team1_dict;
		Dictionary team2_dict;
		team2_dict["base"] = team2_breakdown.base;
		team2_dict["synergy"] = team2_breakdown.synergy;
		team2_dict["matchup"] = team2_breakdown.matchup;
		team2_dict["composition"] = team2_breakdown.composition;
		team2_dict["final"] = team2_breakdown.final;
		result["team2_breakdown"] = team2_dict;
	}

	return result;
}

Dictionary TeamfightSimulationCore::predict_draft_winner_hybrid(
		const Array &team1,
		const Array &team2,
		const String &stats_dir) {
	// Load database with caching (same as predict_draft_winner)
	static thread_local String s_cached_stats_dir;
	static thread_local sim::draft::DraftStatsDatabase s_cached_database;
	static thread_local bool s_cache_loaded = false;
	if (!s_cache_loaded || s_cached_stats_dir != stats_dir) {
		sim::draft::DraftStatsDatabase fresh;
		if (!fresh.load_from_dir(stats_dir)) {
			UtilityFunctions::push_error(fresh.last_error());
			Dictionary result;
			result["team1_prob"] = 0.5;
			result["team2_prob"] = 0.5;
			result["model"] = "error";
			return result;
		}
		s_cached_database = std::move(fresh);
		s_cached_stats_dir = stats_dir;
		s_cache_loaded = true;
	}
	const sim::draft::DraftStatsDatabase &database = s_cached_database;

	// Use hybrid model: partial for depths 1-3, certified for depth 4+
	double team1_prob = database.calculate_hybrid_draft_probability(
		array_to_string_names(team1),
		array_to_string_names(team2));

	Dictionary result;
	result["team1_prob"] = team1_prob;
	result["team2_prob"] = 1.0 - team1_prob;
	result["model"] = "hybrid";
	return result;
}

Dictionary TeamfightSimulationCore::analyze_draft_signal_influence(
		const Array &candidate,
		const Array &allies,
		const Array &enemies,
		const String &stats_dir,
		double base_weight,
		double synergy_weight,
		double matchup_weight,
		double synergy_amplification,
		double matchup_amplification) {
	sim::draft::DraftStatsDatabase database;
	if (!database.load_from_dir(stats_dir)) {
		UtilityFunctions::push_error(database.last_error());
		Dictionary result;
		result["base_only_score"] = 0.0;
		result["synergy_removed_score"] = 0.0;
		result["matchup_removed_score"] = 0.0;
		result["full_score"] = 0.0;
		result["base_only_delta"] = 0.0;
		result["synergy_removed_delta"] = 0.0;
		result["matchup_removed_delta"] = 0.0;
		result["ranking_shift_synergy_removed"] = 0;
		result["ranking_shift_matchup_removed"] = 0;
		return result;
	}

	sim::draft::PredictionConfig config;
	config.base_weight = base_weight;
	config.synergy_weight = synergy_weight;
	config.matchup_weight = matchup_weight;
	config.synergy_amplification = synergy_amplification;
	config.matchup_amplification = matchup_amplification;

	sim::draft::DraftEvaluator evaluator(database, config);
	
	std::vector<StringName> candidate_vec = array_to_string_names(candidate);
	std::vector<StringName> allies_vec = array_to_string_names(allies);
	std::vector<StringName> enemies_vec = array_to_string_names(enemies);
	
	if (candidate_vec.empty()) {
		Dictionary result;
		result["base_only_score"] = 0.0;
		result["synergy_removed_score"] = 0.0;
		result["matchup_removed_score"] = 0.0;
		result["full_score"] = 0.0;
		result["base_only_delta"] = 0.0;
		result["synergy_removed_delta"] = 0.0;
		result["matchup_removed_delta"] = 0.0;
		result["ranking_shift_synergy_removed"] = 0;
		result["ranking_shift_matchup_removed"] = 0;
		return result;
	}
	
	sim::draft::SignalInfluenceReport report = evaluator.analyze_signal_influence(
		candidate_vec[0], allies_vec, enemies_vec);

	Dictionary result;
	result["base_only_score"] = report.base_only_score;
	result["synergy_removed_score"] = report.synergy_removed_score;
	result["matchup_removed_score"] = report.matchup_removed_score;
	result["full_score"] = report.full_score;
	result["base_only_delta"] = report.base_only_delta;
	result["synergy_removed_delta"] = report.synergy_removed_delta;
	result["matchup_removed_delta"] = report.matchup_removed_delta;
	result["ranking_shift_synergy_removed"] = report.ranking_shift_synergy_removed;
	result["ranking_shift_matchup_removed"] = report.ranking_shift_matchup_removed;

	return result;
}

Dictionary TeamfightSimulationCore::run_controlled_draft_evaluation(
		const Array &allies,
		const Array &enemies,
		const Array &available,
		const String &stats_dir,
		double base_weight,
		double synergy_weight,
		double matchup_weight,
		double synergy_amplification,
		double matchup_amplification) {
	sim::draft::DraftStatsDatabase database;
	if (!database.load_from_dir(stats_dir)) {
		UtilityFunctions::push_error(database.last_error());
		Dictionary result;
		result["avg_synergy_impact"] = 0.0;
		result["avg_matchup_impact"] = 0.0;
		result["top3_overlap_synergy_removed"] = 0;
		result["top3_overlap_matchup_removed"] = 0;
		return result;
	}

	sim::draft::PredictionConfig config;
	config.base_weight = base_weight;
	config.synergy_weight = synergy_weight;
	config.matchup_weight = matchup_weight;
	config.synergy_amplification = synergy_amplification;
	config.matchup_amplification = matchup_amplification;

	sim::draft::DraftEvaluator evaluator(database, config);
	
	std::vector<StringName> allies_vec = array_to_string_names(allies);
	std::vector<StringName> enemies_vec = array_to_string_names(enemies);
	std::vector<StringName> available_vec = array_to_string_names(available);
	
	sim::draft::ControlledEvaluationReport report = evaluator.run_controlled_evaluation(
		allies_vec, enemies_vec, available_vec);

	Dictionary result;
	result["avg_synergy_impact"] = report.avg_synergy_impact;
	result["avg_matchup_impact"] = report.avg_matchup_impact;
	result["top3_overlap_synergy_removed"] = report.top3_overlap_synergy_removed;
	result["top3_overlap_matchup_removed"] = report.top3_overlap_matchup_removed;

	return result;
}

Dictionary TeamfightSimulationCore::run_stress_test(
		const Array &allies,
		const Array &enemies,
		const Array &available,
		const String &stats_dir,
		int64_t num_iterations,
		double base_weight,
		double synergy_weight,
		double matchup_weight,
		double synergy_amplification,
		double matchup_amplification) {
	sim::draft::DraftStatsDatabase database;
	if (!database.load_from_dir(stats_dir)) {
		UtilityFunctions::push_error(database.last_error());
		Dictionary result;
		result["candidate_stats"] = Array();
		result["score_min"] = 0.0;
		result["score_max"] = 0.0;
		result["score_mean"] = 0.0;
		result["score_p50"] = 0.0;
		result["score_p90"] = 0.0;
		result["avg_rank_change"] = 0.0;
		result["max_rank_swing"] = 0;
		result["context_sensitivity"] = 0.0;
		result["top1_stability_rate"] = 0.0;
		result["top3_stability_rate"] = 0.0;
		result["baseline_top3"] = Array();
		result["baseline_top1"] = String();
		return result;
	}

	sim::draft::PredictionConfig config;
	config.base_weight = base_weight;
	config.synergy_weight = synergy_weight;
	config.matchup_weight = matchup_weight;
	config.synergy_amplification = synergy_amplification;
	config.matchup_amplification = matchup_amplification;

	sim::draft::DraftEvaluator evaluator(database, config);
	
	std::vector<StringName> allies_vec = array_to_string_names(allies);
	std::vector<StringName> enemies_vec = array_to_string_names(enemies);
	std::vector<StringName> available_vec = array_to_string_names(available);
	
	sim::draft::StressTestReport report = evaluator.run_stress_test(
		allies_vec, enemies_vec, available_vec, num_iterations);

	// Convert candidate stats to Array of Dictionaries
	Array candidate_stats_array;
	for (const auto &stats : report.candidate_stats) {
		Dictionary candidate_dict;
		candidate_dict["champion"] = String(stats.champion);
		candidate_dict["mean_score"] = stats.mean_score;
		candidate_dict["score_stddev"] = stats.score_stddev;
		candidate_dict["mean_rank"] = stats.mean_rank;
		candidate_dict["rank_stddev"] = stats.rank_stddev;
		candidate_dict["max_rank_swing"] = stats.max_rank_swing;
		candidate_dict["baseline_score"] = stats.baseline_score;
		candidate_dict["baseline_rank"] = stats.baseline_rank;
		candidate_stats_array.push_back(candidate_dict);
	}

	Dictionary result;
	result["candidate_stats"] = candidate_stats_array;
	result["score_min"] = report.score_min;
	result["score_max"] = report.score_max;
	result["score_mean"] = report.score_mean;
	result["score_p50"] = report.score_p50;
	result["score_p90"] = report.score_p90;
	result["avg_rank_change"] = report.avg_rank_change;
	result["max_rank_swing"] = report.max_rank_swing;
	result["context_sensitivity"] = report.context_sensitivity;
	result["top1_stability_rate"] = report.top1_stability_rate;
	result["top3_stability_rate"] = report.top3_stability_rate;
	
	// Convert baseline top-3 to Array
	Array baseline_top3_array;
	for (const StringName &champ : report.baseline_top3) {
		baseline_top3_array.push_back(String(champ));
	}
	result["baseline_top3"] = baseline_top3_array;
	result["baseline_top1"] = String(report.baseline_top1);

	return result;
}

Dictionary TeamfightSimulationCore::run_stress_test_with_perturbations(
		const Array &allies,
		const Array &enemies,
		const Array &available,
		const String &stats_dir,
		int64_t seed,
		int64_t scenario_count,
		double base_weight,
		double synergy_weight,
		double matchup_weight,
		double synergy_amplification,
		double matchup_amplification,
		int64_t scoring_mode,
		double logit_sharpness,
		int64_t smoothing_mode) {
	sim::draft::DraftStatsDatabase database;
	if (!database.load_from_dir(stats_dir)) {
		UtilityFunctions::push_error(database.last_error());
		Dictionary result;
		result["candidate_stats"] = Array();
		result["score_min"] = 0.0;
		result["score_max"] = 0.0;
		result["score_mean"] = 0.0;
		result["score_p50"] = 0.0;
		result["score_p90"] = 0.0;
		result["avg_rank_change"] = 0.0;
		result["max_rank_swing"] = 0;
		result["context_sensitivity"] = 0.0;
		result["top1_stability_rate"] = 0.0;
		result["top3_stability_rate"] = 0.0;
		result["baseline_top3"] = Array();
		result["baseline_top1"] = String();
		return result;
	}

	sim::draft::PredictionConfig config;
	config.base_weight = base_weight;
	config.synergy_weight = synergy_weight;
	config.matchup_weight = matchup_weight;
	config.synergy_amplification = synergy_amplification;
	config.matchup_amplification = matchup_amplification;
	config.logit_sharpness = logit_sharpness;
	config.smoothing_mode = static_cast<sim::draft::SmoothingMode>(smoothing_mode);

	// Convert int to ScoringMode enum
	config.scoring_mode = static_cast<sim::draft::ScoringMode>(scoring_mode);

	sim::draft::DraftEvaluator evaluator(database, config);
	
	std::vector<StringName> allies_vec = array_to_string_names(allies);
	std::vector<StringName> enemies_vec = array_to_string_names(enemies);
	std::vector<StringName> available_vec = array_to_string_names(available);
	
	sim::draft::StressTestReport report = evaluator.run_stress_test_with_perturbations(
		allies_vec, enemies_vec, available_vec, static_cast<uint64_t>(seed), scenario_count);

	// Convert candidate stats to Array of Dictionaries
	Array candidate_stats_array;
	for (const auto &stats : report.candidate_stats) {
		Dictionary candidate_dict;
		candidate_dict["champion"] = String(stats.champion);
		candidate_dict["mean_score"] = stats.mean_score;
		candidate_dict["score_stddev"] = stats.score_stddev;
		candidate_dict["mean_rank"] = stats.mean_rank;
		candidate_dict["rank_stddev"] = stats.rank_stddev;
		candidate_dict["max_rank_swing"] = stats.max_rank_swing;
		candidate_dict["baseline_score"] = stats.baseline_score;
		candidate_dict["baseline_rank"] = stats.baseline_rank;
		candidate_stats_array.push_back(candidate_dict);
	}

	Dictionary result;
	result["candidate_stats"] = candidate_stats_array;
	result["score_min"] = report.score_min;
	result["score_max"] = report.score_max;
	result["score_mean"] = report.score_mean;
	result["score_p50"] = report.score_p50;
	result["score_p90"] = report.score_p90;
	result["avg_rank_change"] = report.avg_rank_change;
	result["max_rank_swing"] = report.max_rank_swing;
	result["context_sensitivity"] = report.context_sensitivity;
	result["mean_inter_rank_margin"] = report.mean_inter_rank_margin;
	result["median_inter_rank_margin"] = report.median_inter_rank_margin;
	result["top1_stability_rate"] = report.top1_stability_rate;
	result["top3_stability_rate"] = report.top3_stability_rate;
	
	// Convert baseline top-3 to Array
	Array baseline_top3_array;
	for (const StringName &champ : report.baseline_top3) {
		baseline_top3_array.push_back(String(champ));
	}
	result["baseline_top3"] = baseline_top3_array;
	result["baseline_top1"] = String(report.baseline_top1);

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
