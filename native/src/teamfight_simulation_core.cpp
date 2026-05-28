#include "teamfight_simulation_core.hpp"

#include "simulation/sim_aoe.hpp"
#include "simulation/sim_catalog.hpp"
#include "simulation/sim_channel.hpp"
#include "simulation/sim_combat.hpp"
#include "simulation/sim_effects_compile.hpp"
#include "simulation/sim_effects_exec.hpp"
#include "simulation/sim_damage.hpp"
#include "simulation/sim_match.hpp"
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

// Lazy StringName accessors (function-local static): safe under GDExtension; file-scope StringName can fail DLL load.
namespace {
inline const StringName &sn_player() {
	static const StringName s("player");
	return s;
}
inline const StringName &sn_enemy() {
	static const StringName s("enemy");
	return s;
}
inline const StringName &sn_ally() {
	static const StringName s("ally");
	return s;
}
inline const StringName &sn_tank() {
	static const StringName s("tank");
	return s;
}
inline const StringName &sn_fighter() {
	static const StringName s("fighter");
	return s;
}
inline const StringName &sn_marksman() {
	static const StringName s("marksman");
	return s;
}
inline const StringName &sn_mage() {
	static const StringName s("mage");
	return s;
}
inline const StringName &sn_support() {
	static const StringName s("support");
	return s;
}
inline const StringName &sn_assassin() {
	static const StringName s("assassin");
	return s;
}
inline const StringName &sn_auto() {
	static const StringName s("auto");
	return s;
}
inline const StringName &sn_multi_effect() {
	static const StringName s("multi_effect");
	return s;
}
inline const StringName &sn_damage() {
	static const StringName s("damage");
	return s;
}
inline const StringName &sn_projectile() {
	static const StringName s("projectile");
	return s;
}
inline const StringName &sn_stun() {
	static const StringName s("stun");
	return s;
}
inline const StringName &sn_shield() {
	static const StringName s("shield");
	return s;
}
inline const StringName &sn_heal() {
	static const StringName s("heal");
	return s;
}
inline const StringName &sn_aoe_taunt() {
	static const StringName s("aoe_taunt");
	return s;
}
inline const StringName &sn_aoe_damage() {
	static const StringName s("aoe_damage");
	return s;
}
inline const StringName &sn_damage_threshold_trigger() {
	static const StringName s("damage_threshold_trigger");
	return s;
}
inline const StringName &sn_damage_over_time() {
	static const StringName s("damage_over_time");
	return s;
}
inline const StringName &sn_heal_over_time() {
	static const StringName s("heal_over_time");
	return s;
}
inline const StringName &sn_aoe_damage_over_time() {
	static const StringName s("aoe_damage_over_time");
	return s;
}
inline const StringName &sn_aoe_heal_over_time() {
	static const StringName s("aoe_heal_over_time");
	return s;
}
inline const StringName &sn_mana_regen() {
	static const StringName s("mana_regen");
	return s;
}
inline const StringName &sn_post_damage_mana_gain() {
	static const StringName s("post_damage_mana_gain");
	return s;
}
inline const StringName &sn_damage_based_heal() {
	static const StringName s("damage_based_heal");
	return s;
}
inline const StringName &sn_damage_based_shield() {
	static const StringName s("damage_based_shield");
	return s;
}
inline const StringName &sn_consume_stacks_damage() {
	static const StringName s("consume_stacks_damage");
	return s;
}
inline const StringName &sn_consume_stacks_heal() {
	static const StringName s("consume_stacks_heal");
	return s;
}
inline const StringName &sn_consume_stacks_shield() {
	static const StringName s("consume_stacks_shield");
	return s;
}
inline const StringName &sn_set_stacks() {
	static const StringName s("set_stacks");
	return s;
}
inline const StringName &sn_channel() {
	static const StringName s("channel");
	return s;
}
inline const StringName &sn_mana_restore_on_hit() {
	static const StringName s("mana_restore_on_hit");
	return s;
}
inline const StringName &sn_drain_target_mana_on_hit() {
	static const StringName s("drain_target_mana_on_hit");
	return s;
}
inline const StringName &sn_every_n_attacks_stun() {
	static const StringName s("every_n_attacks_stun");
	return s;
}
inline const StringName &sn_self_dash() {
	static const StringName s("self_dash");
	return s;
}
inline const StringName &sn_auto_dodge() {
	static const StringName s("auto_dodge");
	return s;
}
inline const StringName &sn_constant_multiplier() {
	static const StringName s("constant_multiplier");
	return s;
}
inline const StringName &sn_hp_threshold_damage_multiplier() {
	static const StringName s("hp_threshold_damage_multiplier");
	return s;
}
inline const StringName &sn_distance_threshold_multiplier() {
	static const StringName s("distance_threshold_multiplier");
	return s;
}
inline const StringName &sn_slow() {
	static const StringName s("slow");
	return s;
}
inline const StringName &sn_root() {
	static const StringName s("root");
	return s;
}
inline const StringName &sn_silence() {
	static const StringName s("silence");
	return s;
}
inline const StringName &sn_disarm() {
	static const StringName s("disarm");
	return s;
}
inline const StringName &sn_stealth() {
	static const StringName s("stealth");
	return s;
}
inline const StringName &sn_knockback() {
	static const StringName s("knockback");
	return s;
}
inline const StringName &sn_reflect() {
	static const StringName s("reflect");
	return s;
}
inline const StringName &sn_aoe_slow() {
	static const StringName s("aoe_slow");
	return s;
}
inline const StringName &sn_aoe_root() {
	static const StringName s("aoe_root");
	return s;
}
inline const StringName &sn_aoe_silence() {
	static const StringName s("aoe_silence");
	return s;
}
inline const StringName &sn_aoe_disarm() {
	static const StringName s("aoe_disarm");
	return s;
}
inline const StringName &sn_aoe_knockback() {
	static const StringName s("aoe_knockback");
	return s;
}
inline const StringName &sn_aoe_reflect() {
	static const StringName s("aoe_reflect");
	return s;
}
inline const StringName &sn_aoe_stun() {
	static const StringName s("aoe_stun");
	return s;
}
inline const StringName &sn_reflect_damage() {
	static const StringName s("reflect_damage");
	return s;
}
inline const StringName &sn_redirect_damage() {
	static const StringName s("redirect_damage");
	return s;
}
inline const StringName &sn_summon_ally() {
	static const StringName s("summon_ally");
	return s;
}
inline const StringName &sn_knockback_shield() {
	static const StringName s("knockback_shield");
	return s;
}
inline const StringName &sn_target_status_multiplier() {
	static const StringName s("target_status_multiplier");
	return s;
}
inline const StringName &sn_stat_modifier() {
	static const StringName s("stat_modifier");
	return s;
}
inline const StringName &sn_multi_target() {
	static const StringName s("multi_target");
	return s;
}
inline const StringName &sn_ability() {
	static const StringName s("ability");
	return s;
}
inline const StringName &sn_ultimate() {
	static const StringName s("ultimate");
	return s;
}
inline const StringName &sn_source() {
	static const StringName s("source");
	return s;
}
inline const StringName &sn_target() {
	static const StringName s("target");
	return s;
}
inline const StringName &sn_physical() {
	static const StringName s("physical");
	return s;
}
inline const StringName &sn_magic() {
	static const StringName s("magic");
	return s;
}
inline const StringName &sn_true() {
	static const StringName s("true");
	return s;
}
inline const StringName &sn_passive() {
	static const StringName s("passive");
	return s;
}
inline const StringName &sn_on_attack() {
	static const StringName s("on_attack");
	return s;
}
inline const StringName &sn_on_defense() {
	static const StringName s("on_defense");
	return s;
}
inline const StringName &sn_on_ally_defense() {
	static const StringName s("on_ally_defense");
	return s;
}
inline const StringName &sn_on_tick() {
	static const StringName s("on_tick");
	return s;
}
inline const StringName &sn_post_attack() {
	static const StringName s("post_attack");
	return s;
}
inline const StringName &sn_post_take_damage() {
	static const StringName s("post_take_damage");
	return s;
}
inline const StringName &sn_on_ability() {
	static const StringName s("on_ability");
	return s;
}
inline const StringName &sn_on_ultimate() {
	static const StringName s("on_ultimate");
	return s;
}
inline const StringName &sn_post_heal() {
	static const StringName s("post_heal");
	return s;
}
inline const StringName &sn_on_takedown() {
	static const StringName s("on_takedown");
	return s;
}
inline const StringName &sn_cast_start() {
	static const StringName s("cast_start");
	return s;
}
inline const StringName &sn_success() {
	static const StringName s("success");
	return s;
}
inline const StringName &sn_condition_failed() {
	static const StringName s("condition_failed");
	return s;
}
inline const StringName &sn_damage_dealt() {
	static const StringName s("damage_dealt");
	return s;
}
inline const StringName &sn_target_killed() {
	static const StringName s("target_killed");
	return s;
}
inline const StringName &sn_projectile_created() {
	static const StringName s("projectile_created");
	return s;
}
inline const StringName &sn_stun_applied() {
	static const StringName s("stun_applied");
	return s;
}
inline const StringName &sn_shield_applied() {
	static const StringName s("shield_applied");
	return s;
}
inline const StringName &sn_heal_applied() {
	static const StringName s("heal_applied");
	return s;
}
inline const StringName &sn_taunt_applied() {
	static const StringName s("taunt_applied");
	return s;
}
inline const StringName &sn_splash_applied() {
	static const StringName s("splash_applied");
	return s;
}
inline const StringName &sn_splash_triggered() {
	static const StringName s("splash_triggered");
	return s;
}
inline const StringName &sn_knockback_applied() {
	static const StringName s("knockback_applied");
	return s;
}
inline const StringName &sn_reached_target() {
	static const StringName s("reached_target");
	return s;
}
inline const StringName &sn_amount() {
	static const StringName s("amount");
	return s;
}
inline const StringName &sn_duration() {
	static const StringName s("duration");
	return s;
}
inline const StringName &sn_radius() {
	static const StringName s("radius");
	return s;
}
inline const StringName &sn_mana_restored() {
	static const StringName s("mana_restored");
	return s;
}
inline const StringName &sn_mana_drained() {
	static const StringName s("mana_drained");
	return s;
}
inline const StringName &sn_distance_traveled() {
	static const StringName s("distance_traveled");
	return s;
}
constexpr size_t EFFECT_BUCKET_ON_ATTACK = 0;
constexpr size_t EFFECT_BUCKET_ON_DEFENSE = 1;
constexpr size_t EFFECT_BUCKET_ON_ALLY_DEFENSE = 2;
constexpr size_t EFFECT_BUCKET_ON_TICK = 3;
constexpr size_t EFFECT_BUCKET_POST_ATTACK = 4;
constexpr size_t EFFECT_BUCKET_POST_TAKE_DAMAGE = 5;
constexpr size_t EFFECT_BUCKET_ON_ABILITY = 6;
constexpr size_t EFFECT_BUCKET_ON_ULTIMATE = 7;
constexpr size_t EFFECT_BUCKET_POST_HEAL = 8;
constexpr size_t EFFECT_BUCKET_ON_TAKEDOWN = 9;
} // namespace

namespace {
} // namespace

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

static double strategy_role_prio(const std::array<double, TeamfightSimulationCore::ROLE_SLOT_COUNT> &slots, int64_t role_slot) {
	if (role_slot < 0 || role_slot >= TeamfightSimulationCore::ROLE_SLOT_COUNT) {
		return 0.0;
	}
	return slots[static_cast<size_t>(role_slot)];
}

static Dictionary effect_dict(const StringName &kind, const Dictionary &params = Dictionary()) {
	Dictionary effect;
	effect["kind"] = String(kind);
	effect["params"] = params;
	return effect;
}


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
			entry.action_kind = sn_passive();  // Passive effects are always granted by passives
			cold.passive_reflect_entries.push_back(entry);
		}
	}
}

Dictionary sim_host_execute_effect(void *user_data, const EffectRecord &effect, EffectContext &context) {
	auto *core = static_cast<TeamfightSimulationCore *>(user_data);
	return core->_execute_effect(effect, context);
}

void sim_host_handle_death(void *user_data, UnitState &killer, UnitState &target) {
	static_cast<TeamfightSimulationCore *>(user_data)->_handle_death(killer, target);
}

void sim_host_sync_targeting_frame_unit(void *user_data, const UnitState &unit) {
	static_cast<TeamfightSimulationCore *>(user_data)->_sync_targeting_frame_unit(unit);
}

void sim_host_sync_targeting_frame_index(void *user_data, int64_t index, const UnitState &unit) {
	static_cast<TeamfightSimulationCore *>(user_data)->_sync_targeting_frame_index(index, unit);
}

void sim_host_emit_trace(void *user_data, const StringName &kind, int64_t src_id, int64_t tgt_id, double val) {
	static_cast<TeamfightSimulationCore *>(user_data)->_emit_trace(kind, src_id, tgt_id, val);
}

void sim_host_viewer_record_damage_fx(
		void *user_data,
		const UnitState &source,
		const UnitState &target,
		double total_damage,
		const StringName &action_kind,
		const StringName &damage_type) {
	static_cast<TeamfightSimulationCore *>(user_data)->_viewer_record_damage_fx(source, target, total_damage, action_kind, damage_type);
}

void sim_host_viewer_record_hot_status_fx(void *user_data, const UnitState &target, double duration, const StringName &effect_type) {
	static_cast<TeamfightSimulationCore *>(user_data)->_viewer_record_hot_status_fx(target, duration, effect_type);
}

double sim_host_randf(void *user_data) {
	return static_cast<TeamfightSimulationCore *>(user_data)->_randf();
}

StringName sim_host_archetype_for_unit(void *user_data, const UnitState &unit) {
	return static_cast<TeamfightSimulationCore *>(user_data)->_uc(unit).archetype_id;
}

void sim_host_print_line(void *user_data, const String &line) {
	(void)user_data;
	UtilityFunctions::print(line);
}

void sim_host_print_score_breakdown(
		void *user_data,
		const ScoreBreakdown &breakdown,
		const StringName &attacker_archetype,
		const StringName &enemy_archetype) {
	static_cast<TeamfightSimulationCore *>(user_data)->_print_score_breakdown(breakdown, attacker_archetype, enemy_archetype);
}

double sim_host_heal_unit(
		void *user_data,
		UnitState &source,
		UnitState &target,
		double amount,
		const StringName &action_kind,
		bool allow_overheal) {
	static_cast<TeamfightSimulationCore *>(user_data)->_heal_unit(source, target, amount, action_kind, allow_overheal);
	return target.hp;
}

UnitState *sim_host_select_ally_target(void *user_data, UnitState &unit) {
	return static_cast<TeamfightSimulationCore *>(user_data)->_select_ally_target(unit);
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

void TeamfightSimulationCore::_bind_sim_host() {
	_sim_host_callbacks.user_data = this;
	_sim_host_callbacks.execute_effect = &sim_host_execute_effect;
	_sim_host_callbacks.handle_death = &sim_host_handle_death;
	_sim_host_callbacks.sync_targeting_frame_unit = &sim_host_sync_targeting_frame_unit;
	_sim_host_callbacks.sync_targeting_frame_index = &sim_host_sync_targeting_frame_index;
	_sim_host_callbacks.emit_trace = &sim_host_emit_trace;
	_sim_host_callbacks.viewer_record_damage_fx = &sim_host_viewer_record_damage_fx;
	_sim_host_callbacks.viewer_record_hot_status_fx = &sim_host_viewer_record_hot_status_fx;
	_sim_host_callbacks.randf = &sim_host_randf;
	_bind_sim_exec_hooks();
}

sim::combat::CombatHostHooks TeamfightSimulationCore::_combat_host_hooks() const {
	sim::combat::CombatHostHooks hooks;
	hooks.user_data = const_cast<TeamfightSimulationCore *>(this);
	hooks.heal_unit = &sim_host_heal_unit;
	hooks.select_ally_target = &sim_host_select_ally_target;
	return hooks;
}

void sim_host_unit_tick_respawn(void *user_data, UnitState &unit) {
	static_cast<TeamfightSimulationCore *>(user_data)->_respawn_unit(unit);
}

const UnitStrategy &sim_host_unit_tick_strategy(void *user_data, const UnitState &unit) {
	return static_cast<TeamfightSimulationCore *>(user_data)->_strategy_for_unit(unit);
}

UnitState *sim_host_unit_tick_select_enemy_target(void *user_data, UnitState &unit, bool profile_sim) {
	return static_cast<TeamfightSimulationCore *>(user_data)->_select_enemy_target(unit, profile_sim);
}

UnitState *sim_host_unit_tick_select_ally_target(void *user_data, UnitState &unit) {
	return static_cast<TeamfightSimulationCore *>(user_data)->_select_ally_target(unit);
}

void sim_host_unit_tick_prune_assist_window(void *user_data, UnitState &unit) {
	static_cast<TeamfightSimulationCore *>(user_data)->_prune_assist_window(unit);
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

// --- sim::effects::exec host hooks ---
void sim_host_push_projectile(void *user_data, const ProjectileState &projectile) {
	static_cast<TeamfightSimulationCore *>(user_data)->_projectiles.push_back(projectile);
}

std::vector<UnitState *> sim_host_select_targets(void *user_data, UnitState &source, UnitState *target, int64_t target_count, TargetSelectionStrategy strategy, bool include_source, ExcessTargetHandling excess_handling, const StringName &team_filter) {
	return static_cast<TeamfightSimulationCore *>(user_data)->_select_targets(source, target, target_count, strategy, include_source, excess_handling, team_filter);
}

Vector2 sim_host_find_random_spawn_position_near_excluding_with_expansion(void *user_data, double center_x, double center_y, double initial_radius, double max_radius, int64_t exclude_instance_id, const std::vector<Vector2> &pending_positions) {
	return static_cast<TeamfightSimulationCore *>(user_data)->_find_random_spawn_position_near_excluding_with_expansion(center_x, center_y, initial_radius, max_radius, exclude_instance_id, pending_positions);
}

Dictionary sim_host_get_minion_data(void *user_data, const StringName &minion_id) {
	return static_cast<TeamfightSimulationCore *>(user_data)->_catalog.minion_catalog.get(String(minion_id), Dictionary());
}

bool sim_host_debug_combat_trace(void *user_data) {
	return static_cast<TeamfightSimulationCore *>(user_data)->_debug_combat_trace;
}

sim::channel::ChannelHostHooks TeamfightSimulationCore::_channel_host_hooks() const {
	sim::channel::ChannelHostHooks hooks;
	hooks.user_data = const_cast<TeamfightSimulationCore *>(this);
	hooks.debug_combat_trace = &sim_host_debug_combat_trace;
	return hooks;
}

sim::effects::SimMatchHost TeamfightSimulationCore::_sim_match_host() const {
	sim::effects::SimMatchHost match_host;
	match_host.user_data = const_cast<TeamfightSimulationCore *>(this);
	match_host.pending_spawns = const_cast<std::vector<PendingSpawn> *>(&_pending_spawns);
	match_host.max_instance_id = const_cast<int64_t *>(&_max_instance_id);
	match_host.get_minion_data = &sim_host_get_minion_data;
	match_host.find_random_spawn_position_near_excluding_with_expansion = &sim_host_find_random_spawn_position_near_excluding_with_expansion;
	return match_host;
}

void TeamfightSimulationCore::_bind_sim_exec_hooks() {
	_sim_exec_callbacks.user_data = this;
	_sim_exec_callbacks.push_projectile = &sim_host_push_projectile;
	_sim_exec_callbacks.select_targets = &sim_host_select_targets;
	_sim_exec_callbacks.debug_combat_trace = &sim_host_debug_combat_trace;
}

TeamfightSimulationCore::TeamfightSimulationCore() {
	_bind_sim_host();

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
	_viewer_fx_events.clear();
	_pending_spawns.clear();
}

Dictionary TeamfightSimulationCore::_load_json_file(const String &path) const {
	Dictionary empty;
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::ModeFlags::READ);
	if (file.is_null()) {
		UtilityFunctions::push_error(vformat("Failed to open JSON file: %s", path));
		return empty;
	}
	Variant parsed = JSON::parse_string(file->get_as_text());
	if (parsed.get_type() != Variant::DICTIONARY) {
		UtilityFunctions::push_error(vformat("Failed to parse JSON file: %s", path));
		return empty;
	}
	return Dictionary(parsed);
}

Dictionary TeamfightSimulationCore::_load_json_file_if_exists(const String &path) const {
	Dictionary empty;
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::ModeFlags::READ);
	if (file.is_null()) {
		return empty;
	}
	Variant parsed = JSON::parse_string(file->get_as_text());
	if (parsed.get_type() != Variant::DICTIONARY) {
		UtilityFunctions::push_error(vformat("Failed to parse JSON file: %s", path));
		return empty;
	}
	return Dictionary(parsed);
}

