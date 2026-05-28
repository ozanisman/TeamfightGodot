#include "teamfight_simulation_core.hpp"

#include "simulation/sim_aoe.hpp"
#include "simulation/sim_catalog.hpp"
#include "simulation/sim_channel.hpp"
#include "simulation/sim_combat.hpp"
#include "simulation/sim_effects_compile.hpp"
#include "simulation/sim_effects_exec.hpp"
#include "simulation/sim_effects_host.hpp"
#include "simulation/sim_damage.hpp"
#include "simulation/sim_match.hpp"
#include "simulation/sim_match_lifecycle.hpp"
#include "simulation/sim_coordinator_host.hpp"
#include "simulation/sim_profile.hpp"
#include "simulation/sim_match_benchmark.hpp"
#include "simulation/sim_match_loop.hpp"
#include "simulation/sim_match_roster.hpp"
#include "simulation/sim_movement.hpp"
#include "simulation/sim_periodic.hpp"
#include "simulation/sim_spatial.hpp"
#include "simulation/sim_stats.hpp"
#include "simulation/sim_stats_modifiers.hpp"
#include "simulation/sim_status.hpp"
#include "simulation/sim_targeting.hpp"
#include "simulation/sim_targeting_strategies.hpp"
#include "simulation/sim_unit_tick.hpp"
#include "simulation/sim_world.hpp"

#define X(name, def, min_val, max_val) using sim::get_effective_##name;
STAT_LIST
#undef X

using namespace sim;

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/random_number_generator.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>
#include <utility>

static std::atomic<int64_t> s_benchmark_progress_done{0};
static std::atomic<bool> s_sim_profile_force_enabled{false};
static std::atomic<bool> s_targeting_profile_force_enabled{false};

namespace {
uint64_t sim_profile_elapsed_ns(std::chrono::steady_clock::time_point t0) {
	using namespace std::chrono;
	return duration_cast<nanoseconds>(steady_clock::now() - t0).count();
}

struct SimProfileAccScope {
	bool enabled = false;
	uint64_t *accum = nullptr;
	std::chrono::steady_clock::time_point t0{};
	explicit SimProfileAccScope(bool profile_sim, uint64_t &out_accum)
			: enabled(profile_sim), accum(&out_accum) {
		if (enabled) {
			t0 = std::chrono::steady_clock::now();
		}
	}
	~SimProfileAccScope() {
		if (enabled && accum != nullptr) {
			*accum += sim_profile_elapsed_ns(t0);
		}
	}
	SimProfileAccScope(const SimProfileAccScope &) = delete;
	SimProfileAccScope &operator=(const SimProfileAccScope &) = delete;
};

static bool bench_phases_env_enabled() {
	const char *v = std::getenv("TEAMFIGHT_BENCH_PHASES");
	if (v == nullptr || v[0] == '\0') {
		return false;
	}
	return !(v[0] == '0' && v[1] == '\0');
}
} // namespace

EffectRecord TeamfightSimulationCore::_compile_effect(const Dictionary &effect) const {
	return sim::effects::compile::compile_effect(effect);
}

std::vector<EffectRecord> TeamfightSimulationCore::_compile_effect_array(const Array &effects) const {
	return sim::effects::compile::compile_effect_array(effects);
}

bool TeamfightSimulationCore::_effect_record_contains_opcode(const EffectRecord &effect, EffectOpcode opcode) const {
	const int64_t want = static_cast<int64_t>(opcode);
	if (effect.opcode == want) {
		return true;
	}
	for (const EffectRecord &child : effect.children) {
		if (_effect_record_contains_opcode(child, opcode)) {
			return true;
		}
	}
	return false;
}

UnitStateCold &TeamfightSimulationCore::_uc(UnitState &u) {
	return _unit_cold[static_cast<size_t>(&u - _units.data())];
}

const UnitStateCold &TeamfightSimulationCore::_uc(const UnitState &u) const {
	return _unit_cold[static_cast<size_t>(&u - _units.data())];
}

