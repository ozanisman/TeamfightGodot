#ifndef TEAMFIGHT_SIMULATION_CORE_HPP
#define TEAMFIGHT_SIMULATION_CORE_HPP

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/variant.hpp>

#include "python_random.hpp"

#include "simulation/sim_aoe.hpp"
#include "simulation/sim_catalog.hpp"
#include "simulation/sim_channel.hpp"
#include "simulation/sim_combat.hpp"
#include "simulation/sim_constants.hpp"
#include "simulation/sim_draft_recommender.hpp"
#include "simulation/sim_effects_exec.hpp"
#include "simulation/sim_effects_host.hpp"
#include "simulation/sim_match.hpp"
#include "simulation/sim_match_lifecycle.hpp"
#include "simulation/sim_match_loop.hpp"
#include "simulation/sim_profile_counters.hpp"
#include "simulation/sim_match_roster.hpp"
#include "simulation/sim_unit_builder.hpp"
#include "simulation/simulation_types.hpp"
#include "simulation/sim_status.hpp"
#include "simulation/sim_targeting.hpp"
#include "simulation/sim_unit_tick.hpp"
#include "simulation/sim_coordinator_host.hpp"
#include "simulation/sim_match_benchmark.hpp"
#include "simulation/sim_viewer.hpp"
#include "simulation/sim_world.hpp"

#include <array>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace godot;

// Simulation logic lives under native/src/simulation/; methods here are Godot glue and thin
// delegators unless marked as catalog/match cold path.
class TeamfightSimulationCore : public RefCounted {
	GDCLASS(TeamfightSimulationCore, RefCounted);

	friend struct sim::CoordinatorHostAccess;

protected:
	static void _bind_methods();

public:
	enum RoleSlot : int64_t {
		ROLE_SLOT_TANK = sim::ROLE_SLOT_TANK,
		ROLE_SLOT_FIGHTER = sim::ROLE_SLOT_FIGHTER,
		ROLE_SLOT_ASSASSIN = sim::ROLE_SLOT_ASSASSIN,
		ROLE_SLOT_MARKSMAN = sim::ROLE_SLOT_MARKSMAN,
		ROLE_SLOT_MAGE = sim::ROLE_SLOT_MAGE,
		ROLE_SLOT_SUPPORT = sim::ROLE_SLOT_SUPPORT,
		ROLE_SLOT_COUNT = sim::ROLE_SLOT_COUNT,
	};

	Dictionary effective_champion_for(const StringName &unit_id) const;

	std::vector<sim::ProjectileState> _projectiles;
	std::vector<sim::ProjectileState> _scratch_projectiles;
	std::vector<sim::ProjectileState> _active_projectiles;
	std::vector<int64_t> _scratch_critical_allies;
	Array _summary_unit_stats;
	Dictionary _summary_cache;
	CPythonRandom _rng;
	double _time = 0.0;
	int64_t _tick = 0;
	int64_t _sudden_death_ticks = 0;
	double _tick_rate = sim::DEFAULT_TICK_RATE;
	int64_t _seed = 0;
	StringName _winner_team = StringName("draw");
	bool _record_events = false;
	Array _player_comp;
	Array _enemy_comp;
	int64_t _player_kills = 0;
	int64_t _enemy_kills = 0;
	int64_t _max_instance_id = 0;
	int64_t _next_projectile_id = 1;

	// Spawn slot tracking per team (indices into spawn_points array)
	std::vector<bool> _player_spawn_slots_used;
	std::vector<bool> _enemy_spawn_slots_used;
	sim::catalog::CatalogState _catalog;
	std::unordered_map<int64_t, int64_t> _unit_index_map;
	std::vector<int64_t> _alive_player_indices;
	std::vector<int64_t> _alive_enemy_indices;
	std::unordered_set<int64_t> _alive_player_indices_set;
	std::unordered_set<int64_t> _alive_enemy_indices_set;
	std::vector<sim::TargetingFrameEntry> _targeting_frame;
	std::array<sim::UnitStrategy, ROLE_SLOT_COUNT> _role_strategy_cache_by_slot{};
	sim::UnitStrategy _default_strategy;
	sim::TickContext _tick_ctx;
	std::vector<sim::TraceEvent> _trace_buffer;
	static constexpr size_t TRACE_BUFFER_CAP = 4096;
	bool _debug_combat_trace = false;
	bool _debug_targeting_scoring = false;

