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
#include "simulation/sim_movement.hpp"
#include "simulation/simulation_types.hpp"
#include "simulation/sim_status.hpp"
#include "simulation/sim_unit_tick.hpp"
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

	static constexpr double MATCH_DURATION = 60.0;
	static constexpr double DEFAULT_TICK_RATE = 0.1;
	static constexpr int64_t SUDDEN_DEATH_MAX_TICKS = 10000;
	static constexpr double EPSILON = 0.000001;
	static constexpr double RESPAWN_TIME = 10.0;
	static constexpr double ASSIST_WINDOW = 5.0;
	static constexpr double RETARGET_INTERVAL = 0.5;
	static constexpr double SHIELD_DECAY_RATE = 0.1;
	static constexpr double OVERTIME_DAMAGE_BASE_RATE = 0.0001;
	//Increase rate currently unused.
	static constexpr double OVERTIME_DAMAGE_INCREASE_RATE = 0.0001;
	static constexpr double TARGET_SWITCH_LOCK_DURATION = 0.3;
	static constexpr double TARGET_STICKINESS_THRESHOLD = 5.0;
	static constexpr double STICKINESS_RETARGET_BONUS = 0.5;
	static constexpr double REGEN_TICK_INTERVAL = 1.0;
	static constexpr double CASTING_WINDUP = 0.5;
	static constexpr double RANGED_THRESHOLD = 1.0;
	static constexpr double MELEE_CONTACT_BUFFER = 0.1;
	static constexpr double RANGED_CONTACT_BUFFER = 0.1;
	static constexpr double DEFAULT_PROJECTILE_SPEED = 5.0;
	static constexpr double DEFAULT_PROJECTILE_RADIUS = 0.03;
	static constexpr double DEFAULT_PROJECTILE_STUN_DURATION = 0.0;
	static constexpr int64_t SIMULATION_RULES_VERSION = 1;
	static constexpr double WORLD_SIZE = 10.0;
	static constexpr double WORLD_BOUNDARY_MIN = 0.2;
	static constexpr double WORLD_BOUNDARY_MAX = 9.8;
	static constexpr double BOUNDARY_DETECTION_MARGIN = 0.05;
	static constexpr double RECOVERY_VELOCITY = 1.0;
	static constexpr double SEPARATION_RADIUS_RANGED = 0.8;
	static constexpr double SEPARATION_RADIUS_MELEE = 0.25;
	static constexpr double SEPARATION_RANGE_THRESHOLD = 1.5;
	static constexpr double NUDGE_SPEED_MODIFIER = 0.4;
	static constexpr double KITE_SPEED_MODIFIER = 0.5;
	static constexpr double KITE_DURATION = 1.0;
	static constexpr double KITE_DANGER_THRESHOLD = 0.9;
	/// Minimum effective slow multiplier so movement math stays stable at extreme slow percentages.
	static constexpr double SLOW_MOVEMENT_MULTIPLIER_MIN = 0.05;
	static constexpr double DRAFT_X_BASE = 0.9;
	// Positions are stored as IEEE 754 double for deterministic arithmetic.
	static constexpr double ALLY_CRITICAL_HP_RATIO = 0.35;
	static constexpr double REACTIVE_PEEL_BONUS = 25.0;
	static constexpr double ROLE_PRIORITY_GLOBAL_SCALE = 1.0;
	static constexpr double SCORE_HP_WEIGHT_SCALE = 10.0;
	static constexpr double SCORE_THREAT_WEIGHT_SCALE = 5.0;
	static constexpr double SCORE_DISTANCE_WEIGHT_SCALE = 10.0;
	static constexpr double SCORE_KITING_WEIGHT_SCALE = 1.5;
	static constexpr double DISTANCE_EXPONENT = 1.5;
	static constexpr double SPACING_EXPONENT = 1.5;
	static constexpr double THREAT_RESPONSE_RANGE_FALLOFF = 0.6;
	static constexpr double THREAT_BURST_THRESHOLD = 0.1;
	static constexpr double THREAT_BURST_MULTIPLIER = 5.0;
	static constexpr double THREAT_DECAY_DEFAULT = 2.0;
	static constexpr double TARGET_SWITCH_MARGIN = 2.0;

	// Random vertical spawning constants
	static constexpr double SPAWN_Y_MIN = 3.0;
	static constexpr double SPAWN_Y_MAX = 7.0;
	static constexpr double RESPAWN_Y_MIN = 3.0; // Same as initial spawn
	static constexpr double RESPAWN_Y_MAX = 7.0; // Same as initial spawn
	static constexpr double PLAYER_SPAWN_X_BASE = 1.0;
	static constexpr double ENEMY_SPAWN_X_BASE = 9.0;
	static constexpr double SPAWN_Y_STEP = 0.5;
	static constexpr double ASSASSIN_TANK_CONTEXT_PENALTY = 15.0;
	static constexpr double EXECUTE_BONUS_WEIGHT_DEFAULT = 20.0;
	static constexpr double PREY_INCOMING_TARGET_SCALE = 0.75;
	static constexpr double PREY_PERCEIVED_THREAT_SCALE = 0.35;
	static constexpr double PREY_FRONTLINE_SCALE = 0.0;
	static constexpr double AOE_DENSITY_RADIUS = 2.0;
	static constexpr double KITE_TARGET_WINDOW_MIN_FACTOR = 0.7;
	static constexpr double KITE_TARGET_WINDOW_MAX_FACTOR = 1.3;
	static constexpr double SWITCH_COMMIT_WINDOW_SECONDS = 0.18;
	static constexpr double SWITCH_COMMIT_WINDOW_SWING_FRACTION = 0.25;
	static constexpr double SUPPORT_PEEL_BOOST = 2.2;
	static constexpr double SUPPORT_PEEL_THREAT_THRESHOLD = 2.0;
	static constexpr double TARGET_EXECUTE_HP_RATIO = 0.25;
	static constexpr double IN_RANGE_BONUS_DEFAULT = 0.6;
	static constexpr double THREAT_RESPONSE_RANGE_DEFAULT = 0.0;
	static constexpr double UNIT_COLLISION_RADIUS = 0.15;

	static constexpr int SPATIAL_GRID_DIM = 8;
	/// Broad-phase for targeting/density/kite only when a team has this many **alive** units (4+). Standard 5v5 can enter the spatial path under the current benchmark contract.
	static constexpr int SPATIAL_BROAD_PHASE_TEAM_THRESHOLD = 4;
	/// Separation ally scan uses a grid only at this team alive count or above (custom large teams); 5v5 uses brute O(n) with tiny n.
	static constexpr int SPATIAL_SEPARATION_TEAM_THRESHOLD = 6;

	static constexpr const char *CHAMPION_SCHEMA_PATH = "res://fixtures/goldens/champion_schema.json";
	static constexpr const char *MINION_SCHEMA_PATH = "res://fixtures/goldens/minion_schema.json";
	static constexpr const char *BALANCE_PATCHES_PATH = "res://fixtures/goldens/balance_patches.json";
	static constexpr const char *CHAMPION_KITS_PATH = "res://fixtures/goldens/champion_kits.json";

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
	double _tick_rate = DEFAULT_TICK_RATE;
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

	mutable std::array<std::vector<int64_t>, SPATIAL_GRID_DIM * SPATIAL_GRID_DIM> _spatial_buckets;
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
	void _append_team_units(const Array &spawn_specs, const StringName &team, int64_t &next_instance_id, Array &team_comp);
	std::pair<UnitState, UnitStateCold> _build_unit_state(const Dictionary &spawn_spec, const StringName &team, int64_t instance_id);
	TargetingFrameEntry _make_targeting_frame_entry(const UnitState &unit) const;
	void _sync_targeting_frame_index(int64_t index, const UnitState &unit);
	void _sync_targeting_frame_unit(const UnitState &unit);
	UnitStateCold &_uc(UnitState &u);
	const UnitStateCold &_uc(const UnitState &u) const;

	sim::SimWorld _sim_world() const;
	void _bind_sim_host();
	void _bind_sim_exec_hooks();
	sim::SimHostCallbacks _sim_host_callbacks{};
	sim::effects::execution::SimExecCallbacks _sim_exec_callbacks{};
	sim::effects::SimMatchHost _sim_match_host() const;
	sim::combat::CombatHostHooks _combat_host_hooks() const;
	sim::channel::ChannelHostHooks _channel_host_hooks() const;
	sim::unit_tick::UnitTickHostHooks _unit_tick_host_hooks() const;

	friend Dictionary sim_host_execute_effect(void *user_data, const EffectRecord &effect, EffectContext &context);
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
	friend double sim_host_heal_unit(
			void *user_data,
			UnitState &source,
			UnitState &target,
			double amount,
			const StringName &action_kind,
			bool allow_overheal);
	friend UnitState *sim_host_select_ally_target(void *user_data, UnitState &unit);
	friend void sim_host_push_projectile(void *user_data, const ProjectileState &projectile);
	friend std::vector<UnitState *> sim_host_select_targets(void *user_data, UnitState &source, UnitState *target, int64_t target_count, TargetSelectionStrategy strategy, bool include_source, ExcessTargetHandling excess_handling, const StringName &team_filter);
	friend Vector2 sim_host_find_random_spawn_position_near_excluding_with_expansion(void *user_data, double center_x, double center_y, double initial_radius, double max_radius, int64_t exclude_instance_id, const std::vector<Vector2> &pending_positions);
	friend Dictionary sim_host_get_minion_data(void *user_data, const StringName &minion_id);
	friend bool sim_host_debug_combat_trace(void *user_data);

	UnitState *_unit_by_id(int64_t instance_id);
	const UnitState *_unit_by_id(int64_t instance_id) const;
	int64_t _unit_index_by_id(int64_t instance_id) const;
	int64_t _target_index_for_unit(UnitState &unit);
	void _set_current_target(UnitState &unit, const UnitState &target);
	std::vector<int64_t> &_alive_indices_for_team(const StringName &team);
	const std::vector<int64_t> &_alive_indices_for_team(const StringName &team) const;
	void _add_alive_index(const StringName &team, int64_t index);
	void _remove_alive_index(const StringName &team, int64_t index);
	void _prepare_tick_context();
	void _build_role_strategy_cache();
	const UnitStrategy &_strategy_for_unit(const UnitState &unit) const;
	void _emit_trace(const StringName &kind, int64_t src_id, int64_t tgt_id, double val);
	void _adjust_target_pressure(int64_t old_target_id, int64_t new_target_id);
	void _simulate();
	void _step_tick(bool profile_sim);
	void _update_unit(UnitState &unit, bool profile_sim);
	void _prune_assist_window(UnitState &unit);
	void _update_projectiles();
	bool _kite_from_enemies(UnitState &unit, bool profile_sim = false);
	double _score_enemy_target(const UnitState &attacker, const TargetingFrameEntry &enemy, const TargetingFrameEntry *ally_for_peel, const UnitStrategy &strategy, const TickContext &ctx, const TargetScoreContext &score_ctx, double attacker_enemy_distance = -1.0, bool profile_score = false, int64_t enemy_index = -1, ScoreBreakdown *breakdown = nullptr);
	/// When `unit_ally_distance` is >= 0, used as the unit–ally distance (avoids a duplicate sqrt vs `_distance_between`).
	double _score_ally_target(const UnitState &unit, const TargetingFrameEntry &ally, const TeamfightSimulationCore::UnitStrategy &strategy, double unit_ally_distance = -1.0) const;
	/// When `current_target_distance` is >= 0, used instead of recomputing distance to `unit.target_id` for commit-window logic.
	bool _should_switch(const UnitState &unit, double current_score, double new_score, const UnitStrategy &strategy, double current_target_distance = -1.0) const;
	bool _try_cast_ability(UnitState &unit, UnitState &target, double distance);
	bool _try_cast_ultimate(UnitState &unit, UnitState &target, double distance);
	bool _start_cast(UnitState &unit, UnitState &target, double distance, const StringName &action_kind);
	void _resolve_cast(UnitState &unit);
	void _perform_auto_attack(UnitState &unit, UnitState &target, double distance);
	void _move_toward_target(UnitState &unit, UnitState &target);
	void _move_toward_target_with_range(UnitState &unit, UnitState &target, double target_range);
	void _resolve_projectile(const ProjectileState &projectile);
	EffectContext _build_context(UnitState &source, UnitState *target, UnitState *target_ally, double damage, const StringName &action_kind);
	int _effect_bucket_index(const StringName &kind) const;
	const std::vector<EffectRecord> &_collect_effects(const UnitState &unit, const StringName &kind);
	double _apply_attack_modifiers(UnitState &unit, UnitState &target, double distance, double damage);
	double _apply_ability_modifiers(UnitState &unit, UnitState *target, double damage);
	double _apply_ultimate_modifiers(UnitState &unit, UnitState *target, double damage);
	double _apply_damage(UnitState &source, UnitState &target, double damage, const StringName &damage_type, const StringName &action_kind, const EffectContext &context);
	double _defense_multiplier(UnitState &target, UnitState &source, double damage, const StringName &action_kind);
	double _auto_dodge_multiplier(UnitState &target, UnitState &source, double damage);
	double _damage_type_multiplier(const UnitState &target, const StringName &damage_type);
	double _evaluate_multiplier_effect(const EffectRecord &effect, const EffectContext &context, double current_value);
	Dictionary _execute_effect(const EffectRecord &effect, EffectContext &context);
	bool _target_has_status(const UnitState &target, const StringName &status_kind) const;
	bool _effect_record_contains_opcode(const EffectRecord &effect, EffectOpcode opcode) const;
	void _finalize_reflect_passives(UnitState &unit, UnitStateCold &cold);
	void _apply_reflect_buff(UnitState &source, UnitState &target, double pct, double duration, const StringName &action_kind, const StringName &damage_type, const String &reason);
	void _apply_aoe_reflect(UnitState &source, double radius, double pct, double duration, bool all_damage_types);
	void _apply_aoe_reflect_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double pct, double duration, bool all_damage_types, const StringName &action_kind, const String &reason);
	void _maybe_apply_reflect_damage(UnitState &attacker, UnitState &defender, double total_damage_applied, const StringName &damage_type, const EffectContext &context);
	bool _apply_knockback(UnitState &source, UnitState &target, double distance, bool away_from_source);
	bool _apply_aoe_knockback(UnitState &source, double radius, double distance, bool away_from_source);
	void _run_post_attack_effects(UnitState &source, UnitState &target, double damage, const EffectContext &context);
	void _run_post_heal_effects(UnitState &source, UnitState &target, double heal_amount, double heal_gained, const EffectContext &base_context);
	void _run_on_takedown_effects(UnitState &participant, UnitState &victim, double damage_dealt, bool is_kill, const EffectContext &base_context);
	void _apply_stun(UnitState &source, UnitState &target, double duration);
	void _apply_slow(UnitState &source, UnitState &target, double slow_percentage, double duration);
	void _apply_root(UnitState &source, UnitState &target, double duration);
	void _apply_silence(UnitState &source, UnitState &target, double duration, bool block_abilities, bool block_ultimate);
	void _apply_disarm(UnitState &source, UnitState &target, double duration);
	void _apply_stealth(UnitState &source, UnitState &target, double duration, bool break_on_attack, bool break_on_ability, bool break_on_damage_taken);
	void _apply_aoe_slow(UnitState &source, double radius, double slow_percentage, double duration);
	void _apply_aoe_slow_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double slow_percentage, double duration);
	void _apply_aoe_root(UnitState &source, double radius, double duration);
	void _apply_aoe_root_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double duration);
	void _apply_aoe_silence(UnitState &source, double radius, double duration, bool block_abilities, bool block_ultimate);
	void _apply_aoe_silence_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double duration, bool block_abilities, bool block_ultimate);
	void _apply_aoe_disarm(UnitState &source, double radius, double duration);
	void _apply_aoe_disarm_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double duration);
	void _apply_aoe_stun(UnitState &source, double radius, double duration);
	void _apply_aoe_stun_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double duration);
	bool _apply_aoe_knockback_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double distance, bool away_from_source);
	void _apply_aoe_reflect_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double pct, double duration, bool all_damage_types);
	std::vector<UnitState*> _select_targets(UnitState &source, UnitState *target, int64_t target_count, TargetSelectionStrategy strategy, bool include_source, ExcessTargetHandling excess_handling, const StringName &team_filter);
	double _movement_speed_multiplier(const UnitState &unit) const;
	void _touch_damage_source(UnitState &target, int64_t source_id, double incoming_damage);
	double _trigger_ally_defense_effects(UnitState &target, UnitState &source, double damage, const StringName &damage_type, const StringName &action_kind, const EffectContext &context);
	double _distance_between(const UnitState &unit1, const UnitState &unit2) const;
	double _attack_range(const UnitState &unit) const;
	double _effective_attack_range(const UnitState &unit) const;
	int64_t _assign_spawn_slot(const StringName &team);
	Vector2 _get_random_spawn_position(const StringName &team, bool is_respawn);
	Vector2 _find_random_spawn_position_near(double center_x, double center_y, double radius);
	Vector2 _find_random_spawn_position_near_excluding(double center_x, double center_y, double radius, int64_t exclude_instance_id);
	Vector2 _find_random_spawn_position_near_excluding_with_expansion(double center_x, double center_y, double initial_radius, double max_radius, int64_t exclude_instance_id, const std::vector<Vector2> &pending_positions = std::vector<Vector2>());
	bool _position_collides_with_pending(double x, double y, const std::vector<Vector2> &pending_positions) const;
	void _add_shield(UnitState &source, UnitState &target, double amount, const StringName &action_kind);
	void _heal_unit(UnitState &source, UnitState &target, double amount, const StringName &action_kind, bool allow_overheal = false);
	void _restore_mana(UnitState &source, UnitState &target, double amount);
	void _handle_death(UnitState &killer, UnitState &target);
	Vector2 _find_valid_dash_position(double tx, double ty, double new_x, double new_y, double effective_distance, int64_t target_instance_id) const;
	void _tick_periodic_effects(UnitState &unit, double delta);
	void _cleanse_dots(UnitState &unit, const StringName &effect_type_filter);
	void _clear_periodic_effects(UnitState &unit);
	UnitState *_select_enemy_target(UnitState &unit, bool profile_sim);
	UnitState *_select_ally_target(UnitState &unit);
	bool _position_collides_with_unit(double x, double y, int64_t exclude_instance_id) const;
	void _release_spawn_slot(const StringName &team, int64_t slot_index);
	StringName _determine_winner() const;
	void _respawn_unit(UnitState &unit);
	Dictionary _build_summary();
	Dictionary _build_stats_summary();
	sim::match::MatchSnapshot _match_snapshot() const;
	
	// Pending spawns queue for mid-tick unit creation
	std::vector<PendingSpawn> _pending_spawns;
	
	// Pending spawns processing
	void _process_pending_spawns();

	// Stack debugging functions
	void _debug_print_stack_state(const UnitState &unit) const;
	String _join_team_names(const Array &team) const;
	void _apply_aoe_taunt(UnitState &source, double radius, double duration);
	void _apply_aoe_taunt_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double duration);
	double _apply_aoe_damage(UnitState &source, UnitState &center_source, double damage, double radius, const StringName &damage_type, const StringName &reason, const StringName &action_kind);
	double _apply_aoe_damage_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double damage, const StringName &damage_type, const StringName &action_kind);
	double _apply_aoe_damage_shape_per_target(UnitState &source, UnitState *target, const EffectRecord &effect, double damage_ratio, double flat_amount, double max_hp_ratio, double splash_ratio, const StringName &damage_type, const StringName &action_kind);
	void _apply_dot(UnitState &source, UnitState &target, double attack_damage_ratio, double max_hp_ratio, double flat_amount, double duration, double tick_interval, const StringName &damage_type, const StringName &stacking_mode, int max_stacks, const StringName &effect_type, const String &reason, const StringName &action_kind, bool is_dynamic = false);
	void _apply_hot(UnitState &source, UnitState &target, double max_hp_ratio, double current_hp_ratio, double missing_hp_ratio, double flat_amount, double duration, double tick_interval, const StringName &stacking_mode, int max_stacks, bool allow_overheal, const StringName &effect_type, const String &reason, const StringName &action_kind, bool is_dynamic = false);
	void _apply_aoe_dot(UnitState &source, double radius, double attack_damage_ratio, double max_hp_ratio, double flat_amount, double duration, double tick_interval, const StringName &damage_type, const StringName &stacking_mode, int max_stacks, const StringName &effect_type, const String &reason, bool target_self, const StringName &action_kind, bool is_dynamic = false);
	void _apply_aoe_dot_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double attack_damage_ratio, double max_hp_ratio, double flat_amount, double duration, double tick_interval, const StringName &damage_type, const StringName &stacking_mode, int max_stacks, const StringName &effect_type, const String &reason, bool target_self, const StringName &action_kind, bool is_dynamic = false);
	void _apply_aoe_hot(UnitState &source, double radius, double max_hp_ratio, double current_hp_ratio, double missing_hp_ratio, double flat_amount, double duration, double tick_interval, const StringName &stacking_mode, int max_stacks, bool allow_overheal, const StringName &effect_type, const String &reason, bool target_self, const StringName &action_kind, bool is_dynamic = false);
	void _apply_aoe_hot_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double max_hp_ratio, double current_hp_ratio, double missing_hp_ratio, double flat_amount, double duration, double tick_interval, const StringName &stacking_mode, int max_stacks, bool allow_overheal, const StringName &effect_type, const String &reason, bool target_self, const StringName &action_kind, bool is_dynamic = false);

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

	void _spatial_ensure_stamp_size() const;
	void _spatial_clear_buckets() const;
	double _spatial_cell_size() const;
	int _spatial_flat_index(double x, double y) const;
	void _spatial_add_alive_team(const StringName &team) const;
	void _spatial_next_generation() const;
	void _spatial_stamp_circle(double cx, double cy, double radius, const StringName &team) const;
	void _spatial_stamp_kite_threat(double cx, double cy, double danger_radius) const;
	void _spatial_stamp_separation_candidates(double cx, double cy, double radius, const StringName &team, int64_t self_instance_id) const;
	bool _spatial_stamp_has(int64_t unit_index) const;
	void _spatial_fill_buckets_for_indices(const std::vector<int64_t> &indices) const;
	int _spatial_count_neighbors_in_grid(int64_t self_index, double cx, double cy, double radius) const;
	bool _use_spatial_broad_phase() const;

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
	Dictionary run_generated_matches_stats_partial(int64_t base_seed, int64_t batch_count, int64_t team_size, bool include_match_log, double tick_rate = DEFAULT_TICK_RATE);

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