void TeamfightSimulationCore::_finalize_reflect_passives(UnitState &unit, UnitStateCold &cold) {
	// TODO: Rework reflect_type to be selectable per damage type (physical, magic, true) instead of binary "all" vs "physical"
	// Or remove entirely and use separate reflect effects per damage type
	cold.passive_reflect_entries.clear();
	for (const EffectRecord &eff : cold.passive_effects[1]) {
		if (eff.opcode == EFFECT_OPCODE_REFLECT_DAMAGE) {
			PassiveReflectEntry entry;
			entry.percentage = eff.scalar0;
			entry.damage_type = eff.int0 == 1 ? StringName("all") : StringName("physical");
			entry.action_kind = StringName("passive");
			cold.passive_reflect_entries.push_back(entry);
		}
	}
}


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
			&self._spatial_generation);
}

void TeamfightSimulationCore::_refresh_match_context() {
	sim::unit_builder::UnitBuilderHost &host = _match_ctx.unit_builder_host;
	host.user_data = this;
	host.catalog = &_catalog;
	host.compile_effect = &sim_host_compile_effect;
	host.effective_champion_for = &sim_host_effective_champion_for;
	host.finalize_reflect_passives = &sim_host_finalize_reflect_passives;
	host.assign_spawn_slot = &sim_host_assign_spawn_slot;
	host.get_random_spawn_position = &sim_host_get_random_spawn_position;

	sim::match::MatchLoopHost &loop = _match_ctx.loop_host;
	loop.user_data = this;
	loop.update_projectiles = &sim_host_match_update_projectiles;
	loop.prepare_tick_context = &sim_host_match_prepare_tick_context;
	loop.update_unit = &sim_host_match_update_unit;
	loop.match_roster_state = &sim_host_match_roster_state;
	loop.unit_builder_host = &sim_host_match_unit_builder_host;
	loop.profile_reset = &sim_host_match_profile_reset;
	loop.profile_emit_json_stderr = &sim_host_match_profile_emit_json_stderr;
	loop.profile_env_enabled = &sim_host_match_profile_env_enabled;
	loop.targeting_profile_enabled = &sim_host_match_targeting_profile_enabled;
	loop.log_sudden_death_draw = &sim_host_match_log_sudden_death_draw;
}

sim::unit_builder::UnitBuilderHost TeamfightSimulationCore::_unit_builder_host() const {
	return _match_ctx.unit_builder_host;
}

sim::match_roster::MatchRosterState TeamfightSimulationCore::_match_roster_state() {
	return {
		_max_instance_id,
		_alive_player_indices_set,
		_alive_enemy_indices_set,
		_pending_spawns,
	};
}

sim::match::MatchScoreState TeamfightSimulationCore::_match_score_state() {
	sim::match::MatchScoreState score;
	score.player_kills = &_player_kills;
	score.enemy_kills = &_enemy_kills;
	score.tick_ctx = &_tick_ctx;
	score.time = _time;
	return score;
}

sim::match::SpawnSlotState TeamfightSimulationCore::_spawn_slot_state() {
	return {
		_player_spawn_slots_used,
		_enemy_spawn_slots_used,
		&sim_host_rand_uint32,
		&sim_host_get_random_spawn_position,
		this,
	};
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
	state.profile_active = &_sim_profile_active;
	state.profile_targeting_active = &_sim_profile_targeting_active;
	return state;
}

sim::match::MatchLoopHost TeamfightSimulationCore::_match_loop_host() const {
	return _match_ctx.loop_host;
}

void TeamfightSimulationCore::_bind_effect_exec_bindings() {
	_effect_exec_bindings.units = &_units;
	_effect_exec_bindings.unit_cold = &_unit_cold;
	_effect_exec_bindings.unit_index_map = &_unit_index_map;
	_effect_exec_bindings.targeting_frame = &_targeting_frame;
	_effect_exec_bindings.tick_ctx = &_tick_ctx;
	_effect_exec_bindings.alive_player_indices = &_alive_player_indices;
	_effect_exec_bindings.alive_enemy_indices = &_alive_enemy_indices;
	_effect_exec_bindings.time = &_time;
	_effect_exec_bindings.tick_rate = &_tick_rate;
	_effect_exec_bindings.spatial_buckets = &_spatial_buckets;
	_effect_exec_bindings.spatial_stamp = &_spatial_stamp;
	_effect_exec_bindings.spatial_generation = &_spatial_generation;
	_effect_exec_bindings.match_host.pending_spawns = &_pending_spawns;
	_effect_exec_bindings.match_host.projectiles = &_projectiles;
	_effect_exec_bindings.match_host.max_instance_id = &_max_instance_id;
	_effect_exec_bindings.match_host.catalog = &_catalog;
	_effect_exec_bindings.exec_callbacks = &_sim_exec_callbacks;
}

