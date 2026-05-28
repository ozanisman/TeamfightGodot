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
#include "stat_definitions.hpp"

#include "simulation/sim_aoe.hpp"
#include "simulation/sim_catalog.hpp"
#include "simulation/sim_channel.hpp"
#include "simulation/sim_combat.hpp"
#include "simulation/sim_constants.hpp"
#include "simulation/sim_effects_compile.hpp"
#include "simulation/sim_effects_exec.hpp"
#include "simulation/sim_effects_host.hpp"
#include "simulation/sim_match.hpp"
#include "simulation/sim_match_lifecycle.hpp"
#include "simulation/sim_match_loop.hpp"
#include "simulation/sim_match_roster.hpp"
#include "simulation/sim_movement.hpp"
#include "simulation/sim_unit_builder.hpp"
#include "simulation/simulation_types.hpp"
#include "simulation/sim_status.hpp"
#include "simulation/sim_unit_tick.hpp"
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

	using AoShapeKind = sim::AoShapeKind;
	using AoAnchorKind = sim::AoAnchorKind;
	using AoShapeParams = sim::AoShapeParams;
	using EffectRecord = sim::EffectRecord;
	using EffectContext = sim::EffectContext;
	using StackBehavior = sim::StackBehavior;
	using PendingSpawn = sim::PendingSpawn;
	using PassiveReflectEntry = sim::PassiveReflectEntry;
	using UnitStateCold = sim::UnitStateCold;
	using UnitState = sim::UnitState;
	using RolePriorityConfig = sim::RolePriorityConfig;
	using StrategyRolePriorities = sim::StrategyRolePriorities;
	using ScoreBreakdown = sim::ScoreBreakdown;
	using UnitStrategy = sim::UnitStrategy;
	using StrategyConfig = sim::StrategyConfig;
	using TickContext = sim::TickContext;
	using TargetingFrameEntry = sim::TargetingFrameEntry;
	using TargetScoreContext = sim::TargetScoreContext;
	using TraceEvent = sim::TraceEvent;
	using BalancePatch = sim::BalancePatch;
	using ProjectileState = sim::ProjectileState;
	using TargetSelectionStrategy = sim::TargetSelectionStrategy;
	using ExcessTargetHandling = sim::ExcessTargetHandling;
	using EffectOpcode = sim::EffectOpcode;
	using ViewerFxEvent = sim::ViewerFxEvent;

	std::vector<UnitState> _units;
	/// Parallel to `_units` (same length and indices). Push or clear together with `_units` only.
	/// `_uc(u)` is only valid when `u` references an element stored in `_units` (never a stack copy of `UnitState`).
	std::vector<UnitStateCold> _unit_cold;
	std::vector<ProjectileState> _projectiles;
	std::vector<ProjectileState> _scratch_projectiles;
	std::vector<ProjectileState> _active_projectiles;
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
	
	// Spawn slot tracking per team (indices into spawn_points array)
	std::vector<bool> _player_spawn_slots_used;
	std::vector<bool> _enemy_spawn_slots_used;
	sim::catalog::CatalogState _catalog;
	std::unordered_map<int64_t, int64_t> _unit_index_map;
	std::vector<int64_t> _alive_player_indices;
	std::vector<int64_t> _alive_enemy_indices;
	std::unordered_set<int64_t> _alive_player_indices_set;
	std::unordered_set<int64_t> _alive_enemy_indices_set;
	std::vector<TargetingFrameEntry> _targeting_frame;
	std::array<UnitStrategy, ROLE_SLOT_COUNT> _role_strategy_cache_by_slot{};
	UnitStrategy _default_strategy;
	TickContext _tick_ctx;
	std::vector<TraceEvent> _trace_buffer;
	static constexpr size_t TRACE_BUFFER_CAP = 4096;
	bool _debug_combat_trace = false;
	bool _debug_targeting_scoring = false;

	static constexpr size_t VIEWER_FX_CAP = 256;
	std::vector<ViewerFxEvent> _viewer_fx_events;

	void _viewer_fx_push(const ViewerFxEvent &p_ev);
	void _viewer_record_damage_fx(const UnitState &p_source, const UnitState &p_target, double p_total_damage, const StringName &p_action_kind, const StringName &p_damage_type);
	void _viewer_record_heal_fx(const UnitState &p_target, double p_amount);
	void _viewer_record_shield_fx(const UnitState &p_target, double p_amount);
	void _viewer_record_aoe_ring_fx(const UnitState &p_source, const UnitState &p_center, double p_radius, const StringName &p_kind);
	void _viewer_record_aoe_shape_fx(const UnitState &p_source, const UnitState *p_target, const AoShapeParams &params, const StringName &kind);
	void _viewer_record_hot_status_fx(const UnitState &p_target, double p_duration, const StringName &p_effect_type);
	void _viewer_record_passive_aoe_fx(const UnitState &p_unit, double p_radius, const StringName &p_passive_id);
	String _viewer_state_string(const UnitState &p_u) const;
	void _print_score_breakdown(const ScoreBreakdown &breakdown, const StringName &attacker_archetype, const StringName &enemy_archetype) const;

	mutable std::array<std::vector<int64_t>, sim::SPATIAL_GRID_DIM * sim::SPATIAL_GRID_DIM> _spatial_buckets;
	mutable std::vector<uint32_t> _spatial_stamp;
	mutable uint32_t _spatial_generation = 1;

	/// TEAMFIGHT_SIM_PROFILE env: per-_simulate() wall time (nanoseconds) by _step_tick section.
	uint64_t _sim_profile_ns_projectiles = 0;
	uint64_t _sim_profile_ns_prepare_tick_ctx = 0;
	uint64_t _sim_profile_ns_refresh_pressure_pre = 0;
	uint64_t _sim_profile_ns_update_units = 0;
	uint64_t _sim_profile_ns_refresh_pressure_post = 0;
	int64_t _sim_profile_tick_count = 0;
	/// Sub-timers for _update_unit (summed over all units each tick; same divisor as tick_count).
	uint64_t _sim_profile_uu_dead_respawn = 0;
	uint64_t _sim_profile_uu_cooldowns_cc = 0;
	uint64_t _sim_profile_uu_separation = 0;
	uint64_t _sim_profile_uu_threat_and_assist = 0;
	uint64_t _sim_profile_uu_regen_on_tick = 0;
	uint64_t _sim_profile_uu_casting = 0;
	uint64_t _sim_profile_uu_targeting = 0;
	uint64_t _sim_profile_uu_combat = 0;
	uint64_t _sim_profile_uu_movement = 0;
	/// Sub-timers inside `_score_enemy_target` (summed over calls); use with `_sim_profile_se_calls` for per-call avg.
	uint64_t _sim_profile_se_base = 0;
	int64_t _sim_profile_se_calls = 0;
	/// Sub-timers for uu_combat
	uint64_t _sim_profile_uc_attack_cooldown = 0;
	uint64_t _sim_profile_uc_distance_calc = 0;
	uint64_t _sim_profile_uc_hit_validation = 0;
	uint64_t _sim_profile_uc_damage_apply = 0;
	uint64_t _sim_profile_uc_auto_attack = 0;
	uint64_t _sim_profile_uc_ability = 0;
	/// Sub-timers for uu_cooldowns_cc
	uint64_t _sim_profile_ucc_attack_cd = 0;
	uint64_t _sim_profile_ucc_ability_cd = 0;
	uint64_t _sim_profile_ucc_retarget = 0;
	uint64_t _sim_profile_ucc_target_switch = 0;
	uint64_t _sim_profile_ucc_stun = 0;
	uint64_t _sim_profile_ucc_slow = 0;
	uint64_t _sim_profile_ucc_root = 0;
	uint64_t _sim_profile_ucc_silence = 0;
	uint64_t _sim_profile_ucc_disarm = 0;
	uint64_t _sim_profile_ucc_stealth = 0;
	uint64_t _sim_profile_ucc_shield = 0;
	uint64_t _sim_profile_ucc_reflect = 0;
	uint64_t _sim_profile_ucc_taunt = 0;
	uint64_t _sim_profile_ucc_forced_target = 0;
	/// Sub-timers for ns_prepare_tick_ctx
	uint64_t _sim_profile_ctx_team_centers = 0;
	uint64_t _sim_profile_ctx_role_classification = 0;
	uint64_t _sim_profile_ctx_targeting_sync = 0;
	uint64_t _sim_profile_ctx_spatial_grid = 0;
	uint64_t _sim_profile_ctx_density = 0;
	/// Sub-timers for uu_movement
	uint64_t _sim_profile_um_kiting = 0;
	uint64_t _sim_profile_um_kiting_spatial = 0;
	uint64_t _sim_profile_um_kiting_brute = 0;
	uint64_t _sim_profile_um_toward = 0;
	uint64_t _sim_profile_um_boundary = 0;
	uint64_t _sim_profile_um_nudge = 0;
	/// Sub-timers for uu_regen_on_tick
	uint64_t _sim_profile_ur_hp_mana = 0;
	uint64_t _sim_profile_ur_effects = 0;
	uint64_t _sim_profile_ur_periodic = 0;
	uint64_t _sim_profile_ur_channel = 0;
	bool _sim_profile_active = false;
	bool _sim_profile_targeting_active = false;
	int64_t _sim_profile_tgt_retarget_keeps = 0;
	int64_t _sim_profile_tgt_enemy_scans = 0;
	int64_t _sim_profile_tgt_candidates_scored = 0;
	int64_t _sim_profile_tgt_candidates_prefix_pruned = 0;
	int64_t _sim_profile_tgt_ally_scans = 0;
	int64_t _sim_profile_tgt_frame_syncs = 0;
	int64_t _sim_profile_tgt_ties_adjusted = 0;
	int64_t _sim_profile_tgt_ties_raw = 0;
	int64_t _sim_profile_tgt_ties_distance = 0;
	int64_t _sim_profile_tgt_ties_instance = 0;

	static bool _sim_profile_env_enabled();
	void _sim_profile_reset();
	void _sim_profile_emit_json_stderr() const;

	void _reset_runtime_state();
	double _randf();
	Dictionary _load_json_file(const String &path) const;
	Dictionary _load_json_file_if_exists(const String &path) const;
	Dictionary _effect_to_dict(const Variant &effect) const;
	void _ensure_catalog_loaded();
	sim::catalog::CatalogHooks _catalog_hooks() const;
	static EffectRecord _catalog_compile_effect(void *user_data, const Dictionary &effect);
	Dictionary _effective_champion_for(const StringName &archetype_id) const;
	using ParamTracker = sim::effects::compile::ParamTracker;
	
	EffectRecord _compile_effect(const Dictionary &effect) const;
	std::vector<EffectRecord> _compile_effect_array(const Array &effects) const;
	Dictionary _coerce_match_input(const Variant &match_input) const;
	void _populate_runtime_state(const Dictionary &match_input);
	TargetingFrameEntry _make_targeting_frame_entry(const UnitState &unit) const;
	void _sync_targeting_frame_index(int64_t index, const UnitState &unit);
	void _sync_targeting_frame_unit(const UnitState &unit);
	UnitStateCold &_uc(UnitState &u);
	const UnitStateCold &_uc(const UnitState &u) const;

	sim::SimWorld _sim_world() const;
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

	friend void sim_host_handle_death(void *user_data, UnitState &killer, UnitState &target);
	friend void sim_host_sync_targeting_frame_unit(void *user_data, const UnitState &unit);
	friend void sim_host_sync_targeting_frame_index(void *user_data, int64_t index, const UnitState &unit);
	friend void sim_host_emit_trace(void *user_data, const StringName &kind, int64_t src_id, int64_t tgt_id, double val);
	friend void sim_host_viewer_record_damage_fx(
			void *user_data,
			const UnitState &source,
			const UnitState &target,
			double total_damage,
			const StringName &action_kind,
			const StringName &damage_type);
	friend UnitState *sim_host_select_ally_target(void *user_data, UnitState &unit);
	friend bool sim_host_debug_combat_trace(void *user_data);

	UnitState *_unit_by_id(int64_t instance_id);
	const UnitState *_unit_by_id(int64_t instance_id) const;
	int64_t _unit_index_by_id(int64_t instance_id) const;
	void _prepare_tick_context();
	void _build_role_strategy_cache();
	const UnitStrategy &_strategy_for_unit(const UnitState &unit) const;
	void _emit_trace(const StringName &kind, int64_t src_id, int64_t tgt_id, double val);
	sim::match::MatchLoopState _match_loop_state();
	sim::match::MatchLoopHost _match_loop_host() const;
	void _update_unit(UnitState &unit, bool profile_sim);
	void _prune_assist_window(UnitState &unit);
	void _update_projectiles();
	/// When `unit_ally_distance` is >= 0, used as the unit–ally distance (avoids a duplicate sqrt vs `_distance_between`).
	/// When `current_target_distance` is >= 0, used instead of recomputing distance to `unit.target_id` for commit-window logic.
	bool _effect_record_contains_opcode(const EffectRecord &effect, EffectOpcode opcode) const;
	void _finalize_reflect_passives(UnitState &unit, UnitStateCold &cold);
	double _distance_between(const UnitState &unit1, const UnitState &unit2) const;
	double _effective_attack_range(const UnitState &unit) const;
	Vector2 _get_random_spawn_position(const StringName &team, bool is_respawn);
	bool _position_collides_with_pending(double x, double y, const std::vector<Vector2> &pending_positions) const;
	Vector2 _find_random_spawn_position_near(double center_x, double center_y, double radius);
	Vector2 _find_random_spawn_position_near_excluding(double center_x, double center_y, double radius, int64_t exclude_instance_id);
	Vector2 _find_random_spawn_position_near_excluding_with_expansion(double center_x, double center_y, double initial_radius, double max_radius, int64_t exclude_instance_id, const std::vector<Vector2> &pending_positions = std::vector<Vector2>());
	Vector2 _find_valid_dash_position(double tx, double ty, double new_x, double new_y, double effective_distance, int64_t target_instance_id) const;
	UnitState *_select_enemy_target(UnitState &unit, bool profile_sim);
	UnitState *_select_ally_target(UnitState &unit);
	bool _position_collides_with_unit(double x, double y, int64_t exclude_instance_id) const;
	StringName _determine_winner() const;
	Dictionary _build_summary();
	Dictionary _build_stats_summary();
	sim::match::MatchSnapshot _match_snapshot() const;
	
	// Pending spawns queue for mid-tick unit creation
	std::vector<PendingSpawn> _pending_spawns;

	// Stack debugging functions
	void _debug_print_stack_state(const UnitState &unit) const;
	void _log_sudden_death_draw();
	String _join_team_names(const Array &team) const;

	Dictionary _champion_for(const StringName &archetype_id) const;

	Vector2 _resolve_aoe_direction(const UnitState &source, const AoShapeParams &params, const UnitState *target_override = nullptr) const;

	/// Parameters for circular AoE iteration over an alive-team index list (`_alive_*_indices`).
	/// `spatial_team` must match `UnitState::team` for units referenced by `indices` (used by broad-phase stamp).
	using AoCircleIterationParams = sim::AoCircleIterationParams;
	using AoShapeIterationParams = sim::AoShapeIterationParams;


	template<typename Fn>
	void _for_each_unit_in_circle(const AoCircleIterationParams &p, Fn &&fn) {
		sim::SimWorld w = _sim_world();
		sim::for_each_unit_in_circle(w, p, std::forward<Fn>(fn));
	}

	template<typename Fn>
	void _for_each_unit_in_shape(const AoShapeIterationParams &p, Fn &&fn) {
		sim::SimWorld w = _sim_world();
		sim::for_each_unit_in_shape(w, p, std::forward<Fn>(fn), [&](const UnitState &source, const AoShapeParams &params, const UnitState *target_override) {
			return sim::status::resolve_aoe_direction(w, source, params, target_override);
		});
	}

	static inline double _distance_between_coords(double x1, double y1, double x2, double y2) {
		return Math::sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
	}


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
};

#endif