Dictionary TeamfightSimulationCore::_effect_to_dict(const Variant &effect) const {
	if (effect.get_type() == Variant::DICTIONARY) {
		return Dictionary(effect);
	}
	if (effect.get_type() == Variant::OBJECT) {
		Object *object = effect;
		if (object != nullptr && object->has_method("to_dict")) {
			Variant converted = object->call("to_dict");
			if (converted.get_type() == Variant::DICTIONARY) {
				return Dictionary(converted);
			}
		}
	}
	return Dictionary();
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

Dictionary TeamfightSimulationCore::_coerce_match_input(const Variant &match_input) const {
	if (match_input.get_type() == Variant::DICTIONARY) {
		return Dictionary(match_input);
	}
	if (match_input.get_type() == Variant::OBJECT) {
		Object *object = match_input;
		if (object != nullptr && object->has_method("to_dict")) {
			Variant converted = object->call("to_dict");
			if (converted.get_type() == Variant::DICTIONARY) {
				return Dictionary(converted);
			}
		}
	}
	return Dictionary();
}

Dictionary TeamfightSimulationCore::_champion_for(const StringName &archetype_id) const {
	return sim::catalog::champion_for(_catalog, archetype_id);
}

void TeamfightSimulationCore::_append_team_units(const Array &spawn_specs, const StringName &team, int64_t &next_instance_id, Array &team_comp) {
	for (int64_t index = 0; index < spawn_specs.size(); ++index) {
		Dictionary spawn_spec = _coerce_match_input(spawn_specs[index]);
		std::pair<UnitState, UnitStateCold> built = _build_unit_state(spawn_spec, team, next_instance_id);
		if (built.first.instance_id == 0) {
			continue;
		}
		int64_t unit_index = int64_t(_units.size());
		_units.push_back(std::move(built.first));
		_unit_cold.push_back(std::move(built.second));
		_unit_index_map[next_instance_id] = unit_index;
		_add_alive_index(team, unit_index);
		_targeting_frame.push_back(_make_targeting_frame_entry(_units[static_cast<size_t>(unit_index)]));
		team_comp.append(_unit_cold[static_cast<size_t>(unit_index)].archetype_id);

		// Update max instance ID for efficient minion spawning
		if (next_instance_id > _max_instance_id) {
			_max_instance_id = next_instance_id;
		}

		// Emit passive AOE events for visualization at initial spawn
		UnitState &unit = _units[static_cast<size_t>(unit_index)];
		UnitStateCold &cold = _unit_cold[static_cast<size_t>(unit_index)];
		for (const UnitStateCold::PassiveAoeInfo &aoe_info : cold.passive_aoe_info) {
			_viewer_record_passive_aoe_fx(unit, aoe_info.radius, aoe_info.passive_id);
		}

		++next_instance_id;
	}
}

TeamfightSimulationCore::TargetingFrameEntry TeamfightSimulationCore::_make_targeting_frame_entry(const UnitState &unit) const {
	return sim::targeting::make_targeting_frame_entry(unit);
}

void TeamfightSimulationCore::_sync_targeting_frame_index(int64_t index, const UnitState &unit) {
	if (_sim_profile_targeting_active) {
		_sim_profile_tgt_frame_syncs += 1;
	}
	sim::SimWorld w = _sim_world();
	sim::targeting::sync_targeting_frame_index(w, index, unit);
}

void TeamfightSimulationCore::_sync_targeting_frame_unit(const UnitState &unit) {
	_sync_targeting_frame_index(_unit_index_by_id(unit.instance_id), unit);
}

std::pair<UnitState, UnitStateCold> TeamfightSimulationCore::_build_unit_state(const Dictionary &spawn_spec, const StringName &team, int64_t instance_id) {
	UnitState unit;
	UnitStateCold cold;
	StringName archetype_id = StringName(String(spawn_spec.get("archetype_id", "")));
	
	// Check if this is a minion (in minion_catalog) or a champion (in champion catalog)
	Dictionary minion_data = Dictionary(_catalog.minion_catalog.get(String(archetype_id), Dictionary()));
	Dictionary champion;
	
	if (!minion_data.is_empty()) {
		// This is a minion, use minion data directly
		champion = minion_data;
	} else {
		// This is a champion, use effective champion cache
		champion = _effective_champion_for(archetype_id);
		if (champion.is_empty()) {
			UtilityFunctions::push_error(vformat("Unknown champion archetype: %s", String(archetype_id)));
			return { unit, cold };
		}
	}

	// Stats, kits, and balance overlays are resolved in _rebuild_effective_champion_cache().
	Dictionary stats = Dictionary(champion["stats"]).duplicate(true);
	StringName role_name = StringName(stats.get("role", StringName()));
	Dictionary role_config = Dictionary(_catalog.role_configs.get(role_name, Dictionary()));

	// Minions skip passive/ability/ultimate compilation
	bool is_minion = (role_name == StringName("minion"));
	
	if (!is_minion) {
		auto passive_bucket_index = [](const StringName &kind) -> int {
			if (kind == sn_on_attack()) {
				return 0;
			}
			if (kind == sn_on_defense()) {
				return 1;
			}
			if (kind == sn_on_ally_defense()) {
				return 2;
			}
			if (kind == sn_on_tick()) {
				return 3;
			}
			if (kind == sn_post_attack()) {
				return 4;
			}
			if (kind == sn_post_take_damage()) {
				return 5;
			}
			if (kind == sn_on_ability()) {
				return 6;
			}
			if (kind == sn_on_ultimate()) {
				return 7;
			}
			if (kind == sn_post_heal()) {
				return 8;
			}
			if (kind == sn_on_takedown()) {
				return 9;
			}
			// DEBUG: Log unrecognized trigger kinds
			UtilityFunctions::push_error(vformat("[DEBUG] passive_bucket_index: unrecognized trigger kind '%s', falling through to bucket 5 (post_take_damage)", String(kind)));
			return 5;
		};

		Array passive_ids = Array(champion.get("passive_ids", Array()));
		for (int64_t index = 0; index < passive_ids.size(); ++index) {
			StringName passive_id = StringName(String(passive_ids[index]));
			Dictionary entry = Dictionary(_catalog.passive_registry.get(passive_id, Dictionary()));
			if (entry.is_empty()) {
				continue;
			}
			Array effect_kinds;
			effect_kinds.append(StringName("on_attack"));
			effect_kinds.append(StringName("on_defense"));
			effect_kinds.append(StringName("on_ally_defense"));
			effect_kinds.append(StringName("on_tick"));
			effect_kinds.append(StringName("post_attack"));
			effect_kinds.append(StringName("post_take_damage"));
			effect_kinds.append(StringName("on_ability"));
			effect_kinds.append(StringName("on_ultimate"));
			effect_kinds.append(StringName("post_heal"));
			effect_kinds.append(StringName("on_takedown"));
			if (entry.has("on_ally_defense")) {
				// Extract and store radius from passive definition
				double radius = double(entry.get("radius", 0.0));
				// Take the maximum radius if multiple passives have on_ally_defense
				if (radius > cold.on_ally_defense_radius) {
					cold.on_ally_defense_radius = radius;
				}
			}
			// Store passive AOE radius information for visualization (all passives with radius)
			if (entry.has("radius")) {
				double radius = double(entry.get("radius", 0.0));
				if (radius > 0.0) {
					UnitStateCold::PassiveAoeInfo aoe_info;
					aoe_info.passive_id = passive_id;
					aoe_info.radius = radius;
					cold.passive_aoe_info.push_back(aoe_info);
				}
			}
			for (int64_t kind_index = 0; kind_index < effect_kinds.size(); ++kind_index) {
				Variant kind_value = effect_kinds[kind_index];
				Array effects = Array(entry.get(kind_value, Array()));
				std::vector<EffectRecord> compiled_effects = _compile_effect_array(effects);
				std::vector<EffectRecord> &bucket = cold.passive_effects[passive_bucket_index(StringName(String(kind_value)))];
				bucket.insert(bucket.end(), compiled_effects.begin(), compiled_effects.end());
			}
		}
		Variant role_tick = role_config.get("passive_on_tick", Variant());
		if (role_tick.get_type() != Variant::NIL) {
			cold.passive_effects[3].push_back(_compile_effect(Dictionary(role_tick)));
		}
		Variant role_take_damage = role_config.get("passive_post_take_damage", Variant());
		if (role_take_damage.get_type() != Variant::NIL) {
			cold.passive_effects[5].push_back(_compile_effect(Dictionary(role_take_damage)));
		}
		cold.on_tick_effect_accumulators.resize(cold.passive_effects[EFFECT_BUCKET_ON_TICK].size(), 0.0);
	}

	double max_hp = double(stats.get("max_hp", 0.0));
	double max_mana = double(stats.get("max_mana", 0.0));
	double x, y;

	// Minions skip spawn slot assignment and use provided coordinates
	if (!is_minion) {
		// Generate random spawn position with slot assignment
		int64_t spawn_slot = _assign_spawn_slot(team);
		Vector2 spawn_pos = _get_random_spawn_position(team, false);

		// Override Y coordinate with assigned slot position
		static const std::vector<double> spawn_points = {3.0, 3.5, 4.0, 4.5, 5.0, 5.5, 6.0, 6.5, 7.0};
		if (spawn_slot >= 0 && spawn_slot < int(spawn_points.size())) {
			double x_base = (team == sn_player()) ? PLAYER_SPAWN_X_BASE : ENEMY_SPAWN_X_BASE;
			
			// Adjust melee champions to spawn 0.5 tiles closer to center
			double attack_range = double(stats.get("attack_range", 0.0));
			if (attack_range <= 1.0) {  // Melee threshold
				if (team == sn_player()) {
					x_base += 0.5;
				} else {
					x_base -= 0.5;
				}
			}
			
			x = x_base;
			y = spawn_points[spawn_slot];
			cold.respawn_slot_index = spawn_slot;
		} else {
			x = spawn_pos.x;
			y = spawn_pos.y;
		}
	} else {
		// Minions use x/y from spawn_spec (provided by summoner)
		x = double(spawn_spec.get("x", 0.0));
		y = double(spawn_spec.get("y", 0.0));
		cold.respawn_slot_index = -1;
	}

	unit.instance_id = instance_id;
	cold.archetype_id = archetype_id;
	unit.team = team;
	cold.role_id = role_name;
	
	// Assign pseudo-role based on attack range for minions
	StringName effective_role_name = role_name;
	if (is_minion) {
		// Melee minions (attack_range <= 0.5) get fighter pseudo-role
		// Ranged minions (attack_range > 0.5) get marksman pseudo-role
		double attack_range = double(stats.get("attack_range", 0.0));
		effective_role_name = (attack_range <= 0.5) ? sn_fighter() : sn_marksman();
	}

	// Assign role slot and flags using effective role
	unit.role_slot = sim::targeting::role_slot_for_name(effective_role_name);
	unit.is_tank_role = effective_role_name == sn_tank();
	unit.is_fighter_role = effective_role_name == sn_fighter();
	unit.is_assassin_role = effective_role_name == sn_assassin();
	unit.is_marksman_role = effective_role_name == sn_marksman();
	unit.is_mage_role = effective_role_name == sn_mage();
	unit.is_support_role = effective_role_name == sn_support();
	cold.stats = stats;
	// Initialize combat stats from dictionary - generated by X-Macro
#define X(name, def, min_val, max_val) \
	unit.combat.name = double(stats.get(#name, def));
	STAT_LIST
#undef X
	
	// Minions don't have abilities or ultimates
	if (!is_minion) {
		Variant ability_effect = champion.get("ability", Variant());
		Variant ultimate_effect = champion.get("ultimate", Variant());
		unit.has_ability_effect = ability_effect.get_type() == Variant::DICTIONARY;
		unit.has_ultimate_effect = ultimate_effect.get_type() == Variant::DICTIONARY;
		unit.ability_requires_target_in_range = bool(Dictionary(ability_effect).get("requires_target_in_range", true));
		unit.ultimate_requires_target_in_range = bool(Dictionary(ultimate_effect).get("requires_target_in_range", true));
		if (unit.has_ability_effect) {
			cold.ability_effect = _compile_effect(Dictionary(ability_effect));
		}
		if (unit.has_ultimate_effect) {
			cold.ultimate_effect = _compile_effect(Dictionary(ultimate_effect));
		}
	} else {
		unit.has_ability_effect = false;
		unit.has_ultimate_effect = false;
		unit.ability_requires_target_in_range = false;
		unit.ultimate_requires_target_in_range = false;
	}
	cold.spawn_pos_x = x;
	cold.spawn_pos_y = y;
	unit.pos_x = x;
	unit.pos_y = y;
	cold.respawn_slot_index = -1; // Initialize with no assigned respawn slot
	unit.hp = max_hp;
	unit.shield = 0.0;
	unit.mana = 0.0;
	unit.attack_cooldown = 0.0;
	unit.attack_period = 0.0;
	unit.ability_cooldown = get_effective_ability_cd(unit);
	unit.casting_remaining = 0.0;
	cold.casting_kind = StringName();
	cold.casting_effect = EffectRecord();
	unit.casting_target_id = 0;
	unit.casting_ally_target_id = 0;
	unit.cast_resolved_this_tick = false;
	unit.target_id = 0;
	unit.target_index = -1;
	unit.current_ally_target_id = 0;
	unit.retarget_timer = 0.0;
	unit.target_switch_lock_timer = 0.0;
	unit.last_kite_timer = 0.0;
	unit.current_target_score = 0.0;
	unit.stun_remaining = 0.0;
	unit.hard_cc_seconds = 0.0;
	unit.slow_remaining = 0.0;
	unit.slow_move_mult = 1.0;
	unit.root_remaining = 0.0;
	unit.silence_remaining = 0.0;
	unit.silence_ability_remaining = 0.0;
	unit.silence_ultimate_remaining = 0.0;
	unit.silence_blocks_abilities = false;
	unit.silence_blocks_ultimates = false;
	unit.disarm_remaining = 0.0;
	unit.stealth_remaining = 0.0;
	unit.stealth_break_on_attack = false;
	unit.stealth_break_on_ability = false;
	unit.stealth_break_on_damage_taken = false;
	unit.respawn_timer = 0.0;
	unit.respawned_this_tick = false;
	unit.alive = true;
	unit.incoming_target_count = 0;
	unit.perceived_threat = 0.0;
	unit.attack_count = 0;
	
	// Initialize stat modifier fields - generated by X-Macro
#define X(name, def, min_val, max_val) \
	unit.stat_additive_##name = 0.0; \
	unit.stat_multiplicative_##name = 1.0; \
	unit.stat_temp_##name = 0.0; \
	unit.stat_perm_##name = 0.0;
	STAT_LIST
#undef X
	cold.damage_dealt = 0.0;
	cold.damage_dealt_auto = 0.0;
	cold.damage_dealt_ability = 0.0;
	cold.damage_dealt_ultimate = 0.0;
	cold.damage_dealt_passive = 0.0;
	cold.damage_received = 0.0;
	cold.damage_mitigated = 0.0;
	cold.healing_done = 0.0;
	cold.healing_done_auto = 0.0;
	cold.healing_done_ability = 0.0;
	cold.healing_done_ultimate = 0.0;
	cold.healing_done_passive = 0.0;
	cold.shielding_done = 0.0;
	cold.shielding_done_auto = 0.0;
	cold.shielding_done_ability = 0.0;
	cold.shielding_done_ultimate = 0.0;
	cold.shielding_done_passive = 0.0;
	cold.auto_attacks = 0;
	cold.abilities = 0;
	cold.ultimates = 0;
	cold.stuns = 0;
	cold.kills = 0;
	cold.deaths = 0;
	cold.assists = 0;
	unit.taunt_target_id = 0;
	unit.taunt_remaining = 0.0;
	unit.forced_target_id = 0;
	unit.forced_target_remaining = 0.0;
	cold.forced_target_kind = StringName();
	cold.damage_sources.clear();
	cold.recent_benefactors.clear();
	cold.last_hit_time = 0.0;
	std::fill(cold.on_tick_effect_accumulators.begin(), cold.on_tick_effect_accumulators.end(), 0.0);
	unit.regen_accumulator = 0.0;
	_finalize_reflect_passives(unit, cold);
	return { unit, std::move(cold) };
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

void TeamfightSimulationCore::_spatial_ensure_stamp_size() const {
	sim::SimWorld w = _sim_world();
	sim::ensure_stamp_size(w);
}

void TeamfightSimulationCore::_spatial_clear_buckets() const {
	sim::SimWorld w = _sim_world();
	sim::clear_buckets(w);
}

double TeamfightSimulationCore::_spatial_cell_size() const {
	return sim::cell_size();
}

int TeamfightSimulationCore::_spatial_flat_index(double x, double y) const {
	return sim::flat_index(x, y);
}

void TeamfightSimulationCore::_spatial_add_alive_team(const StringName &team) const {
	sim::SimWorld w = _sim_world();
	sim::add_alive_team(w, team);
}

void TeamfightSimulationCore::_spatial_next_generation() const {
	sim::SimWorld w = _sim_world();
	sim::next_generation(w);
}

bool TeamfightSimulationCore::_spatial_stamp_has(int64_t unit_index) const {
	sim::SimWorld w = _sim_world();
	return sim::stamp_has(w, unit_index);
}

void TeamfightSimulationCore::_spatial_stamp_circle(double cx, double cy, double radius, const StringName &team) const {
	sim::SimWorld w = _sim_world();
	sim::stamp_circle(w, cx, cy, radius, team);
}

void TeamfightSimulationCore::_spatial_stamp_separation_candidates(double cx, double cy, double radius, const StringName &team, int64_t self_instance_id) const {
	sim::SimWorld w = _sim_world();
	sim::stamp_separation_candidates(w, cx, cy, radius, team, self_instance_id);
}

void TeamfightSimulationCore::_spatial_stamp_kite_threat(double cx, double cy, double danger_radius) const {
	sim::SimWorld w = _sim_world();
	sim::stamp_kite_threat(w, cx, cy, danger_radius);
}

void TeamfightSimulationCore::_spatial_fill_buckets_for_indices(const std::vector<int64_t> &indices) const {
	sim::SimWorld w = _sim_world();
	sim::fill_buckets_for_indices(w, indices);
}

int TeamfightSimulationCore::_spatial_count_neighbors_in_grid(int64_t self_index, double cx, double cy, double radius) const {
	sim::SimWorld w = _sim_world();
	return sim::count_neighbors_in_grid(w, self_index, cx, cy, radius);
}

bool TeamfightSimulationCore::_use_spatial_broad_phase() const {
	sim::SimWorld w = _sim_world();
	return sim::use_spatial_broad_phase(w);
}

double TeamfightSimulationCore::_score_ally_target(const UnitState &unit, const TargetingFrameEntry &ally, const UnitStrategy &strategy, double unit_ally_distance) const {
	return sim::targeting::score_ally_target(unit, ally, strategy, unit_ally_distance);
}

double TeamfightSimulationCore::_score_enemy_target(const UnitState &attacker, const TargetingFrameEntry &enemy, const TargetingFrameEntry *ally_for_peel, const UnitStrategy &strategy, const TickContext &ctx, const TargetScoreContext &score_ctx, double attacker_enemy_distance, bool profile_score, int64_t enemy_index, ScoreBreakdown *breakdown) {
	sim::SimWorld w = _sim_world();
	sim::targeting::ScoreEnemyProfileCounters score_profile;
	score_profile.active = profile_score;
	if (profile_score) {
		score_profile.se_base = &_sim_profile_se_base;
		score_profile.se_calls = &_sim_profile_se_calls;
	}
	return sim::targeting::score_enemy_target(
			w,
			attacker,
			enemy,
			ally_for_peel,
			strategy,
			ctx,
			score_ctx,
			attacker_enemy_distance,
			profile_score ? &score_profile : nullptr,
			enemy_index,
			breakdown);
}

bool TeamfightSimulationCore::_should_switch(const UnitState &unit, double current_score, double new_score, const TeamfightSimulationCore::UnitStrategy &strategy, double current_target_distance) const {
	sim::SimWorld w = _sim_world();
	return sim::targeting::should_switch(w, unit, current_score, new_score, strategy, current_target_distance);
}

void TeamfightSimulationCore::_set_current_target(UnitState &unit, const UnitState &target) {
	sim::SimWorld w = _sim_world();
	sim::targeting::set_current_target(w, unit, target, &_sim_host_callbacks);
}

std::vector<int64_t> &TeamfightSimulationCore::_alive_indices_for_team(const StringName &team) {
	if (team == sn_player()) {
		return _alive_player_indices;
	}
	return _alive_enemy_indices;
}

const std::vector<int64_t> &TeamfightSimulationCore::_alive_indices_for_team(const StringName &team) const {
	if (team == sn_player()) {
		return _alive_player_indices;
	}
	return _alive_enemy_indices;
}

void TeamfightSimulationCore::_add_alive_index(const StringName &team, int64_t index) {
	std::vector<int64_t> &alive_indices = _alive_indices_for_team(team);
	std::unordered_set<int64_t> &alive_indices_set = team == sn_player() ? _alive_player_indices_set : _alive_enemy_indices_set;
	
	// O(1) duplicate check using set
	if (alive_indices_set.count(index)) {
		return;
	}
	
	alive_indices.push_back(index);
	alive_indices_set.insert(index);
}

void TeamfightSimulationCore::_remove_alive_index(const StringName &team, int64_t index) {
	std::vector<int64_t> &alive_indices = _alive_indices_for_team(team);
	std::unordered_set<int64_t> &alive_indices_set = team == sn_player() ? _alive_player_indices_set : _alive_enemy_indices_set;
	
	// Remove from set (O(1))
	alive_indices_set.erase(index);
	
	// Swap-and-pop from vector (O(1))
	for (size_t i = 0; i < alive_indices.size(); ++i) {
		if (alive_indices[i] == index) {
			// Swap with last element and pop
			if (i != alive_indices.size() - 1) {
				alive_indices[i] = alive_indices.back();
			}
			alive_indices.pop_back();
			break;
		}
	}
}

void TeamfightSimulationCore::_adjust_target_pressure(int64_t old_target_id, int64_t new_target_id) {
	sim::SimWorld w = _sim_world();
	sim::targeting::adjust_target_pressure(w, old_target_id, new_target_id, &_sim_host_callbacks);
}

bool TeamfightSimulationCore::is_ready() const {
	return true;
}

void TeamfightSimulationCore::clear() {
	_reset_runtime_state();
}

void TeamfightSimulationCore::_populate_runtime_state(const Dictionary &match_input) {
	_seed = int64_t(match_input.get("seed", 0));
	_tick_rate = double(match_input.get("tick_rate", DEFAULT_TICK_RATE));
	_record_events = bool(match_input.get("record_events", false));
	_debug_combat_trace = bool(match_input.get("debug_combat_trace", false));
	_debug_targeting_scoring = bool(match_input.get("debug_targeting_scoring", false));
	_trace_buffer.clear();
	if (_debug_combat_trace) {
		_trace_buffer.reserve(TRACE_BUFFER_CAP);
	}
	_rng.seed_int64(_seed);

	int64_t next_instance_id = 1;
	_append_team_units(Array(match_input.get("player_units", Array())), StringName("player"), next_instance_id, _player_comp);
	_append_team_units(Array(match_input.get("enemy_units", Array())), StringName("enemy"), next_instance_id, _enemy_comp);
	_build_role_strategy_cache();
	_prepare_tick_context();
}

TeamfightSimulationCore::EffectContext TeamfightSimulationCore::_build_context(UnitState &source, UnitState *target, UnitState *target_ally, double damage, const StringName &action_kind) {
	return sim::combat::build_context(source, target, target_ally, damage, action_kind);
}

int TeamfightSimulationCore::_effect_bucket_index(const StringName &kind) const {
	return sim::combat::effect_bucket_index(kind);
}

const std::vector<EffectRecord> &TeamfightSimulationCore::_collect_effects(const UnitState &unit, const StringName &kind) {
	sim::SimWorld w = _sim_world();
	return sim::combat::collect_effects(w, unit, kind);
}

bool TeamfightSimulationCore::_target_has_status(const UnitState &target, const StringName &status_kind) const {
	sim::SimWorld w = _sim_world();
	return sim::status::target_has_status(w, target, status_kind);
}

double TeamfightSimulationCore::_evaluate_multiplier_effect(const EffectRecord &effect, const EffectContext &context, double current_value) {
	sim::SimWorld w = _sim_world();
	return sim::damage::evaluate_multiplier_effect(w, _sim_host_callbacks, effect, context, current_value);
}

double TeamfightSimulationCore::_defense_multiplier(UnitState &target, UnitState &source, double damage, const StringName &action_kind) {
	sim::SimWorld w = _sim_world();
	return sim::damage::defense_multiplier(w, _sim_host_callbacks, target, source, damage, action_kind);
}

double TeamfightSimulationCore::_auto_dodge_multiplier(UnitState &target, UnitState &source, double damage) {
	sim::SimWorld w = _sim_world();
	return sim::damage::auto_dodge_multiplier(w, _sim_host_callbacks, target, source, damage);
}

double TeamfightSimulationCore::_apply_attack_modifiers(UnitState &unit, UnitState &target, double distance, double damage) {
	sim::SimWorld w = _sim_world();
	return sim::damage::apply_attack_modifiers(w, _sim_host_callbacks, unit, target, distance, damage);
}

double TeamfightSimulationCore::_apply_ability_modifiers(UnitState &unit, UnitState *target, double damage) {
	sim::SimWorld w = _sim_world();
	return sim::damage::apply_ability_modifiers(w, _sim_host_callbacks, unit, target, damage);
}

double TeamfightSimulationCore::_apply_ultimate_modifiers(UnitState &unit, UnitState *target, double damage) {
	sim::SimWorld w = _sim_world();
	return sim::damage::apply_ultimate_modifiers(w, _sim_host_callbacks, unit, target, damage);
}

double TeamfightSimulationCore::_apply_damage(UnitState &source, UnitState &target, double damage, const StringName &damage_type, const StringName &action_kind, const EffectContext &context) {
	sim::SimWorld w = _sim_world();
	return sim::damage::apply_damage(w, _sim_host_callbacks, source, target, damage, damage_type, action_kind, context);
}

void TeamfightSimulationCore::_maybe_apply_reflect_damage(UnitState &attacker, UnitState &defender, double total_damage_applied, const StringName &damage_type, const EffectContext &context) {
	sim::SimWorld w = _sim_world();
	sim::damage::maybe_apply_reflect_damage(w, _sim_host_callbacks, attacker, defender, total_damage_applied, damage_type, context);
}

void TeamfightSimulationCore::_touch_damage_source(UnitState &target, int64_t source_id, double incoming_damage) {
	sim::SimWorld w = _sim_world();
	sim::damage::touch_damage_source(w, target, source_id, incoming_damage);
}

double TeamfightSimulationCore::_trigger_ally_defense_effects(UnitState &target, UnitState &source, double damage, const StringName &damage_type, const StringName &action_kind, const EffectContext &context) {
	sim::SimWorld w = _sim_world();
	return sim::damage::trigger_ally_defense_effects(w, _sim_host_callbacks, target, source, damage, damage_type, action_kind, context);
}

void TeamfightSimulationCore::_apply_stun(UnitState &source, UnitState &target, double duration) {
	sim::SimWorld w = _sim_world();
	sim::status::apply_stun(w, source, target, duration);
}

void TeamfightSimulationCore::_apply_slow(UnitState &source, UnitState &target, double slow_percentage, double duration) {
	sim::SimWorld w = _sim_world();
	sim::status::apply_slow(w, source, target, slow_percentage, duration);
}

void TeamfightSimulationCore::_apply_root(UnitState &source, UnitState &target, double duration) {
	sim::SimWorld w = _sim_world();
	sim::status::apply_root(w, source, target, duration);
}

void TeamfightSimulationCore::_apply_silence(UnitState &source, UnitState &target, double duration, bool block_abilities, bool block_ultimate) {
	sim::SimWorld w = _sim_world();
	sim::status::apply_silence(w, source, target, duration, block_abilities, block_ultimate);
}

void TeamfightSimulationCore::_apply_disarm(UnitState &source, UnitState &target, double duration) {
	sim::SimWorld w = _sim_world();
	sim::status::apply_disarm(w, source, target, duration);
}

void TeamfightSimulationCore::_apply_stealth(UnitState &source, UnitState &target, double duration, bool break_on_attack, bool break_on_ability, bool break_on_damage_taken) {
	sim::SimWorld w = _sim_world();
	sim::status::apply_stealth(w, source, target, duration, break_on_attack, break_on_ability, break_on_damage_taken);
}

void TeamfightSimulationCore::_apply_aoe_slow(UnitState &source, double radius, double slow_percentage, double duration) {
	EffectRecord effect;
	effect.aoe_shape_params.shape = AoShapeKind::Circle;
	effect.aoe_shape_params.anchor = AoAnchorKind::Self;
	effect.aoe_shape_params.radius = radius;
	_apply_aoe_slow_shape(source, nullptr, effect, slow_percentage, duration);
}

void TeamfightSimulationCore::_apply_aoe_root(UnitState &source, double radius, double duration) {
	EffectRecord effect;
	effect.aoe_shape_params.shape = AoShapeKind::Circle;
	effect.aoe_shape_params.anchor = AoAnchorKind::Self;
	effect.aoe_shape_params.radius = radius;
	_apply_aoe_root_shape(source, nullptr, effect, duration);
}

void TeamfightSimulationCore::_apply_aoe_silence(UnitState &source, double radius, double duration, bool block_abilities, bool block_ultimate) {
	EffectRecord effect;
	effect.aoe_shape_params.shape = AoShapeKind::Circle;
	effect.aoe_shape_params.anchor = AoAnchorKind::Self;
	effect.aoe_shape_params.radius = radius;
	_apply_aoe_silence_shape(source, nullptr, effect, duration, block_abilities, block_ultimate);
}

void TeamfightSimulationCore::_apply_aoe_disarm(UnitState &source, double radius, double duration) {
	EffectRecord effect;
	effect.aoe_shape_params.shape = AoShapeKind::Circle;
	effect.aoe_shape_params.anchor = AoAnchorKind::Self;
	effect.aoe_shape_params.radius = radius;
	_apply_aoe_disarm_shape(source, nullptr, effect, duration);
}

void TeamfightSimulationCore::_apply_aoe_stun(UnitState &source, double radius, double duration) {
	EffectRecord effect;
	effect.aoe_shape_params.shape = AoShapeKind::Circle;
	effect.aoe_shape_params.anchor = AoAnchorKind::Self;
	effect.aoe_shape_params.radius = radius;
	_apply_aoe_stun_shape(source, nullptr, effect, duration);
}

void TeamfightSimulationCore::_apply_reflect_buff(UnitState &source, UnitState &target, double pct, double duration, const StringName &action_kind, const StringName &damage_type, const String &reason) {
	sim::SimWorld w = _sim_world();
	sim::periodic::apply_reflect_buff(w, source, target, pct, duration, action_kind, damage_type, reason);
}

void TeamfightSimulationCore::_apply_aoe_reflect(UnitState &source, double radius, double pct, double duration, bool all_damage_types) {
	sim::SimWorld w = _sim_world();
	sim::periodic::apply_aoe_reflect(w, _sim_host_callbacks, source, radius, pct, duration, all_damage_types);
}

bool TeamfightSimulationCore::_apply_knockback(UnitState &source, UnitState &target, double distance, bool away_from_source) {
	sim::SimWorld w = _sim_world();
	return sim::periodic::apply_knockback(w, _sim_host_callbacks, source, target, distance, away_from_source);
}

bool TeamfightSimulationCore::_apply_aoe_knockback(UnitState &source, double radius, double distance, bool away_from_source) {
	sim::SimWorld w = _sim_world();
	return sim::periodic::apply_aoe_knockback(w, _sim_host_callbacks, source, radius, distance, away_from_source);
}

double TeamfightSimulationCore::_movement_speed_multiplier(const UnitState &unit) const {
	return sim::movement_speed_multiplier(unit);
}

void TeamfightSimulationCore::_add_shield(UnitState &source, UnitState &target, double amount, const StringName &action_kind) {
	sim::SimWorld w = _sim_world();
	sim::status::add_shield(w, source, target, amount, action_kind);
	if (amount > 1e-9) {
		_viewer_record_shield_fx(target, amount);
	}
}

void TeamfightSimulationCore::_heal_unit(UnitState &source, UnitState &target, double amount, const StringName &action_kind, bool allow_overheal) {
	sim::SimWorld w = _sim_world();
	double gained = sim::status::heal_unit(w, source, target, amount, action_kind, allow_overheal);
	if (gained > 1e-9) {
		_viewer_record_heal_fx(target, gained);
		_sync_targeting_frame_unit(target);
	}
}

void TeamfightSimulationCore::_restore_mana(UnitState &source, UnitState &target, double amount) {
	sim::SimWorld w = _sim_world();
	sim::status::restore_mana(w, source, target, amount);
}

void TeamfightSimulationCore::_run_post_attack_effects(UnitState &source, UnitState &target, double damage, const EffectContext &context) {
	sim::SimWorld w = _sim_world();
	sim::combat::run_post_attack_effects(w, _sim_host_callbacks, source, target, damage, context);
}

void TeamfightSimulationCore::_run_post_heal_effects(UnitState &source, UnitState &target, double heal_amount, double heal_gained, const EffectContext &base_context) {
	sim::SimWorld w = _sim_world();
	sim::combat::run_post_heal_effects(w, _sim_host_callbacks, source, target, heal_amount, heal_gained, base_context);
}

void TeamfightSimulationCore::_run_on_takedown_effects(UnitState &participant, UnitState &victim, double damage_dealt, bool is_kill, const EffectContext &base_context) {
	sim::SimWorld w = _sim_world();
	sim::combat::run_on_takedown_effects(w, _sim_host_callbacks, participant, victim, damage_dealt, is_kill, base_context);
}

void TeamfightSimulationCore::_apply_dot(UnitState &source, UnitState &target, double attack_damage_ratio, double max_hp_ratio, double flat_amount, double duration, double tick_interval, const StringName &damage_type, const StringName &stacking_mode, int max_stacks, const StringName &effect_type, const String &reason, const StringName &action_kind, bool is_dynamic) {
	sim::SimWorld w = _sim_world();
	sim::periodic::apply_dot(w, _sim_host_callbacks, source, target, attack_damage_ratio, max_hp_ratio, flat_amount, duration, tick_interval, damage_type, stacking_mode, max_stacks, effect_type, reason, action_kind, is_dynamic);
}

void TeamfightSimulationCore::_apply_hot(UnitState &source, UnitState &target, double max_hp_ratio, double current_hp_ratio, double missing_hp_ratio, double flat_amount, double duration, double tick_interval, const StringName &stacking_mode, int max_stacks, bool allow_overheal, const StringName &effect_type, const String &reason, const StringName &action_kind, bool is_dynamic) {
	sim::SimWorld w = _sim_world();
	sim::periodic::apply_hot(w, _sim_host_callbacks, source, target, max_hp_ratio, current_hp_ratio, missing_hp_ratio, flat_amount, duration, tick_interval, stacking_mode, max_stacks, allow_overheal, effect_type, reason, action_kind, is_dynamic);
}

void TeamfightSimulationCore::_tick_periodic_effects(UnitState &unit, double delta) {
	sim::SimWorld w = _sim_world();
	sim::periodic::tick_periodic_effects(w, _sim_host_callbacks, unit, delta);
}

void TeamfightSimulationCore::_cleanse_dots(UnitState &unit, const StringName &effect_type_filter) {
	sim::SimWorld w = _sim_world();
	sim::periodic::cleanse_dots(w, unit, effect_type_filter);
}

void TeamfightSimulationCore::_clear_periodic_effects(UnitState &unit) {
	sim::SimWorld w = _sim_world();
	sim::periodic::clear_periodic_effects(w, unit);
}

void TeamfightSimulationCore::_apply_aoe_dot(UnitState &source, double radius, double attack_damage_ratio, double max_hp_ratio, double flat_amount, double duration, double tick_interval, const StringName &damage_type, const StringName &stacking_mode, int max_stacks, const StringName &effect_type, const String &reason, bool target_self, const StringName &action_kind, bool is_dynamic) {
	sim::SimWorld w = _sim_world();
	sim::periodic::apply_aoe_dot(w, _sim_host_callbacks, source, radius, attack_damage_ratio, max_hp_ratio, flat_amount, duration, tick_interval, damage_type, stacking_mode, max_stacks, effect_type, reason, target_self, action_kind, is_dynamic);
}

void TeamfightSimulationCore::_apply_aoe_hot(UnitState &source, double radius, double max_hp_ratio, double current_hp_ratio, double missing_hp_ratio, double flat_amount, double duration, double tick_interval, const StringName &stacking_mode, int max_stacks, bool allow_overheal, const StringName &effect_type, const String &reason, bool target_self, const StringName &action_kind, bool is_dynamic) {
	sim::SimWorld w = _sim_world();
	sim::periodic::apply_aoe_hot(w, _sim_host_callbacks, source, radius, max_hp_ratio, current_hp_ratio, missing_hp_ratio, flat_amount, duration, tick_interval, stacking_mode, max_stacks, allow_overheal, effect_type, reason, target_self, action_kind, is_dynamic);
}

void TeamfightSimulationCore::_apply_aoe_taunt(UnitState &source, double radius, double duration) {
	sim::SimWorld w = _sim_world();
	sim::periodic::apply_aoe_taunt(w, source, radius, duration);
}

double TeamfightSimulationCore::_apply_aoe_damage(UnitState &source, UnitState &center_source, double damage, double radius, const StringName &damage_type, const StringName &reason, const StringName &action_kind) {
	sim::SimWorld w = _sim_world();
	return sim::periodic::apply_aoe_damage(w, _sim_host_callbacks, source, center_source, damage, radius, damage_type, reason, action_kind);
}

Vector2 TeamfightSimulationCore::_resolve_aoe_direction(const UnitState &source, const AoShapeParams &params, const UnitState *target_override) const {
	sim::SimWorld w = _sim_world();
	return sim::status::resolve_aoe_direction(w, source, params, target_override);
}

void TeamfightSimulationCore::_apply_aoe_taunt_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double duration) {
	sim::SimWorld w = _sim_world();
	UnitState *shape_target = target;
	if (shape_target == nullptr && effect.aoe_shape_params.target_id != 0) {
		shape_target = sim::targeting::unit_by_id(w, effect.aoe_shape_params.target_id);
	}
	_viewer_record_aoe_shape_fx(source, shape_target, effect.aoe_shape_params, StringName("aoe_taunt"));
	sim::periodic::apply_aoe_taunt_shape(w, source, target, effect, duration);
}

void TeamfightSimulationCore::_apply_aoe_dot_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double attack_damage_ratio, double max_hp_ratio, double flat_amount, double duration, double tick_interval, const StringName &damage_type, const StringName &stacking_mode, int max_stacks, const StringName &effect_type, const String &reason, bool target_self, const StringName &action_kind, bool is_dynamic) {
	sim::SimWorld w = _sim_world();
	UnitState *shape_target = target;
	if (shape_target == nullptr && effect.aoe_shape_params.target_id != 0) {
		shape_target = sim::targeting::unit_by_id(w, effect.aoe_shape_params.target_id);
	}
	_viewer_record_aoe_shape_fx(source, shape_target, effect.aoe_shape_params, StringName("aoe_dot"));
	sim::periodic::apply_aoe_dot_shape(w, _sim_host_callbacks, source, target, effect, attack_damage_ratio, max_hp_ratio, flat_amount, duration, tick_interval, damage_type, stacking_mode, max_stacks, effect_type, reason, target_self, action_kind, is_dynamic);
}

void TeamfightSimulationCore::_apply_aoe_hot_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double max_hp_ratio, double current_hp_ratio, double missing_hp_ratio, double flat_amount, double duration, double tick_interval, const StringName &stacking_mode, int max_stacks, bool allow_overheal, const StringName &effect_type, const String &reason, bool target_self, const StringName &action_kind, bool is_dynamic) {
	sim::SimWorld w = _sim_world();
	UnitState *shape_target = target;
	if (shape_target == nullptr && effect.aoe_shape_params.target_id != 0) {
		shape_target = sim::targeting::unit_by_id(w, effect.aoe_shape_params.target_id);
	}
	_viewer_record_aoe_shape_fx(source, shape_target, effect.aoe_shape_params, StringName("aoe_hot"));
	sim::periodic::apply_aoe_hot_shape(w, _sim_host_callbacks, source, target, effect, max_hp_ratio, current_hp_ratio, missing_hp_ratio, flat_amount, duration, tick_interval, stacking_mode, max_stacks, allow_overheal, effect_type, reason, target_self, action_kind, is_dynamic);
}

double TeamfightSimulationCore::_apply_aoe_damage_shape_per_target(UnitState &source, UnitState *target, const EffectRecord &effect, double damage_ratio, double flat_amount, double max_hp_ratio, double splash_ratio, const StringName &damage_type, const StringName &action_kind) {
	sim::SimWorld w = _sim_world();
	UnitState *shape_target = target;
	if (shape_target == nullptr && effect.aoe_shape_params.target_id != 0) {
		shape_target = sim::targeting::unit_by_id(w, effect.aoe_shape_params.target_id);
	}
	_viewer_record_aoe_shape_fx(source, shape_target, effect.aoe_shape_params, StringName("aoe_damage"));
	return sim::periodic::apply_aoe_damage_shape_per_target(w, _sim_host_callbacks, source, target, effect, damage_ratio, flat_amount, max_hp_ratio, splash_ratio, damage_type, action_kind);
}

double TeamfightSimulationCore::_apply_aoe_damage_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double damage, const StringName &damage_type, const StringName &action_kind) {
	sim::SimWorld w = _sim_world();
	UnitState *shape_target = target;
	if (shape_target == nullptr && effect.aoe_shape_params.target_id != 0) {
		shape_target = sim::targeting::unit_by_id(w, effect.aoe_shape_params.target_id);
	}
	_viewer_record_aoe_shape_fx(source, shape_target, effect.aoe_shape_params, StringName("aoe_damage"));
	return sim::periodic::apply_aoe_damage_shape(w, _sim_host_callbacks, source, target, effect, damage, damage_type, action_kind);
}