void TeamfightSimulationCore::_bind_sim_host() {
	_bind_effect_exec_bindings();
	_sim_host_callbacks.user_data = this;
	_sim_host_callbacks.effect_exec = &_effect_exec_bindings;
	_sim_host_callbacks.execute_effect = &sim::effects::host_execute_effect;
	_sim_host_callbacks.handle_death = &sim_host_handle_death;
	_sim_host_callbacks.sync_targeting_frame_unit = &sim_host_sync_targeting_frame_unit;
	_sim_host_callbacks.sync_targeting_frame_index = &sim_host_sync_targeting_frame_index;
	_sim_host_callbacks.emit_trace = &sim_host_emit_trace;
	_sim_host_callbacks.viewer_record_damage_fx = &sim_host_viewer_record_damage_fx;
	_sim_host_callbacks.viewer_record_hot_status_fx = &sim_host_viewer_record_hot_status_fx;
	_sim_host_callbacks.randf = &sim_host_randf;
	_viewer_hooks.user_data = this;
	_viewer_hooks.record_damage_fx = &sim_host_viewer_record_damage_fx;
	_viewer_hooks.record_heal_fx = &sim_host_viewer_record_heal_fx;
	_viewer_hooks.record_shield_fx = &sim_host_viewer_record_shield_fx;
	_viewer_hooks.record_hot_status_fx = &sim_host_viewer_record_hot_status_fx;
	_viewer_hooks.record_aoe_shape_fx = &sim_host_viewer_record_aoe_shape_fx;
	_viewer_hooks.record_passive_aoe_fx = &sim_host_viewer_record_passive_aoe_fx;
	_viewer_hooks.sync_targeting_frame_unit = &sim_host_sync_targeting_frame_unit;
	_sim_host_callbacks.viewer_hooks = &_viewer_hooks;
	_bind_sim_exec_hooks();
}

sim::combat::CombatHostHooks TeamfightSimulationCore::_combat_host_hooks() const {
	sim::combat::CombatHostHooks hooks;
	hooks.user_data = const_cast<TeamfightSimulationCore *>(this);
	hooks.select_ally_target = &sim_host_select_ally_target;
	return hooks;
}

sim::unit_tick::UnitTickHostHooks TeamfightSimulationCore::_unit_tick_host_hooks() const {
	sim::unit_tick::UnitTickHostHooks hooks;
	hooks.user_data = const_cast<TeamfightSimulationCore *>(this);
	hooks.respawn_unit = &sim_host_unit_tick_respawn;
	hooks.strategy_for_unit = &sim_host_unit_tick_strategy;
	hooks.select_enemy_target = &sim_host_unit_tick_select_enemy_target;
	hooks.select_ally_target = &sim_host_unit_tick_select_ally_target;
	hooks.prune_assist_window = &sim_host_unit_tick_prune_assist_window;
	return hooks;
}

sim::channel::ChannelHostHooks TeamfightSimulationCore::_channel_host_hooks() const {
	sim::channel::ChannelHostHooks hooks;
	hooks.user_data = const_cast<TeamfightSimulationCore *>(this);
	hooks.debug_combat_trace = &sim_host_debug_combat_trace;
	return hooks;
}

