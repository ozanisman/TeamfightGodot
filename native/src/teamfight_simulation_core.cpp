#include "teamfight_simulation_core.hpp"

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

// When TEAMFIGHT_STATS_EXPORT_MINIMAL=1, skip per-unit telemetry dicts in _build_stats_summary (CSV export path).
static bool stats_export_minimal_telemetry_enabled() {
	const char *v = std::getenv("TEAMFIGHT_STATS_EXPORT_MINIMAL");
	return v != nullptr && std::strcmp(v, "1") == 0;
}

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

static int64_t role_slot_for_name(const StringName &role) {
	if (role == sn_tank()) {
		return TeamfightSimulationCore::ROLE_SLOT_TANK;
	}
	if (role == sn_fighter()) {
		return TeamfightSimulationCore::ROLE_SLOT_FIGHTER;
	}
	if (role == sn_assassin()) {
		return TeamfightSimulationCore::ROLE_SLOT_ASSASSIN;
	}
	if (role == sn_marksman()) {
		return TeamfightSimulationCore::ROLE_SLOT_MARKSMAN;
	}
	if (role == sn_mage()) {
		return TeamfightSimulationCore::ROLE_SLOT_MAGE;
	}
	if (role == sn_support()) {
		return TeamfightSimulationCore::ROLE_SLOT_SUPPORT;
	}
	return -1;
}

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

int64_t TeamfightSimulationCore::_opcode_for_kind(const StringName &kind) {
	if (kind == sn_multi_target()) {
		return EFFECT_OPCODE_MULTI_TARGET;
	}
	if (kind == sn_multi_effect()) {
		return EFFECT_OPCODE_MULTI_EFFECT;
	}
	if (kind == sn_damage()) {
		return EFFECT_OPCODE_DAMAGE;
	}
	if (kind == sn_projectile()) {
		return EFFECT_OPCODE_PROJECTILE;
	}
	if (kind == sn_stun()) {
		return EFFECT_OPCODE_STUN;
	}
	if (kind == sn_shield()) {
		return EFFECT_OPCODE_SHIELD;
	}
	if (kind == sn_heal()) {
		return EFFECT_OPCODE_HEAL;
	}
	if (kind == sn_aoe_taunt()) {
		return EFFECT_OPCODE_AOE_TAUNT;
	}
	if (kind == sn_aoe_damage()) {
		return EFFECT_OPCODE_AOE_DAMAGE;
	}
	if (kind == sn_damage_threshold_trigger()) {
		return EFFECT_OPCODE_DAMAGE_THRESHOLD_TRIGGER;
	}
	if (kind == sn_damage_over_time()) {
		return EFFECT_OPCODE_DAMAGE_OVER_TIME;
	}
	if (kind == sn_heal_over_time()) {
		return EFFECT_OPCODE_HEAL_OVER_TIME;
	}
	if (kind == sn_aoe_damage_over_time()) {
		return EFFECT_OPCODE_AOE_DAMAGE_OVER_TIME;
	}
	if (kind == sn_aoe_heal_over_time()) {
		return EFFECT_OPCODE_AOE_HEAL_OVER_TIME;
	}
	if (kind == sn_mana_regen()) {
		return EFFECT_OPCODE_MANA_REGEN;
	}
	if (kind == sn_post_damage_mana_gain()) {
		return EFFECT_OPCODE_POST_DAMAGE_MANA_GAIN;
	}
	if (kind == sn_damage_based_heal()) {
		return EFFECT_OPCODE_DAMAGE_BASED_HEAL;
	}
	if (kind == sn_damage_based_shield()) {
		return EFFECT_OPCODE_DAMAGE_BASED_SHIELD;
	}
	if (kind == sn_consume_stacks_damage()) {
		return EFFECT_OPCODE_CONSUME_STACKS_DAMAGE;
	}
	if (kind == sn_consume_stacks_heal()) {
		return EFFECT_OPCODE_CONSUME_STACKS_HEAL;
	}
	if (kind == sn_consume_stacks_shield()) {
		return EFFECT_OPCODE_CONSUME_STACKS_SHIELD;
	}
	if (kind == sn_set_stacks()) {
		return EFFECT_OPCODE_SET_STACKS;
	}
	if (kind == sn_channel()) {
		return EFFECT_OPCODE_CHANNEL;
	}
	if (kind == sn_mana_restore_on_hit()) {
		return EFFECT_OPCODE_MANA_RESTORE_ON_HIT;
	}
	if (kind == sn_drain_target_mana_on_hit()) {
		return EFFECT_OPCODE_DRAIN_TARGET_MANA_ON_HIT;
	}
	if (kind == sn_every_n_attacks_stun()) {
		return EFFECT_OPCODE_EVERY_N_ATTACKS_STUN;
	}
	if (kind == sn_self_dash()) {
		return EFFECT_OPCODE_SELF_DASH;
	}
	if (kind == sn_auto_dodge()) {
		return EFFECT_OPCODE_AUTO_DODGE;
	}
	if (kind == sn_constant_multiplier()) {
		return EFFECT_OPCODE_CONSTANT_MULTIPLIER;
	}
	if (kind == sn_hp_threshold_damage_multiplier()) {
		return EFFECT_OPCODE_HP_THRESHOLD_DAMAGE_MULTIPLIER;
	}
	if (kind == sn_distance_threshold_multiplier()) {
		return EFFECT_OPCODE_DISTANCE_THRESHOLD_MULTIPLIER;
	}
	// New effect kinds
	if (kind == sn_slow()) {
		return EFFECT_OPCODE_SLOW;
	}
	if (kind == sn_root()) {
		return EFFECT_OPCODE_ROOT;
	}
	if (kind == sn_silence()) {
		return EFFECT_OPCODE_SILENCE;
	}
	if (kind == sn_disarm()) {
		return EFFECT_OPCODE_DISARM;
	}
	if (kind == sn_stealth()) {
		return EFFECT_OPCODE_STEALTH;
	}
	if (kind == sn_knockback()) {
		return EFFECT_OPCODE_KNOCKBACK;
	}
	if (kind == sn_reflect()) {
		return EFFECT_OPCODE_REFLECT;
	}
	if (kind == sn_aoe_slow()) {
		return EFFECT_OPCODE_AOE_SLOW;
	}
	if (kind == sn_aoe_root()) {
		return EFFECT_OPCODE_AOE_ROOT;
	}
	if (kind == sn_aoe_silence()) {
		return EFFECT_OPCODE_AOE_SILENCE;
	}
	if (kind == sn_aoe_disarm()) {
		return EFFECT_OPCODE_AOE_DISARM;
	}
	if (kind == sn_aoe_knockback()) {
		return EFFECT_OPCODE_AOE_KNOCKBACK;
	}
	if (kind == sn_aoe_reflect()) {
		return EFFECT_OPCODE_AOE_REFLECT;
	}
	if (kind == sn_aoe_stun()) {
		return EFFECT_OPCODE_AOE_STUN;
	}
	if (kind == sn_reflect_damage()) {
		return EFFECT_OPCODE_REFLECT_DAMAGE;
	}
	if (kind == sn_redirect_damage()) {
		return EFFECT_OPCODE_REDIRECT_DAMAGE;
	}
	if (kind == sn_summon_ally()) {
		return EFFECT_OPCODE_SUMMON_ALLY;
	}
	if (kind == sn_knockback_shield()) {
		return EFFECT_OPCODE_KNOCKBACK_SHIELD;
	}
	if (kind == sn_target_status_multiplier()) {
		return EFFECT_OPCODE_TARGET_STATUS_MULTIPLIER;
	}
	if (kind == sn_stat_modifier()) {
		return EFFECT_OPCODE_STAT_MODIFIER;
	}
	// DEBUG: Log unrecognized effect kinds
	UtilityFunctions::push_error(vformat("[DEBUG] _opcode_for_kind: unrecognized effect kind '%s', returning EFFECT_OPCODE_UNKNOWN", String(kind)));
	return EFFECT_OPCODE_UNKNOWN;
}

const StringName &TeamfightSimulationCore::_kind_for_opcode(int64_t opcode) {
	static const StringName empty_kind;
	switch (opcode) {
		case EFFECT_OPCODE_MULTI_EFFECT:
			return sn_multi_effect();
		case EFFECT_OPCODE_DAMAGE:
			return sn_damage();
		case EFFECT_OPCODE_PROJECTILE:
			return sn_projectile();
		case EFFECT_OPCODE_STUN:
			return sn_stun();
		case EFFECT_OPCODE_SHIELD:
			return sn_shield();
		case EFFECT_OPCODE_HEAL:
			return sn_heal();
		case EFFECT_OPCODE_AOE_TAUNT:
			return sn_aoe_taunt();
		case EFFECT_OPCODE_AOE_DAMAGE:
			return sn_aoe_damage();
		case EFFECT_OPCODE_DAMAGE_THRESHOLD_TRIGGER:
			return sn_damage_threshold_trigger();
		case EFFECT_OPCODE_DAMAGE_OVER_TIME:
			return sn_damage_over_time();
		case EFFECT_OPCODE_HEAL_OVER_TIME:
			return sn_heal_over_time();
		case EFFECT_OPCODE_AOE_DAMAGE_OVER_TIME:
			return sn_aoe_damage_over_time();
		case EFFECT_OPCODE_AOE_HEAL_OVER_TIME:
			return sn_aoe_heal_over_time();
		case EFFECT_OPCODE_MANA_REGEN:
			return sn_mana_regen();
		case EFFECT_OPCODE_POST_DAMAGE_MANA_GAIN:
			return sn_post_damage_mana_gain();
		case EFFECT_OPCODE_DAMAGE_BASED_HEAL:
			return sn_damage_based_heal();
		case EFFECT_OPCODE_DAMAGE_BASED_SHIELD:
			return sn_damage_based_shield();
		case EFFECT_OPCODE_CONSUME_STACKS_DAMAGE:
			return sn_consume_stacks_damage();
		case EFFECT_OPCODE_CONSUME_STACKS_HEAL:
			return sn_consume_stacks_heal();
		case EFFECT_OPCODE_CONSUME_STACKS_SHIELD:
			return sn_consume_stacks_shield();
		case EFFECT_OPCODE_SET_STACKS:
			return sn_set_stacks();
		case EFFECT_OPCODE_CHANNEL:
			return sn_channel();
		case EFFECT_OPCODE_MANA_RESTORE_ON_HIT:
			return sn_mana_restore_on_hit();
		case EFFECT_OPCODE_DRAIN_TARGET_MANA_ON_HIT:
			return sn_drain_target_mana_on_hit();
		case EFFECT_OPCODE_EVERY_N_ATTACKS_STUN:
			return sn_every_n_attacks_stun();
		case EFFECT_OPCODE_SELF_DASH:
			return sn_self_dash();
		case EFFECT_OPCODE_AUTO_DODGE:
			return sn_auto_dodge();
		case EFFECT_OPCODE_CONSTANT_MULTIPLIER:
			return sn_constant_multiplier();
		case EFFECT_OPCODE_HP_THRESHOLD_DAMAGE_MULTIPLIER:
			return sn_hp_threshold_damage_multiplier();
		case EFFECT_OPCODE_DISTANCE_THRESHOLD_MULTIPLIER:
			return sn_distance_threshold_multiplier();
		// New effect opcodes
		case EFFECT_OPCODE_SLOW:
			return sn_slow();
		case EFFECT_OPCODE_ROOT:
			return sn_root();
		case EFFECT_OPCODE_SILENCE:
			return sn_silence();
		case EFFECT_OPCODE_DISARM:
			return sn_disarm();
		case EFFECT_OPCODE_STEALTH:
			return sn_stealth();
		case EFFECT_OPCODE_KNOCKBACK:
			return sn_knockback();
		case EFFECT_OPCODE_REFLECT:
			return sn_reflect();
		case EFFECT_OPCODE_AOE_SLOW:
			return sn_aoe_slow();
		case EFFECT_OPCODE_AOE_ROOT:
			return sn_aoe_root();
		case EFFECT_OPCODE_AOE_SILENCE:
			return sn_aoe_silence();
		case EFFECT_OPCODE_AOE_DISARM:
			return sn_aoe_disarm();
		case EFFECT_OPCODE_AOE_KNOCKBACK:
			return sn_aoe_knockback();
		case EFFECT_OPCODE_AOE_REFLECT:
			return sn_aoe_reflect();
		case EFFECT_OPCODE_AOE_STUN:
			return sn_aoe_stun();
		case EFFECT_OPCODE_REFLECT_DAMAGE:
			return sn_reflect_damage();
		case EFFECT_OPCODE_REDIRECT_DAMAGE:
			return sn_redirect_damage();
		case EFFECT_OPCODE_SUMMON_ALLY:
			return sn_summon_ally();
		case EFFECT_OPCODE_KNOCKBACK_SHIELD:
			return sn_knockback_shield();
		case EFFECT_OPCODE_TARGET_STATUS_MULTIPLIER:
			return sn_target_status_multiplier();
		case EFFECT_OPCODE_STAT_MODIFIER:
			return sn_stat_modifier();
		case EFFECT_OPCODE_MULTI_TARGET:
			return sn_multi_target();
		default:
			return empty_kind;
	}
}

void TeamfightSimulationCore::ParamTracker::report_unused(const String &effect_kind) const {
	// Report parameters that were provided but never accessed
	Array keys = params.keys();
	for (int i = 0; i < keys.size(); i++) {
		String key = String(keys[i]);
		bool was_accessed = false;
		for (const auto &accessed_key : accessed) {
			if (key == accessed_key) {
				was_accessed = true;
				break;
			}
		}
		if (!was_accessed) {
			ERR_PRINT(vformat("EFFECT PARAMETER MISMATCH: Effect kind '%s' (reason: '%s') has unused parameter '%s'. This parameter was not read by the C++ compilation code. Check if the parameter name is correct.", effect_kind, reason, key));
		}
	}
}

TeamfightSimulationCore::EffectRecord TeamfightSimulationCore::_compile_effect(const Dictionary &effect) const {
	EffectRecord compiled;
	String kind_str = String(effect.get("kind", ""));
	
	StringName kind;
	Dictionary params = Dictionary(effect.get("params", Dictionary()));
	ParamTracker tracker(params);
	compiled.windup = double(tracker.get("windup_override", -1.0));
	if (kind_str == "multi_target") {
		kind = sn_multi_target();
	} else if (kind_str == "multi_effect") {
		kind = sn_multi_effect();
	} else if (kind_str == "damage") {
		kind = sn_damage();
	} else if (kind_str == "projectile") {
		kind = sn_projectile();
	} else if (kind_str == "stun") {
		kind = sn_stun();
	} else if (kind_str == "shield") {
		kind = sn_shield();
	} else if (kind_str == "heal") {
		kind = sn_heal();
	} else if (kind_str == "aoe_taunt") {
		kind = sn_aoe_taunt();
	} else if (kind_str == "aoe_damage") {
		kind = sn_aoe_damage();
	} else if (kind_str == "damage_threshold_trigger") {
		kind = sn_damage_threshold_trigger();
	} else if (kind_str == "damage_over_time") {
		kind = sn_damage_over_time();
	} else if (kind_str == "heal_over_time") {
		kind = sn_heal_over_time();
	} else if (kind_str == "aoe_damage_over_time") {
		kind = sn_aoe_damage_over_time();
	} else if (kind_str == "aoe_heal_over_time") {
		kind = sn_aoe_heal_over_time();
	} else if (kind_str == "mana_regen") {
		kind = sn_mana_regen();
	} else if (kind_str == "post_damage_mana_gain") {
		kind = sn_post_damage_mana_gain();
	} else if (kind_str == "damage_based_heal") {
		kind = sn_damage_based_heal();
	} else if (kind_str == "damage_based_shield") {
		kind = sn_damage_based_shield();
	} else if (kind_str == "consume_stacks_damage") {
		kind = sn_consume_stacks_damage();
	} else if (kind_str == "consume_stacks_heal") {
		kind = sn_consume_stacks_heal();
	} else if (kind_str == "consume_stacks_shield") {
		kind = sn_consume_stacks_shield();
	} else if (kind_str == "set_stacks") {
		kind = sn_set_stacks();
	} else if (kind_str == "mana_restore_on_hit") {
		kind = sn_mana_restore_on_hit();
	} else if (kind_str == "drain_target_mana_on_hit") {
		kind = sn_drain_target_mana_on_hit();
	} else if (kind_str == "every_n_attacks_stun") {
		kind = sn_every_n_attacks_stun();
	} else if (kind_str == "self_dash") {
		kind = sn_self_dash();
	} else if (kind_str == "auto_dodge") {
		kind = sn_auto_dodge();
	} else if (kind_str == "constant_multiplier") {
		kind = sn_constant_multiplier();
	} else if (kind_str == "hp_threshold_damage_multiplier") {
		kind = sn_hp_threshold_damage_multiplier();
	} else if (kind_str == "distance_threshold_multiplier") {
		kind = sn_distance_threshold_multiplier();
	} else if (kind_str == "slow") {
		kind = sn_slow();
	} else if (kind_str == "root") {
		kind = sn_root();
	} else if (kind_str == "silence") {
		kind = sn_silence();
	} else if (kind_str == "disarm") {
		kind = sn_disarm();
	} else if (kind_str == "stealth") {
		kind = sn_stealth();
	} else if (kind_str == "knockback") {
		kind = sn_knockback();
	} else if (kind_str == "reflect") {
		kind = sn_reflect();
	} else if (kind_str == "aoe_slow") {
		kind = sn_aoe_slow();
	} else if (kind_str == "aoe_root") {
		kind = sn_aoe_root();
	} else if (kind_str == "aoe_silence") {
		kind = sn_aoe_silence();
	} else if (kind_str == "aoe_disarm") {
		kind = sn_aoe_disarm();
	} else if (kind_str == "aoe_knockback") {
		kind = sn_aoe_knockback();
	} else if (kind_str == "aoe_reflect") {
		kind = sn_aoe_reflect();
	} else if (kind_str == "aoe_stun") {
		kind = sn_aoe_stun();
	} else if (kind_str == "reflect_damage") {
		kind = sn_reflect_damage();
	} else if (kind_str == "knockback_shield") {
		kind = sn_knockback_shield();
	} else if (kind_str == "target_status_multiplier") {
		kind = sn_target_status_multiplier();
	} else if (kind_str == "stat_modifier") {
		kind = sn_stat_modifier();
	} else {
		kind = StringName(kind_str);
	}
	compiled.opcode = _opcode_for_kind(kind);
	if (kind == sn_multi_effect()) {
		Variant effects_value = tracker.get("effects", Variant());
		Array effects = effects_value.get_type() == Variant::ARRAY ? Array(effects_value) : Array();
		compiled.children = _compile_effect_array(effects);
		return compiled;
	}
	if (kind == sn_constant_multiplier()) {
		compiled.scalar0 = double(tracker.get("multiplier", 1.0));
		// INCONSISTENT: no reason string
	} else if (kind == sn_hp_threshold_damage_multiplier()) {
		compiled.scalar0 = double(tracker.get("above_hp_ratio", 0.0));
		compiled.scalar1 = double(tracker.get("below_hp_ratio", 0.0));
		compiled.scalar2 = double(tracker.get("multiplier", 1.0));
		// INCONSISTENT: no reason string
	} else if (kind == sn_distance_threshold_multiplier()) {
		compiled.scalar0 = double(tracker.get("min_distance", 0.0));
		compiled.scalar1 = double(tracker.get("multiplier", 1.0));
		// INCONSISTENT: no reason string
	} else if (kind == sn_damage()) {
		compiled.scalar0 = double(tracker.get("max_hp_ratio", 0.0));
		compiled.scalar1 = double(tracker.get("damage_ratio", 0.0));
		compiled.scalar2 = double(tracker.get("flat_amount", 0.0));
		compiled.scalar3 = bool(tracker.get("trigger_on_hit", false)) ? 1.0 : 0.0;
		compiled.int0 = tracker.get("target_self", false) ? 1 : 0;
		compiled.int1 = tracker.get("use_accumulated_damage", false) ? 1 : 0;
		String damage_type_str = String(tracker.get("damage_type", "physical"));
		if (damage_type_str == "physical") {
			compiled.damage_type = sn_physical();
		} else if (damage_type_str == "magic") {
			compiled.damage_type = sn_magic();
		} else if (damage_type_str == "true") {
			compiled.damage_type = sn_true();
		} else {
			compiled.damage_type = StringName(damage_type_str);
		}
		compiled.reason = String(tracker.get("reason", ""));
	} else if (kind == sn_projectile()) {
		// Use -1.0 as sentinel for "use unit's projectile_speed/radius stat" when override is null.
		// Python parity: speed_override=None → unit.stats.projectile_speed, radius_override=None → unit.stats.projectile_radius.
		Variant speed_v = tracker.get("speed_override", Variant());
		compiled.scalar0 = (speed_v.get_type() == Variant::NIL) ? -1.0 : double(speed_v);
		Variant radius_v = tracker.get("radius_override", Variant());
		compiled.scalar1 = (radius_v.get_type() == Variant::NIL) ? -1.0 : double(radius_v);
		compiled.scalar2 = double(tracker.get("damage_ratio", 0.0));
		compiled.scalar3 = double(tracker.get("stun_duration", 0.0));
		String damage_type_str = String(tracker.get("damage_type", "physical"));
		if (damage_type_str == "physical") {
			compiled.damage_type = sn_physical();
		} else if (damage_type_str == "magic") {
			compiled.damage_type = sn_magic();
		} else if (damage_type_str == "true") {
			compiled.damage_type = sn_true();
		} else {
			compiled.damage_type = StringName(damage_type_str);
		}
		compiled.reason = String(tracker.get("reason", ""));
	} else if (kind == sn_stun()) {
		compiled.scalar0 = double(tracker.get("duration", 0.0));
		compiled.reason = String(tracker.get("reason", ""));
	} else if (kind == sn_shield()) {
		compiled.scalar0 = double(tracker.get("max_hp_ratio", 0.0));
		compiled.scalar1 = double(tracker.get("damage_ratio", 0.0));
		compiled.scalar2 = double(tracker.get("flat_amount", 0.0));
		compiled.int0 = tracker.get("target_self", false) ? 1 : 0;
		compiled.reason = String(tracker.get("reason", ""));
	} else if (kind == sn_heal()) {
		compiled.scalar0 = double(tracker.get("max_hp_ratio", 0.0));
		compiled.scalar1 = double(tracker.get("current_hp_ratio", 0.0));
		compiled.scalar2 = double(tracker.get("flat_amount", 0.0));
		compiled.scalar3 = double(tracker.get("missing_hp_ratio", 0.0));
		compiled.int0 = tracker.get("target_self", false) ? 1 : 0;  // target_self parameter
		compiled.reason = String(tracker.get("reason", ""));
	} else if (kind == sn_aoe_taunt()) {
		compiled.scalar0 = double(tracker.get("radius", 0.0));
		compiled.scalar1 = double(tracker.get("duration", 0.0));
		compiled.reason = String(tracker.get("reason", "")); // INCONSISTENT: other AOE effects use descriptive defaults like "AOE Slow"
		compiled.aoe_shape_params = _parse_aoe_shape_metadata(params, tracker);
	} else if (kind == sn_aoe_damage()) {
		compiled.scalar0 = double(tracker.get("radius", 0.0));
		compiled.scalar1 = double(tracker.get("damage_ratio", 0.0));
		compiled.scalar2 = double(tracker.get("splash_ratio", 1.0));
		compiled.scalar3 = double(tracker.get("flat_amount", 0.0));
		compiled.scalar4 = double(tracker.get("max_hp_ratio", 0.0));
		compiled.int0 = tracker.get("use_accumulated_damage", false) ? 1 : 0;
		String damage_type_str = String(tracker.get("damage_type", "physical"));
		if (damage_type_str == "physical") {
			compiled.damage_type = sn_physical();
		} else if (damage_type_str == "magic") {
			compiled.damage_type = sn_magic();
		} else if (damage_type_str == "true") {
			compiled.damage_type = sn_true();
		} else {
			compiled.damage_type = StringName(damage_type_str);
		}
		compiled.reason = String(tracker.get("reason", "")); // INCONSISTENT: other AOE effects use descriptive defaults like "AOE Slow"
		compiled.aoe_shape_params = _parse_aoe_shape_metadata(params, tracker);
	} else if (kind == sn_damage_over_time()) {
		// Ratio-based parameters (now represent TOTAL amounts over full duration)
		compiled.scalar0 = double(tracker.get("damage_ratio", 0.0));
		compiled.scalar1 = double(tracker.get("max_hp_ratio", 0.0));
		compiled.scalar2 = double(tracker.get("tick_interval", 1.0));
		compiled.scalar3 = double(tracker.get("flat_amount", 0.0));
		compiled.scalar4 = double(tracker.get("duration", 0.0));  // Duration moved to scalar4
		String damage_type_str = String(tracker.get("damage_type", "physical"));
		if (damage_type_str == "physical") {
			compiled.damage_type = sn_physical();
		} else if (damage_type_str == "magic") {
			compiled.damage_type = sn_magic();
		} else if (damage_type_str == "true") {
			compiled.damage_type = sn_true();
		} else {
			compiled.damage_type = StringName(damage_type_str);
		}
		compiled.stacking_mode = StringName(tracker.get("stacking_mode", "refresh"));
		compiled.effect_type = StringName(tracker.get("effect_type", "generic"));
		compiled.reason = String(tracker.get("reason", ""));
		compiled.int0 = tracker.get("target_self", false) ? 1 : 0;  // target_self parameter
		compiled.int1 = int64_t(tracker.get("max_stacks", 1));
		String calculation_str = String(tracker.get("calculation", "fixed"));
		compiled.int2 = (calculation_str == "dynamic") ? 1 : 0;  // 0=fixed, 1=dynamic
	} else if (kind == sn_heal_over_time()) {
		// Ratio-based parameters (now represent TOTAL amounts over full duration)
		compiled.scalar0 = double(tracker.get("max_hp_ratio", 0.0));
		compiled.scalar1 = double(tracker.get("current_hp_ratio", 0.0));
		compiled.scalar2 = double(tracker.get("tick_interval", 1.0));
		compiled.scalar3 = double(tracker.get("missing_hp_ratio", 0.0));
		compiled.scalar4 = double(tracker.get("flat_amount", 0.0));
		compiled.scalar5 = double(tracker.get("duration", 0.0));  // Duration moved to scalar5
		compiled.stacking_mode = StringName(tracker.get("stacking_mode", "refresh"));
		compiled.effect_type = StringName(tracker.get("effect_type", "generic"));
		compiled.reason = String(tracker.get("reason", ""));
		compiled.int0 = tracker.get("target_self", false) ? 1 : 0;  // target_self parameter
		compiled.int1 = int64_t(tracker.get("max_stacks", 1));
		compiled.int2 = bool(tracker.get("allow_overheal", false)) ? 1 : 0;
		String calculation_str = String(tracker.get("calculation", "fixed"));
		compiled.int3 = (calculation_str == "dynamic") ? 1 : 0;  // 0=fixed, 1=dynamic
	} else if (kind == sn_aoe_damage_over_time()) {
		// AoE parameters
		compiled.scalar0 = double(tracker.get("radius", 0.0));
		// DoT ratio parameters (now represent TOTAL amounts over full duration)
		compiled.scalar1 = double(tracker.get("damage_ratio", 0.0));
		compiled.scalar2 = double(tracker.get("max_hp_ratio", 0.0));
		compiled.scalar3 = double(tracker.get("flat_amount", 0.0));
		compiled.scalar4 = double(tracker.get("tick_interval", 1.0));
		compiled.scalar5 = double(tracker.get("duration", 0.0));  // Duration moved to scalar5
		String damage_type_str = String(tracker.get("damage_type", "physical"));
		if (damage_type_str == "physical") {
			compiled.damage_type = sn_physical();
		} else if (damage_type_str == "magic") {
			compiled.damage_type = sn_magic();
		} else if (damage_type_str == "true") {
			compiled.damage_type = sn_true();
		} else {
			compiled.damage_type = StringName(damage_type_str);
		}
		compiled.stacking_mode = StringName(tracker.get("stacking_mode", "refresh"));
		compiled.effect_type = StringName(tracker.get("effect_type", "generic"));
		compiled.reason = String(tracker.get("reason", "")); // INCONSISTENT: other AOE effects use descriptive defaults like "AOE Slow"
		compiled.int0 = int64_t(tracker.get("max_stacks", 1));
		String calculation_str = String(tracker.get("calculation", "fixed"));
		compiled.int1 = (calculation_str == "dynamic") ? 1 : 0;  // 0=fixed, 1=dynamic
		compiled.int2 = tracker.get("target_self", false) ? 1 : 0;
		compiled.aoe_shape_params = _parse_aoe_shape_metadata(params, tracker);
	} else if (kind == sn_aoe_heal_over_time()) {
		// AoE parameters
		compiled.scalar0 = double(tracker.get("radius", 0.0));
		// HoT ratio parameters (now represent TOTAL amounts over full duration)
		compiled.scalar1 = double(tracker.get("max_hp_ratio", 0.0));
		compiled.scalar2 = double(tracker.get("current_hp_ratio", 0.0));
		compiled.scalar3 = double(tracker.get("missing_hp_ratio", 0.0));
		compiled.scalar4 = double(tracker.get("flat_amount", 0.0));
		compiled.scalar5 = double(tracker.get("tick_interval", 1.0));
		compiled.stacking_mode = StringName(tracker.get("stacking_mode", "refresh"));
		compiled.effect_type = StringName(tracker.get("effect_type", "generic"));
		compiled.reason = String(tracker.get("reason", "")); // INCONSISTENT: other AOE effects use descriptive defaults like "AOE Slow"
		compiled.int0 = int64_t(tracker.get("max_stacks", 1));
		compiled.int1 = int64_t(tracker.get("duration", 0.0));  // Duration stays in int1
		compiled.int2 = bool(tracker.get("allow_overheal", false)) ? 1 : 0;
		compiled.int3 = tracker.get("target_self", false) ? 1 : 0;
		String calculation_str = String(tracker.get("calculation", "fixed"));
		compiled.int4 = (calculation_str == "dynamic") ? 1 : 0;  // 0=fixed, 1=dynamic
		compiled.aoe_shape_params = _parse_aoe_shape_metadata(params, tracker);
	} else if (kind == sn_damage_threshold_trigger()) {
		compiled.scalar0 = double(tracker.get("threshold_multiplier", 1.0));
		Variant nested = tracker.get("effect", Variant());
		if (nested.get_type() == Variant::DICTIONARY) {
			compiled.children.push_back(_compile_effect(Dictionary(nested)));
		}
		// INCONSISTENT: no reason string
	} else if (kind == sn_mana_regen()) {
		compiled.scalar0 = double(tracker.get("flat_amount", 0.0));
		compiled.scalar1 = double(tracker.get("max_mana_ratio", 0.0));
		// INCONSISTENT: no reason string
	} else if (kind == sn_post_damage_mana_gain()) {
		compiled.scalar0 = double(tracker.get("damage_ratio", 0.0));
		// INCONSISTENT: no reason string
	} else if (kind == sn_damage_based_heal()) {
		compiled.scalar0 = double(tracker.get("damage_ratio", 0.0));
		compiled.int0 = tracker.get("use_accumulated_damage", false) ? 1 : 0;
		compiled.reason = String(tracker.get("reason", ""));
	} else if (kind == sn_damage_based_shield()) {
		compiled.scalar0 = double(tracker.get("damage_ratio", 0.0));
		compiled.reason = String(tracker.get("reason", ""));
	} else if (kind == sn_consume_stacks_damage()) {
		compiled.stat_name = StringName(tracker.get("stat_name", ""));
		compiled.scalar0 = double(tracker.get("base_damage_ratio", 0.0));
		compiled.scalar1 = double(tracker.get("stack_bonus_ratio", 0.0));
		compiled.string0 = String(tracker.get("stacking_mode", "multiplicative"));
		compiled.damage_type = StringName(tracker.get("damage_type", "physical"));
		compiled.string1 = String(tracker.get("stack_reason", ""));
		compiled.reason = String(tracker.get("reason", ""));
	} else if (kind == sn_consume_stacks_heal()) {
		compiled.stat_name = StringName(tracker.get("stat_name", ""));
		compiled.scalar0 = double(tracker.get("base_heal_ratio", 0.0));
		compiled.scalar1 = double(tracker.get("stack_bonus_ratio", 0.0));
		compiled.string0 = String(tracker.get("stacking_mode", "multiplicative"));
		compiled.string1 = String(tracker.get("stack_reason", ""));
		compiled.reason = String(tracker.get("reason", ""));
	} else if (kind == sn_consume_stacks_shield()) {
		compiled.stat_name = StringName(tracker.get("stat_name", ""));
		compiled.scalar0 = double(tracker.get("base_shield_ratio", 0.0));
		compiled.scalar1 = double(tracker.get("stack_bonus_ratio", 0.0));
		compiled.string0 = String(tracker.get("stacking_mode", "multiplicative"));
		compiled.string1 = String(tracker.get("stack_reason", ""));
		compiled.reason = String(tracker.get("reason", ""));
	} else if (kind == sn_set_stacks()) {
		compiled.stat_name = StringName(tracker.get("stat_name", ""));
		compiled.int0 = int64_t(tracker.get("stack_count", 0));
		compiled.int1 = tracker.get("to_max", false);
		compiled.int2 = int64_t(tracker.get("max_stacks", 0));  // Fallback max_stacks when entry doesn't exist
		compiled.scalar0 = double(tracker.get("duration", 0.0));
		compiled.scalar1 = double(tracker.get("additive_per_stack", 0.0));  // Fallback when entry doesn't exist
		compiled.scalar2 = double(tracker.get("multiplicative_per_stack", 1.0));  // Fallback when entry doesn't exist
		compiled.string0 = String(tracker.get("reason", ""));
		compiled.reason = String(tracker.get("reason", ""));
	} else if (kind == sn_channel()) {
		compiled.scalar0 = double(tracker.get("duration", 0.0));  // total duration
		compiled.scalar1 = double(tracker.get("tick_interval", 1.0));  // tick interval
		compiled.int0 = tracker.get("allow_movement", false) ? 1 : 0;
		compiled.string0 = String(tracker.get("target_mode", "fixed"));  // "fixed", "dynamic", or "self"
		compiled.reason = String(tracker.get("reason", ""));
		
		// Parse sub_effect (required)
		if (params.has("sub_effect")) {
			tracker.mark_accessed("sub_effect");
			Dictionary sub_effect_dict = params["sub_effect"];
			compiled.children.push_back(_compile_effect(sub_effect_dict));
		}
		
		// Parse post_complete_effect (optional)
		if (params.has("post_complete_effect")) {
			tracker.mark_accessed("post_complete_effect");
			Dictionary post_complete_dict = params["post_complete_effect"];
			compiled.sub_effects.push_back(_compile_effect(post_complete_dict));
		}
		
		// Parse post_interrupt_effect (optional)
		if (params.has("post_interrupt_effect")) {
			tracker.mark_accessed("post_interrupt_effect");
			Dictionary post_interrupt_dict = params["post_interrupt_effect"];
			compiled.sub_effects.push_back(_compile_effect(post_interrupt_dict));
		}
	} else if (kind == sn_mana_restore_on_hit()) {
		compiled.scalar0 = double(tracker.get("flat_amount", 0.0));
		// INCONSISTENT: no reason string
	} else if (kind == sn_drain_target_mana_on_hit()) {
		compiled.scalar0 = double(tracker.get("flat_amount", 0.0));
		// INCONSISTENT: no reason string
	} else if (kind == sn_every_n_attacks_stun()) {
		compiled.int0 = int64_t(tracker.get("every_n", 0));
		compiled.scalar0 = double(tracker.get("stun_duration", 0.0));
		compiled.reason = String(tracker.get("reason", ""));
	} else if (kind == sn_self_dash()) {
		compiled.scalar0 = double(tracker.get("distance", 0.0));
		Dictionary direction = Dictionary(tracker.get("direction", Dictionary()));
		compiled.scalar1 = double(direction.get("x", 0.0));
		compiled.scalar2 = double(direction.get("y", 0.0));
		// INCONSISTENT: no reason string
	} else if (kind == sn_auto_dodge()) {
		compiled.scalar0 = double(tracker.get("dodge_chance", 0.0));
		compiled.scalar1 = double(tracker.get("on_dodge_multiplier", 0.0));
		compiled.scalar2 = double(tracker.get("on_hit_multiplier", 1.0));
		// INCONSISTENT: no reason string
	}
	else if (kind == sn_slow()) {
		compiled.scalar0 = double(tracker.get("slow_percentage", 0.0));
		compiled.scalar1 = double(tracker.get("duration", 0.0));
		compiled.reason = String(tracker.get("reason", "Slow"));
	} else if (kind == sn_root()) {
		compiled.scalar0 = double(tracker.get("duration", 0.0));
		compiled.reason = String(tracker.get("reason", "Root"));
	} else if (kind == sn_silence()) {
		compiled.scalar0 = double(tracker.get("duration", 0.0));
		compiled.int0 = tracker.get("block_abilities", true) ? 1 : 0;
		compiled.int1 = tracker.get("block_ultimate", true) ? 1 : 0;
		compiled.reason = String(tracker.get("reason", "Silence"));
	} else if (kind == sn_disarm()) {
		compiled.scalar0 = double(tracker.get("duration", 0.0));
		compiled.reason = String(tracker.get("reason", "Disarm"));
	} else if (kind == sn_stealth()) {
		Dictionary break_conditions = Dictionary(tracker.get("break_conditions", Dictionary()));
		compiled.scalar0 = double(tracker.get("duration", 0.0));
		compiled.int0 = bool(break_conditions.get("on_attack", false)) ? 1 : 0;
		compiled.int1 = bool(break_conditions.get("on_ability", false)) ? 1 : 0;
		compiled.int2 = bool(break_conditions.get("on_damage_taken", false)) ? 1 : 0;
		compiled.int3 = tracker.get("target_self", false) ? 1 : 0;
		compiled.reason = String(tracker.get("reason", "Stealth"));
	} else if (kind == sn_knockback()) {
		compiled.scalar0 = double(tracker.get("distance", 0.0));
		compiled.scalar1 = double(tracker.get("duration", 0.0));
		compiled.int0 = tracker.get("direction", "away_from_source") == "away_from_source" ? 1 : 0;
		compiled.reason = String(tracker.get("reason", "Knockback"));
	} else if (kind == sn_reflect()) {
		compiled.scalar0 = double(tracker.get("reflect_percentage", 0.0));
		compiled.scalar1 = double(tracker.get("duration", 0.0));
		compiled.int0 = tracker.get("reflect_type", "all") == "all" ? 1 : 0;
		compiled.reason = String(tracker.get("reason", "Reflect"));
	} else if (kind == sn_aoe_slow()) {
		compiled.scalar0 = double(tracker.get("radius", 0.0));
		compiled.scalar1 = double(tracker.get("slow_percentage", 0.0));
		compiled.scalar2 = double(tracker.get("duration", 0.0));
		compiled.aoe_shape_params = _parse_aoe_shape_metadata(params, tracker);
		compiled.reason = String(tracker.get("reason", "AOE Slow"));
	} else if (kind == sn_aoe_root()) {
		compiled.scalar0 = double(tracker.get("radius", 0.0));
		compiled.scalar1 = double(tracker.get("duration", 0.0));
		compiled.aoe_shape_params = _parse_aoe_shape_metadata(params, tracker);
		compiled.reason = String(tracker.get("reason", "AOE Root"));
	} else if (kind == sn_aoe_silence()) {
		compiled.scalar0 = double(tracker.get("radius", 0.0));
		compiled.scalar1 = double(tracker.get("duration", 0.0));
		compiled.int0 = tracker.get("block_abilities", true) ? 1 : 0;
		compiled.int1 = tracker.get("block_ultimate", true) ? 1 : 0;
		compiled.aoe_shape_params = _parse_aoe_shape_metadata(params, tracker);
		compiled.reason = String(tracker.get("reason", "AOE Silence"));
	} else if (kind == sn_aoe_disarm()) {
		compiled.scalar0 = double(tracker.get("radius", 0.0));
		compiled.scalar1 = double(tracker.get("duration", 0.0));
		compiled.aoe_shape_params = _parse_aoe_shape_metadata(params, tracker);
		compiled.reason = String(tracker.get("reason", "AOE Disarm"));
	} else if (kind == sn_aoe_knockback()) {
		compiled.scalar0 = double(tracker.get("radius", 0.0));
		compiled.scalar1 = double(tracker.get("distance", 0.0));
		compiled.scalar2 = double(tracker.get("duration", 0.0));
		compiled.int0 = tracker.get("direction", "away_from_source") == "away_from_source" ? 1 : 0;
		compiled.aoe_shape_params = _parse_aoe_shape_metadata(params, tracker);
		compiled.reason = String(tracker.get("reason", "AOE Knockback"));
	} else if (kind == sn_aoe_reflect()) {
		// TODO: Rework reflect_type to be selectable per damage type (physical, magic, true) instead of binary "all" vs "physical"
		compiled.scalar0 = double(tracker.get("radius", 0.0));
		compiled.scalar1 = double(tracker.get("reflect_percentage", 0.0));
		compiled.scalar2 = double(tracker.get("duration", 0.0));
		compiled.int0 = tracker.get("reflect_type", "all") == "all" ? 1 : 0;
		compiled.aoe_shape_params = _parse_aoe_shape_metadata(params, tracker);
		compiled.reason = String(tracker.get("reason", "AOE Reflect"));
	} else if (kind == sn_aoe_stun()) {
		compiled.scalar0 = double(tracker.get("radius", 0.0));
		compiled.scalar1 = double(tracker.get("duration", 0.0));
		compiled.aoe_shape_params = _parse_aoe_shape_metadata(params, tracker);
		compiled.reason = String(tracker.get("reason", "AOE Stun"));
	} else if (kind == sn_reflect_damage()) {
		// TODO: Rework reflect_type to be selectable per damage type (physical, magic, true) instead of binary "all" vs "physical"
		compiled.scalar0 = double(tracker.get("reflect_percentage", 0.0));
		compiled.int0 = tracker.get("reflect_type", "all") == "all" ? 1 : 0;
		compiled.reason = String(tracker.get("reason", ""));
	} else if (kind == sn_redirect_damage()) {
		compiled.scalar0 = Math::clamp(double(tracker.get("redirect_ratio", 0.0)), 0.0, 1.0);
		compiled.scalar1 = Math::clamp(double(tracker.get("reduction_ratio", 0.0)), 0.0, 1.0);
		compiled.scalar2 = double(tracker.get("redirect_cap", 0.0));
		compiled.reason = String(tracker.get("reason", ""));
	} else if (kind == sn_summon_ally()) {
		compiled.scalar0 = double(tracker.get("spawn_radius", 0.0));
		// Parse minions array: each entry has minion_id and count
		Array minions_array = tracker.get("minions", Array());
		for (int64_t i = 0; i < minions_array.size(); ++i) {
			Dictionary minion_entry = minions_array[i];
			StringName minion_id = StringName(String(minion_entry.get("minion_id", "")));
			int64_t count = int64_t(minion_entry.get("count", 1));
			// Store minion_id in string0 and count in int0 for each entry
			// We'll use children array to store multiple minion specs
			// Note: opcode not set - children are data containers, not executable sub-effects
			EffectRecord minion_spec;
			minion_spec.string0 = String(minion_id);
			minion_spec.int0 = count;
			compiled.children.push_back(minion_spec);
		}
		compiled.reason = String(tracker.get("reason", "Summon Ally"));
	} else if (kind == sn_knockback_shield()) {
		compiled.scalar0 = double(tracker.get("shield_ratio", 0.0));
		compiled.reason = String(tracker.get("reason", "Knockback shield"));
	} else if (kind == sn_target_status_multiplier()) {
		compiled.scalar0 = double(tracker.get("multiplier", 1.0));
		String status_kind_str = String(tracker.get("status_kind", ""));
		if (status_kind_str == "slow") {
			compiled.damage_type = sn_slow();
		} else if (status_kind_str == "root") {
			compiled.damage_type = sn_root();
		} else if (status_kind_str == "silence") {
			compiled.damage_type = sn_silence();
		} else if (status_kind_str == "disarm") {
			compiled.damage_type = sn_disarm();
		} else if (status_kind_str == "stealth") {
			compiled.damage_type = sn_stealth();
		} else if (status_kind_str == "stun") {
			compiled.damage_type = sn_stun();
		} else if (status_kind_str == "reflect") {
			compiled.damage_type = sn_reflect();
		} else {
			compiled.damage_type = StringName(status_kind_str);
		}
		compiled.reason = String(tracker.get("reason", "Target status multiplier"));
	} else if (kind == sn_stat_modifier()) {
		// Validate stat name
		StringName stat_name = StringName(String(tracker.get("stat_name", "")));
		if (!_is_valid_stat_name(stat_name)) {
			compiled.opcode = EFFECT_OPCODE_UNKNOWN;
			return compiled;
		}

		compiled.damage_type = stat_name;
		compiled.scalar0 = double(tracker.get("additive", 0.0));
		compiled.scalar1 = double(tracker.get("multiplicative", 1.0));
		compiled.scalar2 = double(tracker.get("duration", 0.0));
		compiled.scalar3 = double(tracker.get("heal_gained_ratio", 0.0));
		compiled.int0 = tracker.get("target_self", false) ? 1 : 0;
		compiled.int1 = tracker.get("duration_type", "respawn") == "match" ? 1 : 0;
		compiled.int2 = int64_t(tracker.get("max_stacks", 1));
		String stack_behavior = String(tracker.get("stack_behavior", "refresh"));
		if (stack_behavior == "accumulate") {
			compiled.int3 = 1;
		} else if (stack_behavior == "reset") {
			compiled.int3 = 2;
		} else {
			compiled.int3 = 0;
		}
		compiled.reason = String(tracker.get("reason", "Stat modifier"));
	} else if (kind == sn_multi_target()) {
		// Multi-target effect parameters
		compiled.int0 = int64_t(tracker.get("target_count", 1));
		String strategy_str = String(tracker.get("selection_strategy", "closest"));
		if (strategy_str == "random") {
			compiled.int1 = TARGET_SELECTION_RANDOM;
		} else if (strategy_str == "lowest_hp") {
			compiled.int1 = TARGET_SELECTION_LOWEST_HP;
		} else if (strategy_str == "highest_hp") {
			compiled.int1 = TARGET_SELECTION_HIGHEST_HP;
		} else if (strategy_str == "closest_to_target") {
			compiled.int1 = TARGET_SELECTION_CLOSEST_TO_TARGET;
		} else if (strategy_str == "lowest_percent_hp") {
			compiled.int1 = TARGET_SELECTION_LOWEST_PERCENT_HP;
		} else if (strategy_str == "highest_percent_hp") {
			compiled.int1 = TARGET_SELECTION_HIGHEST_PERCENT_HP;
		} else if (strategy_str == "closest") {
			compiled.int1 = TARGET_SELECTION_CLOSEST;
		} else {
			UtilityFunctions::push_error(vformat("Invalid selection_strategy '%s' for multi_target effect", strategy_str));
			compiled.int1 = -1;
		}
		compiled.int2 = tracker.get("include_self", false) ? 1 : 0;
		String handling_str = String(tracker.get("excess_handling", "drop"));
		if (handling_str == "stack") {
			compiled.int3 = EXCESS_TARGET_STACK;
		} else if (handling_str == "drop") {
			compiled.int3 = EXCESS_TARGET_DROP;
		} else {
			UtilityFunctions::push_error(vformat("Invalid excess_handling '%s' for multi_target effect", handling_str));
			compiled.int3 = -1;
		}
		compiled.int4 = int64_t(tracker.get("repeat_count", 1));
		String team_filter_str = String(tracker.get("team_filter", ""));
		if (team_filter_str == "ally") {
			compiled.team_filter = sn_ally();
		} else if (team_filter_str == "enemy") {
			compiled.team_filter = sn_enemy();
		} else {
			compiled.team_filter = StringName(team_filter_str);
		}
		
		// Compile sub_effects
		Variant sub_effects_value = tracker.get("sub_effects", Variant());
		if (sub_effects_value.get_type() == Variant::ARRAY) {
			Array sub_effects_array = sub_effects_value;
			compiled.sub_effects = _compile_effect_array(sub_effects_array);
		} else if (sub_effects_value.get_type() == Variant::DICTIONARY) {
			Dictionary sub_effect_dict = sub_effects_value;
			compiled.sub_effects.push_back(_compile_effect(sub_effect_dict));
		}
		
		compiled.reason = String(tracker.get("reason", "Multiple target"));
	}
	
	// Handle conditional execution parameters
	compiled.requires_result_from = StringName(String(tracker.get("requires_result_from", "")));
	compiled.requires_field = StringName(String(tracker.get("requires_field", "")));
	compiled.requires_value = tracker.get("requires_value", Variant());
	compiled.requires_target_status = StringName(String(tracker.get("requires_target_status", "")));
	compiled.status_target = StringName(String(tracker.get("status_target", "target")));
	
	// Validate status_target parameter
	if (!compiled.status_target.is_empty() && compiled.status_target != sn_source() && compiled.status_target != sn_target()) {
		UtilityFunctions::push_error(vformat("Invalid status_target '%s' in effect parameters. Valid values: 'source', 'target'", String(compiled.status_target)));
		compiled.status_target = sn_target();  // Default to target on error
	}
	
	// Handle on_tick_interval parameter for timing control
	compiled.on_tick_interval = double(tracker.get("on_tick_interval", 1.0));
	// Validate minimum on_tick_interval (must be at least game tick rate)
	compiled.on_tick_interval = Math::max(compiled.on_tick_interval, DEFAULT_TICK_RATE);
	
	// Set reason for error reporting
	tracker.reason = compiled.reason;
	
	// Report any unused parameters to catch mismatches
	tracker.report_unused(kind_str);
	
	return compiled;
}