void TeamfightSimulationCore::_apply_aoe_slow_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double slow_percentage, double duration) {
	sim::SimWorld w = _sim_world();
	UnitState *shape_target = target;
	if (shape_target == nullptr && effect.aoe_shape_params.target_id != 0) {
		shape_target = sim::targeting::unit_by_id(w, effect.aoe_shape_params.target_id);
	}
	_viewer_record_aoe_shape_fx(source, shape_target, effect.aoe_shape_params, StringName("aoe_slow"));
	sim::status::apply_aoe_slow_shape(w, source, target, effect, slow_percentage, duration);
}

void TeamfightSimulationCore::_apply_aoe_stun_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double duration) {
	sim::SimWorld w = _sim_world();
	UnitState *shape_target = target;
	if (shape_target == nullptr && effect.aoe_shape_params.target_id != 0) {
		shape_target = sim::targeting::unit_by_id(w, effect.aoe_shape_params.target_id);
	}
	_viewer_record_aoe_shape_fx(source, shape_target, effect.aoe_shape_params, StringName("aoe_stun"));
	sim::status::apply_aoe_stun_shape(w, source, target, effect, duration);
}

void TeamfightSimulationCore::_apply_aoe_root_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double duration) {
	sim::SimWorld w = _sim_world();
	UnitState *shape_target = target;
	if (shape_target == nullptr && effect.aoe_shape_params.target_id != 0) {
		shape_target = sim::targeting::unit_by_id(w, effect.aoe_shape_params.target_id);
	}
	_viewer_record_aoe_shape_fx(source, shape_target, effect.aoe_shape_params, StringName("aoe_root"));
	sim::status::apply_aoe_root_shape(w, source, target, effect, duration);
}