void TeamfightSimulationCore::_bind_sim_exec_hooks() {
	_sim_exec_callbacks.debug_combat_trace = &sim_host_debug_combat_trace;
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
	ClassDB::bind_method(D_METHOD("flush_stdio"), &TeamfightSimulationCore::flush_stdio);
	ClassDB::bind_method(D_METHOD("benchmark_progress_reset", "total_matches"), &TeamfightSimulationCore::benchmark_progress_reset);
	ClassDB::bind_method(D_METHOD("benchmark_progress_add", "delta_matches"), &TeamfightSimulationCore::benchmark_progress_add);
	ClassDB::bind_method(D_METHOD("benchmark_progress_read"), &TeamfightSimulationCore::benchmark_progress_read);
	ClassDB::bind_method(D_METHOD("sim_profile_set_enabled", "enabled"), &TeamfightSimulationCore::sim_profile_set_enabled);
	ClassDB::bind_method(D_METHOD("targeting_profile_set_enabled", "enabled"), &TeamfightSimulationCore::targeting_profile_set_enabled);
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

double TeamfightSimulationCore::_randf() {
	return _rng.random_random();
}

void TeamfightSimulationCore::_reset_runtime_state() {
	// Clear stat modifiers for all units before clearing the vectors
	for (UnitState &unit : _units) {
		sim::stats_modifiers::clear_all_stat_modifiers(unit);
	}

	_units.clear();
	_unit_cold.clear();
	_projectiles.clear();
	_projectiles.reserve(128);
	_scratch_projectiles.clear();
	_active_projectiles.clear();
	_scratch_critical_allies.clear();


	_summary_unit_stats.clear();
	_summary_cache.clear();
	_time = 0.0;
	_tick = 0;
	_sudden_death_ticks = 0;
	_tick_rate = DEFAULT_TICK_RATE;
	_seed = 0;
	_winner_team = StringName("draw");
	_record_events = false;
	_player_comp.clear();
	_enemy_comp.clear();
	_player_kills = 0;
	_enemy_kills = 0;
	_max_instance_id = 0;
	
	// Reset spawn slot tracking
	_player_spawn_slots_used.clear();
	_enemy_spawn_slots_used.clear();
	
	_unit_index_map.clear();
	_alive_player_indices.clear();
	_alive_enemy_indices.clear();
	_alive_player_indices_set.clear();
	_alive_enemy_indices_set.clear();
	_targeting_frame.clear();
	_role_strategy_cache_by_slot.fill(UnitStrategy());
	_default_strategy = UnitStrategy();
	_tick_ctx.density_by_unit_index.clear();
	_tick_ctx.player_backliner_indices.clear();
	_tick_ctx.enemy_backliner_indices.clear();
	_tick_ctx.player_backliner_alive_count = 0;
	_tick_ctx.enemy_backliner_alive_count = 0;
	_tick_ctx.has_player_center = false;
	_tick_ctx.has_enemy_center = false;
	_trace_buffer.clear();
	_debug_combat_trace = false;
	_debug_targeting_scoring = false;
	_viewer_fx.clear();
	_pending_spawns.clear();
	if (!_match_context_hosts_cached) {
		_refresh_match_context();
		_match_context_hosts_cached = true;
	}
}

void TeamfightSimulationCore::_ensure_catalog_loaded() {
	sim::catalog::ensure_loaded(_catalog, _catalog_hooks());
}

sim::catalog::CatalogHooks TeamfightSimulationCore::_catalog_hooks() const {
	sim::catalog::CatalogHooks hooks;
	hooks.user_data = const_cast<TeamfightSimulationCore *>(this);
	hooks.compile_effect = &_catalog_compile_effect;
	return hooks;
}

EffectRecord TeamfightSimulationCore::_catalog_compile_effect(void *user_data, const Dictionary &effect) {
	return static_cast<TeamfightSimulationCore *>(user_data)->_compile_effect(effect);
}

Dictionary TeamfightSimulationCore::_effective_champion_for(const StringName &archetype_id) const {
	return sim::catalog::effective_champion_for(_catalog, archetype_id);
}

Dictionary TeamfightSimulationCore::_champion_for(const StringName &archetype_id) const {
	return sim::catalog::champion_for(_catalog, archetype_id);
}

TeamfightSimulationCore::TargetingFrameEntry TeamfightSimulationCore::_make_targeting_frame_entry(const UnitState &unit) const {
	return sim::targeting::make_targeting_frame_entry(unit);
}

void TeamfightSimulationCore::_sync_targeting_frame_index(int64_t index, const UnitState &unit) {
	if (_sim_profile_targeting_active) {
		_sim_profile_counters.tgt_frame_syncs += 1;
	}
	sim::SimWorld w = _sim_world();
	sim::targeting::sync_targeting_frame_index(w, index, unit);
}

void TeamfightSimulationCore::_sync_targeting_frame_unit(const UnitState &unit) {
	sim::SimWorld w = _sim_world();
	_sync_targeting_frame_index(sim::targeting::unit_index_by_id(w, unit.instance_id), unit);
}

void TeamfightSimulationCore::_build_role_strategy_cache() {
	sim::targeting::build_role_strategy_cache(_role_strategy_cache_by_slot, _default_strategy);
}

const TeamfightSimulationCore::UnitStrategy &TeamfightSimulationCore::_strategy_for_unit(const UnitState &unit) const {
	return sim::targeting::strategy_for_unit(unit, _role_strategy_cache_by_slot, _default_strategy);
}

void TeamfightSimulationCore::_prepare_tick_context() {
	sim::SimWorld w = _sim_world();
	sim::targeting::prepare_tick_context(w, _sim_host_callbacks);
}

void TeamfightSimulationCore::_emit_trace(const StringName &kind, int64_t src_id, int64_t tgt_id, double val) {
	if (!_debug_combat_trace) {
		return;
	}
	if (_trace_buffer.size() >= TRACE_BUFFER_CAP) {
		return;
	}
	TraceEvent ev;
	ev.t = _time;
	ev.kind = kind;
	ev.src = src_id;
	ev.tgt = tgt_id;
	ev.val = val;
	_trace_buffer.push_back(ev);
}

bool TeamfightSimulationCore::is_ready() const {
	return true;
}

void TeamfightSimulationCore::clear() {
	_reset_runtime_state();
}

Vector2 TeamfightSimulationCore::_resolve_aoe_direction(const UnitState &source, const AoShapeParams &params, const UnitState *target_override) const {
	sim::SimWorld w = _sim_world();
	return sim::status::resolve_aoe_direction(w, source, params, target_override);
}

sim::targeting::CoordinatorTargetingState TeamfightSimulationCore::_targeting_coordinator_state(bool profile_score_enemy) {
	sim::targeting::CoordinatorTargetingState state;
	state.profile_targeting_active = _sim_profile_targeting_active;
	state.profile_score_enemy = profile_score_enemy;
	state.debug_targeting_scoring = _debug_targeting_scoring;
	state.debug_user_data = const_cast<TeamfightSimulationCore *>(this);
	state.debug_archetype_for_unit = &sim_host_archetype_for_unit;
	state.debug_print_line = &sim_host_print_line;
	state.debug_print_score_breakdown = &sim_host_print_score_breakdown;
	state.tgt_retarget_keeps = &_sim_profile_counters.tgt_retarget_keeps;
	state.tgt_enemy_scans = &_sim_profile_counters.tgt_enemy_scans;
	state.tgt_candidates_scored = &_sim_profile_counters.tgt_candidates_scored;
	state.tgt_ties_adjusted = &_sim_profile_counters.tgt_ties_adjusted;
	state.tgt_ties_distance = &_sim_profile_counters.tgt_ties_distance;
	state.tgt_ties_instance = &_sim_profile_counters.tgt_ties_instance;
	state.tgt_ally_scans = &_sim_profile_counters.tgt_ally_scans;
	state.profile_se_base = &_sim_profile_counters.se_base;
	state.profile_se_calls = &_sim_profile_counters.se_calls;
	return state;
}

UnitState *TeamfightSimulationCore::_select_enemy_target(UnitState &unit, bool profile_sim) {
	sim::SimWorld w = _sim_world();
	return sim::targeting::select_enemy_target_coordinator(
			w,
			unit,
			_strategy_for_unit(unit),
			&_sim_host_callbacks,
			_targeting_coordinator_state(profile_sim));
}

UnitState *TeamfightSimulationCore::_select_ally_target(UnitState &unit) {
	sim::SimWorld w = _sim_world();
	return sim::targeting::select_ally_target_coordinator(
			w, unit, _strategy_for_unit(unit), _targeting_coordinator_state(false));
}

bool TeamfightSimulationCore::_position_collides_with_unit(double x, double y, int64_t exclude_instance_id) const {
	return sim::movement::position_collides_with_unit(_sim_world(), x, y, exclude_instance_id);
}

Vector2 TeamfightSimulationCore::_get_random_spawn_position(const StringName &team, bool is_respawn) {
	double x_base = (team == StringName("player")) ? PLAYER_SPAWN_X_BASE : ENEMY_SPAWN_X_BASE;

	// Use same spawn points for both initial spawn and respawn
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

void TeamfightSimulationCore::_prune_assist_window(UnitState &unit) {
	// Python unit.py: cutoff = world.time - ASSIST_WINDOW; drop stale entries each tick.
	const double cutoff = _time - ASSIST_WINDOW;
	UnitStateCold &c = _uc(unit);
	std::vector<int64_t> remove_ids;
	remove_ids.reserve(c.damage_sources.size());
	for (const auto &entry : c.damage_sources) {
		if (entry.second.last_time <= cutoff) {
			remove_ids.push_back(entry.first);
		}
	}
	for (int64_t id : remove_ids) {
		c.damage_sources.erase(id);
	}
	remove_ids.clear();
	for (const auto &entry : c.recent_benefactors) {
		if (entry.second <= cutoff) {
			remove_ids.push_back(entry.first);
		}
	}
	for (int64_t id : remove_ids) {
		c.recent_benefactors.erase(id);
	}
}

void TeamfightSimulationCore::_update_unit(UnitState &unit, bool profile_sim) {
	sim::SimWorld w = _sim_world();

	sim::unit_tick::UnitTickProfileCounters tick_profile{};
	tick_profile.profile_sim = profile_sim;
	if (profile_sim) {
		tick_profile.uu_dead_respawn = &_sim_profile_counters.uu_dead_respawn;
		tick_profile.uu_cooldowns_cc = &_sim_profile_counters.uu_cooldowns_cc;
		tick_profile.uu_separation = &_sim_profile_counters.uu_separation;
		tick_profile.uu_threat_and_assist = &_sim_profile_counters.uu_threat_and_assist;
		tick_profile.uu_regen_on_tick = &_sim_profile_counters.uu_regen_on_tick;
		tick_profile.uu_casting = &_sim_profile_counters.uu_casting;
		tick_profile.uu_targeting = &_sim_profile_counters.uu_targeting;
		tick_profile.uu_combat = &_sim_profile_counters.uu_combat;
		tick_profile.uu_movement = &_sim_profile_counters.uu_movement;
		tick_profile.ucc_attack_cd = &_sim_profile_counters.ucc_attack_cd;
		tick_profile.ucc_ability_cd = &_sim_profile_counters.ucc_ability_cd;
		tick_profile.ucc_retarget = &_sim_profile_counters.ucc_retarget;
		tick_profile.ucc_target_switch = &_sim_profile_counters.ucc_target_switch;
		tick_profile.ucc_stun = &_sim_profile_counters.ucc_stun;
		tick_profile.ucc_slow = &_sim_profile_counters.ucc_slow;
		tick_profile.ucc_root = &_sim_profile_counters.ucc_root;
		tick_profile.ucc_silence = &_sim_profile_counters.ucc_silence;
		tick_profile.ucc_disarm = &_sim_profile_counters.ucc_disarm;
		tick_profile.ucc_stealth = &_sim_profile_counters.ucc_stealth;
		tick_profile.ucc_shield = &_sim_profile_counters.ucc_shield;
		tick_profile.ucc_reflect = &_sim_profile_counters.ucc_reflect;
		tick_profile.ucc_taunt = &_sim_profile_counters.ucc_taunt;
		tick_profile.ucc_forced_target = &_sim_profile_counters.ucc_forced_target;
		tick_profile.ur_effects = &_sim_profile_counters.ur_effects;
		tick_profile.ur_channel = &_sim_profile_counters.ur_channel;
		tick_profile.ur_periodic = &_sim_profile_counters.ur_periodic;
		tick_profile.uc_distance_calc = &_sim_profile_counters.uc_distance_calc;
		tick_profile.uc_ability = &_sim_profile_counters.uc_ability;
		tick_profile.uc_auto_attack = &_sim_profile_counters.uc_auto_attack;
		tick_profile.um_kiting = &_sim_profile_counters.um_kiting;
		tick_profile.um_kiting_spatial = &_sim_profile_counters.um_kiting_spatial;
		tick_profile.um_kiting_brute = &_sim_profile_counters.um_kiting_brute;
		tick_profile.um_toward = &_sim_profile_counters.um_toward;
	}

	sim::unit_tick::UnitTickMatchState match{};
	match.sudden_death_ticks = _sudden_death_ticks;
	match.player_kills = &_player_kills;
	match.enemy_kills = &_enemy_kills;

	sim::unit_tick::update_unit(
			w,
			unit,
			_sim_host_callbacks,
			_channel_host_hooks(),
			_combat_host_hooks(),
			_unit_tick_host_hooks(),
			match,
			tick_profile,
			_projectiles);
}

void TeamfightSimulationCore::_update_projectiles() {
	sim::SimWorld w = _sim_world();
	sim::combat::ProjectileBuffers buffers{ _projectiles, _active_projectiles, _scratch_projectiles };
	sim::combat::ProjectileMatchState match{ _sudden_death_ticks, _player_kills, _enemy_kills };
	sim::combat::update_projectiles(w, _sim_host_callbacks, _combat_host_hooks(), buffers, match);
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

void TeamfightSimulationCore::_print_score_breakdown(const ScoreBreakdown &breakdown, const StringName &attacker_archetype, const StringName &enemy_archetype) const {
	UtilityFunctions::print("[SCORE BREAKDOWN] " + String(attacker_archetype) + " -> " + String(enemy_archetype));
	UtilityFunctions::print("  Distance: " + String::num_real(breakdown.distance) + " (weighted: " + String::num_real(breakdown.distance_weighted) + ")");
	UtilityFunctions::print("  HP Ratio: " + String::num_real(breakdown.hp_ratio) + " (weighted: " + String::num_real(breakdown.hp_weighted) + ")");
	UtilityFunctions::print("  Role Priority: " + String::num_real(breakdown.role_priority));
	UtilityFunctions::print("  Threat: " + String::num_real(breakdown.threat) + " (weighted: " + String::num_real(breakdown.threat_weighted) + ")");
	UtilityFunctions::print("  In-Range Bonus: " + String::num_real(breakdown.in_range_bonus));
	UtilityFunctions::print("  Execute Bonus: " + String::num_real(breakdown.execute_bonus));
	UtilityFunctions::print("  Support Peel: " + String::num_real(breakdown.support_peel));
	UtilityFunctions::print("  TOTAL: " + String::num_real(breakdown.total));
}

Dictionary TeamfightSimulationCore::get_tick_snapshot() const {
	sim::viewer::TickSnapshotInput input;
	input.tick = _tick;
	input.time = _time;
	input.player_kills = _player_kills;
	input.enemy_kills = _enemy_kills;
	input.live_winner = sim::match::determine_winner(_player_kills, _enemy_kills);
	input.units = &_units;
	input.unit_cold = &_unit_cold;
	input.projectiles = &_projectiles;
	input.viewer_fx = &_viewer_fx;
	sim::SimWorld w = _sim_world();
	input.world = &w;
	return sim::viewer::build_tick_snapshot(input);
}

Array TeamfightSimulationCore::get_trace_events() const {
	Array out;
	out.resize(int64_t(_trace_buffer.size()));
	for (int64_t i = 0; i < int64_t(_trace_buffer.size()); ++i) {
		const TraceEvent &e = _trace_buffer[static_cast<size_t>(i)];
		Dictionary d;
		d["t"] = e.t;
		d["kind"] = e.kind;
		d["src"] = e.src;
		d["tgt"] = e.tgt;
		d["val"] = e.val;
		out[i] = d;
	}
	return out;
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

void TeamfightSimulationCore::set_balance_patches(const Array &patches) {
	// Ensure the champion catalog (champion data, role configs, passives) is loaded
	// before we replace the patch list, so _catalog.champion_catalog stays valid.
	_ensure_catalog_loaded();
	sim::catalog::set_balance_patches(_catalog, patches, _catalog_hooks());
}

Array TeamfightSimulationCore::get_balance_patches() const {
	return sim::catalog::get_balance_patches(_catalog);
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

// Stack debugging implementations
void TeamfightSimulationCore::_debug_print_stack_state(const UnitState &unit) const {
	if (!_debug_combat_trace) {
		return;
	}
	
	String debug_output = vformat("=== STACK STATE FOR UNIT %d ===\n", unit.instance_id);
	debug_output += vformat("Total stacks: %d\n", unit.stat_stacks.size());
	
	Array stack_keys = unit.stat_stacks.keys();
	for (int i = 0; i < stack_keys.size(); i++) {
		Variant key_var = stack_keys[i];
		if (key_var.get_type() != Variant::STRING) continue;
		
		String key = key_var;
		Dictionary stack_dict = unit.stat_stacks[key];
		
		debug_output += vformat("Stack[%s]: stacks=%d/%d, duration=%.2f, reason=%s\n",
			key,
			stack_dict.get("current_stacks", 0),
			stack_dict.get("max_stacks", 1),
			stack_dict.get("duration", 0.0),
			stack_dict.get("reason", "unknown")
		);
	}
	
	UtilityFunctions::push_warning(debug_output);
}