std::vector<TeamfightSimulationCore::EffectRecord> TeamfightSimulationCore::_compile_effect_array(const Array &effects) const {
	std::vector<EffectRecord> compiled;
	compiled.reserve(size_t(effects.size()));
	for (int64_t index = 0; index < effects.size(); ++index) {
		Variant effect = effects[index];
		if (effect.get_type() == Variant::DICTIONARY) {
			compiled.push_back(_compile_effect(Dictionary(effect)));
		}
	}
	return compiled;
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

TeamfightSimulationCore::UnitStateCold &TeamfightSimulationCore::_uc(UnitState &u) {
	return _unit_cold[static_cast<size_t>(&u - _units.data())];
}

const TeamfightSimulationCore::UnitStateCold &TeamfightSimulationCore::_uc(const UnitState &u) const {
	return _unit_cold[static_cast<size_t>(&u - _units.data())];
}

void TeamfightSimulationCore::_finalize_reflect_passives(UnitState &unit, UnitStateCold &cold) {
	// TODO: Rework reflect_type to be selectable per damage type (physical, magic, true) instead of binary "all" vs "physical"
	// Or remove entirely and use separate reflect effects per damage type
	unit.reflect_passive_pct_all = 0.0;
	unit.reflect_passive_pct_physical = 0.0;
	for (const EffectRecord &eff : cold.passive_effects[1]) {
		if (eff.opcode == EFFECT_OPCODE_REFLECT_DAMAGE) {
			double p = eff.scalar0;
			if (eff.int0 == 1) {
				unit.reflect_passive_pct_all += p;
			} else {
				unit.reflect_passive_pct_physical += p;
			}
		}
	}
}

TeamfightSimulationCore::TeamfightSimulationCore() {
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
		_clear_all_stat_modifiers(unit);
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
	// Load catalog every time to pick up changes from champion_schema.json
	// This ensures stats generation uses the most recent champion data
	auto load_json_required = [](const String &path) -> Dictionary {
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
	};
	auto load_json_optional = [](const String &path) -> Dictionary {
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
	};

	Dictionary champion_catalog = load_json_required(String(CHAMPION_SCHEMA_PATH));

	// role_configs
	Dictionary role_configs;
	if (!champion_catalog.has("role_configs")) {
		UtilityFunctions::push_error("Champion schema missing 'role_configs' key");
	} else {
		Dictionary role_configs_dict = Dictionary(champion_catalog.get("role_configs", Dictionary()));
		Array role_keys = role_configs_dict.keys();
		for (int64_t i = 0; i < role_keys.size(); ++i) {
			StringName role_id = StringName(String(role_keys[i]));
			Dictionary role_entry = Dictionary(role_configs_dict.get(role_id, Dictionary()));
			role_configs[role_id] = role_entry;
		}
	}

	// passives
	Dictionary passive_registry;
	if (!champion_catalog.has("passives")) {
		UtilityFunctions::push_error("Champion schema missing 'passives' key");
	} else {
		Dictionary passives_dict = Dictionary(champion_catalog.get("passives", Dictionary()));
		Array passive_keys = passives_dict.keys();
		for (int64_t i = 0; i < passive_keys.size(); ++i) {
			StringName passive_id = StringName(String(passive_keys[i]));
			Dictionary passive_entry = Dictionary(passives_dict.get(passive_id, Dictionary()));
			passive_registry[passive_id] = passive_entry;
		}
	}

	// balance patches (required file, but contents may be empty)
	std::vector<BalancePatch> balance_patches;
	Dictionary bp_root = load_json_required(String(BALANCE_PATCHES_PATH));
	if (!bp_root.is_empty()) {
		Array patches = Array(bp_root.get("patches", Array()));
		for (int64_t i = 0; i < patches.size(); ++i) {
			Dictionary pd = Dictionary(patches[i]);
			BalancePatch patch;
			_parse_balance_patch_from_dict(pd, patch);
			balance_patches.push_back(patch);
		}
	}

	// champion kits (optional)
	Dictionary ability_kits;
	Dictionary kits_root = load_json_optional(String(CHAMPION_KITS_PATH));
	if (!kits_root.is_empty()) {
		ability_kits = Dictionary(kits_root.get("kits", Dictionary()));
	}

	// minions (loaded from separate minion schema file)
	Dictionary minion_catalog;
	Dictionary minion_schema = load_json_required(String(MINION_SCHEMA_PATH));
	if (!minion_schema.is_empty()) {
		Array minion_keys = minion_schema.keys();
		for (int64_t i = 0; i < minion_keys.size(); ++i) {
			StringName minion_id = StringName(String(minion_keys[i]));
			Dictionary minion_entry = Dictionary(minion_schema.get(minion_id, Dictionary()));
			minion_catalog[minion_id] = minion_entry;
		}
	} else {
		UtilityFunctions::push_error("Failed to load minion schema from " + String(MINION_SCHEMA_PATH));
	}

	_champion_catalog = champion_catalog;
	_minion_catalog = minion_catalog;
	_role_configs = role_configs;
	_passive_registry = passive_registry;
	_ability_kits = ability_kits;
	_balance_patches = balance_patches;

	_rebuild_effective_champion_cache();
	_catalog_loaded = true;
}

void TeamfightSimulationCore::_parse_balance_patch_from_dict(const Dictionary &pd, BalancePatch &patch) {
	Array targets = Array(pd.get("targets", Array()));
	for (int64_t t = 0; t < targets.size(); ++t) {
		patch.targets.push_back(StringName(String(targets[t])));
	}
	Array roles = Array(pd.get("roles", Array()));
	for (int64_t r = 0; r < roles.size(); ++r) {
		patch.roles.push_back(StringName(String(roles[r])));
	}
	patch.stat_multipliers = Dictionary(pd.get("stat_multipliers", Dictionary()));
	patch.stat_additions = Dictionary(pd.get("stat_additions", Dictionary()));
	if (pd.has("kit_id")) {
		patch.kit_id = StringName(String(pd["kit_id"]));
	}
	if (pd.has("ability")) {
		patch.ability_override = pd["ability"];
	}
	if (pd.has("ultimate")) {
		patch.ultimate_override = pd["ultimate"];
	}
	if (pd.has("passive_ids")) {
		patch.passive_ids_override = pd["passive_ids"];
	}
}

bool TeamfightSimulationCore::_patch_applies_to(const BalancePatch &patch, const StringName &archetype_id, const StringName &role) const {
	if (!patch.targets.empty()) {
		bool matched = false;
		for (const StringName &t : patch.targets) {
			if (t == archetype_id) {
				matched = true;
				break;
			}
		}
		if (!matched) {
			return false;
		}
	}
	if (!patch.roles.empty()) {
		bool matched = false;
		for (const StringName &r : patch.roles) {
			if (r == role) {
				matched = true;
				break;
			}
		}
		if (!matched) {
			return false;
		}
	}
	return true;
}

void TeamfightSimulationCore::_apply_stat_patch_to_stats(const BalancePatch &patch, Dictionary &stats) const {
	Array mul_keys = patch.stat_multipliers.keys();
	for (int64_t i = 0; i < mul_keys.size(); ++i) {
		Variant key = mul_keys[i];
		if (stats.has(key)) {
			double current = double(stats[key]);
			double multiplier = double(patch.stat_multipliers[key]);
			stats[key] = current * multiplier;
		}
	}
	Array add_keys = patch.stat_additions.keys();
	for (int64_t i = 0; i < add_keys.size(); ++i) {
		Variant key = add_keys[i];
		if (stats.has(key)) {
			double current = double(stats[key]);
			double delta = double(patch.stat_additions[key]);
			stats[key] = current + delta;
		}
	}
}

void TeamfightSimulationCore::_merge_kit_into_champion(Dictionary &champion, const Dictionary &kit) const {
	if (kit.has("ability")) {
		Variant v = kit["ability"];
		if (v.get_type() == Variant::DICTIONARY) {
			champion["ability"] = Dictionary(v).duplicate(true);
		}
	}
	if (kit.has("ultimate")) {
		Variant v = kit["ultimate"];
		if (v.get_type() == Variant::DICTIONARY) {
			champion["ultimate"] = Dictionary(v).duplicate(true);
		}
	}
	if (kit.has("passive_ids")) {
		Variant v = kit["passive_ids"];
		if (v.get_type() == Variant::ARRAY) {
			champion["passive_ids"] = Array(v).duplicate();
		}
	}
}

void TeamfightSimulationCore::_rebuild_effective_champion_cache() {
	_effective_champion_by_archetype.clear();
	Array archetype_keys = _champion_catalog.keys();
	for (int64_t ki = 0; ki < archetype_keys.size(); ++ki) {
		Variant key_var = archetype_keys[ki];
		String key_str = String(key_var);

		// Skip non-champion entries like "passives", "role_configs", and "minions"
		if (key_str == "passives" || key_str == "role_configs" || key_str == "minions") {
			continue;
		}
		
		StringName archetype_id = StringName(key_str);
		Dictionary base = Dictionary(_champion_catalog[key_var]);
		if (base.is_empty()) {
			continue;
		}
		Dictionary eff = base.duplicate(true);
		Dictionary stats = Dictionary(eff["stats"]);
		Dictionary role_config = Dictionary(_role_configs.get(stats.get("role", StringName()), Dictionary()));
		Dictionary stat_mods = Dictionary(role_config.get("stat_mods", Dictionary()));
		Array stat_keys = stat_mods.keys();
		for (int64_t key_index = 0; key_index < stat_keys.size(); ++key_index) {
			Variant key_value = stat_keys[key_index];
			stats[key_value] = stat_mods[key_value];
		}
		StringName role_name = StringName(stats.get("role", StringName()));

		for (const BalancePatch &patch : _balance_patches) {
			if (!_patch_applies_to(patch, archetype_id, role_name)) {
				continue;
			}
			_apply_stat_patch_to_stats(patch, stats);

			if (!patch.kit_id.is_empty()) {
				Variant kit_v = _ability_kits.get(patch.kit_id, Variant());
				if (kit_v.get_type() == Variant::DICTIONARY) {
					_merge_kit_into_champion(eff, Dictionary(kit_v));
				} else {
					UtilityFunctions::push_error(vformat("Balance patch references unknown kit_id '%s' (archetype '%s')", String(patch.kit_id), key_str));
				}
			}
			if (patch.ability_override.get_type() == Variant::DICTIONARY) {
				eff["ability"] = Dictionary(patch.ability_override).duplicate(true);
			}
			if (patch.ultimate_override.get_type() == Variant::DICTIONARY) {
				eff["ultimate"] = Dictionary(patch.ultimate_override).duplicate(true);
			}
			if (patch.passive_ids_override.get_type() == Variant::ARRAY) {
				eff["passive_ids"] = Array(patch.passive_ids_override).duplicate();
			}
		}

		eff["stats"] = stats;

		if (!_validate_effective_champion(archetype_id, eff)) {
			UtilityFunctions::push_error(vformat("Effective champion validation failed for '%s'; using vanilla catalog entry.", key_str));
			Dictionary vanilla = base.duplicate(true);
			Dictionary vstats = Dictionary(vanilla["stats"]);
			Dictionary vrole_config = Dictionary(_role_configs.get(vstats.get("role", StringName()), Dictionary()));
			Dictionary vstat_mods = Dictionary(vrole_config.get("stat_mods", Dictionary()));
			Array vk = vstat_mods.keys();
			for (int64_t i = 0; i < vk.size(); ++i) {
				Variant kv = vk[i];
				vstats[kv] = vstat_mods[kv];
			}
			vanilla["stats"] = vstats;
			_effective_champion_by_archetype[key_str] = vanilla;
		} else {
			_effective_champion_by_archetype[key_str] = eff;
		}
	}
}

bool TeamfightSimulationCore::_validate_effective_champion(const StringName &archetype_id, const Dictionary &champion) const {
	bool ok = true;
	Variant ab = champion.get("ability", Variant());
	if (ab.get_type() == Variant::DICTIONARY) {
		Dictionary abd = Dictionary(ab);
		StringName kind = StringName(String(abd.get("kind", "")));
		EffectRecord compiled = _compile_effect(abd);
		if (!kind.is_empty() && compiled.opcode == EFFECT_OPCODE_UNKNOWN) {
			UtilityFunctions::push_error(vformat("Unknown or unsupported ability kind '%s' for archetype '%s'", String(kind), String(archetype_id)));
			ok = false;
		}
	}
	Variant ult = champion.get("ultimate", Variant());
	if (ult.get_type() == Variant::DICTIONARY) {
		Dictionary ultd = Dictionary(ult);
		StringName kind = StringName(String(ultd.get("kind", "")));
		EffectRecord compiled = _compile_effect(ultd);
		if (!kind.is_empty() && compiled.opcode == EFFECT_OPCODE_UNKNOWN) {
			UtilityFunctions::push_error(vformat("Unknown or unsupported ultimate kind '%s' for archetype '%s'", String(kind), String(archetype_id)));
			ok = false;
		}
	}
	Array passive_ids = Array(champion.get("passive_ids", Array()));
	for (int64_t i = 0; i < passive_ids.size(); ++i) {
		StringName pid = StringName(String(passive_ids[i]));
		if (pid.is_empty()) {
			continue;
		}
		if (!_passive_registry.has(pid)) {
			UtilityFunctions::push_error(vformat("Unknown passive_id '%s' for archetype '%s'", String(pid), String(archetype_id)));
			ok = false;
		}
	}
	return ok;
}

Dictionary TeamfightSimulationCore::_effective_champion_for(const StringName &archetype_id) const {
	String key = String(archetype_id);
	if (_effective_champion_by_archetype.has(key)) {
		return Dictionary(_effective_champion_by_archetype[key]);
	}
	return _champion_for(archetype_id);
}

void TeamfightSimulationCore::_build_role_configs() {
	// Load role configs from the champion schema JSON (generated from GDScript)
	Dictionary catalog = _champion_catalog;
	if (!catalog.has("role_configs")) {
		UtilityFunctions::push_error("Champion schema missing 'role_configs' key");
		return;
	}
	
	Dictionary role_configs_dict = Dictionary(catalog.get("role_configs", Dictionary()));
	Array role_keys = role_configs_dict.keys();
	
	for (int64_t i = 0; i < role_keys.size(); ++i) {
		StringName role_id = StringName(String(role_keys[i]));
		Dictionary role_entry = Dictionary(role_configs_dict.get(role_id, Dictionary()));
		_role_configs[role_id] = role_entry;
	}
}

void TeamfightSimulationCore::_build_passive_registry() {
	// Load passives from the champion schema JSON (generated from GDScript)
	Dictionary catalog = _champion_catalog;
	if (!catalog.has("passives")) {
		UtilityFunctions::push_error("Champion schema missing 'passives' key");
		return;
	}
	
	Dictionary passives_dict = Dictionary(catalog.get("passives", Dictionary()));
	Array passive_keys = passives_dict.keys();
	
	for (int64_t i = 0; i < passive_keys.size(); ++i) {
		StringName passive_id = StringName(String(passive_keys[i]));
		Dictionary passive_entry = Dictionary(passives_dict.get(passive_id, Dictionary()));
		_passive_registry[passive_id] = passive_entry;
	}
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
	// Check champion catalog first
	if (_champion_catalog.has(archetype_id)) {
		return Dictionary(_champion_catalog[archetype_id]);
	}
	// Fall back to minion catalog for minions
	if (_minion_catalog.has(archetype_id)) {
		return Dictionary(_minion_catalog[archetype_id]);
	}
	return Dictionary();
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
	TargetingFrameEntry frame;
	frame.instance_id = unit.instance_id;
	frame.is_player_team = unit.team == sn_player();
	frame.role_slot = unit.role_slot;
	frame.is_tank_role = unit.is_tank_role;
	frame.is_fighter_role = unit.is_fighter_role;
	frame.is_assassin_role = unit.is_assassin_role;
	frame.is_marksman_role = unit.is_marksman_role;
	frame.is_mage_role = unit.is_mage_role;
	frame.is_support_role = unit.is_support_role;
	frame.pos_x = unit.pos_x;
	frame.pos_y = unit.pos_y;
	frame.hp = unit.hp;
	frame.max_hp = get_effective_max_hp(unit);
	frame.alive = unit.alive;
	frame.target_id = unit.target_id;
	frame.incoming_target_count = unit.incoming_target_count;
	frame.perceived_threat = unit.perceived_threat;
	frame.stealth_remaining = unit.stealth_remaining;
	return frame;
}

void TeamfightSimulationCore::_sync_targeting_frame_index(int64_t index, const UnitState &unit) {
	if (index < 0 || index >= int64_t(_targeting_frame.size())) {
		return;
	}
	TargetingFrameEntry &frame = _targeting_frame[static_cast<size_t>(index)];
	
	if (_sim_profile_targeting_active) {
		_sim_profile_tgt_frame_syncs += 1;
	}
	frame.pos_x = unit.pos_x;
	frame.pos_y = unit.pos_y;
	frame.hp = unit.hp;
	frame.max_hp = get_effective_max_hp(unit);
	frame.alive = unit.alive;
	frame.target_id = unit.target_id;
	frame.incoming_target_count = unit.incoming_target_count;
	frame.perceived_threat = unit.perceived_threat;
	frame.stealth_remaining = unit.stealth_remaining;
}

void TeamfightSimulationCore::_sync_targeting_frame_unit(const UnitState &unit) {
	_sync_targeting_frame_index(_unit_index_by_id(unit.instance_id), unit);
}

std::pair<TeamfightSimulationCore::UnitState, TeamfightSimulationCore::UnitStateCold> TeamfightSimulationCore::_build_unit_state(const Dictionary &spawn_spec, const StringName &team, int64_t instance_id) {
	UnitState unit;
	UnitStateCold cold;
	StringName archetype_id = StringName(String(spawn_spec.get("archetype_id", "")));
	
	// Check if this is a minion (in minion_catalog) or a champion (in champion catalog)
	Dictionary minion_data = Dictionary(_minion_catalog.get(String(archetype_id), Dictionary()));
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
	Dictionary role_config = Dictionary(_role_configs.get(role_name, Dictionary()));

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
			Dictionary entry = Dictionary(_passive_registry.get(passive_id, Dictionary()));
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
	unit.role_slot = role_slot_for_name(effective_role_name);
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
	_role_strategy_cache_by_slot.fill(UnitStrategy());
	auto put = [&](const StringName &role, UnitStrategy s) {
		int64_t slot = role_slot_for_name(role);
		if (slot >= 0) {
			_role_strategy_cache_by_slot[static_cast<size_t>(slot)] = std::move(s);
		}
	};
	auto apply_role_priorities = [](std::array<double, TeamfightSimulationCore::ROLE_SLOT_COUNT> &slots, const TeamfightSimulationCore::StrategyRolePriorities &prio_config) {
		for (const auto &prio : prio_config.enemy_priorities) {
			if (prio.role != StringName()) {
				int64_t slot = role_slot_for_name(prio.role);
				if (slot >= 0) {
					slots[static_cast<size_t>(slot)] = prio.priority;
				}
			}
		}
	};
	auto apply_ally_role_priorities = [](std::array<double, TeamfightSimulationCore::ROLE_SLOT_COUNT> &slots, const TeamfightSimulationCore::StrategyRolePriorities &prio_config) {
		for (const auto &prio : prio_config.ally_priorities) {
			if (prio.role != StringName()) {
				int64_t slot = role_slot_for_name(prio.role);
				if (slot >= 0) {
					slots[static_cast<size_t>(slot)] = prio.priority;
				}
			}
		}
	};
	auto apply_strategy_config = [&](UnitStrategy &s, const TeamfightSimulationCore::StrategyConfig &config) {
		s.display_name = config.display_name;
		s.distance_weight = config.distance_weight;
		s.hp_weight = config.hp_weight;
		s.ally_distance_weight = config.ally_distance_weight;
		s.ally_hp_weight = config.ally_hp_weight;
		s.ally_threat_weight = config.ally_threat_weight;
		s.in_range_bonus = config.in_range_bonus;
		s.threat_response_weight = config.threat_response_weight;
		s.execute_bonus_weight = config.execute_bonus_weight;
		s.spacing_weight = config.spacing_weight;
		s.threat_decay_rate = config.threat_decay_rate;
		s.switch_margin = config.switch_margin;
		s.prefers_kiting = config.prefers_kiting;
		s.uses_ally_targeting = config.uses_ally_targeting;
		apply_role_priorities(s.role_priorities, config.role_priorities);
		apply_ally_role_priorities(s.ally_role_priorities, config.role_priorities);
	};

	{
		UnitStrategy s;
		apply_strategy_config(s, TeamfightSimulationCore::get_tank_config());
		put(StringName("tank"), std::move(s));
	}
	{
		UnitStrategy s;
		apply_strategy_config(s, TeamfightSimulationCore::get_assassin_config());
		put(StringName("assassin"), std::move(s));
	}
	{
		UnitStrategy s;
		apply_strategy_config(s, TeamfightSimulationCore::get_marksman_config());
		put(StringName("marksman"), std::move(s));
	}
	{
		UnitStrategy s;
		apply_strategy_config(s, TeamfightSimulationCore::get_fighter_config());
		put(StringName("fighter"), std::move(s));
	}
	{
		UnitStrategy s;
		apply_strategy_config(s, TeamfightSimulationCore::get_mage_config());
		put(StringName("mage"), std::move(s));
	}
	{
		UnitStrategy s;
		apply_strategy_config(s, TeamfightSimulationCore::get_support_config());
		put(StringName("support"), std::move(s));
	}

	_default_strategy = UnitStrategy();
	_default_strategy.display_name = String("Default");
	_default_strategy.distance_weight = 1.0;
	_default_strategy.hp_weight = 0.0;
	_default_strategy.ally_distance_weight = 1.0;
	_default_strategy.ally_hp_weight = 0.0;
	_default_strategy.ally_threat_weight = 0.0;
	_default_strategy.in_range_bonus = 0.6;
	_default_strategy.threat_response_weight = 0.0;
	_default_strategy.execute_bonus_weight = 0.0;
	_default_strategy.threat_decay_rate = 2.0;
	_default_strategy.switch_margin = 0.75;
}

const TeamfightSimulationCore::UnitStrategy &TeamfightSimulationCore::_strategy_for_unit(const UnitState &unit) const {
	int64_t slot = unit.role_slot;
	if (slot >= 0 && slot < ROLE_SLOT_COUNT) {
		return _role_strategy_cache_by_slot[static_cast<size_t>(slot)];
	}
	return _default_strategy;
}

void TeamfightSimulationCore::_prepare_tick_context() {
	const size_t n = _units.size();
	if (_tick_ctx.density_by_unit_index.size() != n) {
		_tick_ctx.density_by_unit_index.assign(n, 0);
	}
	_tick_ctx.player_backliner_indices.clear();
	_tick_ctx.enemy_backliner_indices.clear();
	_tick_ctx.player_frontline_indices.clear();
	_tick_ctx.enemy_frontline_indices.clear();

	double pcx = 0.0;
	double pcy = 0.0;
	int pc = 0;
	for (int64_t idx : _alive_player_indices) {
		UnitState &u = _units[static_cast<size_t>(idx)];
		if (!u.alive) {
			continue;
		}
		_sync_targeting_frame_index(idx, u);
		pcx += u.pos_x;
		pcy += u.pos_y;
		pc += 1;
		if (u.is_marksman_role || u.is_mage_role || u.is_support_role) {
			_tick_ctx.player_backliner_indices.push_back(idx);
			u.is_backliner = true;
		} else {
			u.is_backliner = false;
		}
		if (u.is_tank_role || u.is_fighter_role) {
			_tick_ctx.player_frontline_indices.push_back(idx);
		}
	}
	_tick_ctx.has_player_center = pc > 0;
	if (pc > 0) {
		_tick_ctx.player_team_center = Vector2(pcx / double(pc), pcy / double(pc));
	}

	double ecx = 0.0;
	double ecy = 0.0;
	int ec = 0;
	for (int64_t idx : _alive_enemy_indices) {
		UnitState &u = _units[static_cast<size_t>(idx)];
		if (!u.alive) {
			continue;
		}
		_sync_targeting_frame_index(idx, u);
		ecx += u.pos_x;
		ecy += u.pos_y;
		ec += 1;
		if (u.is_marksman_role || u.is_mage_role || u.is_support_role) {
			_tick_ctx.enemy_backliner_indices.push_back(idx);
			u.is_backliner = true;
		} else {
			u.is_backliner = false;
		}
		if (u.is_tank_role || u.is_fighter_role) {
			_tick_ctx.enemy_frontline_indices.push_back(idx);
		}
	}
	_tick_ctx.has_enemy_center = ec > 0;
	if (ec > 0) {
		_tick_ctx.enemy_team_center = Vector2(ecx / double(ec), ecy / double(ec));
	}
	_tick_ctx.player_backliner_alive_count = int(_tick_ctx.player_backliner_indices.size());
	_tick_ctx.enemy_backliner_alive_count = int(_tick_ctx.enemy_backliner_indices.size());

	for (int64_t idx : _alive_player_indices) {
		const UnitState &u = _units[idx];
		if (!u.alive || u.target_id == 0) {
			continue;
		}
		UnitState &live_unit = _units[static_cast<size_t>(idx)];
		int64_t target_index = _target_index_for_unit(live_unit);
		if (target_index < 0) {
			continue;
		}
		UnitState &target = _units[static_cast<size_t>(target_index)];
		if (!target.alive) {
			continue;
		}
		target.incoming_target_count += 1;
		_sync_targeting_frame_index(target_index, target);
	}
	for (int64_t idx : _alive_enemy_indices) {
		const UnitState &u = _units[idx];
		if (!u.alive || u.target_id == 0) {
			continue;
		}
		UnitState &live_unit = _units[static_cast<size_t>(idx)];
		int64_t target_index = _target_index_for_unit(live_unit);
		if (target_index < 0) {
			continue;
		}
		UnitState &target = _units[static_cast<size_t>(target_index)];
		if (!target.alive) {
			continue;
		}
		target.incoming_target_count += 1;
		_sync_targeting_frame_index(target_index, target);
	}
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
	if (_spatial_stamp.size() < _units.size()) {
		_spatial_stamp.resize(_units.size(), 0);
	}
}

void TeamfightSimulationCore::_spatial_clear_buckets() const {
	for (auto &bucket : _spatial_buckets) {
		bucket.clear();
	}
}

double TeamfightSimulationCore::_spatial_cell_size() const {
	return (WORLD_BOUNDARY_MAX - WORLD_BOUNDARY_MIN) / double(SPATIAL_GRID_DIM);
}

int TeamfightSimulationCore::_spatial_flat_index(double x, double y) const {
	double cs = _spatial_cell_size();
	int ix = int(Math::floor((x - WORLD_BOUNDARY_MIN) / cs));
	int iy = int(Math::floor((y - WORLD_BOUNDARY_MIN) / cs));
	ix = CLAMP(ix, 0, SPATIAL_GRID_DIM - 1);
	iy = CLAMP(iy, 0, SPATIAL_GRID_DIM - 1);
	return iy * SPATIAL_GRID_DIM + ix;
}

void TeamfightSimulationCore::_spatial_add_alive_team(const StringName &team) const {
	const std::vector<int64_t> &indices = _alive_indices_for_team(team);
	for (int64_t idx : indices) {
		const UnitState &u = _units[idx];
		if (!u.alive) {
			continue;
		}
		int fi = _spatial_flat_index(u.pos_x, u.pos_y);
		_spatial_buckets[static_cast<size_t>(fi)].push_back(idx);
	}
}

void TeamfightSimulationCore::_spatial_next_generation() const {
	_spatial_ensure_stamp_size();
	_spatial_generation += 1;
	if (_spatial_generation == 0) {
		std::fill(_spatial_stamp.begin(), _spatial_stamp.end(), 0);
		_spatial_generation = 1;
	}
}

bool TeamfightSimulationCore::_spatial_stamp_has(int64_t unit_index) const {
	if (unit_index < 0 || unit_index >= int64_t(_spatial_stamp.size())) {
		return false;
	}
	return _spatial_stamp[static_cast<size_t>(unit_index)] == _spatial_generation;
}

void TeamfightSimulationCore::_spatial_stamp_circle(double cx, double cy, double radius, const StringName &team) const {
	_spatial_next_generation();
	if (radius <= 0.0) {
		return;
	}
	const double r2 = radius * radius;
	double cs = _spatial_cell_size();
	int icx = int(Math::floor((cx - WORLD_BOUNDARY_MIN) / cs));
	int icy = int(Math::floor((cy - WORLD_BOUNDARY_MIN) / cs));
	int span = int(Math::ceil(radius / cs)) + 1;
	for (int dy = -span; dy <= span; ++dy) {
		for (int dx = -span; dx <= span; ++dx) {
			int ix = icx + dx;
			int iy = icy + dy;
			if (ix < 0 || iy < 0 || ix >= SPATIAL_GRID_DIM || iy >= SPATIAL_GRID_DIM) {
				continue;
			}
			int fi = iy * SPATIAL_GRID_DIM + ix;
			for (int64_t idx : _spatial_buckets[static_cast<size_t>(fi)]) {
				const UnitState &u = _units[idx];
				if (!u.alive || u.team != team) {
					continue;
				}
				double ox = u.pos_x - cx;
				double oy = u.pos_y - cy;
				double dist_sq = ox * ox + oy * oy;
				// Inclusive disk (parity with AoE `_distance_between(...) <= radius`); matches refined tests below.
				if (dist_sq <= r2) {
					_spatial_stamp[static_cast<size_t>(idx)] = _spatial_generation;
				}
			}
		}
	}
}

void TeamfightSimulationCore::_spatial_stamp_separation_candidates(double cx, double cy, double radius, const StringName &team, int64_t self_instance_id) const {
	_spatial_next_generation();
	if (radius <= 0.0) {
		return;
	}
	double r2 = radius * radius;
	double cs = _spatial_cell_size();
	int icx = int(Math::floor((cx - WORLD_BOUNDARY_MIN) / cs));
	int icy = int(Math::floor((cy - WORLD_BOUNDARY_MIN) / cs));
	int span = int(Math::ceil(radius / cs)) + 1;
	for (int dy = -span; dy <= span; ++dy) {
		for (int dx = -span; dx <= span; ++dx) {
			int ix = icx + dx;
			int iy = icy + dy;
			if (ix < 0 || iy < 0 || ix >= SPATIAL_GRID_DIM || iy >= SPATIAL_GRID_DIM) {
				continue;
			}
			int fi = iy * SPATIAL_GRID_DIM + ix;
			for (int64_t idx : _spatial_buckets[static_cast<size_t>(fi)]) {
				const UnitState &u = _units[idx];
				if (!u.alive || u.team != team || u.instance_id == self_instance_id) {
					continue;
				}
				double ox = u.pos_x - cx;
				double oy = u.pos_y - cy;
				double d2 = ox * ox + oy * oy;
				if (d2 <= EPSILON || d2 >= r2) {
					continue;
				}
				_spatial_stamp[static_cast<size_t>(idx)] = _spatial_generation;
			}
		}
	}
}

void TeamfightSimulationCore::_spatial_stamp_kite_threat(double cx, double cy, double danger_radius) const {
	_spatial_next_generation();
	if (danger_radius <= 0.0) {
		return;
	}
	double danger_r2 = danger_radius * danger_radius;
	double cs = _spatial_cell_size();
	int icx = int(Math::floor((cx - WORLD_BOUNDARY_MIN) / cs));
	int icy = int(Math::floor((cy - WORLD_BOUNDARY_MIN) / cs));
	int span = int(Math::ceil(danger_radius / cs)) + 1;
	for (int dy = -span; dy <= span; ++dy) {
		for (int dx = -span; dx <= span; ++dx) {
			int ix = icx + dx;
			int iy = icy + dy;
			if (ix < 0 || iy < 0 || ix >= SPATIAL_GRID_DIM || iy >= SPATIAL_GRID_DIM) {
				continue;
			}
			int fi = iy * SPATIAL_GRID_DIM + ix;
			for (int64_t idx : _spatial_buckets[static_cast<size_t>(fi)]) {
				const UnitState &enemy = _units[idx];
				if (!enemy.alive) {
					continue;
				}
				double ex = enemy.pos_x;
				double ey = enemy.pos_y;
				double ddx = cx - ex;
				double ddy = cy - ey;
				double d2 = ddx * ddx + ddy * ddy;
				if (d2 <= EPSILON || d2 >= danger_r2) {
					continue;
				}
				_spatial_stamp[static_cast<size_t>(idx)] = _spatial_generation;
			}
		}
	}
}

void TeamfightSimulationCore::_spatial_fill_buckets_for_indices(const std::vector<int64_t> &indices) const {
	_spatial_clear_buckets();
	for (int64_t idx : indices) {
		const UnitState &u = _units[idx];
		if (!u.alive) {
			continue;
		}
		int fi = _spatial_flat_index(u.pos_x, u.pos_y);
		_spatial_buckets[static_cast<size_t>(fi)].push_back(idx);
	}
}

int TeamfightSimulationCore::_spatial_count_neighbors_in_grid(int64_t self_index, double cx, double cy, double radius) const {
	const UnitState &self = _units[self_index];
	double r2 = radius * radius;
	double cs = _spatial_cell_size();
	int icx = int(Math::floor((cx - WORLD_BOUNDARY_MIN) / cs));
	int icy = int(Math::floor((cy - WORLD_BOUNDARY_MIN) / cs));
	int span = int(Math::ceil(radius / cs)) + 1;
	int count = 0;
	for (int dy = -span; dy <= span; ++dy) {
		for (int dx = -span; dx <= span; ++dx) {
			int ix = icx + dx;
			int iy = icy + dy;
			if (ix < 0 || iy < 0 || ix >= SPATIAL_GRID_DIM || iy >= SPATIAL_GRID_DIM) {
				continue;
			}
			int fi = iy * SPATIAL_GRID_DIM + ix;
			for (int64_t idx : _spatial_buckets[static_cast<size_t>(fi)]) {
				if (idx == self_index) {
					continue;
				}
				const UnitState &other = _units[idx];
				if (!other.alive) {
					continue;
				}
				double odx = other.pos_x - self.pos_x;
				double ody = other.pos_y - self.pos_y;
				if (odx * odx + ody * ody <= r2) {
					count += 1;
				}
			}
		}
	}
	return count;
}

bool TeamfightSimulationCore::_use_spatial_broad_phase() const {
	return int64_t(_alive_player_indices.size()) >= SPATIAL_BROAD_PHASE_TEAM_THRESHOLD
			|| int64_t(_alive_enemy_indices.size()) >= SPATIAL_BROAD_PHASE_TEAM_THRESHOLD;
}

double TeamfightSimulationCore::_score_ally_target(const UnitState &unit, const TargetingFrameEntry &ally, const UnitStrategy &strategy, double unit_ally_distance) const {
	double dist = unit_ally_distance;
	if (dist < 0.0) {
		dist = _distance_between_coords(unit.pos_x, unit.pos_y, ally.pos_x, ally.pos_y);
	}
	double hp_ratio = ally.hp / Math::max(0.0001, ally.max_hp);
	double score = dist * strategy.ally_distance_weight;
	score += hp_ratio * strategy.ally_hp_weight * SCORE_HP_WEIGHT_SCALE;
	score -= ally.perceived_threat * strategy.ally_threat_weight;
	score += strategy_role_prio(strategy.ally_role_priorities, ally.role_slot);
	return score;
}

double TeamfightSimulationCore::_score_enemy_target(const UnitState &attacker, const TargetingFrameEntry &enemy, const TargetingFrameEntry *ally_for_peel, const UnitStrategy &strategy, const TickContext &ctx, const TargetScoreContext &score_ctx, double attacker_enemy_distance, bool profile_score, int64_t enemy_index, ScoreBreakdown *breakdown) {
	if (profile_score) {
		_sim_profile_se_calls += 1;
	}
	const bool attacker_is_player = attacker.team == sn_player();
	const std::vector<int64_t> &frontline_indices = enemy.is_player_team ? ctx.player_frontline_indices : ctx.enemy_frontline_indices;
	const int64_t enemy_role_slot = enemy.role_slot;

	double score = 0.0;
	double dist = 0.0;
	double attack_range = score_ctx.attack_range;
	double effective_range = score_ctx.effective_range;
	{
		SimProfileAccScope _se_base(profile_score, _sim_profile_se_base);
		// Use provided distance or compute directly
		if (attacker_enemy_distance >= 0.0) {
			dist = attacker_enemy_distance;
		} else {
			dist = _distance_between_coords(attacker.pos_x, attacker.pos_y, enemy.pos_x, enemy.pos_y);
		}
		// Python parity: melee contact uses abs tolerance only (math.isclose rel_tol=0, abs_tol=MELEE_CONTACT_BUFFER).
		// No buffer for testing
		bool in_range = false;
		if (attack_range > RANGED_THRESHOLD) {
			in_range = dist <= attack_range;
		} else {
			in_range = (dist <= effective_range); // || (Math::abs(dist - effective_range) <= MELEE_CONTACT_BUFFER);
		}
		double hp_ratio = enemy.hp / Math::max(0.0001, enemy.max_hp);
		double range_gap = Math::max(0.0, dist - Math::max(effective_range, EPSILON));
		double norm_gap = range_gap / Math::max(effective_range, EPSILON);
		// Phase 5: stabilize distance term across platforms (20-bit mantissa quantization).
		norm_gap = std::round(norm_gap * 1048576.0) / 1048576.0;
		double distance_score = Math::pow(norm_gap, DISTANCE_EXPONENT) * strategy.distance_weight * SCORE_DISTANCE_WEIGHT_SCALE;
		score += distance_score;
		double in_range_bonus = in_range ? strategy.in_range_bonus : 0.0;
		score -= in_range_bonus;
		double execute_bonus = 0.0;
		if (strategy.execute_bonus_weight > 0.0 && in_range) {
			// Execute bonus for low HP targets
			if (attacker.is_assassin_role) {
				// Assassins: scale execute bonus by missing HP percentage
				double missing_hp_factor = 1.0 - hp_ratio;
				execute_bonus = strategy.execute_bonus_weight * missing_hp_factor;
				score -= execute_bonus;
			} else if (hp_ratio <= TARGET_EXECUTE_HP_RATIO) {
				// Other roles: fixed bonus only below threshold
				execute_bonus = strategy.execute_bonus_weight;
				score -= execute_bonus;
			}
		}
		// Reactive peeling: tanks/supports prioritize enemies targeting low HP allies
		if ((attacker.is_tank_role || attacker.is_support_role) && enemy.target_id != 0) {
			UnitState *targeted_ally = _unit_by_id(enemy.target_id);
			if (targeted_ally != nullptr && targeted_ally->alive && targeted_ally->team == attacker.team) {
				double ally_hp_ratio = targeted_ally->hp / Math::max(0.0001, get_effective_max_hp(*targeted_ally));
				// Scale peel bonus by missing HP percentage (like assassin execute)
				double ally_missing_hp_factor = 1.0 - ally_hp_ratio;
				score -= REACTIVE_PEEL_BONUS * ally_missing_hp_factor;
			}
		}
		double role_prio = strategy_role_prio(strategy.role_priorities, enemy_role_slot);
		score += role_prio;
		// Assassins apply an extra tank penalty if there are backliners alive.
		if (enemy.is_tank_role && attacker.is_assassin_role) {
			int64_t enemy_self_idx = enemy_index >= 0 ? enemy_index : _unit_index_by_id(enemy.instance_id);
			const std::vector<int64_t> &bl = enemy.is_player_team ? ctx.player_backliner_indices : ctx.enemy_backliner_indices;
			int n_alive = enemy.is_player_team ? ctx.player_backliner_alive_count : ctx.enemy_backliner_alive_count;
			const UnitState &enemy_unit = _units[static_cast<size_t>(enemy_self_idx)];
			bool self_in_bl = enemy_unit.is_backliner;
			int subtract = (self_in_bl && enemy.alive) ? 1 : 0;
			if (n_alive - subtract > 0) {
				score += ASSASSIN_TANK_CONTEXT_PENALTY;
			}
		}
		double threat_response = 0.0;
		if (enemy.target_id == attacker.instance_id) {
			double falloff = 1.0 / (1.0 + Math::max(0.0, dist - attack_range) * THREAT_RESPONSE_RANGE_FALLOFF);
			threat_response = strategy.threat_response_weight * falloff;
			score -= threat_response;
		}
		double support_peel = 0.0;
		if (attacker.is_support_role) {
			int64_t ally_target_id = attacker.current_ally_target_id;
			if (ally_target_id != 0 && ally_for_peel != nullptr && ally_for_peel->alive) {
				double ally_incoming = double(ally_for_peel->incoming_target_count);
				double ally_threat = ally_for_peel->perceived_threat;
				if ((ally_incoming >= SUPPORT_PEEL_THREAT_THRESHOLD || ally_threat >= SUPPORT_PEEL_THREAT_THRESHOLD) && enemy.target_id == ally_target_id) {
					support_peel = SUPPORT_PEEL_BOOST;
					score -= support_peel;
				}
			}
		}
		
		// Debug: accumulate breakdown
		if (breakdown) {
			breakdown->distance = dist;
			breakdown->distance_weighted = distance_score;
			breakdown->hp_ratio = hp_ratio;
			breakdown->hp_weighted = hp_ratio * strategy.hp_weight * SCORE_HP_WEIGHT_SCALE;
			breakdown->role_priority = role_prio;
			breakdown->threat = enemy.perceived_threat;
			breakdown->threat_weighted = threat_response;
			breakdown->in_range_bonus = in_range_bonus;
			breakdown->execute_bonus = execute_bonus;
			breakdown->support_peel = support_peel;
		}
	}

	{
		SimProfileAccScope _se_base2(profile_score, _sim_profile_se_base);
		// Spacing awareness.
		double spacing_weight = strategy.spacing_weight;
		double spacing_score = 0.0;
		if (spacing_weight > 0.0) {
			spacing_score = Math::pow(double(enemy.incoming_target_count), SPACING_EXPONENT) * spacing_weight;
			score += spacing_score;
		}

		// Kiting tempo bonus inside preferred window.
		// Python parity: kite tempo scoring applies whenever prefers_kiting is true.
		double kiting_tempo_score = 0.0;
		if (strategy.prefers_kiting && score_ctx.has_kite_bounds) {
			double min_w = score_ctx.kite_min_w;
			double max_w = score_ctx.kite_max_w;
			if (dist >= min_w && dist <= max_w && max_w > min_w) {
				double kite_ratio = (dist - min_w) / (max_w - min_w);
				kiting_tempo_score = kite_ratio * SCORE_KITING_WEIGHT_SCALE;
				score -= kiting_tempo_score;
			}
		}
		
		if (breakdown) {
			breakdown->spacing = spacing_score;
			breakdown->kiting_tempo = kiting_tempo_score;
		}
	}

	return score;
}

bool TeamfightSimulationCore::_should_switch(const UnitState &unit, double current_score, double new_score, const TeamfightSimulationCore::UnitStrategy &strategy, double current_target_distance) const {
	// Respect post-switch lock + commit window to avoid last-moment target flip.
	if (unit.target_switch_lock_timer > 0.0) {
		return false;
	}
	// Commit window: avoid switching right before an in-range attack swing fires.
	// We approximate "in_range" using current target distance if available.
	const UnitState *current_target = _unit_by_id(unit.target_id);
	if (current_target != nullptr && current_target->alive && current_target->team != unit.team) {
		double dist = current_target_distance >= 0.0 ? current_target_distance : _distance_between(unit, *current_target);
		double attack_range = _attack_range(unit);
		double effective_range = _effective_attack_range(unit);
		bool in_range = false;
		if (attack_range > RANGED_THRESHOLD) {
			in_range = dist <= attack_range;
		} else {
			in_range = (dist <= effective_range); // No buffer for testing
		}
		if (in_range) {
			double attack_speed = Math::max(0.0001, get_effective_attack_speed(unit));
			double swing = 1.0 / attack_speed;
			double commit_window = Math::min(SWITCH_COMMIT_WINDOW_SECONDS, swing * SWITCH_COMMIT_WINDOW_SWING_FRACTION);
			if (unit.attack_cooldown <= commit_window) {
				return false;
			}
		}
	}
	double margin = strategy.switch_margin;
	return new_score <= (current_score - margin);
}

void TeamfightSimulationCore::_set_current_target(UnitState &unit, const UnitState &target) {
	int64_t old_target_id = unit.target_id;
	int64_t new_target_id = target.instance_id;
	if (old_target_id == new_target_id) {
		return;
	}
	_adjust_target_pressure(old_target_id, new_target_id);
	_emit_trace(StringName("target_switch"), unit.instance_id, new_target_id, double(old_target_id));
	unit.target_id = new_target_id;
	unit.target_index = _unit_index_by_id(new_target_id);
	_sync_targeting_frame_unit(unit);
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
	if (old_target_id == new_target_id) {
		return;
	}
	if (old_target_id != 0) {
		int64_t old_index = _unit_index_by_id(old_target_id);
		if (old_index >= 0) {
			UnitState &old_unit = _units[static_cast<size_t>(old_index)];
			old_unit.incoming_target_count = std::max<int64_t>(0, old_unit.incoming_target_count - 1);
			_sync_targeting_frame_index(old_index, old_unit);
		}
	}
	if (new_target_id != 0) {
		int64_t new_index = _unit_index_by_id(new_target_id);
		if (new_index >= 0) {
			UnitState &new_unit = _units[static_cast<size_t>(new_index)];
			new_unit.incoming_target_count += 1;
			_sync_targeting_frame_index(new_index, new_unit);
		}
	}
}

void TeamfightSimulationCore::_refresh_target_pressure() {
	for (const int64_t index : _alive_player_indices) {
		UnitState &unit = _units[index];
		int64_t target_id = unit.target_id;
		if (target_id == 0) {
			continue;
		}
		int64_t target_index = _target_index_for_unit(unit);
		if (target_index < 0) {
			continue;
		}
		UnitState &target = _units[static_cast<size_t>(target_index)];
		if (!target.alive) {
			continue;
		}
		target.incoming_target_count += 1;
		_sync_targeting_frame_index(target_index, target);
	}
	for (const int64_t index : _alive_enemy_indices) {
		UnitState &unit = _units[index];
		int64_t target_id = unit.target_id;
		if (target_id == 0) {
			continue;
		}
		int64_t target_index = _target_index_for_unit(unit);
		if (target_index < 0) {
			continue;
		}
		UnitState &target = _units[static_cast<size_t>(target_index)];
		if (!target.alive) {
			continue;
		}
		target.incoming_target_count += 1;
		_sync_targeting_frame_index(target_index, target);
	}
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
	EffectContext context;
	context.suppress_reflect_chain = false;
	context.source = &source;
	context.target = target;
	context.target_ally = target_ally;
	context.damage = damage;
	context.action_kind = action_kind;
	if (target != nullptr) {
		context.distance = _distance_between_coords(source.pos_x, source.pos_y, target->pos_x, target->pos_y);
	}
	return context;
}

int TeamfightSimulationCore::_effect_bucket_index(const StringName &kind) const {
	if (kind == sn_on_attack()) {
		return EFFECT_BUCKET_ON_ATTACK;
	}
	if (kind == sn_on_defense()) {
		return EFFECT_BUCKET_ON_DEFENSE;
	}
	if (kind == sn_on_ally_defense()) {
		return EFFECT_BUCKET_ON_ALLY_DEFENSE;
	}
	if (kind == sn_on_tick()) {
		return EFFECT_BUCKET_ON_TICK;
	}
	if (kind == sn_post_attack()) {
		return EFFECT_BUCKET_POST_ATTACK;
	}
	if (kind == sn_post_take_damage()) {
		return EFFECT_BUCKET_POST_TAKE_DAMAGE;
	}
	if (kind == sn_on_ability()) {
		return EFFECT_BUCKET_ON_ABILITY;
	}
	if (kind == sn_on_ultimate()) {
		return EFFECT_BUCKET_ON_ULTIMATE;
	}
	if (kind == sn_post_heal()) {
		return EFFECT_BUCKET_POST_HEAL;
	}
	if (kind == sn_on_takedown()) {
		return EFFECT_BUCKET_ON_TAKEDOWN;
	}
	return -1;
}

const std::vector<TeamfightSimulationCore::EffectRecord> &TeamfightSimulationCore::_collect_effects(const UnitState &unit, const StringName &kind) {
	static const std::vector<EffectRecord> EMPTY_EFFECTS;
	int bucket = _effect_bucket_index(kind);
	if (bucket >= 0 && bucket < 10) {
		return _uc(unit).passive_effects[static_cast<size_t>(bucket)];
	}
	return EMPTY_EFFECTS;
}

bool TeamfightSimulationCore::_target_has_status(const UnitState &target, const StringName &status_kind) const {
	if (status_kind == sn_slow()) {
		return target.slow_remaining > 0.0;
	}
	if (status_kind == sn_root()) {
		return target.root_remaining > 0.0;
	}
	if (status_kind == sn_silence()) {
		return target.silence_remaining > 0.0;
	}
	if (status_kind == sn_disarm()) {
		return target.disarm_remaining > 0.0;
	}
	if (status_kind == sn_stealth()) {
		return target.stealth_remaining > 0.0;
	}
	if (status_kind == sn_stun()) {
		return target.stun_remaining > 0.0;
	}
	if (status_kind == sn_reflect()) {
		return !_uc(target).reflect_buffs.empty() || target.reflect_passive_pct_all > 0.0 || target.reflect_passive_pct_physical > 0.0;
	}
	return false;
}

double TeamfightSimulationCore::_evaluate_multiplier_effect(const EffectRecord &effect, const EffectContext &context, double current_value) {
	(void)current_value;
	switch (effect.opcode) {
		case EFFECT_OPCODE_CONSTANT_MULTIPLIER:
			return effect.scalar0;
		case EFFECT_OPCODE_HP_THRESHOLD_DAMAGE_MULTIPLIER: {
			// Check source HP ratio for above_threshold
			if (effect.scalar0 > 0.0) {
				double hp_ratio = context.source->hp / Math::max(0.0001, get_effective_max_hp(*context.source));
				if (hp_ratio > effect.scalar0) {
					return effect.scalar2;
				}
			}
			// Check target HP ratio for below_threshold
			if (effect.scalar1 > 0.0 && context.target != nullptr) {
				double target_hp = context.target->hp;
				double target_max_hp = Math::max(0.0001, get_effective_max_hp(*context.target));
				if (target_hp / target_max_hp <= effect.scalar1) {
					return effect.scalar2;
				}
			}
			return 1.0;
		}
		case EFFECT_OPCODE_DISTANCE_THRESHOLD_MULTIPLIER:
			return context.distance > effect.scalar0 ? effect.scalar1 : 1.0;
		case EFFECT_OPCODE_AUTO_DODGE:
			if (_randf() < effect.scalar0) {
				return effect.scalar1;
			}
			return effect.scalar2;
		case EFFECT_OPCODE_TARGET_STATUS_MULTIPLIER:
			if (context.target == nullptr) {
				return 1.0;
			}
			return _target_has_status(*context.target, effect.damage_type) ? effect.scalar0 : 1.0;
		default:
			return 1.0;
	}
}

double TeamfightSimulationCore::_defense_multiplier(UnitState &target, UnitState &source, double damage, const StringName &action_kind) {
	double multiplier = 1.0;
	EffectContext context = _build_context(source, &target, nullptr, damage, action_kind);
	const std::vector<EffectRecord> &effects = _uc(target).passive_effects[EFFECT_BUCKET_ON_DEFENSE];
	for (const EffectRecord &effect : effects) {
		if (effect.opcode == EFFECT_OPCODE_AUTO_DODGE) {
			continue;
		}
		if (effect.opcode == EFFECT_OPCODE_STAT_MODIFIER) {
			// Execute stat_modifier effects before damage calculation
			// For on_defense, the target (defender) should be the source of the stat_modifier
			EffectContext stat_context = context;
			stat_context.source = &target;
			stat_context.target = &target;
			_execute_effect(effect, stat_context);
			continue;
		}
		multiplier *= _evaluate_multiplier_effect(effect, context, multiplier);
	}
	return multiplier;
}

double TeamfightSimulationCore::_auto_dodge_multiplier(UnitState &target, UnitState &source, double damage) {
	double multiplier = 1.0;
	EffectContext context = _build_context(source, &target, nullptr, damage, sn_auto());
	const std::vector<EffectRecord> &effects = _uc(target).passive_effects[EFFECT_BUCKET_ON_DEFENSE];
	for (const EffectRecord &effect : effects) {
		if (effect.opcode != EFFECT_OPCODE_AUTO_DODGE) {
			continue;
		}
		multiplier *= _evaluate_multiplier_effect(effect, context, multiplier);
	}
	return multiplier;
}

double TeamfightSimulationCore::_apply_attack_modifiers(UnitState &unit, UnitState &target, double distance, double damage) {
	(void)distance;
	EffectContext context = _build_context(unit, &target, nullptr, damage, sn_auto());
	const std::vector<EffectRecord> &effects = _uc(unit).passive_effects[EFFECT_BUCKET_ON_ATTACK];
	double modified_damage = damage;
	for (const EffectRecord &effect : effects) {
		modified_damage *= _evaluate_multiplier_effect(effect, context, modified_damage);
	}
	return modified_damage;
}

double TeamfightSimulationCore::_apply_ability_modifiers(UnitState &unit, UnitState *target, double damage) {
	EffectContext context = _build_context(unit, target, nullptr, damage, sn_ability());
	const std::vector<EffectRecord> &effects = _uc(unit).passive_effects[EFFECT_BUCKET_ON_ABILITY];
	double modified_damage = damage;
	for (const EffectRecord &effect : effects) {
		modified_damage *= _evaluate_multiplier_effect(effect, context, modified_damage);
	}
	return modified_damage;
}

double TeamfightSimulationCore::_apply_ultimate_modifiers(UnitState &unit, UnitState *target, double damage) {
	EffectContext context = _build_context(unit, target, nullptr, damage, sn_ultimate());
	const std::vector<EffectRecord> &effects = _uc(unit).passive_effects[EFFECT_BUCKET_ON_ULTIMATE];
	double modified_damage = damage;
	for (const EffectRecord &effect : effects) {
		modified_damage *= _evaluate_multiplier_effect(effect, context, modified_damage);
	}
	return modified_damage;
}

double TeamfightSimulationCore::_apply_damage(UnitState &source, UnitState &target, double damage, const StringName &damage_type, const StringName &action_kind, const EffectContext &context) {
	if (damage <= 0.0) {
		return 0.0;
	}
	
	const bool action_is_auto = action_kind == sn_auto();
	const bool action_is_ability = action_kind == sn_ability();
	const bool action_is_ultimate = action_kind == sn_ultimate();
	const bool action_is_passive = action_kind == sn_passive();
	const bool damage_is_physical = damage_type == sn_physical();
	const bool damage_is_magic = damage_type == sn_magic();
	double old_hp = target.hp;
	double pre_res = damage;
	
	// Apply defense multipliers only for non-true damage
	if (damage_type != sn_true()) {
		pre_res *= _defense_multiplier(target, source, damage, action_kind);
	}
	
	// Auto-dodge applies to all damage types (including true) for auto-attacks
	if (action_is_auto) {
		pre_res *= _auto_dodge_multiplier(target, source, damage);
	}
	
	// Trigger on_ally_defense effects to calculate damage reduction before applying damage
	// This allows allies to reduce damage to the target via reduction_ratio
	double ally_defense_reduction = _trigger_ally_defense_effects(target, source, pre_res, damage_type, action_kind, context);
	// Apply the reduction to the damage
	pre_res -= ally_defense_reduction;
	if (pre_res < 0.0) {
		pre_res = 0.0;
	}
	
	double final_damage = pre_res;
	if (damage_is_physical) {
		double armor = get_effective_armor(target);
		final_damage *= (1.0 - armor);
	} else if (damage_is_magic) {
		double mr = get_effective_magic_resist(target);
		final_damage *= (1.0 - mr);
	}
	double incoming = Math::max(0.0, final_damage);
	
	if (incoming <= 0.0) {
		return 0.0;
	}
	double shield_before = target.shield;
	double absorbed = Math::min(shield_before, incoming);
	target.shield = Math::max(0.0, shield_before - absorbed);
	double hp_loss = Math::max(0.0, incoming - absorbed);
	target.hp = Math::max(0.0, target.hp - hp_loss);
	_uc(target).damage_received += hp_loss;
	_uc(target).damage_mitigated += Math::max(0.0, pre_res - final_damage);
	double total_damage = absorbed + hp_loss;
	double max_hp = get_effective_max_hp(target);
	// Python parity: threat burst uses post-shield hp_loss (final_damage after absorption).
	if (max_hp > 0.0 && hp_loss > max_hp * THREAT_BURST_THRESHOLD) {
		target.perceived_threat += (hp_loss / max_hp) * THREAT_BURST_MULTIPLIER;
	}
	_sync_targeting_frame_unit(target);
	// Self-inflicted damage should not count as damage dealt.
	if (source.instance_id != target.instance_id) {
		_uc(source).damage_dealt += total_damage;
		if (action_is_auto) {
			_uc(source).damage_dealt_auto += total_damage;
		} else if (action_is_ability) {
			_uc(source).damage_dealt_ability += total_damage;
		} else if (action_is_ultimate) {
			_uc(source).damage_dealt_ultimate += total_damage;
		} else if (action_is_passive) {
			_uc(source).damage_dealt_passive += total_damage;
		}
	}
	if (hp_loss > 0.0 || absorbed > 0.0) {
		_touch_damage_source(target, source.instance_id, total_damage);
		_uc(target).last_hit_time = _time;
	}
	if (total_damage > 1e-9) {
		_viewer_record_damage_fx(source, target, total_damage, action_kind, damage_type);
	}
	_maybe_apply_reflect_damage(source, target, total_damage, damage_type, context);
	if (target.hp <= 0.0) {
		const std::vector<EffectRecord> &post_take_damage_effects = _uc(target).passive_effects[EFFECT_BUCKET_POST_TAKE_DAMAGE];
		EffectContext post_context = context;
		// Python parity: post_take_damage passives run in the defender's own context (ctx.unit = defender).
		post_context.source = &target;
		post_context.target = nullptr;
		post_context.damage = total_damage;
		post_context.action_kind = sn_passive();
		if (target.stealth_remaining > 0.0 && target.stealth_break_on_damage_taken) {
			target.stealth_remaining = 0.0;
			target.stealth_break_on_attack = false;
			target.stealth_break_on_ability = false;
			target.stealth_break_on_damage_taken = false;
		}
		for (const EffectRecord &effect : post_take_damage_effects) {
			_execute_effect(effect, post_context);
		}
		_handle_death(source, target);
		return total_damage;
	}
	const std::vector<EffectRecord> &post_take_damage_effects = _uc(target).passive_effects[EFFECT_BUCKET_POST_TAKE_DAMAGE];
	EffectContext post_context = context;
	// Python parity: post_take_damage passives run in the defender's own context (ctx.unit = defender).
	post_context.source = &target;
	post_context.target = nullptr;
	post_context.damage = total_damage;
	post_context.action_kind = sn_passive();
	// Break stealth on damage taken if configured
	if (target.stealth_remaining > 0.0 && target.stealth_break_on_damage_taken) {
		target.stealth_remaining = 0.0;
		target.stealth_break_on_attack = false;
		target.stealth_break_on_ability = false;
		target.stealth_break_on_damage_taken = false;
	}
	for (const EffectRecord &effect : post_take_damage_effects) {
		_execute_effect(effect, post_context);
	}
	return total_damage;
}

void TeamfightSimulationCore::_maybe_apply_reflect_damage(UnitState &attacker, UnitState &defender, double total_damage_applied, const StringName &damage_type, const EffectContext &context) {
	if (context.suppress_reflect_chain) {
		return;
	}
	if (!attacker.alive || !defender.alive || attacker.instance_id == defender.instance_id) {
		return;
	}
	if (total_damage_applied <= 1e-9) {
		return;
	}

	// Handle passive reflect (from passive effects)
	double passive_pct = defender.reflect_passive_pct_all;
	if (damage_type == sn_physical()) {
		passive_pct += defender.reflect_passive_pct_physical;
	}
	if (passive_pct > 1e-9) {
		double reflected = total_damage_applied * passive_pct;
		if (reflected > 1e-9) {
			EffectContext bounce = context;
			bounce.suppress_reflect_chain = true;
			bounce.source = &defender;
			bounce.target = &attacker;
			_apply_damage(defender, attacker, reflected, damage_type, sn_passive(), bounce);
		}
	}

	// Handle active reflect buffs (from abilities/ultimates)
	for (const UnitStateCold::ReflectBuff &buff : _uc(defender).reflect_buffs) {
		if (buff.remaining_duration <= 0.0) {
			continue;
		}

		// Check if this buff applies to the damage type
		bool applies = false;
		if (buff.damage_type == StringName("all")) {
			applies = true;
		} else if (buff.damage_type == StringName("physical") && damage_type == sn_physical()) {
			applies = true;
		}

		if (!applies) {
			continue;
		}

		double reflected = total_damage_applied * buff.percentage;
		if (reflected <= 1e-9) {
			continue;
		}

		// Determine source: use buff.source_instance_id if available, otherwise defender
		UnitState *damage_source = _unit_by_id(buff.source_instance_id);
		if (damage_source == nullptr || !damage_source->alive) {
			damage_source = &defender;
		}

		EffectContext bounce = context;
		bounce.suppress_reflect_chain = true;
		bounce.source = damage_source;
		bounce.target = &attacker;
		_apply_damage(*damage_source, attacker, reflected, damage_type, buff.action_kind, bounce);
	}
}

void TeamfightSimulationCore::_touch_damage_source(UnitState &target, int64_t source_id, double incoming_damage) {
	if (source_id <= 0 || incoming_damage <= 0.0) {
		return;
	}
	// Don't track self-damage for assist tracking
	if (source_id == target.instance_id) {
		return;
	}
	UnitStateCold::DamageSourceEntry &entry = _uc(target).damage_sources[source_id];
	entry.damage += incoming_damage;
	entry.last_time = _time;
}

double TeamfightSimulationCore::_trigger_ally_defense_effects(UnitState &target, UnitState &source, double damage, const StringName &damage_type, const StringName &action_kind, const EffectContext &context) {
	// Prevent infinite recursion: if we're already processing ally defense for this target, skip
	if (context.suppress_reflect_chain) {
		return 0.0;
	}
	
	double total_redirected = 0.0;  // Track total damage redirected away from original target
	StringName target_team = target.team;
	const std::vector<int64_t> &ally_indices = _alive_indices_for_team(target_team);
	
	for (int64_t ally_index : ally_indices) {
		UnitState &ally = _units[static_cast<size_t>(ally_index)];
		if (!ally.alive || ally.instance_id == target.instance_id) {
			continue;
		}
		
		// Check if ally has on_ally_defense effects
		const std::vector<EffectRecord> &ally_defense_effects = _uc(ally).passive_effects[EFFECT_BUCKET_ON_ALLY_DEFENSE];
		if (ally_defense_effects.empty()) {
			continue;
		}
		
		// Check radius constraint (use stored radius from passive definition)
		double ally_radius = _uc(ally).on_ally_defense_radius;
		if (ally_radius <= 0.0) {
			continue;  // No radius defined, skip
		}
		double dx = ally.pos_x - target.pos_x;
		double dy = ally.pos_y - target.pos_y;
		double dist_sq = dx * dx + dy * dy;
		double radius_sq = ally_radius * ally_radius;
		// Save dx, dy for distance calculation in context
		double adx = dx;
		double ady = dy;
		
		if (dist_sq > radius_sq) {
			continue;
		}
		
		// Build context for ally defense effects
		EffectContext ally_context = context;
		ally_context.source = &ally;
		ally_context.target = &target;
		ally_context.target_ally = nullptr;  // ally is the source, not a target ally
		ally_context.damage = damage;
		ally_context.action_kind = action_kind;
		// Calculate distance from ally to target for effects that might use it
		ally_context.distance = _distance_between_coords(ally.pos_x, ally.pos_y, target.pos_x, target.pos_y);
		
		// Execute ally defense effects
		for (const EffectRecord &effect : ally_defense_effects) {
			// Handle redirect_damage effect
			if (effect.opcode == EFFECT_OPCODE_REDIRECT_DAMAGE) {
				double redirect_ratio = effect.scalar0;
				double reduction_ratio = effect.scalar1;
				double redirect_cap = effect.scalar2;
				
				// Calculate redirected damage (capped at flat redirect_cap, 0 means no cap)
				double redirected_damage = damage * redirect_ratio;
				if (redirect_cap > 0.0) {
					double max_redirect = redirect_cap;  // flat value cap
					if (redirected_damage > max_redirect) {
						redirected_damage = max_redirect;
					}
				}
				
				// Track total redirected damage to reduce original target's damage
				total_redirected += redirected_damage;
				
				// Apply reduction to redirected damage (reduces damage the guardian takes)
				double mitigated_damage = redirected_damage * (1.0 - reduction_ratio);
				
				// Apply mitigated damage to the ally defender
				if (mitigated_damage > 1e-9) {
					// Track mitigated damage in stats
					double mitigated_amount = redirected_damage - mitigated_damage;
					_uc(ally).damage_mitigated += mitigated_amount;
					
					EffectContext redirect_context = context;
					redirect_context.source = &source;
					redirect_context.target = &ally;
					redirect_context.damage = mitigated_damage;
					redirect_context.action_kind = action_kind;
					redirect_context.suppress_reflect_chain = true;  // Prevent infinite reflection loops
					// Calculate distance from source to ally for effects that might use it
					double rdx = source.pos_x - ally.pos_x;
					double rdy = source.pos_y - ally.pos_y;
					redirect_context.distance = _distance_between_coords(source.pos_x, source.pos_y, ally.pos_x, ally.pos_y);
					_apply_damage(source, ally, mitigated_damage, damage_type, sn_passive(), redirect_context);
				}
			} else {
				Dictionary result = _execute_effect(effect, ally_context);
			}
		}
	}
	
	return total_redirected;  // Return redirected damage to reduce original target's damage
}

void TeamfightSimulationCore::_apply_stun(UnitState &source, UnitState &target, double duration) {
	if (duration <= 0.0) {
		return;
	}
	// Python parity: apply tenacity to reduce stun duration.
	double tenacity = get_effective_tenacity(target);
	double effective_duration = duration * (1.0 - tenacity);
	if (effective_duration <= 0.0) {
		return;
	}
	target.stun_remaining = Math::max(target.stun_remaining, effective_duration);
	_uc(source).stuns += 1;
}

void TeamfightSimulationCore::_apply_slow(UnitState &source, UnitState &target, double slow_percentage, double duration) {
	(void)source;
	if (duration <= 0.0 || slow_percentage <= 0.0) {
		return;
	}
	double tenacity = get_effective_tenacity(target);
	double effective_duration = duration * (1.0 - tenacity);
	if (effective_duration <= 0.0) {
		return;
	}
	double pct = Math::clamp(slow_percentage, 0.0, 1.0);
	double mult = Math::clamp(1.0 - pct, SLOW_MOVEMENT_MULTIPLIER_MIN, 1.0);
	target.slow_remaining = Math::max(target.slow_remaining, effective_duration);
	target.slow_move_mult = Math::min(target.slow_move_mult, mult);
}

void TeamfightSimulationCore::_apply_root(UnitState &source, UnitState &target, double duration) {
	(void)source;
	if (duration <= 0.0) {
		return;
	}
	double tenacity = get_effective_tenacity(target);
	double effective_duration = duration * (1.0 - tenacity);
	if (effective_duration <= 0.0) {
		return;
	}
	target.root_remaining = Math::max(target.root_remaining, effective_duration);
}

void TeamfightSimulationCore::_apply_silence(UnitState &source, UnitState &target, double duration, bool block_abilities, bool block_ultimate) {
	(void)source;
	if (duration <= 0.0) {
		return;
	}
	double tenacity = get_effective_tenacity(target);
	double effective_duration = duration * (1.0 - tenacity);
	if (effective_duration <= 0.0) {
		return;
	}
	if (block_abilities) {
		target.silence_ability_remaining = Math::max(target.silence_ability_remaining, effective_duration);
	}
	if (block_ultimate) {
		target.silence_ultimate_remaining = Math::max(target.silence_ultimate_remaining, effective_duration);
	}
	target.silence_remaining = Math::max(target.silence_ability_remaining, target.silence_ultimate_remaining);
	target.silence_blocks_abilities = target.silence_ability_remaining > 0.0;
	target.silence_blocks_ultimates = target.silence_ultimate_remaining > 0.0;
}

void TeamfightSimulationCore::_apply_disarm(UnitState &source, UnitState &target, double duration) {
	(void)source;
	if (duration <= 0.0) {
		return;
	}
	double tenacity = get_effective_tenacity(target);
	double effective_duration = duration * (1.0 - tenacity);
	if (effective_duration <= 0.0) {
		return;
	}
	target.disarm_remaining = Math::max(target.disarm_remaining, effective_duration);
}

void TeamfightSimulationCore::_apply_stealth(UnitState &source, UnitState &target, double duration, bool break_on_attack, bool break_on_ability, bool break_on_damage_taken) {
	(void)source;
	if (duration <= 0.0) {
		return;
	}
	// Refresh stealth duration (re-application allowed)
	target.stealth_remaining = duration;
	target.stealth_break_on_attack = break_on_attack;
	target.stealth_break_on_ability = break_on_ability;
	target.stealth_break_on_damage_taken = break_on_damage_taken;
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
	if (duration <= 0.0 || pct <= 0.0) {
		return;
	}
	double p = Math::clamp(pct, 0.0, 1.0);
	
	UnitStateCold::ReflectBuff new_buff;
	new_buff.percentage = p;
	new_buff.remaining_duration = duration;
	new_buff.action_kind = action_kind;
	new_buff.source_instance_id = source.instance_id;
	new_buff.damage_type = damage_type;
	new_buff.reason = reason;
	
	_uc(target).reflect_buffs.push_back(new_buff);
}

void TeamfightSimulationCore::_apply_aoe_reflect(UnitState &source, double radius, double pct, double duration, bool all_damage_types) {
	EffectRecord effect;
	effect.aoe_shape_params.shape = AoShapeKind::Circle;
	effect.aoe_shape_params.anchor = AoAnchorKind::Self;
	effect.aoe_shape_params.radius = radius;
	_apply_aoe_reflect_shape(source, nullptr, effect, pct, duration, all_damage_types, StringName("ability"), String("aoe_reflect"));
}

bool TeamfightSimulationCore::_apply_knockback(UnitState &source, UnitState &target, double distance, bool away_from_source) {
	if (distance <= 0.0 || !target.alive) {
		return false;
	}
	double tenacity = get_effective_tenacity(target);
	double effective_distance = distance * (1.0 - tenacity);
	if (effective_distance <= EPSILON) {
		return false;
	}
	double old_x = target.pos_x;
	double old_y = target.pos_y;
	double sx = source.pos_x;
	double sy = source.pos_y;
	double tx = target.pos_x;
	double ty = target.pos_y;
	double dx = tx - sx;
	double dy = ty - sy;
	double dist = _distance_between_coords(sx, sy, tx, ty);
	double nx = 0.0;
	double ny = 0.0;
	if (dist <= EPSILON) {
		nx = (tx >= WORLD_SIZE * 0.5) ? 1.0 : -1.0;
		ny = 0.0;
	} else {
		nx = dx / dist;
		ny = dy / dist;
	}
	if (!away_from_source) {
		nx = -nx;
		ny = -ny;
	}
	double new_x = tx + nx * effective_distance;
	double new_y = ty + ny * effective_distance;
	Vector2 valid = _find_valid_dash_position(tx, ty, new_x, new_y, effective_distance, target.instance_id);
	target.pos_x = Math::clamp(static_cast<double>(valid.x), WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
	target.pos_y = Math::clamp(static_cast<double>(valid.y), WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
	_sync_targeting_frame_unit(target);
	return Math::abs(target.pos_x - old_x) > EPSILON || Math::abs(target.pos_y - old_y) > EPSILON;
}

bool TeamfightSimulationCore::_apply_aoe_knockback(UnitState &source, double radius, double distance, bool away_from_source) {
	EffectRecord effect;
	effect.aoe_shape_params.shape = AoShapeKind::Circle;
	effect.aoe_shape_params.anchor = AoAnchorKind::Self;
	effect.aoe_shape_params.radius = radius;
	return _apply_aoe_knockback_shape(source, nullptr, effect, distance, away_from_source);
}

double TeamfightSimulationCore::_movement_speed_multiplier(const UnitState &unit) const {
	return Math::clamp(unit.slow_move_mult, SLOW_MOVEMENT_MULTIPLIER_MIN, 1.0);
}

void TeamfightSimulationCore::_add_shield(UnitState &source, UnitState &target, double amount, const StringName &action_kind) {
	if (amount <= 0.0) {
		return;
	}
	target.shield += amount;
	if (amount > 1e-9) {
		_viewer_record_shield_fx(target, amount);
	}
	_uc(source).shielding_done += amount;
	if (action_kind == sn_auto()) {
		_uc(source).shielding_done_auto += amount;
	} else if (action_kind == sn_ability()) {
		_uc(source).shielding_done_ability += amount;
	} else if (action_kind == sn_ultimate()) {
		_uc(source).shielding_done_ultimate += amount;
	} else if (action_kind == sn_passive()) {
		_uc(source).shielding_done_passive += amount;
	}
	if (source.instance_id != target.instance_id) {
		_uc(target).recent_benefactors[source.instance_id] = _time;
	}
}

void TeamfightSimulationCore::_heal_unit(UnitState &source, UnitState &target, double amount, const StringName &action_kind, bool allow_overheal) {
	if (amount <= 0.0) {
		return;
	}
	
	double max_hp = get_effective_max_hp(target);
	double old_hp = target.hp;
	double new_hp;
	if (allow_overheal) {
		new_hp = old_hp + amount;
	} else {
		new_hp = Math::min(max_hp, old_hp + amount);
	}
	
	target.hp = new_hp;
	double gained = new_hp - old_hp;
	
	if (gained > 1e-9) {
		_viewer_record_heal_fx(target, gained);
	}
	_sync_targeting_frame_unit(target);
	_uc(source).healing_done += gained;
	if (action_kind == sn_auto()) {
		_uc(source).healing_done_auto += gained;
	} else if (action_kind == sn_ability()) {
		_uc(source).healing_done_ability += gained;
	} else if (action_kind == sn_ultimate()) {
		_uc(source).healing_done_ultimate += gained;
	} else if (action_kind == sn_passive()) {
		_uc(source).healing_done_passive += gained;
	}
	if (source.instance_id != target.instance_id) {
		_uc(target).recent_benefactors[source.instance_id] = _time;
	}
}

void TeamfightSimulationCore::_restore_mana(UnitState &source, UnitState &target, double amount) {
	if (amount <= 0.0) {
		return;
	}
	double max_mana = get_effective_max_mana(target);
	target.mana = Math::min(max_mana, target.mana + amount);
	(void)source;
}

void TeamfightSimulationCore::_apply_stat_modifier(UnitState &source, UnitState &target, StringName stat_name, double additive, double multiplicative, double duration, bool is_match_duration) {
	if (additive == 0.0 && multiplicative == 1.0) {
		return;
	}
	
	// Validate and clamp dangerous values
	if (additive < -10000.0) {
		additive = -10000.0; // Prevent extreme negative additive values
	}
	if (multiplicative < 0.0) {
		multiplicative = 0.0; // Prevent negative multiplication
	}
	if (multiplicative > 1000.0) {
		multiplicative = 1000.0; // Prevent extreme values
	}
	if (multiplicative == 0.0) {
		multiplicative = 0.0001; // Prevent permanent stat zeroing
	}
	if (duration < 0.0) {
		duration = 0.0; // Prevent negative durations
	}
	
	// Apply modifiers based on stat name - generated by X-Macro
#define X(name, def, min_val, max_val) \
	if (stat_name == StringName(#name)) { \
		target.stat_additive_##name += additive; \
		target.stat_multiplicative_##name *= multiplicative; \
		if (is_match_duration) { \
			double effective_duration = (duration > 0.0) ? duration : MATCH_DURATION; \
			target.stat_perm_##name = Math::max(target.stat_perm_##name, effective_duration); \
		} else if (duration > 0.0) { \
			target.stat_temp_##name = Math::max(target.stat_temp_##name, duration); \
		} \
		return; \
	}
	STAT_LIST
#undef X
}

void TeamfightSimulationCore::_apply_simple_stat_modifier(UnitState &source, UnitState &target, StringName stat_name, double additive, double multiplicative, double duration, bool is_match_duration, const String &reason) {
	if (additive == 0.0 && multiplicative == 1.0) {
		return;
	}
	if (duration <= 0.0) {
		_apply_stat_modifier(source, target, stat_name, additive, multiplicative, duration, is_match_duration);
		return;
	}

	String modifier_key = _get_stack_key(stat_name, reason);
	Dictionary existing = Dictionary(target.stat_modifiers.get(modifier_key, Dictionary()));
	if (!existing.is_empty()) {
		double previous_additive = double(existing.get("additive", 0.0));
		double previous_multiplicative = double(existing.get("multiplicative", 1.0));
		double inverse_multiplicative = previous_multiplicative != 0.0 ? 1.0 / previous_multiplicative : 1.0;
		_apply_stat_modifier(source, target, stat_name, -previous_additive, inverse_multiplicative, 0.0, false);
	}

	_apply_stat_modifier(source, target, stat_name, additive, multiplicative, 0.0, false);

	Dictionary entry;
	entry["stat_name"] = stat_name;
	entry["additive"] = additive;
	entry["multiplicative"] = multiplicative;
	entry["duration"] = duration;
	entry["is_match_duration"] = is_match_duration;
	target.stat_modifiers[modifier_key] = entry;
}

void TeamfightSimulationCore::_set_stat_modifier_duration(UnitState &unit, StringName stat_name, double duration, bool is_match_duration) {
	if (duration < 0.0) {
		duration = 0.0;
	}
	// Set stat modifier duration - generated by X-Macro
#define X(name, def, min_val, max_val) \
	if (stat_name == StringName(#name)) { \
		if (is_match_duration) { \
			unit.stat_perm_##name = Math::max(unit.stat_perm_##name, duration); \
		} else { \
			unit.stat_temp_##name = Math::max(unit.stat_temp_##name, duration); \
		} \
		return; \
	}
	STAT_LIST
#undef X
}

void TeamfightSimulationCore::_clear_all_stat_modifiers(UnitState &unit) {
	// Clear only temporary stat modifiers (respawn-duration)
	// Preserve match-duration stat modifiers (stat_perm_* fields and their additive/multiplicative values)
#define X(name, def, min_val, max_val) \
	unit.stat_temp_##name = 0.0;
	STAT_LIST
#undef X
	
	// Clear stack tracking
	unit.stat_stacks.clear();
	unit.stat_modifiers.clear();
}

bool TeamfightSimulationCore::_is_valid_stat_name(const StringName &stat_name) const {
	// List of all valid stat names - generated by X-Macro
#define X(name, def, min_val, max_val) \
	if (stat_name == StringName(#name)) return true;
	STAT_LIST
#undef X
	return false;
}

void TeamfightSimulationCore::_update_stat_modifier_durations(UnitState &unit, double delta) {
	Array keys_to_remove;
	Array modifier_keys = unit.stat_modifiers.keys();
	for (int i = 0; i < modifier_keys.size(); i++) {
		Variant key_variant = modifier_keys[i];
		if (key_variant.get_type() != Variant::STRING) {
			continue;
		}
		String key = key_variant;
		Dictionary modifier = unit.stat_modifiers[key];
		bool is_match_duration = bool(modifier.get("is_match_duration", false));
		if (is_match_duration) {
			continue;
		}
		double duration = double(modifier.get("duration", 0.0)) - delta;
		if (duration > 0.0) {
			modifier["duration"] = duration;
			unit.stat_modifiers[key] = modifier;
			continue;
		}
		StringName stat_name = StringName(String(modifier.get("stat_name", "")));
		double additive = double(modifier.get("additive", 0.0));
		double multiplicative = double(modifier.get("multiplicative", 1.0));
		double inverse_multiplicative = multiplicative != 0.0 ? 1.0 / multiplicative : 1.0;
		_apply_stat_modifier(unit, unit, stat_name, -additive, inverse_multiplicative, 0.0, false);
		keys_to_remove.append(key);
	}
	for (int i = 0; i < keys_to_remove.size(); i++) {
		unit.stat_modifiers.erase(String(keys_to_remove[i]));
	}

	// Update temporary modifier durations - generated by X-Macro
#define X(name, def, min_val, max_val) \
	unit.stat_temp_##name = Math::max(0.0, unit.stat_temp_##name - delta);
	STAT_LIST
#undef X
}

void TeamfightSimulationCore::_clear_expired_stat_modifiers(UnitState &unit) {
	// Clear expired temporary modifiers (but preserve match-duration modifiers) - generated by X-Macro
#define X(name, def, min_val, max_val) \
	if (unit.stat_temp_##name <= 0.0 && unit.stat_perm_##name <= 0.0) { \
		unit.stat_additive_##name = 0.0; \
		unit.stat_multiplicative_##name = 1.0; \
	}
	STAT_LIST
#undef X
}

void TeamfightSimulationCore::_apply_stacked_stat_modifier(UnitState &source, UnitState &target, StringName stat_name, double additive, double multiplicative, double duration, bool is_match_duration, int max_stacks, StackBehavior stack_behavior, const String &reason) {
	if (max_stacks <= 0) {
		return;
	}

	String stack_key = _get_stack_key(stat_name, reason);
	Dictionary stack_entry = Dictionary(target.stat_stacks.get(stack_key, Dictionary()));
	int current_stacks = int(stack_entry.get("current_stacks", 0));
	double previous_additive = double(stack_entry.get("applied_additive", 0.0));
	double previous_multiplicative = double(stack_entry.get("applied_multiplicative", 1.0));
	double previous_duration = double(stack_entry.get("duration", 0.0));

	if (current_stacks > 0) {
		double inverse_multiplicative = previous_multiplicative != 0.0 ? 1.0 / previous_multiplicative : 1.0;
		_apply_stat_modifier(source, target, stat_name, -previous_additive, inverse_multiplicative, 0.0, false);
		
		// Reset temp tracker when undoing previous modifiers
		_reset_stat_temp_tracker(target, stat_name);
	}

	if (stack_behavior == StackBehavior::Reset) {
		current_stacks = 0;
		previous_duration = 0.0;
	}

	if (current_stacks < max_stacks) {
		current_stacks++;
	}

	double applied_additive = additive * double(current_stacks);
	double applied_multiplicative = 1.0;
	if (!Math::is_equal_approx(multiplicative, 1.0)) {
		applied_multiplicative = Math::pow(multiplicative, double(current_stacks));
	}
	_apply_stat_modifier(source, target, stat_name, applied_additive, applied_multiplicative, 0.0, false);

	double next_duration = duration;
	if (stack_behavior == StackBehavior::Accumulate) {
		next_duration = previous_duration + duration;
	}
	if (next_duration < 0.0) {
		next_duration = 0.0;
	}

	stack_entry["current_stacks"] = current_stacks;
	stack_entry["max_stacks"] = max_stacks;
	stack_entry["duration"] = next_duration;
	stack_entry["additive_per_stack"] = additive;
	stack_entry["multiplicative_per_stack"] = multiplicative;
	stack_entry["applied_additive"] = applied_additive;
	stack_entry["applied_multiplicative"] = applied_multiplicative;
	stack_entry["stack_behavior"] = int(stack_behavior);
	stack_entry["is_match_duration"] = is_match_duration;
	target.stat_stacks[stack_key] = stack_entry;
	_set_stat_modifier_duration(target, stat_name, next_duration, is_match_duration);
}

// Stack management functions
void TeamfightSimulationCore::_reset_stat_temp_tracker(UnitState &unit, StringName stat_name) {
#define X(name, def, min_val, max_val) \
	if (stat_name == StringName(#name)) { \
		unit.stat_temp_##name = 0.0; \
		return; \
	}
	STAT_LIST
#undef X
}

String TeamfightSimulationCore::_get_stack_key(StringName stat_name, const String &reason) {
	return String(stat_name) + "|" + reason;
}

int TeamfightSimulationCore::_consume_stat_stacks(UnitState &unit, StringName stat_name, String reason) {
	if (!_is_valid_stat_name(stat_name)) {
		return 0;
	}
	
	String stack_key = _get_stack_key(stat_name, reason);
	Dictionary stack_entry = Dictionary(unit.stat_stacks.get(stack_key, Dictionary()));
	int current_stacks = int(stack_entry.get("current_stacks", 0));
	
	if (current_stacks > 0) {
		double applied_additive = double(stack_entry.get("applied_additive", 0.0));
		double applied_multiplicative = double(stack_entry.get("applied_multiplicative", 1.0));
		
		// Undo the stat modifiers
		double inverse_multiplicative = applied_multiplicative != 0.0 ? 1.0 / applied_multiplicative : 1.0;
		_apply_stat_modifier(unit, unit, stat_name, -applied_additive, inverse_multiplicative, 0.0, false);
		
		// Remove the stack entry
		unit.stat_stacks.erase(stack_key);
		
		// Reset the temp duration tracker to prevent stat from dropping below base
		// when new stacks are applied later and expire
		_reset_stat_temp_tracker(unit, stat_name);
	}
	
	return current_stacks;
}

void TeamfightSimulationCore::_set_stat_stacks(UnitState &unit, StringName stat_name, String reason, int stack_count, double duration, bool to_max, int fallback_max_stacks, double fallback_additive_per_stack, double fallback_multiplicative_per_stack) {
	if (!_is_valid_stat_name(stat_name)) {
		return;
	}
	
	String stack_key = _get_stack_key(stat_name, reason);
	Dictionary stack_entry = Dictionary(unit.stat_stacks.get(stack_key, Dictionary()));
	
	int max_stacks = fallback_max_stacks > 0 ? fallback_max_stacks : 1;
	double additive_per_stack = fallback_additive_per_stack;
	double multiplicative_per_stack = fallback_multiplicative_per_stack;
	double current_duration = duration;
	int stack_behavior = int(StackBehavior::Refresh);
	bool is_match_duration = false;
	bool entry_existed = !stack_entry.is_empty();
	
	if (entry_existed) {
		// Entry exists, undo current modifiers
		double applied_additive = double(stack_entry.get("applied_additive", 0.0));
		double applied_multiplicative = double(stack_entry.get("applied_multiplicative", 1.0));
		double inverse_multiplicative = applied_multiplicative != 0.0 ? 1.0 / applied_multiplicative : 1.0;
		_apply_stat_modifier(unit, unit, stat_name, -applied_additive, inverse_multiplicative, 0.0, false);
		
		// Reset temp tracker when undoing existing modifiers
		_reset_stat_temp_tracker(unit, stat_name);
		
		// Get parameters from existing entry
		max_stacks = int(stack_entry.get("max_stacks", max_stacks));
		additive_per_stack = double(stack_entry.get("additive_per_stack", additive_per_stack));
		multiplicative_per_stack = double(stack_entry.get("multiplicative_per_stack", multiplicative_per_stack));
		if (duration <= 0.0) {
			current_duration = double(stack_entry.get("duration", 0.0));
		}
		stack_behavior = int(stack_entry.get("stack_behavior", int(StackBehavior::Refresh)));
		is_match_duration = bool(stack_entry.get("is_match_duration", false));
	}
	
	// Determine final stack count
	int final_stacks = stack_count;
	if (to_max) {
		final_stacks = max_stacks;
	}
	if (final_stacks < 0) {
		final_stacks = 0;
	}
	if (final_stacks > max_stacks) {
		final_stacks = max_stacks;
	}
	
	// Calculate new applied values
	double new_applied_additive = additive_per_stack * double(final_stacks);
	double new_applied_multiplicative = 1.0;
	if (!Math::is_equal_approx(multiplicative_per_stack, 1.0)) {
		new_applied_multiplicative = Math::pow(multiplicative_per_stack, double(final_stacks));
	}
	
	// Apply new modifiers
	_apply_stat_modifier(unit, unit, stat_name, new_applied_additive, new_applied_multiplicative, 0.0, false);
	
	// Update or create stack entry
	stack_entry["current_stacks"] = final_stacks;
	stack_entry["max_stacks"] = max_stacks;
	stack_entry["duration"] = current_duration;
	stack_entry["additive_per_stack"] = additive_per_stack;
	stack_entry["multiplicative_per_stack"] = multiplicative_per_stack;
	stack_entry["applied_additive"] = new_applied_additive;
	stack_entry["applied_multiplicative"] = new_applied_multiplicative;
	stack_entry["stack_behavior"] = stack_behavior;
	stack_entry["is_match_duration"] = is_match_duration;
	unit.stat_stacks[stack_key] = stack_entry;
	_set_stat_modifier_duration(unit, stat_name, current_duration, is_match_duration);
}


void TeamfightSimulationCore::_update_stacks(UnitState &unit, double delta, double current_time) {
	(void)current_time;
	Array keys_to_remove;
	Array stack_keys = unit.stat_stacks.keys();
	for (int i = 0; i < stack_keys.size(); i++) {
		Variant key_variant = stack_keys[i];
		if (key_variant.get_type() != Variant::STRING) {
			continue;
		}
		String key = key_variant;
		Dictionary stack_dict = unit.stat_stacks[key];
		double duration = double(stack_dict.get("duration", 0.0));
		if (duration <= 0.0) {
			continue;
		}
		duration -= delta;
		if (duration > 0.0) {
			stack_dict["duration"] = duration;
			unit.stat_stacks[key] = stack_dict;
			continue;
		}

		StringName stat_name = StringName(key.get_slice("|", 0));
		double additive = double(stack_dict.get("applied_additive", 0.0));
		double multiplicative = double(stack_dict.get("applied_multiplicative", 1.0));
		double inverse_multiplicative = multiplicative != 0.0 ? 1.0 / multiplicative : 1.0;
		int current_stacks = int(stack_dict.get("current_stacks", 0));
		if (current_stacks > 0) {
			_apply_stat_modifier(unit, unit, stat_name, -additive, inverse_multiplicative, 0.0, false);
		}
		
		// Reset the temp duration tracker to ensure stat returns to base
		_reset_stat_temp_tracker(unit, stat_name);
		
		keys_to_remove.append(key);
	}
	for (int i = 0; i < keys_to_remove.size(); i++) {
		unit.stat_stacks.erase(String(keys_to_remove[i]));
	}
}

void TeamfightSimulationCore::_cleanup_expired_stacks(UnitState &unit, double current_time) {
	(void)unit;
	(void)current_time;
}

void TeamfightSimulationCore::_run_post_attack_effects(UnitState &source, UnitState &target, double damage, const EffectContext &context) {
	// Break stealth on attack cast finish if configured
	if (source.stealth_remaining > 0.0 && source.stealth_break_on_attack) {
		source.stealth_remaining = 0.0;
		source.stealth_break_on_attack = false;
		source.stealth_break_on_ability = false;
		source.stealth_break_on_damage_taken = false;
	}
	const std::vector<EffectRecord> &post_attack_effects = _uc(source).passive_effects[EFFECT_BUCKET_POST_ATTACK];
	if (post_attack_effects.empty()) {
		return;
	}
	EffectContext effect_context = context;
	effect_context.damage = damage;
	for (const EffectRecord &effect : post_attack_effects) {
		_execute_effect(effect, effect_context);
	}
}

void TeamfightSimulationCore::_run_post_heal_effects(UnitState &source, UnitState &target, double heal_amount, double heal_gained, const StringName &action_kind, const EffectContext &base_context) {
	const std::vector<EffectRecord> &post_heal_effects = _uc(target).passive_effects[EFFECT_BUCKET_POST_HEAL];
	if (post_heal_effects.empty()) {
		return;
	}
	EffectContext effect_context = base_context;
	effect_context.heal_amount = heal_amount;
	effect_context.heal_gained = heal_gained;
	effect_context.action_kind = action_kind;
	effect_context.source = &source;
	effect_context.target = &target;
	
	for (const EffectRecord &effect : post_heal_effects) {
		_execute_effect(effect, effect_context);
	}
}

void TeamfightSimulationCore::_run_on_takedown_effects(UnitState &participant, UnitState &victim, double damage_dealt, bool is_kill, const StringName &action_kind, const EffectContext &base_context) {
	const std::vector<EffectRecord> &takedown_effects = _uc(participant).passive_effects[EFFECT_BUCKET_ON_TAKEDOWN];
	if (takedown_effects.empty()) {
		return;
	}
	EffectContext effect_context = base_context;
	effect_context.takedown_target_id = victim.instance_id;
	effect_context.takedown_damage_dealt = damage_dealt;
	effect_context.is_takedown_kill = is_kill;
	effect_context.damage = damage_dealt;
	effect_context.action_kind = action_kind;
	effect_context.source = &participant;
	effect_context.target = &victim;
	
	for (const EffectRecord &effect : takedown_effects) {
		_execute_effect(effect, effect_context);
	}
}

void TeamfightSimulationCore::_apply_dot(UnitState &source, UnitState &target, double attack_damage_ratio, double max_hp_ratio, double flat_amount, double duration, double tick_interval, const StringName &damage_type, const StringName &stacking_mode, int max_stacks, const StringName &effect_type, const String &reason, const StringName &action_kind, bool is_dynamic) {
	// Don't apply effects to dead units
	if (!target.alive) {
		return;
	}
	
	// Validate tick_interval to prevent division by zero
	if (tick_interval <= 0.0) {
		UtilityFunctions::push_error(vformat("DoT effect '%s' has invalid tick_interval: %f. Must be > 0.", String(effect_type), tick_interval));
		return;
	}
	
	// Calculate total damage from ratios at application time
	double source_attack_damage = get_effective_attack_damage(source);
	double damage_total = source_attack_damage * attack_damage_ratio;
	damage_total += get_effective_max_hp(target) * max_hp_ratio;
	damage_total += flat_amount;
	
	if (duration <= 0.0 || damage_total <= 0.0) {
		return;
	}
	
	// Assumption: duration and tick_interval are always evenly divisible
	// This ensures tick_count is always an integer
	double tick_count = duration / tick_interval;
	// Validate divisibility to prevent incorrect damage distribution
	double tick_count_rounded = Math::round(tick_count);
	if (Math::abs(tick_count - tick_count_rounded) > 0.0001) {
		UtilityFunctions::push_error(vformat("DoT effect '%s' has non-divisible duration (%f) and tick_interval (%f). tick_count is %f (should be integer).", String(effect_type), duration, tick_interval, tick_count));
		return;  // Reject effect if not divisible
	}
	
	auto &periodic_effects = _uc(target).periodic_effects;
	
	// Skip matching logic for separate mode - always create independent instances
	if (stacking_mode != StringName("separate")) {
		for (auto &existing : periodic_effects) {
			if (existing.effect_type == effect_type && existing.source_instance_id == source.instance_id && existing.reason == reason) {
				// Apply stacking logic
				if (stacking_mode == StringName("refresh")) {
					existing.damage_total = damage_total;
					existing.total_attack_damage_ratio = attack_damage_ratio;
					existing.total_max_hp_ratio = max_hp_ratio;
					existing.total_flat_amount = flat_amount;
					existing.remaining_duration = duration;
					existing.original_tick_count = tick_count;
					existing.calculation_mode = is_dynamic ? StringName("dynamic") : StringName("fixed");
					return;
				} else if (stacking_mode == StringName("extend")) {
					// Additive logic: calculate remaining damage, add new damage, distribute over new duration
					// This avoids numerical instability when remaining_duration is very small
					double remaining_damage = 0.0;
					double damage_scaling_factor = 0.0;
					if (existing.original_tick_count > 0.0 && existing.damage_total > 0.0) {
						double per_tick_damage = existing.damage_total / existing.original_tick_count;
						double remaining_ticks = existing.remaining_duration / existing.tick_interval;
						remaining_damage = per_tick_damage * remaining_ticks;
						damage_scaling_factor = remaining_damage / existing.damage_total;
					}
					
					// Add new damage to remaining damage
					existing.damage_total = remaining_damage + damage_total;
					
					// Scale existing ratios to match remaining damage, then add new ratios
					// This ensures dynamic mode calculates correctly
					existing.total_attack_damage_ratio = existing.total_attack_damage_ratio * damage_scaling_factor + attack_damage_ratio;
					existing.total_max_hp_ratio = existing.total_max_hp_ratio * damage_scaling_factor + max_hp_ratio;
					existing.total_flat_amount = existing.total_flat_amount * damage_scaling_factor + flat_amount;
					
					existing.remaining_duration = existing.remaining_duration + duration;
					// Validate existing tick_interval (shouldn't be 0, but defensive check)
					if (existing.tick_interval <= 0.0) {
						existing.tick_interval = tick_interval;
					}
					existing.original_tick_count = existing.remaining_duration / existing.tick_interval;
					return;
				}
			}
		}
	}
	
	// For separate mode: enforce max_stacks by counting effects of same type, source, and reason
	if (stacking_mode == StringName("separate") && max_stacks > 0) {
		int count = 0;
		for (auto &existing : periodic_effects) {
			if (existing.effect_type == effect_type && existing.source_instance_id == source.instance_id && existing.reason == reason) {
				count++;
			}
		}
		if (count >= max_stacks) {
			return; // Max stacks reached, don't add new instance
		}
	}
	
	// Apply instant tick with adjusted per-tick value (total distributed over tick_count + 1 ticks)
	double per_tick = damage_total / (tick_count + 1.0);
	if (per_tick > 0.0) {
		EffectContext context = _build_context(source, &target, nullptr, per_tick, action_kind);
		_apply_damage(source, target, per_tick, damage_type, action_kind, context);
	}
	
	// Store adjusted total for periodic ticks (per_tick * tick_count)
	double adjusted_damage_total = per_tick * tick_count;
	
	UnitStateCold::PeriodicEffect new_effect;
	new_effect.effect_type = effect_type;
	new_effect.damage_total = adjusted_damage_total;
	new_effect.heal_total = 0.0;
	new_effect.remaining_duration = duration;
	new_effect.tick_interval = tick_interval;
	new_effect.original_tick_count = tick_count;
	new_effect.tick_accumulator = 0.0;
	new_effect.source_instance_id = source.instance_id;
	new_effect.damage_type = damage_type;
	new_effect.stacking_mode = stacking_mode;
	new_effect.reason = reason;
	new_effect.allow_overheal = false;
	new_effect.stack_count = 1;
	new_effect.max_stacks = max_stacks;
	new_effect.action_kind = action_kind;
	
	// Store total ratio parameters for dynamic calculation
	new_effect.total_attack_damage_ratio = attack_damage_ratio;
	new_effect.total_max_hp_ratio = max_hp_ratio;
	new_effect.total_flat_amount = flat_amount;
	new_effect.calculation_mode = is_dynamic ? StringName("dynamic") : StringName("fixed");
	periodic_effects.push_back(new_effect);
}

void TeamfightSimulationCore::_apply_hot(UnitState &source, UnitState &target, double max_hp_ratio, double current_hp_ratio, double missing_hp_ratio, double flat_amount, double duration, double tick_interval, const StringName &stacking_mode, int max_stacks, bool allow_overheal, const StringName &effect_type, const String &reason, const StringName &action_kind, bool is_dynamic) {
	// Don't apply effects to dead units
	if (!target.alive) {
		return;
	}
	
	// Validate tick_interval to prevent division by zero
	if (tick_interval <= 0.0) {
		UtilityFunctions::push_error(vformat("HoT effect '%s' has invalid tick_interval: %f. Must be > 0.", String(effect_type), tick_interval));
		return;
	}
	
	// Calculate total heal from ratios at application time
	double heal_total = get_effective_max_hp(target) * max_hp_ratio;
	heal_total += target.hp * current_hp_ratio;
	heal_total += (get_effective_max_hp(target) - target.hp) * missing_hp_ratio;
	heal_total += flat_amount;
	
	if (duration <= 0.0 || heal_total <= 0.0) {
		return;
	}
	
	// Assumption: duration and tick_interval are always evenly divisible
	// This ensures tick_count is always an integer
	double tick_count = duration / tick_interval;
	// Validate divisibility to prevent incorrect heal distribution
	double tick_count_rounded = Math::round(tick_count);
	if (Math::abs(tick_count - tick_count_rounded) > 0.0001) {
		UtilityFunctions::push_error(vformat("HoT effect '%s' has non-divisible duration (%f) and tick_interval (%f). tick_count is %f (should be integer).", String(effect_type), duration, tick_interval, tick_count));
		return;  // Reject effect if not divisible
	}
	
	// Visual effect: green progress border
	_viewer_record_hot_status_fx(target, duration, effect_type);
	
	// Skip matching logic for separate mode - always create independent instances
	auto &periodic_effects = _uc(target).periodic_effects;
	if (stacking_mode != StringName("separate")) {
		for (auto &existing : periodic_effects) {
			if (existing.effect_type == effect_type && existing.source_instance_id == source.instance_id && existing.reason == reason) {
				// Apply stacking logic
				if (stacking_mode == StringName("refresh")) {
					existing.heal_total = heal_total;
					existing.total_max_hp_ratio = max_hp_ratio;
					existing.total_current_hp_ratio = current_hp_ratio;
					existing.total_missing_hp_ratio = missing_hp_ratio;
					existing.total_flat_amount = flat_amount;
					existing.allow_overheal = allow_overheal;
					existing.remaining_duration = duration;
					existing.original_tick_count = tick_count;
					existing.calculation_mode = is_dynamic ? StringName("dynamic") : StringName("fixed");
					return;
				} else if (stacking_mode == StringName("extend")) {
					// Additive logic: calculate remaining heal, add new heal, distribute over new duration
					// This avoids numerical instability when remaining_duration is very small
					double remaining_heal = 0.0;
					double heal_scaling_factor = 0.0;
					if (existing.original_tick_count > 0.0 && existing.heal_total > 0.0) {
						double per_tick_heal = existing.heal_total / existing.original_tick_count;
						double remaining_ticks = existing.remaining_duration / existing.tick_interval;
						remaining_heal = per_tick_heal * remaining_ticks;
						heal_scaling_factor = remaining_heal / existing.heal_total;
					}
					
					// Add new heal to remaining heal
					existing.heal_total = remaining_heal + heal_total;
					
					// Scale existing ratios to match remaining heal, then add new ratios
					// This ensures dynamic mode calculates correctly
					existing.total_max_hp_ratio = existing.total_max_hp_ratio * heal_scaling_factor + max_hp_ratio;
					existing.total_current_hp_ratio = existing.total_current_hp_ratio * heal_scaling_factor + current_hp_ratio;
					existing.total_missing_hp_ratio = existing.total_missing_hp_ratio * heal_scaling_factor + missing_hp_ratio;
					existing.total_flat_amount = existing.total_flat_amount * heal_scaling_factor + flat_amount;
					
					existing.remaining_duration = existing.remaining_duration + duration;
					// Validate existing tick_interval (shouldn't be 0, but defensive check)
					if (existing.tick_interval <= 0.0) {
						existing.tick_interval = tick_interval;
					}
					existing.original_tick_count = existing.remaining_duration / existing.tick_interval;
					return;
				}
			}
		}
	}
	
	// For separate mode: enforce max_stacks by counting effects of same type, source, and reason
	if (stacking_mode == StringName("separate") && max_stacks > 0) {
		int count = 0;
		for (auto &existing : periodic_effects) {
			if (existing.effect_type == effect_type && existing.source_instance_id == source.instance_id && existing.reason == reason) {
				count++;
			}
		}
		if (count >= max_stacks) {
			return; // Max stacks reached, don't add new instance
		}
	}
	
	// Apply instant tick with adjusted per-tick value (total distributed over tick_count + 1 ticks)
	double per_tick = heal_total / (tick_count + 1.0);
	if (per_tick > 0.0) {
		EffectContext context = _build_context(source, &target, nullptr, 0.0, action_kind);
		double old_hp = target.hp;
		_heal_unit(source, target, per_tick, action_kind, allow_overheal);
		double heal_gained = target.hp - old_hp;
		_run_post_heal_effects(source, target, per_tick, heal_gained, action_kind, context);
	}
	
	// Store adjusted total for periodic ticks (per_tick * tick_count)
	double adjusted_heal_total = per_tick * tick_count;
	
	// No matching effect found or "separate" mode - add new
	UnitStateCold::PeriodicEffect new_effect;
	new_effect.effect_type = effect_type;
	new_effect.damage_total = 0.0;
	new_effect.heal_total = adjusted_heal_total;
	new_effect.remaining_duration = duration;
	new_effect.tick_interval = tick_interval;
	new_effect.original_tick_count = tick_count;
	new_effect.tick_accumulator = 0.0;
	new_effect.source_instance_id = source.instance_id;
	new_effect.damage_type = StringName();
	new_effect.stacking_mode = stacking_mode;
	new_effect.reason = reason;
	new_effect.allow_overheal = allow_overheal;
	new_effect.stack_count = 1;
	new_effect.max_stacks = max_stacks;
	new_effect.action_kind = action_kind;
	
	// Store total ratio parameters for dynamic calculation
	new_effect.total_max_hp_ratio = max_hp_ratio;
	new_effect.total_current_hp_ratio = current_hp_ratio;
	new_effect.total_missing_hp_ratio = missing_hp_ratio;
	new_effect.total_flat_amount = flat_amount;
	new_effect.calculation_mode = is_dynamic ? StringName("dynamic") : StringName("fixed");
	periodic_effects.push_back(new_effect);
}

void TeamfightSimulationCore::_tick_periodic_effects(UnitState &unit, double delta) {
	auto &periodic_effects = _uc(unit).periodic_effects;
	size_t index = 0;
	while (index < periodic_effects.size()) {
		UnitStateCold::PeriodicEffect effect = periodic_effects[index];
		
		// Update accumulator
		effect.tick_accumulator += delta;
		
		// Check if tick should occur (process all pending ticks)
		while (effect.tick_accumulator >= effect.tick_interval) {
			effect.tick_accumulator -= effect.tick_interval;
			
			// Use original_tick_count for consistent per-tick values throughout effect lifetime
			// Fallback to calculation if original_tick_count is invalid (e.g., from deserialization)
			double tick_count = effect.original_tick_count;
			if (tick_count <= 0.0) {
				if (effect.tick_interval <= 0.0) {
					// Invalid tick_interval, skip effect to prevent division by zero
					index++;
					continue;
				}
				tick_count = effect.remaining_duration / effect.tick_interval;
				if (tick_count <= 0.0) {
					// Invalid effect, skip it
					index++;
					continue;
				}
			}
			
			// Calculate per-tick damage
			double damage_per_tick = 0.0;
			if (effect.damage_total > 0.0) {
				if (effect.calculation_mode == StringName("dynamic")) {
					// Recalculate from ratios on each tick using current source stats
					UnitState *source = _unit_by_id(effect.source_instance_id);
					if (source != nullptr) {
						double damage_total = get_effective_attack_damage(*source) * effect.total_attack_damage_ratio;
						damage_total += get_effective_max_hp(unit) * effect.total_max_hp_ratio;
						damage_total += effect.total_flat_amount;
						damage_per_tick = damage_total / (tick_count + 1.0);
					} else {
						// Source is dead/null, fall back to stored total values calculated at application time
						// This allows effects to continue ticking even after the source dies
						damage_per_tick = effect.damage_total / tick_count;
					}
				} else {
					// Fixed mode: use stored total divided by tick count
					damage_per_tick = effect.damage_total / tick_count;
				}
			}
			
			// Calculate per-tick heal
			double heal_per_tick = 0.0;
			if (effect.heal_total > 0.0) {
				if (effect.calculation_mode == StringName("dynamic")) {
					// Recalculate from ratios on each tick using current target HP state
					UnitState *source = _unit_by_id(effect.source_instance_id);
					if (source != nullptr) {
						double heal_total = get_effective_max_hp(unit) * effect.total_max_hp_ratio;
						heal_total += unit.hp * effect.total_current_hp_ratio;
						heal_total += (get_effective_max_hp(unit) - unit.hp) * effect.total_missing_hp_ratio;
						heal_total += effect.total_flat_amount;
						heal_per_tick = heal_total / (tick_count + 1.0);
					} else {
						// Source is dead/null, fall back to stored total values calculated at application time
						// This allows effects to continue ticking even after the source dies
						heal_per_tick = effect.heal_total / tick_count;
					}
				} else {
					// Fixed mode: use stored total divided by tick count
					heal_per_tick = effect.heal_total / tick_count;
				}
			}
			
			if (damage_per_tick > 0.0) {
				// Apply DoT damage
				UnitState *source = _unit_by_id(effect.source_instance_id);
				if (source != nullptr) {
					EffectContext context = _build_context(*source, &unit, nullptr, damage_per_tick, effect.action_kind);
					_apply_damage(*source, unit, damage_per_tick, effect.damage_type, effect.action_kind, context);
					if (!unit.alive) {
						return;
					}
				} else {
					UtilityFunctions::push_error(vformat("[DEBUG] Skipping DoT application: source is null (source_instance_id=%d, damage_per_tick=%.2f)", effect.source_instance_id, damage_per_tick));
				}
			}
			
			if (heal_per_tick > 0.0) {
				// Apply HoT heal
				UnitState *source = _unit_by_id(effect.source_instance_id);
				if (source != nullptr) {
					EffectContext context = _build_context(*source, &unit, nullptr, 0.0, effect.action_kind);
					double old_hp = unit.hp;
					_heal_unit(*source, unit, heal_per_tick, effect.action_kind, effect.allow_overheal);
					double heal_gained = unit.hp - old_hp;
					_run_post_heal_effects(*source, unit, heal_per_tick, heal_gained, effect.action_kind, context);
					if (!unit.alive) {
						return;
					}
				} else {
					UtilityFunctions::push_error(vformat("[DEBUG] Skipping HoT application: source is null (source_instance_id=%d, heal_per_tick=%.2f)", effect.source_instance_id, heal_per_tick));
				}
			}
		}
		
		// Update remaining duration
		effect.remaining_duration -= delta;
		if (index >= periodic_effects.size()) {
			continue;
		}
		
		// Remove expired effects
		if (effect.remaining_duration <= 0.0) {
			periodic_effects.erase(periodic_effects.begin() + static_cast<std::ptrdiff_t>(index));
		} else {
			periodic_effects[index] = effect;
			++index;
		}
	}
}

// Channel effect functions

int64_t TeamfightSimulationCore::_get_channel_tick_count(const UnitStateCold &cold) {
	if (cold.channel_tick_interval <= 0.0) {
		return 0;
	}
	double total_ticked = cold.channel_tick_accumulator;
	return static_cast<int64_t>(total_ticked / cold.channel_tick_interval);
}

void TeamfightSimulationCore::_clear_channel_state(UnitStateCold &cold) {
	cold.is_channeling = false;
	cold.channel_remaining_duration = 0.0;
	cold.channel_tick_interval = 0.0;
	cold.channel_tick_accumulator = 0.0;
	cold.channel_accumulated_damage = 0.0;
	cold.channel_target_instance_id = 0;
	cold.channel_source_instance_id = 0;
	cold.channel_reason = StringName();
	cold.channel_action_kind = StringName();
	cold.channel_allow_movement = false;
	cold.channel_target_mode = StringName();
	cold.channel_sub_effect = EffectRecord();
	cold.channel_post_complete_effect = EffectRecord();
	cold.channel_post_interrupt_effect = EffectRecord();
}

bool TeamfightSimulationCore::_should_interrupt_channel(UnitState &unit, const UnitStateCold &cold) {
	// Death interrupts
	if (!unit.alive) return true;
	
	// Stun interrupts
	if (unit.stun_remaining > 0.0) return true;
	
	// Silence interrupts (check relevant type)
	if (cold.channel_action_kind == sn_ability() && unit.silence_blocks_abilities) return true;
	if (cold.channel_action_kind == sn_ultimate() && unit.silence_blocks_ultimates) return true;
	
	// Movement interrupts (if not allowed and not rooted)
	if (!cold.channel_allow_movement && unit.root_remaining <= 0.0) {
		// Interrupt if no valid targets in attack range
		StringName enemy_team = unit.team == StringName("player") ? StringName("enemy") : StringName("player");
		const std::vector<int64_t> &enemy_indices = _alive_indices_for_team(enemy_team);
		double attack_range = get_effective_attack_range(unit);
		bool has_target_in_range = false;
		
		for (int64_t index : enemy_indices) {
			UnitState &enemy = _units[static_cast<size_t>(index)];
			double dist = _distance_between_coords(unit.pos_x, unit.pos_y, enemy.pos_x, enemy.pos_y);
			if (dist <= attack_range) {
				has_target_in_range = true;
				break;
			}
		}
		
		if (!has_target_in_range) {
			return true;
		}
	}
	
	return false;
}

void TeamfightSimulationCore::_complete_channel(UnitState &unit, UnitStateCold &cold) {
	cold.is_channeling = false;
	
	// Execute post-complete effect
	if (cold.channel_post_complete_effect.opcode != 0) {
		UnitState *target = cold.channel_target_instance_id != 0 ? _unit_by_id(cold.channel_target_instance_id) : nullptr;
		if (cold.channel_target_mode == StringName("self")) {
			target = &unit;
		}
		EffectContext context = _build_context(unit, target, nullptr, 0.0, StringName("channel_complete"));
		context.channel_remaining_duration = cold.channel_remaining_duration;
		context.channel_tick_count = _get_channel_tick_count(cold);
		context.channel_accumulated_damage = cold.channel_accumulated_damage;
		context.channel_completed = true;
		_execute_effect(cold.channel_post_complete_effect, context);
	}
	
	// Start cooldown
	if (cold.channel_action_kind == sn_ability()) {
		unit.ability_cooldown = get_effective_ability_cd(unit);
	}
	// Ultimates use mana-based cooldowns (mana consumed on cast start, no refund on completion)
	
	_clear_channel_state(cold);
}

void TeamfightSimulationCore::_interrupt_channel(UnitState &unit, UnitStateCold &cold) {
	cold.is_channeling = false;
	
	// Execute post-interrupt effect
	if (cold.channel_post_interrupt_effect.opcode != 0) {
		UnitState *target = cold.channel_target_instance_id != 0 ? _unit_by_id(cold.channel_target_instance_id) : nullptr;
		if (cold.channel_target_mode == StringName("self")) {
			target = &unit;
		}
		EffectContext context = _build_context(unit, target, nullptr, 0.0, StringName("channel_interrupted"));
		context.channel_remaining_duration = cold.channel_remaining_duration;
		context.channel_tick_count = _get_channel_tick_count(cold);
		context.channel_accumulated_damage = cold.channel_accumulated_damage;
		context.channel_completed = false;
		_execute_effect(cold.channel_post_interrupt_effect, context);
	}
	
	// Apply cooldown on interrupt (no refund)
	if (cold.channel_action_kind == sn_ability()) {
		unit.ability_cooldown = get_effective_ability_cd(unit);
	}
	// Ultimates: mana already consumed on cast, no additional action needed
	
	_clear_channel_state(cold);
}

void TeamfightSimulationCore::_process_channel_tick(UnitState &unit, double delta) {
	UnitStateCold &cold = _uc(unit);
	
	// Check interrupt conditions
	if (_should_interrupt_channel(unit, cold)) {
		_interrupt_channel(unit, cold);
		return;
	}
	
	// Update tick accumulator
	cold.channel_tick_accumulator += delta;
	
	// Execute sub-effect on each tick
	if (cold.channel_tick_accumulator >= cold.channel_tick_interval) {
		cold.channel_tick_accumulator -= cold.channel_tick_interval;
		
		// Select target
		UnitState *target = nullptr;
		if (cold.channel_target_mode == StringName("self")) {
			target = &unit;
		} else if (cold.channel_target_mode == StringName("fixed")) {
			target = cold.channel_target_instance_id != 0 ? _unit_by_id(cold.channel_target_instance_id) : nullptr;
		} else if (cold.channel_target_mode == StringName("dynamic")) {
			// For dynamic target mode, use the closest enemy for now
			// This can be extended later to support other selection strategies
			StringName enemy_team = unit.team == StringName("player") ? StringName("enemy") : StringName("player");
			const std::vector<int64_t> &enemy_indices = _alive_indices_for_team(enemy_team);
			
			if (!enemy_indices.empty()) {
				// Find closest enemy
				UnitState *closest = nullptr;
				double closest_dist = std::numeric_limits<double>::max();
				
				for (int64_t index : enemy_indices) {
					UnitState &enemy = _units[static_cast<size_t>(index)];
					double dist = _distance_between_coords(unit.pos_x, unit.pos_y, enemy.pos_x, enemy.pos_y);
					if (dist < closest_dist) {
						closest_dist = dist;
						closest = &enemy;
					}
				}
				
				target = closest;
			}
		}
		
		if (target != nullptr && target->alive) {
			EffectContext context = _build_context(unit, target, nullptr, 0.0, StringName("channel_tick"));
			context.channel_remaining_duration = cold.channel_remaining_duration;
			context.channel_tick_count = _get_channel_tick_count(cold);
			context.channel_accumulated_damage = cold.channel_accumulated_damage;
			
			Dictionary result = _execute_effect(cold.channel_sub_effect, context);
			
			// Accumulate damage from result
			if (result.has("damage")) {
				cold.channel_accumulated_damage += double(result["damage"]);
			}
		} else {
			// Log warning when no valid target exists (only in debug mode)
			if (_debug_combat_trace) {
				String warning_msg = "Channel tick skipped: no valid target. Unit ID: ";
				warning_msg += String::num_int64(unit.instance_id);
				warning_msg += ", Channel reason: ";
				warning_msg += cold.channel_reason;
				warning_msg += ", Target mode: ";
				warning_msg += cold.channel_target_mode;
				UtilityFunctions::push_warning(warning_msg);
			}
		}
	}
	
	// Update duration
	cold.channel_remaining_duration -= delta;
	
	// Check if channel completed
	if (cold.channel_remaining_duration <= 0.0) {
		_complete_channel(unit, cold);
	}
}

void TeamfightSimulationCore::_cleanse_dots(UnitState &unit, const StringName &effect_type_filter) {
	auto &periodic_effects = _uc(unit).periodic_effects;
	if (effect_type_filter.is_empty()) {
		// Remove all DoTs (damage_total > 0)
		periodic_effects.erase(
			std::remove_if(periodic_effects.begin(), periodic_effects.end(),
				[](const UnitStateCold::PeriodicEffect &e) { return e.damage_total > 0.0; }),
			periodic_effects.end()
		);
	} else {
		// Remove DoTs matching effect type
		periodic_effects.erase(
			std::remove_if(periodic_effects.begin(), periodic_effects.end(),
				[&effect_type_filter](const UnitStateCold::PeriodicEffect &e) {
					return e.damage_total > 0.0 && e.effect_type == effect_type_filter;
				}),
			periodic_effects.end()
		);
	}
}

void TeamfightSimulationCore::_clear_periodic_effects(UnitState &unit) {
	_uc(unit).periodic_effects.clear();
}

void TeamfightSimulationCore::_apply_aoe_dot(UnitState &source, double radius, double attack_damage_ratio, double max_hp_ratio, double flat_amount, double duration, double tick_interval, const StringName &damage_type, const StringName &stacking_mode, int max_stacks, const StringName &effect_type, const String &reason, bool target_self, const StringName &action_kind, bool is_dynamic) {
	EffectRecord effect;
	effect.aoe_shape_params.shape = AoShapeKind::Circle;
	effect.aoe_shape_params.anchor = AoAnchorKind::Self;
	effect.aoe_shape_params.radius = radius;
	effect.damage_type = damage_type;
	effect.stacking_mode = stacking_mode;
	effect.effect_type = effect_type;
	_apply_aoe_dot_shape(source, nullptr, effect, attack_damage_ratio, max_hp_ratio, flat_amount, duration, tick_interval, damage_type, stacking_mode, max_stacks, effect_type, reason, target_self, action_kind, is_dynamic);
}

void TeamfightSimulationCore::_apply_aoe_hot(UnitState &source, double radius, double max_hp_ratio, double current_hp_ratio, double missing_hp_ratio, double flat_amount, double duration, double tick_interval, const StringName &stacking_mode, int max_stacks, bool allow_overheal, const StringName &effect_type, const String &reason, bool target_self, const StringName &action_kind, bool is_dynamic) {
	EffectRecord effect;
	effect.aoe_shape_params.shape = AoShapeKind::Circle;
	effect.aoe_shape_params.anchor = AoAnchorKind::Self;
	effect.aoe_shape_params.radius = radius;
	effect.stacking_mode = stacking_mode;
	effect.effect_type = effect_type;
	_apply_aoe_hot_shape(source, nullptr, effect, max_hp_ratio, current_hp_ratio, missing_hp_ratio, flat_amount, duration, tick_interval, stacking_mode, max_stacks, allow_overheal, effect_type, reason, target_self, action_kind, is_dynamic);
}

void TeamfightSimulationCore::_apply_aoe_taunt(UnitState &source, double radius, double duration) {
	EffectRecord effect;
	effect.aoe_shape_params.shape = AoShapeKind::Circle;
	effect.aoe_shape_params.anchor = AoAnchorKind::Self;
	effect.aoe_shape_params.radius = radius;
	_apply_aoe_taunt_shape(source, nullptr, effect, duration);
}

double TeamfightSimulationCore::_apply_aoe_damage(UnitState &source, UnitState &center_source, double damage, double radius, const StringName &damage_type, const StringName &reason, const StringName &action_kind) {
	(void)reason;
	EffectRecord effect;
	effect.aoe_shape_params.shape = AoShapeKind::Circle;
	effect.aoe_shape_params.anchor = AoAnchorKind::Target;
	effect.aoe_shape_params.radius = radius;
	effect.damage_type = damage_type;
	return _apply_aoe_damage_shape(source, &center_source, effect, damage, damage_type, action_kind);
}

Vector2 TeamfightSimulationCore::_resolve_aoe_direction(const UnitState &source, const AoShapeParams &params, const UnitState *target_override) const {
	Vector2 forward(1.0, 0.0);
	
	const UnitState *target = target_override;
	if (target == nullptr && params.target_id != 0) {
		target = _unit_by_id(params.target_id);
	}
	
	if (target != nullptr) {
		Vector2 diff(target->pos_x - source.pos_x, target->pos_y - source.pos_y);
		if (diff.length_squared() > EPSILON * EPSILON) {
			forward = diff.normalized();
		}
	} else {
		// For AOE without explicit target, use source's current attack target
		int64_t source_target_id = source.target_id;
		if (source_target_id != 0) {
			const UnitState *source_target = _unit_by_id(source_target_id);
			if (source_target != nullptr) {
				Vector2 diff(source_target->pos_x - source.pos_x, source_target->pos_y - source.pos_y);
				if (diff.length_squared() > EPSILON * EPSILON) {
					forward = diff.normalized();
				}
			}
		}
		
		// Fallback to enemy team center if no target found
		if (forward == Vector2(1.0, 0.0)) {
			StringName enemy_team = source.team == sn_player() ? sn_enemy() : sn_player();
			const std::vector<int64_t> &enemy_indices = _alive_indices_for_team(enemy_team);
			if (!enemy_indices.empty()) {
				double cx = 0.0;
				double cy = 0.0;
				for (int64_t idx : enemy_indices) {
					const UnitState &u = _units[static_cast<size_t>(idx)];
					cx += u.pos_x;
					cy += u.pos_y;
				}
				cx /= double(enemy_indices.size());
				cy /= double(enemy_indices.size());
				Vector2 diff(cx - source.pos_x, cy - source.pos_y);
				if (diff.length_squared() > EPSILON * EPSILON) {
					forward = diff.normalized();
				}
			}
		}
	}
	
	// Apply rotation if specified
	if (!Math::is_zero_approx(params.rotation_radians)) {
		forward = forward.rotated(params.rotation_radians);
	}
	
	return forward;
}

TeamfightSimulationCore::AoShapeParams TeamfightSimulationCore::_parse_aoe_shape_metadata(const Dictionary &params, ParamTracker &tracker) const {
	AoShapeParams shape_params;
	shape_params.shape = AoShapeKind::Circle;
	shape_params.anchor = AoAnchorKind::Self;
	shape_params.radius = 0.0;
	shape_params.width = 0.0;
	shape_params.height = 0.0;
	shape_params.rotation_radians = 0.0;
	shape_params.anchor_x = 0.0;
	shape_params.anchor_y = 0.0;
	shape_params.target_id = 0;
	
	// Parse shape
	Variant shape_var = params.get("shape", Variant());
	tracker.mark_accessed("shape");
	if (shape_var.get_type() == Variant::STRING) {
		String shape_str = String(shape_var);
		if (shape_str == "circle") {
			shape_params.shape = AoShapeKind::Circle;
		} else if (shape_str == "cone") {
			shape_params.shape = AoShapeKind::Cone;
		} else if (shape_str == "rectangle") {
			shape_params.shape = AoShapeKind::Rectangle;
		}
	}
	
	// Parse anchor
	Variant anchor_var = params.get("anchor", Variant());
	tracker.mark_accessed("anchor");
	if (anchor_var.get_type() == Variant::STRING) {
		String anchor_str = String(anchor_var);
		if (anchor_str == "self") {
			shape_params.anchor = AoAnchorKind::Self;
		} else if (anchor_str == "target") {
			shape_params.anchor = AoAnchorKind::Target;
		} else if (anchor_str == "point") {
			shape_params.anchor = AoAnchorKind::Point;
		} else if (anchor_str == "forward") {
			shape_params.anchor = AoAnchorKind::Forward;
		}
	}
	
	// Parse numeric parameters
	shape_params.radius = params.get("radius", 0.0);
	tracker.mark_accessed("radius");
	shape_params.width = params.get("width", 0.0);
	tracker.mark_accessed("width");
	shape_params.height = params.get("height", 0.0);
	tracker.mark_accessed("height");
	
	// Parse rotation (degrees to radians)
	Variant rotation_var = params.get("rotation_degrees", Variant());
	tracker.mark_accessed("rotation_degrees");
	if (rotation_var.get_type() == Variant::FLOAT || rotation_var.get_type() == Variant::INT) {
		double rotation_degrees = rotation_var;
		shape_params.rotation_radians = Math::deg_to_rad(rotation_degrees);
	}
	
	// Parse anchor position for Point anchor
	shape_params.anchor_x = params.get("anchor_x", 0.0);
	tracker.mark_accessed("anchor_x");
	shape_params.anchor_y = params.get("anchor_y", 0.0);
	tracker.mark_accessed("anchor_y");
	
	// Parse target_id for Target anchor
	shape_params.target_id = params.get("target_id", 0);
	tracker.mark_accessed("target_id");
	
	return shape_params;
}

void TeamfightSimulationCore::_apply_aoe_taunt_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double duration) {
	StringName source_team = source.team;
	StringName enemy_team = source_team == StringName("player") ? StringName("enemy") : StringName("player");
	const std::vector<int64_t> &enemy_indices = _alive_indices_for_team(enemy_team);
	UnitState *shape_target = target;
	if (shape_target == nullptr && effect.aoe_shape_params.target_id != 0) {
		shape_target = _unit_by_id(effect.aoe_shape_params.target_id);
	}
	_viewer_record_aoe_shape_fx(source, shape_target, effect.aoe_shape_params, StringName("aoe_taunt"));
	
	AoShapeIterationParams shape_iter;
	shape_iter.shape_params = effect.aoe_shape_params;
	shape_iter.indices = &enemy_indices;
	shape_iter.spatial_team = enemy_team;
	shape_iter.exclude_instance_id = 0;
	shape_iter.source = &source;
	shape_iter.target_override = shape_target;
	
	_for_each_unit_in_shape(shape_iter, [&](UnitState &unit) {
		double tenacity = get_effective_tenacity(unit);
		double effective_duration = duration * (1.0 - tenacity);
		if (effective_duration > 0.0) {
			unit.taunt_target_id = source.instance_id;
			unit.taunt_remaining = Math::max(unit.taunt_remaining, effective_duration);
			unit.forced_target_id = source.instance_id;
			unit.forced_target_remaining = Math::max(unit.forced_target_remaining, effective_duration);
			_uc(unit).forced_target_kind = StringName("taunt");
		}
	});
}

void TeamfightSimulationCore::_apply_aoe_dot_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double attack_damage_ratio, double max_hp_ratio, double flat_amount, double duration, double tick_interval, const StringName &damage_type, const StringName &stacking_mode, int max_stacks, const StringName &effect_type, const String &reason, bool target_self, const StringName &action_kind, bool is_dynamic) {
	StringName source_team = source.team;
	StringName enemy_team = source_team == StringName("player") ? StringName("enemy") : StringName("player");
	const std::vector<int64_t> &enemy_indices = _alive_indices_for_team(enemy_team);
	UnitState *shape_target = target;
	if (shape_target == nullptr && effect.aoe_shape_params.target_id != 0) {
		shape_target = _unit_by_id(effect.aoe_shape_params.target_id);
	}
	_viewer_record_aoe_shape_fx(source, shape_target, effect.aoe_shape_params, StringName("aoe_dot"));
	
	AoShapeIterationParams shape_iter;
	shape_iter.shape_params = effect.aoe_shape_params;
	shape_iter.indices = &enemy_indices;
	shape_iter.spatial_team = enemy_team;
	shape_iter.exclude_instance_id = target_self ? 0 : source.instance_id;
	shape_iter.source = &source;
	shape_iter.target_override = shape_target;
	
	_for_each_unit_in_shape(shape_iter, [&](UnitState &unit) {
		_apply_dot(source, unit, attack_damage_ratio, max_hp_ratio, flat_amount, duration, tick_interval, damage_type, stacking_mode, max_stacks, effect_type, reason, action_kind, is_dynamic);
	});
}

void TeamfightSimulationCore::_apply_aoe_hot_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double max_hp_ratio, double current_hp_ratio, double missing_hp_ratio, double flat_amount, double duration, double tick_interval, const StringName &stacking_mode, int max_stacks, bool allow_overheal, const StringName &effect_type, const String &reason, bool target_self, const StringName &action_kind, bool is_dynamic) {
	StringName source_team = source.team;
	StringName ally_team = source_team == StringName("player") ? StringName("player") : StringName("enemy");
	const std::vector<int64_t> &ally_indices = _alive_indices_for_team(ally_team);
	UnitState *shape_target = target;
	if (shape_target == nullptr && effect.aoe_shape_params.target_id != 0) {
		shape_target = _unit_by_id(effect.aoe_shape_params.target_id);
	}
	_viewer_record_aoe_shape_fx(source, shape_target, effect.aoe_shape_params, StringName("aoe_hot"));
	
	AoShapeIterationParams shape_iter;
	shape_iter.shape_params = effect.aoe_shape_params;
	shape_iter.indices = &ally_indices;
	shape_iter.spatial_team = ally_team;
	shape_iter.exclude_instance_id = target_self ? 0 : source.instance_id;
	shape_iter.source = &source;
	shape_iter.target_override = shape_target;
	
	_for_each_unit_in_shape(shape_iter, [&](UnitState &unit) {
		_apply_hot(source, unit, max_hp_ratio, current_hp_ratio, missing_hp_ratio, flat_amount, duration, tick_interval, stacking_mode, max_stacks, allow_overheal, effect_type, reason, action_kind, is_dynamic);
	});
}

double TeamfightSimulationCore::_apply_aoe_damage_shape_per_target(UnitState &source, UnitState *target, const EffectRecord &effect, double damage_ratio, double flat_amount, double max_hp_ratio, double splash_ratio, const StringName &damage_type, const StringName &action_kind) {
	StringName source_team = source.team;
	StringName enemy_team = source_team == StringName("player") ? StringName("enemy") : StringName("player");
	const std::vector<int64_t> &enemy_indices = _alive_indices_for_team(enemy_team);
	double total_damage = 0.0;
	UnitState *shape_target = target;
	if (shape_target == nullptr && effect.aoe_shape_params.target_id != 0) {
		shape_target = _unit_by_id(effect.aoe_shape_params.target_id);
	}
	_viewer_record_aoe_shape_fx(source, shape_target, effect.aoe_shape_params, StringName("aoe_damage"));
	
	AoShapeIterationParams shape_iter;
	shape_iter.shape_params = effect.aoe_shape_params;
	shape_iter.indices = &enemy_indices;
	shape_iter.spatial_team = enemy_team;
	shape_iter.exclude_instance_id = 0;
	shape_iter.source = &source;
	shape_iter.target_override = shape_target;
	
	_for_each_unit_in_shape(shape_iter, [&](UnitState &unit) {
		// Calculate damage per-target using target's max_hp
		double target_damage = get_effective_max_hp(unit) * max_hp_ratio;
		target_damage += get_effective_attack_damage(source) * damage_ratio;
		target_damage += flat_amount;
		if (splash_ratio != 1.0) {
			target_damage *= splash_ratio;
		}
		EffectContext context = _build_context(source, &unit, nullptr, target_damage, action_kind);
		total_damage += _apply_damage(source, unit, target_damage, damage_type, action_kind, context);
	});
	return total_damage;
}

double TeamfightSimulationCore::_apply_aoe_damage_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double damage, const StringName &damage_type, const StringName &action_kind) {
	StringName source_team = source.team;
	StringName enemy_team = source_team == StringName("player") ? StringName("enemy") : StringName("player");
	const std::vector<int64_t> &enemy_indices = _alive_indices_for_team(enemy_team);
	double total_damage = 0.0;
	UnitState *shape_target = target;
	if (shape_target == nullptr && effect.aoe_shape_params.target_id != 0) {
		shape_target = _unit_by_id(effect.aoe_shape_params.target_id);
	}
	_viewer_record_aoe_shape_fx(source, shape_target, effect.aoe_shape_params, StringName("aoe_damage"));
	
	AoShapeIterationParams shape_iter;
	shape_iter.shape_params = effect.aoe_shape_params;
	shape_iter.indices = &enemy_indices;
	shape_iter.spatial_team = enemy_team;
	shape_iter.exclude_instance_id = 0;
	shape_iter.source = &source;
	shape_iter.target_override = shape_target;
	
	_for_each_unit_in_shape(shape_iter, [&](UnitState &unit) {
		EffectContext context = _build_context(source, &unit, nullptr, damage, action_kind);
		total_damage += _apply_damage(source, unit, damage, damage_type, action_kind, context);
	});
	return total_damage;
}


void TeamfightSimulationCore::_apply_aoe_slow_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double slow_percentage, double duration) {
	StringName enemy_team = source.team == sn_player() ? sn_enemy() : sn_player();
	const std::vector<int64_t> &enemy_indices = _alive_indices_for_team(enemy_team);
	UnitState *shape_target = target;
	if (shape_target == nullptr && effect.aoe_shape_params.target_id != 0) {
		shape_target = _unit_by_id(effect.aoe_shape_params.target_id);
	}
	_viewer_record_aoe_shape_fx(source, shape_target, effect.aoe_shape_params, StringName("aoe_slow"));
	
	AoShapeIterationParams shape_iter;
	shape_iter.shape_params = effect.aoe_shape_params;
	shape_iter.indices = &enemy_indices;
	shape_iter.spatial_team = enemy_team;
	shape_iter.exclude_instance_id = 0;
	shape_iter.source = &source;
	shape_iter.target_override = shape_target;
	
	_for_each_unit_in_shape(shape_iter, [&](UnitState &unit) {
		_apply_slow(source, unit, slow_percentage, duration);
	});
}

void TeamfightSimulationCore::_apply_aoe_stun_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double duration) {
	StringName enemy_team = source.team == sn_player() ? sn_enemy() : sn_player();
	const std::vector<int64_t> &enemy_indices = _alive_indices_for_team(enemy_team);
	UnitState *shape_target = target;
	if (shape_target == nullptr && effect.aoe_shape_params.target_id != 0) {
		shape_target = _unit_by_id(effect.aoe_shape_params.target_id);
	}
	_viewer_record_aoe_shape_fx(source, shape_target, effect.aoe_shape_params, StringName("aoe_stun"));
	
	AoShapeIterationParams shape_iter;
	shape_iter.shape_params = effect.aoe_shape_params;
	shape_iter.indices = &enemy_indices;
	shape_iter.spatial_team = enemy_team;
	shape_iter.exclude_instance_id = 0;
	shape_iter.source = &source;
	shape_iter.target_override = shape_target;
	
	_for_each_unit_in_shape(shape_iter, [&](UnitState &unit) {
		_apply_stun(source, unit, duration);
	});
}

void TeamfightSimulationCore::_apply_aoe_root_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double duration) {
	StringName enemy_team = source.team == sn_player() ? sn_enemy() : sn_player();
	const std::vector<int64_t> &enemy_indices = _alive_indices_for_team(enemy_team);
	UnitState *shape_target = target;
	if (shape_target == nullptr && effect.aoe_shape_params.target_id != 0) {
		shape_target = _unit_by_id(effect.aoe_shape_params.target_id);
	}
	_viewer_record_aoe_shape_fx(source, shape_target, effect.aoe_shape_params, StringName("aoe_root"));
	
	AoShapeIterationParams shape_iter;
	shape_iter.shape_params = effect.aoe_shape_params;
	shape_iter.indices = &enemy_indices;
	shape_iter.spatial_team = enemy_team;
	shape_iter.exclude_instance_id = 0;
	shape_iter.source = &source;
	shape_iter.target_override = shape_target;
	
	_for_each_unit_in_shape(shape_iter, [&](UnitState &unit) {
		_apply_root(source, unit, duration);
	});
}

void TeamfightSimulationCore::_apply_aoe_silence_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double duration, bool block_abilities, bool block_ultimate) {
	StringName enemy_team = source.team == sn_player() ? sn_enemy() : sn_player();
	const std::vector<int64_t> &enemy_indices = _alive_indices_for_team(enemy_team);
	UnitState *shape_target = target;
	if (shape_target == nullptr && effect.aoe_shape_params.target_id != 0) {
		shape_target = _unit_by_id(effect.aoe_shape_params.target_id);
	}
	_viewer_record_aoe_shape_fx(source, shape_target, effect.aoe_shape_params, StringName("aoe_silence"));
	
	AoShapeIterationParams shape_iter;
	shape_iter.shape_params = effect.aoe_shape_params;
	shape_iter.indices = &enemy_indices;
	shape_iter.spatial_team = enemy_team;
	shape_iter.exclude_instance_id = 0;
	shape_iter.source = &source;
	shape_iter.target_override = shape_target;
	
	_for_each_unit_in_shape(shape_iter, [&](UnitState &unit) {
		_apply_silence(source, unit, duration, block_abilities, block_ultimate);
	});
}