void TeamfightSimulationCore::_apply_aoe_silence_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double duration, bool block_abilities, bool block_ultimate) {
	sim::SimWorld w = _sim_world();
	UnitState *shape_target = target;
	if (shape_target == nullptr && effect.aoe_shape_params.target_id != 0) {
		shape_target = sim::targeting::unit_by_id(w, effect.aoe_shape_params.target_id);
	}
	_viewer_record_aoe_shape_fx(source, shape_target, effect.aoe_shape_params, StringName("aoe_silence"));
	sim::status::apply_aoe_silence_shape(w, source, target, effect, duration, block_abilities, block_ultimate);
}

void TeamfightSimulationCore::_apply_aoe_disarm_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double duration) {
	sim::SimWorld w = _sim_world();
	UnitState *shape_target = target;
	if (shape_target == nullptr && effect.aoe_shape_params.target_id != 0) {
		shape_target = sim::targeting::unit_by_id(w, effect.aoe_shape_params.target_id);
	}
	_viewer_record_aoe_shape_fx(source, shape_target, effect.aoe_shape_params, StringName("aoe_disarm"));
	sim::status::apply_aoe_disarm_shape(w, source, target, effect, duration);
}




bool TeamfightSimulationCore::_apply_aoe_knockback_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double distance, bool away_from_source) {
	sim::SimWorld w = _sim_world();
	UnitState *shape_target = target;
	if (shape_target == nullptr && effect.aoe_shape_params.target_id != 0) {
		shape_target = sim::targeting::unit_by_id(w, effect.aoe_shape_params.target_id);
	}
	_viewer_record_aoe_shape_fx(source, shape_target, effect.aoe_shape_params, StringName("aoe_knockback"));
	return sim::periodic::apply_aoe_knockback_shape(w, _sim_host_callbacks, source, target, effect, distance, away_from_source);
}

void TeamfightSimulationCore::_apply_aoe_reflect_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double pct, double duration, bool all_damage_types, const StringName &action_kind, const String &reason) {
	sim::SimWorld w = _sim_world();
	UnitState *shape_target = target;
	if (shape_target == nullptr && effect.aoe_shape_params.target_id != 0) {
		shape_target = sim::targeting::unit_by_id(w, effect.aoe_shape_params.target_id);
	}
	_viewer_record_aoe_shape_fx(source, shape_target, effect.aoe_shape_params, StringName("aoe_reflect"));
	sim::periodic::apply_aoe_reflect_shape(w, _sim_host_callbacks, source, target, effect, pct, duration, all_damage_types, action_kind, reason);
}

std::vector<UnitState*> TeamfightSimulationCore::_select_targets(UnitState &source, UnitState *target, int64_t target_count, TargetSelectionStrategy strategy, bool include_source, ExcessTargetHandling excess_handling, const StringName &team_filter) {
	std::vector<UnitState*> selected;
	
	// Get the team to filter by
	StringName target_team = team_filter;
	if (target_team.is_empty()) {
		// Default to enemies if not specified
		target_team = (source.team == sn_player()) ? sn_enemy() : sn_player();
	} else if (target_team == sn_enemy()) {
		// "enemy" means the opposite team from the source (semantically: the enemy of the caster)
		target_team = (source.team == sn_player()) ? sn_enemy() : sn_player();
	} else if (target_team == sn_ally()) {
		// "ally" means the same team as the source (semantically: the allies of the caster)
		target_team = source.team;
	} else {
		UtilityFunctions::push_error(vformat("Invalid team_filter '%s', must be 'ally' or 'enemy'", String(target_team)));
		return selected;
	}
	
	// Get alive indices for the target team
	const std::vector<int64_t> &alive_indices = _alive_indices_for_team(target_team);
	
	if (alive_indices.empty()) {
		return selected;
	}
	
	// Collect potential targets
	std::vector<UnitState*> candidates;
	for (int64_t idx : alive_indices) {
		if (idx < 0 || idx >= int64_t(_units.size())) {
			continue;
		}
		UnitState *unit = &_units[static_cast<size_t>(idx)];
		if (unit == nullptr || !unit->alive) {
			continue;
		}
		
		// Exclude source if not including source
		if (!include_source && unit->instance_id == source.instance_id) {
			continue;
		}
		
		candidates.push_back(unit);
	}
	
	// If no candidates, return empty
	if (candidates.empty()) {
		return selected;
	}
	
	// Sort/select based on strategy
	switch (strategy) {
		case TARGET_SELECTION_CLOSEST: {
			// Sort by distance to source
			std::sort(candidates.begin(), candidates.end(), [&](UnitState *a, UnitState *b) {
				double dist_a = _distance_between(source, *a);
				double dist_b = _distance_between(source, *b);
				return dist_a < dist_b;
			});
			break;
		}
		case TARGET_SELECTION_RANDOM: {
			// Simple random shuffle using _rng.random_random()
			for (size_t i = candidates.size(); i > 1; --i) {
				double rand_val = _rng.random_random();
				size_t j = static_cast<size_t>(rand_val * i);
				std::swap(candidates[i - 1], candidates[j]);
			}
			break;
		}
		case TARGET_SELECTION_LOWEST_HP: {
			// Sort by HP ascending
			std::sort(candidates.begin(), candidates.end(), [&](UnitState *a, UnitState *b) {
				return a->hp < b->hp;
			});
			break;
		}
		case TARGET_SELECTION_HIGHEST_HP: {
			// Sort by HP descending
			std::sort(candidates.begin(), candidates.end(), [&](UnitState *a, UnitState *b) {
				return a->hp > b->hp;
			});
			break;
		}
		case TARGET_SELECTION_LOWEST_PERCENT_HP: {
			// Sort by HP percentage ascending
			std::sort(candidates.begin(), candidates.end(), [&](UnitState *a, UnitState *b) {
				double pct_a = (get_effective_max_hp(*a) > EPSILON) ? (a->hp / get_effective_max_hp(*a)) : 0.0;
				double pct_b = (get_effective_max_hp(*b) > EPSILON) ? (b->hp / get_effective_max_hp(*b)) : 0.0;
				return pct_a < pct_b;
			});
			break;
		}
		case TARGET_SELECTION_HIGHEST_PERCENT_HP: {
			// Sort by HP percentage descending
			std::sort(candidates.begin(), candidates.end(), [&](UnitState *a, UnitState *b) {
				double pct_a = (get_effective_max_hp(*a) > EPSILON) ? (a->hp / get_effective_max_hp(*a)) : 0.0;
				double pct_b = (get_effective_max_hp(*b) > EPSILON) ? (b->hp / get_effective_max_hp(*b)) : 0.0;
				return pct_a > pct_b;
			});
			break;
		}
		case TARGET_SELECTION_CLOSEST_TO_TARGET: {
			// Sort by distance to target
			if (target != nullptr) {
				std::sort(candidates.begin(), candidates.end(), [&](UnitState *a, UnitState *b) {
					double dist_a = _distance_between(*target, *a);
					double dist_b = _distance_between(*target, *b);
					return dist_a < dist_b;
				});
			} else {
				// Fall back to closest to source if no target
				std::sort(candidates.begin(), candidates.end(), [&](UnitState *a, UnitState *b) {
					double dist_a = _distance_between(source, *a);
					double dist_b = _distance_between(source, *b);
					return dist_a < dist_b;
				});
			}
			break;
		}
	}
	
	// Determine how many targets to select
	int64_t num_to_select = target_count;
	if (target_count == -1) {
		// Select all candidates
		num_to_select = candidates.size();
	} else if (target_count > int64_t(candidates.size())) {
		// Handle excess targets
		if (excess_handling == EXCESS_TARGET_DROP) {
			num_to_select = candidates.size();
		} else {
			// STACK: cycle through candidates to reach target_count, then effect_count applies to each position in cycled list
			// Example: 2 enemies, request 3 targets with STACK → [A, B, A], effect_count=2 → A gets 4 instances, B gets 2
			for (int64_t i = 0; i < target_count; ++i) {
				selected.push_back(candidates[i % candidates.size()]);
			}
			return selected;
		}
	}
	
	// Select the first N candidates
	for (int64_t i = 0; i < num_to_select && i < int64_t(candidates.size()); ++i) {
		selected.push_back(candidates[i]);
	}
	
	return selected;
}

UnitState *TeamfightSimulationCore::_select_enemy_target(UnitState &unit, bool profile_sim) {
	sim::SimWorld w = _sim_world();
	const UnitStrategy &strategy = _strategy_for_unit(unit);
	TargetScoreContext score_ctx;
	score_ctx.attack_range = sim::targeting::attack_range(unit);
	score_ctx.effective_range = sim::targeting::effective_attack_range(unit);
	score_ctx.use_spatial = sim::use_spatial_broad_phase(w);
	if (strategy.prefers_kiting) {
		score_ctx.has_kite_bounds = true;
		score_ctx.kite_min_w = score_ctx.effective_range * KITE_TARGET_WINDOW_MIN_FACTOR;
		score_ctx.kite_max_w = score_ctx.effective_range * KITE_TARGET_WINDOW_MAX_FACTOR;
	} else {
		score_ctx.has_kite_bounds = false;
	}

	sim::targeting::TargetingProfileCounters profile;
	profile.active = _sim_profile_targeting_active;
	if (profile.active) {
		profile.retarget_keeps = &_sim_profile_tgt_retarget_keeps;
		profile.enemy_scans = &_sim_profile_tgt_enemy_scans;
		profile.candidates_scored = &_sim_profile_tgt_candidates_scored;
		profile.ties_adjusted = &_sim_profile_tgt_ties_adjusted;
		profile.ties_distance = &_sim_profile_tgt_ties_distance;
		profile.ties_instance = &_sim_profile_tgt_ties_instance;
	}

	sim::targeting::ScoreEnemyProfileCounters score_profile;
	score_profile.active = profile_sim;
	if (profile_sim) {
		score_profile.se_base = &_sim_profile_se_base;
		score_profile.se_calls = &_sim_profile_se_calls;
	}

	sim::targeting::TargetingDebugHooks debug;
	debug.enabled = _debug_targeting_scoring;
	if (debug.enabled) {
		debug.user_data = this;
		debug.archetype_for_unit = &sim_host_archetype_for_unit;
		debug.print_line = &sim_host_print_line;
		debug.print_score_breakdown = &sim_host_print_score_breakdown;
	}

	return sim::targeting::select_enemy_target(
			w,
			unit,
			strategy,
			score_ctx,
			&_sim_host_callbacks,
			profile.active ? &profile : nullptr,
			score_profile.active ? &score_profile : nullptr,
			debug.enabled ? &debug : nullptr);
}

UnitState *TeamfightSimulationCore::_select_ally_target(UnitState &unit) {
	sim::SimWorld w = _sim_world();
	const UnitStrategy &strategy = _strategy_for_unit(unit);
	sim::targeting::TargetingProfileCounters profile;
	profile.active = _sim_profile_targeting_active;
	if (profile.active) {
		profile.ally_scans = &_sim_profile_tgt_ally_scans;
	}
	return sim::targeting::select_ally_target(w, unit, strategy, profile.active ? &profile : nullptr);
}

double TeamfightSimulationCore::_distance_between(const UnitState &left, const UnitState &right) const {
	return sim::distance_between_coords(left.pos_x, left.pos_y, right.pos_x, right.pos_y);
}

bool TeamfightSimulationCore::_position_collides_with_unit(double x, double y, int64_t exclude_instance_id) const {
	return sim::movement::position_collides_with_unit(_sim_world(), x, y, exclude_instance_id);
}

Vector2 TeamfightSimulationCore::_find_valid_dash_position(double start_x, double start_y, double target_x, double target_y, double max_distance, int64_t exclude_instance_id) const {
	return sim::movement::find_valid_dash_position(_sim_world(), start_x, start_y, target_x, target_y, max_distance, exclude_instance_id);
}

Vector2 TeamfightSimulationCore::_get_random_spawn_position(const StringName &team, bool is_respawn) {
	double x_base = (team == StringName("player")) ? PLAYER_SPAWN_X_BASE : ENEMY_SPAWN_X_BASE;

	// Use same spawn points for both initial spawn and respawn
	static const std::vector<double> spawn_points = {3.0, 3.5, 4.0, 4.5, 5.0, 5.5, 6.0, 6.5, 7.0};

	int max_index = int(spawn_points.size() - 1);
	int index = (_rng.genrand_uint32() % (max_index + 1));

	return Vector2(x_base, spawn_points[index]);
}

Vector2 TeamfightSimulationCore::_find_random_spawn_position_near(double center_x, double center_y, double radius) {
	return _find_random_spawn_position_near_excluding(center_x, center_y, radius, 0);
}

