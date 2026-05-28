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

void sim_host_handle_death(void *user_data, UnitState &killer, UnitState &target) {
	auto *core = static_cast<TeamfightSimulationCore *>(user_data);
	sim::SimWorld w = core->_sim_world();
	sim::match_roster::MatchRosterState roster = core->_match_roster_state();
	sim::match::MatchScoreState score = core->_match_score_state();
	score.roster = &roster;
	sim::match::handle_death(
			w,
			core->_sim_host_callbacks,
			&core->_viewer_hooks,
			score,
			core->_spawn_slot_state(),
			killer,
			target);
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

void sim_host_viewer_record_heal_fx(void *user_data, const UnitState &target, double amount) {
	static_cast<TeamfightSimulationCore *>(user_data)->_viewer_record_heal_fx(target, amount);
}

void sim_host_viewer_record_shield_fx(void *user_data, const UnitState &target, double amount) {
	static_cast<TeamfightSimulationCore *>(user_data)->_viewer_record_shield_fx(target, amount);
}

void sim_host_viewer_record_aoe_shape_fx(
		void *user_data,
		const UnitState &source,
		const UnitState *target,
		const AoShapeParams &params,
		const StringName &kind) {
	static_cast<TeamfightSimulationCore *>(user_data)->_viewer_record_aoe_shape_fx(source, target, params, kind);
}

void sim_host_viewer_record_passive_aoe_fx(
		void *user_data,
		const UnitState &unit,
		double radius,
		const StringName &passive_id) {
	static_cast<TeamfightSimulationCore *>(user_data)->_viewer_record_passive_aoe_fx(unit, radius, passive_id);
}

Dictionary sim_host_effective_champion_for(void *user_data, const StringName &archetype_id) {
	return static_cast<TeamfightSimulationCore *>(user_data)->_effective_champion_for(archetype_id);
}

EffectRecord sim_host_compile_effect(void *user_data, const Dictionary &effect) {
	return static_cast<TeamfightSimulationCore *>(user_data)->_compile_effect(effect);
}

void sim_host_finalize_reflect_passives(void *user_data, UnitState &unit, UnitStateCold &cold) {
	static_cast<TeamfightSimulationCore *>(user_data)->_finalize_reflect_passives(unit, cold);
}

int64_t sim_host_assign_spawn_slot(void *user_data, const StringName &team) {
	auto *core = static_cast<TeamfightSimulationCore *>(user_data);
	return sim::match::assign_spawn_slot(core->_spawn_slot_state(), team);
}

Vector2 sim_host_get_random_spawn_position(void *user_data, const StringName &team, bool is_respawn) {
	return static_cast<TeamfightSimulationCore *>(user_data)->_get_random_spawn_position(team, is_respawn);
}

Dictionary sim_host_coerce_spawn_spec(void *user_data, const Variant &value) {
	return static_cast<TeamfightSimulationCore *>(user_data)->_coerce_match_input(value);
}

uint32_t sim_host_rand_uint32(void *user_data) {
	return static_cast<TeamfightSimulationCore *>(user_data)->_rng.genrand_uint32();
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

UnitState *sim_host_select_ally_target(void *user_data, UnitState &unit) {
	return static_cast<TeamfightSimulationCore *>(user_data)->_select_ally_target(unit);
}

void sim_host_match_profile_emit_json_stderr(void *user_data);
bool sim_host_match_profile_env_enabled(void *user_data);
bool sim_host_match_targeting_profile_enabled(void *user_data);
void sim_host_match_log_sudden_death_draw(void *user_data);

void sim_host_match_update_projectiles(void *user_data, bool /*profile_sim*/) {
	static_cast<TeamfightSimulationCore *>(user_data)->_update_projectiles();
}

void sim_host_match_prepare_tick_context(void *user_data, bool /*profile_sim*/) {
	static_cast<TeamfightSimulationCore *>(user_data)->_prepare_tick_context();
}

void sim_host_match_update_unit(void *user_data, UnitState &unit, bool profile_sim) {
	static_cast<TeamfightSimulationCore *>(user_data)->_update_unit(unit, profile_sim);
}

sim::match_roster::MatchRosterState sim_host_match_roster_state(void *user_data) {
	return static_cast<TeamfightSimulationCore *>(user_data)->_match_roster_state();
}

sim::unit_builder::UnitBuilderHost sim_host_match_unit_builder_host(void *user_data) {
	return static_cast<TeamfightSimulationCore *>(user_data)->_unit_builder_host();
}

void sim_host_match_profile_reset(void *user_data) {
	static_cast<TeamfightSimulationCore *>(user_data)->_sim_profile_reset();
}

void sim_host_match_profile_emit_json_stderr(void *user_data) {
	static_cast<TeamfightSimulationCore *>(user_data)->_sim_profile_emit_json_stderr();
}

bool sim_host_match_profile_env_enabled(void *user_data) {
	return TeamfightSimulationCore::_sim_profile_env_enabled();
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

sim::unit_builder::UnitBuilderHost TeamfightSimulationCore::_unit_builder_host() const {
	sim::unit_builder::UnitBuilderHost host;
	host.user_data = const_cast<TeamfightSimulationCore *>(this);
	host.catalog = &_catalog;
	host.compile_effect = &sim_host_compile_effect;
	host.effective_champion_for = &sim_host_effective_champion_for;
	host.finalize_reflect_passives = &sim_host_finalize_reflect_passives;
	host.assign_spawn_slot = &sim_host_assign_spawn_slot;
	host.get_random_spawn_position = &sim_host_get_random_spawn_position;
	return host;
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
		_viewer_fx_events,
	};
	state.profile.ns_projectiles = &_sim_profile_ns_projectiles;
	state.profile.ns_prepare_tick_ctx = &_sim_profile_ns_prepare_tick_ctx;
	state.profile.ns_update_units = &_sim_profile_ns_update_units;
	state.profile.tick_count = &_sim_profile_tick_count;
	state.profile_active = &_sim_profile_active;
	state.profile_targeting_active = &_sim_profile_targeting_active;
	return state;
}

sim::match::MatchLoopHost TeamfightSimulationCore::_match_loop_host() const {
	sim::match::MatchLoopHost host;
	host.user_data = const_cast<TeamfightSimulationCore *>(this);
	host.update_projectiles = &sim_host_match_update_projectiles;
	host.prepare_tick_context = &sim_host_match_prepare_tick_context;
	host.update_unit = &sim_host_match_update_unit;
	host.match_roster_state = &sim_host_match_roster_state;
	host.unit_builder_host = &sim_host_match_unit_builder_host;
	host.profile_reset = &sim_host_match_profile_reset;
	host.profile_emit_json_stderr = &sim_host_match_profile_emit_json_stderr;
	host.profile_env_enabled = &sim_host_match_profile_env_enabled;
	host.targeting_profile_enabled = &sim_host_match_targeting_profile_enabled;
	host.log_sudden_death_draw = &sim_host_match_log_sudden_death_draw;
	return host;
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

void sim_host_unit_tick_respawn(void *user_data, UnitState &unit) {
	auto *core = static_cast<TeamfightSimulationCore *>(user_data);
	sim::SimWorld w = core->_sim_world();
	sim::match_roster::MatchRosterState roster = core->_match_roster_state();
	sim::match::MatchScoreState score = core->_match_score_state();
	score.roster = &roster;
	sim::match::respawn_unit(
			w,
			core->_sim_host_callbacks,
			&core->_viewer_hooks,
			score,
			core->_spawn_slot_state(),
			unit);
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

bool sim_host_debug_combat_trace(void *user_data) {
	return static_cast<TeamfightSimulationCore *>(user_data)->_debug_combat_trace;
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
	sim::SimWorld w = _sim_world();
	sim::match_roster::append_team_units(
			w,
			&_viewer_hooks,
			_match_roster_state(),
			_unit_builder_host(),
			Array(match_input.get("player_units", Array())),
			StringName("player"),
			next_instance_id,
			_player_comp,
			&sim_host_coerce_spawn_spec,
			this);
	sim::match_roster::append_team_units(
			w,
			&_viewer_hooks,
			_match_roster_state(),
			_unit_builder_host(),
			Array(match_input.get("enemy_units", Array())),
			StringName("enemy"),
			next_instance_id,
			_enemy_comp,
			&sim_host_coerce_spawn_spec,
			this);
	_build_role_strategy_cache();
	_prepare_tick_context();
}

Vector2 TeamfightSimulationCore::_resolve_aoe_direction(const UnitState &source, const AoShapeParams &params, const UnitState *target_override) const {
	sim::SimWorld w = _sim_world();
	return sim::status::resolve_aoe_direction(w, source, params, target_override);
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

StringName TeamfightSimulationCore::_determine_winner() const {
	return sim::match::determine_winner(_player_kills, _enemy_kills);
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

bool sim_host_match_targeting_profile_enabled(void * /*user_data*/) {
	return targeting_profile_enabled();
}

void sim_host_match_log_sudden_death_draw(void *user_data) {
	static_cast<TeamfightSimulationCore *>(user_data)->_log_sudden_death_draw();
}

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

Dictionary TeamfightSimulationCore::run_match(const Variant &match_input) {
	_ensure_catalog_loaded();
	Dictionary input = _coerce_match_input(match_input);
	if (input.is_empty()) {
		UtilityFunctions::push_error("TeamfightSimulationCore.run_match() expected MatchReplayInput or Dictionary.");
		return Dictionary();
	}
	_reset_runtime_state();
	_populate_runtime_state(input);
	sim::match::simulate(_match_loop_state(), _match_loop_host());
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
	sim::match::simulate(_match_loop_state(), _match_loop_host());
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
	sim::match::simulate(_match_loop_state(), _match_loop_host());
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
			sim::SimWorld w = _sim_world();
			std::pair<UnitState, UnitStateCold> built =
					sim::unit_builder::build_unit(_unit_builder_host(), spawn_spec, sn_player(), next_instance_id);
			const int64_t unit_index = sim::match_roster::register_built_unit(
					w, _match_roster_state(), std::move(built), sn_player(), next_instance_id);
			if (unit_index < 0) {
				continue;
			}
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
			sim::SimWorld w = _sim_world();
			std::pair<UnitState, UnitStateCold> built =
					sim::unit_builder::build_unit(_unit_builder_host(), spawn_spec, sn_enemy(), next_instance_id);
			const int64_t unit_index = sim::match_roster::register_built_unit(
					w, _match_roster_state(), std::move(built), sn_enemy(), next_instance_id);
			if (unit_index < 0) {
				continue;
			}
			_enemy_comp.append(_unit_cold[static_cast<size_t>(unit_index)].archetype_id);
			next_instance_id += 1;
		}
		_build_role_strategy_cache();
		_prepare_tick_context();
		if (bench_phases) {
			ns_match_setup_total += sim_profile_elapsed_ns(t_match0);
		}
		auto t_sim0 = std::chrono::steady_clock::now();
		sim::match::simulate(_match_loop_state(), _match_loop_host());
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
		sim::SimWorld w = _sim_world();
		std::pair<UnitState, UnitStateCold> built = sim::unit_builder::build_unit(_unit_builder_host(), spawn_spec, team, next_instance_id);
		const int64_t unit_index = sim::match_roster::register_built_unit(w, _match_roster_state(), std::move(built), team, next_instance_id);
		if (unit_index < 0) {
			return;
		}
		comp.append(_unit_cold[static_cast<size_t>(unit_index)].archetype_id);
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
		sim::match::simulate(_match_loop_state(), _match_loop_host());

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
	sim::match::step_tick(_match_loop_state(), _match_loop_host(), false);
	
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
		sim::match::simulate(_match_loop_state(), _match_loop_host());
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