void TeamfightSimulationCore::_apply_aoe_disarm_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double duration) {
	StringName enemy_team = source.team == sn_player() ? sn_enemy() : sn_player();
	const std::vector<int64_t> &enemy_indices = _alive_indices_for_team(enemy_team);
	UnitState *shape_target = target;
	if (shape_target == nullptr && effect.aoe_shape_params.target_id != 0) {
		shape_target = _unit_by_id(effect.aoe_shape_params.target_id);
	}
	_viewer_record_aoe_shape_fx(source, shape_target, effect.aoe_shape_params, StringName("aoe_disarm"));
	
	AoShapeIterationParams shape_iter;
	shape_iter.shape_params = effect.aoe_shape_params;
	shape_iter.indices = &enemy_indices;
	shape_iter.spatial_team = enemy_team;
	shape_iter.exclude_instance_id = 0;
	shape_iter.source = &source;
	shape_iter.target_override = shape_target;
	
	_for_each_unit_in_shape(shape_iter, [&](UnitState &unit) {
		_apply_disarm(source, unit, duration);
	});
}




bool TeamfightSimulationCore::_apply_aoe_knockback_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double distance, bool away_from_source) {
	if (effect.aoe_shape_params.radius <= 0.0 || distance <= 0.0) {
		return false;
	}
	StringName enemy_team = source.team == sn_player() ? sn_enemy() : sn_player();
	const std::vector<int64_t> &enemy_indices = _alive_indices_for_team(enemy_team);
	UnitState *shape_target = target;
	if (shape_target == nullptr && effect.aoe_shape_params.target_id != 0) {
		shape_target = _unit_by_id(effect.aoe_shape_params.target_id);
	}
	_viewer_record_aoe_shape_fx(source, shape_target, effect.aoe_shape_params, StringName("aoe_knockback"));

	AoShapeIterationParams shape_iter;
	shape_iter.shape_params = effect.aoe_shape_params;
	shape_iter.indices = &enemy_indices;
	shape_iter.spatial_team = enemy_team;
	shape_iter.exclude_instance_id = 0;
	shape_iter.source = &source;
	shape_iter.target_override = shape_target;

	bool applied = false;
	_for_each_unit_in_shape(shape_iter, [&](UnitState &unit) {
		applied = _apply_knockback(source, unit, distance, away_from_source) || applied;
	});
	return applied;
}