	sim::viewer::ViewerFxBuffer _viewer_fx;

	void _print_score_breakdown(const sim::ScoreBreakdown &breakdown, const StringName &attacker_archetype, const StringName &enemy_archetype) const;

	mutable std::array<std::vector<int64_t>, sim::SPATIAL_GRID_DIM * sim::SPATIAL_GRID_DIM> _spatial_buckets;
	mutable std::vector<uint32_t> _spatial_stamp;
	mutable uint32_t _spatial_generation = 1;
	mutable sim::SpatialBucketFillCache _spatial_fill_cache;

	sim::profile::Counters _sim_profile_counters;
	sim::profile::RuntimeFlags _sim_profile_runtime;
	bool _match_context_hosts_cached = false;

	static bool _sim_profile_env_enabled();
	static bool targeting_profile_env_enabled();
	void _sim_profile_reset();
	void _sim_profile_emit_json_stderr() const;

	void _reset_runtime_state();
	double _randf();
	void _ensure_catalog_loaded();
	sim::catalog::CatalogHooks _catalog_hooks() const;
	static sim::EffectRecord _catalog_compile_effect(void *user_data, const Dictionary &effect);
	Dictionary _effective_champion_for(const StringName &unit_id) const;

	sim::EffectRecord _compile_effect(const Dictionary &effect) const;
	std::vector<sim::EffectRecord> _compile_effect_array(const Array &effects) const;
	Dictionary _coerce_match_input(const Variant &match_input) const;
	void _populate_runtime_state(const Dictionary &match_input);
	sim::TargetingFrameEntry _make_targeting_frame_entry(const sim::UnitState &unit) const;
	void _sync_targeting_frame_index(int64_t index, const sim::UnitState &unit);
	void _sync_targeting_frame_unit(const sim::UnitState &unit);
	sim::UnitStateCold &_uc(sim::UnitState &u);
	const sim::UnitStateCold &_uc(const sim::UnitState &u) const;

	sim::SimWorld _sim_world() const;
	struct CoordinatorMatchContext {
		sim::match::MatchLoopHost loop_host{};
		sim::unit_builder::UnitBuilderHost unit_builder_host{};
	};
	CoordinatorMatchContext _match_ctx{};

	void _refresh_match_context();
	sim::unit_builder::UnitBuilderHost _unit_builder_host() const;
	sim::match_roster::MatchRosterState _match_roster_state();
	sim::match::MatchScoreState _match_score_state();
	sim::match::SpawnSlotState _spawn_slot_state();
	void _bind_sim_host();
	void _bind_sim_exec_hooks();
	void _bind_effect_exec_bindings();
	sim::SimHostCallbacks _sim_host_callbacks{};
	sim::ViewerHooks _viewer_hooks{};
	sim::effects::EffectExecBindings _effect_exec_bindings{};
	sim::effects::execution::SimExecCallbacks _sim_exec_callbacks{};
	sim::combat::CombatHostHooks _combat_host_hooks() const;
	sim::channel::ChannelHostHooks _channel_host_hooks() const;
	sim::unit_tick::UnitTickHostHooks _unit_tick_host_hooks() const;

	void _prepare_tick_context();
	void _build_role_strategy_cache();
	const sim::UnitStrategy &_strategy_for_unit(const sim::UnitState &unit) const;
	void _emit_trace(const StringName &kind, int64_t src_id, int64_t tgt_id, double val);
	sim::match::MatchLoopState _match_loop_state();
	sim::match::MatchLoopHost _match_loop_host() const;
	void _update_unit(sim::UnitState &unit, bool profile_sim);
	void _prune_assist_window(sim::UnitState &unit);
	void _update_projectiles();
	/// When `unit_ally_distance` is >= 0, used as the unit–ally distance (avoids a duplicate sqrt vs `_distance_between`).
	/// When `current_target_distance` is >= 0, used instead of recomputing distance to `unit.target_id` for commit-window logic.
	bool _effect_record_contains_opcode(const sim::EffectRecord &effect, sim::EffectOpcode opcode) const;
	void _finalize_reflect_passives(sim::UnitState &unit, sim::UnitStateCold &cold);
	Vector2 _get_random_spawn_position(const StringName &team, bool is_respawn);
	sim::targeting::CoordinatorTargetingState _targeting_coordinator_state(bool profile_score_enemy);
	sim::UnitState *_select_enemy_target(sim::UnitState &unit, bool profile_sim);
	sim::UnitState *_select_ally_target(sim::UnitState &unit);
	bool _position_collides_with_unit(double x, double y, int64_t exclude_instance_id) const;
	Dictionary _build_summary();
	Dictionary _build_stats_summary();
	sim::match::MatchSnapshot _match_snapshot() const;
	