Vector2 TeamfightSimulationCore::_find_random_spawn_position_near_excluding(double center_x, double center_y, double radius, int64_t exclude_instance_id) {
	// Try random positions within radius, with collision checking
	constexpr int max_attempts = 50;
	constexpr double pi = 3.14159265358979323846;

	for (int attempt = 0; attempt < max_attempts; ++attempt) {
		// Generate random angle and distance
		double angle = _rng.random_random() * pi * 2.0;
		double distance = _rng.random_random() * radius;

		double test_x = center_x + Math::cos(angle) * distance;
		double test_y = center_y + Math::sin(angle) * distance;

		// Clamp to world boundaries
		test_x = Math::clamp(test_x, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
		test_y = Math::clamp(test_y, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);

		// Check collision with existing units, excluding specified unit
		if (!_position_collides_with_unit(test_x, test_y, exclude_instance_id)) {
			return Vector2(test_x, test_y);
		}
	}

	// Failed to find valid position, return invalid position
	return Vector2(-1.0, -1.0);
}

Vector2 TeamfightSimulationCore::_find_random_spawn_position_near_excluding_with_expansion(double center_x, double center_y, double initial_radius, double max_radius, int64_t exclude_instance_id, const std::vector<Vector2> &pending_positions) {
	double collision_radius_sq = UNIT_COLLISION_RADIUS * UNIT_COLLISION_RADIUS * 4.0; // (2 * r)^2
	
	// Try with initial radius first
	Vector2 result = _find_random_spawn_position_near_excluding(center_x, center_y, initial_radius, exclude_instance_id);
	if (result.x >= 0.0) {
		// Check collision with pending positions
		bool collides_with_pending = false;
		for (const Vector2 &pending_pos : pending_positions) {
			double dx = result.x - pending_pos.x;
			double dy = result.y - pending_pos.y;
			double dist_sq = dx * dx + dy * dy;
			if (dist_sq < collision_radius_sq) {
				collides_with_pending = true;
				break;
			}
		}
		if (!collides_with_pending) {
			return result;
		}
	}

	// Failed with initial radius, warn and expand to max_radius with exponential backoff
	//UtilityFunctions::push_warning(vformat("Spawn position not found within initial radius %.2f, expanding to max radius %.2f", initial_radius, max_radius));

	constexpr int expansion_steps = 5;
	constexpr double pi = 3.14159265358979323846;
	constexpr int attempts_per_step = 30;

	for (int step = 0; step < expansion_steps; ++step) {
		double expansion_factor = 1.0 + (double(step + 1) / double(expansion_steps)) * ((max_radius / initial_radius) - 1.0);
		double current_radius = initial_radius * expansion_factor;

		for (int attempt = 0; attempt < attempts_per_step; ++attempt) {
			double angle = _rng.random_random() * pi * 2.0;
			double distance = _rng.random_random() * current_radius;

			double test_x = center_x + Math::cos(angle) * distance;
			double test_y = center_y + Math::sin(angle) * distance;

			test_x = Math::clamp(test_x, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
			test_y = Math::clamp(test_y, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);

			if (!_position_collides_with_unit(test_x, test_y, exclude_instance_id)) {
				// Check collision with pending positions
				bool collides_with_pending = false;
				for (const Vector2 &pending_pos : pending_positions) {
					double dx = test_x - pending_pos.x;
					double dy = test_y - pending_pos.y;
					double dist_sq = dx * dx + dy * dy;
					if (dist_sq < collision_radius_sq) {
						collides_with_pending = true;
						break;
					}
				}
				if (!collides_with_pending) {
					return Vector2(test_x, test_y);
				}
			}
		}
	}

	// Still failed after all expansion attempts, log critical error
	UtilityFunctions::push_error(vformat("Spawn position failed completely: could not find valid position within max radius %.2f near (%.2f, %.2f). Active units: %d", max_radius, center_x, center_y, _units.size()));
	return Vector2(-1.0, -1.0);
}

int64_t TeamfightSimulationCore::_assign_spawn_slot(const StringName &team) {
	// Initialize slot tracking if needed
	std::vector<bool> &slots = (team == StringName("player")) ? _player_spawn_slots_used : _enemy_spawn_slots_used;
	if (slots.empty()) {
		slots.resize(9, false); // 9 spawn slots: 3.0, 3.5, 4.0, 4.5, 5.0, 5.5, 6.0, 6.5, 7.0
	}
	
	// Find available slot and randomly select one
	std::vector<int64_t> available_slots;
	for (int64_t i = 0; i < 9; ++i) {
		if (!slots[i]) {
			available_slots.push_back(i);
		}
	}
	
	if (available_slots.empty()) {
		// Fallback to random slot if none available
		return (_rng.genrand_uint32() % 9);
	}
	
	// Randomly select from available slots
	int64_t random_index = _rng.genrand_uint32() % available_slots.size();
	int64_t selected_slot = available_slots[random_index];
	slots[selected_slot] = true;
	return selected_slot;
}

void TeamfightSimulationCore::_release_spawn_slot(const StringName &team, int64_t slot_index) {
	std::vector<bool> &slots = (team == StringName("player")) ? _player_spawn_slots_used : _enemy_spawn_slots_used;
	if (slot_index >= 0 && slot_index < int(slots.size())) {
		slots[slot_index] = false;
	}
}

double TeamfightSimulationCore::_attack_range(const UnitState &unit) const {
	return sim::targeting::attack_range(unit);
}

double TeamfightSimulationCore::_effective_attack_range(const UnitState &unit) const {
	return sim::targeting::effective_attack_range(unit);
}

UnitState *TeamfightSimulationCore::_unit_by_id(int64_t instance_id) {
	sim::SimWorld w = _sim_world();
	return sim::targeting::unit_by_id(w, instance_id);
}

const UnitState *TeamfightSimulationCore::_unit_by_id(int64_t instance_id) const {
	sim::SimWorld w = _sim_world();
	return sim::targeting::unit_by_id(w, instance_id);
}

int64_t TeamfightSimulationCore::_unit_index_by_id(int64_t instance_id) const {
	sim::SimWorld w = _sim_world();
	return sim::targeting::unit_index_by_id(w, instance_id);
}

int64_t TeamfightSimulationCore::_target_index_for_unit(UnitState &unit) {
	sim::SimWorld w = _sim_world();
	return sim::targeting::target_index_for_unit(w, unit);
}

void TeamfightSimulationCore::_handle_death(UnitState &killer, UnitState &target) {
	if (!target.alive) {
		return;
	}
	int64_t target_id = target.instance_id;
	int64_t target_index = _unit_index_by_id(target_id);
	if (target_index >= 0) {
		_remove_alive_index(target.team, target_index);
	}
	if (target.is_marksman_role || target.is_mage_role || target.is_support_role) {
		if (target.team == sn_player()) {
			_tick_ctx.player_backliner_alive_count = Math::max(0, _tick_ctx.player_backliner_alive_count - 1);
		} else if (target.team == sn_enemy()) {
			_tick_ctx.enemy_backliner_alive_count = Math::max(0, _tick_ctx.enemy_backliner_alive_count - 1);
		}
	}
	target.alive = false;
	target.respawn_timer = get_effective_respawn_time(target);
	_uc(target).deaths += 1;
	_sync_targeting_frame_index(target_index, target);

	// Check if target is a minion (role_id="minion") - skip scoring for minions
	bool is_minion = (_uc(target).role_id == StringName("minion"));

	if (is_minion) {
		// Minions don't respawn - set respawn timer to 0
		target.respawn_timer = 0.0;
		// Skip scoring logic for minions
		return;
	}
	
	// Clear periodic effects on death
	_clear_periodic_effects(target);

	const std::unordered_map<int64_t, UnitStateCold::DamageSourceEntry> &damage_sources = _uc(target).damage_sources;
	// Killer = source with max accumulated damage in window; ties resolve to lower instance_id.
	int64_t killer_id = 0;
	double killer_damage = -1.0;
	for (const auto &entry : damage_sources) {
		int64_t source_id = entry.first;
		double dealt = entry.second.damage;
		double hit_time = entry.second.last_time;
		if (_time - hit_time > ASSIST_WINDOW) {
			continue;
		}
		bool better = false;
		if (killer_id == 0) {
			better = true;
		} else if (dealt > killer_damage + EPSILON) {
			better = true;
		} else if (Math::abs(dealt - killer_damage) <= EPSILON && source_id < killer_id) {
			better = true;
		}
		if (better) {
			killer_id = source_id;
			killer_damage = dealt;
		}
	}

	_emit_trace(StringName("death"), killer_id, target.instance_id, 0.0);
	
	// Emit passive AOE cleanup events (radius=0) to clear visualization
	UnitStateCold &target_cold = _uc(target);
	for (const UnitStateCold::PassiveAoeInfo &aoe_info : target_cold.passive_aoe_info) {
		_viewer_record_passive_aoe_fx(target, 0.0, aoe_info.passive_id);
	}

	UnitState *killer_unit = _unit_by_id(killer_id);
	if (killer_unit != nullptr) {
		_uc(*killer_unit).kills += 1;
		if (killer_unit->team == StringName("player")) {
			_player_kills += 1;
		} else if (killer_unit->team == StringName("enemy")) {
			_enemy_kills += 1;
		}
		// Run on_takedown effects for killer (runs before assists - killer gets first credit)
		EffectContext takedown_context = _build_context(*killer_unit, &target, nullptr, killer_damage, StringName("takedown"));
		_run_on_takedown_effects(*killer_unit, target, killer_damage, true, takedown_context);
	}

	// Collect assist IDs and award assists (both damage sources and benefactors)
	// Deduplicate to prevent double-dipping if a unit is both a damage source and benefactor
	std::set<int64_t> assist_ids;
	std::unordered_map<int64_t, double> assist_damage_map;  // Track damage for each assist (0.0 for benefactors)

	for (const auto &entry : damage_sources) {
		int64_t source_id = entry.first;
		if (source_id == killer_id) {
			continue;
		}
		double hit_time = entry.second.last_time;
		if (_time - hit_time <= ASSIST_WINDOW) {
			UnitState *assist_unit = _unit_by_id(source_id);
			if (assist_unit != nullptr) {
				_uc(*assist_unit).assists += 1;
				assist_ids.insert(source_id);
				assist_damage_map[source_id] = entry.second.damage;
			}
		}
	}

	const std::unordered_map<int64_t, double> empty_benefactors;
	const std::unordered_map<int64_t, double> &recent_benefactors = killer_unit != nullptr ? _uc(*killer_unit).recent_benefactors : empty_benefactors;
	for (const auto &entry : recent_benefactors) {
		int64_t benefactor_id = entry.first;
		double benefactor_time = entry.second;
		if (_time - benefactor_time > ASSIST_WINDOW) {
			continue;
		}
		UnitState *assist_unit = _unit_by_id(benefactor_id);
		if (assist_unit != nullptr) {
			_uc(*assist_unit).assists += 1;
			assist_ids.insert(benefactor_id);
			// Benefactors don't track damage, use 0.0 if not already in damage map
			if (assist_damage_map.find(benefactor_id) == assist_damage_map.end()) {
				assist_damage_map[benefactor_id] = 0.0;
			}
		}
	}

	// Run on_takedown effects once per unique assist
	for (int64_t assist_id : assist_ids) {
		UnitState *assist_unit = _unit_by_id(assist_id);
		if (assist_unit != nullptr) {
			double damage = assist_damage_map[assist_id];
			EffectContext takedown_context = _build_context(*assist_unit, &target, nullptr, damage, StringName("takedown"));
			_run_on_takedown_effects(*assist_unit, target, damage, false, takedown_context);
		}
	}

	// Release previous spawn slot if this unit had one and died again
	UnitStateCold &tcd = _uc(target);
	if (tcd.respawn_slot_index != -1) {
		_release_spawn_slot(target.team, tcd.respawn_slot_index);
	}

	// Assign new spawn slot for this unit
	tcd.respawn_slot_index = _assign_spawn_slot(target.team);
}

StringName TeamfightSimulationCore::_determine_winner() const {
	if (_player_kills > _enemy_kills) {
		return StringName("player");
	}
	if (_enemy_kills > _player_kills) {
		return StringName("enemy");
	}
	return StringName("draw");
}

void TeamfightSimulationCore::_respawn_unit(UnitState &unit) {
	if (unit.alive) {
		return;
	}
	int64_t unit_index = _unit_index_by_id(unit.instance_id);
	UnitStateCold &c = _uc(unit);
	unit.alive = true;
	unit.hp = get_effective_max_hp(unit);
	unit.mana = 0.0;
	unit.shield = 0.0;
	unit.perceived_threat = 0.0;
	unit.ability_cooldown = get_effective_ability_cd(unit);
	// Python parity: ultimate_timer is NOT reset on respawn (preserved across death like attack_cooldown).
	unit.stun_remaining = 0.0;
	unit.slow_remaining = 0.0;
	unit.slow_move_mult = 1.0;
	unit.root_remaining = 0.0;
	unit.silence_remaining = 0.0;
	unit.silence_ability_remaining = 0.0;
	unit.silence_ultimate_remaining = 0.0;
	unit.silence_blocks_abilities = false;
	unit.silence_blocks_ultimates = false;
	unit.disarm_remaining = 0.0;
	unit.stealth_remaining = 0.0;
	unit.stealth_break_on_attack = false;
	unit.stealth_break_on_ability = false;
	unit.stealth_break_on_damage_taken = false;
	_uc(unit).reflect_buffs.clear();
	unit.taunt_remaining = 0.0;
	unit.forced_target_remaining = 0.0;
	unit.last_kite_timer = 0.0;
	unit.target_switch_lock_timer = 0.0;
	unit.respawn_timer = 0.0;
	
	// Clear respawn-duration stat modifiers
	sim::stats_modifiers::clear_all_stat_modifiers(unit);
		
	c.damage_sources.clear();
	c.recent_benefactors.clear();
	c.last_hit_time = 0.0;
	std::fill(c.on_tick_effect_accumulators.begin(), c.on_tick_effect_accumulators.end(), 0.0);
	unit.respawned_this_tick = true;
	unit.cast_resolved_this_tick = false;
	// Clear casting state on respawn
	unit.casting_remaining = 0.0;
	c.casting_kind = StringName();
	c.casting_effect = EffectRecord();
	
	// Emit passive AOE events for visualization
	for (const UnitStateCold::PassiveAoeInfo &aoe_info : c.passive_aoe_info) {
		_viewer_record_passive_aoe_fx(unit, aoe_info.radius, aoe_info.passive_id);
	}
	unit.casting_target_id = 0;
	unit.casting_ally_target_id = 0;
	unit.has_casting_effect = false;
	unit.taunt_target_id = 0;
	unit.forced_target_id = 0;
	c.forced_target_kind = StringName();

	// Use assigned spawn slot if available, otherwise assign one
	if (c.respawn_slot_index == -1) {
		c.respawn_slot_index = _assign_spawn_slot(unit.team);
	}

	// Generate respawn position from assigned slot
	static const std::vector<double> spawn_points = {3.0, 3.5, 4.0, 4.5, 5.0, 5.5, 6.0, 6.5, 7.0};
	double x_base = (unit.team == StringName("player")) ? PLAYER_SPAWN_X_BASE : ENEMY_SPAWN_X_BASE;
	
	// Adjust melee champions to spawn 0.5 tiles closer to center
	Dictionary stats = Dictionary(c.stats);
	double attack_range = double(stats.get("attack_range", 0.0));
	if (attack_range <= 1.0) {  // Melee threshold
		if (unit.team == StringName("player")) {
			x_base += 0.5;
		} else {
			x_base -= 0.5;
		}
	}
	
	if (c.respawn_slot_index >= 0 && c.respawn_slot_index < int(spawn_points.size())) {
		unit.pos_x = x_base;
		unit.pos_y = spawn_points[static_cast<size_t>(c.respawn_slot_index)];
	} else {
		// Fallback to random position if slot is invalid
		Vector2 respawn_pos = _get_random_spawn_position(unit.team, true);
		unit.pos_x = respawn_pos.x;
		unit.pos_y = respawn_pos.y;
	}
	if (unit_index >= 0) {
		_add_alive_index(unit.team, unit_index);
	}
	_sync_targeting_frame_index(unit_index, unit);
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

namespace {
bool targeting_profile_enabled() {
	if (s_targeting_profile_force_enabled.load(std::memory_order_relaxed)) {
		return true;
	}
	const char *v = std::getenv("TEAMFIGHT_TARGETING_PROFILE");
	if (v == nullptr || v[0] == '\0') {
		return false;
	}
	return !(v[0] == '0' && v[1] == '\0');
}
} // namespace

void TeamfightSimulationCore::_sim_profile_reset() {
	_sim_profile_ns_projectiles = 0;
	_sim_profile_ns_prepare_tick_ctx = 0;
	_sim_profile_ns_refresh_pressure_pre = 0;
	_sim_profile_ns_update_units = 0;
	_sim_profile_ns_refresh_pressure_post = 0;
	_sim_profile_tick_count = 0;
	_sim_profile_uu_dead_respawn = 0;
	_sim_profile_uu_cooldowns_cc = 0;
	_sim_profile_uu_separation = 0;
	_sim_profile_uu_threat_and_assist = 0;
	_sim_profile_uu_regen_on_tick = 0;
	_sim_profile_uu_casting = 0;
	_sim_profile_uu_targeting = 0;
	_sim_profile_uu_combat = 0;
	_sim_profile_uu_movement = 0;
	_sim_profile_se_base = 0;
	_sim_profile_se_calls = 0;
	_sim_profile_uc_attack_cooldown = 0;
	_sim_profile_uc_distance_calc = 0;
	_sim_profile_uc_hit_validation = 0;
	_sim_profile_uc_damage_apply = 0;
	_sim_profile_uc_auto_attack = 0;
	_sim_profile_uc_ability = 0;
	_sim_profile_ucc_attack_cd = 0;
	_sim_profile_ucc_ability_cd = 0;
	_sim_profile_ucc_retarget = 0;
	_sim_profile_ucc_target_switch = 0;
	_sim_profile_ucc_stun = 0;
	_sim_profile_ucc_slow = 0;
	_sim_profile_ucc_root = 0;
	_sim_profile_ucc_silence = 0;
	_sim_profile_ucc_disarm = 0;
	_sim_profile_ucc_stealth = 0;
	_sim_profile_ucc_shield = 0;
	_sim_profile_ucc_reflect = 0;
	_sim_profile_ucc_taunt = 0;
	_sim_profile_ucc_forced_target = 0;
	_sim_profile_ctx_team_centers = 0;
	_sim_profile_ctx_role_classification = 0;
	_sim_profile_ctx_targeting_sync = 0;
	_sim_profile_ctx_spatial_grid = 0;
	_sim_profile_ctx_density = 0;
	_sim_profile_um_kiting = 0;
	_sim_profile_um_kiting_spatial = 0;
	_sim_profile_um_kiting_brute = 0;
	_sim_profile_um_toward = 0;
	_sim_profile_um_boundary = 0;
	_sim_profile_um_nudge = 0;
	_sim_profile_ur_hp_mana = 0;
	_sim_profile_ur_effects = 0;
	_sim_profile_ur_periodic = 0;
	_sim_profile_ur_channel = 0;
	_sim_profile_tgt_retarget_keeps = 0;
	_sim_profile_tgt_enemy_scans = 0;
	_sim_profile_tgt_candidates_scored = 0;
	_sim_profile_tgt_candidates_prefix_pruned = 0;
	_sim_profile_tgt_ally_scans = 0;
	_sim_profile_tgt_frame_syncs = 0;
	_sim_profile_tgt_ties_adjusted = 0;
	_sim_profile_tgt_ties_raw = 0;
	_sim_profile_tgt_ties_distance = 0;
	_sim_profile_tgt_ties_instance = 0;
}

void TeamfightSimulationCore::_sim_profile_emit_json_stderr() const {
	Dictionary profile;
	profile["ns_projectiles"] = _sim_profile_ns_projectiles;
	profile["ns_prepare_tick_ctx"] = _sim_profile_ns_prepare_tick_ctx;
	profile["ns_refresh_pressure_pre"] = _sim_profile_ns_refresh_pressure_pre;
	profile["ns_update_units"] = _sim_profile_ns_update_units;
	profile["ns_refresh_pressure_post"] = _sim_profile_ns_refresh_pressure_post;
	profile["tick_count"] = _sim_profile_tick_count;
	profile["uu_dead_respawn"] = _sim_profile_uu_dead_respawn;
	profile["uu_cooldowns_cc"] = _sim_profile_uu_cooldowns_cc;
	profile["uu_separation"] = _sim_profile_uu_separation;
	profile["uu_threat_and_assist"] = _sim_profile_uu_threat_and_assist;
	profile["uu_regen_on_tick"] = _sim_profile_uu_regen_on_tick;
	profile["uu_casting"] = _sim_profile_uu_casting;
	profile["uu_targeting"] = _sim_profile_uu_targeting;
	profile["uu_combat"] = _sim_profile_uu_combat;
	profile["uu_movement"] = _sim_profile_uu_movement;
	profile["se_base"] = _sim_profile_se_base;
	profile["se_calls"] = _sim_profile_se_calls;
	profile["uc_attack_cooldown"] = _sim_profile_uc_attack_cooldown;
	profile["uc_distance_calc"] = _sim_profile_uc_distance_calc;
	profile["uc_hit_validation"] = _sim_profile_uc_hit_validation;
	profile["uc_damage_apply"] = _sim_profile_uc_damage_apply;
	profile["uc_auto_attack"] = _sim_profile_uc_auto_attack;
	profile["uc_ability"] = _sim_profile_uc_ability;
	profile["ucc_attack_cd"] = _sim_profile_ucc_attack_cd;
	profile["ucc_ability_cd"] = _sim_profile_ucc_ability_cd;
	profile["ucc_retarget"] = _sim_profile_ucc_retarget;
	profile["ucc_target_switch"] = _sim_profile_ucc_target_switch;
	profile["ucc_stun"] = _sim_profile_ucc_stun;
	profile["ucc_slow"] = _sim_profile_ucc_slow;
	profile["ucc_root"] = _sim_profile_ucc_root;
	profile["ucc_silence"] = _sim_profile_ucc_silence;
	profile["ucc_disarm"] = _sim_profile_ucc_disarm;
	profile["ucc_stealth"] = _sim_profile_ucc_stealth;
	profile["ucc_shield"] = _sim_profile_ucc_shield;
	profile["ucc_reflect"] = _sim_profile_ucc_reflect;
	profile["ucc_taunt"] = _sim_profile_ucc_taunt;
	profile["ucc_forced_target"] = _sim_profile_ucc_forced_target;
	profile["ctx_team_centers"] = _sim_profile_ctx_team_centers;
	profile["ctx_role_classification"] = _sim_profile_ctx_role_classification;
	profile["ctx_targeting_sync"] = _sim_profile_ctx_targeting_sync;
	profile["ctx_spatial_grid"] = _sim_profile_ctx_spatial_grid;
	profile["ctx_density"] = _sim_profile_ctx_density;
	profile["um_kiting"] = _sim_profile_um_kiting;
	profile["um_kiting_spatial"] = _sim_profile_um_kiting_spatial;
	profile["um_kiting_brute"] = _sim_profile_um_kiting_brute;
	profile["um_toward"] = _sim_profile_um_toward;
	profile["um_boundary"] = _sim_profile_um_boundary;
	profile["um_nudge"] = _sim_profile_um_nudge;
	profile["ur_hp_mana"] = _sim_profile_ur_hp_mana;
	profile["ur_effects"] = _sim_profile_ur_effects;
	profile["ur_periodic"] = _sim_profile_ur_periodic;
	profile["ur_channel"] = _sim_profile_ur_channel;
	if (_sim_profile_targeting_active) {
		profile["tgt_retarget_keeps"] = _sim_profile_tgt_retarget_keeps;
		profile["tgt_enemy_scans"] = _sim_profile_tgt_enemy_scans;
		profile["tgt_candidates_scored"] = _sim_profile_tgt_candidates_scored;
		profile["tgt_candidates_prefix_pruned"] = _sim_profile_tgt_candidates_prefix_pruned;
		profile["tgt_ally_scans"] = _sim_profile_tgt_ally_scans;
		profile["tgt_frame_syncs"] = _sim_profile_tgt_frame_syncs;
		profile["tgt_ties_adjusted"] = _sim_profile_tgt_ties_adjusted;
		profile["tgt_ties_raw"] = _sim_profile_tgt_ties_raw;
		profile["tgt_ties_distance"] = _sim_profile_tgt_ties_distance;
		profile["tgt_ties_instance"] = _sim_profile_tgt_ties_instance;
	}
	
	String json = JSON::stringify(profile);
	UtilityFunctions::print(json);
}

void TeamfightSimulationCore::_step_tick(bool profile_sim) {
	// Python World.step(): tick++ then time = tick * tick_rate
	_tick += 1;
	_time = double(_tick) * _tick_rate;
	_viewer_fx_events.clear();
	if (profile_sim) {
		_sim_profile_tick_count += 1;
	}
	for (UnitState &unit : _units) {
		unit.respawned_this_tick = false;
		unit.cast_resolved_this_tick = false;
		unit.incoming_target_count = 0;
	}
	// Python: projectiles resolve before unit updates.
	if (profile_sim) {
		auto t0 = std::chrono::steady_clock::now();
		_update_projectiles();
		_sim_profile_ns_projectiles += sim_profile_elapsed_ns(t0);
	} else {
		_update_projectiles();
	}
	// Process pending spawns before tick context is prepared so minions are included in role indices
	_process_pending_spawns();
	if (profile_sim) {
		auto t0 = std::chrono::steady_clock::now();
		_prepare_tick_context();
		_sim_profile_ns_prepare_tick_ctx += sim_profile_elapsed_ns(t0);
	} else {
		_prepare_tick_context();
	}
	if (profile_sim) {
		auto t0 = std::chrono::steady_clock::now();
		for (UnitState &unit : _units) {
			_update_unit(unit, true);
		}
		_sim_profile_ns_update_units += sim_profile_elapsed_ns(t0);
	} else {
		for (UnitState &unit : _units) {
			_update_unit(unit, false);
		}
	}
}

// Simulation hot path: dominates wall-time once FFI/batching is amortized.
// Profile with TEAMFIGHT_SIM_PROFILE=1 (benchmark: --sim-profile) before altering targeting tick order/_step_tick hot paths.
void TeamfightSimulationCore::_simulate() {
	const bool profile = _sim_profile_env_enabled();
	_sim_profile_active = profile;
	_sim_profile_targeting_active = profile && targeting_profile_enabled();
	if (profile) {
		_sim_profile_reset();
	}
	double effective_tick_rate = Math::max(_tick_rate, EPSILON);
	int64_t max_ticks = int64_t(Math::ceil(MATCH_DURATION / effective_tick_rate));
	for (int64_t tick_index = 0; tick_index < max_ticks; ++tick_index) {
		_step_tick(profile);
	}
	
	_sudden_death_ticks = 0;
	
	// Sudden death: continue simulation until kills are unequal
	if (_player_kills == _enemy_kills) {
		while (_player_kills == _enemy_kills && _sudden_death_ticks < SUDDEN_DEATH_MAX_TICKS) {
			_step_tick(profile);
			_sudden_death_ticks++;
		}
		
		// Log only when safety limit is reached (indicating a draw)
		if (_sudden_death_ticks >= SUDDEN_DEATH_MAX_TICKS) {
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
	}
	
	_winner_team = _determine_winner();
	
	if (profile) {
		_sim_profile_emit_json_stderr();
	}
	_sim_profile_active = false;
	_sim_profile_targeting_active = false;
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
		tick_profile.uu_dead_respawn = &_sim_profile_uu_dead_respawn;
		tick_profile.uu_cooldowns_cc = &_sim_profile_uu_cooldowns_cc;
		tick_profile.uu_separation = &_sim_profile_uu_separation;
		tick_profile.uu_threat_and_assist = &_sim_profile_uu_threat_and_assist;
		tick_profile.uu_regen_on_tick = &_sim_profile_uu_regen_on_tick;
		tick_profile.uu_casting = &_sim_profile_uu_casting;
		tick_profile.uu_targeting = &_sim_profile_uu_targeting;
		tick_profile.uu_combat = &_sim_profile_uu_combat;
		tick_profile.uu_movement = &_sim_profile_uu_movement;
		tick_profile.ucc_attack_cd = &_sim_profile_ucc_attack_cd;
		tick_profile.ucc_ability_cd = &_sim_profile_ucc_ability_cd;
		tick_profile.ucc_retarget = &_sim_profile_ucc_retarget;
		tick_profile.ucc_target_switch = &_sim_profile_ucc_target_switch;
		tick_profile.ucc_stun = &_sim_profile_ucc_stun;
		tick_profile.ucc_slow = &_sim_profile_ucc_slow;
		tick_profile.ucc_root = &_sim_profile_ucc_root;
		tick_profile.ucc_silence = &_sim_profile_ucc_silence;
		tick_profile.ucc_disarm = &_sim_profile_ucc_disarm;
		tick_profile.ucc_stealth = &_sim_profile_ucc_stealth;
		tick_profile.ucc_shield = &_sim_profile_ucc_shield;
		tick_profile.ucc_reflect = &_sim_profile_ucc_reflect;
		tick_profile.ucc_taunt = &_sim_profile_ucc_taunt;
		tick_profile.ucc_forced_target = &_sim_profile_ucc_forced_target;
		tick_profile.ur_effects = &_sim_profile_ur_effects;
		tick_profile.ur_channel = &_sim_profile_ur_channel;
		tick_profile.ur_periodic = &_sim_profile_ur_periodic;
		tick_profile.uc_distance_calc = &_sim_profile_uc_distance_calc;
		tick_profile.uc_ability = &_sim_profile_uc_ability;
		tick_profile.uc_auto_attack = &_sim_profile_uc_auto_attack;
		tick_profile.um_kiting = &_sim_profile_um_kiting;
		tick_profile.um_kiting_spatial = &_sim_profile_um_kiting_spatial;
		tick_profile.um_kiting_brute = &_sim_profile_um_kiting_brute;
		tick_profile.um_toward = &_sim_profile_um_toward;
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

bool TeamfightSimulationCore::_try_cast_ability(UnitState &unit, UnitState &target, double distance) {
	sim::SimWorld w = _sim_world();
	return sim::combat::try_cast_ability(w, _sim_host_callbacks, _combat_host_hooks(), unit, target, distance);
}

bool TeamfightSimulationCore::_try_cast_ultimate(UnitState &unit, UnitState &target, double distance) {
	sim::SimWorld w = _sim_world();
	return sim::combat::try_cast_ultimate(w, _sim_host_callbacks, _combat_host_hooks(), unit, target, distance);
}

bool TeamfightSimulationCore::_start_cast(UnitState &unit, UnitState &target, double distance, const StringName &action_kind) {
	sim::SimWorld w = _sim_world();
	return sim::combat::start_cast(w, _sim_host_callbacks, _combat_host_hooks(), unit, target, distance, action_kind);
}

void TeamfightSimulationCore::_resolve_cast(UnitState &unit) {
	sim::SimWorld w = _sim_world();
	sim::combat::resolve_cast(w, _sim_host_callbacks, _combat_host_hooks(), unit);
}

void TeamfightSimulationCore::_perform_auto_attack(UnitState &unit, UnitState &target, double distance) {
	sim::SimWorld w = _sim_world();
	sim::combat::perform_auto_attack(w, _sim_host_callbacks, _combat_host_hooks(), unit, target, distance, _projectiles);
}

void TeamfightSimulationCore::_move_toward_target(UnitState &unit, UnitState &target) {
	sim::SimWorld w = _sim_world();
	sim::movement::move_toward_target(w, unit, target);
}

void TeamfightSimulationCore::_move_toward_target_with_range(UnitState &unit, UnitState &target, double target_range) {
	sim::SimWorld w = _sim_world();
	sim::movement::move_toward_target_with_range(w, unit, target, target_range);
}

bool TeamfightSimulationCore::_kite_from_enemies(UnitState &unit, bool profile_sim) {
	sim::SimWorld w = _sim_world();
	sim::movement::KiteProfileCounters profile{};
	if (profile_sim) {
		profile.active = true;
		profile.kiting_spatial = &_sim_profile_um_kiting_spatial;
		profile.kiting_brute = &_sim_profile_um_kiting_brute;
	}
	return sim::movement::kite_from_enemies(w, _sim_host_callbacks, unit, profile.active ? &profile : nullptr);
}

void TeamfightSimulationCore::_resolve_projectile(const ProjectileState &projectile) {
	sim::SimWorld w = _sim_world();
	sim::combat::resolve_projectile(w, _sim_host_callbacks, _combat_host_hooks(), projectile);
}

void TeamfightSimulationCore::_update_projectiles() {
	sim::SimWorld w = _sim_world();
	sim::combat::ProjectileBuffers buffers{ _projectiles, _active_projectiles, _scratch_projectiles };
	sim::combat::ProjectileMatchState match{ _sudden_death_ticks, _player_kills, _enemy_kills };
	sim::combat::update_projectiles(w, _sim_host_callbacks, _combat_host_hooks(), buffers, match);
}


Dictionary TeamfightSimulationCore::_execute_effect(const EffectRecord &effect, EffectContext &context) {
	sim::SimWorld world = _sim_world();
	return sim::effects::execution::execute(effect, context, world, _sim_host_callbacks, _sim_exec_callbacks, _sim_match_host());
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

Dictionary TeamfightSimulationCore::run_match(const Variant &match_input) {
	_ensure_catalog_loaded();
	Dictionary input = _coerce_match_input(match_input);
	if (input.is_empty()) {
		UtilityFunctions::push_error("TeamfightSimulationCore.run_match() expected MatchReplayInput or Dictionary.");
		return Dictionary();
	}
	_reset_runtime_state();
	_populate_runtime_state(input);
	_simulate();
	return _build_summary();
}

Dictionary TeamfightSimulationCore::run_match_stats(const Variant &match_input) {
	_ensure_catalog_loaded();
	Dictionary input = _coerce_match_input(match_input);
	if (input.is_empty()) {
		UtilityFunctions::push_error("TeamfightSimulationCore.run_match_stats() expected MatchReplayInput or Dictionary.");
		return Dictionary();
	}
	_reset_runtime_state();
	_populate_runtime_state(input);
	_simulate();
	return _build_stats_summary();
}

void TeamfightSimulationCore::run_match_simulation_only(const Variant &match_input) {
	_ensure_catalog_loaded();
	Dictionary input = _coerce_match_input(match_input);
	if (input.is_empty()) {
		UtilityFunctions::push_error("TeamfightSimulationCore.run_match_simulation_only() expected MatchReplayInput or Dictionary.");
		return;
	}
	_reset_runtime_state();
	_populate_runtime_state(input);
	_simulate();
}

void TeamfightSimulationCore::run_matches_simulation_only(const Array &match_inputs) {
	const int64_t total = match_inputs.size();
	for (int64_t index = 0; index < total; ++index) {
		run_match_simulation_only(match_inputs[index]);
		clear();
		const int64_t done = index + 1;
		if (done % 1000 == 0) {
			benchmark_progress_add(1000);
		}
	}
	const int64_t tail = total % 1000;
	if (tail != 0) {
		benchmark_progress_add(tail);
	}
}

void TeamfightSimulationCore::run_generated_matches_simulation_only(int64_t base_seed, int64_t batch_count, int64_t team_size) {
	if (batch_count <= 0) {
		return;
	}
	const bool bench_phases = bench_phases_env_enabled();
	uint64_t ns_catalog_ensure = 0;
	uint64_t ns_chunk_preamble = 0;
	uint64_t ns_match_setup_total = 0;
	uint64_t ns_simulate_total = 0;
	auto t_catalog0 = std::chrono::steady_clock::now();
	_ensure_catalog_loaded();
	if (bench_phases) {
		ns_catalog_ensure = sim_profile_elapsed_ns(t_catalog0);
	}
	auto t_preamble0 = std::chrono::steady_clock::now();
	Array champion_keys = _catalog.effective_champion_by_archetype.keys();
	const int64_t champion_count = champion_keys.size();
	if (champion_count <= 0) {
		UtilityFunctions::push_error("TeamfightSimulationCore.run_generated_matches_simulation_only() requires champion catalog.");
		return;
	}
	const int64_t units_per_team = Math::max(int64_t(1), team_size);
	std::vector<StringName> archetypes;
	archetypes.reserve(static_cast<size_t>(champion_count));
	for (int64_t index = 0; index < champion_count; ++index) {
		archetypes.push_back(StringName(String(champion_keys[index])));
	}
	Dictionary spawn_spec;
	int64_t next_progress = 0;
	if (bench_phases) {
		ns_chunk_preamble = sim_profile_elapsed_ns(t_preamble0);
	}
	for (int64_t match_index = 0; match_index < batch_count; ++match_index) {
		auto t_match0 = std::chrono::steady_clock::now();
		const int64_t seed = base_seed + match_index;
		_reset_runtime_state();
		_seed = seed;
		_tick_rate = DEFAULT_TICK_RATE;
		_record_events = false;
		_debug_combat_trace = false;
		_trace_buffer.clear();
		_rng.seed_int64(_seed);
		CPythonRandom draft_rng;
		draft_rng.seed_int64(seed);
		int64_t next_instance_id = 1;
		
		// Create a mutable copy of archetypes for selection
		std::vector<StringName> available_archetypes = archetypes;
		
		// Generate player team
		for (int64_t slot = 0; slot < units_per_team; ++slot) {
			if (available_archetypes.empty()) {
				break; // No more champions available
			}
			const size_t archetype_index = static_cast<size_t>(draft_rng.genrand_uint32() % uint32_t(available_archetypes.size()));
			StringName selected_archetype = available_archetypes[archetype_index];
			
			// Remove selected archetype from available pool
			available_archetypes.erase(available_archetypes.begin() + archetype_index);
			
			spawn_spec.clear();
			spawn_spec["archetype_id"] = selected_archetype;
			std::pair<UnitState, UnitStateCold> built = _build_unit_state(spawn_spec, sn_player(), next_instance_id);
			if (built.first.instance_id == 0) {
				continue;
			}
			const int64_t unit_index = int64_t(_units.size());
			_units.push_back(std::move(built.first));
			_unit_cold.push_back(std::move(built.second));
			_unit_index_map[next_instance_id] = unit_index;
			_add_alive_index(sn_player(), unit_index);
			_targeting_frame.push_back(_make_targeting_frame_entry(_units[static_cast<size_t>(unit_index)]));
			_player_comp.append(_unit_cold[static_cast<size_t>(unit_index)].archetype_id);
			next_instance_id += 1;
		}
		
		// Generate enemy team from remaining champions
		for (int64_t slot = 0; slot < units_per_team; ++slot) {
			if (available_archetypes.empty()) {
				break; // No more champions available
			}
			const size_t archetype_index = static_cast<size_t>(draft_rng.genrand_uint32() % uint32_t(available_archetypes.size()));
			StringName selected_archetype = available_archetypes[archetype_index];
			
			// Remove selected archetype from available pool
			available_archetypes.erase(available_archetypes.begin() + archetype_index);
			
			spawn_spec.clear();
			spawn_spec["archetype_id"] = selected_archetype;
			std::pair<UnitState, UnitStateCold> built = _build_unit_state(spawn_spec, sn_enemy(), next_instance_id);
			if (built.first.instance_id == 0) {
				continue;
			}
			const int64_t unit_index = int64_t(_units.size());
			_units.push_back(std::move(built.first));
			_unit_cold.push_back(std::move(built.second));
			_unit_index_map[next_instance_id] = unit_index;
			_add_alive_index(sn_enemy(), unit_index);
			_targeting_frame.push_back(_make_targeting_frame_entry(_units[static_cast<size_t>(unit_index)]));
			_enemy_comp.append(_unit_cold[static_cast<size_t>(unit_index)].archetype_id);
			next_instance_id += 1;
		}
		_build_role_strategy_cache();
		_prepare_tick_context();
		if (bench_phases) {
			ns_match_setup_total += sim_profile_elapsed_ns(t_match0);
		}
		auto t_sim0 = std::chrono::steady_clock::now();
		_simulate();
		if (bench_phases) {
			ns_simulate_total += sim_profile_elapsed_ns(t_sim0);
		}
		_reset_runtime_state();
		next_progress += 1;
		if (next_progress == 1000) {
			benchmark_progress_add(1000);
			next_progress = 0;
		}
	}
	if (next_progress != 0) {
		benchmark_progress_add(next_progress);
	}
	if (bench_phases) {
		const double inv_bc = batch_count > 0 ? 1.0 / double(batch_count) : 0.0;
		std::fprintf(stderr,
				"{\"bench_phases\":{\"batch_count\":%lld,\"ns_catalog_ensure\":%llu,\"ns_chunk_preamble\":%llu,"
				"\"ns_match_setup_total\":%llu,\"ns_simulate_total\":%llu,"
				"\"avg_ns_per_match_setup\":%.0f,\"avg_ns_per_match_simulate\":%.0f}}\n",
				static_cast<long long>(batch_count),
				static_cast<unsigned long long>(ns_catalog_ensure),
				static_cast<unsigned long long>(ns_chunk_preamble),
				static_cast<unsigned long long>(ns_match_setup_total),
				static_cast<unsigned long long>(ns_simulate_total),
				double(ns_match_setup_total) * inv_bc,
				double(ns_simulate_total) * inv_bc);
		std::fflush(stderr);
	}
}

Dictionary TeamfightSimulationCore::run_generated_matches_stats_partial(int64_t base_seed, int64_t batch_count, int64_t team_size, bool include_match_log, double tick_rate) {
	_ensure_catalog_loaded();
	Dictionary result;
	if (batch_count <= 0) {
		result["stats_partial"] = Dictionary();
		result["matchup_data"] = Dictionary();
		return result;
	}

	Array champion_keys = _catalog.effective_champion_by_archetype.keys();
	const int64_t champion_count = champion_keys.size();
	if (champion_count <= 0) {
		UtilityFunctions::push_error("TeamfightSimulationCore.run_generated_matches_stats_partial() requires champion catalog.");
		result["stats_partial"] = Dictionary();
		result["matchup_data"] = Dictionary();
		return result;
	}

	const int64_t units_per_team = Math::max(int64_t(1), team_size);
	std::vector<StringName> archetypes;
	archetypes.reserve(static_cast<size_t>(champion_count));
	for (int64_t index = 0; index < champion_count; ++index) {
		archetypes.push_back(StringName(String(champion_keys[index])));
	}

	auto make_stat_entry = []() -> Dictionary {
		Dictionary entry;
		entry["w"] = int64_t(0);
		entry["l"] = int64_t(0);
		entry["d"] = int64_t(0);
		entry["dmg_d"] = 0.0;
		entry["dmg_r"] = 0.0;
		entry["dmg_m"] = 0.0;
		entry["heal"] = 0.0;
		entry["heal_auto"] = 0.0;
		entry["heal_ability"] = 0.0;
		entry["heal_ultimate"] = 0.0;
		entry["heal_passive"] = 0.0;
		entry["shield"] = 0.0;
		entry["shield_auto"] = 0.0;
		entry["shield_ability"] = 0.0;
		entry["shield_ultimate"] = 0.0;
		entry["shield_passive"] = 0.0;
		entry["stuns"] = int64_t(0);
		entry["kills"] = int64_t(0);
		entry["deaths"] = int64_t(0);
		entry["assists"] = int64_t(0);
		entry["d_auto"] = 0.0;
		entry["d_ab"] = 0.0;
		entry["d_ult"] = 0.0;
		entry["d_passive"] = 0.0;
		entry["minion_dmg_d"] = 0.0;
		entry["minion_dmg_r"] = 0.0;
		return entry;
	};
	auto make_role_entry = [&make_stat_entry]() -> Dictionary {
		Dictionary entry = make_stat_entry();
		entry.erase("kills");
		entry.erase("deaths");
		entry.erase("assists");
		return entry;
	};
	auto accumulate_common = [](Dictionary &entry, const UnitStateCold &c) {
		entry["dmg_d"] = double(entry["dmg_d"]) + c.damage_dealt;
		entry["dmg_r"] = double(entry["dmg_r"]) + c.damage_received;
		entry["dmg_m"] = double(entry["dmg_m"]) + c.damage_mitigated;
		entry["heal"] = double(entry["heal"]) + c.healing_done;
		entry["heal_auto"] = double(entry["heal_auto"]) + c.healing_done_auto;
		entry["heal_ability"] = double(entry["heal_ability"]) + c.healing_done_ability;
		entry["heal_ultimate"] = double(entry["heal_ultimate"]) + c.healing_done_ultimate;
		entry["heal_passive"] = double(entry["heal_passive"]) + c.healing_done_passive;
		entry["shield"] = double(entry["shield"]) + c.shielding_done;
		entry["shield_auto"] = double(entry["shield_auto"]) + c.shielding_done_auto;
		entry["shield_ability"] = double(entry["shield_ability"]) + c.shielding_done_ability;
		entry["shield_ultimate"] = double(entry["shield_ultimate"]) + c.shielding_done_ultimate;
		entry["shield_passive"] = double(entry["shield_passive"]) + c.shielding_done_passive;
		entry["stuns"] = int64_t(entry["stuns"]) + c.stuns;
		entry["d_auto"] = double(entry["d_auto"]) + c.damage_dealt_auto;
		entry["d_ab"] = double(entry["d_ab"]) + c.damage_dealt_ability;
		entry["d_ult"] = double(entry["d_ult"]) + c.damage_dealt_ultimate;
		entry["d_passive"] = double(entry["d_passive"]) + c.damage_dealt_passive;
	};
	auto add_record = [&accumulate_common](Dictionary &entry, const UnitStateCold &c, bool won, bool draw, bool include_kda) {
		if (draw) {
			entry["d"] = int64_t(entry["d"]) + 1;
		} else if (won) {
			entry["w"] = int64_t(entry["w"]) + 1;
		} else {
			entry["l"] = int64_t(entry["l"]) + 1;
		}
		accumulate_common(entry, c);
		if (include_kda) {
			entry["kills"] = int64_t(entry["kills"]) + c.kills;
			entry["deaths"] = int64_t(entry["deaths"]) + c.deaths;
			entry["assists"] = int64_t(entry["assists"]) + c.assists;
		}
	};
	auto sorted_combo_label = [](const Array &comp) -> String {
		std::vector<String> names;
		names.reserve(static_cast<size_t>(comp.size()));
		for (int64_t i = 0; i < comp.size(); ++i) {
			names.push_back(String(comp[i]));
		}
		std::sort(names.begin(), names.end());
		String label;
		for (size_t i = 0; i < names.size(); ++i) {
			if (i > 0) {
				label += " + ";
			}
			label += names[i];
		}
		return label;
	};
	auto record_matchup = [](Dictionary &matchup_data, const StringName &champion_id, const String &key, bool won) {
		const String champion = String(champion_id);
		Dictionary champion_data = Dictionary(matchup_data.get(champion, Dictionary()));
		Dictionary data = Dictionary(champion_data.get(key, Dictionary()));
		if (data.is_empty()) {
			data["wins"] = int64_t(0);
			data["losses"] = int64_t(0);
			data["winrate"] = 0.0;
		}
		if (won) {
			data["wins"] = int64_t(data["wins"]) + 1;
		} else {
			data["losses"] = int64_t(data["losses"]) + 1;
		}
		const int64_t wins = int64_t(data["wins"]);
		const int64_t losses = int64_t(data["losses"]);
		const int64_t total = wins + losses;
		data["winrate"] = total > 0 ? double(wins) / double(total) : 0.0;
		champion_data[key] = data;
		matchup_data[champion] = champion_data;
	};
	auto record_matchup_result = [&record_matchup](Dictionary &matchup_data, const Array &winners, const Array &losers) {
		for (int64_t wi = 0; wi < winners.size(); ++wi) {
			StringName winner = StringName(winners[wi]);
			for (int64_t li = 0; li < losers.size(); ++li) {
				StringName loser = StringName(losers[li]);
				record_matchup(matchup_data, winner, "vs_" + String(loser), true);
			}
			for (int64_t ai = 0; ai < winners.size(); ++ai) {
				StringName ally = StringName(winners[ai]);
				if (winner != ally) {
					record_matchup(matchup_data, winner, "with_" + String(ally), true);
				}
			}
		}
		for (int64_t li = 0; li < losers.size(); ++li) {
			StringName loser = StringName(losers[li]);
			for (int64_t wi = 0; wi < winners.size(); ++wi) {
				StringName winner = StringName(winners[wi]);
				record_matchup(matchup_data, loser, "vs_" + String(winner), false);
			}
			for (int64_t ai = 0; ai < losers.size(); ++ai) {
				StringName ally = StringName(losers[ai]);
				if (loser != ally) {
					record_matchup(matchup_data, loser, "with_" + String(ally), false);
				}
			}
		}
	};
	auto append_generated_unit = [this](Dictionary &spawn_spec, const StringName &team, const StringName &archetype, int64_t &next_instance_id, Array &comp) {
		spawn_spec.clear();
		spawn_spec["archetype_id"] = archetype;
		std::pair<UnitState, UnitStateCold> built = _build_unit_state(spawn_spec, team, next_instance_id);
		if (built.first.instance_id == 0) {
			return;
		}
		const int64_t unit_index = int64_t(_units.size());
		_units.push_back(std::move(built.first));
		_unit_cold.push_back(std::move(built.second));
		_unit_index_map[next_instance_id] = unit_index;
		_add_alive_index(team, unit_index);
		_targeting_frame.push_back(_make_targeting_frame_entry(_units[static_cast<size_t>(unit_index)]));
		comp.append(_unit_cold[static_cast<size_t>(unit_index)].archetype_id);

		// Update max instance ID for efficient minion spawning
		if (next_instance_id > _max_instance_id) {
			_max_instance_id = next_instance_id;
		}

		next_instance_id += 1;
	};
	auto pick_index = [](const Ref<RandomNumberGenerator> &rng, int64_t upper_inclusive) -> int64_t {
		return int64_t(rng->randi_range(0, int32_t(upper_inclusive)));
	};

	Dictionary bucket;
	bucket["p1"] = int64_t(0);
	bucket["p2"] = int64_t(0);
	bucket["draws"] = int64_t(0);
	bucket["total"] = int64_t(0);
	bucket["heroes"] = Dictionary();
	bucket["roles"] = Dictionary();
	bucket["combos"] = Dictionary();
	Array match_logs;
	Dictionary matchup_data;
	Dictionary spawn_spec;
	int64_t next_progress = 0;

	for (int64_t match_index = 0; match_index < batch_count; ++match_index) {
		const int64_t seed = base_seed + match_index;
		_reset_runtime_state();
		_seed = seed;
		_tick_rate = tick_rate;
		_record_events = false;
		_debug_combat_trace = false;
		_trace_buffer.clear();
		_rng.seed_int64(_seed);

		Ref<RandomNumberGenerator> draft_rng;
		draft_rng.instantiate();
		draft_rng->set_seed(uint64_t(seed));
		int64_t next_instance_id = 1;
		if (champion_count < units_per_team * 2) {
			for (int64_t slot = 0; slot < units_per_team; ++slot) {
				const int64_t archetype_index = pick_index(draft_rng, champion_count - 1);
				append_generated_unit(spawn_spec, sn_player(), archetypes[static_cast<size_t>(archetype_index)], next_instance_id, _player_comp);
			}
			for (int64_t slot = 0; slot < units_per_team; ++slot) {
				const int64_t archetype_index = pick_index(draft_rng, champion_count - 1);
				append_generated_unit(spawn_spec, sn_enemy(), archetypes[static_cast<size_t>(archetype_index)], next_instance_id, _enemy_comp);
			}
		} else {
			std::vector<int64_t> indices;
			indices.reserve(static_cast<size_t>(champion_count));
			for (int64_t i = 0; i < champion_count; ++i) {
				indices.push_back(i);
			}
			for (int64_t i = champion_count - 1; i > 0; --i) {
				const int64_t j = pick_index(draft_rng, i);
				std::swap(indices[static_cast<size_t>(i)], indices[static_cast<size_t>(j)]);
			}
			for (int64_t slot = 0; slot < units_per_team; ++slot) {
				append_generated_unit(spawn_spec, sn_player(), archetypes[static_cast<size_t>(indices[static_cast<size_t>(slot)])], next_instance_id, _player_comp);
			}
			for (int64_t slot = 0; slot < units_per_team; ++slot) {
				const int64_t source_index = slot + units_per_team;
				append_generated_unit(spawn_spec, sn_enemy(), archetypes[static_cast<size_t>(indices[static_cast<size_t>(source_index)])], next_instance_id, _enemy_comp);
			}
		}
		_build_role_strategy_cache();
		_prepare_tick_context();
		_simulate();

		// Aggregate minion stats to summoners
		std::unordered_map<int64_t, double> summoner_minion_damage_dealt;
		std::unordered_map<int64_t, double> summoner_minion_damage_received;
		for (const UnitState &unit : _units) {
			if (unit.summoner_instance_id != 0) {
				const UnitStateCold &c = _uc(unit);
				summoner_minion_damage_dealt[unit.summoner_instance_id] += c.damage_dealt;
				summoner_minion_damage_received[unit.summoner_instance_id] += c.damage_received;
			}
		}

		// Define lambdas that capture the summoner maps (must be after maps are created)
		auto accumulate_common_with_minions = [&summoner_minion_damage_dealt, &summoner_minion_damage_received](Dictionary &entry, const UnitStateCold &c, int64_t instance_id) {
			entry["dmg_d"] = double(entry["dmg_d"]) + c.damage_dealt;
			entry["dmg_r"] = double(entry["dmg_r"]) + c.damage_received;
			entry["dmg_m"] = double(entry["dmg_m"]) + c.damage_mitigated;
			entry["heal"] = double(entry["heal"]) + c.healing_done;
			entry["heal_auto"] = double(entry["heal_auto"]) + c.healing_done_auto;
			entry["heal_ability"] = double(entry["heal_ability"]) + c.healing_done_ability;
			entry["heal_ultimate"] = double(entry["heal_ultimate"]) + c.healing_done_ultimate;
			entry["heal_passive"] = double(entry["heal_passive"]) + c.healing_done_passive;
			entry["shield"] = double(entry["shield"]) + c.shielding_done;
			entry["shield_auto"] = double(entry["shield_auto"]) + c.shielding_done_auto;
			entry["shield_ability"] = double(entry["shield_ability"]) + c.shielding_done_ability;
			entry["shield_ultimate"] = double(entry["shield_ultimate"]) + c.shielding_done_ultimate;
			entry["shield_passive"] = double(entry["shield_passive"]) + c.shielding_done_passive;
			entry["stuns"] = int64_t(entry["stuns"]) + c.stuns;
			entry["d_auto"] = double(entry["d_auto"]) + c.damage_dealt_auto;
			entry["d_ab"] = double(entry["d_ab"]) + c.damage_dealt_ability;
			entry["d_ult"] = double(entry["d_ult"]) + c.damage_dealt_ultimate;
			entry["d_passive"] = double(entry["d_passive"]) + c.damage_dealt_passive;
			// Add minion damage for this summoner
			auto it_dealt = summoner_minion_damage_dealt.find(instance_id);
			auto it_received = summoner_minion_damage_received.find(instance_id);
			entry["minion_dmg_d"] = double(entry["minion_dmg_d"]) + (it_dealt != summoner_minion_damage_dealt.end() ? it_dealt->second : 0.0);
			entry["minion_dmg_r"] = double(entry["minion_dmg_r"]) + (it_received != summoner_minion_damage_received.end() ? it_received->second : 0.0);
		};
		auto add_record_with_minions = [&accumulate_common_with_minions](Dictionary &entry, const UnitStateCold &c, int64_t instance_id, bool won, bool draw, bool include_kda) {
			if (draw) {
				entry["d"] = int64_t(entry["d"]) + 1;
			} else if (won) {
				entry["w"] = int64_t(entry["w"]) + 1;
			} else {
				entry["l"] = int64_t(entry["l"]) + 1;
			}
			accumulate_common_with_minions(entry, c, instance_id);
			if (include_kda) {
				entry["kills"] = int64_t(entry["kills"]) + c.kills;
				entry["deaths"] = int64_t(entry["deaths"]) + c.deaths;
				entry["assists"] = int64_t(entry["assists"]) + c.assists;
			}
		};

		const bool player_won = _winner_team == sn_player();
		const bool enemy_won = _winner_team == sn_enemy();
		const bool draw = !player_won && !enemy_won;
		if (player_won) {
			bucket["p1"] = int64_t(bucket["p1"]) + 1;
		} else if (enemy_won) {
			bucket["p2"] = int64_t(bucket["p2"]) + 1;
		} else {
			bucket["draws"] = int64_t(bucket["draws"]) + 1;
		}
		bucket["total"] = int64_t(bucket["total"]) + 1;
		if (include_match_log) {
			Dictionary row;
			row["team_size"] = team_size;
			row["seed"] = _seed;
			row["winner"] = String(_winner_team);
			row["sudden_death_ticks"] = int64_t(_sudden_death_ticks);
			row["duration"] = _time;
			match_logs.append(row);
		}

		Dictionary heroes = Dictionary(bucket["heroes"]);
		Dictionary roles = Dictionary(bucket["roles"]);
		for (const UnitState &unit : _units) {
			const UnitStateCold &c = _uc(unit);
			// Skip minions (spawned during combat, not drafted champions)
			if (c.role_id == StringName("minion")) {
				continue;
			}
			const String hero = String(c.archetype_id);
			Dictionary hero_entry = Dictionary(heroes.get(hero, Dictionary()));
			if (hero_entry.is_empty()) {
				hero_entry = make_stat_entry();
			}
			const bool unit_won = _winner_team != StringName() && unit.team == _winner_team;
			add_record_with_minions(hero_entry, c, unit.instance_id, unit_won, draw, true);
			heroes[hero] = hero_entry;

			const String role = String(Dictionary(c.stats).get("role", String("unknown")));
			Dictionary role_entry = Dictionary(roles.get(role, Dictionary()));
			if (role_entry.is_empty()) {
				role_entry = make_role_entry();
			}
			add_record_with_minions(role_entry, c, unit.instance_id, unit_won, draw, false);
			roles[role] = role_entry;
		}
		bucket["heroes"] = heroes;
		bucket["roles"] = roles;

		if (team_size > 1) {
			Dictionary combos = Dictionary(bucket["combos"]);
			const String player_label = sorted_combo_label(_player_comp);
			Dictionary player_combo = Dictionary(combos.get(player_label, Dictionary()));
			if (player_combo.is_empty()) {
				player_combo["w"] = int64_t(0);
				player_combo["n"] = int64_t(0);
			}
			player_combo["n"] = int64_t(player_combo["n"]) + 1;
			if (player_won) {
				player_combo["w"] = int64_t(player_combo["w"]) + 1;
			}
			combos[player_label] = player_combo;

			const String enemy_label = sorted_combo_label(_enemy_comp);
			Dictionary enemy_combo = Dictionary(combos.get(enemy_label, Dictionary()));
			if (enemy_combo.is_empty()) {
				enemy_combo["w"] = int64_t(0);
				enemy_combo["n"] = int64_t(0);
			}
			enemy_combo["n"] = int64_t(enemy_combo["n"]) + 1;
			if (enemy_won) {
				enemy_combo["w"] = int64_t(enemy_combo["w"]) + 1;
			}
			combos[enemy_label] = enemy_combo;
			bucket["combos"] = combos;
		}

		if (player_won) {
			record_matchup_result(matchup_data, _player_comp, _enemy_comp);
		} else if (enemy_won) {
			record_matchup_result(matchup_data, _enemy_comp, _player_comp);
		}

		_reset_runtime_state();
		next_progress += 1;
		if (next_progress == 1000) {
			benchmark_progress_add(1000);
			next_progress = 0;
		}
	}
	if (next_progress != 0) {
		benchmark_progress_add(next_progress);
	}

	Dictionary by_size;
	by_size[team_size] = bucket;
	Dictionary stats_partial;
	stats_partial["by_size"] = by_size;
	stats_partial["match_logs"] = include_match_log ? match_logs : Array();
	result["stats_partial"] = stats_partial;
	result["matchup_data"] = matchup_data;
	return result;
}

void TeamfightSimulationCore::begin_match(const Variant &match_input) {
	_ensure_catalog_loaded();
	Dictionary input = _coerce_match_input(match_input);
	if (input.is_empty()) {
		UtilityFunctions::push_error("TeamfightSimulationCore.begin_match() expected MatchReplayInput or Dictionary.");
		return;
	}
	_reset_runtime_state();
	_populate_runtime_state(input);
}

void TeamfightSimulationCore::advance_one_tick() {
	if (match_ticks_exhausted()) {
		return;
	}
	_step_tick(false);
	
	// Track sudden death ticks after regulation
	double effective_tick_rate = Math::max(_tick_rate, EPSILON);
	int64_t max_ticks = int64_t(Math::ceil(MATCH_DURATION / effective_tick_rate));
	if (_tick >= max_ticks) {
		// Log only when safety limit is reached (indicating a draw)
		if (_sudden_death_ticks == SUDDEN_DEATH_MAX_TICKS) {
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
		_sudden_death_ticks++;
	}
}

bool TeamfightSimulationCore::match_ticks_exhausted() const {
	double effective_tick_rate = Math::max(_tick_rate, EPSILON);
	int64_t max_ticks = int64_t(Math::ceil(MATCH_DURATION / effective_tick_rate));
	// Allow sudden death: continue ticking if kills are tied
	if (_tick >= max_ticks) {
		// After regulation, only stop if kills are unequal or sudden death limit reached
		if (_player_kills != _enemy_kills) {
			return true;
		}
		// Use same sudden death limit as main simulation: SUDDEN_DEATH_MAX_TICKS ticks
		if (_sudden_death_ticks >= SUDDEN_DEATH_MAX_TICKS) {
			return true;
		}
		return false;
	}
	return false;
}

Dictionary TeamfightSimulationCore::finish_and_summarize() {
	_winner_team = _determine_winner();
	return _build_summary();
}

void TeamfightSimulationCore::_viewer_fx_push(const ViewerFxEvent &p_ev) {
	if (_viewer_fx_events.size() >= VIEWER_FX_CAP) {
		return;
	}
	_viewer_fx_events.push_back(p_ev);
}

void TeamfightSimulationCore::_viewer_record_damage_fx(const UnitState &p_source, const UnitState &p_target, double p_total_damage, const StringName &p_action_kind, const StringName &p_damage_type) {
	ViewerFxEvent ev;
	ev.kind = StringName("damage");
	ev.target_id = p_target.instance_id;
	ev.src_id = p_source.instance_id;
	ev.pos_x = p_target.pos_x;
	ev.pos_y = p_target.pos_y;
	ev.val = p_total_damage;
	ev.damage_type = p_damage_type;
	_viewer_fx_push(ev);
	if (p_action_kind == StringName("auto") && get_effective_attack_range(p_source) <= RANGED_THRESHOLD) {
		ViewerFxEvent slash;
		slash.kind = StringName("melee_slash");
		slash.pos_x = p_target.pos_x;
		slash.pos_y = p_target.pos_y;
		_viewer_fx_push(slash);
	}
}

void TeamfightSimulationCore::_viewer_record_heal_fx(const UnitState &p_target, double p_amount) {
	ViewerFxEvent ev;
	ev.kind = StringName("heal");
	ev.target_id = p_target.instance_id;
	ev.pos_x = p_target.pos_x;
	ev.pos_y = p_target.pos_y;
	ev.val = p_amount;
	_viewer_fx_push(ev);
}

void TeamfightSimulationCore::_viewer_record_shield_fx(const UnitState &p_target, double p_amount) {
	ViewerFxEvent ev;
	ev.kind = StringName("shield");
	ev.target_id = p_target.instance_id;
	ev.pos_x = p_target.pos_x;
	ev.pos_y = p_target.pos_y;
	ev.val = p_amount;
	_viewer_fx_push(ev);
}

void TeamfightSimulationCore::_viewer_record_aoe_ring_fx(const UnitState &p_source, const UnitState &p_center, double p_radius, const StringName &p_kind) {
	if (p_radius <= 0.0) {
		return;
	}
	ViewerFxEvent ev;
	ev.kind = p_kind;
	ev.src_id = p_source.instance_id;
	ev.target_id = p_center.instance_id;
	ev.pos_x = p_center.pos_x;
	ev.pos_y = p_center.pos_y;
	ev.val = 0.0;
	ev.radius = p_radius;
	_viewer_fx_push(ev);
}

void TeamfightSimulationCore::_viewer_record_aoe_shape_fx(const UnitState &p_source, const UnitState *p_target, const AoShapeParams &params, const StringName &kind) {
	ViewerFxEvent ev;
	ev.kind = kind;
	ev.src_id = p_source.instance_id;
	ev.target_id = p_target != nullptr ? p_target->instance_id : params.target_id;
	
	// Resolve anchor position
	if (params.anchor == AoAnchorKind::Self) {
		ev.pos_x = p_source.pos_x;
		ev.pos_y = p_source.pos_y;
	} else if (params.anchor == AoAnchorKind::Target) {
		const UnitState *target = p_target != nullptr ? p_target : _unit_by_id(params.target_id);
		if (target != nullptr) {
			ev.pos_x = target->pos_x;
			ev.pos_y = target->pos_y;
		} else {
			ev.pos_x = p_source.pos_x;
			ev.pos_y = p_source.pos_y;
		}
	} else if (params.anchor == AoAnchorKind::Forward) {
		// Calculate forward position from champion center for visualization
		Vector2 direction = _resolve_aoe_direction(p_source, params, p_target);
		if (params.shape == AoShapeKind::Rectangle) {
			ev.pos_x = p_source.pos_x + direction.x * params.height * 0.5;
			ev.pos_y = p_source.pos_y + direction.y * params.height * 0.5;
		} else if (params.shape == AoShapeKind::Cone) {
			// Cone apex at source, no offset needed (cone extends forward from apex)
			ev.pos_x = p_source.pos_x;
			ev.pos_y = p_source.pos_y;
		} else if (params.shape == AoShapeKind::Circle) {
			ev.pos_x = p_source.pos_x + direction.x * params.radius * 0.5;
			ev.pos_y = p_source.pos_y + direction.y * params.radius * 0.5;
		} else {
			ev.pos_x = p_source.pos_x;
			ev.pos_y = p_source.pos_y;
		}
	} else {
		ev.pos_x = params.anchor_x;
		ev.pos_y = params.anchor_y;
	}
	
	// Serialize shape parameters to dictionary
	Dictionary shape_dict;
	switch (params.shape) {
		case AoShapeKind::Circle:
			shape_dict["shape"] = "circle";
			break;
		case AoShapeKind::Cone:
			shape_dict["shape"] = "cone";
			break;
		case AoShapeKind::Rectangle:
			shape_dict["shape"] = "rectangle";
			break;
	}
	
	switch (params.anchor) {
		case AoAnchorKind::Self:
			shape_dict["anchor"] = "self";
			break;
		case AoAnchorKind::Target:
			shape_dict["anchor"] = "target";
			break;
		case AoAnchorKind::Point:
			shape_dict["anchor"] = "point";
			break;
		case AoAnchorKind::Forward:
			shape_dict["anchor"] = "forward";
			break;
	}
	
	shape_dict["radius"] = params.radius;
	shape_dict["width"] = params.width;
	shape_dict["height"] = params.height;
	shape_dict["rotation_radians"] = params.rotation_radians;
	shape_dict["anchor_x"] = params.anchor_x;
	shape_dict["anchor_y"] = params.anchor_y;
	shape_dict["target_id"] = ev.target_id;
	Vector2 forward = _resolve_aoe_direction(p_source, params, p_target);
	shape_dict["forward_x"] = forward.x;
	shape_dict["forward_y"] = forward.y;
	
	ev.val = 0.0;
	ev.radius = params.radius;
	ev.extra = shape_dict;
	_viewer_fx_push(ev);
}

void TeamfightSimulationCore::_viewer_record_hot_status_fx(const UnitState &p_target, double p_duration, const StringName &p_effect_type) {
	ViewerFxEvent ev;
	ev.kind = StringName("hot_status");
	ev.target_id = p_target.instance_id;
	ev.val = p_duration;
	ev.radius = 0.0;
	_viewer_fx_push(ev);
}

void TeamfightSimulationCore::_viewer_record_passive_aoe_fx(const UnitState &p_unit, double p_radius, const StringName &p_passive_id) {
	ViewerFxEvent ev;
	ev.kind = StringName("passive_aoe");
	ev.target_id = p_unit.instance_id;
	ev.src_id = p_unit.instance_id;
	ev.pos_x = p_unit.pos_x;
	ev.pos_y = p_unit.pos_y;
	ev.val = 0.0;
	ev.radius = p_radius;
	Dictionary extra;
	extra["passive_id"] = String(p_passive_id);
	ev.extra = extra;
	_viewer_fx_push(ev);
}

String TeamfightSimulationCore::_viewer_state_string(const UnitState &p_u) const {
	if (!p_u.alive) {
		return String("DEAD");
	}
	if (p_u.stun_remaining > 0.0) {
		return String("STUNNED");
	}
	if (p_u.root_remaining > 0.0) {
		return String("ROOTED");
	}
	if (p_u.silence_remaining > 0.0 && (p_u.silence_blocks_abilities || p_u.silence_blocks_ultimates)) {
		return String("SILENCED");
	}
	if (!_uc(p_u).reflect_buffs.empty()) {
		return String("REFLECTING");
	}
	if (p_u.disarm_remaining > 0.0) {
		return String("DISARMED");
	}
	if (p_u.slow_remaining > 0.0) {
		return String("SLOWED");
	}
	if (_uc(p_u).is_channeling) {
		return String("CHANNELING");
	}
	if (p_u.casting_remaining > 0.0) {
		return String("CASTING");
	}
	if (p_u.last_kite_timer > 0.0) {
		return String("KITING");
	}
	return String("ALIVE");
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
	Dictionary root;
	root["tick"] = _tick;
	root["time"] = _time;
	root["match_duration"] = MATCH_DURATION;
	root["time_remaining"] = Math::max(0.0, MATCH_DURATION - _time);
	root["player_kills"] = _player_kills;
	root["enemy_kills"] = _enemy_kills;
	root["live_winner"] = String(_determine_winner());
	Array units;
	for (const UnitState &u : _units) {
		const UnitStateCold &uc = _uc(u);
		Dictionary d;
		d["id"] = u.instance_id;
		d["instance_id"] = u.instance_id;
		d["x"] = u.pos_x;
		d["y"] = u.pos_y;
		d["pos_x"] = u.pos_x;
		d["pos_y"] = u.pos_y;
		d["team"] = String(u.team);
		d["archetype_id"] = String(uc.archetype_id);
		d["hp"] = u.hp;
		d["max_hp"] = get_effective_max_hp(u);
		d["shield"] = u.shield;
		d["mana"] = u.mana;
		d["max_mana"] = get_effective_max_mana(u);
		d["target"] = u.target_id;
		d["target_id"] = u.target_id;
		d["stun"] = u.stun_remaining;
		d["stun_remaining"] = u.stun_remaining;
		d["slow_remaining"] = u.slow_remaining;
		d["root_remaining"] = u.root_remaining;
		d["silence_remaining"] = u.silence_remaining;
		d["disarm_remaining"] = u.disarm_remaining;
		d["stealth_remaining"] = u.stealth_remaining;
		d["reflect_buff_remaining"] = _uc(u).reflect_buffs.empty() ? 0.0 : 1.0;
		d["alive"] = u.alive;
		d["state"] = _viewer_state_string(u);
		d["acd"] = u.attack_cooldown;
		d["abi"] = u.ability_cooldown;
		d["attack_cooldown"] = u.attack_cooldown;
		d["attack_period"] = u.attack_period;
		d["attack_range"] = get_effective_attack_range(u);
		d["attack_speed"] = get_effective_attack_speed(u);
		d["ability_cd_max"] = get_effective_ability_cd(u);
		d["attack_damage"] = get_effective_attack_damage(u);
		d["move_speed"] = get_effective_move_speed(u);
		d["armor"] = get_effective_armor(u);
		d["magic_resist"] = get_effective_magic_resist(u);
		d["tenacity"] = get_effective_tenacity(u);
		d["life_steal"] = get_effective_life_steal(u);
		d["casting_remaining"] = u.casting_remaining;
		d["casting_kind"] = String(uc.casting_kind);
		d["is_channeling"] = _uc(u).is_channeling;
		d["channel_remaining_duration"] = _uc(u).channel_remaining_duration;
		d["channel_action_kind"] = String(_uc(u).channel_action_kind);
		
		// Calculate distance to target and in-range status
	bool in_range = false;
	if (u.target_id > 0) {
		const UnitState *target = _unit_by_id(u.target_id);
		if (target != nullptr && target->alive) {
			double distance = sim::distance_between_coords(u.pos_x, u.pos_y, target->pos_x, target->pos_y);
			d["target_distance"] = distance;
			// Use effective attack range for in_range determination (allows melee to attack while closing gap)
			double effective_range = _effective_attack_range(u);
			in_range = distance <= effective_range;
		}
	} else {
		d["target_distance"] = -1.0;  // No target
	}
	d["in_range"] = in_range;
		d["kills"] = uc.kills;
		d["deaths"] = uc.deaths;
		d["assists"] = uc.assists;
		d["respawn_timer"] = u.respawn_timer;
		d["taunt_remaining"] = u.taunt_remaining;
		d["damage_dealt"] = uc.damage_dealt;
		d["healing_done"] = uc.healing_done;
		d["damage_mitigated"] = uc.damage_mitigated;
		d["role"] = String(uc.role_id);
		units.append(d);
	}
	root["units"] = units;
	Array projs;
	for (int64_t i = 0; i < int64_t(_projectiles.size()); ++i) {
		const ProjectileState &p = _projectiles[static_cast<size_t>(i)];
		Dictionary pd;
		pd["id"] = i;
		pd["pos_x"] = p.pos_x;
		pd["pos_y"] = p.pos_y;
		pd["radius"] = p.radius;
		pd["source_id"] = p.source_id;
		pd["target_id"] = p.target_id;
		const UnitState *src = _unit_by_id(p.source_id);
		if (src != nullptr) {
			pd["team"] = String(src->team);
		} else {
			pd["team"] = String("player");
		}
		projs.append(pd);
	}
	root["projectiles"] = projs;
	Array fx;
	for (const ViewerFxEvent &ve : _viewer_fx_events) {
		Dictionary e;
		e["kind"] = String(ve.kind);
		e["target_id"] = ve.target_id;
		e["src_id"] = ve.src_id;
		e["x"] = ve.pos_x;
		e["y"] = ve.pos_y;
		e["val"] = ve.val;
		if (ve.radius > 0.0) {
			const double r = ve.radius;
			e["r"] = r;
			e["radius"] = r;
		}
		if (!ve.damage_type.is_empty()) {
			e["damage_type"] = String(ve.damage_type);
		}
		if (ve.extra.get_type() != Variant::NIL) {
			e["extra"] = ve.extra;
		}
		fx.append(e);
	}
	root["tick_fx"] = fx;
	return root;
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

Array TeamfightSimulationCore::run_matches(const Array &match_inputs) {
	Array summaries;
	summaries.resize(match_inputs.size());
	for (int64_t index = 0; index < match_inputs.size(); ++index) {
		summaries[index] = run_match(match_inputs[index]);
		clear();
	}
	return summaries;
}

Array TeamfightSimulationCore::run_matches_stats(const Array &match_inputs) {
	_ensure_catalog_loaded();
	Array summaries;
	const int64_t n = match_inputs.size();
	summaries.resize(n);
	for (int64_t index = 0; index < n; ++index) {
		Dictionary input = _coerce_match_input(match_inputs[index]);
		if (input.is_empty()) {
			UtilityFunctions::push_error(
					vformat("TeamfightSimulationCore.run_matches_stats(): bad match_input at index %d.", index));
			summaries[index] = Dictionary();
			clear();
			continue;
		}
		_reset_runtime_state();
		_populate_runtime_state(input);
		_simulate();
		summaries[index] = _build_stats_summary();
		clear();
	}
	return summaries;
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

String TeamfightSimulationCore::_join_team_names(const Array &team) const {
	String result = "";
	for (int64_t i = 0; i < team.size(); ++i) {
		if (i > 0) {
			result += ", ";
		}
		result += String(team[i]);
	}
	return result;
}

void TeamfightSimulationCore::_process_pending_spawns() {
	if (_pending_spawns.empty()) {
		return;
	}
	
	// Use tracked max instance ID for efficient ID generation
	int64_t next_instance_id = _max_instance_id + 1;
	
	for (const PendingSpawn &pending : _pending_spawns) {
		// Build unit state
		std::pair<UnitState, UnitStateCold> built = _build_unit_state(pending.spawn_spec, pending.team, next_instance_id);
		if (built.first.instance_id == 0) {
			UtilityFunctions::push_error(vformat("Pending spawn failed: _build_unit_state returned instance_id=0"));
			continue;
		}
		
		// Add to units array
		int64_t unit_index = int64_t(_units.size());
		_units.push_back(std::move(built.first));
		_unit_cold.push_back(std::move(built.second));
		_unit_index_map[next_instance_id] = unit_index;
		
		// Set summoner relationship for minions
		_units[unit_index].summoner_instance_id = pending.summoner_instance_id;
		
		// Add to targeting frame so minions are immediately targetable
		// NOTE: Must use push_back instead of _sync_targeting_frame_unit because
		// minions are spawned mid-game and don't have existing frame entries.
		// The targeting frame arrays (_units, _unit_cold, _targeting_frame) grow
		// monotonically - units are never removed on death to maintain index stability.
		// This means dead units (including minions) accumulate in the arrays over time,
		// but memory overhead is negligible for typical match sizes.
		_targeting_frame.push_back(_make_targeting_frame_entry(_units[unit_index]));
		
		// Add to alive indices
		_add_alive_index(pending.team, unit_index);
		
		// Update max instance ID
		if (next_instance_id > _max_instance_id) {
			_max_instance_id = next_instance_id;
		}
		
		next_instance_id++;
	}
	
	_pending_spawns.clear();
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