void TeamfightSimulationCore::_apply_aoe_reflect_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double pct, double duration, bool all_damage_types, const StringName &action_kind, const String &reason) {
	if (effect.aoe_shape_params.radius <= 0.0 || duration <= 0.0 || pct <= 0.0) {
		return;
	}
	StringName ally_team = source.team == sn_player() ? sn_player() : sn_enemy();
	const std::vector<int64_t> &ally_indices = _alive_indices_for_team(ally_team);
	UnitState *shape_target = target;
	if (shape_target == nullptr && effect.aoe_shape_params.target_id != 0) {
		shape_target = _unit_by_id(effect.aoe_shape_params.target_id);
	}
	_viewer_record_aoe_shape_fx(source, shape_target, effect.aoe_shape_params, StringName("aoe_reflect"));

	AoShapeIterationParams shape_iter;
	shape_iter.shape_params = effect.aoe_shape_params;
	shape_iter.indices = &ally_indices;
	shape_iter.spatial_team = ally_team;
	shape_iter.exclude_instance_id = 0;
	shape_iter.source = &source;
	shape_iter.target_override = shape_target;

	StringName damage_type = all_damage_types ? StringName("all") : StringName("physical");

	_for_each_unit_in_shape(shape_iter, [&](UnitState &ally) {
		_apply_reflect_buff(source, ally, pct, duration, action_kind, damage_type, reason);
	});
}