	// Pending spawns queue for mid-tick unit creation
	std::vector<sim::PendingSpawn> _pending_spawns;

	// Stack debugging functions
	void _debug_print_stack_state(const sim::UnitState &unit) const;
	void _log_sudden_death_draw();
	String _join_team_names(const Array &team) const;

	Dictionary _champion_for(const StringName &unit_id) const;

	Vector2 _resolve_aoe_direction(const sim::UnitState &source, const sim::AoShapeParams &params, const sim::UnitState *target_override = nullptr) const;

	/// Parameters for circular AoE iteration over an alive-team index list (`_alive_*_indices`).
	/// `spatial_team` must match `UnitState::team` for units referenced by `indices` (used by broad-phase stamp).
	template<typename Fn>
	void _for_each_unit_in_circle(const sim::AoCircleIterationParams &p, Fn &&fn) {
		sim::SimWorld w = _sim_world();
		sim::for_each_unit_in_circle(w, p, std::forward<Fn>(fn));
	}

	template<typename Fn>
	void _for_each_unit_in_shape(const sim::AoShapeIterationParams &p, Fn &&fn) {
		sim::SimWorld w = _sim_world();
		sim::for_each_unit_in_shape(w, p, std::forward<Fn>(fn), [&](const sim::UnitState &source, const sim::AoShapeParams &params, const sim::UnitState *target_override) {
			return sim::status::resolve_aoe_direction(w, source, params, target_override);
		});
	}

	static inline double _distance_between_coords(double x1, double y1, double x2, double y2) {
		return Math::sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
	}

private:
	/// Parallel to `_units` (same length and indices). Push or clear together with `_units` only.
	/// `_uc(u)` is only valid when `u` references an element stored in `_units` (never a stack copy of `UnitState`).
	std::vector<sim::UnitState> _units;
	std::vector<sim::UnitStateCold> _unit_cold;

public:
	TeamfightSimulationCore();
	~TeamfightSimulationCore() override;

	bool is_ready() const;
	void clear();
	Dictionary run_match(const Variant &match_input);
	Dictionary run_match_stats(const Variant &match_input);
	Array run_matches(const Array &match_inputs);
	/// Runs `run_match_stats` semantics for each input; clears between matches like `run_matches`.
	Array run_matches_stats(const Array &match_inputs);
	void run_match_simulation_only(const Variant &match_input);
	void run_matches_simulation_only(const Array &match_inputs);
	void run_generated_matches_simulation_only(int64_t base_seed, int64_t batch_count, int64_t team_size);
	Dictionary run_generated_matches_stats_partial(int64_t base_seed, int64_t batch_count, int64_t team_size, bool include_match_log, double tick_rate = sim::DEFAULT_TICK_RATE);

	/// Incremental match API (used by gameplay loops and viewer stepping). Does not replace run_match for batch parity runs.
	void begin_match(const Variant &match_input);
	void advance_one_tick();
	bool match_ticks_exhausted() const;
	Dictionary finish_and_summarize();
	/// Debug: compact per-unit state after the last completed tick (parity tooling).
	Dictionary get_tick_snapshot() const;
	Array get_trace_events() const;

	// Balance patch API: inject patches at runtime without modifying balance_patches.json.
	// Each element in `patches` must be a Dictionary with keys:
	//   "targets"         Array[String]   — archetype IDs to target (empty = all)
	//   "roles"           Array[String]   — roles to target (empty = all)
	//   "stat_multipliers" Dictionary     — { stat_name: multiplier }
	//   "stat_additions"  Dictionary      — { stat_name: delta }
	// Calling this resets the catalog so patches take effect on the next run_match().
	void set_balance_patches(const Array &patches);
	Array get_balance_patches() const;

	/// Force libc stdio flush (headless / PowerShell often fully-buffers stdout).
	void flush_stdio();

