#include "teamfight_simulation_core.hpp"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
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
#include <mutex>
#include <utility>

static std::atomic<int64_t> s_benchmark_progress_done{0};
static std::atomic<bool> s_sim_profile_force_enabled{false};

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
inline const StringName &sn_commit() {
	static const StringName s("commit");
	return s;
}
inline const StringName &sn_peel() {
	static const StringName s("peel");
	return s;
}
inline const StringName &sn_burst() {
	static const StringName s("burst");
	return s;
}
inline const StringName &sn_kite() {
	static const StringName s("kite");
	return s;
}
inline const StringName &sn_objective() {
	static const StringName s("objective");
	return s;
}
inline const StringName &sn_auto() {
	static const StringName s("auto");
	return s;
}
inline const StringName &sn_multi() {
	static const StringName s("multi");
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
inline const StringName &sn_self_aoe_taunt() {
	static const StringName s("self_aoe_taunt");
	return s;
}
inline const StringName &sn_self_aoe_damage() {
	static const StringName s("self_aoe_damage");
	return s;
}
inline const StringName &sn_target_aoe_damage() {
	static const StringName s("target_aoe_damage");
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
inline const StringName &sn_self_aoe_slow() {
	static const StringName s("self_aoe_slow");
	return s;
}
inline const StringName &sn_self_aoe_root() {
	static const StringName s("self_aoe_root");
	return s;
}
inline const StringName &sn_target_aoe_slow() {
	static const StringName s("target_aoe_slow");
	return s;
}
inline const StringName &sn_target_aoe_root() {
	static const StringName s("target_aoe_root");
	return s;
}
inline const StringName &sn_target_aoe_silence() {
	static const StringName s("target_aoe_silence");
	return s;
}
inline const StringName &sn_target_aoe_disarm() {
	static const StringName s("target_aoe_disarm");
	return s;
}
inline const StringName &sn_self_aoe_silence() {
	static const StringName s("self_aoe_silence");
	return s;
}
inline const StringName &sn_self_aoe_disarm() {
	static const StringName s("self_aoe_disarm");
	return s;
}
inline const StringName &sn_self_aoe_knockback() {
	static const StringName s("self_aoe_knockback");
	return s;
}
inline const StringName &sn_self_aoe_reflect() {
	static const StringName s("self_aoe_reflect");
	return s;
}
inline const StringName &sn_reflect_damage() {
	static const StringName s("reflect_damage");
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
inline const StringName &sn_ability() {
	static const StringName s("ability");
	return s;
}
inline const StringName &sn_ultimate() {
	static const StringName s("ultimate");
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
constexpr size_t EFFECT_BUCKET_ON_TICK = 2;
constexpr size_t EFFECT_BUCKET_POST_ATTACK = 3;
constexpr size_t EFFECT_BUCKET_POST_TAKE_DAMAGE = 4;
constexpr size_t EFFECT_BUCKET_ON_ABILITY = 5;
constexpr size_t EFFECT_BUCKET_ON_ULTIMATE = 6;
} // namespace

namespace {
using TargetBucketTag = TeamfightSimulationCore::TargetBucketTag;

TargetBucketTag tag_for_bucket_order_string(const StringName &nm) {
	if (nm == sn_commit()) {
		return TargetBucketTag::Commit;
	}
	if (nm == sn_peel()) {
		return TargetBucketTag::Peel;
	}
	if (nm == sn_burst()) {
		return TargetBucketTag::Burst;
	}
	if (nm == sn_kite()) {
		return TargetBucketTag::Kite;
	}
	if (nm == sn_objective()) {
		return TargetBucketTag::Objective;
	}
	return TargetBucketTag::TagCount;
}
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
	if (kind == StringName("multi")) {
		return EFFECT_OPCODE_MULTI;
	}
	if (kind == StringName("damage")) {
		return EFFECT_OPCODE_DAMAGE;
	}
	if (kind == StringName("projectile")) {
		return EFFECT_OPCODE_PROJECTILE;
	}
	if (kind == StringName("stun")) {
		return EFFECT_OPCODE_STUN;
	}
	if (kind == StringName("shield")) {
		return EFFECT_OPCODE_SHIELD;
	}
	if (kind == StringName("heal")) {
		return EFFECT_OPCODE_HEAL;
	}
	if (kind == StringName("self_aoe_taunt")) {
		return EFFECT_OPCODE_SELF_AOE_TAUNT;
	}
	if (kind == StringName("self_aoe_damage")) {
		return EFFECT_OPCODE_SELF_AOE_DAMAGE;
	}
	if (kind == StringName("target_aoe_damage")) {
		return EFFECT_OPCODE_TARGET_AOE_DAMAGE;
	}
	if (kind == StringName("damage_threshold_trigger")) {
		return EFFECT_OPCODE_DAMAGE_THRESHOLD_TRIGGER;
	}
	if (kind == StringName("damage_over_time")) {
		return EFFECT_OPCODE_DAMAGE_OVER_TIME;
	}
	if (kind == StringName("heal_over_time")) {
		return EFFECT_OPCODE_HEAL_OVER_TIME;
	}
	if (kind == StringName("aoe_damage_over_time")) {
		return EFFECT_OPCODE_AOE_DAMAGE_OVER_TIME;
	}
	if (kind == StringName("aoe_heal_over_time")) {
		return EFFECT_OPCODE_AOE_HEAL_OVER_TIME;
	}
	if (kind == StringName("mana_regen")) {
		return EFFECT_OPCODE_MANA_REGEN;
	}
	if (kind == StringName("post_damage_mana_gain")) {
		return EFFECT_OPCODE_POST_DAMAGE_MANA_GAIN;
	}
	if (kind == StringName("damage_based_heal")) {
		return EFFECT_OPCODE_DAMAGE_BASED_HEAL;
	}
	if (kind == StringName("mana_restore_on_hit")) {
		return EFFECT_OPCODE_MANA_RESTORE_ON_HIT;
	}
	if (kind == StringName("drain_target_mana_on_hit")) {
		return EFFECT_OPCODE_DRAIN_TARGET_MANA_ON_HIT;
	}
	if (kind == StringName("every_n_attacks_stun")) {
		return EFFECT_OPCODE_EVERY_N_ATTACKS_STUN;
	}
	if (kind == StringName("self_dash")) {
		return EFFECT_OPCODE_SELF_DASH;
	}
	if (kind == StringName("auto_dodge")) {
		return EFFECT_OPCODE_AUTO_DODGE;
	}
	if (kind == StringName("constant_multiplier")) {
		return EFFECT_OPCODE_CONSTANT_MULTIPLIER;
	}
	if (kind == StringName("hp_threshold_damage_multiplier")) {
		return EFFECT_OPCODE_HP_THRESHOLD_DAMAGE_MULTIPLIER;
	}
	if (kind == StringName("distance_threshold_multiplier")) {
		return EFFECT_OPCODE_DISTANCE_THRESHOLD_MULTIPLIER;
	}
	// New effect kinds
	if (kind == StringName("slow")) {
		return EFFECT_OPCODE_SLOW;
	}
	if (kind == StringName("root")) {
		return EFFECT_OPCODE_ROOT;
	}
	if (kind == StringName("silence")) {
		return EFFECT_OPCODE_SILENCE;
	}
	if (kind == StringName("disarm")) {
		return EFFECT_OPCODE_DISARM;
	}
	if (kind == StringName("stealth")) {
		return EFFECT_OPCODE_STEALTH;
	}
	if (kind == StringName("knockback")) {
		return EFFECT_OPCODE_KNOCKBACK;
	}
	if (kind == StringName("reflect")) {
		return EFFECT_OPCODE_REFLECT;
	}
	if (kind == StringName("self_aoe_slow")) {
		return EFFECT_OPCODE_SELF_AOE_SLOW;
	}
	if (kind == StringName("self_aoe_root")) {
		return EFFECT_OPCODE_SELF_AOE_ROOT;
	}
	if (kind == StringName("self_aoe_silence")) {
		return EFFECT_OPCODE_SELF_AOE_SILENCE;
	}
	if (kind == StringName("self_aoe_disarm")) {
		return EFFECT_OPCODE_SELF_AOE_DISARM;
	}
	if (kind == StringName("target_aoe_slow")) {
		return EFFECT_OPCODE_TARGET_AOE_SLOW;
	}
	if (kind == StringName("target_aoe_root")) {
		return EFFECT_OPCODE_TARGET_AOE_ROOT;
	}
	if (kind == StringName("target_aoe_silence")) {
		return EFFECT_OPCODE_TARGET_AOE_SILENCE;
	}
	if (kind == StringName("target_aoe_disarm")) {
		return EFFECT_OPCODE_TARGET_AOE_DISARM;
	}
	if (kind == StringName("self_aoe_knockback")) {
		return EFFECT_OPCODE_SELF_AOE_KNOCKBACK;
	}
	if (kind == StringName("self_aoe_reflect")) {
		return EFFECT_OPCODE_SELF_AOE_REFLECT;
	}
	if (kind == StringName("reflect_damage")) {
		return EFFECT_OPCODE_REFLECT_DAMAGE;
	}
	if (kind == StringName("knockback_shield")) {
		return EFFECT_OPCODE_KNOCKBACK_SHIELD;
	}
	if (kind == StringName("target_status_multiplier")) {
		return EFFECT_OPCODE_TARGET_STATUS_MULTIPLIER;
	}
	if (kind == StringName("stat_modifier")) {
		return EFFECT_OPCODE_STAT_MODIFIER;
	}
	return EFFECT_OPCODE_UNKNOWN;
}

const StringName &TeamfightSimulationCore::_kind_for_opcode(int64_t opcode) {
	static const StringName empty_kind;
	switch (opcode) {
		case EFFECT_OPCODE_MULTI:
			return sn_multi();
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
		case EFFECT_OPCODE_SELF_AOE_TAUNT:
			return sn_self_aoe_taunt();
		case EFFECT_OPCODE_SELF_AOE_DAMAGE:
			return sn_self_aoe_damage();
		case EFFECT_OPCODE_TARGET_AOE_DAMAGE:
			return sn_target_aoe_damage();
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
		case EFFECT_OPCODE_SELF_AOE_SLOW:
			return sn_self_aoe_slow();
		case EFFECT_OPCODE_SELF_AOE_ROOT:
			return sn_self_aoe_root();
		case EFFECT_OPCODE_SELF_AOE_SILENCE:
			return sn_self_aoe_silence();
		case EFFECT_OPCODE_SELF_AOE_DISARM:
			return sn_self_aoe_disarm();
		case EFFECT_OPCODE_TARGET_AOE_SLOW:
			return sn_target_aoe_slow();
		case EFFECT_OPCODE_TARGET_AOE_ROOT:
			return sn_target_aoe_root();
		case EFFECT_OPCODE_TARGET_AOE_SILENCE:
			return sn_target_aoe_silence();
		case EFFECT_OPCODE_TARGET_AOE_DISARM:
			return sn_target_aoe_disarm();
		case EFFECT_OPCODE_SELF_AOE_KNOCKBACK:
			return sn_self_aoe_knockback();
		case EFFECT_OPCODE_SELF_AOE_REFLECT:
			return sn_self_aoe_reflect();
		case EFFECT_OPCODE_REFLECT_DAMAGE:
			return sn_reflect_damage();
		case EFFECT_OPCODE_KNOCKBACK_SHIELD:
			return sn_knockback_shield();
		case EFFECT_OPCODE_TARGET_STATUS_MULTIPLIER:
			return sn_target_status_multiplier();
		case EFFECT_OPCODE_STAT_MODIFIER:
			return sn_stat_modifier();
		default:
			return empty_kind;
	}
}

TeamfightSimulationCore::EffectRecord TeamfightSimulationCore::_compile_effect(const Dictionary &effect) const {
	EffectRecord compiled;
	StringName kind = StringName(String(effect.get("kind", "")));
	compiled.opcode = _opcode_for_kind(kind);
	Dictionary params = Dictionary(effect.get("params", Dictionary()));
	if (kind == StringName("multi")) {
		Variant effects_value = params.get("effects", Variant());
		Array effects = effects_value.get_type() == Variant::ARRAY ? Array(effects_value) : Array();
		compiled.children = _compile_effect_array(effects);
		return compiled;
	}
	if (kind == StringName("constant_multiplier")) {
		compiled.scalar0 = double(params.get("multiplier", 1.0));
	} else if (kind == StringName("hp_threshold_damage_multiplier")) {
		compiled.scalar0 = double(params.get("above_hp_ratio", 0.0));
		compiled.scalar1 = double(params.get("below_hp_ratio", 0.0));
		compiled.scalar2 = double(params.get("multiplier", 1.0));
	} else if (kind == StringName("distance_threshold_multiplier")) {
		compiled.scalar0 = double(params.get("min_distance", 0.0));
		compiled.scalar1 = double(params.get("multiplier", 1.0));
	} else if (kind == StringName("damage")) {
		compiled.scalar0 = double(params.get("max_hp_ratio", 0.0));
		compiled.scalar1 = double(params.get("damage_ratio", 0.0));
		compiled.scalar2 = double(params.get("flat_amount", 0.0));
		compiled.scalar3 = bool(params.get("trigger_on_hit", false)) ? 1.0 : 0.0;
		compiled.int0 = params.get("target_self", false) ? 1 : 0;
		compiled.damage_type = StringName(String(params.get("damage_type", "physical")));
		compiled.reason = String(params.get("reason", ""));
	} else if (kind == StringName("projectile")) {
		// Use -1.0 as sentinel for "use unit's projectile_speed/radius stat" when override is null.
		// Python parity: speed_override=None → unit.stats.projectile_speed, radius_override=None → unit.stats.projectile_radius.
		Variant speed_v = params.get("speed_override", Variant());
		compiled.scalar0 = (speed_v.get_type() == Variant::NIL) ? -1.0 : double(speed_v);
		Variant radius_v = params.get("radius_override", Variant());
		compiled.scalar1 = (radius_v.get_type() == Variant::NIL) ? -1.0 : double(radius_v);
		compiled.scalar2 = double(params.get("damage_multiplier", 1.0));
		compiled.scalar3 = double(params.get("stun_duration", 0.0));
		compiled.damage_type = StringName(String(params.get("damage_type", "physical")));
		compiled.reason = String(params.get("reason", ""));
	} else if (kind == StringName("stun")) {
		compiled.scalar0 = double(params.get("duration", 0.0));
		compiled.reason = String(params.get("reason", ""));
	} else if (kind == StringName("shield")) {
		compiled.scalar0 = double(params.get("max_hp_ratio", 0.0));
		compiled.scalar1 = double(params.get("damage_ratio", 0.0));
		compiled.scalar2 = double(params.get("flat_amount", 0.0));
		compiled.int0 = params.get("target_self", false) ? 1 : 0;
		compiled.reason = String(params.get("reason", ""));
	} else if (kind == StringName("heal")) {
		compiled.scalar0 = double(params.get("max_hp_ratio", 0.0));
		compiled.scalar1 = double(params.get("current_hp_ratio", 0.0));
		compiled.scalar2 = double(params.get("flat_amount", 0.0));
		compiled.scalar3 = double(params.get("missing_hp_ratio", 0.0));
		compiled.int0 = params.get("target_self", false) ? 1 : 0;  // target_self parameter
		compiled.reason = String(params.get("reason", ""));
	} else if (kind == StringName("self_aoe_taunt")) {
		compiled.scalar0 = double(params.get("radius", 0.0));
		compiled.scalar1 = double(params.get("duration", 0.0));
		compiled.reason = String(params.get("reason", ""));
	} else if (kind == StringName("self_aoe_damage")) {
		compiled.scalar0 = double(params.get("radius", 0.0));
		compiled.scalar1 = double(params.get("damage_multiplier", 1.0));
		compiled.scalar2 = double(params.get("flat_amount", 0.0));
		compiled.damage_type = StringName(String(params.get("damage_type", "physical")));
		compiled.reason = String(params.get("reason", ""));
	} else if (kind == StringName("target_aoe_damage")) {
		compiled.scalar0 = double(params.get("radius", 0.0));
		compiled.scalar1 = double(params.get("ratio", 0.0));
		compiled.damage_type = StringName(params.get("damage_type", "physical"));
		compiled.reason = String(params.get("reason", "TargetAoeDamage"));
	} else if (kind == StringName("damage_over_time")) {
		// Ratio-based parameters
		compiled.scalar0 = double(params.get("attack_damage_ratio", 0.0));
		compiled.scalar1 = double(params.get("max_hp_ratio", 0.0));
		compiled.scalar2 = double(params.get("damage_tick_interval", 1.0));
		compiled.scalar3 = double(params.get("flat_amount", 0.0));
		// Backward compatibility: damage_per_tick maps to flat_amount
		if (params.has("damage_per_tick") && !params.has("flat_amount")) {
			compiled.scalar3 = double(params.get("damage_per_tick", 0.0));
		}
		compiled.damage_type = StringName(params.get("damage_type", "physical"));
		compiled.stacking_mode = StringName(params.get("stacking_mode", "refresh"));
		compiled.effect_type = StringName(params.get("effect_type", "generic"));
		compiled.int0 = params.get("target_self", false) ? 1 : 0;  // target_self parameter
		compiled.int1 = int64_t(params.get("max_stacks", 1));
		compiled.int2 = int64_t(params.get("duration", 0.0));
	} else if (kind == StringName("heal_over_time")) {
		// Ratio-based parameters
		compiled.scalar0 = double(params.get("max_hp_ratio", 0.0));
		compiled.scalar1 = double(params.get("current_hp_ratio", 0.0));
		compiled.scalar2 = double(params.get("heal_tick_interval", 1.0));
		compiled.scalar3 = double(params.get("missing_hp_ratio", 0.0));
		compiled.scalar4 = double(params.get("flat_amount", 0.0));
		// Backward compatibility: heal_per_tick maps to flat_amount
		if (params.has("heal_per_tick") && !params.has("flat_amount")) {
			compiled.scalar4 = double(params.get("heal_per_tick", 0.0));
		}
		compiled.stacking_mode = StringName(params.get("stacking_mode", "refresh"));
		compiled.effect_type = StringName(params.get("effect_type", "generic"));
		compiled.int0 = params.get("target_self", false) ? 1 : 0;  // target_self parameter
		compiled.int1 = int64_t(params.get("max_stacks", 1));
		compiled.int2 = int64_t(params.get("duration", 0.0));
		compiled.int3 = bool(params.get("allow_overheal", false)) ? 1 : 0;
	} else if (kind == StringName("aoe_damage_over_time")) {
		// AoE parameters
		compiled.scalar0 = double(params.get("radius", 0.0));
		// DoT ratio parameters
		compiled.scalar1 = double(params.get("attack_damage_ratio", 0.0));
		compiled.scalar2 = double(params.get("max_hp_ratio", 0.0));
		compiled.scalar3 = double(params.get("flat_amount", 0.0));
		compiled.scalar4 = double(params.get("damage_tick_interval", 1.0));
		compiled.damage_type = StringName(params.get("damage_type", "physical"));
		compiled.stacking_mode = StringName(params.get("stacking_mode", "refresh"));
		compiled.effect_type = StringName(params.get("effect_type", "generic"));
		compiled.int0 = int64_t(params.get("max_stacks", 1));
		compiled.int1 = int64_t(params.get("duration", 0.0));
		compiled.int2 = params.get("target_self", false) ? 1 : 0;
	} else if (kind == StringName("aoe_heal_over_time")) {
		// AoE parameters
		compiled.scalar0 = double(params.get("radius", 0.0));
		// HoT ratio parameters
		compiled.scalar1 = double(params.get("max_hp_ratio", 0.0));
		compiled.scalar2 = double(params.get("current_hp_ratio", 0.0));
		compiled.scalar3 = double(params.get("missing_hp_ratio", 0.0));
		compiled.scalar4 = double(params.get("flat_amount", 0.0));
		compiled.scalar5 = double(params.get("heal_tick_interval", 1.0));
		compiled.stacking_mode = StringName(params.get("stacking_mode", "refresh"));
		compiled.effect_type = StringName(params.get("effect_type", "generic"));
		compiled.int0 = int64_t(params.get("max_stacks", 1));
		compiled.int1 = int64_t(params.get("duration", 0.0));
		compiled.int2 = bool(params.get("allow_overheal", false)) ? 1 : 0;
		compiled.int3 = params.get("target_self", false) ? 1 : 0;
	} else if (kind == StringName("damage_threshold_trigger")) {
		compiled.scalar0 = double(params.get("threshold_multiplier", 1.0));
		Variant nested = params.get("effect", Variant());
		if (nested.get_type() == Variant::DICTIONARY) {
			compiled.children.push_back(_compile_effect(Dictionary(nested)));
		}
	} else if (kind == StringName("mana_regen")) {
		compiled.scalar0 = double(params.get("flat_amount", 0.0));
		compiled.scalar1 = double(params.get("max_mana_ratio", 0.0));
	} else if (kind == StringName("post_damage_mana_gain")) {
		compiled.scalar0 = double(params.get("damage_ratio", 0.0));
	} else if (kind == StringName("damage_based_heal")) {
		compiled.scalar0 = double(params.get("heal_ratio", 0.0));
	} else if (kind == StringName("mana_restore_on_hit")) {
		compiled.scalar0 = double(params.get("flat_amount", 0.0));
	} else if (kind == StringName("drain_target_mana_on_hit")) {
		compiled.scalar0 = double(params.get("flat_amount", 0.0));
	} else if (kind == StringName("every_n_attacks_stun")) {
		compiled.int0 = int64_t(params.get("every_n", 0));
		compiled.scalar0 = double(params.get("stun_duration", 0.0));
		compiled.reason = String(params.get("reason", ""));
	} else if (kind == StringName("self_dash")) {
		compiled.scalar0 = double(params.get("distance", 1.0));
		Dictionary direction = Dictionary(params.get("direction", Dictionary()));
		compiled.scalar1 = double(direction.get("x", 0.0));
		compiled.scalar2 = double(direction.get("y", 0.0));
	} else if (kind == StringName("auto_dodge")) {
		compiled.scalar0 = double(params.get("dodge_chance", 0.0));
		compiled.scalar1 = double(params.get("on_dodge_multiplier", 0.0));
		compiled.scalar2 = double(params.get("on_hit_multiplier", 1.0));
	}
	// New effect compilation
	else if (kind == StringName("slow")) {
		compiled.scalar0 = double(params.get("slow_percentage", 0.0));
		compiled.scalar1 = double(params.get("duration", 0.0));
		compiled.reason = String(params.get("reason", "Slow"));
	} else if (kind == StringName("root")) {
		compiled.scalar0 = double(params.get("duration", 0.0));
		compiled.reason = String(params.get("reason", "Root"));
	} else if (kind == StringName("silence")) {
		compiled.scalar0 = double(params.get("duration", 0.0));
		compiled.int0 = params.get("block_abilities", true) ? 1 : 0;
		compiled.int1 = params.get("block_ultimate", true) ? 1 : 0;
		compiled.reason = String(params.get("reason", "Silence"));
	} else if (kind == StringName("disarm")) {
		compiled.scalar0 = double(params.get("duration", 0.0));
		compiled.reason = String(params.get("reason", "Disarm"));
	} else if (kind == StringName("stealth")) {
		Dictionary break_conditions = Dictionary(params.get("break_conditions", Dictionary()));
		compiled.scalar0 = double(params.get("duration", 0.0));
		compiled.int0 = bool(break_conditions.get("on_attack", false)) ? 1 : 0;
		compiled.int1 = bool(break_conditions.get("on_ability", false)) ? 1 : 0;
		compiled.int2 = bool(break_conditions.get("on_damage_taken", false)) ? 1 : 0;
		compiled.int3 = params.get("target_self", false) ? 1 : 0;
		compiled.reason = String(params.get("reason", "Stealth"));
	} else if (kind == StringName("knockback")) {
		compiled.scalar0 = double(params.get("distance", 0.0));
		compiled.scalar1 = double(params.get("duration", 0.0));
		compiled.int0 = params.get("direction", "away_from_source") == "away_from_source" ? 1 : 0;
		compiled.reason = String(params.get("reason", "Knockback"));
	} else if (kind == StringName("reflect")) {
		compiled.scalar0 = double(params.get("reflect_percentage", 0.0));
		compiled.scalar1 = double(params.get("duration", 0.0));
		compiled.int0 = params.get("reflect_type", "all") == "all" ? 1 : 0;
		compiled.reason = String(params.get("reason", "Reflect"));
	} else if (kind == StringName("self_aoe_slow")) {
		compiled.scalar0 = double(params.get("radius", 0.0));
		compiled.scalar1 = double(params.get("slow_percentage", 0.0));
		compiled.scalar2 = double(params.get("duration", 0.0));
		compiled.reason = String(params.get("reason", "AOE Slow"));
	} else if (kind == StringName("self_aoe_root")) {
		compiled.scalar0 = double(params.get("radius", 0.0));
		compiled.scalar1 = double(params.get("duration", 0.0));
		compiled.reason = String(params.get("reason", "AOE Root"));
	} else if (kind == StringName("self_aoe_silence")) {
		compiled.scalar0 = double(params.get("radius", 0.0));
		compiled.scalar1 = double(params.get("duration", 0.0));
		compiled.int0 = params.get("block_abilities", true) ? 1 : 0;
		compiled.int1 = params.get("block_ultimate", true) ? 1 : 0;
		compiled.reason = String(params.get("reason", "AOE Silence"));
	} else if (kind == StringName("self_aoe_disarm")) {
		compiled.scalar0 = double(params.get("radius", 0.0));
		compiled.scalar1 = double(params.get("duration", 0.0));
		compiled.reason = String(params.get("reason", "AOE Disarm"));
	} else if (kind == StringName("target_aoe_slow")) {
		compiled.scalar0 = double(params.get("radius", 0.0));
		compiled.scalar1 = double(params.get("slow_percentage", 0.0));
		compiled.scalar2 = double(params.get("duration", 0.0));
		compiled.reason = String(params.get("reason", "TargetAoeSlow"));
	} else if (kind == StringName("target_aoe_root")) {
		compiled.scalar0 = double(params.get("radius", 0.0));
		compiled.scalar1 = double(params.get("duration", 0.0));
		compiled.reason = String(params.get("reason", "TargetAoeRoot"));
	} else if (kind == StringName("target_aoe_silence")) {
		compiled.scalar0 = double(params.get("radius", 0.0));
		compiled.scalar1 = double(params.get("duration", 0.0));
		compiled.int0 = params.get("blocks_abilities", false) ? 1 : 0;
		compiled.int1 = params.get("blocks_ultimates", false) ? 1 : 0;
		compiled.reason = String(params.get("reason", "TargetAoeSilence"));
	} else if (kind == StringName("target_aoe_disarm")) {
		compiled.scalar0 = double(params.get("radius", 0.0));
		compiled.scalar1 = double(params.get("duration", 0.0));
		compiled.reason = String(params.get("reason", "TargetAoeDisarm"));
	} else if (kind == StringName("self_aoe_knockback")) {
		compiled.scalar0 = double(params.get("radius", 0.0));
		compiled.scalar1 = double(params.get("distance", 0.0));
		compiled.scalar2 = double(params.get("duration", 0.0));
		compiled.int0 = params.get("direction", "away_from_source") == "away_from_source" ? 1 : 0;
		compiled.reason = String(params.get("reason", "AOE Knockback"));
	} else if (kind == StringName("self_aoe_reflect")) {
		compiled.scalar0 = double(params.get("radius", 0.0));
		compiled.scalar1 = double(params.get("reflect_percentage", 0.0));
		compiled.scalar2 = double(params.get("duration", 0.0));
		compiled.int0 = params.get("reflect_type", "all") == "all" ? 1 : 0;
		compiled.reason = String(params.get("reason", "AOE Reflect"));
	} else if (kind == StringName("reflect_damage")) {
		compiled.scalar0 = double(params.get("reflect_percentage", 0.0));
		compiled.int0 = params.get("reflect_type", "all") == "all" ? 1 : 0;
		compiled.reason = String(params.get("reason", "Reflect damage"));
	} else if (kind == StringName("knockback_shield")) {
		compiled.scalar0 = double(params.get("shield_ratio", 0.0));
		compiled.reason = String(params.get("reason", "Knockback shield"));
	} else if (kind == StringName("target_status_multiplier")) {
		compiled.scalar0 = double(params.get("multiplier", 1.0));
		compiled.damage_type = StringName(String(params.get("status_kind", "")));
		compiled.reason = String(params.get("reason", "Target status multiplier"));
	} else if (kind == StringName("stat_modifier")) {
		// Validate stat name
		StringName stat_name = StringName(String(params.get("stat_name", "")));
		if (!_is_valid_stat_name(stat_name)) {
			compiled.opcode = EFFECT_OPCODE_UNKNOWN;
			return compiled;
		}
		
		compiled.damage_type = stat_name;
		compiled.scalar0 = double(params.get("additive", 0.0));
		compiled.scalar1 = double(params.get("multiplicative", 1.0));
		compiled.scalar2 = double(params.get("duration", 0.0));
		compiled.int0 = params.get("target_self", false) ? 1 : 0;
		compiled.int1 = params.get("duration_type", "respawn") == "match" ? 1 : 0;
		compiled.int2 = int64_t(params.get("max_stacks", 1));
		String stack_behavior = String(params.get("stack_behavior", "refresh"));
		if (stack_behavior == "accumulate") {
			compiled.int3 = 1;
		} else if (stack_behavior == "reset") {
			compiled.int3 = 2;
		} else {
			compiled.int3 = 0;
		}
		compiled.reason = String(params.get("reason", "Stat modifier"));
	}
	
	// Handle conditional execution parameters
	compiled.requires_result_from = StringName(String(params.get("requires_result_from", "")));
	compiled.requires_field = StringName(String(params.get("requires_field", "")));
	compiled.requires_value = params.get("requires_value", Variant());
	
	// Handle on_tick_interval parameter for timing control
	compiled.on_tick_interval = double(params.get("on_tick_interval", 1.0));
	// Validate minimum on_tick_interval (must be at least game tick rate)
	compiled.on_tick_interval = Math::max(compiled.on_tick_interval, DEFAULT_TICK_RATE);
	
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
	unit.reflect_passive_pct_all = 0.0;
	unit.reflect_passive_pct_physical = 0.0;
	for (const EffectRecord &eff : cold.passive_effects[1]) {
		if (eff.opcode == EFFECT_OPCODE_REFLECT_DAMAGE) {
			double p = Math::clamp(eff.scalar0, 0.0, 1.0);
			if (eff.int0 == 1) {
				unit.reflect_passive_pct_all += p;
			} else {
				unit.reflect_passive_pct_physical += p;
			}
		}
	}
	unit.reflect_passive_pct_all = Math::clamp(unit.reflect_passive_pct_all, 0.0, 1.0);
	unit.reflect_passive_pct_physical = Math::clamp(unit.reflect_passive_pct_physical, 0.0, 1.0);
}

TeamfightSimulationCore::TeamfightSimulationCore() {
	_scratch_projectiles.reserve(32);
	for (auto &bucket : _spatial_buckets) {
		bucket.reserve(4);
	}
	for (auto &bucket : _spatial_buckets_aux) {
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
	ClassDB::bind_method(D_METHOD("run_match_simulation_only", "match_input"), &TeamfightSimulationCore::run_match_simulation_only);
	ClassDB::bind_method(D_METHOD("run_matches_simulation_only", "match_inputs"), &TeamfightSimulationCore::run_matches_simulation_only);
	ClassDB::bind_method(D_METHOD("run_generated_matches_simulation_only", "base_seed", "batch_count", "team_size"), &TeamfightSimulationCore::run_generated_matches_simulation_only);
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
	_scratch_projectiles.clear();
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
	
	// Reset spawn slot tracking
	_player_spawn_slots_used.clear();
	_enemy_spawn_slots_used.clear();
	
	_unit_index_map.clear();
	_alive_player_indices.clear();
	_alive_enemy_indices.clear();
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
	_tick_ctx.needs_cluster_density = false;
	_trace_buffer.clear();
	_debug_combat_trace = false;
	_viewer_fx_events.clear();
	_obscurance_aux_enemy_grid_tick = -1;
	_obscurance_aux_enemy_grid_sig = 0;
	_obscurance_aux_player_grid_tick = -1;
	_obscurance_aux_player_grid_sig = 0;
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
	if (_catalog_loaded) {
		return;
	}
	// Godot FileAccess / JSON from multiple threads during first load has faulted on Windows;
	// serialize so each core instance still gets its own catalog copy, but not concurrently.
	static std::mutex s_catalog_load_mutex;
	std::lock_guard<std::mutex> lock(s_catalog_load_mutex);
	if (_catalog_loaded) {
		return;
	}
	_champion_catalog = _load_json_file(String(CHAMPION_SCHEMA_PATH));
	_build_role_configs();
	_build_passive_registry();

	// Load balance patches (file is optional; silently skip if absent or empty).
	_balance_patches.clear();
	Dictionary bp_root = _load_json_file(String(BALANCE_PATCHES_PATH));
	if (!bp_root.is_empty()) {
		Array patches = Array(bp_root.get("patches", Array()));
		for (int64_t i = 0; i < patches.size(); ++i) {
			Dictionary pd = Dictionary(patches[i]);
			BalancePatch patch;
			_parse_balance_patch_from_dict(pd, patch);
			_balance_patches.push_back(patch);
		}
	}

	// Optional champion kits (preset ability/ultimate/passive swaps).
	_ability_kits = Dictionary();
	Dictionary kits_root = _load_json_file_if_exists(String(CHAMPION_KITS_PATH));
	if (!kits_root.is_empty()) {
		_ability_kits = Dictionary(kits_root.get("kits", Dictionary()));
	}

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
		
		// Skip non-champion entries like "passives" and "role_configs"
		if (key_str == "passives" || key_str == "role_configs") {
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
	if (!_champion_catalog.has(archetype_id)) {
		return Dictionary();
	}
	return Dictionary(_champion_catalog[archetype_id]);
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

void TeamfightSimulationCore::_sync_targeting_frame_unit(const UnitState &unit) {
	int64_t index = _unit_index_by_id(unit.instance_id);
	if (index < 0 || index >= int64_t(_targeting_frame.size())) {
		return;
	}
	_targeting_frame[static_cast<size_t>(index)] = _make_targeting_frame_entry(unit);
}

std::pair<TeamfightSimulationCore::UnitState, TeamfightSimulationCore::UnitStateCold> TeamfightSimulationCore::_build_unit_state(const Dictionary &spawn_spec, const StringName &team, int64_t instance_id) {
	UnitState unit;
	UnitStateCold cold;
	StringName archetype_id = StringName(String(spawn_spec.get("archetype_id", "")));
	Dictionary champion = _effective_champion_for(archetype_id);
	if (champion.is_empty()) {
		UtilityFunctions::push_error(vformat("Unknown champion archetype: %s", String(archetype_id)));
		return { unit, cold };
	}

	// Stats, kits, and balance overlays are resolved in _rebuild_effective_champion_cache().
	Dictionary stats = Dictionary(champion["stats"]).duplicate(true);
	StringName role_name = StringName(stats.get("role", StringName()));
	Dictionary role_config = Dictionary(_role_configs.get(role_name, Dictionary()));

	auto passive_bucket_index = [](const StringName &kind) -> int {
		if (kind == StringName("on_attack")) {
			return 0;
		}
		if (kind == StringName("on_defense")) {
			return 1;
		}
		if (kind == StringName("on_tick")) {
			return 2;
		}
		if (kind == StringName("post_attack")) {
			return 3;
		}
		if (kind == StringName("post_take_damage")) {
			return 4;
		}
		if (kind == StringName("on_ability")) {
			return 5;
		}
		if (kind == StringName("on_ultimate")) {
			return 6;
		}
		return 4;
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
		effect_kinds.append(StringName("on_tick"));
		effect_kinds.append(StringName("post_attack"));
		effect_kinds.append(StringName("post_take_damage"));
		effect_kinds.append(StringName("on_ability"));
		effect_kinds.append(StringName("on_ultimate"));
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
		cold.passive_effects[2].push_back(_compile_effect(Dictionary(role_tick)));
	}
	Variant role_take_damage = role_config.get("passive_post_take_damage", Variant());
	if (role_take_damage.get_type() != Variant::NIL) {
		cold.passive_effects[4].push_back(_compile_effect(Dictionary(role_take_damage)));
	}

	double max_hp = double(stats.get("max_hp", 0.0));
	double max_mana = double(stats.get("max_mana", 0.0));
	double x, y;

	// Generate random spawn position with slot assignment
	int64_t spawn_slot = _assign_spawn_slot(team);
	Vector2 spawn_pos = _get_random_spawn_position(team, false);

	// Override Y coordinate with assigned slot position
	static const std::vector<double> spawn_points = {3.0, 3.5, 4.0, 4.5, 5.0, 5.5, 6.0, 6.5, 7.0};
	if (spawn_slot >= 0 && spawn_slot < int(spawn_points.size())) {
		double x_base = (team == StringName("player")) ? PLAYER_SPAWN_X_BASE : ENEMY_SPAWN_X_BASE;
		
		// Adjust melee champions to spawn 0.5 tiles closer to center
		double attack_range = double(stats.get("attack_range", 0.0));
		if (attack_range <= 1.0) {  // Melee threshold
			if (team == StringName("player")) {
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

	unit.instance_id = instance_id;
	cold.archetype_id = archetype_id;
	unit.team = team;
	cold.role_id = role_name;
	unit.role_slot = role_slot_for_name(role_name);
	unit.is_tank_role = role_name == StringName("tank");
	unit.is_fighter_role = role_name == StringName("fighter");
	unit.is_assassin_role = role_name == StringName("assassin");
	unit.is_marksman_role = role_name == StringName("marksman");
	unit.is_mage_role = role_name == StringName("mage");
	unit.is_support_role = role_name == StringName("support");
	cold.stats = stats;
	unit.combat.max_hp = max_hp;
	unit.combat.max_mana = max_mana;
	unit.combat.ability_cd = double(stats.get("ability_cd", 0.0));
	unit.combat.attack_range = double(stats.get("attack_range", 0.0));
	unit.combat.cast_range = double(stats.get("cast_range", 0.0));
	unit.combat.move_speed = double(stats.get("move_speed", 0.0));
	unit.combat.attack_speed = double(stats.get("attack_speed", 1.0));
	unit.combat.attack_damage = double(stats.get("attack_damage", 0.0));
	unit.combat.projectile_speed = double(stats.get("projectile_speed", DEFAULT_PROJECTILE_SPEED));
	unit.combat.projectile_radius = double(stats.get("projectile_radius", DEFAULT_PROJECTILE_RADIUS));
	unit.combat.life_steal = double(stats.get("life_steal", 0.0));
	unit.combat.mana_per_attack = double(stats.get("mana_per_attack", 0.0));
	unit.combat.respawn_time = double(stats.get("respawn_time", RESPAWN_TIME));
	unit.combat.armor = double(stats.get("armor", 0.0));
	unit.combat.magic_resist = double(stats.get("magic_resist", 0.0));
	unit.combat.tenacity = double(stats.get("tenacity", 0.0));
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
	cold.spawn_pos_x = x;
	cold.spawn_pos_y = y;
	unit.pos_x = x;
	unit.pos_y = y;
	cold.respawn_slot_index = -1; // Initialize with no assigned respawn slot
	unit.hp = max_hp;
	unit.shield = 0.0;
	unit.mana = 0.0;
	unit.attack_cooldown = 0.0;
	unit.ability_cooldown = unit.combat.ability_cd;
	unit.casting_remaining = 0.0;
	cold.casting_kind = StringName();
	cold.casting_effect = EffectRecord();
	unit.casting_target_id = 0;
	unit.casting_ally_target_id = 0;
	unit.cast_resolved_this_tick = false;
	unit.target_id = 0;
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
	
	// Initialize stat modifier fields
	unit.stat_additive_max_hp = 0.0;
	unit.stat_multiplicative_max_hp = 1.0;
	unit.stat_temp_max_hp = 0.0;
	unit.stat_perm_max_hp = 0.0;
	unit.stat_additive_attack_damage = 0.0;
	unit.stat_multiplicative_attack_damage = 1.0;
	unit.stat_temp_attack_damage = 0.0;
	unit.stat_perm_attack_damage = 0.0;
	unit.stat_additive_attack_speed = 0.0;
	unit.stat_multiplicative_attack_speed = 1.0;
	unit.stat_temp_attack_speed = 0.0;
	unit.stat_perm_attack_speed = 0.0;
	unit.stat_additive_move_speed = 0.0;
	unit.stat_multiplicative_move_speed = 1.0;
	unit.stat_temp_move_speed = 0.0;
	unit.stat_perm_move_speed = 0.0;
	unit.stat_additive_armor = 0.0;
	unit.stat_multiplicative_armor = 1.0;
	unit.stat_temp_armor = 0.0;
	unit.stat_perm_armor = 0.0;
	unit.stat_additive_magic_resist = 0.0;
	unit.stat_multiplicative_magic_resist = 1.0;
	unit.stat_temp_magic_resist = 0.0;
	unit.stat_perm_magic_resist = 0.0;
	unit.stat_additive_tenacity = 0.0;
	unit.stat_multiplicative_tenacity = 1.0;
	unit.stat_temp_tenacity = 0.0;
	unit.stat_perm_tenacity = 0.0;
	unit.stat_additive_life_steal = 0.0;
	unit.stat_multiplicative_life_steal = 1.0;
	unit.stat_temp_life_steal = 0.0;
	unit.stat_perm_life_steal = 0.0;
	unit.stat_additive_max_mana = 0.0;
	unit.stat_multiplicative_max_mana = 1.0;
	unit.stat_temp_max_mana = 0.0;
	unit.stat_perm_max_mana = 0.0;
	unit.stat_additive_mana_per_attack = 0.0;
	unit.stat_multiplicative_mana_per_attack = 1.0;
	unit.stat_temp_mana_per_attack = 0.0;
	unit.stat_perm_mana_per_attack = 0.0;
	unit.stat_additive_ability_cd = 0.0;
	unit.stat_multiplicative_ability_cd = 1.0;
	unit.stat_temp_ability_cd = 0.0;
	unit.stat_perm_ability_cd = 0.0;
	unit.stat_additive_projectile_speed = 0.0;
	unit.stat_multiplicative_projectile_speed = 1.0;
	unit.stat_temp_projectile_speed = 0.0;
	unit.stat_perm_projectile_speed = 0.0;
	unit.stat_additive_projectile_radius = 0.0;
	unit.stat_multiplicative_projectile_radius = 1.0;
	unit.stat_temp_projectile_radius = 0.0;
	unit.stat_perm_projectile_radius = 0.0;
	unit.stat_additive_respawn_time = 0.0;
	unit.stat_multiplicative_respawn_time = 1.0;
	unit.stat_temp_respawn_time = 0.0;
	unit.stat_perm_respawn_time = 0.0;
	unit.stat_additive_attack_range = 0.0;
	unit.stat_multiplicative_attack_range = 1.0;
	unit.stat_temp_attack_range = 0.0;
	unit.stat_perm_attack_range = 0.0;
	unit.stat_additive_cast_range = 0.0;
	unit.stat_multiplicative_cast_range = 1.0;
	unit.stat_temp_cast_range = 0.0;
	unit.stat_perm_cast_range = 0.0;
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
	cold.effect_accumulators.clear();
	unit.regen_accumulator = 0.0;
	unit.reflect_buff_remaining = 0.0;
	unit.reflect_buff_pct_all = 0.0;
	unit.reflect_buff_pct_physical = 0.0;
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
	auto fill_buckets = [](UnitStrategy &s, std::initializer_list<const char *> names) {
		int i = 0;
		for (const char *n : names) {
			if (i >= 8) {
				break;
			}
			s.bucket_order[static_cast<size_t>(i++)] = StringName(n);
		}
		s.bucket_order_len = i;
		s.bucket_rank_by_tag.fill(i);
		for (int rank = 0; rank < i; ++rank) {
			TargetBucketTag tt = tag_for_bucket_order_string(s.bucket_order[static_cast<size_t>(rank)]);
			if (tt != TargetBucketTag::TagCount) {
				s.bucket_rank_by_tag[static_cast<size_t>(tt)] = std::min(s.bucket_rank_by_tag[static_cast<size_t>(tt)], rank);
			}
		}
	};
	auto set_role_prio = [](std::array<double, TeamfightSimulationCore::ROLE_SLOT_COUNT> &slots, const StringName &role, double value) {
		int64_t slot = role_slot_for_name(role);
		if (slot >= 0) {
			slots[static_cast<size_t>(slot)] = value;
		}
	};

	{
		UnitStrategy s;
		s.display_name = String("Protector");
		s.distance_weight = DISTANCE_WEIGHT_TANK;
		s.hp_weight = 0.0;
		s.ally_distance_weight = 1.0;
		s.ally_hp_weight = 0.0;
		s.ally_threat_weight = SCORE_THREAT_WEIGHT_SCALE * SCORE_THREAT_WEIGHT_SCALE;
		set_role_prio(s.ally_role_priorities, StringName("marksman"), -5.0 * ROLE_PRIORITY_GLOBAL_SCALE);
		set_role_prio(s.ally_role_priorities, StringName("mage"), -5.0 * ROLE_PRIORITY_GLOBAL_SCALE);
		set_role_prio(s.ally_role_priorities, StringName("support"), -3.0 * ROLE_PRIORITY_GLOBAL_SCALE);
		s.stickiness_bonus = STICKINESS_DEFAULT;
		s.in_range_bonus = IN_RANGE_BONUS_TANK;
		s.tank_penalty = TANK_PENALTY_TANK;
		s.threat_response_weight = THREAT_RESPONSE_TANK;
		s.execute_bonus_weight = 0.0;
		s.prefers_kiting = false;
		s.prey_instinct_weight = 0.0;
		s.cluster_weight = CLUSTER_WEIGHT_TANK;
		s.spacing_weight = 0.0;
		s.bodyguard_weight = BODYGUARD_WEIGHT_TANK;
		s.obscurance_weight = 0.0;
		s.carry_peel_weight = 0.0;
		s.flanking_weight = 0.0;
		s.threat_decay_rate = THREAT_DECAY_TANK;
		s.uses_ally_targeting = true;
		set_role_prio(s.role_priorities, StringName("assassin"), -5.0 * ROLE_PRIORITY_GLOBAL_SCALE);
		set_role_prio(s.role_priorities, StringName("fighter"), -2.0 * ROLE_PRIORITY_GLOBAL_SCALE);
		s.switch_margin = TARGET_SWITCH_MARGIN;
		s.bucket_margin = TARGET_BUCKET_MARGIN;
		s.projectile_time_weight = 0.0;
		fill_buckets(s, { "commit", "peel", "burst", "objective", "kite" });
		put(StringName("tank"), std::move(s));
	}
	{
		UnitStrategy s;
		s.display_name = String("Diver");
		s.distance_weight = DISTANCE_WEIGHT_ASSASSIN;
		s.hp_weight = HP_WEIGHT_ASSASSIN;
		s.stickiness_bonus = STICKINESS_DEFAULT;
		s.in_range_bonus = IN_RANGE_BONUS_ASSASSIN;
		s.tank_penalty = TANK_PENALTY_ASSASSIN;
		s.threat_response_weight = THREAT_RESPONSE_ASSASSIN;
		s.execute_bonus_weight = EXECUTE_BONUS_WEIGHT_DEFAULT;
		s.prey_instinct_weight = 2.0;
		s.prefers_kiting = false;
		s.cluster_weight = 0.0;
		s.spacing_weight = 0.0;
		s.bodyguard_weight = 0.0;
		s.obscurance_weight = 0.0;
		s.carry_peel_weight = 0.0;
		s.flanking_weight = FLANKING_WEIGHT_ASSASSIN;
		s.threat_decay_rate = THREAT_DECAY_FRAGILE;
		s.uses_ally_targeting = false;
		set_role_prio(s.role_priorities, StringName("marksman"), -15.0 * ROLE_PRIORITY_GLOBAL_SCALE);
		set_role_prio(s.role_priorities, StringName("mage"), -15.0 * ROLE_PRIORITY_GLOBAL_SCALE);
		set_role_prio(s.role_priorities, StringName("support"), -10.0 * ROLE_PRIORITY_GLOBAL_SCALE);
		set_role_prio(s.role_priorities, StringName("fighter"), 10.0 * ROLE_PRIORITY_GLOBAL_SCALE);
		s.switch_margin = TARGET_SWITCH_MARGIN;
		s.bucket_margin = TARGET_BUCKET_MARGIN;
		s.projectile_time_weight = 0.0;
		fill_buckets(s, { "commit", "burst", "objective", "peel", "kite" });
		put(StringName("assassin"), std::move(s));
	}
	{
		UnitStrategy s;
		s.display_name = String("Kiter");
		s.distance_weight = 1.0;
		s.hp_weight = 0.5;
		s.stickiness_bonus = STICKINESS_MARKSMAN;
		s.in_range_bonus = IN_RANGE_BONUS_MARKSMAN;
		s.tank_penalty = TANK_PENALTY_MARKSMAN;
		s.threat_response_weight = THREAT_RESPONSE_MARKSMAN;
		s.execute_bonus_weight = EXECUTE_BONUS_WEIGHT_DEFAULT;
		s.prey_instinct_weight = 0.0;
		s.prefers_kiting = true;
		s.cluster_weight = 0.0;
		s.spacing_weight = SPACING_WEIGHT_MARKSMAN;
		s.bodyguard_weight = 0.0;
		s.obscurance_weight = OBSCURANCE_WEIGHT_DEFAULT;
		s.carry_peel_weight = 60.0;
		s.flanking_weight = 0.0;
		s.threat_decay_rate = THREAT_DECAY_FRAGILE;
		s.uses_ally_targeting = false;
		s.switch_margin = TARGET_SWITCH_MARGIN;
		s.bucket_margin = TARGET_BUCKET_MARGIN;
		s.projectile_time_weight = PROJECTILE_TIME_WEIGHT_MARKSMAN;
		fill_buckets(s, { "commit", "peel", "burst", "objective", "kite" });
		put(StringName("marksman"), std::move(s));
	}
	{
		UnitStrategy s;
		s.display_name = String("Brawler");
		s.distance_weight = DISTANCE_WEIGHT_FIGHTER_CLOSE;
		s.hp_weight = 0.5;
		s.stickiness_bonus = STICKINESS_DEFAULT;
		s.in_range_bonus = IN_RANGE_BONUS_FIGHTER;
		s.tank_penalty = TANK_PENALTY_FIGHTER;
		s.threat_response_weight = THREAT_RESPONSE_FIGHTER;
		s.execute_bonus_weight = EXECUTE_BONUS_WEIGHT_DEFAULT;
		s.prey_instinct_weight = 0.0;
		s.prefers_kiting = false;
		s.cluster_weight = 0.0;
		s.spacing_weight = 0.0;
		s.bodyguard_weight = 0.0;
		s.obscurance_weight = 0.0;
		s.carry_peel_weight = 0.0;
		s.flanking_weight = 0.0;
		s.threat_decay_rate = THREAT_DECAY_FIGHTER;
		s.uses_ally_targeting = false;
		set_role_prio(s.role_priorities, StringName("marksman"), -1.0 * ROLE_PRIORITY_GLOBAL_SCALE);
		set_role_prio(s.role_priorities, StringName("mage"), -1.0 * ROLE_PRIORITY_GLOBAL_SCALE);
		s.switch_margin = TARGET_SWITCH_MARGIN;
		s.bucket_margin = TARGET_BUCKET_MARGIN;
		s.projectile_time_weight = 0.0;
		fill_buckets(s, { "commit", "objective", "peel", "burst", "kite" });
		put(StringName("fighter"), std::move(s));
	}
	{
		UnitStrategy s;
		s.display_name = String("Spellcaster");
		s.distance_weight = DISTANCE_WEIGHT_MAGE;
		s.hp_weight = HP_WEIGHT_MAGE;
		s.stickiness_bonus = STICKINESS_DEFAULT;
		s.in_range_bonus = IN_RANGE_BONUS_MAGE;
		s.tank_penalty = TANK_PENALTY_MAGE;
		s.threat_response_weight = THREAT_RESPONSE_MAGE;
		s.execute_bonus_weight = EXECUTE_BONUS_WEIGHT_DEFAULT;
		s.prey_instinct_weight = 0.0;
		s.prefers_kiting = true;
		s.cluster_weight = CLUSTER_WEIGHT_MAGE;
		s.spacing_weight = SPACING_WEIGHT_MAGE;
		s.bodyguard_weight = 0.0;
		s.obscurance_weight = OBSCURANCE_WEIGHT_DEFAULT;
		s.carry_peel_weight = 60.0;
		s.flanking_weight = 0.0;
		s.threat_decay_rate = THREAT_DECAY_FRAGILE;
		set_role_prio(s.role_priorities, StringName("marksman"), -4.0 * ROLE_PRIORITY_GLOBAL_SCALE);
		set_role_prio(s.role_priorities, StringName("support"), -2.0 * ROLE_PRIORITY_GLOBAL_SCALE);
		s.switch_margin = TARGET_SWITCH_MARGIN;
		s.bucket_margin = TARGET_BUCKET_MARGIN;
		s.projectile_time_weight = PROJECTILE_TIME_WEIGHT_MAGE;
		fill_buckets(s, { "commit", "peel", "objective", "burst", "kite" });
		put(StringName("mage"), std::move(s));
	}
	{
		UnitStrategy s;
		s.display_name = String("Enchanter");
		s.distance_weight = DISTANCE_WEIGHT_SUPPORT;
		s.hp_weight = 0.0;
		s.ally_distance_weight = 1.0;
		s.ally_hp_weight = HP_WEIGHT_SUPPORT;
		s.ally_threat_weight = SCORE_THREAT_WEIGHT_SCALE * SCORE_THREAT_WEIGHT_SCALE;
		set_role_prio(s.ally_role_priorities, StringName("marksman"), -5.0 * ROLE_PRIORITY_GLOBAL_SCALE);
		set_role_prio(s.ally_role_priorities, StringName("mage"), -5.0 * ROLE_PRIORITY_GLOBAL_SCALE);
		set_role_prio(s.ally_role_priorities, StringName("fighter"), -2.0 * ROLE_PRIORITY_GLOBAL_SCALE);
		s.stickiness_bonus = STICKINESS_SUPPORT;
		s.in_range_bonus = IN_RANGE_BONUS_SUPPORT;
		s.tank_penalty = TANK_PENALTY_SUPPORT;
		s.threat_response_weight = THREAT_RESPONSE_SUPPORT;
		s.execute_bonus_weight = 0.0;
		s.prey_instinct_weight = 0.0;
		s.prefers_kiting = true;
		s.cluster_weight = 0.0;
		s.spacing_weight = SPACING_WEIGHT_SUPPORT;
		s.bodyguard_weight = BODYGUARD_WEIGHT_SUPPORT;
		s.obscurance_weight = OBSCURANCE_WEIGHT_DEFAULT;
		s.carry_peel_weight = 0.0;
		s.flanking_weight = 0.0;
		s.threat_decay_rate = THREAT_DECAY_FRAGILE;
		s.uses_ally_targeting = true;
		set_role_prio(s.role_priorities, StringName("assassin"), -8.0 * ROLE_PRIORITY_GLOBAL_SCALE);
		set_role_prio(s.role_priorities, StringName("fighter"), -4.0 * ROLE_PRIORITY_GLOBAL_SCALE);
		s.switch_margin = TARGET_SWITCH_MARGIN;
		s.bucket_margin = TARGET_BUCKET_MARGIN;
		s.projectile_time_weight = PROJECTILE_TIME_WEIGHT_SUPPORT;
		fill_buckets(s, { "commit", "peel", "objective", "kite", "burst" });
		put(StringName("support"), std::move(s));
	}

	_default_strategy = UnitStrategy();
	_default_strategy.display_name = String("Default");
	_default_strategy.distance_weight = 1.0;
	_default_strategy.hp_weight = 0.0;
	_default_strategy.ally_distance_weight = 1.0;
	_default_strategy.ally_hp_weight = 0.0;
	_default_strategy.ally_threat_weight = 0.0;
	_default_strategy.stickiness_bonus = STICKINESS_DEFAULT;
	_default_strategy.in_range_bonus = IN_RANGE_BONUS_DEFAULT;
	_default_strategy.tank_penalty = 2.0;
	_default_strategy.threat_response_weight = 0.0;
	_default_strategy.execute_bonus_weight = 0.0;
	_default_strategy.prey_instinct_weight = 0.0;
	_default_strategy.bodyguard_weight = 0.0;
	_default_strategy.threat_decay_rate = THREAT_DECAY_DEFAULT;
	_default_strategy.switch_margin = TARGET_SWITCH_MARGIN;
	_default_strategy.projectile_time_weight = 0.0;
	_default_strategy.bucket_order_len = 0;
}

const TeamfightSimulationCore::UnitStrategy &TeamfightSimulationCore::_strategy_for_unit(const UnitState &unit) const {
	int64_t slot = unit.role_slot;
	if (slot >= 0 && slot < ROLE_SLOT_COUNT) {
		return _role_strategy_cache_by_slot[static_cast<size_t>(slot)];
	}
	return _default_strategy;
}

void TeamfightSimulationCore::_prepare_tick_context() {
	// Avoid O(n) zero-fill every tick: size changes only on roster change; cluster density overwrites
	// all live slots when any cluster_weight > 0; otherwise density is never read.
	const size_t n = _units.size();
	if (_tick_ctx.density_by_unit_index.size() != n) {
		_tick_ctx.density_by_unit_index.assign(n, 0);
	}
	_tick_ctx.player_backliner_indices.clear();
	_tick_ctx.enemy_backliner_indices.clear();
	_tick_ctx.player_frontline_indices.clear();
	_tick_ctx.enemy_frontline_indices.clear();
	_tick_ctx.player_carry_indices.clear();
	_tick_ctx.enemy_carry_indices.clear();
	_tick_ctx.needs_cluster_density = false;

	double pcx = 0.0;
	double pcy = 0.0;
	int pc = 0;
	for (int64_t idx : _alive_player_indices) {
		const UnitState &u = _units[idx];
		if (!u.alive) {
			continue;
		}
		_sync_targeting_frame_unit(_units[idx]);
		if (!_tick_ctx.needs_cluster_density && _strategy_for_unit(u).cluster_weight > 0.0) {
			_tick_ctx.needs_cluster_density = true;
		}
		pcx += u.pos_x;
		pcy += u.pos_y;
		pc += 1;
		if (u.is_marksman_role || u.is_mage_role || u.is_support_role) {
			_tick_ctx.player_backliner_indices.push_back(idx);
		}
		if (u.is_tank_role || u.is_fighter_role) {
			_tick_ctx.player_frontline_indices.push_back(idx);
		}
		if (u.is_marksman_role || u.is_mage_role) {
			_tick_ctx.player_carry_indices.push_back(idx);
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
		const UnitState &u = _units[idx];
		if (!u.alive) {
			continue;
		}
		_sync_targeting_frame_unit(_units[idx]);
		if (!_tick_ctx.needs_cluster_density && _strategy_for_unit(u).cluster_weight > 0.0) {
			_tick_ctx.needs_cluster_density = true;
		}
		ecx += u.pos_x;
		ecy += u.pos_y;
		ec += 1;
		if (u.is_marksman_role || u.is_mage_role || u.is_support_role) {
			_tick_ctx.enemy_backliner_indices.push_back(idx);
		}
		if (u.is_tank_role || u.is_fighter_role) {
			_tick_ctx.enemy_frontline_indices.push_back(idx);
		}
		if (u.is_marksman_role || u.is_mage_role) {
			_tick_ctx.enemy_carry_indices.push_back(idx);
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
		UnitState *target = _unit_by_id(u.target_id);
		if (target == nullptr || !target->alive) {
			continue;
		}
		target->incoming_target_count += 1;
		_sync_targeting_frame_unit(*target);
	}
	for (int64_t idx : _alive_enemy_indices) {
		const UnitState &u = _units[idx];
		if (!u.alive || u.target_id == 0) {
			continue;
		}
		UnitState *target = _unit_by_id(u.target_id);
		if (target == nullptr || !target->alive) {
			continue;
		}
		target->incoming_target_count += 1;
		_sync_targeting_frame_unit(*target);
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

void TeamfightSimulationCore::_spatial_clear_buckets_aux() const {
	for (auto &bucket : _spatial_buckets_aux) {
		bucket.clear();
	}
}

void TeamfightSimulationCore::_spatial_fill_buckets_for_indices_aux(const std::vector<int64_t> &indices) const {
	_spatial_clear_buckets_aux();
	for (int64_t idx : indices) {
		const UnitState &u = _units[idx];
		if (!u.alive) {
			continue;
		}
		int fi = _spatial_flat_index(u.pos_x, u.pos_y);
		_spatial_buckets_aux[static_cast<size_t>(fi)].push_back(idx);
	}
}

namespace {
// FNV-1a 64-bit: basis and prime (standard constants for string hashing).
constexpr uint64_t kObscuranceAuxSigFnvOffsetBasis = UINT64_C(0xcbf29ce484222325);
constexpr uint64_t kObscuranceAuxSigFnvPrime = UINT64_C(0x100000001b3);

inline uint64_t obscurance_aux_sig_mix(uint64_t h, uint64_t word) {
	h ^= word;
	h *= kObscuranceAuxSigFnvPrime;
	return h;
}
} // namespace

uint64_t TeamfightSimulationCore::_obscurance_aux_frontline_signature(const std::vector<int64_t> &indices) const {
	uint64_t h = kObscuranceAuxSigFnvOffsetBasis;
	for (int64_t idx : indices) {
		const UnitState &u = _units[idx];
		if (!u.alive) {
			continue;
		}
		h = obscurance_aux_sig_mix(h, static_cast<uint64_t>(u.instance_id));
		uint64_t bits_x = 0;
		uint64_t bits_y = 0;
		static_assert(sizeof(double) == sizeof(uint64_t), "double bit pattern for signature");
		std::memcpy(&bits_x, &u.pos_x, sizeof(double));
		std::memcpy(&bits_y, &u.pos_y, sizeof(double));
		h = obscurance_aux_sig_mix(h, bits_x);
		h = obscurance_aux_sig_mix(h, bits_y);
	}
	return h;
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

int TeamfightSimulationCore::_spatial_count_obscurance_blockers_cached(double ux, double uy, double tx, double ty, int64_t target_instance_id) const {
	double pad = OBSCURANCE_LINE_RADIUS;
	double minx = Math::min(ux, tx) - pad;
	double maxx = Math::max(ux, tx) + pad;
	double miny = Math::min(uy, ty) - pad;
	double maxy = Math::max(uy, ty) + pad;
	double cs = _spatial_cell_size();
	int ix0 = int(Math::floor((minx - WORLD_BOUNDARY_MIN) / cs));
	int ix1 = int(Math::floor((maxx - WORLD_BOUNDARY_MIN) / cs));
	int iy0 = int(Math::floor((miny - WORLD_BOUNDARY_MIN) / cs));
	int iy1 = int(Math::floor((maxy - WORLD_BOUNDARY_MIN) / cs));
	ix0 = CLAMP(ix0, 0, SPATIAL_GRID_DIM - 1);
	ix1 = CLAMP(ix1, 0, SPATIAL_GRID_DIM - 1);
	iy0 = CLAMP(iy0, 0, SPATIAL_GRID_DIM - 1);
	iy1 = CLAMP(iy1, 0, SPATIAL_GRID_DIM - 1);
	double segx = tx - ux;
	double segy = ty - uy;
	double seg_len_sq = segx * segx + segy * segy;
	int blockers = 0;
	for (int iy = iy0; iy <= iy1; ++iy) {
		for (int ix = ix0; ix <= ix1; ++ix) {
			int fi = iy * SPATIAL_GRID_DIM + ix;
			for (int64_t idx : _spatial_buckets_aux[static_cast<size_t>(fi)]) {
				const UnitState &other = _units[idx];
				if (other.instance_id == target_instance_id) {
					continue;
				}
				double ox = other.pos_x;
				double oy = other.pos_y;
				double odx = ox - ux;
				double ody = oy - uy;
				double other_dist_sq = odx * odx + ody * ody;
				if (seg_len_sq > EPSILON && other_dist_sq >= seg_len_sq) {
					continue;
				}
				double dist_sq = 0.0;
				if (seg_len_sq <= EPSILON) {
					dist_sq = other_dist_sq;
				} else {
					double t = (odx * segx + ody * segy) / seg_len_sq;
					t = Math::clamp(t, 0.0, 1.0);
					double px = ux + segx * t;
					double py = uy + segy * t;
					double ddx = ox - px;
					double ddy = oy - py;
					dist_sq = ddx * ddx + ddy * ddy;
				}
				if (dist_sq <= OBSCURANCE_LINE_RADIUS * OBSCURANCE_LINE_RADIUS) {
					blockers += 1;
				}
			}
		}
	}
	return blockers;
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

int TeamfightSimulationCore::_spatial_count_obscurance_blockers(double ux, double uy, double tx, double ty, const std::vector<int64_t> &frontline_indices, int64_t target_instance_id) const {
	_spatial_clear_buckets_aux();
	for (int64_t idx : frontline_indices) {
		const UnitState &u = _units[idx];
		if (!u.alive) {
			continue;
		}
		int fi = _spatial_flat_index(u.pos_x, u.pos_y);
		_spatial_buckets_aux[static_cast<size_t>(fi)].push_back(idx);
	}
	double pad = OBSCURANCE_LINE_RADIUS;
	double minx = Math::min(ux, tx) - pad;
	double maxx = Math::max(ux, tx) + pad;
	double miny = Math::min(uy, ty) - pad;
	double maxy = Math::max(uy, ty) + pad;
	double cs = _spatial_cell_size();
	int ix0 = int(Math::floor((minx - WORLD_BOUNDARY_MIN) / cs));
	int ix1 = int(Math::floor((maxx - WORLD_BOUNDARY_MIN) / cs));
	int iy0 = int(Math::floor((miny - WORLD_BOUNDARY_MIN) / cs));
	int iy1 = int(Math::floor((maxy - WORLD_BOUNDARY_MIN) / cs));
	ix0 = CLAMP(ix0, 0, SPATIAL_GRID_DIM - 1);
	ix1 = CLAMP(ix1, 0, SPATIAL_GRID_DIM - 1);
	iy0 = CLAMP(iy0, 0, SPATIAL_GRID_DIM - 1);
	iy1 = CLAMP(iy1, 0, SPATIAL_GRID_DIM - 1);
	double segx = tx - ux;
	double segy = ty - uy;
	double seg_len_sq = segx * segx + segy * segy;
	int blockers = 0;
	for (int iy = iy0; iy <= iy1; ++iy) {
		for (int ix = ix0; ix <= ix1; ++ix) {
			int fi = iy * SPATIAL_GRID_DIM + ix;
			for (int64_t idx : _spatial_buckets_aux[static_cast<size_t>(fi)]) {
				const UnitState &other = _units[idx];
				if (other.instance_id == target_instance_id) {
					continue;
				}
				double ox = other.pos_x;
				double oy = other.pos_y;
				double odx = ox - ux;
				double ody = oy - uy;
				double other_dist_sq = odx * odx + ody * ody;
				if (seg_len_sq > EPSILON && other_dist_sq >= seg_len_sq) {
					continue;
				}
				double dist_sq = 0.0;
				if (seg_len_sq <= EPSILON) {
					dist_sq = other_dist_sq;
				} else {
					double t = (odx * segx + ody * segy) / seg_len_sq;
					t = Math::clamp(t, 0.0, 1.0);
					double px = ux + segx * t;
					double py = uy + segy * t;
					double ddx = ox - px;
					double ddy = oy - py;
					dist_sq = ddx * ddx + ddy * ddy;
				}
				if (dist_sq <= OBSCURANCE_LINE_RADIUS * OBSCURANCE_LINE_RADIUS) {
					blockers += 1;
				}
			}
		}
	}
	return blockers;
}

bool TeamfightSimulationCore::_use_spatial_broad_phase() const {
	return int64_t(_alive_player_indices.size()) >= SPATIAL_BROAD_PHASE_TEAM_THRESHOLD
			|| int64_t(_alive_enemy_indices.size()) >= SPATIAL_BROAD_PHASE_TEAM_THRESHOLD;
}

double TeamfightSimulationCore::_score_ally_target(const UnitState &unit, const TargetingFrameEntry &ally, const TeamfightSimulationCore::UnitStrategy &strategy, double unit_ally_distance) const {
	double dist = unit_ally_distance >= 0.0 ? unit_ally_distance : Math::sqrt((ally.pos_x - unit.pos_x) * (ally.pos_x - unit.pos_x) + (ally.pos_y - unit.pos_y) * (ally.pos_y - unit.pos_y));
	double hp_ratio = ally.hp / Math::max(0.0001, ally.max_hp);
	double score = dist * strategy.ally_distance_weight;
	score += hp_ratio * strategy.ally_hp_weight * SCORE_HP_WEIGHT_SCALE;
	score -= ally.perceived_threat * strategy.ally_threat_weight;
	score += strategy_role_prio(strategy.ally_role_priorities, ally.role_slot);
	return score;
}

double TeamfightSimulationCore::_score_enemy_target_prefix(const UnitState &attacker, const TargetingFrameEntry &enemy, const TargetingFrameEntry *ally_for_peel, const TeamfightSimulationCore::UnitStrategy &strategy, const TeamfightSimulationCore::TickContext &ctx, const TeamfightSimulationCore::TargetScoreContext &score_ctx, double attacker_enemy_distance, bool profile_score, int64_t enemy_index) {
	if (profile_score) {
		_sim_profile_se_calls += 1;
	}
	const bool attacker_is_player = attacker.team == sn_player();
	const std::vector<int64_t> &carry_indices = attacker_is_player ? ctx.player_carry_indices : ctx.enemy_carry_indices;
	const std::vector<int64_t> &frontline_indices = enemy.is_player_team ? ctx.player_frontline_indices : ctx.enemy_frontline_indices;
	const int64_t enemy_role_slot = enemy.role_slot;

	double score = 0.0;
	double dist = 0.0;
	double attack_range = score_ctx.attack_range;
	double effective_range = score_ctx.effective_range;
	{
		SimProfileAccScope _se_base(profile_score, _sim_profile_se_base);
		dist = attacker_enemy_distance >= 0.0 ? attacker_enemy_distance : Math::sqrt((enemy.pos_x - attacker.pos_x) * (enemy.pos_x - attacker.pos_x) + (enemy.pos_y - attacker.pos_y) * (enemy.pos_y - attacker.pos_y));
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
		score += Math::pow(norm_gap, DISTANCE_EXPONENT) * strategy.distance_weight * SCORE_DISTANCE_WEIGHT_SCALE;
		if (in_range) {
			score -= strategy.in_range_bonus;
		}
		score += hp_ratio * strategy.hp_weight * SCORE_HP_WEIGHT_SCALE;
		if (strategy.execute_bonus_weight > 0.0 && in_range && hp_ratio <= TARGET_EXECUTE_HP_RATIO) {
			score -= strategy.execute_bonus_weight;
		}
		score += strategy_role_prio(strategy.role_priorities, enemy_role_slot);
		if (enemy.is_tank_role) {
			score += strategy.tank_penalty;
			// Assassins apply an extra tank penalty if there are backliners alive.
			if (attacker.is_assassin_role) {
				int64_t enemy_self_idx = enemy_index >= 0 ? enemy_index : _unit_index_by_id(enemy.instance_id);
				const std::vector<int64_t> &bl = enemy.is_player_team ? ctx.player_backliner_indices : ctx.enemy_backliner_indices;
				int n_alive = enemy.is_player_team ? ctx.player_backliner_alive_count : ctx.enemy_backliner_alive_count;
				bool self_in_bl = false;
				for (int64_t idx : bl) {
					if (idx == enemy_self_idx) {
						self_in_bl = true;
						break;
					}
				}
				int subtract = (self_in_bl && enemy.alive) ? 1 : 0;
				if (n_alive - subtract > 0) {
					score += ASSASSIN_TANK_CONTEXT_PENALTY;
				}
			}
		}
		if (enemy.target_id == attacker.instance_id) {
			double falloff = 1.0 / (1.0 + Math::max(0.0, dist - attack_range) * THREAT_RESPONSE_RANGE_FALLOFF);
			score -= strategy.threat_response_weight * falloff;
		}
		double enemy_incoming = double(enemy.incoming_target_count);
		double prey_focus = enemy_incoming * PREY_INCOMING_TARGET_SCALE + enemy.perceived_threat * PREY_PERCEIVED_THREAT_SCALE;
		if (enemy.is_tank_role || enemy.is_fighter_role) {
			prey_focus *= PREY_FRONTLINE_SCALE;
		}
		score -= prey_focus * strategy.prey_instinct_weight;
		if (attacker.target_id == enemy.instance_id) {
			score -= Math::max(1.0, strategy.distance_weight) * strategy.stickiness_bonus;
		}
		if (attacker.is_support_role) {
			int64_t ally_target_id = attacker.current_ally_target_id;
			if (ally_target_id != 0 && ally_for_peel != nullptr && ally_for_peel->alive) {
				double ally_incoming = double(ally_for_peel->incoming_target_count);
				double ally_threat = ally_for_peel->perceived_threat;
				if ((ally_incoming >= SUPPORT_PEEL_THREAT_THRESHOLD || ally_threat >= SUPPORT_PEEL_THREAT_THRESHOLD) && enemy.target_id == ally_target_id) {
					score -= SUPPORT_PEEL_BOOST;
				}
			}
		}
	}

	{
		SimProfileAccScope _se_fl(profile_score, _sim_profile_se_flanking);
		// Flanking (assassin): reward isolation from enemy team center.
		double flanking_weight = strategy.flanking_weight;
		if (flanking_weight > 0.0) {
			double cx = 0.0;
			double cy = 0.0;
			bool ok_center = false;
			if (enemy.is_player_team) {
				ok_center = ctx.has_player_center;
				cx = ctx.player_team_center.x;
				cy = ctx.player_team_center.y;
			} else {
				ok_center = ctx.has_enemy_center;
				cx = ctx.enemy_team_center.x;
				cy = ctx.enemy_team_center.y;
			}
			if (ok_center) {
				double ex = enemy.pos_x;
				double ey = enemy.pos_y;
				double to_tx = ex - cx;
				double to_ty = ey - cy;
				double ax = attacker.pos_x;
				double ay = attacker.pos_y;
				double to_ax = ax - ex;
				double to_ay = ay - ey;
				double len_t_sq = to_tx * to_tx + to_ty * to_ty;
				double len_a_sq = to_ax * to_ax + to_ay * to_ay;
				if (len_t_sq > EPSILON * EPSILON && len_a_sq > EPSILON * EPSILON) {
					double len_prod = Math::sqrt(len_t_sq * len_a_sq);
					double align = (to_tx * to_ax + to_ty * to_ay) / len_prod;
					align = Math::max(0.0, align);
					double len_t = Math::sqrt(len_t_sq);
					double isolation = Math::min(1.0, len_t * FLANKING_TEAM_CENTER_SCALE);
					score -= align * isolation * flanking_weight;
				}
			}
		}
		// Commit bucket: forced target is massively preferred (Python TAUNT_SCORE_BONUS).
		if (attacker.forced_target_id != 0 && attacker.forced_target_remaining > 0.0 && enemy.instance_id == attacker.forced_target_id) {
			score += TAUNT_SCORE_BONUS;
		}
	}

	return score;
}

double TeamfightSimulationCore::_score_enemy_target(const UnitState &attacker, const TargetingFrameEntry &enemy, const TargetingFrameEntry *ally_for_peel, const TeamfightSimulationCore::UnitStrategy &strategy, const TeamfightSimulationCore::TickContext &ctx, const TeamfightSimulationCore::TargetScoreContext &score_ctx, double attacker_enemy_distance, bool profile_score, int64_t enemy_index) {
	if (profile_score) {
		_sim_profile_se_calls += 1;
	}
	const bool attacker_is_player = attacker.team == sn_player();
	const std::vector<int64_t> &carry_indices = attacker_is_player ? ctx.player_carry_indices : ctx.enemy_carry_indices;
	const std::vector<int64_t> &frontline_indices = enemy.is_player_team ? ctx.player_frontline_indices : ctx.enemy_frontline_indices;
	const int64_t enemy_role_slot = enemy.role_slot;

	double score = 0.0;
	double dist = 0.0;
	double attack_range = score_ctx.attack_range;
	double effective_range = score_ctx.effective_range;
	{
		SimProfileAccScope _se_base(profile_score, _sim_profile_se_base);
		dist = attacker_enemy_distance >= 0.0 ? attacker_enemy_distance : Math::sqrt((enemy.pos_x - attacker.pos_x) * (enemy.pos_x - attacker.pos_x) + (enemy.pos_y - attacker.pos_y) * (enemy.pos_y - attacker.pos_y));
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
		score += Math::pow(norm_gap, DISTANCE_EXPONENT) * strategy.distance_weight * SCORE_DISTANCE_WEIGHT_SCALE;
		if (in_range) {
			score -= strategy.in_range_bonus;
		}
		score += hp_ratio * strategy.hp_weight * SCORE_HP_WEIGHT_SCALE;
		if (strategy.execute_bonus_weight > 0.0 && in_range && hp_ratio <= TARGET_EXECUTE_HP_RATIO) {
			score -= strategy.execute_bonus_weight;
		}
		score += strategy_role_prio(strategy.role_priorities, enemy_role_slot);
		if (enemy.is_tank_role) {
			score += strategy.tank_penalty;
			// Assassins apply an extra tank penalty if there are backliners alive.
			if (attacker.is_assassin_role) {
				int64_t enemy_self_idx = enemy_index >= 0 ? enemy_index : _unit_index_by_id(enemy.instance_id);
				const std::vector<int64_t> &bl = enemy.is_player_team ? ctx.player_backliner_indices : ctx.enemy_backliner_indices;
				int n_alive = enemy.is_player_team ? ctx.player_backliner_alive_count : ctx.enemy_backliner_alive_count;
				bool self_in_bl = false;
				for (int64_t idx : bl) {
					if (idx == enemy_self_idx) {
						self_in_bl = true;
						break;
					}
				}
				int subtract = (self_in_bl && enemy.alive) ? 1 : 0;
				if (n_alive - subtract > 0) {
					score += ASSASSIN_TANK_CONTEXT_PENALTY;
				}
			}
		}
		if (enemy.target_id == attacker.instance_id) {
			double falloff = 1.0 / (1.0 + Math::max(0.0, dist - attack_range) * THREAT_RESPONSE_RANGE_FALLOFF);
			score -= strategy.threat_response_weight * falloff;
		}
		double enemy_incoming = double(enemy.incoming_target_count);
		double prey_focus = enemy_incoming * PREY_INCOMING_TARGET_SCALE + enemy.perceived_threat * PREY_PERCEIVED_THREAT_SCALE;
		if (enemy.is_tank_role || enemy.is_fighter_role) {
			prey_focus *= PREY_FRONTLINE_SCALE;
		}
		score -= prey_focus * strategy.prey_instinct_weight;
		if (attacker.target_id == enemy.instance_id) {
			score -= Math::max(1.0, strategy.distance_weight) * strategy.stickiness_bonus;
		}
		if (attacker.is_support_role) {
			int64_t ally_target_id = attacker.current_ally_target_id;
			if (ally_target_id != 0 && ally_for_peel != nullptr && ally_for_peel->alive) {
				double ally_incoming = double(ally_for_peel->incoming_target_count);
				double ally_threat = ally_for_peel->perceived_threat;
				if ((ally_incoming >= SUPPORT_PEEL_THREAT_THRESHOLD || ally_threat >= SUPPORT_PEEL_THREAT_THRESHOLD) && enemy.target_id == ally_target_id) {
					score -= SUPPORT_PEEL_BOOST;
				}
			}
		}
	}

	{
		SimProfileAccScope _se_bg(profile_score, _sim_profile_se_bodyguard);
		double bodyguard_weight = strategy.bodyguard_weight;
		if (bodyguard_weight > 0.0) {
			const double bodyguard_r2 = BODYGUARD_RADIUS * BODYGUARD_RADIUS;
			for (int64_t ally_index : carry_indices) {
				const TargetingFrameEntry &ally = _targeting_frame[static_cast<size_t>(ally_index)];
				if (!ally.alive) {
					continue;
				}
				double adx = ally.pos_x - enemy.pos_x;
				double ady = ally.pos_y - enemy.pos_y;
				double ally_dist_sq = adx * adx + ady * ady;
				if (ally_dist_sq < bodyguard_r2) {
					double ally_dist = Math::sqrt(ally_dist_sq);
					double guard_bonus = (1.0 - (ally_dist / BODYGUARD_RADIUS)) * bodyguard_weight;
					score -= guard_bonus;
				}
			}
		}
	}

	{
		SimProfileAccScope _se_base2(profile_score, _sim_profile_se_base);
		// Cluster awareness (density).
		double cluster_weight = strategy.cluster_weight;
		if (cluster_weight > 0.0) {
			int64_t enemy_idx = enemy_index >= 0 ? enemy_index : _unit_index_by_id(enemy.instance_id);
			int64_t dens = 0;
			if (enemy_idx >= 0 && enemy_idx < int64_t(ctx.density_by_unit_index.size())) {
				dens = ctx.density_by_unit_index[static_cast<size_t>(enemy_idx)];
			}
			score -= double(dens) * cluster_weight;
		}

		// Spacing awareness.
		double spacing_weight = strategy.spacing_weight;
		if (spacing_weight > 0.0) {
			score += Math::pow(double(enemy.incoming_target_count), SPACING_EXPONENT) * spacing_weight;
		}

		// Carry peel (for threatened carries).
		double carry_peel_weight = strategy.carry_peel_weight;
		if (carry_peel_weight > 0.0) {
			if ((attacker.is_marksman_role || attacker.is_mage_role) && enemy.target_id == attacker.instance_id) {
				double falloff = 1.0 / (1.0 + Math::max(0.0, dist - attack_range) * THREAT_RESPONSE_RANGE_FALLOFF);
				score -= carry_peel_weight * falloff;
			}
		}

		// Projectile tempo: penalize time-to-hit for ranged attackers.
		double projectile_time_weight = strategy.projectile_time_weight;
		if (projectile_time_weight > 0.0 && attack_range > RANGED_THRESHOLD) {
			double proj_speed = attacker.combat.projectile_speed;
			if (proj_speed > EPSILON) {
				double t_hit = dist / proj_speed;
				score += t_hit * projectile_time_weight;
			}
		}

		// Kiting tempo bonus inside preferred window.
		// Python parity: kite tempo scoring applies whenever prefers_kiting is true.
		if (strategy.prefers_kiting && score_ctx.has_kite_bounds) {
			double min_w = score_ctx.kite_min_w;
			double max_w = score_ctx.kite_max_w;
			if (dist >= min_w && dist <= max_w && max_w > min_w) {
				double kite_ratio = (dist - min_w) / (max_w - min_w);
				score -= kite_ratio * SCORE_KITING_WEIGHT_SCALE;
			}
		}
	}

	{
		SimProfileAccScope _se_obs(profile_score, _sim_profile_se_obscurance);
		// Obscurance: penalize targeting past frontline blockers.
		double obscurance_weight = strategy.obscurance_weight;
		if (obscurance_weight > 0.0) {
			int blockers = 0;
			if (score_ctx.use_spatial && score_ctx.has_obscurance_cache) {
				blockers = _spatial_count_obscurance_blockers_cached(attacker.pos_x, attacker.pos_y, enemy.pos_x, enemy.pos_y, enemy.instance_id);
			} else {
				double ux = attacker.pos_x;
				double uy = attacker.pos_y;
				double tx = enemy.pos_x;
				double ty = enemy.pos_y;
				double segx = tx - ux;
				double segy = ty - uy;
				double seg_len_sq = segx * segx + segy * segy;
				for (int64_t idx : frontline_indices) {
					const UnitState &other = _units[idx];
					if (!other.alive) {
						continue;
					}
					if (other.instance_id == enemy.instance_id) {
						continue;
					}
					double ox = other.pos_x;
					double oy = other.pos_y;
					double odx = ox - ux;
					double ody = oy - uy;
					double other_dist_sq = odx * odx + ody * ody;
					if (seg_len_sq > EPSILON && other_dist_sq >= seg_len_sq) {
						continue;
					}
					double dist_sq = 0.0;
					if (seg_len_sq <= EPSILON) {
						dist_sq = other_dist_sq;
					} else {
						double t = (odx * segx + ody * segy) / seg_len_sq;
						t = Math::clamp(t, 0.0, 1.0);
						double px = ux + segx * t;
						double py = uy + segy * t;
						double ddx = ox - px;
						double ddy = oy - py;
						dist_sq = ddx * ddx + ddy * ddy;
					}
					if (dist_sq <= OBSCURANCE_LINE_RADIUS * OBSCURANCE_LINE_RADIUS) {
						blockers += 1;
					}
				}
			}
			score += double(blockers) * obscurance_weight;
		}
	}

	{
		SimProfileAccScope _se_fl(profile_score, _sim_profile_se_flanking);
		// Flanking (assassin): reward isolation from enemy team center.
		double flanking_weight = strategy.flanking_weight;
		if (flanking_weight > 0.0) {
			double cx = 0.0;
			double cy = 0.0;
			bool ok_center = false;
			if (enemy.is_player_team) {
				ok_center = ctx.has_player_center;
				cx = ctx.player_team_center.x;
				cy = ctx.player_team_center.y;
			} else {
				ok_center = ctx.has_enemy_center;
				cx = ctx.enemy_team_center.x;
				cy = ctx.enemy_team_center.y;
			}
			if (ok_center) {
				double ex = enemy.pos_x;
				double ey = enemy.pos_y;
				double to_tx = ex - cx;
				double to_ty = ey - cy;
				double ax = attacker.pos_x;
				double ay = attacker.pos_y;
				double to_ax = ax - ex;
				double to_ay = ay - ey;
				double len_t_sq = to_tx * to_tx + to_ty * to_ty;
				double len_a_sq = to_ax * to_ax + to_ay * to_ay;
				if (len_t_sq > EPSILON * EPSILON && len_a_sq > EPSILON * EPSILON) {
					double len_prod = Math::sqrt(len_t_sq * len_a_sq);
					double align = (to_tx * to_ax + to_ty * to_ay) / len_prod;
					align = Math::max(0.0, align);
					double len_t = Math::sqrt(len_t_sq);
					double isolation = Math::min(1.0, len_t * FLANKING_TEAM_CENTER_SCALE);
					score -= align * isolation * flanking_weight;
				}
			}
		}
		// Commit bucket: forced target is massively preferred (Python TAUNT_SCORE_BONUS).
		if (attacker.forced_target_id != 0 && attacker.forced_target_remaining > 0.0 && enemy.instance_id == attacker.forced_target_id) {
			score += TAUNT_SCORE_BONUS;
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
	_sync_targeting_frame_unit(unit);
}

std::vector<int64_t> &TeamfightSimulationCore::_alive_indices_for_team(const StringName &team) {
	if (team == StringName("player")) {
		return _alive_player_indices;
	}
	return _alive_enemy_indices;
}

const std::vector<int64_t> &TeamfightSimulationCore::_alive_indices_for_team(const StringName &team) const {
	if (team == StringName("player")) {
		return _alive_player_indices;
	}
	return _alive_enemy_indices;
}

void TeamfightSimulationCore::_add_alive_index(const StringName &team, int64_t index) {
	std::vector<int64_t> &alive_indices = _alive_indices_for_team(team);
	for (int64_t existing : alive_indices) {
		if (existing == index) {
			return;
		}
	}
	alive_indices.push_back(index);
}

void TeamfightSimulationCore::_remove_alive_index(const StringName &team, int64_t index) {
	std::vector<int64_t> &alive_indices = _alive_indices_for_team(team);
	for (size_t i = 0; i < alive_indices.size(); ++i) {
		if (alive_indices[i] == index) {
			alive_indices.erase(alive_indices.begin() + long(i));
			break;
		}
	}
}

void TeamfightSimulationCore::_adjust_target_pressure(int64_t old_target_id, int64_t new_target_id) {
	if (old_target_id == new_target_id) {
		return;
	}
	if (old_target_id != 0) {
		UnitState *old_unit = _unit_by_id(old_target_id);
		if (old_unit != nullptr) {
			old_unit->incoming_target_count = std::max<int64_t>(0, old_unit->incoming_target_count - 1);
			_sync_targeting_frame_unit(*old_unit);
		}
	}
	if (new_target_id != 0) {
		UnitState *new_unit = _unit_by_id(new_target_id);
		if (new_unit != nullptr) {
			new_unit->incoming_target_count += 1;
			_sync_targeting_frame_unit(*new_unit);
		}
	}
}

void TeamfightSimulationCore::_refresh_target_pressure(bool update_cluster_density) {
	for (const int64_t index : _alive_player_indices) {
		UnitState &unit = _units[index];
		int64_t target_id = unit.target_id;
		if (target_id == 0) {
			continue;
		}
		UnitState *target = _unit_by_id(target_id);
		if (target == nullptr || !target->alive) {
			continue;
		}
		target->incoming_target_count += 1;
		_sync_targeting_frame_unit(*target);
	}
	for (const int64_t index : _alive_enemy_indices) {
		UnitState &unit = _units[index];
		int64_t target_id = unit.target_id;
		if (target_id == 0) {
			continue;
		}
		UnitState *target = _unit_by_id(target_id);
		if (target == nullptr || !target->alive) {
			continue;
		}
		target->incoming_target_count += 1;
		_sync_targeting_frame_unit(*target);
	}

	if (!update_cluster_density) {
		return;
	}

	// Python parity: cache per-unit density used by cluster scoring (stored on TickContext).
	const bool needs_density = _tick_ctx.needs_cluster_density;
	if (needs_density) {
		auto compute_density_for_team = [&](const std::vector<int64_t> &indices) {
			if (int64_t(indices.size()) >= SPATIAL_BROAD_PHASE_TEAM_THRESHOLD) {
				_spatial_fill_buckets_for_indices(indices);
				for (int64_t i = 0; i < int64_t(indices.size()); ++i) {
					int64_t ui = indices[i];
					UnitState &u = _units[ui];
					if (!u.alive) {
						if (ui >= 0 && ui < int64_t(_tick_ctx.density_by_unit_index.size())) {
							_tick_ctx.density_by_unit_index[static_cast<size_t>(ui)] = 0;
						}
						continue;
					}
					int count = _spatial_count_neighbors_in_grid(ui, u.pos_x, u.pos_y, AOE_DENSITY_RADIUS);
					if (ui >= 0 && ui < int64_t(_tick_ctx.density_by_unit_index.size())) {
						_tick_ctx.density_by_unit_index[static_cast<size_t>(ui)] = count;
					}
				}
			} else {
				const double r2 = AOE_DENSITY_RADIUS * AOE_DENSITY_RADIUS;
				for (int64_t i = 0; i < int64_t(indices.size()); ++i) {
					int64_t ui = indices[i];
					UnitState &u = _units[ui];
					if (!u.alive) {
						if (ui >= 0 && ui < int64_t(_tick_ctx.density_by_unit_index.size())) {
							_tick_ctx.density_by_unit_index[static_cast<size_t>(ui)] = 0;
						}
						continue;
					}
					int count = 0;
					for (int64_t j = 0; j < int64_t(indices.size()); ++j) {
						if (i == j) {
							continue;
						}
						const UnitState &other = _units[indices[j]];
						if (!other.alive) {
							continue;
						}
						double dx = u.pos_x - other.pos_x;
						double dy = u.pos_y - other.pos_y;
						if (dx * dx + dy * dy <= r2) {
							count += 1;
						}
					}
					if (ui >= 0 && ui < int64_t(_tick_ctx.density_by_unit_index.size())) {
						_tick_ctx.density_by_unit_index[static_cast<size_t>(ui)] = count;
					}
				}
			}
		};
		compute_density_for_team(_alive_player_indices);
		compute_density_for_team(_alive_enemy_indices);
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
	_debug_stack_operations = bool(match_input.get("debug_stack_operations", false));
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
		double sx = source.pos_x;
		double sy = source.pos_y;
		double tx = target->pos_x;
		double ty = target->pos_y;
		double dx = tx - sx;
		double dy = ty - sy;
		context.distance = Math::sqrt(dx * dx + dy * dy);
	}
	return context;
}

const std::vector<TeamfightSimulationCore::EffectRecord> &TeamfightSimulationCore::_collect_effects(const UnitState &unit, const StringName &kind) {
	static const std::vector<EffectRecord> EMPTY_EFFECTS;
	if (kind == sn_on_attack()) {
		return _uc(unit).passive_effects[0];
	}
	if (kind == sn_on_defense()) {
		return _uc(unit).passive_effects[1];
	}
	if (kind == sn_on_tick()) {
		return _uc(unit).passive_effects[2];
	}
	if (kind == sn_post_attack()) {
		return _uc(unit).passive_effects[3];
	}
	if (kind == sn_post_take_damage()) {
		return _uc(unit).passive_effects[4];
	}
	if (kind == sn_on_ability()) {
		return _uc(unit).passive_effects[5];
	}
	if (kind == sn_on_ultimate()) {
		return _uc(unit).passive_effects[6];
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
		return target.reflect_buff_remaining > 0.0 || target.reflect_passive_pct_all > 0.0 || target.reflect_passive_pct_physical > 0.0;
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
				double hp_ratio = context.source->hp / Math::max(0.0001, context.source->combat.max_hp);
				if (hp_ratio > effect.scalar0) {
					return effect.scalar2;
				}
			}
			// Check target HP ratio for below_threshold
			if (effect.scalar1 > 0.0 && context.target != nullptr) {
				double target_hp = context.target->hp;
				double target_max_hp = Math::max(0.0001, context.target->combat.max_hp);
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
	
	double final_damage = pre_res;
	if (damage_is_physical) {
		double armor = get_effective_armor(target);
		final_damage *= Math::clamp(1.0 - armor, 0.05, 1.0);
	} else if (damage_is_magic) {
		double mr = get_effective_magic_resist(target);
		final_damage *= Math::clamp(1.0 - mr, 0.05, 1.0);
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
	double pct = defender.reflect_passive_pct_all;
	if (damage_type == sn_physical()) {
		pct += defender.reflect_passive_pct_physical;
	}
	if (defender.reflect_buff_remaining > 0.0) {
		pct += defender.reflect_buff_pct_all;
		if (damage_type == sn_physical()) {
			pct += defender.reflect_buff_pct_physical;
		}
	}
	pct = Math::clamp(pct, 0.0, 1.0);
	if (pct <= 1e-9) {
		return;
	}
	double reflected = total_damage_applied * pct;
	if (reflected <= 1e-9) {
		return;
	}
	EffectContext bounce = context;
	bounce.suppress_reflect_chain = true;
	bounce.source = &defender;
	bounce.target = &attacker;
	_apply_damage(defender, attacker, reflected, damage_type, sn_passive(), bounce);
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

void TeamfightSimulationCore::_apply_stun(UnitState &source, UnitState &target, double duration) {
	if (duration <= 0.0) {
		return;
	}
	// Python parity: apply tenacity to reduce stun duration.
	double tenacity = target.combat.tenacity;
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
	double tenacity = target.combat.tenacity;
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
	double tenacity = target.combat.tenacity;
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
	double tenacity = target.combat.tenacity;
	double effective_duration = duration * (1.0 - tenacity);
	if (effective_duration <= 0.0) {
		return;
	}
	target.silence_remaining = Math::max(target.silence_remaining, effective_duration);
	if (block_abilities) {
		target.silence_blocks_abilities = true;
	}
	if (block_ultimate) {
		target.silence_blocks_ultimates = true;
	}
}

void TeamfightSimulationCore::_apply_disarm(UnitState &source, UnitState &target, double duration) {
	(void)source;
	if (duration <= 0.0) {
		return;
	}
	double tenacity = target.combat.tenacity;
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

void TeamfightSimulationCore::_apply_self_aoe_slow(UnitState &source, double radius, double slow_percentage, double duration) {
	if (radius <= 0.0 || duration <= 0.0 || slow_percentage <= 0.0) {
		return;
	}
	_viewer_record_aoe_ring_fx(source, source, radius, StringName("aoe_slow"));
	StringName enemy_team = source.team == sn_player() ? sn_enemy() : sn_player();
	const std::vector<int64_t> &enemy_indices = _alive_indices_for_team(enemy_team);
	AoCircleIterationParams cir;
	cir.center_x = source.pos_x;
	cir.center_y = source.pos_y;
	cir.radius = radius;
	cir.indices = &enemy_indices;
	cir.spatial_team = enemy_team;
	_for_each_unit_in_circle(cir, [&](UnitState &unit) {
		_apply_slow(source, unit, slow_percentage, duration);
	});
}

void TeamfightSimulationCore::_apply_self_aoe_root(UnitState &source, double radius, double duration) {
	if (radius <= 0.0 || duration <= 0.0) {
		return;
	}
	_viewer_record_aoe_ring_fx(source, source, radius, StringName("aoe_root"));
	StringName enemy_team = source.team == sn_player() ? sn_enemy() : sn_player();
	const std::vector<int64_t> &enemy_indices = _alive_indices_for_team(enemy_team);
	AoCircleIterationParams cir;
	cir.center_x = source.pos_x;
	cir.center_y = source.pos_y;
	cir.radius = radius;
	cir.indices = &enemy_indices;
	cir.spatial_team = enemy_team;
	_for_each_unit_in_circle(cir, [&](UnitState &unit) {
		_apply_root(source, unit, duration);
	});
}

void TeamfightSimulationCore::_apply_self_aoe_silence(UnitState &source, double radius, double duration, bool block_abilities, bool block_ultimate) {
	if (radius <= 0.0 || duration <= 0.0) {
		return;
	}
	_viewer_record_aoe_ring_fx(source, source, radius, StringName("aoe_silence"));
	StringName enemy_team = source.team == sn_player() ? sn_enemy() : sn_player();
	const std::vector<int64_t> &enemy_indices = _alive_indices_for_team(enemy_team);
	AoCircleIterationParams cir;
	cir.center_x = source.pos_x;
	cir.center_y = source.pos_y;
	cir.radius = radius;
	cir.indices = &enemy_indices;
	cir.spatial_team = enemy_team;
	_for_each_unit_in_circle(cir, [&](UnitState &unit) {
		_apply_silence(source, unit, duration, block_abilities, block_ultimate);
	});
}

void TeamfightSimulationCore::_apply_self_aoe_disarm(UnitState &source, double radius, double duration) {
	if (radius <= 0.0 || duration <= 0.0) {
		return;
	}
	_viewer_record_aoe_ring_fx(source, source, radius, StringName("aoe_disarm"));
	StringName enemy_team = source.team == sn_player() ? sn_enemy() : sn_player();
	const std::vector<int64_t> &enemy_indices = _alive_indices_for_team(enemy_team);
	AoCircleIterationParams cir;
	cir.center_x = source.pos_x;
	cir.center_y = source.pos_y;
	cir.radius = radius;
	cir.indices = &enemy_indices;
	cir.spatial_team = enemy_team;
	_for_each_unit_in_circle(cir, [&](UnitState &unit) {
		_apply_disarm(source, unit, duration);
	});
}

void TeamfightSimulationCore::_apply_target_aoe_slow(UnitState &source, UnitState &target, double radius, double slow_percentage, double duration) {
	if (radius <= 0.0 || duration <= 0.0 || slow_percentage <= 0.0) {
		return;
	}
	_viewer_record_aoe_ring_fx(source, target, radius, StringName("aoe_slow"));
	StringName enemy_team = source.team == sn_player() ? sn_enemy() : sn_player();
	const std::vector<int64_t> &enemy_indices = _alive_indices_for_team(enemy_team);
	AoCircleIterationParams cir;
	cir.center_x = target.pos_x;
	cir.center_y = target.pos_y;
	cir.radius = radius;
	cir.indices = &enemy_indices;
	cir.spatial_team = enemy_team;
	_for_each_unit_in_circle(cir, [&](UnitState &unit) {
		_apply_slow(source, unit, slow_percentage, duration);
	});
}

void TeamfightSimulationCore::_apply_target_aoe_root(UnitState &source, UnitState &target, double radius, double duration) {
	if (radius <= 0.0 || duration <= 0.0) {
		return;
	}
	_viewer_record_aoe_ring_fx(source, target, radius, StringName("aoe_root"));
	StringName enemy_team = source.team == sn_player() ? sn_enemy() : sn_player();
	const std::vector<int64_t> &enemy_indices = _alive_indices_for_team(enemy_team);
	AoCircleIterationParams cir;
	cir.center_x = target.pos_x;
	cir.center_y = target.pos_y;
	cir.radius = radius;
	cir.indices = &enemy_indices;
	cir.spatial_team = enemy_team;
	_for_each_unit_in_circle(cir, [&](UnitState &unit) {
		_apply_root(source, unit, duration);
	});
}

void TeamfightSimulationCore::_apply_target_aoe_silence(UnitState &source, UnitState &target, double radius, double duration, bool blocks_abilities, bool blocks_ultimates) {
	if (radius <= 0.0 || duration <= 0.0) {
		return;
	}
	_viewer_record_aoe_ring_fx(source, target, radius, StringName("aoe_silence"));
	StringName enemy_team = source.team == sn_player() ? sn_enemy() : sn_player();
	const std::vector<int64_t> &enemy_indices = _alive_indices_for_team(enemy_team);
	AoCircleIterationParams cir;
	cir.center_x = target.pos_x;
	cir.center_y = target.pos_y;
	cir.radius = radius;
	cir.indices = &enemy_indices;
	cir.spatial_team = enemy_team;
	_for_each_unit_in_circle(cir, [&](UnitState &unit) {
		_apply_silence(source, unit, duration, blocks_abilities, blocks_ultimates);
	});
}

void TeamfightSimulationCore::_apply_target_aoe_disarm(UnitState &source, UnitState &target, double radius, double duration) {
	if (radius <= 0.0 || duration <= 0.0) {
		return;
	}
	_viewer_record_aoe_ring_fx(source, target, radius, StringName("aoe_disarm"));
	StringName enemy_team = source.team == sn_player() ? sn_enemy() : sn_player();
	const std::vector<int64_t> &enemy_indices = _alive_indices_for_team(enemy_team);
	AoCircleIterationParams cir;
	cir.center_x = target.pos_x;
	cir.center_y = target.pos_y;
	cir.radius = radius;
	cir.indices = &enemy_indices;
	cir.spatial_team = enemy_team;
	_for_each_unit_in_circle(cir, [&](UnitState &unit) {
		_apply_disarm(source, unit, duration);
	});
}

void TeamfightSimulationCore::_apply_reflect_buff(UnitState &unit, double pct, double duration, bool all_damage_types) {
	if (duration <= 0.0 || pct <= 0.0) {
		return;
	}
	double p = Math::clamp(pct, 0.0, 1.0);
	unit.reflect_buff_remaining = Math::max(unit.reflect_buff_remaining, duration);
	if (all_damage_types) {
		unit.reflect_buff_pct_all = Math::clamp(unit.reflect_buff_pct_all + p, 0.0, 1.0);
	} else {
		unit.reflect_buff_pct_physical = Math::clamp(unit.reflect_buff_pct_physical + p, 0.0, 1.0);
	}
}

void TeamfightSimulationCore::_apply_self_aoe_reflect(UnitState &source, double radius, double pct, double duration, bool all_damage_types) {
	if (radius <= 0.0 || duration <= 0.0 || pct <= 0.0) {
		return;
	}
	_viewer_record_aoe_ring_fx(source, source, radius, StringName("aoe_reflect"));
	const std::vector<int64_t> &ally_indices = _alive_indices_for_team(source.team);
	AoCircleIterationParams cir;
	cir.center_x = source.pos_x;
	cir.center_y = source.pos_y;
	cir.radius = radius;
	cir.indices = &ally_indices;
	cir.spatial_team = source.team;
	_for_each_unit_in_circle(cir, [&](UnitState &ally) {
		_apply_reflect_buff(ally, pct, duration, all_damage_types);
	});
}

void TeamfightSimulationCore::_apply_knockback(UnitState &source, UnitState &target, double distance, bool away_from_source) {
	if (distance <= 0.0 || !target.alive) {
		return;
	}
	double tenacity = target.combat.tenacity;
	double effective_distance = distance * (1.0 - tenacity);
	if (effective_distance <= EPSILON) {
		return;
	}
	double sx = source.pos_x;
	double sy = source.pos_y;
	double tx = target.pos_x;
	double ty = target.pos_y;
	double dx = tx - sx;
	double dy = ty - sy;
	double dist = Math::sqrt(dx * dx + dy * dy);
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
}

void TeamfightSimulationCore::_apply_self_aoe_knockback(UnitState &source, double radius, double distance, bool away_from_source) {
	if (radius <= 0.0 || distance <= 0.0) {
		return;
	}
	_viewer_record_aoe_ring_fx(source, source, radius, StringName("aoe_knockback"));
	StringName enemy_team = source.team == sn_player() ? sn_enemy() : sn_player();
	const std::vector<int64_t> &enemy_indices = _alive_indices_for_team(enemy_team);
	AoCircleIterationParams cir;
	cir.center_x = source.pos_x;
	cir.center_y = source.pos_y;
	cir.radius = radius;
	cir.indices = &enemy_indices;
	cir.spatial_team = enemy_team;
	_for_each_unit_in_circle(cir, [&](UnitState &unit) {
		_apply_knockback(source, unit, distance, away_from_source);
	});
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
	
	double max_hp = target.combat.max_hp;
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
	_uc(source).healing_done += amount;
	if (action_kind == sn_auto()) {
		_uc(source).healing_done_auto += amount;
	} else if (action_kind == sn_ability()) {
		_uc(source).healing_done_ability += amount;
	} else if (action_kind == sn_ultimate()) {
		_uc(source).healing_done_ultimate += amount;
	} else if (action_kind == sn_passive()) {
		_uc(source).healing_done_passive += amount;
	}
	if (source.instance_id != target.instance_id) {
		_uc(target).recent_benefactors[source.instance_id] = _time;
	}
}

void TeamfightSimulationCore::_restore_mana(UnitState &source, UnitState &target, double amount) {
	if (amount <= 0.0) {
		return;
	}
	double max_mana = target.combat.max_mana;
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
	
	// Apply modifiers based on stat name
	if (stat_name == StringName("max_hp")) {
		target.stat_additive_max_hp += additive;
		target.stat_multiplicative_max_hp *= multiplicative;
		if (duration > 0.0) {
			if (is_match_duration) {
				target.stat_perm_max_hp = Math::max(target.stat_perm_max_hp, duration);
			} else {
				target.stat_temp_max_hp = Math::max(target.stat_temp_max_hp, duration);
			}
		}
	} else if (stat_name == StringName("attack_damage")) {
		target.stat_additive_attack_damage += additive;
		target.stat_multiplicative_attack_damage *= multiplicative;
		if (duration > 0.0) {
			if (is_match_duration) {
				target.stat_perm_attack_damage = Math::max(target.stat_perm_attack_damage, duration);
			} else {
				target.stat_temp_attack_damage = Math::max(target.stat_temp_attack_damage, duration);
			}
		}
	} else if (stat_name == StringName("attack_speed")) {
		target.stat_additive_attack_speed += additive;
		target.stat_multiplicative_attack_speed *= multiplicative;
		if (duration > 0.0) {
			if (is_match_duration) {
				target.stat_perm_attack_speed = Math::max(target.stat_perm_attack_speed, duration);
			} else {
				target.stat_temp_attack_speed = Math::max(target.stat_temp_attack_speed, duration);
			}
		}
	} else if (stat_name == StringName("move_speed")) {
		target.stat_additive_move_speed += additive;
		target.stat_multiplicative_move_speed *= multiplicative;
		if (duration > 0.0) {
			if (is_match_duration) {
				target.stat_perm_move_speed = Math::max(target.stat_perm_move_speed, duration);
			} else {
				target.stat_temp_move_speed = Math::max(target.stat_temp_move_speed, duration);
			}
		}
	} else if (stat_name == StringName("armor")) {
		target.stat_additive_armor += additive;
		target.stat_multiplicative_armor *= multiplicative;
		if (duration > 0.0) {
			if (is_match_duration) {
				target.stat_perm_armor = Math::max(target.stat_perm_armor, duration);
			} else {
				target.stat_temp_armor = Math::max(target.stat_temp_armor, duration);
			}
		}
	} else if (stat_name == StringName("magic_resist")) {
		target.stat_additive_magic_resist += additive;
		target.stat_multiplicative_magic_resist *= multiplicative;
		if (duration > 0.0) {
			if (is_match_duration) {
				target.stat_perm_magic_resist = Math::max(target.stat_perm_magic_resist, duration);
			} else {
				target.stat_temp_magic_resist = Math::max(target.stat_temp_magic_resist, duration);
			}
		}
	} else if (stat_name == StringName("tenacity")) {
		target.stat_additive_tenacity += additive;
		target.stat_multiplicative_tenacity *= multiplicative;
		if (duration > 0.0) {
			if (is_match_duration) {
				target.stat_perm_tenacity = Math::max(target.stat_perm_tenacity, duration);
			} else {
				target.stat_temp_tenacity = Math::max(target.stat_temp_tenacity, duration);
			}
		}
	} else if (stat_name == StringName("life_steal")) {
		target.stat_additive_life_steal += additive;
		target.stat_multiplicative_life_steal *= multiplicative;
		if (duration > 0.0) {
			if (is_match_duration) {
				target.stat_perm_life_steal = Math::max(target.stat_perm_life_steal, duration);
			} else {
				target.stat_temp_life_steal = Math::max(target.stat_temp_life_steal, duration);
			}
		}
	} else if (stat_name == StringName("max_mana")) {
		target.stat_additive_max_mana += additive;
		target.stat_multiplicative_max_mana *= multiplicative;
		if (duration > 0.0) {
			if (is_match_duration) {
				target.stat_perm_max_mana = Math::max(target.stat_perm_max_mana, duration);
			} else {
				target.stat_temp_max_mana = Math::max(target.stat_temp_max_mana, duration);
			}
		}
	} else if (stat_name == StringName("mana_per_attack")) {
		target.stat_additive_mana_per_attack += additive;
		target.stat_multiplicative_mana_per_attack *= multiplicative;
		if (duration > 0.0) {
			if (is_match_duration) {
				target.stat_perm_mana_per_attack = Math::max(target.stat_perm_mana_per_attack, duration);
			} else {
				target.stat_temp_mana_per_attack = Math::max(target.stat_temp_mana_per_attack, duration);
			}
		}
	} else if (stat_name == StringName("ability_cd")) {
		target.stat_additive_ability_cd += additive;
		target.stat_multiplicative_ability_cd *= multiplicative;
		if (duration > 0.0) {
			if (is_match_duration) {
				target.stat_perm_ability_cd = Math::max(target.stat_perm_ability_cd, duration);
			} else {
				target.stat_temp_ability_cd = Math::max(target.stat_temp_ability_cd, duration);
			}
		}
	} else if (stat_name == StringName("projectile_speed")) {
		target.stat_additive_projectile_speed += additive;
		target.stat_multiplicative_projectile_speed *= multiplicative;
		if (duration > 0.0) {
			if (is_match_duration) {
				target.stat_perm_projectile_speed = Math::max(target.stat_perm_projectile_speed, duration);
			} else {
				target.stat_temp_projectile_speed = Math::max(target.stat_temp_projectile_speed, duration);
			}
		}
	} else if (stat_name == StringName("projectile_radius")) {
		target.stat_additive_projectile_radius += additive;
		target.stat_multiplicative_projectile_radius *= multiplicative;
		if (duration > 0.0) {
			if (is_match_duration) {
				target.stat_perm_projectile_radius = Math::max(target.stat_perm_projectile_radius, duration);
			} else {
				target.stat_temp_projectile_radius = Math::max(target.stat_temp_projectile_radius, duration);
			}
		}
	} else if (stat_name == StringName("respawn_time")) {
		target.stat_additive_respawn_time += additive;
		target.stat_multiplicative_respawn_time *= multiplicative;
		if (duration > 0.0) {
			if (is_match_duration) {
				target.stat_perm_respawn_time = Math::max(target.stat_perm_respawn_time, duration);
			} else {
				target.stat_temp_respawn_time = Math::max(target.stat_temp_respawn_time, duration);
			}
		}
	} else if (stat_name == StringName("attack_range")) {
		target.stat_additive_attack_range += additive;
		target.stat_multiplicative_attack_range *= multiplicative;
		if (duration > 0.0) {
			if (is_match_duration) {
				target.stat_perm_attack_range = Math::max(target.stat_perm_attack_range, duration);
			} else {
				target.stat_temp_attack_range = Math::max(target.stat_temp_attack_range, duration);
			}
		}
	} else if (stat_name == StringName("cast_range")) {
		target.stat_additive_cast_range += additive;
		target.stat_multiplicative_cast_range *= multiplicative;
		if (duration > 0.0) {
			if (is_match_duration) {
				target.stat_perm_cast_range = Math::max(target.stat_perm_cast_range, duration);
			} else {
				target.stat_temp_cast_range = Math::max(target.stat_temp_cast_range, duration);
			}
		}
	}
}

void TeamfightSimulationCore::_set_stat_modifier_duration(UnitState &unit, StringName stat_name, double duration, bool is_match_duration) {
	if (duration < 0.0) {
		duration = 0.0;
	}
	if (stat_name == StringName("max_hp")) {
		if (is_match_duration) {
			unit.stat_perm_max_hp = Math::max(unit.stat_perm_max_hp, duration);
		} else {
			unit.stat_temp_max_hp = Math::max(unit.stat_temp_max_hp, duration);
		}
	} else if (stat_name == StringName("attack_damage")) {
		if (is_match_duration) {
			unit.stat_perm_attack_damage = Math::max(unit.stat_perm_attack_damage, duration);
		} else {
			unit.stat_temp_attack_damage = Math::max(unit.stat_temp_attack_damage, duration);
		}
	} else if (stat_name == StringName("attack_speed")) {
		if (is_match_duration) {
			unit.stat_perm_attack_speed = Math::max(unit.stat_perm_attack_speed, duration);
		} else {
			unit.stat_temp_attack_speed = Math::max(unit.stat_temp_attack_speed, duration);
		}
	} else if (stat_name == StringName("move_speed")) {
		if (is_match_duration) {
			unit.stat_perm_move_speed = Math::max(unit.stat_perm_move_speed, duration);
		} else {
			unit.stat_temp_move_speed = Math::max(unit.stat_temp_move_speed, duration);
		}
	} else if (stat_name == StringName("armor")) {
		if (is_match_duration) {
			unit.stat_perm_armor = Math::max(unit.stat_perm_armor, duration);
		} else {
			unit.stat_temp_armor = Math::max(unit.stat_temp_armor, duration);
		}
	} else if (stat_name == StringName("magic_resist")) {
		if (is_match_duration) {
			unit.stat_perm_magic_resist = Math::max(unit.stat_perm_magic_resist, duration);
		} else {
			unit.stat_temp_magic_resist = Math::max(unit.stat_temp_magic_resist, duration);
		}
	} else if (stat_name == StringName("tenacity")) {
		if (is_match_duration) {
			unit.stat_perm_tenacity = Math::max(unit.stat_perm_tenacity, duration);
		} else {
			unit.stat_temp_tenacity = Math::max(unit.stat_temp_tenacity, duration);
		}
	} else if (stat_name == StringName("life_steal")) {
		if (is_match_duration) {
			unit.stat_perm_life_steal = Math::max(unit.stat_perm_life_steal, duration);
		} else {
			unit.stat_temp_life_steal = Math::max(unit.stat_temp_life_steal, duration);
		}
	} else if (stat_name == StringName("max_mana")) {
		if (is_match_duration) {
			unit.stat_perm_max_mana = Math::max(unit.stat_perm_max_mana, duration);
		} else {
			unit.stat_temp_max_mana = Math::max(unit.stat_temp_max_mana, duration);
		}
	} else if (stat_name == StringName("mana_per_attack")) {
		if (is_match_duration) {
			unit.stat_perm_mana_per_attack = Math::max(unit.stat_perm_mana_per_attack, duration);
		} else {
			unit.stat_temp_mana_per_attack = Math::max(unit.stat_temp_mana_per_attack, duration);
		}
	} else if (stat_name == StringName("ability_cd")) {
		if (is_match_duration) {
			unit.stat_perm_ability_cd = Math::max(unit.stat_perm_ability_cd, duration);
		} else {
			unit.stat_temp_ability_cd = Math::max(unit.stat_temp_ability_cd, duration);
		}
	} else if (stat_name == StringName("projectile_speed")) {
		if (is_match_duration) {
			unit.stat_perm_projectile_speed = Math::max(unit.stat_perm_projectile_speed, duration);
		} else {
			unit.stat_temp_projectile_speed = Math::max(unit.stat_temp_projectile_speed, duration);
		}
	} else if (stat_name == StringName("projectile_radius")) {
		if (is_match_duration) {
			unit.stat_perm_projectile_radius = Math::max(unit.stat_perm_projectile_radius, duration);
		} else {
			unit.stat_temp_projectile_radius = Math::max(unit.stat_temp_projectile_radius, duration);
		}
	} else if (stat_name == StringName("respawn_time")) {
		if (is_match_duration) {
			unit.stat_perm_respawn_time = Math::max(unit.stat_perm_respawn_time, duration);
		} else {
			unit.stat_temp_respawn_time = Math::max(unit.stat_temp_respawn_time, duration);
		}
	} else if (stat_name == StringName("attack_range")) {
		if (is_match_duration) {
			unit.stat_perm_attack_range = Math::max(unit.stat_perm_attack_range, duration);
		} else {
			unit.stat_temp_attack_range = Math::max(unit.stat_temp_attack_range, duration);
		}
	} else if (stat_name == StringName("cast_range")) {
		if (is_match_duration) {
			unit.stat_perm_cast_range = Math::max(unit.stat_perm_cast_range, duration);
		} else {
			unit.stat_temp_cast_range = Math::max(unit.stat_temp_cast_range, duration);
		}
	}
}

void TeamfightSimulationCore::_clear_all_stat_modifiers(UnitState &unit) {
	// Clear all stat modifier fields to defaults
	unit.stat_additive_max_hp = 0.0;
	unit.stat_multiplicative_max_hp = 1.0;
	unit.stat_temp_max_hp = 0.0;
	unit.stat_perm_max_hp = 0.0;
	
	unit.stat_additive_attack_damage = 0.0;
	unit.stat_multiplicative_attack_damage = 1.0;
	unit.stat_temp_attack_damage = 0.0;
	unit.stat_perm_attack_damage = 0.0;
	
	unit.stat_additive_attack_speed = 0.0;
	unit.stat_multiplicative_attack_speed = 1.0;
	unit.stat_temp_attack_speed = 0.0;
	unit.stat_perm_attack_speed = 0.0;
	
	unit.stat_additive_move_speed = 0.0;
	unit.stat_multiplicative_move_speed = 1.0;
	unit.stat_temp_move_speed = 0.0;
	unit.stat_perm_move_speed = 0.0;
	
	unit.stat_additive_armor = 0.0;
	unit.stat_multiplicative_armor = 1.0;
	unit.stat_temp_armor = 0.0;
	unit.stat_perm_armor = 0.0;
	
	unit.stat_additive_magic_resist = 0.0;
	unit.stat_multiplicative_magic_resist = 1.0;
	unit.stat_temp_magic_resist = 0.0;
	unit.stat_perm_magic_resist = 0.0;
	
	unit.stat_additive_tenacity = 0.0;
	unit.stat_multiplicative_tenacity = 1.0;
	unit.stat_temp_tenacity = 0.0;
	unit.stat_perm_tenacity = 0.0;
	
	unit.stat_additive_life_steal = 0.0;
	unit.stat_multiplicative_life_steal = 1.0;
	unit.stat_temp_life_steal = 0.0;
	unit.stat_perm_life_steal = 0.0;
	
	unit.stat_additive_max_mana = 0.0;
	unit.stat_multiplicative_max_mana = 1.0;
	unit.stat_temp_max_mana = 0.0;
	unit.stat_perm_max_mana = 0.0;
	
	unit.stat_additive_mana_per_attack = 0.0;
	unit.stat_multiplicative_mana_per_attack = 1.0;
	unit.stat_temp_mana_per_attack = 0.0;
	unit.stat_perm_mana_per_attack = 0.0;
	
	unit.stat_additive_ability_cd = 0.0;
	unit.stat_multiplicative_ability_cd = 1.0;
	unit.stat_temp_ability_cd = 0.0;
	unit.stat_perm_ability_cd = 0.0;
	
	unit.stat_additive_projectile_speed = 0.0;
	unit.stat_multiplicative_projectile_speed = 1.0;
	unit.stat_temp_projectile_speed = 0.0;
	unit.stat_perm_projectile_speed = 0.0;
	
	unit.stat_additive_projectile_radius = 0.0;
	unit.stat_multiplicative_projectile_radius = 1.0;
	unit.stat_temp_projectile_radius = 0.0;
	unit.stat_perm_projectile_radius = 0.0;
	
	unit.stat_additive_respawn_time = 0.0;
	unit.stat_multiplicative_respawn_time = 1.0;
	unit.stat_temp_respawn_time = 0.0;
	unit.stat_perm_respawn_time = 0.0;
	
	unit.stat_additive_attack_range = 0.0;
	unit.stat_multiplicative_attack_range = 1.0;
	unit.stat_temp_attack_range = 0.0;
	unit.stat_perm_attack_range = 0.0;
	
	unit.stat_additive_cast_range = 0.0;
	unit.stat_multiplicative_cast_range = 1.0;
	unit.stat_temp_cast_range = 0.0;
	unit.stat_perm_cast_range = 0.0;
	
	// Clear stack tracking
	unit.stat_stacks.clear();
}

bool TeamfightSimulationCore::_is_valid_stat_name(const StringName &stat_name) const {
	// List of all valid stat names
	return stat_name == StringName("max_hp") ||
		   stat_name == StringName("attack_damage") ||
		   stat_name == StringName("attack_speed") ||
		   stat_name == StringName("move_speed") ||
		   stat_name == StringName("armor") ||
		   stat_name == StringName("magic_resist") ||
		   stat_name == StringName("tenacity") ||
		   stat_name == StringName("life_steal") ||
		   stat_name == StringName("max_mana") ||
		   stat_name == StringName("mana_per_attack") ||
		   stat_name == StringName("ability_cd") ||
		   stat_name == StringName("projectile_speed") ||
		   stat_name == StringName("projectile_radius") ||
		   stat_name == StringName("respawn_time") ||
		   stat_name == StringName("attack_range") ||
		   stat_name == StringName("cast_range");
}

void TeamfightSimulationCore::_update_stat_modifier_durations(UnitState &unit, double delta) {
	// Update temporary modifier durations
	unit.stat_temp_max_hp = Math::max(0.0, unit.stat_temp_max_hp - delta);
	unit.stat_temp_attack_damage = Math::max(0.0, unit.stat_temp_attack_damage - delta);
	unit.stat_temp_attack_speed = Math::max(0.0, unit.stat_temp_attack_speed - delta);
	unit.stat_temp_move_speed = Math::max(0.0, unit.stat_temp_move_speed - delta);
	unit.stat_temp_armor = Math::max(0.0, unit.stat_temp_armor - delta);
	unit.stat_temp_magic_resist = Math::max(0.0, unit.stat_temp_magic_resist - delta);
	unit.stat_temp_tenacity = Math::max(0.0, unit.stat_temp_tenacity - delta);
	unit.stat_temp_life_steal = Math::max(0.0, unit.stat_temp_life_steal - delta);
	unit.stat_temp_max_mana = Math::max(0.0, unit.stat_temp_max_mana - delta);
	unit.stat_temp_mana_per_attack = Math::max(0.0, unit.stat_temp_mana_per_attack - delta);
	unit.stat_temp_ability_cd = Math::max(0.0, unit.stat_temp_ability_cd - delta);
	unit.stat_temp_projectile_speed = Math::max(0.0, unit.stat_temp_projectile_speed - delta);
	unit.stat_temp_projectile_radius = Math::max(0.0, unit.stat_temp_projectile_radius - delta);
	unit.stat_temp_respawn_time = Math::max(0.0, unit.stat_temp_respawn_time - delta);
	unit.stat_temp_attack_range = Math::max(0.0, unit.stat_temp_attack_range - delta);
	unit.stat_temp_cast_range = Math::max(0.0, unit.stat_temp_cast_range - delta);
}

void TeamfightSimulationCore::_clear_expired_stat_modifiers(UnitState &unit) {
	// Clear expired temporary modifiers
	if (unit.stat_temp_max_hp <= 0.0) {
		unit.stat_additive_max_hp = 0.0;
		unit.stat_multiplicative_max_hp = 1.0;
	}
	if (unit.stat_temp_attack_damage <= 0.0) {
		unit.stat_additive_attack_damage = 0.0;
		unit.stat_multiplicative_attack_damage = 1.0;
	}
	if (unit.stat_temp_attack_speed <= 0.0) {
		unit.stat_additive_attack_speed = 0.0;
		unit.stat_multiplicative_attack_speed = 1.0;
	}
	if (unit.stat_temp_move_speed <= 0.0) {
		unit.stat_additive_move_speed = 0.0;
		unit.stat_multiplicative_move_speed = 1.0;
	}
	if (unit.stat_temp_armor <= 0.0) {
		unit.stat_additive_armor = 0.0;
		unit.stat_multiplicative_armor = 1.0;
	}
	if (unit.stat_temp_magic_resist <= 0.0) {
		unit.stat_additive_magic_resist = 0.0;
		unit.stat_multiplicative_magic_resist = 1.0;
	}
	if (unit.stat_temp_tenacity <= 0.0) {
		unit.stat_additive_tenacity = 0.0;
		unit.stat_multiplicative_tenacity = 1.0;
	}
	if (unit.stat_temp_life_steal <= 0.0) {
		unit.stat_additive_life_steal = 0.0;
		unit.stat_multiplicative_life_steal = 1.0;
	}
	if (unit.stat_temp_max_mana <= 0.0) {
		unit.stat_additive_max_mana = 0.0;
		unit.stat_multiplicative_max_mana = 1.0;
	}
	if (unit.stat_temp_mana_per_attack <= 0.0) {
		unit.stat_additive_mana_per_attack = 0.0;
		unit.stat_multiplicative_mana_per_attack = 1.0;
	}
	if (unit.stat_temp_ability_cd <= 0.0) {
		unit.stat_additive_ability_cd = 0.0;
		unit.stat_multiplicative_ability_cd = 1.0;
	}
	if (unit.stat_temp_projectile_speed <= 0.0) {
		unit.stat_additive_projectile_speed = 0.0;
		unit.stat_multiplicative_projectile_speed = 1.0;
	}
	if (unit.stat_temp_projectile_radius <= 0.0) {
		unit.stat_additive_projectile_radius = 0.0;
		unit.stat_multiplicative_projectile_radius = 1.0;
	}
	if (unit.stat_temp_respawn_time <= 0.0) {
		unit.stat_additive_respawn_time = 0.0;
		unit.stat_multiplicative_respawn_time = 1.0;
	}
	if (unit.stat_temp_attack_range <= 0.0) {
		unit.stat_additive_attack_range = 0.0;
		unit.stat_multiplicative_attack_range = 1.0;
	}
	if (unit.stat_temp_cast_range <= 0.0) {
		unit.stat_additive_cast_range = 0.0;
		unit.stat_multiplicative_cast_range = 1.0;
	}
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
	_debug_log_stack_operation("APPLY", String(stat_name), current_stacks, max_stacks, next_duration, reason);
}

// Stack management functions
String TeamfightSimulationCore::_get_stack_key(StringName stat_name, const String &reason) {
	return String(stat_name) + "|" + reason;
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
			_debug_log_stack_operation("EXPIRE", String(stat_name), current_stacks, int(stack_dict.get("max_stacks", 1)), duration, String(key));
		}
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

void TeamfightSimulationCore::_apply_target_aoe_damage(UnitState &source, UnitState &target, double damage, double radius, const StringName &damage_type, const StringName &action_kind, const String &reason, double splash_ratio) {
	(void)reason;
	if (radius <= 0.0) {
		return;
	}
	_viewer_record_aoe_ring_fx(source, target, radius, StringName("aoe_splash"));
	StringName source_team = source.team;
	StringName enemy_team = source_team == StringName("player") ? StringName("enemy") : StringName("player");
	const std::vector<int64_t> &enemy_indices = _alive_indices_for_team(enemy_team);
	AoCircleIterationParams cir;
	cir.center_x = target.pos_x;
	cir.center_y = target.pos_y;
	cir.radius = radius;
	cir.indices = &enemy_indices;
	cir.spatial_team = enemy_team;
	_for_each_unit_in_circle(cir, [&](UnitState &unit) {
		double splash_damage = damage * splash_ratio;
		EffectContext context = _build_context(source, &unit, nullptr, splash_damage, action_kind);
		_apply_damage(source, unit, splash_damage, damage_type, action_kind, context);
	});
}

void TeamfightSimulationCore::_apply_dot(UnitState &source, UnitState &target, double attack_damage_ratio, double max_hp_ratio, double flat_amount, double duration, double tick_interval, const StringName &damage_type, const StringName &stacking_mode, int max_stacks, const StringName &effect_type, const StringName &action_kind) {
	// Calculate damage_per_tick from ratios at application time
	double damage_per_tick = source.combat.attack_damage * attack_damage_ratio;
	damage_per_tick += target.combat.max_hp * max_hp_ratio;
	damage_per_tick += flat_amount;
	
	if (duration <= 0.0 || damage_per_tick <= 0.0) {
		return;
	}
	
	auto &periodic_effects = _uc(target).periodic_effects;
	for (auto &existing : periodic_effects) {
		if (existing.effect_type == effect_type && existing.source_instance_id == source.instance_id) {
			// Apply stacking logic
			if (stacking_mode == StringName("refresh")) {
				existing.remaining_duration = duration;
				return;
			} else if (stacking_mode == StringName("extend")) {
				existing.remaining_duration += duration;
				return;
			} else if (stacking_mode == StringName("stack_damage")) {
				if (existing.stack_count < max_stacks) {
					existing.damage_per_tick += damage_per_tick;
					existing.stack_count++;
					existing.remaining_duration = duration;
				} else {
					existing.remaining_duration = duration;
				}
				return;
			} else if (stacking_mode == StringName("stack_duration")) {
				if (existing.stack_count < max_stacks) {
					existing.remaining_duration += duration;
					existing.stack_count++;
				} else {
					existing.remaining_duration = duration;
				}
				return;
			}
			// "separate" mode: allow multiple independent effects
		}
	}
	
	UnitStateCold::PeriodicEffect new_effect;
	new_effect.effect_type = effect_type;
	new_effect.damage_per_tick = damage_per_tick;
	new_effect.heal_per_tick = 0.0;
	new_effect.remaining_duration = duration;
	new_effect.tick_interval = tick_interval;
	new_effect.tick_accumulator = 0.0;
	new_effect.source_instance_id = source.instance_id;
	new_effect.damage_type = damage_type;
	new_effect.stacking_mode = stacking_mode;
	new_effect.allow_overheal = false;
	new_effect.stack_count = 1;
	new_effect.max_stacks = max_stacks;
	new_effect.action_kind = action_kind;
	periodic_effects.push_back(new_effect);
}

void TeamfightSimulationCore::_apply_hot(UnitState &source, UnitState &target, double max_hp_ratio, double current_hp_ratio, double missing_hp_ratio, double flat_amount, double duration, double tick_interval, const StringName &stacking_mode, int max_stacks, bool allow_overheal, const StringName &effect_type, const StringName &action_kind) {
	// Calculate heal_per_tick from ratios at application time
	double heal_per_tick = target.combat.max_hp * max_hp_ratio;
	heal_per_tick += target.hp * current_hp_ratio;
	heal_per_tick += (target.combat.max_hp - target.hp) * missing_hp_ratio;
	heal_per_tick += flat_amount;
	
	if (duration <= 0.0 || heal_per_tick <= 0.0) {
		return;
	}
	
	// Visual effect: green progress border
	_viewer_record_hot_status_fx(target, duration, effect_type);
	
	UnitStateCold::PeriodicEffect new_effect;
	new_effect.effect_type = effect_type;
	new_effect.damage_per_tick = 0.0;
	new_effect.heal_per_tick = heal_per_tick;
	new_effect.remaining_duration = duration;
	new_effect.tick_interval = tick_interval;
	new_effect.tick_accumulator = 0.0;
	new_effect.source_instance_id = source.instance_id;
	new_effect.damage_type = StringName();
	new_effect.stacking_mode = stacking_mode;
	new_effect.allow_overheal = allow_overheal;
	new_effect.stack_count = 1;
	new_effect.max_stacks = max_stacks;
	new_effect.action_kind = action_kind;
	
	// Search for existing effect with same type and source
	auto &periodic_effects = _uc(target).periodic_effects;
	for (auto &existing : periodic_effects) {
		if (existing.effect_type == effect_type && existing.source_instance_id == source.instance_id) {
			// Apply stacking logic
			if (stacking_mode == StringName("refresh")) {
				existing.remaining_duration = duration;
				return;
			} else if (stacking_mode == StringName("extend")) {
				existing.remaining_duration += duration;
				return;
			} else if (stacking_mode == StringName("stack_damage")) {
				if (existing.stack_count < max_stacks) {
					existing.heal_per_tick += heal_per_tick;
					existing.stack_count++;
					existing.remaining_duration = duration;
				} else {
					existing.remaining_duration = duration;
				}
				return;
			} else if (stacking_mode == StringName("stack_duration")) {
				existing.remaining_duration += duration;
				return;
			}
			// "separate" mode: do not modify existing, fall through to add new instance
		}
	}
	
	// No matching effect found or "separate" mode - add new
	periodic_effects.push_back(new_effect);
}

// DoT/HoT Limitations:
// - Damage multipliers: DoT damage is applied directly without checking damage multiplier effects
//   (constant_multiplier, hp_threshold_damage_multiplier, target_status_multiplier). This is
//   intentional for simplicity and performance. DoTs represent fixed damage over time calculated
//   at application time.
// - Healing multipliers: HoT healing is applied directly without checking healing multiplier effects.
//   This is intentional for the same reasons.
// - Source behavior: When the source unit dies, DoT/HoT continues ticking on the target. The source
//   is only used for damage/healing attribution (stats tracking). If the source is dead, damage/healing
//   still occurs but is attributed to the now-dead source.
// - Visual effects: No visual effect hooks are currently implemented. DoT/HoT ticks do not trigger
//   damage numbers or particle effects. This will be added in a future update.
void TeamfightSimulationCore::_tick_periodic_effects(UnitState &unit, double delta) {
	auto &periodic_effects = _uc(unit).periodic_effects;
	auto it = periodic_effects.begin();
	while (it != periodic_effects.end()) {
		auto &effect = *it;
		
		// Update accumulator
		effect.tick_accumulator += delta;
		
		// Check if tick should occur
		if (effect.tick_accumulator >= effect.tick_interval) {
			effect.tick_accumulator -= effect.tick_interval;
			
			if (effect.damage_per_tick > 0.0) {
				// Apply DoT damage
				UnitState *source = _unit_by_id(effect.source_instance_id);
				if (source != nullptr) {
					EffectContext context = _build_context(*source, &unit, nullptr, effect.damage_per_tick, effect.action_kind);
					_apply_damage(*source, unit, effect.damage_per_tick, effect.damage_type, effect.action_kind, context);
				}
			}
			
			if (effect.heal_per_tick > 0.0) {
				// Apply HoT heal
				UnitState *source = _unit_by_id(effect.source_instance_id);
				if (source != nullptr) {
					EffectContext context = _build_context(*source, &unit, nullptr, 0.0, effect.action_kind);
					_heal_unit(*source, unit, effect.heal_per_tick, effect.action_kind, effect.allow_overheal);
				}
			}
		}
		
		// Update remaining duration
		effect.remaining_duration -= delta;
		
		// Remove expired effects
		if (effect.remaining_duration <= 0.0) {
			it = periodic_effects.erase(it);
		} else {
			++it;
		}
	}
}

void TeamfightSimulationCore::_cleanse_dots(UnitState &unit, const StringName &effect_type_filter) {
	auto &periodic_effects = _uc(unit).periodic_effects;
	if (effect_type_filter.is_empty()) {
		// Remove all DoTs (damage_per_tick > 0)
		periodic_effects.erase(
			std::remove_if(periodic_effects.begin(), periodic_effects.end(),
				[](const UnitStateCold::PeriodicEffect &e) { return e.damage_per_tick > 0.0; }),
			periodic_effects.end()
		);
	} else {
		// Remove DoTs matching effect type
		periodic_effects.erase(
			std::remove_if(periodic_effects.begin(), periodic_effects.end(),
				[&effect_type_filter](const UnitStateCold::PeriodicEffect &e) {
					return e.damage_per_tick > 0.0 && e.effect_type == effect_type_filter;
				}),
			periodic_effects.end()
		);
	}
}

void TeamfightSimulationCore::_clear_periodic_effects(UnitState &unit) {
	_uc(unit).periodic_effects.clear();
}

void TeamfightSimulationCore::_apply_aoe_dot(UnitState &source, double radius, double attack_damage_ratio, double max_hp_ratio, double flat_amount, double duration, double tick_interval, const StringName &damage_type, const StringName &stacking_mode, int max_stacks, const StringName &effect_type, bool target_self, const StringName &action_kind) {
	StringName source_team = source.team;
	StringName enemy_team = source_team == StringName("player") ? StringName("enemy") : StringName("player");
	const std::vector<int64_t> &enemy_indices = _alive_indices_for_team(enemy_team);
	AoCircleIterationParams cir;
	cir.center_x = source.pos_x;
	cir.center_y = source.pos_y;
	cir.radius = radius;
	cir.indices = &enemy_indices;
	cir.spatial_team = enemy_team;
	if (!target_self) {
		cir.exclude_instance_id = source.instance_id;
	}
	_for_each_unit_in_circle(cir, [&](UnitState &unit) {
		_apply_dot(source, unit, attack_damage_ratio, max_hp_ratio, flat_amount, duration, tick_interval, damage_type, stacking_mode, max_stacks, effect_type, action_kind);
	});
}

void TeamfightSimulationCore::_apply_aoe_hot(UnitState &source, double radius, double max_hp_ratio, double current_hp_ratio, double missing_hp_ratio, double flat_amount, double duration, double tick_interval, const StringName &stacking_mode, int max_stacks, bool allow_overheal, const StringName &effect_type, bool target_self, const StringName &action_kind) {
	// Visual effect: AoE ring
	_viewer_record_aoe_ring_fx(source, source, radius, StringName("aoe_hot"));
	
	StringName source_team = source.team;
	StringName ally_team = source_team == StringName("player") ? StringName("player") : StringName("enemy");
	const std::vector<int64_t> &ally_indices = _alive_indices_for_team(ally_team);
	AoCircleIterationParams cir;
	cir.center_x = source.pos_x;
	cir.center_y = source.pos_y;
	cir.radius = radius;
	cir.indices = &ally_indices;
	cir.spatial_team = ally_team;
	if (!target_self) {
		cir.exclude_instance_id = source.instance_id;
	}
	_for_each_unit_in_circle(cir, [&](UnitState &unit) {
		_apply_hot(source, unit, max_hp_ratio, current_hp_ratio, missing_hp_ratio, flat_amount, duration, tick_interval, stacking_mode, max_stacks, allow_overheal, effect_type, action_kind);
	});
}

void TeamfightSimulationCore::_apply_aoe_taunt(UnitState &source, double radius, double duration) {
	_viewer_record_aoe_ring_fx(source, source, radius, StringName("aoe_taunt"));
	StringName source_team = source.team;
	StringName enemy_team = source_team == StringName("player") ? StringName("enemy") : StringName("player");
	const std::vector<int64_t> &enemy_indices = _alive_indices_for_team(enemy_team);
	AoCircleIterationParams cir;
	cir.center_x = source.pos_x;
	cir.center_y = source.pos_y;
	cir.radius = radius;
	cir.indices = &enemy_indices;
	cir.spatial_team = enemy_team;
	_for_each_unit_in_circle(cir, [&](UnitState &unit) {
		unit.taunt_target_id = source.instance_id;
		unit.taunt_remaining = Math::max(unit.taunt_remaining, duration);
		unit.forced_target_id = source.instance_id;
		unit.forced_target_remaining = Math::max(unit.forced_target_remaining, duration);
		_uc(unit).forced_target_kind = StringName("taunt");
	});
}

double TeamfightSimulationCore::_apply_aoe_damage(UnitState &source, UnitState &center_source, double damage, double radius, const StringName &damage_type, const StringName &reason, const StringName &action_kind) {
	(void)reason;
	_viewer_record_aoe_ring_fx(source, center_source, radius, StringName("aoe_damage"));
	StringName source_team = source.team;
	StringName enemy_team = source_team == StringName("player") ? StringName("enemy") : StringName("player");
	const std::vector<int64_t> &enemy_indices = _alive_indices_for_team(enemy_team);
	double total_damage = 0.0;
	AoCircleIterationParams cir;
	cir.center_x = center_source.pos_x;
	cir.center_y = center_source.pos_y;
	cir.radius = radius;
	cir.indices = &enemy_indices;
	cir.spatial_team = enemy_team;
	_for_each_unit_in_circle(cir, [&](UnitState &unit) {
		EffectContext context = _build_context(source, &unit, nullptr, damage, action_kind);
		total_damage += _apply_damage(source, unit, damage, damage_type, action_kind, context);
	});
	return total_damage;
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
	const int64_t current_target_index = unit.target_id != 0 ? _unit_index_by_id(unit.target_id) : -1;
	UnitState *current_target_live = current_target_index >= 0 ? &_units[static_cast<size_t>(current_target_index)] : nullptr;
	const TargetingFrameEntry *current_target = current_target_index >= 0 && current_target_index < int64_t(_targeting_frame.size())
			? &_targeting_frame[static_cast<size_t>(current_target_index)]
			: nullptr;
	bool forced_reason = true;
	if (current_target != nullptr && current_target->alive && current_target->is_player_team != (unit.team == sn_player())) {
		forced_reason = false;
	}

	const UnitStrategy &strategy = _strategy_for_unit(unit);
	const TickContext &ctx = _tick_ctx;
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
	const bool unit_is_player = unit.team == sn_player();
	const int64_t unit_instance_id = unit.instance_id;
	const int64_t unit_forced_target_id = unit.forced_target_id;
	const double unit_forced_target_remaining = unit.forced_target_remaining;
	const int64_t unit_current_ally_target_id = unit.current_ally_target_id;
	const bool unit_is_assassin_role = unit.is_assassin_role;
	const double unit_attack_damage = get_effective_attack_damage(unit);
	const std::vector<int64_t> &carry_indices = unit_is_player ? ctx.player_carry_indices : ctx.enemy_carry_indices;
	const std::vector<int64_t> &frontline_indices = unit_is_player ? ctx.enemy_frontline_indices : ctx.player_frontline_indices;
	const std::vector<int64_t> &enemy_indices = unit_is_player ? _alive_enemy_indices : _alive_player_indices;

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
		// Score is diagnostic-only; retarget cooldown means no gameplay decision needs recomputing.
		if (current_target_live != nullptr) {
			_set_current_target(unit, *current_target_live);
		}
		return current_target_live;
	}

	// Obscurance aux: only needed when scoring enemies; reuse grid per tick when opposing frontline snapshot unchanged.
	if (score_ctx.use_spatial && strategy.obscurance_weight > 0.0) {
		const uint64_t sig = _obscurance_aux_frontline_signature(frontline_indices);
		if (unit_is_player) {
			if (_obscurance_aux_enemy_grid_tick == _tick && _obscurance_aux_enemy_grid_sig == sig) {
				score_ctx.has_obscurance_cache = true;
			} else {
				_spatial_fill_buckets_for_indices_aux(frontline_indices);
				score_ctx.has_obscurance_cache = true;
				_obscurance_aux_enemy_grid_tick = _tick;
				_obscurance_aux_enemy_grid_sig = sig;
			}
		} else {
			if (_obscurance_aux_player_grid_tick == _tick && _obscurance_aux_player_grid_sig == sig) {
				score_ctx.has_obscurance_cache = true;
			} else {
				_spatial_fill_buckets_for_indices_aux(frontline_indices);
				score_ctx.has_obscurance_cache = true;
				_obscurance_aux_player_grid_tick = _tick;
				_obscurance_aux_player_grid_sig = sig;
			}
		}
	}

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
	auto classify_bucket = [&](const TargetingFrameEntry &candidate, double dist, const UnitStrategy &strat) -> TeamfightSimulationCore::TargetBucketTag {
		if (unit_has_forced_target && candidate.instance_id == unit_forced_target_id) {
			return TeamfightSimulationCore::TargetBucketTag::Commit;
		}
		TeamfightSimulationCore::TargetBucketTag bucket_tag = TeamfightSimulationCore::TargetBucketTag::Objective;
		if (strat.carry_peel_weight > 0.0 && candidate.target_id == unit_instance_id) {
			return TeamfightSimulationCore::TargetBucketTag::Peel;
		}
		if (unit_can_peel_for_ally && candidate.target_id == unit_current_ally_target_id) {
			return TeamfightSimulationCore::TargetBucketTag::Peel;
		}
		if (has_execute_bonus) {
			double hp_ratio = candidate.hp / Math::max(0.0001, candidate.max_hp);
			double atk_dmg = unit_attack_damage;
			if (hp_ratio <= TARGET_EXECUTE_HP_RATIO || candidate.hp <= atk_dmg) {
				return TeamfightSimulationCore::TargetBucketTag::Burst;
			}
		}
		if (prefers_kiting) {
			if (dist >= score_ctx.kite_min_w && dist <= score_ctx.kite_max_w) {
				return TeamfightSimulationCore::TargetBucketTag::Kite;
			}
		}
		return bucket_tag;
	};
	const std::array<int, static_cast<size_t>(TeamfightSimulationCore::TargetBucketTag::TagCount)> &bucket_rank_by_tag = strategy.bucket_rank_by_tag;

	// Python: whenever we do evaluate, we reset the retarget timer immediately (even if we end up keeping).
	unit.retarget_timer = RETARGET_INTERVAL;

	bool current_target_raw_valid = false;
	double current_target_raw = 0.0;
	double current_target_dist_for_switch = -1.0;
	const double unit_x = unit.pos_x;
	const double unit_y = unit.pos_y;
	const bool has_bodyguard_weight = strategy.bodyguard_weight > 0.0;
	const double bodyguard_bonus_bound = has_bodyguard_weight ? strategy.bodyguard_weight * double(carry_indices.size()) : 0.0;
	UnitState *best_live = nullptr;
	int64_t best_index = -1;
	double best_adjusted = std::numeric_limits<double>::infinity();
	double best_raw = std::numeric_limits<double>::infinity();
	int best_bucket_rank = 0;
	double best_dist = std::numeric_limits<double>::infinity();
	for (int64_t enemy_index : enemy_indices) {
		const TargetingFrameEntry &candidate = _targeting_frame[static_cast<size_t>(enemy_index)];
		// Skip stealthed enemy units (cannot be targeted)
		if (candidate.stealth_remaining > 0.0) {
			continue;
		}
		double dx = candidate.pos_x - unit_x;
		double dy = candidate.pos_y - unit_y;
		double dist = Math::sqrt(dx * dx + dy * dy);
		TeamfightSimulationCore::TargetBucketTag bucket_tag = classify_bucket(candidate, dist, strategy);
		int rank = bucket_rank_by_tag[static_cast<size_t>(bucket_tag)];
		double prefix_score = _score_enemy_target_prefix(unit, candidate, ally_for_peel, strategy, ctx, score_ctx, dist, profile_sim, enemy_index);
		double adjusted_lower_bound = prefix_score + double(rank) * strategy.bucket_margin;
		if (best_live != nullptr && enemy_index != current_target_index) {
			double candidate_lower_bound = adjusted_lower_bound - bodyguard_bonus_bound;
			if (candidate_lower_bound > best_adjusted) {
				continue;
			}
		}
		double raw = _score_enemy_target(unit, candidate, ally_for_peel, strategy, ctx, score_ctx, dist, profile_sim, enemy_index);
		if (current_target_index >= 0 && enemy_index == current_target_index) {
			current_target_raw = raw;
			current_target_raw_valid = true;
			current_target_dist_for_switch = dist;
		}
		double adjusted = raw + double(rank) * strategy.bucket_margin;
		// Python parity: strict lexicographic ordering on key:
		// (adjusted_score, raw_score, bucket_rank, distance, instance_id)
		if (best_live == nullptr
				|| std::make_tuple(adjusted, raw, rank, dist, candidate.instance_id) <
						std::make_tuple(best_adjusted, best_raw, best_bucket_rank, best_dist, _targeting_frame[static_cast<size_t>(best_index)].instance_id)) {
			best_live = &_units[static_cast<size_t>(enemy_index)];
			best_index = enemy_index;
			best_adjusted = adjusted;
			best_raw = raw;
			best_bucket_rank = rank;
			best_dist = dist;
		}
	}
	if (best_live == nullptr) {
		unit.target_id = 0;
		unit.current_target_score = 0.0;
		_sync_targeting_frame_unit(unit);
		return nullptr;
	}

	const TargetingFrameEntry *best_frame = &_targeting_frame[static_cast<size_t>(best_index)];

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
		double current_dist = Math::sqrt((current_target->pos_x - unit.pos_x) * (current_target->pos_x - unit.pos_x) + (current_target->pos_y - unit.pos_y) * (current_target->pos_y - unit.pos_y));
		current_score = _score_enemy_target(unit, *current_target, ally_for_peel, strategy, ctx, score_ctx, current_dist, profile_sim, current_target_index);
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
		double dist = Math::sqrt((candidate.pos_x - unit.pos_x) * (candidate.pos_x - unit.pos_x) + (candidate.pos_y - unit.pos_y) * (candidate.pos_y - unit.pos_y));
		double score = _score_ally_target(unit, candidate, strat, dist);
		double hp_ratio = candidate.hp / Math::max(0.0001, candidate.max_hp);
		if (_scratch_critical_allies.empty()) {
			// Python: min by (score, distance, instance_id)
			if (best == nullptr
					|| std::make_tuple(score, dist, candidate.instance_id) <
							std::make_tuple(best_score, best_dist, best->instance_id)) {
				best = &_units[static_cast<size_t>(ally_index)];
				best_score = score;
				best_dist = dist;
				best_hp_ratio = hp_ratio;
			}
			continue;
		}
		// Python critical allies: min by (hp_ratio, score, distance, instance_id)
		if (best == nullptr
				|| std::make_tuple(hp_ratio, score, dist, candidate.instance_id) <
						std::make_tuple(best_hp_ratio, best_score, best_dist, best->instance_id)) {
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
	double dx = right.pos_x - left.pos_x;
	double dy = right.pos_y - left.pos_y;
	return Math::sqrt(dx * dx + dy * dy);
}

bool TeamfightSimulationCore::_position_collides_with_unit(double x, double y, int64_t exclude_instance_id) const {
	double collision_radius = UNIT_COLLISION_RADIUS;
	for (const UnitState &unit : _units) {
		if (unit.instance_id == exclude_instance_id) {
			continue;
		}
		if (!unit.alive) {
			continue;
		}
		double unit_x = unit.pos_x;
		double unit_y = unit.pos_y;
		double dist = Math::sqrt((x - unit_x) * (x - unit_x) + (y - unit_y) * (y - unit_y));
		if (dist < collision_radius * 2.0) {
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
	double target_to_start_dist = Math::sqrt((target_x - start_x) * (target_x - start_x) + (target_y - start_y) * (target_y - start_y));
	
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
	_sync_targeting_frame_unit(target);
	
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

	UnitState *killer_unit = _unit_by_id(killer_id);
	if (killer_unit != nullptr) {
		_uc(*killer_unit).kills += 1;
		if (killer_unit->team == StringName("player")) {
			_player_kills += 1;
		} else if (killer_unit->team == StringName("enemy")) {
			_enemy_kills += 1;
		}
	}

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
	unit.silence_blocks_abilities = false;
	unit.silence_blocks_ultimates = false;
	unit.disarm_remaining = 0.0;
	unit.stealth_remaining = 0.0;
	unit.stealth_break_on_attack = false;
	unit.stealth_break_on_ability = false;
	unit.stealth_break_on_damage_taken = false;
	unit.reflect_buff_remaining = 0.0;
	unit.reflect_buff_pct_all = 0.0;
	unit.reflect_buff_pct_physical = 0.0;
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
	c.effect_accumulators.clear();
	unit.respawned_this_tick = true;
	unit.cast_resolved_this_tick = false;
	// Clear casting state on respawn
	unit.casting_remaining = 0.0;
	c.casting_kind = StringName();
	c.casting_effect = EffectRecord();
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
	_sync_targeting_frame_unit(unit);
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
	_sim_profile_se_bodyguard = 0;
	_sim_profile_se_obscurance = 0;
	_sim_profile_se_flanking = 0;
	_sim_profile_se_calls = 0;
}

void TeamfightSimulationCore::_sim_profile_emit_json_stderr() const {
	if (_sim_profile_tick_count <= 0) {
		return;
	}
	const double inv = 1.0 / double(_sim_profile_tick_count);
	auto avg = [&](uint64_t ns) { return double(ns) * inv; };
	const uint64_t uu_sum = _sim_profile_uu_dead_respawn + _sim_profile_uu_cooldowns_cc + _sim_profile_uu_separation +
			_sim_profile_uu_threat_and_assist + _sim_profile_uu_regen_on_tick + _sim_profile_uu_casting +
			_sim_profile_uu_targeting + _sim_profile_uu_combat + _sim_profile_uu_movement;
	const uint64_t sum = _sim_profile_ns_projectiles + _sim_profile_ns_prepare_tick_ctx +
			_sim_profile_ns_refresh_pressure_pre + uu_sum + _sim_profile_ns_refresh_pressure_post;
	const int64_t se_calls = _sim_profile_se_calls;
	const double se_inv = se_calls > 0 ? 1.0 / double(se_calls) : 0.0;
	auto se_avg = [&](uint64_t ns) { return double(ns) * se_inv; };
	std::fprintf(stderr,
			"{\"sim_profile\":{\"ticks\":%lld,\"ns_per_tick_avg\":{\"projectiles\":%.0f,\"prepare_tick_ctx\":%.0f,\"refresh_pressure_pre\":%.0f,\"update_units\":%.0f,\"refresh_pressure_post\":%.0f},\"update_unit_ns_per_tick_avg\":{\"uu_dead_respawn\":%.0f,\"uu_cooldowns_cc\":%.0f,\"uu_separation\":%.0f,\"uu_threat_and_assist\":%.0f,\"uu_regen_on_tick\":%.0f,\"uu_casting\":%.0f,\"uu_targeting\":%.0f,\"uu_combat\":%.0f,\"uu_movement\":%.0f},\"sum_ns_per_tick_avg\":%.0f,\"score_enemy_ns\":{\"calls\":%lld,\"avg_per_call_ns\":{\"base\":%.0f,\"bodyguard\":%.0f,\"obscurance\":%.0f,\"flanking\":%.0f}}}}\n",
			static_cast<long long>(_sim_profile_tick_count),
			avg(_sim_profile_ns_projectiles),
			avg(_sim_profile_ns_prepare_tick_ctx),
			avg(_sim_profile_ns_refresh_pressure_pre),
			avg(uu_sum),
			avg(_sim_profile_ns_refresh_pressure_post),
			avg(_sim_profile_uu_dead_respawn),
			avg(_sim_profile_uu_cooldowns_cc),
			avg(_sim_profile_uu_separation),
			avg(_sim_profile_uu_threat_and_assist),
			avg(_sim_profile_uu_regen_on_tick),
			avg(_sim_profile_uu_casting),
			avg(_sim_profile_uu_targeting),
			avg(_sim_profile_uu_combat),
			avg(_sim_profile_uu_movement),
			double(sum) * inv,
			static_cast<long long>(se_calls),
			se_avg(_sim_profile_se_base),
			se_avg(_sim_profile_se_bodyguard),
			se_avg(_sim_profile_se_obscurance),
			se_avg(_sim_profile_se_flanking));
	std::fflush(stderr);
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
	} else {
		for (UnitState &unit : _units) {
			_update_unit(unit, false);
		}
	}
}

void TeamfightSimulationCore::_simulate() {
	const bool profile = _sim_profile_env_enabled();
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

		unit.attack_cooldown = Math::max(0.0, unit.attack_cooldown - _tick_rate);
		unit.ability_cooldown = Math::max(0.0, unit.ability_cooldown - _tick_rate);
		unit.retarget_timer = Math::max(0.0, unit.retarget_timer - _tick_rate);
		unit.target_switch_lock_timer = Math::max(0.0, unit.target_switch_lock_timer - _tick_rate);
		if (unit.stun_remaining > 0.0) {
			unit.hard_cc_seconds += Math::min(unit.stun_remaining, _tick_rate);
		}
		unit.stun_remaining = Math::max(0.0, unit.stun_remaining - _tick_rate);
		unit.slow_remaining = Math::max(0.0, unit.slow_remaining - _tick_rate);
		if (unit.slow_remaining <= 0.0) {
			unit.slow_remaining = 0.0;
			unit.slow_move_mult = 1.0;
		}
		unit.root_remaining = Math::max(0.0, unit.root_remaining - _tick_rate);
		unit.silence_remaining = Math::max(0.0, unit.silence_remaining - _tick_rate);
		if (unit.silence_remaining <= 0.0) {
			unit.silence_remaining = 0.0;
			unit.silence_blocks_abilities = false;
			unit.silence_blocks_ultimates = false;
		}
		unit.disarm_remaining = Math::max(0.0, unit.disarm_remaining - _tick_rate);
		unit.stealth_remaining = Math::max(0.0, unit.stealth_remaining - _tick_rate);
		if (unit.shield > 0.0) {
			unit.shield *= (1.0 - SHIELD_DECAY_RATE * _tick_rate);
			if (unit.shield < 0.01) {
				unit.shield = 0.0;
			}
		}
		if (unit.stealth_remaining <= 0.0) {
			unit.stealth_remaining = 0.0;
			unit.stealth_break_on_attack = false;
			unit.stealth_break_on_ability = false;
			unit.stealth_break_on_damage_taken = false;
		}
		unit.reflect_buff_remaining = Math::max(0.0, unit.reflect_buff_remaining - _tick_rate);
		if (unit.reflect_buff_remaining <= 0.0) {
			unit.reflect_buff_remaining = 0.0;
			unit.reflect_buff_pct_all = 0.0;
			unit.reflect_buff_pct_physical = 0.0;
		}
		unit.taunt_remaining = Math::max(0.0, unit.taunt_remaining - _tick_rate);
		unit.forced_target_remaining = Math::max(0.0, unit.forced_target_remaining - _tick_rate);
		unit.last_kite_timer = Math::max(0.0, unit.last_kite_timer - _tick_rate);
		if (unit.taunt_remaining <= 0.0) {
			unit.taunt_remaining = 0.0;
			unit.taunt_target_id = 0;
		}
		
		// Stat modifier duration management
		_update_stat_modifier_durations(unit, _tick_rate);
		_clear_expired_stat_modifiers(unit);
		
		// Stack duration management
		_update_stacks(unit, _tick_rate, _time);
		if (unit.forced_target_remaining <= 0.0) {
			unit.forced_target_remaining = 0.0;
			unit.forced_target_id = 0;
			_uc(unit).forced_target_kind = StringName();
		}
	}

	// Apply overtime damage during sudden death
	if (_sudden_death_ticks > 0) {
		for (UnitState &unit : _units) {
			if (!unit.alive) {
				continue;
			}
			
			double max_hp = get_effective_max_hp(unit);
			double damage = (max_hp * OVERTIME_DAMAGE_BASE_RATE) + 1.0;
			
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

	// Separation (Python: after CC tick, before threat decay; skipped while stunned or rooted after decrement).
	{
		SimProfileAccScope _uu_sep(profile_sim, _sim_profile_uu_separation);
		if (unit.stun_remaining <= 0.0 && unit.root_remaining <= 0.0) {
			double move_speed = get_effective_move_speed(unit) * _movement_speed_multiplier(unit);
			if (move_speed > 0.0) {
				double attack_range = get_effective_attack_range(unit);
				double radius = attack_range > SEPARATION_RANGE_THRESHOLD ? SEPARATION_RADIUS_RANGED : SEPARATION_RADIUS_MELEE;
				double r2 = radius * radius;
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
					if (d2 <= EPSILON || d2 >= r2) {
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
		unit.perceived_threat = Math::max(0.0, unit.perceived_threat - strategy.threat_decay_rate * _tick_rate);
		_sync_targeting_frame_unit(unit);

		_prune_assist_window(unit);
	}

	{
		SimProfileAccScope _uu_regen(profile_sim, _sim_profile_uu_regen_on_tick);
		const std::vector<EffectRecord> &effects = _uc(unit).passive_effects[EFFECT_BUCKET_ON_TICK];
		for (const EffectRecord &effect : effects) {
			// Use effect address as unique key for per-unit timing
			size_t effect_key = reinterpret_cast<size_t>(&effect);
			
			// Get or create per-unit accumulator for this effect
			double &accumulator = _uc(unit).effect_accumulators[effect_key];
			
			// Update accumulator
			accumulator += _tick_rate;
			
			// Check if this effect should execute
			if (accumulator >= effect.on_tick_interval) {
				accumulator -= effect.on_tick_interval;
				EffectContext context = _build_context(unit, nullptr, nullptr, 0.0, sn_passive());
				_execute_effect(effect, context);
			}
		}

		// Tick periodic effects (DoT/HoT)
		_tick_periodic_effects(unit, _tick_rate);

		if (unit.stun_remaining > 0.0) {
			return;
		}
	}

	if (unit.casting_remaining > 0.0) {
		SimProfileAccScope _uu_cast(profile_sim, _sim_profile_uu_casting);
		unit.casting_remaining = Math::max(0.0, unit.casting_remaining - _tick_rate);
		if (unit.casting_remaining <= 0.0) {
			_resolve_cast(unit);
			unit.cast_resolved_this_tick = true;
		}
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
			_sync_targeting_frame_unit(unit);
			return;
		}
	}

	{
		SimProfileAccScope _uu_combat(profile_sim, _sim_profile_uu_combat);
		double distance = _distance_between(unit, *target);
		double effective_range = _effective_attack_range(unit);
		bool in_contact = (distance <= effective_range); // No buffer for testing

		// Action priority: check range requirements per action type
		bool can_cast_ultimate = in_contact || !unit.ultimate_requires_target_in_range;
		bool can_cast_ability = in_contact || !unit.ability_requires_target_in_range;

		if (can_cast_ultimate) {
			if (_try_cast_ultimate(unit, *target, distance)) {
				return;
			}
		}
		if (can_cast_ability) {
			if (_try_cast_ability(unit, *target, distance)) {
				return;
			}
		}
		if (in_contact) {
			if (unit.attack_cooldown <= 0.0) {
				if (unit.combat.attack_speed > 0.0) {
					_perform_auto_attack(unit, *target, distance);
					return;
				}
			}
		}
	}

	{
		SimProfileAccScope _uu_move(profile_sim, _sim_profile_uu_movement);
		if (unit.root_remaining > 0.0) {
			return;
		}
		// Movement: kite first if applicable, else move toward.
		if (strategy.prefers_kiting && (unit.attack_cooldown > 0.0 || unit.combat.attack_speed == 0.0) && unit.taunt_remaining <= 0.0) {
			if (_kite_from_enemies(unit)) {
				return;
			}
		}
		// For melee champions: move while attacking until reaching actual attack range
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

bool TeamfightSimulationCore::_try_cast_ability(UnitState &unit, UnitState &target, double distance) {
	(void)distance;
	if (unit.silence_remaining > 0.0 && unit.silence_blocks_abilities) {
		return false;
	}
	if (unit.root_remaining > 0.0 && unit.has_ability_effect && _effect_record_contains_opcode(_uc(unit).ability_effect, EFFECT_OPCODE_SELF_DASH)) {
		return false;
	}
	if (unit.ability_cooldown > 0.0) {
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
	double max_mana = get_effective_max_mana(unit);
	if (max_mana <= 0.0 || unit.mana < max_mana) {
		return false;
	}
	return _start_cast(unit, target, distance, StringName("ultimate"));
}

bool TeamfightSimulationCore::_start_cast(UnitState &unit, UnitState &target, double distance, const StringName &action_kind) {
	(void)distance;
	bool has_effect = action_kind == sn_ability() ? unit.has_ability_effect : unit.has_ultimate_effect;
	if (!has_effect) {
		return false;
	}
	UnitState *target_ally = _select_ally_target(unit);
	if (action_kind == sn_ability()) {
		_uc(unit).abilities += 1;
	} else {
		unit.mana = Math::max(0.0, unit.mana - get_effective_max_mana(unit));
	}
	unit.casting_remaining = CASTING_WINDUP;
	UnitStateCold &ucast = _uc(unit);
	ucast.casting_kind = action_kind;
	ucast.casting_effect = action_kind == sn_ability() ? ucast.ability_effect : ucast.ultimate_effect;
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
	auto _effect_uses_projectile = [](const EffectRecord &e) -> bool {
		if (e.opcode == EFFECT_OPCODE_PROJECTILE) return true;
		if (e.opcode == EFFECT_OPCODE_MULTI) {
			for (const EffectRecord &child : e.children) {
				if (child.opcode == EFFECT_OPCODE_PROJECTILE) return true;
			}
		}
		return false;
	};
	if (_effect_uses_projectile(effect) && (target == nullptr || !target->alive)) {
		if (action_kind == StringName("ability")) {
			unit.ability_cooldown = 0.0;
		} else if (action_kind == StringName("ultimate")) {
			double effective_max_mana = get_effective_max_mana(unit);
			unit.mana = Math::min(effective_max_mana, unit.mana + effective_max_mana);
		}
		return;
	}
	EffectContext context = _build_context(unit, target, target_ally, 0.0, action_kind);
	_execute_effect(effect, context);
}

void TeamfightSimulationCore::_perform_auto_attack(UnitState &unit, UnitState &target, double distance) {
	if (unit.disarm_remaining > 0.0) {
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
		projectile.damage_type = StringName("physical");
		projectile.stun_duration = DEFAULT_PROJECTILE_STUN_DURATION;
		projectile.radius = get_effective_projectile_radius(unit);
		projectile.speed = Math::max(0.0001, get_effective_projectile_speed(unit));
		projectile.pos_x = unit.pos_x;
		projectile.pos_y = unit.pos_y;
		projectile.action_kind = StringName("auto");
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
			_heal_unit(unit, unit, dealt * life_steal, StringName("auto"));
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
	double distance = Math::sqrt(dx * dx + dy * dy);
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
	double distance = Math::sqrt(dx * dx + dy * dy);
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

bool TeamfightSimulationCore::_kite_from_enemies(UnitState &unit) {
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
	double mag = Math::sqrt(rep_x * rep_x + rep_y * rep_y);
	if (mag <= EPSILON) {
		return false;
	}
	double vel_x = rep_x / mag;
	double vel_y = rep_y / mag;
	bool blocked_x = (ux <= WORLD_BOUNDARY_MIN + BOUNDARY_DETECTION_MARGIN && vel_x < 0.0) || (ux >= WORLD_BOUNDARY_MAX - BOUNDARY_DETECTION_MARGIN && vel_x > 0.0);
	bool blocked_y = (uy <= WORLD_BOUNDARY_MIN + BOUNDARY_DETECTION_MARGIN && vel_y < 0.0) || (uy >= WORLD_BOUNDARY_MAX - BOUNDARY_DETECTION_MARGIN && vel_y > 0.0);
	if (blocked_x) {
		vel_x = 0.0;
	}
	if (blocked_y) {
		vel_y = 0.0;
	}
	if (Math::is_zero_approx(vel_x) && Math::is_zero_approx(vel_y)) {
		vel_x = ux < (WORLD_SIZE * 0.5) ? RECOVERY_VELOCITY : -RECOVERY_VELOCITY;
		vel_y = uy < (WORLD_SIZE * 0.5) ? RECOVERY_VELOCITY : -RECOVERY_VELOCITY;
	}
	// Python parity: renormalize after boundary blocking so wall-hugging units
	// still move at full kite speed (Python unit_movement.py lines 99-101).
	double new_mag = Math::sqrt(vel_x * vel_x + vel_y * vel_y);
	if (new_mag <= EPSILON) {
		return false;
	}
	vel_x /= new_mag;
	vel_y /= new_mag;
	unit.last_kite_timer = KITE_DURATION;
	double move_speed = get_effective_move_speed(unit) * _movement_speed_multiplier(unit);
	double step = move_speed * KITE_SPEED_MODIFIER * _tick_rate;
	unit.pos_x = Math::clamp(ux + vel_x * step, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
	unit.pos_y = Math::clamp(uy + vel_y * step, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
	_sync_targeting_frame_unit(unit);
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
			_heal_unit(*source, *source, dealt * life_steal, sn_auto());
		}
	}
	if (projectile.stun_duration > 0.0 && target->alive) {
		_apply_stun(*source, *target, projectile.stun_duration);
	}
}

void TeamfightSimulationCore::_update_projectiles() {
	// Python world.py: each tick, projectiles step toward the target's *current* position
	// (homing); hit when dist <= speed * dt + radius.
	_scratch_projectiles.clear();
	for (const ProjectileState &data : _projectiles) {
		UnitState *target = _unit_by_id(data.target_id);
		if (target == nullptr || !target->alive) {
			continue;
		}
		double dx = target->pos_x - data.pos_x;
		double dy = target->pos_y - data.pos_y;
		double dist = Math::sqrt(dx * dx + dy * dy);
		double move_dist = data.speed * _tick_rate;
		if (dist <= move_dist + data.radius) {
			_resolve_projectile(data);
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
	using std::swap;
	swap(_projectiles, _scratch_projectiles);
	_scratch_projectiles.clear();
}

Dictionary TeamfightSimulationCore::_execute_effect(const EffectRecord &effect, EffectContext &context) {
	Dictionary result;
	
	if (context.source == nullptr) {
		return result;
	}
	UnitState &source = *context.source;
	UnitState *target = context.target;
	UnitState *target_ally = context.target_ally;
	
	// Check conditional requirements
	if (!_check_condition(effect, context.accumulated_results)) {
		Dictionary failed_result;
		failed_result["success"] = false;
		failed_result["condition_failed"] = true;
		return failed_result;
	}
	
	switch (effect.opcode) {
		case EFFECT_OPCODE_MULTI: {
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
			double damage = source.combat.max_hp * effect.scalar0;  // max_hp_ratio
			damage += source.combat.attack_damage * effect.scalar1;  // damage_ratio
			damage += effect.scalar2;  // flat_amount
			
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
			double damage = source.combat.attack_damage * effect.scalar2;
			
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
				? Math::max(0.0001, source.combat.projectile_speed)
				: Math::max(0.0001, effect.scalar0);
			projectile_state.radius = (effect.scalar1 < 0.0)
				? source.combat.projectile_radius
				: effect.scalar1;
			projectile_state.speed = projectile_speed;
			projectile_state.pos_x = source.pos_x;
			projectile_state.pos_y = source.pos_y;
			projectile_state.action_kind = context.action_kind;
			projectile_state.reason = effect.reason;
			_projectiles.push_back(projectile_state);
			projectile_result["success"] = true;
			projectile_result["projectile_created"] = true;
			projectile_result["damage"] = projectile_state.damage;
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
			double amount = source.combat.max_hp * effect.scalar0;
			
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
			double heal_amount = source.combat.max_hp * effect.scalar0 + heal_target.hp * effect.scalar1 + effect.scalar2;
			// Add missing HP scaling
			double missing_hp = heal_target.combat.max_hp - heal_target.hp;
			heal_amount += missing_hp * effect.scalar3;
			_heal_unit(source, heal_target, heal_amount, context.action_kind);
			heal_result["heal_applied"] = true;
			heal_result["amount"] = heal_amount;
			return heal_result;
		}
		case EFFECT_OPCODE_SELF_AOE_TAUNT: {
			Dictionary taunt_result;
			taunt_result["success"] = true;
			_apply_aoe_taunt(source, effect.scalar0, effect.scalar1);
			taunt_result["taunt_applied"] = true;
			taunt_result["radius"] = effect.scalar0;
			taunt_result["duration"] = effect.scalar1;
			return taunt_result;
		}
		case EFFECT_OPCODE_SELF_AOE_DAMAGE: {
			Dictionary aoe_damage_result;
			aoe_damage_result["success"] = true;
			double aoe_damage;
			if (effect.scalar2 > 0.0) {
				aoe_damage = effect.scalar2;
			} else {
				aoe_damage = source.combat.attack_damage * effect.scalar1;
			}
			double total_damage = _apply_aoe_damage(source, source, aoe_damage, effect.scalar0, effect.damage_type.is_empty() ? StringName("physical") : effect.damage_type, effect.reason, context.action_kind);
			// Store total damage in context for damage_based_heal to use
			context.damage = total_damage;
			aoe_damage_result["damage_dealt"] = total_damage;
			return aoe_damage_result;
		}
		case EFFECT_OPCODE_TARGET_AOE_DAMAGE: {
			Dictionary splash_result;
			splash_result["success"] = true;
			if (target != nullptr) {
				_apply_target_aoe_damage(source, *target, context.damage, effect.scalar0, effect.damage_type.is_empty() ? StringName("physical") : effect.damage_type, context.action_kind, effect.reason, effect.scalar1);
				splash_result["splash_applied"] = true;
			}
			return splash_result;
		}
		case EFFECT_OPCODE_DAMAGE_THRESHOLD_TRIGGER: {
			Dictionary threshold_result;
			threshold_result["success"] = true;
			if (context.damage > source.combat.attack_damage * effect.scalar0 && !effect.children.empty()) {
				Dictionary child_result = _execute_effect(effect.children[0], context);
				threshold_result["triggered"] = true;
				_merge_accumulated_results(threshold_result, child_result);
			}
			return threshold_result;
		}
		case EFFECT_OPCODE_DAMAGE_OVER_TIME: {
			Dictionary dot_result;
			dot_result["success"] = true;
			UnitState *dot_target = (effect.int0 == 1) ? &source : target;
			if (dot_target != nullptr) {
				_apply_dot(source, *dot_target, effect.scalar0, effect.scalar1, effect.scalar3,
						   double(effect.int2), effect.scalar2,
						   effect.damage_type.is_empty() ? StringName("physical") : effect.damage_type,
						   effect.stacking_mode, effect.int1, effect.effect_type, context.action_kind);
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
					   double(effect.int2), effect.scalar2,
					   effect.stacking_mode, effect.int1, effect.int3 != 0, effect.effect_type, context.action_kind);
			hot_result["hot_applied"] = true;
			return hot_result;
		}
		case EFFECT_OPCODE_AOE_DAMAGE_OVER_TIME: {
			Dictionary aoe_dot_result;
			aoe_dot_result["success"] = true;
			_apply_aoe_dot(source, effect.scalar0, effect.scalar1, effect.scalar2, effect.scalar3,
						   double(effect.int1), effect.scalar4,
						   effect.damage_type.is_empty() ? StringName("physical") : effect.damage_type,
						   effect.stacking_mode, effect.int0, effect.effect_type, effect.int2 != 0, context.action_kind);
			aoe_dot_result["aoe_dot_applied"] = true;
			return aoe_dot_result;
		}
		case EFFECT_OPCODE_AOE_HEAL_OVER_TIME: {
			Dictionary aoe_hot_result;
			aoe_hot_result["success"] = true;
			_apply_aoe_hot(source, effect.scalar0, effect.scalar1, effect.scalar2, effect.scalar3, effect.scalar4,
						   double(effect.int1), effect.scalar5,
						   effect.stacking_mode, effect.int0, effect.int2 != 0, effect.effect_type, effect.int3 != 0, context.action_kind);
			aoe_hot_result["aoe_hot_applied"] = true;
			return aoe_hot_result;
		}
		case EFFECT_OPCODE_MANA_REGEN: {
			Dictionary mana_result;
			mana_result["success"] = true;
			_restore_mana(source, source, effect.scalar0 + source.combat.max_mana * effect.scalar1);
			mana_result["mana_restored"] = effect.scalar0 + source.combat.max_mana * effect.scalar1;
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
			_heal_unit(source, source, context.damage * effect.scalar0, context.action_kind);
			heal_result["heal_applied"] = true;
			heal_result["amount"] = context.damage * effect.scalar0;
			return heal_result;
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
		case EFFECT_OPCODE_SELF_AOE_SLOW: {
			Dictionary aoe_slow_result;
			aoe_slow_result["success"] = true;
			_apply_self_aoe_slow(source, effect.scalar0, effect.scalar1, effect.scalar2);
			return aoe_slow_result;
		}
		case EFFECT_OPCODE_SELF_AOE_ROOT: {
			Dictionary aoe_root_result;
			aoe_root_result["success"] = true;
			_apply_self_aoe_root(source, effect.scalar0, effect.scalar1);
			return aoe_root_result;
		}
		case EFFECT_OPCODE_SELF_AOE_SILENCE: {
			Dictionary aoe_silence_result;
			aoe_silence_result["success"] = true;
			_apply_self_aoe_silence(source, effect.scalar0, effect.scalar1, effect.int0 != 0, effect.int1 != 0);
			return aoe_silence_result;
		}
		case EFFECT_OPCODE_SELF_AOE_DISARM: {
			Dictionary aoe_disarm_result;
			aoe_disarm_result["success"] = true;
			_apply_self_aoe_disarm(source, effect.scalar0, effect.scalar1);
			return aoe_disarm_result;
		}
		case EFFECT_OPCODE_TARGET_AOE_SLOW: {
			Dictionary aoe_slow_result;
			aoe_slow_result["success"] = true;
			if (target != nullptr) {
				_apply_target_aoe_slow(source, *target, effect.scalar0, effect.scalar1, effect.scalar2);
			}
			return aoe_slow_result;
		}
		case EFFECT_OPCODE_TARGET_AOE_ROOT: {
			Dictionary aoe_root_result;
			aoe_root_result["success"] = true;
			if (target != nullptr) {
				_apply_target_aoe_root(source, *target, effect.scalar0, effect.scalar1);
			}
			return aoe_root_result;
		}
		case EFFECT_OPCODE_TARGET_AOE_SILENCE: {
			Dictionary aoe_silence_result;
			aoe_silence_result["success"] = true;
			if (target != nullptr) {
				_apply_target_aoe_silence(source, *target, effect.scalar0, effect.scalar1, effect.int0 != 0, effect.int1 != 0);
			}
			return aoe_silence_result;
		}
		case EFFECT_OPCODE_TARGET_AOE_DISARM: {
			Dictionary aoe_disarm_result;
			aoe_disarm_result["success"] = true;
			if (target != nullptr) {
				_apply_target_aoe_disarm(source, *target, effect.scalar0, effect.scalar1);
			}
			return aoe_disarm_result;
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
				_apply_knockback(source, *target, effect.scalar0, effect.int0 != 0);
				kb_result["success"] = true;
				kb_result["knockback_applied"] = true;
			}
			return kb_result;
		}
		case EFFECT_OPCODE_REFLECT: {
			Dictionary rf_result;
			rf_result["success"] = true;
			_apply_reflect_buff(source, effect.scalar0, effect.scalar1, effect.int0 == 1);
			return rf_result;
		}
		case EFFECT_OPCODE_SELF_AOE_KNOCKBACK: {
			Dictionary aoe_kb_result;
			aoe_kb_result["success"] = true;
			aoe_kb_result["knockback_applied"] = true;
			_apply_self_aoe_knockback(source, effect.scalar0, effect.scalar1, effect.int0 != 0);
			return aoe_kb_result;
		}
		case EFFECT_OPCODE_SELF_AOE_REFLECT: {
			Dictionary aoe_rf_result;
			aoe_rf_result["success"] = true;
			_apply_self_aoe_reflect(source, effect.scalar0, effect.scalar1, effect.scalar2, effect.int0 == 1);
			return aoe_rf_result;
		}
		case EFFECT_OPCODE_REFLECT_DAMAGE: {
			Dictionary rd_noop;
			rd_noop["success"] = true;
			return rd_noop;
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
						double dist = Math::sqrt(dx * dx + dy * dy);
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
					double final_dist = Math::sqrt(final_dx * final_dx + final_dy * final_dy);
					// Consider target reached if we're within attack range (with melee buffer)
					bool reached = (final_dist <= get_effective_attack_range(source) + 0.1);
					dash_result["reached_target"] = reached;
				}
			}
			
			source.pos_x = new_x;
			source.pos_y = new_y;
			_sync_targeting_frame_unit(source);
			
			// Calculate actual distance traveled
			double actual_distance = Math::sqrt((new_x - current_x) * (new_x - current_x) + (new_y - current_y) * (new_y - current_y));
			dash_result["distance_traveled"] = actual_distance;
			
			return dash_result;
		}
		case EFFECT_OPCODE_AUTO_DODGE:
		case EFFECT_OPCODE_CONSTANT_MULTIPLIER:
		case EFFECT_OPCODE_HP_THRESHOLD_DAMAGE_MULTIPLIER:
		case EFFECT_OPCODE_DISTANCE_THRESHOLD_MULTIPLIER:
		case EFFECT_OPCODE_STAT_MODIFIER: {
			Dictionary stat_result;
			stat_result["success"] = true;
			if (target != nullptr) {
				UnitState &modifier_target = (effect.int0 == 1) ? source : *target;
				
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
						effect.scalar0,
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
					// Use original simple application
					_apply_stat_modifier(source, modifier_target, effect.damage_type, effect.scalar0, effect.scalar1, effect.scalar2, effect.int1 != 0);
					stat_result["stat_modifier_applied"] = true;
					stat_result["use_stacking"] = false;
				}
				
				stat_result["stat_name"] = String(effect.damage_type);
				stat_result["additive"] = effect.scalar0;
				stat_result["multiplicative"] = effect.scalar1;
				stat_result["duration"] = effect.scalar2;
				stat_result["is_match_duration"] = effect.int1 != 0;
				stat_result["target_self"] = effect.int0 == 1;
				stat_result["reason"] = effect.reason;
			}
			return stat_result;
		}
		default: {
			// Opcodes without runtime execution resolve here (unknown kinds compile to UNKNOWN).
			Dictionary default_result;
			default_result["success"] = true;
			return default_result;
		}
	}
}

void TeamfightSimulationCore::_merge_accumulated_results(Dictionary &target, const Dictionary &source) {
	Variant source_variant = source;
	Variant iter;
	bool valid = false;
	if (!source_variant.iter_init(iter, valid)) {
		return;
	}
	while (valid) {
		Variant key = source_variant.iter_get(iter, valid);
		Variant source_value = source[key];
		target[key] = source_value;
		source_variant.iter_next(iter, valid);
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

void TeamfightSimulationCore::_merge_result(Dictionary &target_result, const Dictionary &source_result) {
	Variant source_variant = source_result;
	Variant iter;
	bool valid = false;
	if (!source_variant.iter_init(iter, valid)) {
		return;
	}
	while (valid) {
		Variant key = source_variant.iter_get(iter, valid);
		Variant source_value = source_result[key];
		if (target_result.has(key) && source_value.get_type() == Variant::FLOAT && target_result[key].get_type() == Variant::FLOAT) {
			target_result[key] = double(target_result[key]) + double(source_value);
		} else if (target_result.has(key) && source_value.get_type() == Variant::INT && target_result[key].get_type() == Variant::INT) {
			target_result[key] = int64_t(target_result[key]) + int64_t(source_value);
		} else {
			target_result[key] = source_value;
		}
		source_variant.iter_next(iter, valid);
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
	for (const UnitState &unit : _units) {
		const UnitStateCold &c = _uc(unit);
		Dictionary unit_summary;
		unit_summary["instance_id"] = unit.instance_id;
		unit_summary["archetype"] = String(c.archetype_id);
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
	Dictionary summary;
	summary["seed"] = _seed;
	summary["winner_team"] = String(_winner_team);
	summary["duration"] = _time;
	summary["sudden_death_ticks"] = int64_t(_sudden_death_ticks);
	summary["player_comp"] = _player_comp;
	summary["enemy_comp"] = _enemy_comp;
	Array unit_stats;
	for (const UnitState &unit : _units) {
		const UnitStateCold &c = _uc(unit);
		Dictionary unit_summary;
		unit_summary["archetype_id"] = String(c.archetype_id);
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

void TeamfightSimulationCore::_viewer_record_hot_status_fx(const UnitState &p_target, double p_duration, const StringName &p_effect_type) {
	ViewerFxEvent ev;
	ev.kind = StringName("hot_status");
	ev.target_id = p_target.instance_id;
	ev.val = p_duration;
	ev.radius = 0.0;
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
	if (p_u.reflect_buff_remaining > 0.0) {
		return String("REFLECTING");
	}
	if (p_u.disarm_remaining > 0.0) {
		return String("DISARMED");
	}
	if (p_u.slow_remaining > 0.0) {
		return String("SLOWED");
	}
	if (p_u.casting_remaining > 0.0) {
		return String("CASTING");
	}
	if (p_u.last_kite_timer > 0.0) {
		return String("KITING");
	}
	return String("ALIVE");
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
		d["max_hp"] = u.combat.max_hp;
		d["shield"] = u.shield;
		d["mana"] = u.mana;
		d["max_mana"] = u.combat.max_mana;
		d["target"] = u.target_id;
		d["target_id"] = u.target_id;
		d["stun"] = u.stun_remaining;
		d["stun_remaining"] = u.stun_remaining;
		d["slow_remaining"] = u.slow_remaining;
		d["root_remaining"] = u.root_remaining;
		d["silence_remaining"] = u.silence_remaining;
		d["disarm_remaining"] = u.disarm_remaining;
		d["stealth_remaining"] = u.stealth_remaining;
		d["reflect_buff_remaining"] = u.reflect_buff_remaining;
		d["alive"] = u.alive;
		d["state"] = _viewer_state_string(u);
		d["acd"] = u.attack_cooldown;
		d["abi"] = u.ability_cooldown;
		d["attack_cooldown"] = u.attack_cooldown;
		d["attack_range"] = get_effective_attack_range(u);
		d["attack_speed"] = u.combat.attack_speed;
		d["casting_remaining"] = u.casting_remaining;
		d["casting_kind"] = String(uc.casting_kind);
		
		// Calculate distance to target and in-range status
	bool in_range = false;
	if (u.target_id > 0) {
		const UnitState *target = _unit_by_id(u.target_id);
		if (target != nullptr && target->alive) {
			double dx = target->pos_x - u.pos_x;
			double dy = target->pos_y - u.pos_y;
			double distance = Math::sqrt(dx * dx + dy * dy);
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

// Optimized stat function implementations to replace massive if-else chains
void TeamfightSimulationCore::_apply_stat_max_hp(UnitState &unit, double value, bool is_multiplicative) {
	if (is_multiplicative) {
		unit.stat_multiplicative_max_hp *= value;
	} else {
		unit.stat_additive_max_hp += value;
	}
}

void TeamfightSimulationCore::_apply_stat_attack_damage(UnitState &unit, double value, bool is_multiplicative) {
	if (is_multiplicative) {
		unit.stat_multiplicative_attack_damage *= value;
	} else {
		unit.stat_additive_attack_damage += value;
	}
}

void TeamfightSimulationCore::_apply_stat_attack_speed(UnitState &unit, double value, bool is_multiplicative) {
	if (is_multiplicative) {
		unit.stat_multiplicative_attack_speed *= value;
	} else {
		unit.stat_additive_attack_speed += value;
	}
}

void TeamfightSimulationCore::_apply_stat_move_speed(UnitState &unit, double value, bool is_multiplicative) {
	if (is_multiplicative) {
		unit.stat_multiplicative_move_speed *= value;
	} else {
		unit.stat_additive_move_speed += value;
	}
}

void TeamfightSimulationCore::_apply_stat_armor(UnitState &unit, double value, bool is_multiplicative) {
	if (is_multiplicative) {
		unit.stat_multiplicative_armor *= value;
	} else {
		unit.stat_additive_armor += value;
	}
}

void TeamfightSimulationCore::_apply_stat_magic_resist(UnitState &unit, double value, bool is_multiplicative) {
	if (is_multiplicative) {
		unit.stat_multiplicative_magic_resist *= value;
	} else {
		unit.stat_additive_magic_resist += value;
	}
}

void TeamfightSimulationCore::_apply_stat_tenacity(UnitState &unit, double value, bool is_multiplicative) {
	if (is_multiplicative) {
		unit.stat_multiplicative_tenacity *= value;
	} else {
		unit.stat_additive_tenacity += value;
	}
}

void TeamfightSimulationCore::_apply_stat_life_steal(UnitState &unit, double value, bool is_multiplicative) {
	if (is_multiplicative) {
		unit.stat_multiplicative_life_steal *= value;
	} else {
		unit.stat_additive_life_steal += value;
	}
}

void TeamfightSimulationCore::_apply_stat_max_mana(UnitState &unit, double value, bool is_multiplicative) {
	if (is_multiplicative) {
		unit.stat_multiplicative_max_mana *= value;
	} else {
		unit.stat_additive_max_mana += value;
	}
}

void TeamfightSimulationCore::_apply_stat_mana_per_attack(UnitState &unit, double value, bool is_multiplicative) {
	if (is_multiplicative) {
		unit.stat_multiplicative_mana_per_attack *= value;
	} else {
		unit.stat_additive_mana_per_attack += value;
	}
}

void TeamfightSimulationCore::_apply_stat_ability_cd(UnitState &unit, double value, bool is_multiplicative) {
	if (is_multiplicative) {
		unit.stat_multiplicative_ability_cd *= value;
	} else {
		unit.stat_additive_ability_cd += value;
	}
}

void TeamfightSimulationCore::_apply_stat_projectile_speed(UnitState &unit, double value, bool is_multiplicative) {
	if (is_multiplicative) {
		unit.stat_multiplicative_projectile_speed *= value;
	} else {
		unit.stat_additive_projectile_speed += value;
	}
}

void TeamfightSimulationCore::_apply_stat_projectile_radius(UnitState &unit, double value, bool is_multiplicative) {
	if (is_multiplicative) {
		unit.stat_multiplicative_projectile_radius *= value;
	} else {
		unit.stat_additive_projectile_radius += value;
	}
}

void TeamfightSimulationCore::_apply_stat_respawn_time(UnitState &unit, double value, bool is_multiplicative) {
	if (is_multiplicative) {
		unit.stat_multiplicative_respawn_time *= value;
	} else {
		unit.stat_additive_respawn_time += value;
	}
}

void TeamfightSimulationCore::_apply_stat_attack_range(UnitState &unit, double value, bool is_multiplicative) {
	if (is_multiplicative) {
		unit.stat_multiplicative_attack_range *= value;
	} else {
		unit.stat_additive_attack_range += value;
	}
}

void TeamfightSimulationCore::_apply_stat_cast_range(UnitState &unit, double value, bool is_multiplicative) {
	if (is_multiplicative) {
		unit.stat_multiplicative_cast_range *= value;
	} else {
		unit.stat_additive_cast_range += value;
	}
}

// Optimized stack management functions
uint64_t TeamfightSimulationCore::_compute_stack_key(StringName stat_name, const String &reason) {
	// Use existing StringName hashes + reason hash for fast key generation
	return (static_cast<uint64_t>(stat_name.hash()) << 32) ^ 
		   static_cast<uint64_t>(reason.hash());
}

TeamfightSimulationCore::StackEntry* TeamfightSimulationCore::_allocate_stack_entry() {
	// Simple pool allocation - in production, use more sophisticated pool
	if (_stack_pool.size() < 1000) { // Prevent unbounded growth
		_stack_pool.emplace_back();
		return &_stack_pool.back();
	}
	// Pool full - allocate dynamically (should be rare)
	return new StackEntry();
}

void TeamfightSimulationCore::_free_stack_entry(StackEntry* entry) {
	// Simple cleanup - in production, return to pool
	if (entry >= &_stack_pool.front() && entry <= &_stack_pool.back()) {
		// Was from pool - mark as inactive
		entry->active = false;
	} else {
		// Was dynamic allocation
		delete entry;
	}
}

TeamfightSimulationCore::StackEntry* TeamfightSimulationCore::_get_stack_entry(UnitState &unit, uint64_t key_hash) {
	// Check if we already have this stack
	auto it = _stack_key_to_pool_index.find(key_hash);
	if (it != _stack_key_to_pool_index.end()) {
		int index = it->second;
		if (index >= 0 && index < _stack_pool.size()) {
			StackEntry* entry = &_stack_pool[index];
			if (entry->active && entry->key_hash == key_hash) {
				return entry;
			}
		}
	}
	return nullptr;
}

void TeamfightSimulationCore::_process_expiration_queue(double current_time) {
	// Process all expired stacks
	while (!_expiration_queue.empty() && _expiration_queue.top().expire_time <= current_time) {
		ExpirationEntry entry = _expiration_queue.top();
		_expiration_queue.pop();
		
		if (entry.unit != nullptr) {
			StackEntry* stack_entry = _get_stack_entry(*entry.unit, entry.stack_key_hash);
			if (stack_entry != nullptr) {
				// Remove expired effect from unit stats
				double total_effect = stack_entry->current_stacks * stack_entry->base_value;
				
				// Apply removal based on stat name (extract from key)
				StringName stat_name = StringName(); // This needs to be reconstructed from hash
				// For now, use existing logic in _update_stacks
				
				// Mark as inactive
				stack_entry->active = false;
			}
		}
	}
}

TeamfightSimulationCore::StackError TeamfightSimulationCore::_apply_stat_modifier_optimized(UnitState &source, UnitState &target, const StackParams &params, double duration, bool is_match_duration, double current_time) {
	// Validate parameters first
	if (!params.validate(this)) {
		return StackError::InvalidStat;
	}
	
	// Compute optimized key
	uint64_t key_hash = _compute_stack_key(params.stat_name, params.reason);
	
	// Get existing or allocate new entry
	StackEntry* entry = _get_stack_entry(target, key_hash);
	if (entry == nullptr) {
		entry = _allocate_stack_entry();
		entry->key_hash = key_hash;
		entry->max_stacks = params.max_stacks;
		entry->base_value = params.base_value;
		entry->is_multiplicative = (params.base_value == 0.0); // Simplified check
		entry->reason = params.reason;
		entry->stack_behavior = params.behavior;
		entry->current_stacks = 0;
		entry->duration = 0.0;
		_stack_key_to_pool_index[key_hash] = entry - &_stack_pool[0];
	}
	
	// Remove previous effect before applying new one
	if (entry->current_stacks > 0) {
		double previous_effect = entry->current_stacks * entry->base_value;
		bool is_multiplicative = entry->is_multiplicative;
		
		// Apply removal using function pointer table
		if (params.stat_name == StringName("max_hp")) {
			_apply_stat_max_hp(target, -previous_effect, is_multiplicative);
		} else if (params.stat_name == StringName("attack_damage")) {
			_apply_stat_attack_damage(target, -previous_effect, is_multiplicative);
		} else if (params.stat_name == StringName("attack_speed")) {
			_apply_stat_attack_speed(target, -previous_effect, is_multiplicative);
		} else if (params.stat_name == StringName("move_speed")) {
			_apply_stat_move_speed(target, -previous_effect, is_multiplicative);
		} else if (params.stat_name == StringName("armor")) {
			_apply_stat_armor(target, -previous_effect, is_multiplicative);
		} else if (params.stat_name == StringName("magic_resist")) {
			_apply_stat_magic_resist(target, -previous_effect, is_multiplicative);
		} else if (params.stat_name == StringName("tenacity")) {
			_apply_stat_tenacity(target, -previous_effect, is_multiplicative);
		} else if (params.stat_name == StringName("life_steal")) {
			_apply_stat_life_steal(target, -previous_effect, is_multiplicative);
		} else if (params.stat_name == StringName("max_mana")) {
			_apply_stat_max_mana(target, -previous_effect, is_multiplicative);
		} else if (params.stat_name == StringName("mana_per_attack")) {
			_apply_stat_mana_per_attack(target, -previous_effect, is_multiplicative);
		} else if (params.stat_name == StringName("ability_cd")) {
			_apply_stat_ability_cd(target, -previous_effect, is_multiplicative);
		} else if (params.stat_name == StringName("projectile_speed")) {
			_apply_stat_projectile_speed(target, -previous_effect, is_multiplicative);
		} else if (params.stat_name == StringName("projectile_radius")) {
			_apply_stat_projectile_radius(target, -previous_effect, is_multiplicative);
		} else if (params.stat_name == StringName("respawn_time")) {
			_apply_stat_respawn_time(target, -previous_effect, is_multiplicative);
		} else if (params.stat_name == StringName("attack_range")) {
			_apply_stat_attack_range(target, -previous_effect, is_multiplicative);
		} else if (params.stat_name == StringName("cast_range")) {
			_apply_stat_cast_range(target, -previous_effect, is_multiplicative);
		}
	}
	
	// Apply stacking behavior
	switch (params.behavior) {
		case StackBehavior::Refresh:
			entry->duration = duration;
			break;
		case StackBehavior::Accumulate:
			entry->duration += duration;
			break;
		case StackBehavior::Reset:
			entry->current_stacks = 0;
			entry->duration = duration;
			break;
	}
	
	// Update stacks
	if (entry->current_stacks < params.max_stacks) {
		entry->current_stacks++;
	}
	entry->last_applied_time = current_time;
	
	// Apply new effect using function pointer table
	double total_effect = entry->current_stacks * entry->base_value;
	bool is_multiplicative = entry->is_multiplicative;
	
	// Debug stack application
	_debug_log_stack_operation("APPLY_OPT", String(params.stat_name), 
		entry->current_stacks, entry->max_stacks, duration, params.reason);
	
	if (params.stat_name == StringName("max_hp")) {
		_apply_stat_max_hp(target, total_effect, is_multiplicative);
	} else if (params.stat_name == StringName("attack_damage")) {
		_apply_stat_attack_damage(target, total_effect, is_multiplicative);
	} else if (params.stat_name == StringName("attack_speed")) {
		_apply_stat_attack_speed(target, total_effect, is_multiplicative);
	} else if (params.stat_name == StringName("move_speed")) {
		_apply_stat_move_speed(target, total_effect, is_multiplicative);
	} else if (params.stat_name == StringName("armor")) {
		_apply_stat_armor(target, total_effect, is_multiplicative);
	} else if (params.stat_name == StringName("magic_resist")) {
		_apply_stat_magic_resist(target, total_effect, is_multiplicative);
	} else if (params.stat_name == StringName("tenacity")) {
		_apply_stat_tenacity(target, total_effect, is_multiplicative);
	} else if (params.stat_name == StringName("life_steal")) {
		_apply_stat_life_steal(target, total_effect, is_multiplicative);
	} else if (params.stat_name == StringName("max_mana")) {
		_apply_stat_max_mana(target, total_effect, is_multiplicative);
	} else if (params.stat_name == StringName("mana_per_attack")) {
		_apply_stat_mana_per_attack(target, total_effect, is_multiplicative);
	} else if (params.stat_name == StringName("ability_cd")) {
		_apply_stat_ability_cd(target, total_effect, is_multiplicative);
	} else if (params.stat_name == StringName("projectile_speed")) {
		_apply_stat_projectile_speed(target, total_effect, is_multiplicative);
	} else if (params.stat_name == StringName("projectile_radius")) {
		_apply_stat_projectile_radius(target, total_effect, is_multiplicative);
	} else if (params.stat_name == StringName("respawn_time")) {
		_apply_stat_respawn_time(target, total_effect, is_multiplicative);
	} else if (params.stat_name == StringName("attack_range")) {
		_apply_stat_attack_range(target, total_effect, is_multiplicative);
	} else if (params.stat_name == StringName("cast_range")) {
		_apply_stat_cast_range(target, total_effect, is_multiplicative);
	}
	
	// Add to expiration queue if duration > 0
	if (duration > 0.0 && !is_match_duration) {
		ExpirationEntry exp_entry;
		exp_entry.expire_time = current_time + duration;
		exp_entry.stack_key_hash = key_hash;
		exp_entry.unit = &target;
		_expiration_queue.push(exp_entry);
	}
	
	return TeamfightSimulationCore::StackError::None;
}

void TeamfightSimulationCore::_validate_stack_consistency(UnitState &unit) {
	// Validate stack state for debugging
	for (const auto& pair : _stack_key_to_pool_index) {
		int index = pair.second;
		if (index >= 0 && index < _stack_pool.size()) {
			StackEntry& entry = const_cast<StackEntry&>(_stack_pool[index]);
			if (entry.active) {
				// Validate stack invariants
				if (entry.current_stacks < 0 || entry.current_stacks > entry.max_stacks) {
					// Stack corruption detected
					entry.active = false;
				}
			}
		}
	}
}

// Define static member variables
thread_local std::vector<TeamfightSimulationCore::StackEntry> TeamfightSimulationCore::_stack_pool;
thread_local std::unordered_map<uint64_t, int> TeamfightSimulationCore::_stack_key_to_pool_index;
std::priority_queue<TeamfightSimulationCore::ExpirationEntry> TeamfightSimulationCore::_expiration_queue;

// Stack debugging implementations
void TeamfightSimulationCore::_debug_print_stack_state(const UnitState &unit) const {
	if (!_debug_stack_operations && !_debug_combat_trace) {
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

void TeamfightSimulationCore::_debug_log_stack_operation(const String &operation, const String &stat_name, int stacks, int max_stacks, double duration, const String &reason) const {
	if (!_debug_stack_operations && !_debug_combat_trace) {
		return;
	}
	
	String debug_msg = vformat("STACK_OP: %s | stat=%s | stacks=%d/%d | duration=%.2f | reason=%s",
		operation,
		stat_name,
		stacks,
		max_stacks,
		duration,
		reason
	);
	
	UtilityFunctions::push_warning(debug_msg);
}