std::vector<TeamfightSimulationCore::UnitState*> TeamfightSimulationCore::_select_targets(UnitState &source, UnitState *target, int64_t target_count, TargetSelectionStrategy strategy, bool include_source, ExcessTargetHandling excess_handling, const StringName &team_filter) {
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

TeamfightSimulationCore::UnitState *TeamfightSimulationCore::_select_enemy_target(UnitState &unit, bool profile_sim) {
	// Python forced-target model: if a forced target is active, selection collapses to it.
	int64_t forced_target_id = unit.forced_target_id;
	if (forced_target_id != 0 && unit.forced_target_remaining > 0.0) {
		UnitState *forced_target = _unit_by_id(forced_target_id);
		if (forced_target != nullptr && forced_target->alive && forced_target->team != unit.team) {
			unit.retarget_timer = 0.0;
			unit.current_target_score = -1000.0;
			_set_current_target(unit, *forced_target);
			return forced_target;
		}
	}

	int64_t taunt_id = unit.taunt_target_id;
	if (taunt_id != 0 && unit.taunt_remaining > 0.0) {
		UnitState *taunt_target = _unit_by_id(taunt_id);
		if (taunt_target != nullptr && taunt_target->alive && taunt_target->team != unit.team) {
			unit.retarget_timer = 0.0;
			unit.current_target_score = -1000.0;
			_set_current_target(unit, *taunt_target);
			return taunt_target;
		}
	}

	// Python Unit._retarget_if_needed behavior:
	// - "forced reasons" (no/invalid/dead target) always evaluate immediately.
	// - retarget_timer blocks evaluation only if there is no special condition.
	// - target_switch_lock_timer does NOT block evaluation; it only blocks switching (handled in _should_switch).
	const int64_t current_target_index = _target_index_for_unit(unit);
	UnitState *current_target_live = current_target_index >= 0 ? &_units[static_cast<size_t>(current_target_index)] : nullptr;
	const TargetingFrameEntry *current_target = current_target_index >= 0 && current_target_index < int64_t(_targeting_frame.size())
			? &_targeting_frame[static_cast<size_t>(current_target_index)]
			: nullptr;
	bool forced_reason = true;
	if (current_target != nullptr && current_target->alive && current_target->is_player_team != (unit.team == sn_player()) && current_target->stealth_remaining <= 0.0) {
		forced_reason = false;
	}

	const UnitStrategy &strategy = _strategy_for_unit(unit);
	const TickContext &ctx = _tick_ctx;
	const bool unit_is_player = unit.team == sn_player();
	const bool unit_is_assassin_role = unit.is_assassin_role;

	// Assassin frontline pressure bypass: evaluate even if retarget_timer > 0.
	bool assassin_pressuring_frontline = false;
	if (!forced_reason && unit_is_assassin_role && current_target != nullptr) {
		if (current_target->is_tank_role || current_target->is_fighter_role) {
			const int bl_alive = unit_is_player ? ctx.player_backliner_alive_count : ctx.enemy_backliner_alive_count;
			assassin_pressuring_frontline = bl_alive > 0;
		}
	}

	// Periodic keep path: if we are within retarget interval and no special conditions, just keep current target.
	if (!forced_reason && unit.retarget_timer > 0.0 && !assassin_pressuring_frontline) {
		if (_sim_profile_targeting_active) {
			_sim_profile_tgt_retarget_keeps += 1;
		}
		// Score is diagnostic-only; retarget cooldown means no gameplay decision needs recomputing.
		if (current_target_live != nullptr) {
			_set_current_target(unit, *current_target_live);
		}
		return current_target_live;
	}

	TargetScoreContext score_ctx;
	score_ctx.attack_range = _attack_range(unit);
	score_ctx.effective_range = _effective_attack_range(unit);
	score_ctx.use_spatial = _use_spatial_broad_phase();
	if (strategy.prefers_kiting) {
		score_ctx.has_kite_bounds = true;
		score_ctx.kite_min_w = score_ctx.effective_range * KITE_TARGET_WINDOW_MIN_FACTOR;
		score_ctx.kite_max_w = score_ctx.effective_range * KITE_TARGET_WINDOW_MAX_FACTOR;
	} else {
		score_ctx.has_kite_bounds = false;
	}
	const int64_t unit_instance_id = unit.instance_id;
	const int64_t unit_forced_target_id = unit.forced_target_id;
	const double unit_forced_target_remaining = unit.forced_target_remaining;
	const int64_t unit_current_ally_target_id = unit.current_ally_target_id;
	const double unit_attack_damage = get_effective_attack_damage(unit);
	const std::vector<int64_t> &frontline_indices = unit_is_player ? ctx.enemy_frontline_indices : ctx.player_frontline_indices;
	const std::vector<int64_t> &enemy_indices = unit_is_player ? _alive_enemy_indices : _alive_player_indices;

	const TargetingFrameEntry *ally_for_peel = nullptr;
	if (unit_current_ally_target_id != 0) {
		int64_t ally_index = _unit_index_by_id(unit_current_ally_target_id);
		if (ally_index >= 0 && ally_index < int64_t(_targeting_frame.size())) {
			ally_for_peel = &_targeting_frame[static_cast<size_t>(ally_index)];
		}
	}
	auto is_under_threat = [&](const TargetingFrameEntry &u) -> bool {
		return double(u.incoming_target_count) >= SUPPORT_PEEL_THREAT_THRESHOLD || u.perceived_threat >= SUPPORT_PEEL_THREAT_THRESHOLD;
	};
	const bool unit_under_threat = double(unit.incoming_target_count) >= SUPPORT_PEEL_THREAT_THRESHOLD || unit.perceived_threat >= SUPPORT_PEEL_THREAT_THRESHOLD;
	const bool unit_has_forced_target = unit_forced_target_id != 0 && unit_forced_target_remaining > 0.0;
	const bool ally_for_peel_under_threat = ally_for_peel != nullptr && ally_for_peel->alive && is_under_threat(*ally_for_peel);
	const bool unit_can_peel_for_ally = unit_current_ally_target_id != 0 && ally_for_peel_under_threat;
	const bool prefers_kiting = strategy.prefers_kiting && !unit_under_threat && score_ctx.has_kite_bounds;
	const bool has_execute_bonus = strategy.execute_bonus_weight > 0.0;

	// Python: whenever we do evaluate, we reset the retarget timer immediately (even if we end up keeping).
	unit.retarget_timer = RETARGET_INTERVAL;

	bool current_target_raw_valid = false;
	double current_target_raw = 0.0;
	double current_target_dist_for_switch = -1.0;
	const double unit_x = unit.pos_x;
	const double unit_y = unit.pos_y;
	UnitState *best_live = nullptr;
	int64_t best_index = -1;
	double best_adjusted = std::numeric_limits<double>::infinity();
	double best_raw = std::numeric_limits<double>::infinity();
	double best_dist = std::numeric_limits<double>::infinity();
	
	// Debug: store all candidate scores for target switch logging
	std::vector<std::pair<int64_t, double>> candidate_scores;
	
	if (_sim_profile_targeting_active) {
		_sim_profile_tgt_enemy_scans += 1;
	}
	for (int64_t enemy_index : enemy_indices) {
		const TargetingFrameEntry &candidate = _targeting_frame[static_cast<size_t>(enemy_index)];
		// Skip stealthed enemy units (cannot be targeted)
		if (candidate.stealth_remaining > 0.0) {
			continue;
		}
		double dx = candidate.pos_x - unit_x;
		double dy = candidate.pos_y - unit_y;
		double dist_sq = dx * dx + dy * dy;
		if (_sim_profile_targeting_active) {
			_sim_profile_tgt_candidates_scored += 1;
		}
		double dist = Math::sqrt(dist_sq);
		
		// Debug: accumulate breakdown for best candidate when debug enabled
		ScoreBreakdown local_breakdown;
		ScoreBreakdown *breakdown_ptr = _debug_targeting_scoring ? &local_breakdown : nullptr;
		
		double raw = _score_enemy_target(unit, candidate, ally_for_peel, strategy, ctx, score_ctx, dist, profile_sim, enemy_index, breakdown_ptr);
		
		// Debug: store candidate score
		candidate_scores.push_back({candidate.instance_id, raw});
		
		if (current_target_index >= 0 && enemy_index == current_target_index) {
			current_target_raw = raw;
			current_target_raw_valid = true;
			current_target_dist_for_switch = dist;
		}
		// Simplified tiebreaking: (raw_score, distance, instance_id)
		// Ties are extremely rare due to continuous float scoring (distance, HP ratio, weighted factors)
		bool is_better = best_live == nullptr;
		if (!is_better) {
			const int64_t best_instance_id = _targeting_frame[static_cast<size_t>(best_index)].instance_id;
			if (_sim_profile_targeting_active) {
				if (raw == best_raw) {
					_sim_profile_tgt_ties_adjusted += 1;
					if (dist == best_dist) {
						_sim_profile_tgt_ties_distance += 1;
						_sim_profile_tgt_ties_instance += 1;
					}
				}
			}
			is_better = raw < best_raw
					|| (raw == best_raw && dist < best_dist)
					|| (raw == best_raw && dist == best_dist && candidate.instance_id < best_instance_id);
		}
		if (is_better) {
			best_live = &_units[static_cast<size_t>(enemy_index)];
			best_index = enemy_index;
			best_adjusted = raw;
			best_raw = raw;
			best_dist = dist;
		}
	}
	if (best_live == nullptr) {
		unit.target_id = 0;
			unit.target_index = -1;
		unit.current_target_score = 0.0;
		_sync_targeting_frame_unit(unit);
		return nullptr;
	}

	const TargetingFrameEntry *best_frame = &_targeting_frame[static_cast<size_t>(best_index)];

	// Debug: print candidate scores for all target evaluations
	if (_debug_targeting_scoring) {
		String msg = "[TARGET EVAL] Unit " + String::num_int64(unit.instance_id) + " (" + String(_uc(unit).archetype_id) + ") current target " + String::num_int64(unit.target_id) + " best " + String::num_int64(best_live->instance_id) + " (score " + String::num_real(best_raw) + "). All candidates:";
		UtilityFunctions::print(msg);
		for (const auto &pair : candidate_scores) {
			const UnitStateCold &c = _uc(_units[_unit_index_by_id(pair.first)]);
			String candidate_msg = "  - " + String::num_int64(pair.first) + " (" + String(c.archetype_id) + "): " + String::num_real(pair.second);
			UtilityFunctions::print(candidate_msg);
		}
	}

	// Debug: print detailed breakdown for best candidate when debug enabled
	if (_debug_targeting_scoring) {
		ScoreBreakdown best_breakdown;
		ScoreBreakdown *breakdown_ptr = &best_breakdown;
		double best_dist = _distance_between_coords(unit.pos_x, unit.pos_y, best_frame->pos_x, best_frame->pos_y);
		_score_enemy_target(unit, *best_frame, ally_for_peel, strategy, ctx, score_ctx, best_dist, profile_sim, best_index, breakdown_ptr);
		best_breakdown.total = best_raw;
		_print_score_breakdown(best_breakdown, _uc(unit).archetype_id, _uc(_units[static_cast<size_t>(best_index)]).archetype_id);
	}

	// Forced reasons: always pick immediately (no switch logic).
	if (forced_reason) {
		_set_current_target(unit, *best_live);
		unit.current_target_score = best_raw;
		return best_live;
	}

	if (current_target != nullptr && best_frame->instance_id == current_target->instance_id) {
		unit.current_target_score = best_raw;
		if (current_target_live != nullptr) {
			_set_current_target(unit, *current_target_live);
		}
		return current_target_live;
	}
	double current_score = 0.0;
	if (current_target_raw_valid) {
		current_score = current_target_raw;
	} else if (current_target != nullptr && current_target_live != nullptr) {
		double current_dist = _distance_between_coords(unit.pos_x, unit.pos_y, current_target->pos_x, current_target->pos_y);
		current_score = _score_enemy_target(unit, *current_target, ally_for_peel, strategy, ctx, score_ctx, current_dist, profile_sim, current_target_index, nullptr);
	}

	if (assassin_pressuring_frontline) {
		bool best_is_backliner = (best_frame->is_marksman_role || best_frame->is_mage_role || best_frame->is_support_role);
		if (best_is_backliner) {
			_set_current_target(unit, *best_live);
			unit.target_switch_lock_timer = 0.0;
			unit.current_target_score = best_raw;
			return best_live;
		}
	}

	// Target stickiness optimization: extend retarget_timer if current target is "good enough"
	if (current_target_live != nullptr && current_score <= best_raw + TARGET_STICKINESS_THRESHOLD) {
		// Current target is good enough - extend timer instead of resetting
		unit.retarget_timer = RETARGET_INTERVAL + STICKINESS_RETARGET_BONUS;
		unit.current_target_score = current_score;
		_set_current_target(unit, *current_target_live);
		return current_target_live;
	}

	if (current_target_live == nullptr || !_should_switch(unit, current_score, best_raw, strategy, current_target_dist_for_switch)) {
		unit.current_target_score = current_score;
		if (current_target_live != nullptr) {
			_set_current_target(unit, *current_target_live);
		}
		return current_target_live;
	}

	_set_current_target(unit, *best_live);
	unit.target_switch_lock_timer = TARGET_SWITCH_LOCK_DURATION;
	unit.current_target_score = best_raw;
	return best_live;
}

TeamfightSimulationCore::UnitState *TeamfightSimulationCore::_select_ally_target(UnitState &unit) {
	// Python unit.py: only tanks/support run ally selection each tick.
	const UnitStrategy &strat = _strategy_for_unit(unit);
	bool allow_ally = strat.uses_ally_targeting;
	if (!allow_ally) {
		allow_ally = unit.is_support_role || unit.is_tank_role;
	}
	if (!allow_ally) {
		unit.current_ally_target_id = 0;
		return nullptr;
	}

	const std::vector<int64_t> &ally_indices = _alive_indices_for_team(unit.team);
	if (ally_indices.empty()) {
		unit.current_ally_target_id = 0;
		return nullptr;
	}
	if (_sim_profile_targeting_active) {
		_sim_profile_tgt_ally_scans += 1;
	}
	_scratch_critical_allies.clear();
	for (int64_t ally_index : ally_indices) {
		const TargetingFrameEntry &candidate = _targeting_frame[static_cast<size_t>(ally_index)];
		double hp_ratio = candidate.hp / Math::max(0.0001, candidate.max_hp);
		if (hp_ratio <= ALLY_CRITICAL_HP_RATIO) {
			_scratch_critical_allies.push_back(ally_index);
		}
	}
	const std::vector<int64_t> &pool = _scratch_critical_allies.empty() ? ally_indices : _scratch_critical_allies;
	UnitState *best = nullptr;
	double best_score = std::numeric_limits<double>::infinity();
	double best_dist = std::numeric_limits<double>::infinity();
	double best_hp_ratio = std::numeric_limits<double>::infinity();
	for (int64_t ally_index : pool) {
		const TargetingFrameEntry &candidate = _targeting_frame[static_cast<size_t>(ally_index)];
		double dist = _distance_between_coords(unit.pos_x, unit.pos_y, candidate.pos_x, candidate.pos_y);
		double score = _score_ally_target(unit, candidate, strat, dist);
		double hp_ratio = candidate.hp / Math::max(0.0001, candidate.max_hp);
		if (_scratch_critical_allies.empty()) {
			// Python: min by (score, distance, instance_id)
			bool is_better = best == nullptr;
			if (!is_better) {
				is_better = score < best_score
						|| (score == best_score && dist < best_dist)
						|| (score == best_score && dist == best_dist && candidate.instance_id < best->instance_id);
			}
			if (is_better) {
				best = &_units[static_cast<size_t>(ally_index)];
				best_score = score;
				best_dist = dist;
				best_hp_ratio = hp_ratio;
			}
			continue;
		}
		// Python critical allies: min by (hp_ratio, score, distance, instance_id)
		bool is_better = best == nullptr;
		if (!is_better) {
			is_better = hp_ratio < best_hp_ratio
					|| (hp_ratio == best_hp_ratio && score < best_score)
					|| (hp_ratio == best_hp_ratio && score == best_score && dist < best_dist)
					|| (hp_ratio == best_hp_ratio && score == best_score && dist == best_dist && candidate.instance_id < best->instance_id);
		}
		if (is_better) {
			best = &_units[static_cast<size_t>(ally_index)];
			best_score = score;
			best_dist = dist;
			best_hp_ratio = hp_ratio;
		}
	}
	unit.current_ally_target_id = best == nullptr ? 0 : best->instance_id;
	return best;
}

double TeamfightSimulationCore::_distance_between(const UnitState &left, const UnitState &right) const {
	return _distance_between_coords(left.pos_x, left.pos_y, right.pos_x, right.pos_y);
}

bool TeamfightSimulationCore::_position_collides_with_unit(double x, double y, int64_t exclude_instance_id) const {
	double collision_radius = UNIT_COLLISION_RADIUS;
	double collision_radius_sq = collision_radius * collision_radius * 4.0; // (2 * r)^2
	for (const UnitState &unit : _units) {
		if (unit.instance_id == exclude_instance_id) {
			continue;
		}
		if (!unit.alive) {
			continue;
		}
		double unit_x = unit.pos_x;
		double unit_y = unit.pos_y;
		double dx = x - unit_x;
		double dy = y - unit_y;
		double dist_sq = dx * dx + dy * dy;
		if (dist_sq < collision_radius_sq) {
			return true;
		}
	}
	return false;
}

Vector2 TeamfightSimulationCore::_find_valid_dash_position(double start_x, double start_y, double target_x, double target_y, double max_distance, int64_t exclude_instance_id) const {
	double collision_radius = UNIT_COLLISION_RADIUS;
	Vector2 direction(target_x - start_x, target_y - start_y);
	if (direction.length_squared() < EPSILON * EPSILON) {
		return Vector2(start_x, start_y);
	}
	direction = direction.normalized();
	
	// Check if destination is valid
	if (!_position_collides_with_unit(target_x, target_y, exclude_instance_id)) {
		return Vector2(target_x, target_y);
	}
	
	// Search outward from target and inward from target
	double step_size = 0.01;
	double outward_dist = step_size;
	double inward_dist = step_size;
	double target_to_start_dist = _distance_between_coords(target_x, target_y, start_x, start_y);
	
	while (outward_dist <= max_distance || inward_dist <= target_to_start_dist) {
		// Test outward positions (beyond target)
		if (outward_dist <= max_distance) {
			double test_x_out = target_x + direction.x * outward_dist;
			double test_y_out = target_y + direction.y * outward_dist;
			test_x_out = Math::clamp(test_x_out, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
			test_y_out = Math::clamp(test_y_out, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
			if (!_position_collides_with_unit(test_x_out, test_y_out, exclude_instance_id)) {
				return Vector2(test_x_out, test_y_out);
			}
		}
		
		// Test inward positions (between start and target)
		if (inward_dist <= target_to_start_dist) {
			double test_x_in = target_x - direction.x * inward_dist;
			double test_y_in = target_y - direction.y * inward_dist;
			test_x_in = Math::clamp(test_x_in, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
			test_y_in = Math::clamp(test_y_in, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
			if (!_position_collides_with_unit(test_x_in, test_y_in, exclude_instance_id)) {
				return Vector2(test_x_in, test_y_in);
			}
		}
		
		outward_dist += step_size;
		inward_dist += step_size;
	}
	
	// No valid position found, return start position
	return Vector2(start_x, start_y);
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
	return get_effective_attack_range(unit);
}

double TeamfightSimulationCore::_effective_attack_range(const UnitState &unit) const {
	double attack_range = _attack_range(unit);
	// Add melee contact buffer for melee units
	if (attack_range <= RANGED_THRESHOLD) {
		return attack_range + MELEE_CONTACT_BUFFER;
	}
	// Add ranged contact buffer for ranged units
	return attack_range + RANGED_CONTACT_BUFFER;
}

TeamfightSimulationCore::UnitState *TeamfightSimulationCore::_unit_by_id(int64_t instance_id) {
	auto it = _unit_index_map.find(instance_id);
	if (it == _unit_index_map.end()) {
		return nullptr;
	}
	int64_t index = it->second;
	if (index < 0 || index >= int64_t(_units.size())) {
		return nullptr;
	}
	return &_units[index];
}

const TeamfightSimulationCore::UnitState *TeamfightSimulationCore::_unit_by_id(int64_t instance_id) const {
	auto it = _unit_index_map.find(instance_id);
	if (it == _unit_index_map.end()) {
		return nullptr;
	}
	int64_t index = it->second;
	if (index < 0 || index >= int64_t(_units.size())) {
		return nullptr;
	}
	return &_units[index];
}

int64_t TeamfightSimulationCore::_unit_index_by_id(int64_t instance_id) const {
	auto it = _unit_index_map.find(instance_id);
	if (it != _unit_index_map.end()) {
		return it->second;
	}
	return -1;
}

int64_t TeamfightSimulationCore::_target_index_for_unit(UnitState &unit) {
	if (unit.target_id == 0) {
		unit.target_index = -1;
		return -1;
	}
	if (unit.target_index >= 0 && unit.target_index < int64_t(_units.size())) {
		const UnitState &target = _units[static_cast<size_t>(unit.target_index)];
		if (target.instance_id == unit.target_id) {
			return unit.target_index;
		}
	}
	unit.target_index = _unit_index_by_id(unit.target_id);
	return unit.target_index;
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
		_run_on_takedown_effects(*killer_unit, target, killer_damage, true, StringName("takedown"), takedown_context);
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
			_run_on_takedown_effects(*assist_unit, target, damage, false, StringName("takedown"), takedown_context);
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
	_clear_all_stat_modifiers(unit);
		
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
	if (!unit.alive) {
		SimProfileAccScope _uu_dead(profile_sim, _sim_profile_uu_dead_respawn);
		unit.respawn_timer = Math::max(0.0, unit.respawn_timer - _tick_rate);
		if (unit.respawn_timer <= 0.0) {
			_respawn_unit(unit);
		}
		return;
	}

	const UnitStrategy &strategy = _strategy_for_unit(unit);
	{
		SimProfileAccScope _uu_cc(profile_sim, _sim_profile_uu_cooldowns_cc);

		if (unit.attack_cooldown > 0.0) {
			SimProfileAccScope _ucc_acd(profile_sim, _sim_profile_ucc_attack_cd);
			// Prevent attack cd from decrementing while channeling/casting
			if (!_uc(unit).is_channeling && !(unit.casting_remaining >= 0.0 && unit.has_casting_effect)) {
				unit.attack_cooldown -= _tick_rate;
				if (unit.attack_cooldown < 0.0) {
					unit.attack_cooldown = 0.0;
				}
			}
		}
		if (unit.ability_cooldown > 0.0) {
			SimProfileAccScope _ucc_abcd(profile_sim, _sim_profile_ucc_ability_cd);
			unit.ability_cooldown -= _tick_rate;
			if (unit.ability_cooldown < 0.0) {
				unit.ability_cooldown = 0.0;
			}
		}
		if (unit.retarget_timer > 0.0) {
			SimProfileAccScope _ucc_ret(profile_sim, _sim_profile_ucc_retarget);
			unit.retarget_timer -= _tick_rate;
			if (unit.retarget_timer < 0.0) {
				unit.retarget_timer = 0.0;
			}
		}
		if (unit.target_switch_lock_timer > 0.0) {
			SimProfileAccScope _ucc_tsw(profile_sim, _sim_profile_ucc_target_switch);
			unit.target_switch_lock_timer -= _tick_rate;
			if (unit.target_switch_lock_timer < 0.0) {
				unit.target_switch_lock_timer = 0.0;
			}
		}
		if (unit.stun_remaining > 0.0) {
			SimProfileAccScope _ucc_stun(profile_sim, _sim_profile_ucc_stun);
			unit.hard_cc_seconds += Math::min(unit.stun_remaining, _tick_rate);
			unit.stun_remaining -= _tick_rate;
			if (unit.stun_remaining < 0.0) {
				unit.stun_remaining = 0.0;
			}
		}
		if (unit.slow_remaining > 0.0) {
			SimProfileAccScope _ucc_slow(profile_sim, _sim_profile_ucc_slow);
			unit.slow_remaining -= _tick_rate;
			if (unit.slow_remaining <= 0.0) {
				unit.slow_remaining = 0.0;
				unit.slow_move_mult = 1.0;
			}
		}
		if (unit.root_remaining > 0.0) {
			SimProfileAccScope _ucc_root(profile_sim, _sim_profile_ucc_root);
			unit.root_remaining -= _tick_rate;
			if (unit.root_remaining < 0.0) {
				unit.root_remaining = 0.0;
			}
		}
		if (unit.silence_ability_remaining > 0.0 || unit.silence_ultimate_remaining > 0.0) {
			SimProfileAccScope _ucc_sil(profile_sim, _sim_profile_ucc_silence);
			unit.silence_ability_remaining -= _tick_rate;
			if (unit.silence_ability_remaining < 0.0) {
				unit.silence_ability_remaining = 0.0;
			}
		}
		unit.silence_ultimate_remaining -= _tick_rate;
		if (unit.silence_ultimate_remaining < 0.0) {
			unit.silence_ultimate_remaining = 0.0;
		}
		unit.silence_remaining = Math::max(unit.silence_ability_remaining, unit.silence_ultimate_remaining);
		unit.silence_blocks_abilities = unit.silence_ability_remaining > 0.0;
		unit.silence_blocks_ultimates = unit.silence_ultimate_remaining > 0.0;
		if (unit.disarm_remaining > 0.0) {
			SimProfileAccScope _ucc_dis(profile_sim, _sim_profile_ucc_disarm);
			unit.disarm_remaining -= _tick_rate;
			if (unit.disarm_remaining < 0.0) {
				unit.disarm_remaining = 0.0;
			}
		}
		if (unit.stealth_remaining > 0.0) {
			SimProfileAccScope _ucc_ste(profile_sim, _sim_profile_ucc_stealth);
			unit.stealth_remaining -= _tick_rate;
			if (unit.stealth_remaining <= 0.0) {
				unit.stealth_remaining = 0.0;
				unit.stealth_break_on_attack = false;
				unit.stealth_break_on_ability = false;
				unit.stealth_break_on_damage_taken = false;
			}
		}
		if (unit.shield > 0.0) {
			SimProfileAccScope _ucc_shi(profile_sim, _sim_profile_ucc_shield);
			unit.shield *= (1.0 - SHIELD_DECAY_RATE * _tick_rate);
			if (unit.shield < 0.01) {
				unit.shield = 0.0;
			}
		}
		if (!_uc(unit).reflect_buffs.empty()) {
			SimProfileAccScope _ucc_ref(profile_sim, _sim_profile_ucc_reflect);
			auto &reflect_buffs = _uc(unit).reflect_buffs;
			size_t index = 0;
			while (index < reflect_buffs.size()) {
				reflect_buffs[index].remaining_duration = Math::max(0.0, reflect_buffs[index].remaining_duration - _tick_rate);
				if (reflect_buffs[index].remaining_duration <= 0.0) {
					reflect_buffs.erase(reflect_buffs.begin() + static_cast<std::ptrdiff_t>(index));
				} else {
					++index;
				}
			}
		}
		if (unit.taunt_remaining > 0.0) {
			SimProfileAccScope _ucc_tau(profile_sim, _sim_profile_ucc_taunt);
			unit.taunt_remaining = Math::max(0.0, unit.taunt_remaining - _tick_rate);
			if (unit.taunt_remaining <= 0.0) {
				unit.taunt_remaining = 0.0;
				unit.taunt_target_id = 0;
			}
		}
		if (unit.forced_target_remaining > 0.0) {
			SimProfileAccScope _ucc_ft(profile_sim, _sim_profile_ucc_forced_target);
			unit.forced_target_remaining = Math::max(0.0, unit.forced_target_remaining - _tick_rate);
		}
		if (unit.last_kite_timer > 0.0) {
			unit.last_kite_timer = Math::max(0.0, unit.last_kite_timer - _tick_rate);
		}

		bool has_temporary_stat_modifiers = !unit.stat_modifiers.is_empty()
#define X(name, def, min_val, max_val) || unit.stat_temp_##name > 0.0
				STAT_LIST
#undef X
				;
		
		// Update stacks first so temp tracker resets take effect before clearing expired modifiers
		if (!unit.stat_stacks.is_empty()) {
			_update_stacks(unit, _tick_rate, _time);
		}
		
		if (has_temporary_stat_modifiers) {
			_update_stat_modifier_durations(unit, _tick_rate);
			_clear_expired_stat_modifiers(unit);
		}
		if (unit.forced_target_remaining <= 0.0) {
			unit.forced_target_remaining = 0.0;
			unit.forced_target_id = 0;
			_uc(unit).forced_target_kind = StringName();
		}
	}

	// Apply overtime damage during sudden death
	if (_sudden_death_ticks > 0) {
		double damage_rate = OVERTIME_DAMAGE_BASE_RATE; //+ (OVERTIME_DAMAGE_INCREASE_RATE * _sudden_death_ticks);
		
		for (UnitState &unit : _units) {
			if (!unit.alive) {
				continue;
			}
			
			double max_hp = get_effective_max_hp(unit);
			double damage = 1 + max_hp * damage_rate;
			
			// Apply shield absorption first (bypass _apply_damage to avoid passives/stats)
			double shield_before = unit.shield;
			double absorbed = Math::min(shield_before, damage);
			unit.shield = Math::max(0.0, shield_before - absorbed);
			double hp_loss = Math::max(0.0, damage - absorbed);
			unit.hp = Math::max(0.0, unit.hp - hp_loss);

			// Handle death
			if (unit.hp <= 0.0 && unit.alive) {
				unit.alive = false;
							// Update kill counters
				if (unit.team == sn_player()) {
					_enemy_kills++;
				} else {
					_player_kills++;
				}
			}
		}
	}

	{
		SimProfileAccScope _uu_sep(profile_sim, _sim_profile_uu_separation);
		if (unit.stun_remaining <= 0.0 && unit.root_remaining <= 0.0) {
			double move_speed = get_effective_move_speed(unit) * _movement_speed_multiplier(unit);
			if (move_speed > 0.0) {
				double attack_range = get_effective_attack_range(unit);
				double radius = attack_range > SEPARATION_RANGE_THRESHOLD ? SEPARATION_RADIUS_RANGED : SEPARATION_RADIUS_MELEE;
				double r2 = radius * radius;
				double threshold_r2 = 4.0 * r2;
				double ux = unit.pos_x;
				double uy = unit.pos_y;
				double sep_x = 0.0;
				double sep_y = 0.0;
				const std::vector<int64_t> &ally_indices = _alive_indices_for_team(unit.team);
				auto accumulate_separation_from_ally = [&](int64_t idx) {
					const UnitState &ally = _units[idx];
					if (!ally.alive || ally.instance_id == unit.instance_id) {
						return;
					}
					double ax = ally.pos_x;
					double ay = ally.pos_y;
					double dx = ux - ax;
					double dy = uy - ay;
					double d2 = dx * dx + dy * dy;
					// Early-out: skip units beyond 2x separation radius
					if (d2 <= EPSILON || d2 >= threshold_r2) {
						return;
					}
					if (d2 >= r2) {
						return;
					}
					double d = Math::sqrt(d2);
					double force = (radius - d) / radius;
					sep_x += (dx / d) * force;
					sep_y += (dy / d) * force;
				};
				if (int64_t(ally_indices.size()) >= SPATIAL_SEPARATION_TEAM_THRESHOLD) {
					_spatial_fill_buckets_for_indices(ally_indices);
					_spatial_stamp_separation_candidates(ux, uy, radius, unit.team, unit.instance_id);
					for (int64_t idx : ally_indices) {
						if (!_spatial_stamp_has(idx)) {
							continue;
						}
						accumulate_separation_from_ally(idx);
					}
				} else {
					for (int64_t idx : ally_indices) {
						accumulate_separation_from_ally(idx);
					}
				}
				if (!Math::is_zero_approx(sep_x) || !Math::is_zero_approx(sep_y)) {
					double nudge_speed = move_speed * NUDGE_SPEED_MODIFIER * _tick_rate;
					double nx = sep_x * nudge_speed;
					double ny = sep_y * nudge_speed;
					unit.pos_x = Math::clamp(ux + nx, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
					unit.pos_y = Math::clamp(uy + ny, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
				}
			}
		}
	}

	{
		SimProfileAccScope _uu_ta(profile_sim, _sim_profile_uu_threat_and_assist);
		double old_threat = unit.perceived_threat;
		unit.perceived_threat = Math::max(0.0, unit.perceived_threat - strategy.threat_decay_rate * _tick_rate);
		// Only sync targeting frame if threat changed meaningfully (threshold: 0.001)
		if (Math::abs(unit.perceived_threat - old_threat) >= 0.001) {
			_sync_targeting_frame_unit(unit);
		}

		_prune_assist_window(unit);
	}

	{
		SimProfileAccScope _uu_regen(profile_sim, _sim_profile_uu_regen_on_tick);
		UnitStateCold &cold = _uc(unit);
		const std::vector<EffectRecord> &effects = cold.passive_effects[EFFECT_BUCKET_ON_TICK];

		// Early-out: skip if no on_tick effects and not channeling
		bool has_regen_work = !effects.empty() || _uc(unit).is_channeling;
		if (!has_regen_work) {
			// No regen work to do this tick
		} else {
			{
				SimProfileAccScope _ur_eff(profile_sim, _sim_profile_ur_effects);
				if (cold.on_tick_effect_accumulators.size() < effects.size()) {
					cold.on_tick_effect_accumulators.resize(effects.size(), 0.0);
				}
				for (size_t effect_index = 0; effect_index < effects.size(); ++effect_index) {
					const EffectRecord &effect = effects[effect_index];
					double &accumulator = cold.on_tick_effect_accumulators[effect_index];
					accumulator += _tick_rate;
					if (accumulator >= effect.on_tick_interval) {
						accumulator -= effect.on_tick_interval;
						EffectContext context = _build_context(unit, nullptr, nullptr, 0.0, sn_passive());
						_execute_effect(effect, context);
					}
				}
			}

			// Process channel effects
			if (_uc(unit).is_channeling) {
				SimProfileAccScope _ur_chn(profile_sim, _sim_profile_ur_channel);
				_process_channel_tick(unit, _tick_rate);
			}
		}

		// Tick periodic effects (DoT/HoT) independently of on_tick effects
		if (!cold.periodic_effects.empty()) {
			SimProfileAccScope _ur_per(profile_sim, _sim_profile_ur_periodic);
			_tick_periodic_effects(unit, _tick_rate);
		}

		if (unit.stun_remaining > 0.0) {
			return;
		}
	}

	bool should_return_casting = false;
	if (unit.casting_remaining >= 0.0 && unit.has_casting_effect) {
		SimProfileAccScope _uu_cast(profile_sim, _sim_profile_uu_casting);
		unit.casting_remaining = Math::max(0.0, unit.casting_remaining - _tick_rate);
		if (unit.casting_remaining <= 0.0) {
			_resolve_cast(unit);
			unit.cast_resolved_this_tick = true;
		}
		should_return_casting = true;
	}
	if (should_return_casting) {
		return;
	}

	UnitState *target_ally = nullptr;
	UnitState *target = nullptr;
	{
		SimProfileAccScope _uu_tgt(profile_sim, _sim_profile_uu_targeting);
		target_ally = _select_ally_target(unit);
		unit.current_ally_target_id = target_ally == nullptr ? 0 : target_ally->instance_id;
		target = _select_enemy_target(unit, profile_sim);
		if (target == nullptr) {
			unit.target_id = 0;
				unit.target_index = -1;
			_sync_targeting_frame_unit(unit);
			return;
		}
	}

	{
		SimProfileAccScope _uu_combat(profile_sim, _sim_profile_uu_combat);
		{
			SimProfileAccScope _uc_dc(profile_sim, _sim_profile_uc_distance_calc);
			double effective_range = _effective_attack_range(unit);
			double dx = target->pos_x - unit.pos_x;
			double dy = target->pos_y - unit.pos_y;
			double dist_sq = dx * dx + dy * dy;
			double range_sq = effective_range * effective_range;
			bool in_contact = (dist_sq <= range_sq); // No buffer for testing
			double distance = Math::sqrt(dist_sq);

			// Action priority: check range requirements per action type
			// Current system: abilities use champion's attack_range when requires_target_in_range is true
			// Future enhancement: use cast_range stat (defined in stat_definitions.hpp but unused)
			// to enable independent ability ranges separate from auto-attack range
			bool can_cast_ultimate = in_contact || !unit.ultimate_requires_target_in_range;
			bool can_cast_ability = in_contact || !unit.ability_requires_target_in_range;

			if (can_cast_ultimate) {
				SimProfileAccScope _uc_ab(profile_sim, _sim_profile_uc_ability);
				if (_try_cast_ultimate(unit, *target, distance)) {
					return;
				}
			}
			if (can_cast_ability) {
				SimProfileAccScope _uc_ab2(profile_sim, _sim_profile_uc_ability);
				if (_try_cast_ability(unit, *target, distance)) {
					return;
				}
			}
			if (in_contact) {
				if (unit.attack_cooldown <= 0.0) {
					// Block auto-attack while channeling
					if (!_uc(unit).is_channeling) {
						if (unit.combat.attack_speed > 0.0) {
							SimProfileAccScope _uc_aa(profile_sim, _sim_profile_uc_auto_attack);
							_perform_auto_attack(unit, *target, distance);
							return;
						}
					}
				}
			}
		}
	}

	bool should_return = false;
	{
		SimProfileAccScope _uu_move(profile_sim, _sim_profile_uu_movement);
		if (unit.root_remaining > 0.0) {
			should_return = true;
		} else {
			// Movement: kite first if applicable, else move toward.
			if (strategy.prefers_kiting && (unit.attack_cooldown > 0.0 || unit.combat.attack_speed == 0.0) && unit.taunt_remaining <= 0.0) {
				SimProfileAccScope _um_kit(profile_sim, _sim_profile_um_kiting);
				if (_kite_from_enemies(unit, profile_sim)) {
					should_return = true;
				}
			}
			// For melee champions: move while attacking until reaching actual attack range
			if (!should_return) {
				SimProfileAccScope _um_tow(profile_sim, _sim_profile_um_toward);
				double distance = _distance_between(unit, *target);
				double effective_range = _effective_attack_range(unit);
				double actual_attack_range = _attack_range(unit);

				// Move if not at actual attack range yet (allows movement while attacking at effective range)
				if (distance > actual_attack_range) {
					// Use actual attack range for movement target (allows closing gap while attacking)
					_move_toward_target_with_range(unit, *target, actual_attack_range);
				}
			}
		}
	}
	if (should_return) {
		return;
	}
}

bool TeamfightSimulationCore::_try_cast_ability(UnitState &unit, UnitState &target, double distance) {
	if (unit.silence_remaining > 0.0 && unit.silence_blocks_abilities) {
		return false;
	}
	if (unit.root_remaining > 0.0 && unit.has_ability_effect && _effect_record_contains_opcode(_uc(unit).ability_effect, EFFECT_OPCODE_SELF_DASH)) {
		return false;
	}
	if (unit.ability_cooldown > 0.0) {
		return false;
	}
	// Block ability cast while channeling
	if (_uc(unit).is_channeling) {
		return false;
	}
	return _start_cast(unit, target, distance, StringName("ability"));
}

bool TeamfightSimulationCore::_try_cast_ultimate(UnitState &unit, UnitState &target, double distance) {
	if (unit.silence_remaining > 0.0 && unit.silence_blocks_ultimates) {
		return false;
	}
	if (unit.root_remaining > 0.0 && unit.has_ultimate_effect && _effect_record_contains_opcode(_uc(unit).ultimate_effect, EFFECT_OPCODE_SELF_DASH)) {
		return false;
	}
	// Block ultimate cast while channeling
	if (_uc(unit).is_channeling) {
		return false;
	}
	double max_mana = get_effective_max_mana(unit);
	if (max_mana <= 0.0 || unit.mana < max_mana) {
		return false;
	}
	return _start_cast(unit, target, distance, StringName("ultimate"));
}

bool TeamfightSimulationCore::_start_cast(UnitState &unit, UnitState &target, double distance, const StringName &action_kind) {
	bool has_effect = action_kind == sn_ability() ? unit.has_ability_effect : unit.has_ultimate_effect;
	if (!has_effect) {
		return false;
	}
	UnitState *target_ally = _select_ally_target(unit);
	if (action_kind == StringName("ability")) {
		_uc(unit).abilities += 1;
	} else {
		unit.mana = Math::max(0.0, unit.mana - get_effective_max_mana(unit));
	}
	UnitStateCold &ucast = _uc(unit);
	ucast.casting_kind = action_kind;
	ucast.casting_effect = action_kind == sn_ability() ? ucast.ability_effect : ucast.ultimate_effect;
	double windup = CASTING_WINDUP;
	if (ucast.casting_effect.windup >= 0.0) {
		windup = ucast.casting_effect.windup;
	}
	unit.casting_remaining = windup;
	unit.has_casting_effect = true;
	unit.casting_target_id = unit.target_id != 0 ? unit.target_id : target.instance_id;
	unit.casting_ally_target_id = unit.current_ally_target_id != 0 ? unit.current_ally_target_id : (target_ally == nullptr ? 0 : target_ally->instance_id);

	_emit_trace(sn_cast_start(), unit.instance_id, target.instance_id, action_kind == sn_ultimate() ? 1.0 : 0.0);

	return true;
}

void TeamfightSimulationCore::_resolve_cast(UnitState &unit) {
	// Break stealth on ability cast finish if configured
	if (unit.stealth_remaining > 0.0 && unit.stealth_break_on_ability) {
		unit.stealth_remaining = 0.0;
		unit.stealth_break_on_attack = false;
		unit.stealth_break_on_ability = false;
		unit.stealth_break_on_damage_taken = false;
	}
	UnitStateCold &c = _uc(unit);
	EffectRecord effect = c.casting_effect;
	StringName action_kind = c.casting_kind;
	UnitState *target = _unit_by_id(unit.casting_target_id);
	UnitState *target_ally = _unit_by_id(unit.casting_ally_target_id);
	bool had_effect = unit.has_casting_effect;
	unit.casting_remaining = 0.0;
	c.casting_kind = StringName();
	c.casting_effect = EffectRecord();
	unit.has_casting_effect = false;
	unit.casting_target_id = 0;
	unit.casting_ally_target_id = 0;
	if (!had_effect) {
		return;
	}
	
	// Start cooldown after cast resolves
	if (action_kind == sn_ability()) {
		unit.ability_cooldown = get_effective_ability_cd(unit);
	}
	
	// Python parity: only fail the cast for projectile abilities when the enemy target is gone.
	// Self-targeting abilities (shield, heal) always succeed using source as fallback.
	// Matches Python's execute_ability which returns False only for use_projectile with dead target.
	// Extended: also fail when target is stealthed (target no longer valid).
	auto _effect_uses_projectile = [](const EffectRecord &e, const auto &self) -> bool {
		if (e.opcode == EFFECT_OPCODE_PROJECTILE) return true;
		if (e.opcode == EFFECT_OPCODE_MULTI_EFFECT) {
			for (const EffectRecord &child : e.children) {
				if (self(child, self)) return true;
			}
		}
		if (e.opcode == EFFECT_OPCODE_MULTI_TARGET) {
			for (const EffectRecord &sub_effect : e.sub_effects) {
				if (self(sub_effect, self)) return true;
			}
		}
		return false;
	};
	if (_effect_uses_projectile(effect, _effect_uses_projectile) && (target == nullptr || !target->alive || target->stealth_remaining > 0.0)) {
		if (action_kind == StringName("ability")) {
			unit.ability_cooldown = 0.0;
		} else if (action_kind == StringName("ultimate")) {
			double effective_max_mana = get_effective_max_mana(unit);
			unit.mana = Math::min(effective_max_mana, unit.mana + effective_max_mana);
		}
		return;
	}
	EffectContext context = _build_context(unit, target, target_ally, 0.0, action_kind);
	Dictionary result = _execute_effect(effect, context);
}

void TeamfightSimulationCore::_perform_auto_attack(UnitState &unit, UnitState &target, double distance) {
	if (unit.disarm_remaining > 0.0) {
		return;
	}
	// Break attack if target is stealthed (target no longer valid)
	if (target.stealth_remaining > 0.0) {
		return;
	}
	_uc(unit).auto_attacks += 1;
	unit.attack_count += 1;
	// Python parity: damage + on_attack modifiers first, then projectile/hit,
	// then mana gain, then attack cooldown (resolve_attack → mana → cooldown).
	double damage = get_effective_attack_damage(unit);
	damage = _apply_attack_modifiers(unit, target, distance, damage);
	if (get_effective_attack_range(unit) > RANGED_THRESHOLD) {
		ProjectileState projectile;
		projectile.source_id = unit.instance_id;
		projectile.target_id = target.instance_id;
		projectile.damage = damage;
		projectile.damage_type = sn_physical();
		projectile.stun_duration = DEFAULT_PROJECTILE_STUN_DURATION;
		projectile.radius = get_effective_projectile_radius(unit);
		projectile.speed = Math::max(0.0001, get_effective_projectile_speed(unit));
		projectile.pos_x = unit.pos_x;
		projectile.pos_y = unit.pos_y;
		projectile.action_kind = sn_auto();
		projectile.reason = String("Auto Attack");
		_projectiles.push_back(projectile);
		_emit_trace(StringName("projectile"), unit.instance_id, target.instance_id, damage);
	} else {
		EffectContext context = _build_context(unit, &target, nullptr, damage, StringName("auto"));
		double dealt = _apply_damage(unit, target, damage, StringName("physical"), StringName("auto"), context);
		_emit_trace(StringName("auto_melee"), unit.instance_id, target.instance_id, dealt);
		_run_post_attack_effects(unit, target, dealt, context);
		double life_steal = get_effective_life_steal(unit);
		if (life_steal > 0.0) {
			double old_hp = unit.hp;
			double heal_amount = dealt * life_steal;
			_heal_unit(unit, unit, heal_amount, StringName("auto"));
			double heal_gained = unit.hp - old_hp;
			_run_post_heal_effects(unit, unit, heal_amount, heal_gained, StringName("auto"), context);
		}
	}
	// Python parity: mana gain happens after attack resolution.
	double mana_gain = get_effective_mana_per_attack(unit);
	if (mana_gain > 0.0) {
		double max_mana = get_effective_max_mana(unit);
		unit.mana = Math::min(max_mana, unit.mana + mana_gain);
	}
	// Python parity: attack cooldown set after mana gain.
	double attack_speed = Math::max(0.0001, get_effective_attack_speed(unit));
	unit.attack_cooldown = 1.0 / attack_speed;
	unit.attack_period = unit.attack_cooldown;
}

void TeamfightSimulationCore::_move_toward_target(UnitState &unit, UnitState &target) {
	double speed = get_effective_move_speed(unit) * _movement_speed_multiplier(unit) * _tick_rate;
	if (unit.last_kite_timer > 0.0) {
		speed *= KITE_SPEED_MODIFIER;
	}
	if (speed <= 0.0) {
		return;
	}
	double dx = target.pos_x - unit.pos_x;
	double dy = target.pos_y - unit.pos_y;
	double distance = _distance_between_coords(unit.pos_x, unit.pos_y, target.pos_x, target.pos_y);
	if (distance <= EPSILON) {
		return;
	}
	double effective_range = _effective_attack_range(unit);
	double desired_step = Math::max(0.0, distance - effective_range);
	double max_step = Math::min(speed, desired_step);
	if (max_step <= 0.0) {
		return;
	}
	double nx = dx / distance;
	double ny = dy / distance;
	unit.pos_x = Math::clamp(unit.pos_x + nx * max_step, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
	unit.pos_y = Math::clamp(unit.pos_y + ny * max_step, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
}

void TeamfightSimulationCore::_move_toward_target_with_range(UnitState &unit, UnitState &target, double target_range) {
	double speed = get_effective_move_speed(unit) * _movement_speed_multiplier(unit) * _tick_rate;
	if (unit.last_kite_timer > 0.0) {
		speed *= KITE_SPEED_MODIFIER;
	}
	if (speed <= 0.0) {
		return;
	}
	double dx = target.pos_x - unit.pos_x;
	double dy = target.pos_y - unit.pos_y;
	double distance = _distance_between_coords(unit.pos_x, unit.pos_y, target.pos_x, target.pos_y);
	if (distance <= EPSILON) {
		return;
	}
	double desired_step = Math::max(0.0, distance - target_range);
	double max_step = Math::min(speed, desired_step);
	if (max_step <= 0.0) {
		return;
	}
	double nx = dx / distance;
	double ny = dy / distance;
	unit.pos_x = Math::clamp(unit.pos_x + nx * max_step, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
	unit.pos_y = Math::clamp(unit.pos_y + ny * max_step, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
}

bool TeamfightSimulationCore::_kite_from_enemies(UnitState &unit, bool profile_sim) {
	const StringName &enemy_team = unit.team == sn_player() ? sn_enemy() : sn_player();
	const std::vector<int64_t> &enemy_indices = _alive_indices_for_team(enemy_team);
	if (enemy_indices.empty()) {
		return false;
	}
	double attack_range = _attack_range(unit);
	double danger_radius = attack_range * KITE_DANGER_THRESHOLD;
	if (danger_radius <= 0.0) {
		return false;
	}
	double danger_r2 = danger_radius * danger_radius;
	double ux = unit.pos_x;
	double uy = unit.pos_y;
	double rep_x = 0.0;
	double rep_y = 0.0;
	int count = 0;
	if (_use_spatial_broad_phase()) {
		SimProfileAccScope _um_ksp(profile_sim, _sim_profile_um_kiting_spatial);
		_spatial_fill_buckets_for_indices(enemy_indices);
		_spatial_stamp_kite_threat(ux, uy, danger_radius);
		for (int64_t idx : enemy_indices) {
			if (!_spatial_stamp_has(idx)) {
				continue;
			}
			const UnitState &enemy = _units[idx];
			if (!enemy.alive) {
				continue;
			}
			double ex = enemy.pos_x;
			double ey = enemy.pos_y;
			double dx = ux - ex;
			double dy = uy - ey;
			double d2 = dx * dx + dy * dy;
			if (d2 <= EPSILON || d2 >= danger_r2) {
				continue;
			}
			double d = Math::sqrt(d2);
			double weight = 1.0 / d;
			rep_x += (dx / d) * weight;
			rep_y += (dy / d) * weight;
			count += 1;
		}
	} else {
		SimProfileAccScope _um_kbr(profile_sim, _sim_profile_um_kiting_brute);
		for (int64_t idx : enemy_indices) {
			const UnitState &enemy = _units[idx];
			if (!enemy.alive) {
				continue;
			}
			double ex = enemy.pos_x;
			double ey = enemy.pos_y;
			double dx = ux - ex;
			double dy = uy - ey;
			double d2 = dx * dx + dy * dy;
			if (d2 <= EPSILON || d2 >= danger_r2) {
				continue;
			}
			double d = Math::sqrt(d2);
			double weight = 1.0 / d;
			rep_x += (dx / d) * weight;
			rep_y += (dy / d) * weight;
			count += 1;
		}
	}
	if (count <= 0) {
		return false;
	}
	double mag = _distance_between_coords(0.0, 0.0, rep_x, rep_y);
	if (mag <= EPSILON) {
		return false;
	}
	double vel_x = rep_x / mag;
	double vel_y = rep_y / mag;
	const double boundary_safe_min = WORLD_BOUNDARY_MIN + BOUNDARY_DETECTION_MARGIN + 1.0;
	const double boundary_safe_max = WORLD_BOUNDARY_MAX - BOUNDARY_DETECTION_MARGIN - 1.0;
	if (ux >= boundary_safe_min && ux <= boundary_safe_max && uy >= boundary_safe_min && uy <= boundary_safe_max) {
		// Unit is far from boundaries, skip boundary checks
	} else {
		bool blocked_x = (ux <= WORLD_BOUNDARY_MIN + BOUNDARY_DETECTION_MARGIN && vel_x < 0.0) || (ux >= WORLD_BOUNDARY_MAX - BOUNDARY_DETECTION_MARGIN && vel_x > 0.0);
		bool blocked_y = (uy <= WORLD_BOUNDARY_MIN + BOUNDARY_DETECTION_MARGIN && vel_y < 0.0) || (uy >= WORLD_BOUNDARY_MAX - BOUNDARY_DETECTION_MARGIN && vel_y > 0.0);
		if (blocked_x) {
			vel_x = 0.0;
		}
		if (blocked_y) {
			vel_y = 0.0;
		}
	}
	if (Math::is_zero_approx(vel_x) && Math::is_zero_approx(vel_y)) {
		vel_x = ux < (WORLD_SIZE * 0.5) ? RECOVERY_VELOCITY : -RECOVERY_VELOCITY;
		vel_y = uy < (WORLD_SIZE * 0.5) ? RECOVERY_VELOCITY : -RECOVERY_VELOCITY;
	}
	// Python parity: renormalize after boundary blocking so wall-hugging units
	// still move at full kite speed (Python unit_movement.py lines 99-101).
	double new_mag = _distance_between_coords(0.0, 0.0, vel_x, vel_y);
	if (new_mag <= EPSILON) {
		return false;
	}
	vel_x /= new_mag;
	vel_y /= new_mag;
	unit.last_kite_timer = KITE_DURATION;
	double move_speed = get_effective_move_speed(unit) * _movement_speed_multiplier(unit);
	double step = move_speed * KITE_SPEED_MODIFIER * _tick_rate;
	double new_x = Math::clamp(ux + vel_x * step, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
	double new_y = Math::clamp(uy + vel_y * step, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
	double dx = new_x - ux;
	double dy = new_y - uy;
	if (dx * dx + dy * dy > EPSILON) {
		unit.pos_x = new_x;
		unit.pos_y = new_y;
		_sync_targeting_frame_unit(unit);
	}
	return true;
}

void TeamfightSimulationCore::_resolve_projectile(const ProjectileState &projectile) {
	UnitState *source = _unit_by_id(projectile.source_id);
	UnitState *target = _unit_by_id(projectile.target_id);
	// Python parity: projectiles resolve even if the source has since died.
	// Only cancel if the target is gone (no unit to hit) or source record is missing.
	if (source == nullptr || target == nullptr || !target->alive) {
		return;
	}
	double damage = projectile.damage;
	StringName damage_type = projectile.damage_type;
	StringName action_kind = projectile.action_kind;
	EffectContext context = _build_context(*source, target, nullptr, damage, action_kind);
	double dealt = _apply_damage(*source, *target, damage, damage_type, action_kind, context);
	_run_post_attack_effects(*source, *target, dealt, context);
	if (projectile.action_kind == sn_auto()) {
		double life_steal = get_effective_life_steal(*source);
		if (life_steal > 0.0) {
			double old_hp = source->hp;
			double heal_amount = dealt * life_steal;
			_heal_unit(*source, *source, heal_amount, sn_auto());
			double heal_gained = source->hp - old_hp;
			_run_post_heal_effects(*source, *source, heal_amount, heal_gained, sn_auto(), context);
		}
	}
	if (projectile.stun_duration > 0.0 && target->alive) {
		_apply_stun(*source, *target, projectile.stun_duration);
	}
}

void TeamfightSimulationCore::_update_projectiles() {
	// Python world.py: each tick, projectiles step toward the target's *current* position
	// (homing); hit when dist <= speed * dt + radius.
	using std::swap;
	_active_projectiles.clear();
	swap(_active_projectiles, _projectiles);
	_scratch_projectiles.clear();
	for (const ProjectileState &data : _active_projectiles) {
		UnitState *target = _unit_by_id(data.target_id);
		if (target == nullptr || !target->alive) {
			continue;
		}
		double dx = target->pos_x - data.pos_x;
		double dy = target->pos_y - data.pos_y;
		double dist = _distance_between_coords(data.pos_x, data.pos_y, target->pos_x, target->pos_y);
		double move_dist = data.speed * _tick_rate;
		if (dist <= move_dist + data.radius) {
			ProjectileState hit = data;
			_resolve_projectile(hit);
			// Check if match ended after this projectile (only during overtime)
			if (_sudden_death_ticks > 0 && _player_kills != _enemy_kills) {
				// Match ended, discard remaining projectiles
				_scratch_projectiles.clear();
				break;
			}
			continue;
		}
		if (dist > EPSILON) {
			ProjectileState next = data;
			double inv = 1.0 / dist;
			next.pos_x = data.pos_x + dx * inv * move_dist;
			next.pos_y = data.pos_y + dy * inv * move_dist;
			_scratch_projectiles.push_back(next);
		} else {
			_scratch_projectiles.push_back(data);
		}
	}
	if (!_projectiles.empty()) {
		_scratch_projectiles.insert(_scratch_projectiles.end(), _projectiles.begin(), _projectiles.end());
	}
	swap(_projectiles, _scratch_projectiles);
	_scratch_projectiles.clear();
}

Dictionary TeamfightSimulationCore::_execute_effect(const EffectRecord &effect, EffectContext &context) {
	Dictionary result;
	
	// DEBUG: Log when _execute_effect is called with unknown opcode
	if (effect.opcode == EFFECT_OPCODE_UNKNOWN) {
		UtilityFunctions::push_error(vformat("[DEBUG] _execute_effect: called with EFFECT_OPCODE_UNKNOWN for source_id=%d", context.source != nullptr ? context.source->instance_id : 0));
	}
	
	if (context.source == nullptr) {
		return result;
	}
	UnitState &source = *context.source;
	UnitState *target = context.target;
	UnitState *target_ally = context.target_ally;
	
	// Check conditional requirements
	if (!_check_all_conditions(effect, context.accumulated_results, context)) {
		Dictionary failed_result;
		failed_result["success"] = false;
		failed_result["condition_failed"] = true;
		return failed_result;
	}
	
	switch (effect.opcode) {
		case EFFECT_OPCODE_MULTI_EFFECT: {
			Dictionary combined_results;
			for (const EffectRecord &child : effect.children) {
				// Update context with accumulated results before executing child
				context.accumulated_results = combined_results;
				Dictionary child_result = _execute_effect(child, context);
				// Store the result under the effect's kind name for conditional access
				const StringName &child_kind = _kind_for_opcode(child.opcode);
				if (!child_kind.is_empty()) {
					combined_results[child_kind] = child_result;
					// Also update the accumulated_results for subsequent children
					context.accumulated_results = combined_results;
				}
			}
			return combined_results;
		}
		case EFFECT_OPCODE_DAMAGE: {
			Dictionary damage_result;
			damage_result["success"] = false;
			
			// Target resolution with self-targeting support
			UnitState *damage_target = (effect.int0 == 1) ? &source : target;
			if (damage_target == nullptr) {
				return damage_result;
			}
			
			// Unified damage calculation
			double damage;
			// Use accumulated damage if requested
			if (effect.int1 != 0 && context.channel_accumulated_damage > 0.0) {
				damage = context.channel_accumulated_damage * effect.scalar1;
			} else {
				damage = get_effective_max_hp(*damage_target) * effect.scalar0;  // max_hp_ratio
				damage += get_effective_attack_damage(source) * effect.scalar1;  // damage_ratio
				damage += effect.scalar2;  // flat_amount
			}
			
			// Apply ability/ultimate modifiers if applicable
			if (context.action_kind == sn_ability()) {
				damage = _apply_ability_modifiers(source, damage_target, damage);
			} else if (context.action_kind == sn_ultimate()) {
				damage = _apply_ultimate_modifiers(source, damage_target, damage);
			}
			
			double dealt = _apply_damage(source, *damage_target, damage, effect.damage_type.is_empty() ? sn_physical() : effect.damage_type, context.action_kind, context);
			context.damage = dealt;
			
			// Minimum health check for self-damage
			if (effect.int0 == 1 && damage_target->alive && damage_target->hp <= 1.0) {
				damage_target->hp = 1.0;
			}
			
			// trigger_on_hit logic
			if (effect.scalar3 > 0.5) {
				_run_post_attack_effects(source, *damage_target, dealt, context);
				
				// Apply lifesteal for abilities with trigger_on_hit (excluding self-damage)
				if (source.instance_id != damage_target->instance_id) {
					double life_steal = get_effective_life_steal(source);
					if (life_steal > 0.0) {
						double old_hp = source.hp;
						double heal_amount = dealt * life_steal;
						_heal_unit(source, source, heal_amount, context.action_kind);
						double heal_gained = source.hp - old_hp;
						_run_post_heal_effects(source, source, heal_amount, heal_gained, context.action_kind, context);
					}
				}
			}
			damage_result["success"] = true;
			damage_result["damage_dealt"] = dealt;
			damage_result["target_killed"] = !damage_target->alive;
			return damage_result;
		}
		case EFFECT_OPCODE_PROJECTILE: {
			Dictionary projectile_result;
			projectile_result["success"] = false;
			if (target == nullptr) {
				return projectile_result;
			}
			ProjectileState projectile_state;
			projectile_state.source_id = source.instance_id;
			projectile_state.target_id = target->instance_id;
			double damage = get_effective_attack_damage(source) * effect.scalar2;
			
			// Apply ability/ultimate modifiers if applicable
			if (context.action_kind == sn_ability()) {
				damage = _apply_ability_modifiers(source, target, damage);
			} else if (context.action_kind == sn_ultimate()) {
				damage = _apply_ultimate_modifiers(source, target, damage);
			}
			
			projectile_state.damage = damage;
			projectile_state.damage_type = effect.damage_type.is_empty() ? sn_physical() : effect.damage_type;
			projectile_state.stun_duration = effect.scalar3;
			// Python parity: null speed/radius override → fall back to unit's projectile stats.
			double projectile_speed = (effect.scalar0 < 0.0)
				? Math::max(0.0001, get_effective_projectile_speed(source))
				: Math::max(0.0001, effect.scalar0);
			projectile_state.radius = (effect.scalar1 < 0.0)
				? get_effective_projectile_radius(source)
				: effect.scalar1;
			projectile_state.speed = projectile_speed;
			projectile_state.pos_x = source.pos_x;
			projectile_state.pos_y = source.pos_y;
			projectile_state.action_kind = context.action_kind;
			projectile_state.reason = String(effect.reason);
			_projectiles.push_back(projectile_state);
			projectile_result["success"] = true;
			projectile_result["projectile_created"] = true;
			return projectile_result;
		}
		case EFFECT_OPCODE_STUN: {
			Dictionary stun_result;
			stun_result["success"] = false;
			if (target != nullptr) {
				_apply_stun(source, *target, effect.scalar0);
				stun_result["success"] = true;
				stun_result["stun_applied"] = true;
				stun_result["duration"] = effect.scalar0;
			}
			return stun_result;
		}
		case EFFECT_OPCODE_SHIELD: {
			Dictionary shield_result;
			shield_result["success"] = true;
			UnitState &shield_target = (effect.int0 == 1) ? source : (target_ally == nullptr ? source : *target_ally);
			double amount = get_effective_max_hp(shield_target) * effect.scalar0;
			
			// Use context.damage, but fall back to accumulated damage if needed
			double damage_for_ratio = context.damage;
			if (damage_for_ratio <= 0.0 && context.accumulated_results.has("damage")) {
				Dictionary damage_result = context.accumulated_results["damage"];
				if (damage_result.has("damage_dealt")) {
					damage_for_ratio = damage_result["damage_dealt"];
				}
			}
			amount += damage_for_ratio * effect.scalar1;
			
			amount += effect.scalar2;
			
			if (amount <= 0.0) {
				shield_result["shield_applied"] = false;
				shield_result["amount"] = 0.0;
				return shield_result;
			}
			_add_shield(source, shield_target, amount, context.action_kind);
			shield_result["shield_applied"] = true;
			shield_result["amount"] = amount;
			return shield_result;
		}
		case EFFECT_OPCODE_HEAL: { 
			Dictionary heal_result;
			heal_result["success"] = true;
			UnitState &heal_target = (effect.int0 == 1) ?
				source :
				(target_ally == nullptr ? source : *target_ally);
			double heal_amount = get_effective_max_hp(heal_target) * effect.scalar0 + heal_target.hp * effect.scalar1 + effect.scalar2;
			// Add missing HP scaling
			double missing_hp = get_effective_max_hp(heal_target) - heal_target.hp;
			heal_amount += missing_hp * effect.scalar3;
			double old_hp = heal_target.hp;
			_heal_unit(source, heal_target, heal_amount, context.action_kind);
			double heal_gained = heal_target.hp - old_hp;
			_run_post_heal_effects(source, heal_target, heal_amount, heal_gained, context.action_kind, context);
			heal_result["heal_applied"] = true;
			heal_result["amount"] = heal_gained;
			return heal_result;
		}
		case EFFECT_OPCODE_AOE_TAUNT: {
			Dictionary taunt_result;
			taunt_result["success"] = true;
			_apply_aoe_taunt_shape(source, target, effect, effect.scalar1);
			taunt_result["taunt_applied"] = true;
			taunt_result["radius"] = effect.scalar0;
			taunt_result["duration"] = effect.scalar1;
			// INCONSISTENT: has extra fields (taunt_applied, radius, duration) while other AOE status effects only return success
			return taunt_result;
		}
		case EFFECT_OPCODE_AOE_DAMAGE: {
			Dictionary aoe_damage_result;
			aoe_damage_result["success"] = true;
			// Use accumulated damage if requested
			if (effect.int0 != 0 && context.channel_accumulated_damage > 0.0) {
				double aoe_damage = context.channel_accumulated_damage * effect.scalar1;
				double splash_ratio = effect.scalar2;
				if (splash_ratio != 1.0) {
					aoe_damage *= splash_ratio;
				}
				double total_damage = _apply_aoe_damage_shape(source, target, effect, aoe_damage, effect.damage_type.is_empty() ? StringName("physical") : effect.damage_type, context.action_kind);
				aoe_damage_result["aoe_damage_applied"] = true;
				aoe_damage_result["damage"] = total_damage;
				context.damage = total_damage;
				return aoe_damage_result;
			} else {
				// Calculate per-target damage using target's max_hp for max_hp_ratio
				double total_damage = _apply_aoe_damage_shape_per_target(source, target, effect, effect.scalar1, effect.scalar3, effect.scalar4, effect.scalar2, effect.damage_type.is_empty() ? StringName("physical") : effect.damage_type, context.action_kind);
				aoe_damage_result["aoe_damage_applied"] = true;
				aoe_damage_result["damage"] = total_damage;
				context.damage = total_damage;
				return aoe_damage_result;
			}
		}
	case EFFECT_OPCODE_DAMAGE_THRESHOLD_TRIGGER: {
		{
			Dictionary threshold_result;
			threshold_result["success"] = true;
			if (context.damage > get_effective_attack_damage(source) * effect.scalar0 && !effect.children.empty()) {
				Dictionary child_result = _execute_effect(effect.children[0], context);
				threshold_result["triggered"] = true;
				_merge_accumulated_results(threshold_result, child_result);
				return threshold_result;
			}
		}
		return Dictionary(); // INCONSISTENT: returns empty Dictionary instead of success=false
	}
	case EFFECT_OPCODE_DAMAGE_OVER_TIME: {
			Dictionary dot_result;
			dot_result["success"] = true;
			UnitState *dot_target = (effect.int0 == 1) ? &source : target;
			if (dot_target != nullptr) {
				_apply_dot(source, *dot_target, effect.scalar0, effect.scalar1, effect.scalar3,
						   effect.scalar4, effect.scalar2,
						   effect.damage_type.is_empty() ? StringName("physical") : effect.damage_type,
						   effect.stacking_mode, effect.int1, effect.effect_type, effect.reason, context.action_kind, effect.int2 == 1);
				dot_result["dot_applied"] = true;
			}
			return dot_result;
		}
		case EFFECT_OPCODE_HEAL_OVER_TIME: {
			Dictionary hot_result;
			hot_result["success"] = true;
			UnitState &hot_target = (effect.int0 == 1) ? 
				source : 
				(target_ally == nullptr ? source : *target_ally);
			_apply_hot(source, hot_target, effect.scalar0, effect.scalar1, effect.scalar3, effect.scalar4,
					   effect.scalar5, effect.scalar2,
					   effect.stacking_mode, effect.int1, effect.int2 != 0, effect.effect_type, effect.reason, context.action_kind, effect.int3 == 1);
			hot_result["hot_applied"] = true;
			return hot_result;
		}
		case EFFECT_OPCODE_AOE_DAMAGE_OVER_TIME: {
			Dictionary aoe_dot_result;
			aoe_dot_result["success"] = true;
			_apply_aoe_dot_shape(source, target, effect, effect.scalar1, effect.scalar2, effect.scalar3,
					   effect.scalar5, effect.scalar4,
					   effect.damage_type.is_empty() ? StringName("physical") : effect.damage_type,
					   effect.stacking_mode, effect.int0, effect.effect_type, effect.reason, effect.int2 != 0, context.action_kind, effect.int1 == 1);
			aoe_dot_result["aoe_dot_applied"] = true;
			return aoe_dot_result;
		}
		case EFFECT_OPCODE_AOE_HEAL_OVER_TIME: {
			Dictionary aoe_hot_result;
			aoe_hot_result["success"] = true;
			_apply_aoe_hot_shape(source, target, effect, effect.scalar1, effect.scalar2, effect.scalar3, effect.scalar4,
					   double(effect.int1), effect.scalar5,
					   effect.stacking_mode, effect.int0, effect.int2 != 0, effect.effect_type, effect.reason, effect.int3 != 0, context.action_kind, effect.int4 == 1);
			aoe_hot_result["aoe_hot_applied"] = true;
			return aoe_hot_result;
		}
		case EFFECT_OPCODE_MANA_REGEN: {
			Dictionary mana_result;
			mana_result["success"] = true;
			_restore_mana(source, source, effect.scalar0 + get_effective_max_mana(source) * effect.scalar1);
			mana_result["mana_restored"] = effect.scalar0 + get_effective_max_mana(source) * effect.scalar1;
			return mana_result;
		}
		case EFFECT_OPCODE_POST_DAMAGE_MANA_GAIN: {
			Dictionary post_mana_result;
			post_mana_result["success"] = true;
			_restore_mana(source, source, context.damage * effect.scalar0);
			post_mana_result["mana_restored"] = context.damage * effect.scalar0;
			return post_mana_result;
		}
		case EFFECT_OPCODE_DAMAGE_BASED_HEAL: {
			Dictionary heal_result;
			heal_result["success"] = true;
			double damage_to_use = context.damage;
			// Use accumulated damage from channel if requested
			if (effect.int0 != 0 && context.channel_accumulated_damage > 0.0) {
				damage_to_use = context.channel_accumulated_damage;
			}
			double old_hp = source.hp;
			double heal_amount = damage_to_use * effect.scalar0;
			_heal_unit(source, source, heal_amount, context.action_kind);
			double heal_gained = source.hp - old_hp;
			_run_post_heal_effects(source, source, heal_amount, heal_gained, context.action_kind, context);
			heal_result["heal_applied"] = true;
			heal_result["amount"] = heal_gained;
			return heal_result;
		}
		case EFFECT_OPCODE_DAMAGE_BASED_SHIELD: {
			Dictionary shield_result;
			shield_result["success"] = true;
			_add_shield(source, source, context.damage * effect.scalar0, context.action_kind);
			shield_result["shield_applied"] = true;
			shield_result["amount"] = context.damage * effect.scalar0;
			return shield_result;
		}
		case EFFECT_OPCODE_CONSUME_STACKS_DAMAGE: {
			Dictionary result;
			result["success"] = true;
			if (target == nullptr) {
				result["damage_dealt"] = 0.0;
				result["stacks_consumed"] = 0;
				result["target_killed"] = false;
				return result;
			}
			double attack_damage = get_effective_attack_damage(source);
			int stacks = _consume_stat_stacks(source, effect.stat_name, effect.string1);
			double base_ratio = effect.scalar0;
			double stack_bonus = effect.scalar1;
			String stacking_mode = effect.string0;
			double final_ratio = base_ratio;
			if (stacking_mode == "multiplicative") {
				final_ratio = base_ratio * (1.0 + double(stacks) * stack_bonus);
			} else {
				final_ratio = base_ratio + (double(stacks) * stack_bonus);
			}
			double damage = attack_damage * final_ratio;
			_apply_damage(source, *target, damage, effect.damage_type, context.action_kind, context);
			result["damage_dealt"] = damage;
			result["stacks_consumed"] = stacks;
			result["target_killed"] = !target->alive;
			return result;
		}
		case EFFECT_OPCODE_CONSUME_STACKS_HEAL: {
			Dictionary result;
			result["success"] = true;
			double max_hp = get_effective_max_hp(source);
			int stacks = _consume_stat_stacks(source, effect.stat_name, effect.string1);
			double base_ratio = effect.scalar0;
			double stack_bonus = effect.scalar1;
			String stacking_mode = effect.string0;
			double final_ratio = base_ratio;
			if (stacking_mode == "multiplicative") {
				final_ratio = base_ratio * (1.0 + double(stacks) * stack_bonus);
			} else {
				final_ratio = base_ratio + (double(stacks) * stack_bonus);
			}
			double heal_amount = max_hp * final_ratio;
			double old_hp = source.hp;
			_heal_unit(source, source, heal_amount, context.action_kind);
			double heal_gained = source.hp - old_hp;
			_run_post_heal_effects(source, source, heal_amount, heal_gained, context.action_kind, context);
			result["heal_applied"] = true;
			result["amount"] = heal_gained;
			result["stacks_consumed"] = stacks;
			return result;
		}
		case EFFECT_OPCODE_CONSUME_STACKS_SHIELD: {
			Dictionary result;
			result["success"] = true;
			double max_hp = get_effective_max_hp(source);
			int stacks = _consume_stat_stacks(source, effect.stat_name, effect.string1);
			double base_ratio = effect.scalar0;
			double stack_bonus = effect.scalar1;
			String stacking_mode = effect.string0;
			double final_ratio = base_ratio;
			if (stacking_mode == "multiplicative") {
				final_ratio = base_ratio * (1.0 + double(stacks) * stack_bonus);
			} else {
				final_ratio = base_ratio + (double(stacks) * stack_bonus);
			}
			double shield_amount = max_hp * final_ratio;
			_add_shield(source, source, shield_amount, context.action_kind);
			result["shield_applied"] = true;
			result["amount"] = shield_amount;
			result["stacks_consumed"] = stacks;
			return result;
		}
		case EFFECT_OPCODE_SET_STACKS: {
			Dictionary result;
			result["success"] = true;
			int stack_count = int(effect.int0);
			bool to_max = effect.int1 != 0;
			int fallback_max_stacks = int(effect.int2);
			double duration = effect.scalar0;
			double fallback_additive_per_stack = effect.scalar1;
			double fallback_multiplicative_per_stack = effect.scalar2;
			String stack_reason = effect.string0;
			_set_stat_stacks(source, effect.stat_name, stack_reason, stack_count, duration, to_max, fallback_max_stacks, fallback_additive_per_stack, fallback_multiplicative_per_stack);
			// Return the actual stack count after setting (accounting for to_max and clamping)
			String stack_key = _get_stack_key(effect.stat_name, stack_reason);
			Dictionary stack_entry = Dictionary(source.stat_stacks.get(stack_key, Dictionary()));
			int final_stacks = int(stack_entry.get("current_stacks", stack_count));
			result["stacks_set"] = final_stacks;
			return result;
		}
		case EFFECT_OPCODE_CHANNEL: {
			Dictionary channel_result;
			channel_result["success"] = true;
			
			// Check if already channeling
			if (_uc(source).is_channeling) {
				UtilityFunctions::push_error("Unit is already channeling");
				// Apply cooldown on start failure
				if (context.action_kind == sn_ability()) {
					source.ability_cooldown = get_effective_ability_cd(source);
				}
				// Ultimates: mana already consumed on cast, no additional action needed
				channel_result["success"] = false;
				return channel_result;
			}
			
			// Initialize channel state
			UnitStateCold &cold = _uc(source);
			cold.is_channeling = true;
			cold.channel_remaining_duration = effect.scalar0;  // duration
			cold.channel_tick_interval = effect.scalar1;  // tick_interval
			cold.channel_allow_movement = effect.int0 != 0;
			cold.channel_target_mode = effect.string0;
			cold.channel_reason = effect.reason;
			cold.channel_action_kind = context.action_kind;
			cold.channel_source_instance_id = source.instance_id;
			cold.channel_tick_accumulator = 0.0;
			cold.channel_accumulated_damage = 0.0;
			
			// Set target
			if (target != nullptr) {
				cold.channel_target_instance_id = target->instance_id;
			}
			
			// Set sub-effect (required for channel to function)
			if (!effect.children.empty()) {
				cold.channel_sub_effect = effect.children[0];
				// Validate sub-effect is valid
				if (cold.channel_sub_effect.opcode == 0) {
					String error_msg = "Channel sub-effect has invalid opcode. Channel reason: ";
					error_msg += cold.channel_reason;
					UtilityFunctions::push_error(error_msg);
					channel_result["success"] = false;
					return channel_result;
				}
			} else {
				String error_msg = "Channel missing required sub-effect. Channel reason: ";
				error_msg += cold.channel_reason;
				UtilityFunctions::push_error(error_msg);
				channel_result["success"] = false;
				return channel_result;
			}
			
			// Set post-complete effect
			if (effect.sub_effects.size() > 0) {
				cold.channel_post_complete_effect = effect.sub_effects[0];
			}
			
			// Set post-interrupt effect
			if (effect.sub_effects.size() > 1) {
				cold.channel_post_interrupt_effect = effect.sub_effects[1];
			}
			
			// Process first tick immediately to avoid 1-tick delay
			_process_channel_tick(source, cold.channel_tick_interval);
			
			channel_result["channel_started"] = true;
			return channel_result;
		}
		case EFFECT_OPCODE_MANA_RESTORE_ON_HIT: {
			Dictionary mana_result;
			mana_result["success"] = true;
			_restore_mana(source, source, effect.scalar0);
			mana_result["mana_restored"] = effect.scalar0;
			return mana_result;
		}
		case EFFECT_OPCODE_DRAIN_TARGET_MANA_ON_HIT: {
			Dictionary drain_result;
			drain_result["success"] = true;
			if (target != nullptr) {
				target->mana = Math::max(0.0, target->mana - effect.scalar0);
				drain_result["mana_drained"] = effect.scalar0;
			}
			return drain_result;
		}
		case EFFECT_OPCODE_EVERY_N_ATTACKS_STUN: {
			Dictionary stun_result;
			stun_result["success"] = true;
			if (effect.int0 > 0 && target != nullptr && source.attack_count % effect.int0 == 0) {
				_apply_stun(source, *target, effect.scalar0);
				stun_result["stun_applied"] = true;
				stun_result["duration"] = effect.scalar0;
			}
			return stun_result;
		}
		case EFFECT_OPCODE_SLOW: {
			Dictionary slow_result;
			slow_result["success"] = true;
			if (target != nullptr) {
				_apply_slow(source, *target, effect.scalar0, effect.scalar1);
			}
			return slow_result;
		}
		case EFFECT_OPCODE_ROOT: {
			Dictionary root_result;
			root_result["success"] = true;
			if (target != nullptr) {
				_apply_root(source, *target, effect.scalar0);
			}
			return root_result;
		}
		case EFFECT_OPCODE_SILENCE: {
			Dictionary silence_result;
			silence_result["success"] = true;
			if (target != nullptr) {
				_apply_silence(source, *target, effect.scalar0, effect.int0 != 0, effect.int1 != 0);
			}
			return silence_result;
		}
		case EFFECT_OPCODE_DISARM: {
			Dictionary disarm_result;
			disarm_result["success"] = true;
			if (target != nullptr) {
				_apply_disarm(source, *target, effect.scalar0);
			}
			return disarm_result;
		}
		case EFFECT_OPCODE_STEALTH: {
			Dictionary stealth_result;
			stealth_result["success"] = true;
			stealth_result["stealth_applied"] = false;
			bool target_self = effect.int3 != 0;
			UnitState *stealth_target = target_self ? &source : target;
			if (stealth_target != nullptr) {
				_apply_stealth(source, *stealth_target, effect.scalar0, effect.int0 != 0, effect.int1 != 0, effect.int2 != 0);
				stealth_result["stealth_applied"] = true;
			}
			return stealth_result;
		}
		case EFFECT_OPCODE_AOE_SLOW: {
			Dictionary aoe_slow_result;
			aoe_slow_result["success"] = true;
			_apply_aoe_slow_shape(source, target, effect, effect.scalar1, effect.scalar2);
			return aoe_slow_result;
		}
		case EFFECT_OPCODE_AOE_ROOT: {
			Dictionary aoe_root_result;
			aoe_root_result["success"] = true;
			_apply_aoe_root_shape(source, target, effect, effect.scalar1);
			return aoe_root_result;
		}
		case EFFECT_OPCODE_AOE_SILENCE: {
			Dictionary aoe_silence_result;
			aoe_silence_result["success"] = true;
			_apply_aoe_silence_shape(source, target, effect, effect.scalar1, effect.int0 != 0, effect.int1 != 0);
			return aoe_silence_result;
		}
		case EFFECT_OPCODE_AOE_DISARM: {
			Dictionary aoe_disarm_result;
			aoe_disarm_result["success"] = true;
			_apply_aoe_disarm_shape(source, target, effect, effect.scalar1);
			return aoe_disarm_result;
		}
		case EFFECT_OPCODE_AOE_KNOCKBACK: {
			Dictionary aoe_kb_result;
			aoe_kb_result["success"] = true;
			aoe_kb_result["knockback_applied"] = _apply_aoe_knockback_shape(source, target, effect, effect.scalar1, effect.int0 != 0);
			return aoe_kb_result;
		}
		case EFFECT_OPCODE_AOE_REFLECT: {
			Dictionary aoe_rf_result;
			aoe_rf_result["success"] = true;
			_apply_aoe_reflect_shape(source, target, effect, effect.scalar1, effect.scalar2, effect.int0 == 1, context.action_kind, effect.reason);
			return aoe_rf_result;
		}
		case EFFECT_OPCODE_AOE_STUN: {
			Dictionary aoe_stun_result;
			aoe_stun_result["success"] = true;
			_apply_aoe_stun_shape(source, target, effect, effect.scalar1);
			return aoe_stun_result;
		}
		case EFFECT_OPCODE_KNOCKBACK_SHIELD: {
			Dictionary ks_result;
			ks_result["success"] = false;
			Dictionary knockback_result = Dictionary(context.accumulated_results.get(StringName("knockback"), Dictionary()));
			if (bool(knockback_result.get("knockback_applied", false))) {
				double shield_amt = context.damage * effect.scalar0;
				_add_shield(source, source, shield_amt, context.action_kind);
				ks_result["success"] = true;
				ks_result["shield_applied"] = true;
				ks_result["amount"] = shield_amt;
			}
			return ks_result;
		}
		case EFFECT_OPCODE_KNOCKBACK: {
			Dictionary kb_result;
			kb_result["success"] = false;
			if (target != nullptr) {
				bool knocked_back = _apply_knockback(source, *target, effect.scalar0, effect.int0 != 0);
				kb_result["success"] = true;
				kb_result["knockback_applied"] = knocked_back;
			}
			return kb_result;
		}
		case EFFECT_OPCODE_REFLECT: {
			Dictionary rf_result;
			rf_result["success"] = true;
			StringName damage_type = effect.int0 == 1 ? StringName("all") : StringName("physical");
			_apply_reflect_buff(source, source, effect.scalar0, effect.scalar1, context.action_kind, damage_type, effect.reason);
			return rf_result;
		}
		case EFFECT_OPCODE_REFLECT_DAMAGE: {
			Dictionary rd_noop;
			rd_noop["success"] = true;
			// INCONSISTENT: returns only success with no information about the passive effect it enables
			return rd_noop;
		}
		case EFFECT_OPCODE_REDIRECT_DAMAGE: {
			Dictionary redirect_result;
			redirect_result["success"] = true;
			redirect_result["redirected_damage"] = 0.0;

			// redirect_damage is a passive modifier that should be applied during damage calculation
			// This effect doesn't directly apply damage, but stores parameters for the damage system
			// The actual redirect logic is handled in _apply_damage or a similar function
			// For now, this is a no-op that stores the parameters in the effect record
			return redirect_result;
		}
		case EFFECT_OPCODE_SUMMON_ALLY: {
			Dictionary summon_result;
			summon_result["success"] = true;
			summon_result["minions_spawned"] = 0;

			double spawn_radius = effect.scalar0;
			int64_t total_spawned = 0;

			// Copy source data before modifying _units (push_back can reallocate and invalidate references)
			int64_t source_instance_id = source.instance_id;
			double source_pos_x = source.pos_x;
			double source_pos_y = source.pos_y;
			StringName source_team = source.team;

			// Use tracked max instance ID for efficient ID generation
			int64_t next_instance_id = _max_instance_id + 1;

			// Max radius for expansion fallback (3x initial radius, capped at 5.0)
			constexpr double max_spawn_radius_expansion = 5.0;
			double max_spawn_radius = Math::min(spawn_radius * 3.0, max_spawn_radius_expansion);

			// Track pending spawn positions to prevent intra-batch overlap
			std::vector<Vector2> pending_positions;

			// Iterate through children (each child is a minion spec with minion_id and count)
			for (const EffectRecord &minion_spec : effect.children) {
				StringName minion_id = StringName(minion_spec.string0);
				int64_t count = minion_spec.int0;

				// Get minion data from minion_catalog
				Dictionary minion_data = _minion_catalog.get(String(minion_id), Dictionary());
				if (minion_data.is_empty()) {
					UtilityFunctions::push_error(vformat("Summon failed: unknown minion archetype: %s", String(minion_id)));
					continue;
				}

			// Spawn count minions of this type
			for (int64_t i = 0; i < count; ++i) {
				// Find random valid position within spawn_radius, with expansion fallback on failure
				Vector2 spawn_pos = _find_random_spawn_position_near_excluding_with_expansion(source_pos_x, source_pos_y, spawn_radius, max_spawn_radius, source_instance_id, pending_positions);
				if (spawn_pos.x < 0.0) {
					// Failed to find valid position even with expansion
					UtilityFunctions::push_error(vformat("Summon failed: could not find valid spawn position for minion %s (%d/%d) near (%.2f, %.2f) with radius %.2f (expanded to %.2f). Active units: %d",
						String(minion_id), i + 1, count, source_pos_x, source_pos_y, spawn_radius, max_spawn_radius, _units.size()));
					continue;
				}

				// Add to pending positions to prevent overlap with subsequent minions in this batch
				pending_positions.push_back(spawn_pos);

				// Create spawn spec
				Dictionary spawn_spec;
				spawn_spec["archetype_id"] = minion_id;
				spawn_spec["x"] = spawn_pos.x;
				spawn_spec["y"] = spawn_pos.y;

				// Queue the spawn for later processing (at end of tick)
				PendingSpawn pending;
				pending.spawn_spec = spawn_spec;
				pending.team = source_team;
				pending.summoner_instance_id = source_instance_id;
				_pending_spawns.push_back(pending);

				next_instance_id++;
				_max_instance_id = next_instance_id;
				total_spawned++;
			}
			}

			summon_result["minions_spawned"] = total_spawned;
			return summon_result;
		}
		case EFFECT_OPCODE_SELF_DASH: {
			Dictionary dash_result;
			dash_result["success"] = true;
			dash_result["reached_target"] = false;
			if (source.root_remaining > 0.0) {
				dash_result["distance_traveled"] = 0.0;
				return dash_result;
			}
			
			double dash_distance = effect.scalar0;
			double dir_x = effect.scalar1;
			double dir_y = effect.scalar2;
			
			double current_x = source.pos_x;
			double current_y = source.pos_y;
			double new_x = current_x;
			double new_y = current_y;
			
			// Check if direction is explicitly provided (non-zero values)
			bool has_direction = (dir_x != 0.0 || dir_y != 0.0);
			
			if (has_direction) {
				// Use explicit direction
				new_x = current_x + dir_x * dash_distance;
				new_y = current_y + dir_y * dash_distance;
			} else {
				// Use target from context
				if (target != nullptr) {
					int64_t target_id = target->instance_id;
					UnitState *target_unit = _unit_by_id(target_id);
					if (target_unit != nullptr) {
						double target_x = target_unit->pos_x;
						double target_y = target_unit->pos_y;
						double dx = target_x - current_x;
						double dy = target_y - current_y;
						double dist = _distance_between_coords(current_x, current_y, target_x, target_y);
						if (dist > 0.0) {
							double move_dist = Math::min(dash_distance, dist);
							new_x = current_x + (dx / dist) * move_dist;
							new_y = current_y + (dy / dist) * move_dist;
						}
					}
				}
			}
			
			// Find valid position avoiding unit collisions
			int64_t source_id = source.instance_id;
			Vector2 valid_pos = _find_valid_dash_position(current_x, current_y, new_x, new_y, dash_distance, source_id);
			new_x = valid_pos.x;
			new_y = valid_pos.y;
			
			// Clamp to boundaries (already done in _find_valid_dash_position, but ensure it)
			new_x = Math::clamp(new_x, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
			new_y = Math::clamp(new_y, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
			
			// Calculate reached_target based on final distance to target
			if (!has_direction && target != nullptr) {
				int64_t target_id = target->instance_id;
				UnitState *target_unit = _unit_by_id(target_id);
				if (target_unit != nullptr) {
					double target_x = target_unit->pos_x;
					double target_y = target_unit->pos_y;
					double final_dx = target_x - new_x;
					double final_dy = target_y - new_y;
					double final_dist = _distance_between_coords(new_x, new_y, target_x, target_y);
					// Consider target reached if we're within attack range (with melee buffer)
					bool reached = (final_dist <= get_effective_attack_range(source) + 0.1);
					dash_result["reached_target"] = reached;
				}
			}
			
			source.pos_x = new_x;
			source.pos_y = new_y;
			_sync_targeting_frame_unit(source);
			
			// Calculate actual distance traveled
			double actual_distance = _distance_between_coords(current_x, current_y, new_x, new_y);
			dash_result["distance_traveled"] = actual_distance;
			
			return dash_result;
		}
		case EFFECT_OPCODE_AUTO_DODGE:
		case EFFECT_OPCODE_CONSTANT_MULTIPLIER:
		case EFFECT_OPCODE_HP_THRESHOLD_DAMAGE_MULTIPLIER:
		case EFFECT_OPCODE_DISTANCE_THRESHOLD_MULTIPLIER:
		// INCONSISTENT: multiplier effects fallthrough to stat_modifier execution case instead of having separate logic
		case EFFECT_OPCODE_STAT_MODIFIER: {
			Dictionary stat_result;
			stat_result["success"] = true;
			if (target != nullptr) {
				UnitState &modifier_target = (effect.int0 == 1) ? source : *target;

				// Calculate heal-based additive value if heal_gained_ratio is set
				double additive_value = effect.scalar0;
				double heal_based_additive = 0.0;
				if (effect.scalar3 > 0.0) {
					heal_based_additive = context.heal_gained * effect.scalar3;
					additive_value += heal_based_additive;
				}

				bool use_stacking = effect.int2 > 1 || effect.int3 != 0;
				if (use_stacking) {
					StackBehavior stack_behavior = StackBehavior::Refresh;
					if (effect.int3 == 1) {
						stack_behavior = StackBehavior::Accumulate;
					} else if (effect.int3 == 2) {
						stack_behavior = StackBehavior::Reset;
					}
					_apply_stacked_stat_modifier(
						source,
						modifier_target,
						effect.damage_type,
						additive_value,
						effect.scalar1,
						effect.scalar2,
						effect.int1 != 0,
						int(effect.int2),
						stack_behavior,
						effect.reason
					);
					stat_result["stat_modifier_applied"] = true;
					stat_result["use_stacking"] = true;
					stat_result["max_stacks"] = effect.int2;
					stat_result["stack_behavior"] = effect.int3;
				} else {
					_apply_simple_stat_modifier(source, modifier_target, effect.damage_type, additive_value, effect.scalar1, effect.scalar2, effect.int1 != 0, effect.reason);
					stat_result["stat_modifier_applied"] = true;
					stat_result["use_stacking"] = false;
				}

				stat_result["stat_name"] = String(effect.damage_type);
				stat_result["additive"] = additive_value;
				stat_result["multiplicative"] = effect.scalar1;
				stat_result["duration"] = effect.scalar2;
				stat_result["is_match_duration"] = effect.int1 != 0;
				stat_result["target_self"] = effect.int0 == 1;
				stat_result["reason"] = effect.reason;
			}
			return stat_result;
		}
		case EFFECT_OPCODE_MULTI_TARGET: {
			Dictionary multi_result;
			multi_result["success"] = true;
			
			// Extract multi-target parameters
			int64_t target_count = effect.int0;
			TargetSelectionStrategy strategy = static_cast<TargetSelectionStrategy>(effect.int1);
			bool include_self = effect.int2 != 0;
			ExcessTargetHandling excess_handling = static_cast<ExcessTargetHandling>(effect.int3);
			int64_t repeat_count = effect.int4;
			
			// Validate target_count (must be >= 1, where -1 means "all")
			if (target_count != -1 && target_count < 1) {
				UtilityFunctions::push_error(vformat("Invalid target_count %d, must be >= 1 (-1 for all targets)", target_count));
				multi_result["success"] = false;
				return multi_result;
			}
			
			// Validate repeat_count (must be >= 1)
			if (repeat_count < 1) {
				UtilityFunctions::push_error(vformat("Invalid repeat_count %d, must be >= 1", repeat_count));
				multi_result["success"] = false;
				return multi_result;
			}
			
			if (effect.int1 < TARGET_SELECTION_CLOSEST || effect.int1 > TARGET_SELECTION_HIGHEST_PERCENT_HP) {
				UtilityFunctions::push_error(vformat("Invalid selection_strategy opcode %d for multi_target effect", effect.int1));
				multi_result["success"] = false;
				return multi_result;
			}
			
			if (effect.int3 != EXCESS_TARGET_DROP && effect.int3 != EXCESS_TARGET_STACK) {
				UtilityFunctions::push_error(vformat("Invalid excess_handling opcode %d for multi_target effect", effect.int3));
				multi_result["success"] = false;
				return multi_result;
			}
			
			// Validate sub_effects
			if (effect.sub_effects.empty()) {
				UtilityFunctions::push_error("No sub_effects provided for multi_target effect");
				multi_result["success"] = false;
				return multi_result;
			}
			
			// Validate team_filter is explicitly provided
			if (effect.team_filter.is_empty()) {
				UtilityFunctions::push_error("team_filter is required for multi_target effect, must be 'ally' or 'enemy'");
				multi_result["success"] = false;
				return multi_result;
			}
			
			// Warn about sub-effects with target_self=true
			for (const EffectRecord &sub_effect : effect.sub_effects) {
				if (sub_effect.int0 == 1) {
					UtilityFunctions::push_warning("Sub-effect has target_self=true in multi_target effect, which may override selected targets");
				}
			}
			
			// Select targets
			std::vector<UnitState*> targets = _select_targets(source, target, target_count, strategy, include_self, excess_handling, effect.team_filter);
			
			if (targets.empty()) {
				multi_result["success"] = false;
				multi_result["targets_affected"] = 0;
				return multi_result;
			}
			
			// Apply each sub-effect to each target repeat_count times
			Dictionary nested_results;
			Dictionary summary_applications;
			for (UnitState *current_target : targets) {
				if (current_target == nullptr) {
					continue;
				}
				
				int64_t target_id = current_target->instance_id;
				
				for (const EffectRecord &sub_effect : effect.sub_effects) {
					for (int64_t i = 0; i < repeat_count; ++i) {
						EffectContext sub_context = context;
						sub_context.target = current_target;
						sub_context.target_ally = current_target;
						
						// Execute sub-effect
						Dictionary sub_result = _execute_effect(sub_effect, sub_context);
						
						// Check if sub-effect failed
						if (!sub_result.has("success") || !bool(sub_result.get("success", false))) {
							continue;
						}
						
						// Get effect kind for nesting
						const StringName &effect_kind = _kind_for_opcode(sub_effect.opcode);
						if (effect_kind.is_empty()) {
							continue;
						}
						
						// Ensure nested structure exists for this effect kind
						// Convert to String for safer Dictionary key handling across GDExtension
						String effect_kind_str = String(effect_kind);
						if (!nested_results.has(effect_kind_str)) {
							Dictionary effect_dict;
							Dictionary by_target_dict;
							effect_dict["by_target"] = by_target_dict;
							effect_dict["total"] = 0.0;
							nested_results[effect_kind_str] = effect_dict;
						}
						
						Dictionary effect_dict = nested_results[effect_kind_str];
						Dictionary by_target = effect_dict["by_target"];
						
						// Add result to by_target
						if (!by_target.has(target_id)) {
							by_target[target_id] = 0.0;
						}
						if (!summary_applications.has(effect_kind_str)) {
							Dictionary summary_by_target;
							summary_applications[effect_kind_str] = summary_by_target;
						}
						Dictionary summary_by_target = summary_applications[effect_kind_str];
						if (!summary_by_target.has(target_id)) {
							Array applications;
							summary_by_target[target_id] = applications;
						}
						
						// Extract numeric value from result
						double value = 0.0;
						if (effect_kind == sn_damage()) {
							value = double(sub_result.get("damage_dealt", 0.0));
						} else if (effect_kind == sn_heal()) {
							value = double(sub_result.get("amount", 0.0));
						} else if (effect_kind == sn_shield()) {
							value = double(sub_result.get("amount", 0.0));
						}
						// For CC effects, just count applications
						else {
							value = 1.0;
						}
						
						Array applications = summary_by_target[target_id];
						applications.append(value);
						summary_by_target[target_id] = applications;
						summary_applications[effect_kind_str] = summary_by_target;
						by_target[target_id] = double(by_target[target_id]) + value;
						effect_dict["total"] = double(effect_dict["total"]) + value;
						nested_results[effect_kind_str] = effect_dict;
					}
				}
			}
			
			String summary = vformat("multi_target summary source=%d targets=%d", source.instance_id, targets.size());
			Array effect_keys = nested_results.keys();
			for (int64_t effect_index = 0; effect_index < effect_keys.size(); ++effect_index) {
				String effect_key = String(effect_keys[effect_index]);
				Dictionary effect_dict = nested_results[effect_key];
				Dictionary by_target = effect_dict["by_target"];
				Dictionary summary_by_target = summary_applications[effect_key];
				summary += vformat("\n%s:", effect_key);
				Array target_keys = by_target.keys();
				for (int64_t target_index = 0; target_index < target_keys.size(); ++target_index) {
					Variant target_key = target_keys[target_index];
					Array applications = summary_by_target[target_key];
					String values = "[";
					for (int64_t application_index = 0; application_index < applications.size(); ++application_index) {
						if (application_index > 0) {
							values += ", ";
						}
						values += vformat("%.3f", double(applications[application_index]));
					}
					values += "]";
					summary += vformat("\n  target %s: %s total=%.3f", String(target_key), values, double(by_target[target_key]));
				}
				summary += vformat("\n  total: %.3f", double(effect_dict["total"]));
			}
			if (_debug_combat_trace) {
				UtilityFunctions::print(summary);
			}
			
			multi_result["targets_affected"] = targets.size();
			multi_result["results"] = nested_results;
			multi_result["success"] = true;
			return multi_result;
		}
		default: {
			// Opcodes without runtime execution resolve here (unknown kinds compile to UNKNOWN).
			Dictionary default_result;
			default_result["success"] = true;
			return default_result;
		}
	}

	// Should never reach here - all cases should return
	UtilityFunctions::push_error(vformat("_execute_effect: fell through switch for opcode %d", effect.opcode));
	Dictionary fallback_result;
	fallback_result["success"] = false;
	fallback_result["error"] = "Unhandled opcode in _execute_effect";
	return fallback_result;
}

void TeamfightSimulationCore::_merge_accumulated_results(Dictionary &target, const Dictionary &source) {
	Array keys = source.keys();
	for (int64_t i = 0; i < keys.size(); ++i) {
		Variant key = keys[i];
		Variant source_value = source[key];
		target[key] = source_value;
	}
}

bool TeamfightSimulationCore::_check_condition(const EffectRecord &effect, const Dictionary &results) {
	if (effect.requires_result_from.is_empty()) {
		return true;  // No condition
	}

	if (!results.has(effect.requires_result_from)) {
		UtilityFunctions::push_error(vformat(
			"Missing requires_result_from '%s' for conditional effect '%s'",
			String(effect.requires_result_from),
			String(effect.reason)
		));
		return false;  // Required effect didn't run
	}

	Dictionary required_result = results[effect.requires_result_from];
	if (!required_result.has(effect.requires_field)) {
		UtilityFunctions::push_error(vformat(
			"Missing requires_field '%s' in result '%s' for conditional effect '%s'",
			String(effect.requires_field),
			String(effect.requires_result_from),
			String(effect.reason)
		));
		return false;  // Field doesn't exist
	}

	Variant actual_value = required_result[effect.requires_field];
	return actual_value == effect.requires_value;
}

bool TeamfightSimulationCore::_check_target_status_condition(const EffectRecord &effect, const EffectContext &context) {
	if (effect.requires_target_status.is_empty()) {
		return true;  // No condition
	}
	
	// Validate status string
	StringName status_to_check = effect.requires_target_status;
	bool is_valid_status = (status_to_check == sn_slow() || status_to_check == sn_root() || 
	                       status_to_check == sn_silence() || status_to_check == sn_disarm() || 
	                       status_to_check == sn_stealth() || status_to_check == sn_stun() || 
	                       status_to_check == sn_reflect());
	if (!is_valid_status) {
		UtilityFunctions::push_error(vformat("Invalid requires_target_status '%s' for effect '%s'. Valid statuses: slow, root, silence, disarm, stealth, stun, reflect", 
			String(status_to_check), String(effect.reason)));
		return false;
	}
	
	UnitState *unit_to_check = nullptr;
	if (effect.status_target == sn_source()) {
		unit_to_check = context.source;
	} else if (effect.status_target == sn_target()) {
		unit_to_check = context.target;
	} else {
		UtilityFunctions::push_error(vformat("Invalid status_target '%s' for effect '%s'", String(effect.status_target), String(effect.reason)));
		return false;
	}
	
	if (unit_to_check == nullptr) {
		return false;
	}
	
	return _target_has_status(*unit_to_check, status_to_check);
}

bool TeamfightSimulationCore::_check_all_conditions(const EffectRecord &effect, const Dictionary &results, const EffectContext &context) {
	// Check requires_result_from condition
	if (!_check_condition(effect, results)) {
		return false;
	}
	
	// Check requires_target_status condition
	if (!_check_target_status_condition(effect, context)) {
		return false;
	}
	
	return true;
}

void TeamfightSimulationCore::_merge_result(Dictionary &target_result, const Dictionary &source_result) {
	Array keys = source_result.keys();
	for (int64_t i = 0; i < keys.size(); ++i) {
		Variant key = keys[i];
		Variant source_value = source_result[key];
		if (target_result.has(key) && source_value.get_type() == Variant::FLOAT && target_result[key].get_type() == Variant::FLOAT) {
			target_result[key] = double(target_result[key]) + double(source_value);
		} else if (target_result.has(key) && source_value.get_type() == Variant::INT && target_result[key].get_type() == Variant::INT) {
			target_result[key] = int64_t(target_result[key]) + int64_t(source_value);
		} else {
			target_result[key] = source_value;
		}
	}
}

Dictionary TeamfightSimulationCore::_build_summary() {
	_summary_cache.clear();
	_summary_cache["seed"] = _seed;
	_summary_cache["winner_team"] = String(_winner_team);
	_summary_cache["duration"] = _time;
	_summary_cache["sudden_death_ticks"] = int64_t(_sudden_death_ticks);
	_summary_cache["player_kills"] = int64_t(_player_kills);
	_summary_cache["enemy_kills"] = int64_t(_enemy_kills);
	_summary_cache["player_comp"] = _player_comp;
	_summary_cache["enemy_comp"] = _enemy_comp;
	_summary_unit_stats.clear();
	
	// First pass: aggregate minion stats to their summoners
	std::unordered_map<int64_t, double> summoner_minion_damage_dealt;
	std::unordered_map<int64_t, double> summoner_minion_damage_received;
	for (const UnitState &unit : _units) {
		if (unit.summoner_instance_id != 0) {
			// This is a minion, aggregate its stats to the summoner
			const UnitStateCold &c = _uc(unit);
			summoner_minion_damage_dealt[unit.summoner_instance_id] += c.damage_dealt;
			summoner_minion_damage_received[unit.summoner_instance_id] += c.damage_received;
		}
	}
	
	for (const UnitState &unit : _units) {
		// Skip minions in unit_stats output - their damage is aggregated to summoners
		if (unit.summoner_instance_id != 0) {
			continue;
		}
		
		const UnitStateCold &c = _uc(unit);
		Dictionary unit_summary;
		unit_summary["instance_id"] = unit.instance_id;
		unit_summary["archetype"] = String(c.archetype_id);
		unit_summary["role"] = String(c.role_id);
		unit_summary["team"] = String(unit.team);
		unit_summary["won"] = _winner_team != StringName() && unit.team == _winner_team;
		unit_summary["damage_dealt"] = c.damage_dealt;
		unit_summary["damage_dealt_auto"] = c.damage_dealt_auto;
		unit_summary["damage_dealt_ability"] = c.damage_dealt_ability;
		unit_summary["damage_dealt_ultimate"] = c.damage_dealt_ultimate;
		unit_summary["damage_dealt_passive"] = c.damage_dealt_passive;
		unit_summary["damage_received"] = c.damage_received;
		unit_summary["damage_mitigated"] = c.damage_mitigated;
		unit_summary["healing_done"] = c.healing_done;
		unit_summary["healing_done_auto"] = c.healing_done_auto;
		unit_summary["healing_done_ability"] = c.healing_done_ability;
		unit_summary["healing_done_ultimate"] = c.healing_done_ultimate;
		unit_summary["healing_done_passive"] = c.healing_done_passive;
		unit_summary["shielding_done"] = c.shielding_done;
		unit_summary["shielding_done_auto"] = c.shielding_done_auto;
		unit_summary["shielding_done_ability"] = c.shielding_done_ability;
		unit_summary["shielding_done_ultimate"] = c.shielding_done_ultimate;
		unit_summary["shielding_done_passive"] = c.shielding_done_passive;
		unit_summary["auto_attacks"] = c.auto_attacks;
		unit_summary["abilities"] = c.abilities;
		unit_summary["ultimates"] = c.ultimates;
		unit_summary["stuns"] = c.stuns;
		unit_summary["kills"] = c.kills;
		unit_summary["deaths"] = c.deaths;
		unit_summary["assists"] = c.assists;
		
		// Add aggregated minion stats if this unit summoned any minions
		auto it_dealt = summoner_minion_damage_dealt.find(unit.instance_id);
		auto it_received = summoner_minion_damage_received.find(unit.instance_id);
		if (it_dealt != summoner_minion_damage_dealt.end() || it_received != summoner_minion_damage_received.end()) {
			unit_summary["minion_damage_dealt"] = it_dealt != summoner_minion_damage_dealt.end() ? it_dealt->second : 0.0;
			unit_summary["minion_damage_received"] = it_received != summoner_minion_damage_received.end() ? it_received->second : 0.0;
		} else {
			unit_summary["minion_damage_dealt"] = 0.0;
			unit_summary["minion_damage_received"] = 0.0;
		}
		
		Dictionary telemetry;
		telemetry["schema"] = String("teamfight.telemetry.v1");
		telemetry["hard_cc_seconds"] = unit.hard_cc_seconds;
		unit_summary["telemetry"] = telemetry;
		_summary_unit_stats.append(unit_summary);
	}
	_summary_cache["unit_stats"] = _summary_unit_stats;
	return _summary_cache;
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

Dictionary TeamfightSimulationCore::_build_stats_summary() {
	const bool omit_unit_telemetry = stats_export_minimal_telemetry_enabled();
	Dictionary summary;
	summary["seed"] = _seed;
	summary["winner_team"] = String(_winner_team);
	summary["duration"] = _time;
	summary["sudden_death_ticks"] = int64_t(_sudden_death_ticks);
	summary["player_comp"] = _player_comp;
	summary["enemy_comp"] = _enemy_comp;
	
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
	
	Array unit_stats;
	for (const UnitState &unit : _units) {
		// Skip minions in unit_stats output - their damage is aggregated to summoners
		if (unit.summoner_instance_id != 0) {
			continue;
		}
		
		const UnitStateCold &c = _uc(unit);
		Dictionary unit_summary;
		unit_summary["archetype_id"] = String(c.archetype_id);
		unit_summary["role"] = String(c.role_id);
		unit_summary["won"] = _winner_team != StringName() && unit.team == _winner_team;
		unit_summary["damage_dealt"] = c.damage_dealt;
		unit_summary["damage_dealt_auto"] = c.damage_dealt_auto;
		unit_summary["damage_dealt_ability"] = c.damage_dealt_ability;
		unit_summary["damage_dealt_ultimate"] = c.damage_dealt_ultimate;
		unit_summary["damage_dealt_passive"] = c.damage_dealt_passive;
		unit_summary["damage_received"] = c.damage_received;
		unit_summary["damage_mitigated"] = c.damage_mitigated;
		unit_summary["healing_done"] = c.healing_done;
		unit_summary["healing_done_auto"] = c.healing_done_auto;
		unit_summary["healing_done_ability"] = c.healing_done_ability;
		unit_summary["healing_done_ultimate"] = c.healing_done_ultimate;
		unit_summary["healing_done_passive"] = c.healing_done_passive;
		unit_summary["shielding_done"] = c.shielding_done;
		unit_summary["shielding_done_auto"] = c.shielding_done_auto;
		unit_summary["shielding_done_ability"] = c.shielding_done_ability;
		unit_summary["shielding_done_ultimate"] = c.shielding_done_ultimate;
		unit_summary["shielding_done_passive"] = c.shielding_done_passive;
		unit_summary["auto_attacks"] = c.auto_attacks;
		unit_summary["abilities"] = c.abilities;
		unit_summary["ultimates"] = c.ultimates;
		unit_summary["stuns"] = c.stuns;
		unit_summary["kills"] = c.kills;
		unit_summary["deaths"] = c.deaths;
		unit_summary["assists"] = c.assists;
		
		// Add aggregated minion stats if this unit summoned any minions
		auto it_dealt = summoner_minion_damage_dealt.find(unit.instance_id);
		auto it_received = summoner_minion_damage_received.find(unit.instance_id);
		if (it_dealt != summoner_minion_damage_dealt.end() || it_received != summoner_minion_damage_received.end()) {
			unit_summary["minion_damage_dealt"] = it_dealt != summoner_minion_damage_dealt.end() ? it_dealt->second : 0.0;
			unit_summary["minion_damage_received"] = it_received != summoner_minion_damage_received.end() ? it_received->second : 0.0;
		} else {
			unit_summary["minion_damage_dealt"] = 0.0;
			unit_summary["minion_damage_received"] = 0.0;
		}
		
		if (!omit_unit_telemetry) {
			Dictionary telemetry;
			telemetry["schema"] = String("teamfight.telemetry.v1");
			telemetry["hard_cc_seconds"] = unit.hard_cc_seconds;
			unit_summary["telemetry"] = telemetry;
		}
		unit_stats.append(unit_summary);
	}
	summary["unit_stats"] = unit_stats;
	return summary;
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
	Array champion_keys = _effective_champion_by_archetype.keys();
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

	Array champion_keys = _effective_champion_by_archetype.keys();
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
			double distance = _distance_between_coords(u.pos_x, u.pos_y, target->pos_x, target->pos_y);
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
	// before we replace the patch list, so _champion_catalog stays valid.
	_ensure_catalog_loaded();
	_balance_patches.clear();
	for (int64_t i = 0; i < patches.size(); ++i) {
		Dictionary pd = Dictionary(patches[i]);
		BalancePatch patch;
		_parse_balance_patch_from_dict(pd, patch);
		_balance_patches.push_back(patch);
	}
	_rebuild_effective_champion_cache();
}

Array TeamfightSimulationCore::get_balance_patches() const {
	Array result;
	for (const BalancePatch &patch : _balance_patches) {
		Dictionary pd;
		Array targets;
		for (const StringName &t : patch.targets) {
			targets.append(String(t));
		}
		pd["targets"] = targets;
		Array roles;
		for (const StringName &r : patch.roles) {
			roles.append(String(r));
		}
		pd["roles"] = roles;
		pd["stat_multipliers"] = patch.stat_multipliers;
		pd["stat_additions"] = patch.stat_additions;
		if (!patch.kit_id.is_empty()) {
			pd["kit_id"] = String(patch.kit_id);
		}
		if (patch.ability_override.get_type() != Variant::NIL) {
			pd["ability"] = patch.ability_override;
		}
		if (patch.ultimate_override.get_type() != Variant::NIL) {
			pd["ultimate"] = patch.ultimate_override;
		}
		if (patch.passive_ids_override.get_type() != Variant::NIL) {
			pd["passive_ids"] = patch.passive_ids_override;
		}
		result.append(pd);
	}
	return result;
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