	/// Thread-safe batch progress (workers call add; main thread reads + prints — never print from worker threads).
	void benchmark_progress_reset(int64_t total_matches);
	void benchmark_progress_add(int64_t delta_matches);
	int64_t benchmark_progress_read();

	/// When true (or env TEAMFIGHT_SIM_PROFILE), _simulate emits one stderr JSON line per match with per-section tick timings.
	void sim_profile_set_enabled(bool enabled);
	void targeting_profile_set_enabled(bool enabled);
	void debug_print_draft_recommendations(const Array &allies, const Array &enemies, const Array &available, int64_t top_n = 5, const String &stats_dir = "res://stats_output", double base_weight = 0.50, double synergy_weight = 0.25, double counter_weight = 0.25, bool debug_mode = false, double synergy_amplification = 1.2, double matchup_amplification = 1.2);
	void run_debug_draft_evaluation_batch(const Array &allies, const Array &enemies, const Array &available, int64_t num_runs = 50, const String &stats_dir = "res://stats_output", double base_weight = 0.50, double synergy_weight = 0.25, double counter_weight = 0.25, double synergy_amplification = 1.2, double matchup_amplification = 1.2);
	Array get_draft_recommendation_names(const Array &allies, const Array &enemies, const Array &available, int64_t top_n = 3, const String &stats_dir = "res://stats_output", double base_weight = 0.50, double synergy_weight = 0.25, double counter_weight = 0.25, double synergy_amplification = 1.2, double matchup_amplification = 1.2);
	Array get_draft_recommendations_with_breakdowns(const Array &allies, const Array &enemies, const Array &available, int64_t top_n = 3, const String &stats_dir = "res://stats_output", double base_weight = 0.50, double synergy_weight = 0.25, double counter_weight = 0.25, double synergy_amplification = 1.2, double matchup_amplification = 1.2, int draft_position = 0, double early_pick_base_weight = 0.7, double late_pick_counter_weight = 0.4);
	Dictionary predict_draft_winner(const Array &team1, const Array &team2, const String &stats_dir = "res://stats_output", double base_weight = 0.50, double synergy_weight = 0.25, double counter_weight = 0.25, double matchup_weight = 0.25, double composition_weight = 0.0, double logistic_k = 10.0, bool include_breakdown = false, double synergy_amplification = 1.2, double matchup_amplification = 1.2, double score_sharpness = 1.0, double interaction_weight = 0.0, int scoring_mode = 3, double variance_weight = 0.0, double cc_weight = 0.0, double mobility_weight = 0.0, double sustain_weight = 0.0, double best_counter_weight = 0.0, double worst_counter_weight = 0.0, double best_synergy_weight = 0.0, double worst_synergy_weight = 0.0, int synergy_aggregation = 0, int counter_aggregation = 0, bool use_decorrelated_scoring = false, int draft_position = 0, double early_pick_base_weight = 0.7, double late_pick_counter_weight = 0.4);
	Dictionary analyze_draft_signal_influence(const Array &candidate, const Array &allies, const Array &enemies, const String &stats_dir = "res://stats_output", double base_weight = 0.50, double synergy_weight = 0.25, double matchup_weight = 0.25, double synergy_amplification = 1.2, double matchup_amplification = 1.2);
	Dictionary run_controlled_draft_evaluation(const Array &allies, const Array &enemies, const Array &available, const String &stats_dir = "res://stats_output", double base_weight = 0.50, double synergy_weight = 0.25, double matchup_weight = 0.25, double synergy_amplification = 1.2, double matchup_amplification = 1.2);
	Dictionary run_stress_test(const Array &allies, const Array &enemies, const Array &available, const String &stats_dir = "res://stats_output", int64_t num_iterations = 50, double base_weight = 0.50, double synergy_weight = 0.25, double matchup_weight = 0.25, double synergy_amplification = 1.2, double matchup_amplification = 1.2);
	Dictionary run_stress_test_with_perturbations(const Array &allies, const Array &enemies, const Array &available, const String &stats_dir = "res://stats_output", int64_t seed = 42, int64_t scenario_count = 30, double base_weight = 0.50, double synergy_weight = 0.25, double matchup_weight = 0.25, double synergy_amplification = 1.2, double matchup_amplification = 1.2, int64_t scoring_mode = 0, int64_t smoothing_mode = 0);
};

#endif
