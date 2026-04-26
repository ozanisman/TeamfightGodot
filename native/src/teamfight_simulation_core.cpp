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
} // namespace

static double strategy_role_prio(const std::map<StringName, double> &m, const StringName &key) {
	auto it = m.find(key);
	return it == m.end() ? 0.0 : it->second;
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
	if (kind == StringName("self_damage")) {
		return EFFECT_OPCODE_SELF_DAMAGE;
	}
	if (kind == StringName("self_shield")) {
		return EFFECT_OPCODE_SELF_SHIELD;
	}
	if (kind == StringName("self_aoe_taunt")) {
		return EFFECT_OPCODE_SELF_AOE_TAUNT;
	}
	if (kind == StringName("self_aoe_damage")) {
		return EFFECT_OPCODE_SELF_AOE_DAMAGE;
	}
	if (kind == StringName("splash_damage")) {
		return EFFECT_OPCODE_SPLASH_DAMAGE;
	}
	if (kind == StringName("threshold_splash_damage")) {
		return EFFECT_OPCODE_THRESHOLD_SPLASH_DAMAGE;
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
	if (kind == StringName("dodge")) {
		return EFFECT_OPCODE_DODGE;
	}
	if (kind == StringName("constant_multiplier")) {
		return EFFECT_OPCODE_CONSTANT_MULTIPLIER;
	}
	if (kind == StringName("target_hp_threshold_multiplier")) {
		return EFFECT_OPCODE_TARGET_HP_THRESHOLD_MULTIPLIER;
	}
	if (kind == StringName("distance_threshold_multiplier")) {
		return EFFECT_OPCODE_DISTANCE_THRESHOLD_MULTIPLIER;
	}
	if (kind == StringName("self_hp_threshold_multiplier")) {
		return EFFECT_OPCODE_SELF_HP_THRESHOLD_MULTIPLIER;
	}
	return EFFECT_OPCODE_UNKNOWN;
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
	} else if (kind == StringName("target_hp_threshold_multiplier")) {
		compiled.scalar0 = double(params.get("hp_ratio_threshold", 0.0));
		compiled.scalar1 = double(params.get("multiplier", 1.0));
	} else if (kind == StringName("distance_threshold_multiplier")) {
		compiled.scalar0 = double(params.get("min_distance", 0.0));
		compiled.scalar1 = double(params.get("multiplier", 1.0));
	} else if (kind == StringName("self_hp_threshold_multiplier")) {
		compiled.scalar0 = double(params.get("min_hp_ratio", 0.0));
		compiled.scalar1 = double(params.get("multiplier", 1.0));
	} else if (kind == StringName("damage")) {
		compiled.scalar0 = double(params.get("damage_multiplier", 1.0));
		compiled.scalar1 = bool(params.get("trigger_on_hit", true)) ? 1.0 : 0.0;
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
		compiled.reason = String(params.get("reason", ""));
	} else if (kind == StringName("heal")) {
		compiled.scalar0 = double(params.get("max_hp_ratio", 0.0));
		compiled.scalar1 = double(params.get("current_hp_ratio", 0.0));
		compiled.scalar2 = double(params.get("flat_amount", 0.0));
		compiled.reason = String(params.get("reason", ""));
	} else if (kind == StringName("self_damage")) {
		compiled.scalar0 = double(params.get("damage_ratio", 0.0));
		compiled.damage_type = StringName(String(params.get("damage_type", "true")));
		compiled.reason = String(params.get("reason", ""));
	} else if (kind == StringName("self_shield")) {
		compiled.scalar0 = double(params.get("shield_ratio", 0.0));
		compiled.reason = String(params.get("reason", ""));
	} else if (kind == StringName("self_aoe_taunt")) {
		compiled.scalar0 = double(params.get("radius", 0.0));
		compiled.scalar1 = double(params.get("duration", 0.0));
		compiled.reason = String(params.get("reason", ""));
	} else if (kind == StringName("self_aoe_damage")) {
		compiled.scalar0 = double(params.get("radius", 0.0));
		compiled.scalar1 = double(params.get("damage_multiplier", 1.0));
		compiled.damage_type = StringName(String(params.get("damage_type", "physical")));
		compiled.reason = String(params.get("reason", ""));
	} else if (kind == StringName("splash_damage")) {
		compiled.scalar0 = double(params.get("radius", 0.0));
		compiled.scalar1 = double(params.get("ratio", 0.0));
		compiled.damage_type = StringName(String(params.get("damage_type", "physical")));
		compiled.reason = String(params.get("reason", "Splash"));
	} else if (kind == StringName("threshold_splash_damage")) {
		compiled.scalar0 = double(params.get("threshold_multiplier", 1.0));
		Variant nested = params.get("splash", Variant());
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
	} else if (kind == StringName("dodge")) {
		compiled.scalar0 = double(params.get("dodge_chance", 0.0));
		compiled.scalar1 = double(params.get("on_dodge_multiplier", 0.0));
		compiled.scalar2 = double(params.get("on_hit_multiplier", 1.0));
	}
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
	ClassDB::bind_method(D_METHOD("run_matches", "match_inputs"), &TeamfightSimulationCore::run_matches);
	ClassDB::bind_method(D_METHOD("run_match_simulation_only", "match_input"), &TeamfightSimulationCore::run_match_simulation_only);
	ClassDB::bind_method(D_METHOD("run_matches_simulation_only", "match_inputs"), &TeamfightSimulationCore::run_matches_simulation_only);
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
	_units.clear();
	_projectiles.clear();
	_scratch_projectiles.clear();
	_scratch_critical_allies.clear();
	_summary_unit_stats.clear();
	_summary_cache.clear();
	_time = 0.0;
	_tick = 0;
	_tick_rate = DEFAULT_TICK_RATE;
	_seed = 0;
	_winner_team = StringName("draw");
	_record_events = false;
	_player_comp.clear();
	_enemy_comp.clear();
	_player_kills = 0;
	_enemy_kills = 0;
	_unit_index_map.clear();
	_alive_player_indices.clear();
	_alive_enemy_indices.clear();
	_role_strategy_cache.clear();
	_default_strategy = UnitStrategy();
	_tick_ctx.density_by_unit_index.clear();
	_tick_ctx.player_backliner_indices.clear();
	_tick_ctx.enemy_backliner_indices.clear();
	_tick_ctx.has_player_center = false;
	_tick_ctx.has_enemy_center = false;
	_tick_ctx.needs_cluster_density = false;
	_trace_buffer.clear();
	_debug_combat_trace = false;
	_viewer_fx_events.clear();
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

	// Optional ability kits (preset ability/ultimate/passive swaps).
	_ability_kits = Dictionary();
	Dictionary kits_root = _load_json_file_if_exists(String(ABILITY_KITS_PATH));
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
		if (Dictionary(_passive_registry.get(pid, Dictionary())).is_empty()) {
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
		UnitState unit = _build_unit_state(spawn_spec, team, next_instance_id);
		if (unit.instance_id == 0) {
			continue;
		}
		int64_t unit_index = int64_t(_units.size());
		_units.push_back(unit);
		_unit_index_map[next_instance_id] = unit_index;
		_add_alive_index(team, unit_index);
		team_comp.append(unit.archetype_id);
		++next_instance_id;
	}
}

TeamfightSimulationCore::UnitState TeamfightSimulationCore::_build_unit_state(const Dictionary &spawn_spec, const StringName &team, int64_t instance_id) {
	UnitState unit;
	StringName archetype_id = StringName(String(spawn_spec.get("archetype_id", "")));
	Dictionary champion = _effective_champion_for(archetype_id);
	if (champion.is_empty()) {
		UtilityFunctions::push_error(vformat("Unknown champion archetype: %s", String(archetype_id)));
		return unit;
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
		for (int64_t kind_index = 0; kind_index < effect_kinds.size(); ++kind_index) {
			Variant kind_value = effect_kinds[kind_index];
			Array effects = Array(entry.get(kind_value, Array()));
			std::vector<EffectRecord> compiled_effects = _compile_effect_array(effects);
			std::vector<EffectRecord> &bucket = unit.passive_effects[passive_bucket_index(StringName(String(kind_value)))];
			bucket.insert(bucket.end(), compiled_effects.begin(), compiled_effects.end());
		}
	}
	Variant role_tick = role_config.get("passive_on_tick", Variant());
	if (role_tick.get_type() != Variant::NIL) {
		unit.passive_effects[2].push_back(_compile_effect(Dictionary(role_tick)));
	}
	Variant role_take_damage = role_config.get("passive_post_take_damage", Variant());
	if (role_take_damage.get_type() != Variant::NIL) {
		unit.passive_effects[4].push_back(_compile_effect(Dictionary(role_take_damage)));
	}

	double max_hp = double(stats.get("max_hp", 0.0));
	double max_mana = double(stats.get("max_mana", 0.0));
	double x = double(spawn_spec.get("x", 0.0));
	double y = double(spawn_spec.get("y", 0.0));

	unit.instance_id = instance_id;
	unit.archetype_id = archetype_id;
	unit.team = team;
	unit.role_id = role_name;
	unit.stats = stats;
	unit.combat.max_hp = max_hp;
	unit.combat.max_mana = max_mana;
	unit.combat.ability_cd = double(stats.get("ability_cd", 0.0));
	unit.combat.ultimate_cd = double(stats.get("ultimate_cd", 0.0));
	unit.combat.attack_range = double(stats.get("attack_range", 0.0));
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
	if (unit.has_ability_effect) {
		unit.ability_effect = _compile_effect(Dictionary(ability_effect));
	}
	if (unit.has_ultimate_effect) {
		unit.ultimate_effect = _compile_effect(Dictionary(ultimate_effect));
	}
	unit.spawn_pos_x = x;
	unit.spawn_pos_y = y;
	unit.pos_x = x;
	unit.pos_y = y;
	unit.hp = max_hp;
	unit.shield = 0.0;
	unit.mana = 0.0;
	unit.attack_cooldown = 0.0;
	unit.ability_cooldown = unit.combat.ability_cd;
	// Match golden fixtures: ultimates start ready (mana-gated), not on their full cooldown.
	unit.ultimate_cooldown = 0.0;
	unit.casting_remaining = 0.0;
	unit.casting_kind = StringName();
	unit.casting_effect = EffectRecord();
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
	unit.respawn_timer = 0.0;
	unit.respawned_this_tick = false;
	unit.alive = true;
	unit.incoming_target_count = 0;
	unit.perceived_threat = 0.0;
	unit.attack_count = 0;
	unit.damage_dealt = 0.0;
	unit.damage_dealt_auto = 0.0;
	unit.damage_dealt_ability = 0.0;
	unit.damage_dealt_ultimate = 0.0;
	unit.damage_received = 0.0;
	unit.damage_mitigated = 0.0;
	unit.healing_done = 0.0;
	unit.shielding_done = 0.0;
	unit.auto_attacks = 0;
	unit.abilities = 0;
	unit.ultimates = 0;
	unit.stuns = 0;
	unit.kills = 0;
	unit.deaths = 0;
	unit.assists = 0;
	unit.taunt_target_id = 0;
	unit.taunt_remaining = 0.0;
	unit.forced_target_id = 0;
	unit.forced_target_remaining = 0.0;
	unit.forced_target_kind = StringName();
	unit.damage_sources.clear();
	unit.recent_benefactors.clear();
	unit.last_hit_time = 0.0;
	unit.regen_accumulator = 0.0;
	return unit;
}

void TeamfightSimulationCore::_build_role_strategy_cache() {
	_role_strategy_cache.clear();
	auto put = [&](const StringName &role, UnitStrategy s) {
		_role_strategy_cache.emplace(role, std::move(s));
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
	};

	{
		UnitStrategy s;
		s.display_name = String("Protector");
		s.distance_weight = DISTANCE_WEIGHT_TANK;
		s.hp_weight = 0.0;
		s.ally_distance_weight = 1.0;
		s.ally_hp_weight = 0.0;
		s.ally_threat_weight = SCORE_THREAT_WEIGHT_SCALE * SCORE_THREAT_WEIGHT_SCALE;
		s.ally_role_priorities[StringName("marksman")] = -5.0 * ROLE_PRIORITY_GLOBAL_SCALE;
		s.ally_role_priorities[StringName("mage")] = -5.0 * ROLE_PRIORITY_GLOBAL_SCALE;
		s.ally_role_priorities[StringName("support")] = -3.0 * ROLE_PRIORITY_GLOBAL_SCALE;
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
		s.role_priorities[StringName("assassin")] = -5.0 * ROLE_PRIORITY_GLOBAL_SCALE;
		s.role_priorities[StringName("fighter")] = -2.0 * ROLE_PRIORITY_GLOBAL_SCALE;
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
		s.role_priorities[StringName("marksman")] = -15.0 * ROLE_PRIORITY_GLOBAL_SCALE;
		s.role_priorities[StringName("mage")] = -15.0 * ROLE_PRIORITY_GLOBAL_SCALE;
		s.role_priorities[StringName("support")] = -10.0 * ROLE_PRIORITY_GLOBAL_SCALE;
		s.role_priorities[StringName("fighter")] = 10.0 * ROLE_PRIORITY_GLOBAL_SCALE;
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
		s.role_priorities[StringName("marksman")] = -1.0 * ROLE_PRIORITY_GLOBAL_SCALE;
		s.role_priorities[StringName("mage")] = -1.0 * ROLE_PRIORITY_GLOBAL_SCALE;
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
		s.role_priorities[StringName("marksman")] = -4.0 * ROLE_PRIORITY_GLOBAL_SCALE;
		s.role_priorities[StringName("support")] = -2.0 * ROLE_PRIORITY_GLOBAL_SCALE;
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
		s.ally_role_priorities[StringName("marksman")] = -5.0 * ROLE_PRIORITY_GLOBAL_SCALE;
		s.ally_role_priorities[StringName("mage")] = -5.0 * ROLE_PRIORITY_GLOBAL_SCALE;
		s.ally_role_priorities[StringName("fighter")] = -2.0 * ROLE_PRIORITY_GLOBAL_SCALE;
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
		s.role_priorities[StringName("assassin")] = -8.0 * ROLE_PRIORITY_GLOBAL_SCALE;
		s.role_priorities[StringName("fighter")] = -4.0 * ROLE_PRIORITY_GLOBAL_SCALE;
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
	StringName role = unit.role_id;
	auto it = _role_strategy_cache.find(role);
	if (it != _role_strategy_cache.end()) {
		return it->second;
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
		if (!_tick_ctx.needs_cluster_density && _strategy_for_unit(u).cluster_weight > 0.0) {
			_tick_ctx.needs_cluster_density = true;
		}
		pcx += u.pos_x;
		pcy += u.pos_y;
		pc += 1;
		if (u.role_id == StringName("marksman") || u.role_id == StringName("mage") || u.role_id == StringName("support")) {
			_tick_ctx.player_backliner_indices.push_back(idx);
		}
		if (u.role_id == sn_tank() || u.role_id == sn_fighter()) {
			_tick_ctx.player_frontline_indices.push_back(idx);
		}
		if (u.role_id == sn_marksman() || u.role_id == sn_mage()) {
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
		if (!_tick_ctx.needs_cluster_density && _strategy_for_unit(u).cluster_weight > 0.0) {
			_tick_ctx.needs_cluster_density = true;
		}
		ecx += u.pos_x;
		ecy += u.pos_y;
		ec += 1;
		if (u.role_id == StringName("marksman") || u.role_id == StringName("mage") || u.role_id == StringName("support")) {
			_tick_ctx.enemy_backliner_indices.push_back(idx);
		}
		if (u.role_id == sn_tank() || u.role_id == sn_fighter()) {
			_tick_ctx.enemy_frontline_indices.push_back(idx);
		}
		if (u.role_id == sn_marksman() || u.role_id == sn_mage()) {
			_tick_ctx.enemy_carry_indices.push_back(idx);
		}
	}
	_tick_ctx.has_enemy_center = ec > 0;
	if (ec > 0) {
		_tick_ctx.enemy_team_center = Vector2(ecx / double(ec), ecy / double(ec));
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

void TeamfightSimulationCore::_spatial_rebuild_all_alive() const {
	_spatial_clear_buckets();
	_spatial_add_alive_team(StringName("player"));
	_spatial_add_alive_team(StringName("enemy"));
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
				double ally_dist = Math::sqrt(ox * ox + oy * oy);
				if (ally_dist < radius) {
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

int TeamfightSimulationCore::_spatial_count_obscurance_blockers(double ux, double uy, double tx, double ty, const std::vector<int64_t> &enemy_indices, int64_t target_instance_id) const {
	_spatial_clear_buckets_aux();
	for (int64_t idx : enemy_indices) {
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
				if (other.role_id != sn_tank() && other.role_id != sn_fighter()) {
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

double TeamfightSimulationCore::_score_ally_target(const UnitState &unit, const UnitState &ally, const TeamfightSimulationCore::UnitStrategy &strategy, double unit_ally_distance) const {
	double dist = unit_ally_distance >= 0.0 ? unit_ally_distance : _distance_between(unit, ally);
	double hp_ratio = ally.hp / Math::max(0.0001, ally.combat.max_hp);
	double score = dist * strategy.ally_distance_weight;
	score += hp_ratio * strategy.ally_hp_weight * SCORE_HP_WEIGHT_SCALE;
	score -= ally.perceived_threat * strategy.ally_threat_weight;
	score += strategy_role_prio(strategy.ally_role_priorities, ally.role_id);
	return score;
}

double TeamfightSimulationCore::_score_enemy_target(const UnitState &attacker, const UnitState &enemy, const TeamfightSimulationCore::UnitStrategy &strategy, const TeamfightSimulationCore::TickContext &ctx, double attacker_enemy_distance, bool profile_score) {
	if (profile_score) {
		_sim_profile_se_calls += 1;
	}
	const std::vector<int64_t> &carry_indices = attacker.team == sn_player() ? ctx.player_carry_indices : ctx.enemy_carry_indices;
	const std::vector<int64_t> &frontline_indices = enemy.team == sn_player() ? ctx.player_frontline_indices : ctx.enemy_frontline_indices;

	double score = 0.0;
	double dist = 0.0;
	double attack_range = 0.0;
	double effective_range = 0.0;
	{
		SimProfileAccScope _se_base(profile_score, _sim_profile_se_base);
		dist = attacker_enemy_distance >= 0.0 ? attacker_enemy_distance : _distance_between(attacker, enemy);
		attack_range = _attack_range(attacker);
		effective_range = _effective_attack_range(attacker);
		// Python parity: melee contact uses abs tolerance only (math.isclose rel_tol=0, abs_tol=MELEE_CONTACT_BUFFER).
		bool in_range = false;
		if (attack_range > RANGED_THRESHOLD) {
			in_range = dist <= attack_range;
		} else {
			in_range = (dist <= effective_range) || (Math::abs(dist - effective_range) <= MELEE_CONTACT_BUFFER);
		}
		double hp_ratio = enemy.hp / Math::max(0.0001, enemy.combat.max_hp);
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
		score += strategy_role_prio(strategy.role_priorities, enemy.role_id);
		if (enemy.role_id == sn_tank()) {
			score += strategy.tank_penalty;
			// Python oracle: assassins apply an extra tank penalty if there are backliners alive.
			if (attacker.role_id == sn_assassin()) {
				int64_t enemy_self_idx = _unit_index_by_id(enemy.instance_id);
				const std::vector<int64_t> &bl = enemy.team == sn_player() ? ctx.player_backliner_indices : ctx.enemy_backliner_indices;
				bool has_backliner = false;
				for (int64_t idx : bl) {
					if (idx == enemy_self_idx) {
						continue;
					}
					const UnitState &other = _units[idx];
					if (other.alive) {
						has_backliner = true;
						break;
					}
				}
				if (has_backliner) {
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
		StringName enemy_role = enemy.role_id;
		if (enemy_role == sn_tank() || enemy_role == sn_fighter()) {
			prey_focus *= PREY_FRONTLINE_SCALE;
		}
		score -= prey_focus * strategy.prey_instinct_weight;
		if (attacker.target_id == enemy.instance_id) {
			score -= Math::max(1.0, strategy.distance_weight) * strategy.stickiness_bonus;
		}
		if (attacker.role_id == sn_support()) {
			int64_t ally_target_id = attacker.current_ally_target_id;
			if (ally_target_id != 0) {
				const UnitState *ally = _unit_by_id(ally_target_id);
				if (ally != nullptr && ally->alive) {
					double ally_incoming = double(ally->incoming_target_count);
					double ally_threat = ally->perceived_threat;
					if ((ally_incoming >= SUPPORT_PEEL_THREAT_THRESHOLD || ally_threat >= SUPPORT_PEEL_THREAT_THRESHOLD) && enemy.target_id == ally_target_id) {
						score -= SUPPORT_PEEL_BOOST;
					}
				}
			}
		}
	}

	{
		SimProfileAccScope _se_bg(profile_score, _sim_profile_se_bodyguard);
		double bodyguard_weight = strategy.bodyguard_weight;
		if (bodyguard_weight > 0.0) {
			if (_use_spatial_broad_phase()) {
				_spatial_stamp_circle(enemy.pos_x, enemy.pos_y, BODYGUARD_RADIUS, attacker.team);
				const double bodyguard_r2 = BODYGUARD_RADIUS * BODYGUARD_RADIUS;
				for (int64_t ally_index : carry_indices) {
					if (!_spatial_stamp_has(ally_index)) {
						continue;
					}
					const UnitState &ally = _units[ally_index];
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
			} else {
				const double bodyguard_r2 = BODYGUARD_RADIUS * BODYGUARD_RADIUS;
				for (int64_t ally_index : carry_indices) {
					const UnitState &ally = _units[ally_index];
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
	}

	{
		SimProfileAccScope _se_base2(profile_score, _sim_profile_se_base);
		// Cluster awareness (density).
		double cluster_weight = strategy.cluster_weight;
		if (cluster_weight > 0.0) {
			int64_t enemy_idx = _unit_index_by_id(enemy.instance_id);
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
			if ((attacker.role_id == sn_marksman() || attacker.role_id == sn_mage()) && enemy.target_id == attacker.instance_id) {
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
		if (strategy.prefers_kiting) {
			double min_w = effective_range * KITE_TARGET_WINDOW_MIN_FACTOR;
			double max_w = effective_range * KITE_TARGET_WINDOW_MAX_FACTOR;
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
			if (_use_spatial_broad_phase()) {
				blockers = _spatial_count_obscurance_blockers(attacker.pos_x, attacker.pos_y, enemy.pos_x, enemy.pos_y, frontline_indices, enemy.instance_id);
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
			if (enemy.team == sn_player()) {
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
				double len_t = Math::sqrt(to_tx * to_tx + to_ty * to_ty);
				double len_a = Math::sqrt(to_ax * to_ax + to_ay * to_ay);
				if (len_t > EPSILON && len_a > EPSILON) {
					double align = (to_tx * to_ax + to_ty * to_ay) / (len_t * len_a);
					align = Math::max(0.0, align);
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

bool TeamfightSimulationCore::_should_switch(const UnitState &unit, double current_score, double new_score, const TeamfightSimulationCore::UnitStrategy &strategy) const {
	// Python oracle parity: respect post-switch lock + commit window to avoid last-moment target flip.
	if (unit.target_switch_lock_timer > 0.0) {
		return false;
	}
	// Commit window: avoid switching right before an in-range attack swing fires.
	// We approximate "in_range" using current target distance if available.
	const UnitState *current_target = _unit_by_id(unit.target_id);
	if (current_target != nullptr && current_target->alive && current_target->team != unit.team) {
		double dist = _distance_between(unit, *current_target);
		double attack_range = _attack_range(unit);
		double effective_range = _effective_attack_range(unit);
		bool in_range = false;
		if (attack_range > RANGED_THRESHOLD) {
			in_range = dist <= attack_range;
		} else {
			in_range = (dist <= effective_range) || (Math::abs(dist - effective_range) <= MELEE_CONTACT_BUFFER);
		}
		if (in_range) {
			double attack_speed = Math::max(0.0001, unit.combat.attack_speed);
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
		}
	}
	if (new_target_id != 0) {
		UnitState *new_unit = _unit_by_id(new_target_id);
		if (new_unit != nullptr) {
			new_unit->incoming_target_count += 1;
		}
	}
}

void TeamfightSimulationCore::_refresh_target_pressure(bool update_cluster_density) {
	for (int64_t index : _alive_player_indices) {
		_units[index].incoming_target_count = 0;
	}
	for (int64_t index : _alive_enemy_indices) {
		_units[index].incoming_target_count = 0;
	}
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
	_refresh_target_pressure();
}

TeamfightSimulationCore::EffectContext TeamfightSimulationCore::_build_context(UnitState &source, UnitState *target, UnitState *target_ally, double damage, const StringName &action_kind) {
	EffectContext context;
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
	if (kind == StringName("on_attack")) {
		return unit.passive_effects[0];
	}
	if (kind == StringName("on_defense")) {
		return unit.passive_effects[1];
	}
	if (kind == StringName("on_tick")) {
		return unit.passive_effects[2];
	}
	if (kind == StringName("post_attack")) {
		return unit.passive_effects[3];
	}
	if (kind == StringName("post_take_damage")) {
		return unit.passive_effects[4];
	}
	return EMPTY_EFFECTS;
}

double TeamfightSimulationCore::_evaluate_multiplier_effect(const EffectRecord &effect, const EffectContext &context, double current_value) {
	(void)current_value;
	switch (effect.opcode) {
		case EFFECT_OPCODE_CONSTANT_MULTIPLIER:
			return effect.scalar0;
		case EFFECT_OPCODE_TARGET_HP_THRESHOLD_MULTIPLIER: {
			if (context.target == nullptr) {
				return 1.0;
			}
			double target_hp = context.target->hp;
			double target_max_hp = Math::max(0.0001, context.target->combat.max_hp);
			if (target_hp / target_max_hp < effect.scalar0) {
				return effect.scalar1;
			}
			return 1.0;
		}
		case EFFECT_OPCODE_DISTANCE_THRESHOLD_MULTIPLIER:
			return context.distance > effect.scalar0 ? effect.scalar1 : 1.0;
		case EFFECT_OPCODE_SELF_HP_THRESHOLD_MULTIPLIER: {
			if (context.source == nullptr) {
				return 1.0;
			}
			double hp_ratio = context.source->hp / Math::max(0.0001, context.source->combat.max_hp);
			return hp_ratio > effect.scalar0 ? effect.scalar1 : 1.0;
		}
		case EFFECT_OPCODE_DODGE:
			if (_randf() < effect.scalar0) {
				return effect.scalar1;
			}
			return effect.scalar2;
		default:
			return 1.0;
	}
}

double TeamfightSimulationCore::_defense_multiplier(UnitState &target, UnitState &source, double damage, const StringName &action_kind) {
	double multiplier = 1.0;
	EffectContext context = _build_context(source, &target, nullptr, damage, action_kind);
	const std::vector<EffectRecord> &effects = _collect_effects(target, StringName("on_defense"));
	for (const EffectRecord &effect : effects) {
		multiplier *= _evaluate_multiplier_effect(effect, context, multiplier);
	}
	return multiplier;
}

double TeamfightSimulationCore::_apply_attack_modifiers(UnitState &unit, UnitState &target, double distance, double damage) {
	(void)distance;
	EffectContext context = _build_context(unit, &target, nullptr, damage, StringName("auto"));
	const std::vector<EffectRecord> &effects = _collect_effects(unit, StringName("on_attack"));
	double modified_damage = damage;
	for (const EffectRecord &effect : effects) {
		modified_damage *= _evaluate_multiplier_effect(effect, context, modified_damage);
	}
	return modified_damage;
}

double TeamfightSimulationCore::_apply_damage(UnitState &source, UnitState &target, double damage, const StringName &damage_type, const StringName &action_kind, const EffectContext &context) {
	if (!target.alive) {
		return 0.0;
	}
	double pre_res = damage * _defense_multiplier(target, source, damage, action_kind);
	double final_damage = pre_res;
	if (damage_type == StringName("physical")) {
		double armor = target.combat.armor;
		final_damage *= Math::clamp(1.0 - armor, 0.05, 1.0);
	} else if (damage_type == StringName("magic")) {
		double mr = target.combat.magic_resist;
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
	target.damage_received += hp_loss;
	target.damage_mitigated += Math::max(0.0, pre_res - final_damage);
	double total_damage = absorbed + hp_loss;
	double max_hp = target.combat.max_hp;
	// Python parity: threat burst uses post-shield hp_loss (final_damage after absorption).
	if (max_hp > 0.0 && hp_loss > max_hp * THREAT_BURST_THRESHOLD) {
		target.perceived_threat += (hp_loss / max_hp) * THREAT_BURST_MULTIPLIER;
	}
	// Self-inflicted damage should not count as damage dealt.
	if (source.instance_id != target.instance_id) {
		source.damage_dealt += total_damage;
		if (action_kind == StringName("auto")) {
			source.damage_dealt_auto += total_damage;
		} else if (action_kind == StringName("ability")) {
			source.damage_dealt_ability += total_damage;
		} else if (action_kind == StringName("ultimate")) {
			source.damage_dealt_ultimate += total_damage;
		}
	}
	if (hp_loss > 0.0 || absorbed > 0.0) {
		_touch_damage_source(target, source.instance_id, total_damage);
		target.last_hit_time = _time;
	}
	if (total_damage > 1e-9) {
		_viewer_record_damage_fx(source, target, total_damage, action_kind);
	}
	if (target.hp <= 0.0) {
		const std::vector<EffectRecord> &post_take_damage_effects = _collect_effects(target, StringName("post_take_damage"));
		EffectContext post_context = context;
		// Python parity: post_take_damage passives run in the defender's own context (ctx.unit = defender).
		post_context.source = &target;
		post_context.target = nullptr;
		post_context.damage = total_damage;
		for (const EffectRecord &effect : post_take_damage_effects) {
			_execute_effect(effect, post_context);
		}
		_handle_death(source, target);
		return total_damage;
	}
	const std::vector<EffectRecord> &post_take_damage_effects = _collect_effects(target, StringName("post_take_damage"));
	EffectContext post_context = context;
	// Python parity: post_take_damage passives run in the defender's own context (ctx.unit = defender).
	post_context.source = &target;
	post_context.target = nullptr;
	post_context.damage = total_damage;
	for (const EffectRecord &effect : post_take_damage_effects) {
		_execute_effect(effect, post_context);
	}
	return total_damage;
}

void TeamfightSimulationCore::_touch_damage_source(UnitState &target, int64_t source_id, double incoming_damage) {
	if (source_id <= 0 || incoming_damage <= 0.0) {
		return;
	}
	UnitState::DamageSourceEntry &entry = target.damage_sources[source_id];
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
	source.stuns += 1;
}

void TeamfightSimulationCore::_add_shield(UnitState &source, UnitState &target, double amount) {
	if (amount <= 0.0) {
		return;
	}
	target.shield += amount;
	if (amount > 1e-9) {
		_viewer_record_shield_fx(target, amount);
	}
	source.shielding_done += amount;
	if (source.instance_id != target.instance_id) {
		target.recent_benefactors[source.instance_id] = _time;
	}
}

void TeamfightSimulationCore::_heal_unit(UnitState &source, UnitState &target, double amount) {
	if (amount <= 0.0) {
		return;
	}
	double max_hp = target.combat.max_hp;
	double old_hp = target.hp;
	double new_hp = Math::min(max_hp, old_hp + amount);
	target.hp = new_hp;
	double gained = new_hp - old_hp;
	if (gained > 1e-9) {
		_viewer_record_heal_fx(target, gained);
	}
	source.healing_done += amount;
	if (source.instance_id != target.instance_id) {
		target.recent_benefactors[source.instance_id] = _time;
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

void TeamfightSimulationCore::_run_post_attack_effects(UnitState &source, UnitState &target, double damage, const EffectContext &context) {
	const std::vector<EffectRecord> &post_attack_effects = _collect_effects(source, StringName("post_attack"));
	if (post_attack_effects.empty()) {
		return;
	}
	EffectContext effect_context = context;
	effect_context.damage = damage;
	for (const EffectRecord &effect : post_attack_effects) {
		_execute_effect(effect, effect_context);
	}
}

void TeamfightSimulationCore::_apply_splash_damage(UnitState &source, UnitState &target, double damage, double radius, const StringName &damage_type, const StringName &action_kind, const String &reason, double splash_ratio) {
	(void)reason;
	if (radius <= 0.0) {
		return;
	}
	_viewer_record_aoe_ring_fx(source, target, radius, StringName("aoe_splash"));
	StringName source_team = source.team;
	StringName enemy_team = source_team == StringName("player") ? StringName("enemy") : StringName("player");
	const std::vector<int64_t> &enemy_indices = _alive_indices_for_team(enemy_team);
	for (int64_t enemy_index : enemy_indices) {
		UnitState &unit = _units[enemy_index];
		if (unit.instance_id == target.instance_id) {
			continue;
		}
		if (_distance_between(target, unit) <= radius) {
			double splash_damage = damage * splash_ratio;
			EffectContext context = _build_context(source, &unit, nullptr, splash_damage, action_kind);
			_apply_damage(source, unit, splash_damage, damage_type, action_kind, context);
		}
	}
}

void TeamfightSimulationCore::_apply_aoe_taunt(UnitState &source, double radius, double duration) {
	_viewer_record_aoe_ring_fx(source, source, radius, StringName("aoe_taunt"));
	StringName source_team = source.team;
	StringName enemy_team = source_team == StringName("player") ? StringName("enemy") : StringName("player");
	const std::vector<int64_t> &enemy_indices = _alive_indices_for_team(enemy_team);
	for (int64_t enemy_index : enemy_indices) {
		UnitState &unit = _units[enemy_index];
		if (_distance_between(source, unit) <= radius) {
			unit.taunt_target_id = source.instance_id;
			unit.taunt_remaining = Math::max(unit.taunt_remaining, duration);
			// Python forced-target model: taunt is a forced target.
			unit.forced_target_id = source.instance_id;
			unit.forced_target_remaining = Math::max(unit.forced_target_remaining, duration);
			unit.forced_target_kind = StringName("taunt");
		}
	}
}

void TeamfightSimulationCore::_apply_aoe_damage(UnitState &source, UnitState &center_source, double damage, double radius, const StringName &damage_type, const String &reason, const StringName &action_kind) {
	(void)reason;
	_viewer_record_aoe_ring_fx(source, center_source, radius, StringName("aoe_damage"));
	StringName source_team = source.team;
	StringName enemy_team = source_team == StringName("player") ? StringName("enemy") : StringName("player");
	const std::vector<int64_t> &enemy_indices = _alive_indices_for_team(enemy_team);
	for (int64_t enemy_index : enemy_indices) {
		UnitState &unit = _units[enemy_index];
		if (_distance_between(center_source, unit) <= radius) {
			EffectContext context = _build_context(source, &unit, nullptr, damage, action_kind);
			_apply_damage(source, unit, damage, damage_type, action_kind, context);
		}
	}
}

TeamfightSimulationCore::UnitState *TeamfightSimulationCore::_select_enemy_target(UnitState &unit, bool profile_sim) {
	auto bucket_rank_for = [&](const UnitStrategy &strat, const StringName &bucket) -> int {
		for (int i = 0; i < strat.bucket_order_len; ++i) {
			if (strat.bucket_order[static_cast<size_t>(i)] == bucket) {
				return i;
			}
		}
		return strat.bucket_order_len;
	};
	auto is_under_threat = [&](const UnitState &u) -> bool {
		return double(u.incoming_target_count) >= SUPPORT_PEEL_THREAT_THRESHOLD || u.perceived_threat >= SUPPORT_PEEL_THREAT_THRESHOLD;
	};
	auto classify_bucket = [&](const UnitState &candidate, double dist, const UnitStrategy &strat) -> StringName {
		// Python forced-target model: forced target is the COMMIT bucket.
		if (unit.forced_target_id != 0 && unit.forced_target_remaining > 0.0 && candidate.instance_id == unit.forced_target_id) {
			return StringName("commit");
		}
		StringName bucket = StringName("objective");
		double carry_peel_weight = strat.carry_peel_weight;
		// Self-peel for carries: if the enemy is targeting me, bucket as peel.
		if (carry_peel_weight > 0.0 && candidate.target_id == unit.instance_id) {
			return StringName("peel");
		}
		// Ally peel: if our selected ally is under threat and being focused by this enemy.
		// (Python: applies when unit.current_ally_target is set; tanks and supports can do this.)
		int64_t ally_id = unit.current_ally_target_id;
		if (ally_id != 0) {
			const UnitState *ally = _unit_by_id(ally_id);
			if (ally != nullptr && ally->alive && candidate.target_id == ally_id && is_under_threat(*ally)) {
				return StringName("peel");
			}
		}
		// Burst bucket (execute-ish).
		if (strat.execute_bonus_weight > 0.0) {
			double hp_ratio = candidate.hp / Math::max(0.0001, candidate.combat.max_hp);
			double atk_dmg = unit.combat.attack_damage;
			if (hp_ratio <= TARGET_EXECUTE_HP_RATIO || candidate.hp <= atk_dmg) {
				return StringName("burst");
			}
		}
		// Kite bucket.
		if (strat.prefers_kiting && !is_under_threat(unit)) {
			double effective_range = _effective_attack_range(unit);
			double min_w = effective_range * KITE_TARGET_WINDOW_MIN_FACTOR;
			double max_w = effective_range * KITE_TARGET_WINDOW_MAX_FACTOR;
			if (dist >= min_w && dist <= max_w) {
				return StringName("kite");
			}
		}
		return bucket;
	};

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
	UnitState *current_target = _unit_by_id(unit.target_id);
	bool forced_reason = true;
	if (current_target != nullptr && current_target->alive && current_target->team != unit.team) {
		forced_reason = false;
	}

	const UnitStrategy &strategy = _strategy_for_unit(unit);
	const TickContext &ctx = _tick_ctx;
	UnitState *best = nullptr;
	double best_adjusted = std::numeric_limits<double>::infinity();
	double best_raw = std::numeric_limits<double>::infinity();
	int best_bucket_rank = 0;
	double best_dist = std::numeric_limits<double>::infinity();
	const StringName &enemy_team = unit.team == sn_player() ? sn_enemy() : sn_player();
	const std::vector<int64_t> &enemy_indices = _alive_indices_for_team(enemy_team);
	if (_use_spatial_broad_phase()) {
		_spatial_rebuild_all_alive();
	}

	// Assassin frontline pressure bypass: evaluate even if retarget_timer > 0.
	bool assassin_pressuring_frontline = false;
	if (!forced_reason && unit.role_id == sn_assassin() && current_target != nullptr) {
		StringName cur_role = current_target->role_id;
		if (cur_role == sn_tank() || cur_role == sn_fighter()) {
			const std::vector<int64_t> &bl = enemy_team == sn_player() ? _tick_ctx.player_backliner_indices : _tick_ctx.enemy_backliner_indices;
			for (int64_t idx : bl) {
				const UnitState &enemy = _units[idx];
				if (enemy.alive) {
					assassin_pressuring_frontline = true;
					break;
				}
			}
		}
	}

	// Periodic keep path: if we are within retarget interval and no special conditions, just keep current target.
	if (!forced_reason && unit.retarget_timer > 0.0 && !assassin_pressuring_frontline) {
		// Match Python: skip evaluation entirely; just return current target score (no switching).
		double keep_dist = _distance_between(unit, *current_target);
		unit.current_target_score = _score_enemy_target(unit, *current_target, strategy, ctx, keep_dist, profile_sim);
		_set_current_target(unit, *current_target);
		return current_target;
	}

	// Python: whenever we do evaluate, we reset the retarget timer immediately (even if we end up keeping).
	unit.retarget_timer = RETARGET_INTERVAL;

	for (int64_t enemy_index : enemy_indices) {
		UnitState &candidate = _units[enemy_index];
		double dist = _distance_between(unit, candidate);
		double raw = _score_enemy_target(unit, candidate, strategy, ctx, dist, profile_sim);
		StringName bucket = classify_bucket(candidate, dist, strategy);
		int rank = bucket_rank_for(strategy, bucket);
		double adjusted = raw + double(rank) * strategy.bucket_margin;
		// Python parity: strict lexicographic ordering on key:
		// (adjusted_score, raw_score, bucket_rank, distance, instance_id)
		if (best == nullptr
				|| std::make_tuple(adjusted, raw, rank, dist, candidate.instance_id) <
						std::make_tuple(best_adjusted, best_raw, best_bucket_rank, best_dist, best->instance_id)) {
			best = &candidate;
			best_adjusted = adjusted;
			best_raw = raw;
			best_bucket_rank = rank;
			best_dist = dist;
		}
	}
	if (best == nullptr) {
		unit.target_id = 0;
		unit.current_target_score = 0.0;
		return nullptr;
	}

	// Forced reasons: always pick immediately (no switch logic).
	if (forced_reason) {
		String reason = "no current target";
		if (current_target != nullptr && !current_target->alive) {
			reason = "current target dead";
		} else if (current_target != nullptr && current_target->team == unit.team) {
			reason = "current target invalid team";
		}
		_set_current_target(unit, *best);
		unit.current_target_score = best_raw;
		return best;
	}

	double current_dist = _distance_between(unit, *current_target);
	double current_score = _score_enemy_target(unit, *current_target, strategy, ctx, current_dist, profile_sim);
	if (best->instance_id == current_target->instance_id) {
		unit.current_target_score = current_score;
		_set_current_target(unit, *current_target);
		return current_target;
	}

	if (assassin_pressuring_frontline) {
		bool best_is_backliner = (best->role_id == sn_marksman() || best->role_id == sn_mage() || best->role_id == sn_support());
		if (best_is_backliner) {
			_set_current_target(unit, *best);
			unit.target_switch_lock_timer = 0.0;
			unit.current_target_score = best_raw;
			return best;
		}
	}

	if (!_should_switch(unit, current_score, best_raw, strategy)) {
		unit.current_target_score = current_score;
		_set_current_target(unit, *current_target);
		return current_target;
	}

	_set_current_target(unit, *best);
	unit.target_switch_lock_timer = TARGET_SWITCH_LOCK_DURATION;
	unit.current_target_score = best_raw;
	return best;
}

TeamfightSimulationCore::UnitState *TeamfightSimulationCore::_select_ally_target(UnitState &unit) {
	// Python unit.py: only tanks/support run ally selection each tick.
	const UnitStrategy &strat = _strategy_for_unit(unit);
	String strategy_name = strat.display_name;
	bool allow_ally = strategy_name == String("Enchanter") || strategy_name == String("Protector");
	if (!allow_ally) {
		allow_ally = (unit.role_id == sn_support() || unit.role_id == sn_tank());
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
		UnitState &candidate = _units[ally_index];
		double hp_ratio = candidate.hp / Math::max(0.0001, candidate.combat.max_hp);
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
		UnitState &candidate = _units[ally_index];
		double dist = _distance_between(unit, candidate);
		double score = _score_ally_target(unit, candidate, strat, dist);
		double hp_ratio = candidate.hp / Math::max(0.0001, candidate.combat.max_hp);
		if (_scratch_critical_allies.empty()) {
			// Python: min by (score, distance, instance_id)
			if (best == nullptr
					|| std::make_tuple(score, dist, candidate.instance_id) <
							std::make_tuple(best_score, best_dist, best->instance_id)) {
				best = &candidate;
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
			best = &candidate;
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

double TeamfightSimulationCore::_attack_range(const UnitState &unit) const {
	return unit.combat.attack_range;
}

double TeamfightSimulationCore::_effective_attack_range(const UnitState &unit) const {
	double attack_range = _attack_range(unit);
	if (attack_range <= RANGED_THRESHOLD) {
		return attack_range + MELEE_CONTACT_BUFFER;
	}
	return attack_range;
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
	target.alive = false;
	target.respawn_timer = target.combat.respawn_time;
	target.deaths += 1;

	const std::unordered_map<int64_t, UnitState::DamageSourceEntry> &damage_sources = target.damage_sources;
	// Reference (teamfight_simulation_core.gd): killer = source with max accumulated damage in window; ties → lower instance_id.
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
		killer_unit->kills += 1;
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
				assist_unit->assists += 1;
			}
		}
	}

	const std::unordered_map<int64_t, double> empty_benefactors;
	const std::unordered_map<int64_t, double> &recent_benefactors = killer_unit != nullptr ? killer_unit->recent_benefactors : empty_benefactors;
	for (const auto &entry : recent_benefactors) {
		int64_t benefactor_id = entry.first;
		double benefactor_time = entry.second;
		if (_time - benefactor_time > ASSIST_WINDOW) {
			continue;
		}
		UnitState *assist_unit = _unit_by_id(benefactor_id);
		if (assist_unit != nullptr) {
			assist_unit->assists += 1;
		}
	}
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
	unit.alive = true;
	unit.hp = unit.combat.max_hp;
	unit.mana = 0.0;
	unit.shield = 0.0;
	unit.perceived_threat = 0.0;
	unit.ability_cooldown = unit.combat.ability_cd;
	// Python parity: ultimate_timer is NOT reset on respawn (preserved across death like attack_cooldown).
	unit.stun_remaining = 0.0;
	unit.taunt_remaining = 0.0;
	unit.forced_target_remaining = 0.0;
	unit.last_kite_timer = 0.0;
	unit.target_switch_lock_timer = 0.0;
	unit.respawn_timer = 0.0;
	unit.damage_sources.clear();
	unit.recent_benefactors.clear();
	unit.last_hit_time = 0.0;
	unit.respawned_this_tick = true;
	unit.cast_resolved_this_tick = false;
	// Python parity: pending casts (casting_remaining > 0) are preserved across death/respawn.
	// Python's respawn() does not clear any cast state, so a cast started before death continues after respawn.
	unit.taunt_target_id = 0;
	unit.forced_target_id = 0;
	unit.forced_target_kind = StringName();
	if (unit.team == StringName("player")) {
		unit.pos_x = DRAFT_X_BASE;
		unit.pos_y = unit.spawn_pos_y;
	} else {
		unit.pos_x = WORLD_SIZE - DRAFT_X_BASE;
		unit.pos_y = unit.spawn_pos_y;
	}
	if (unit_index >= 0) {
		_add_alive_index(unit.team, unit_index);
	}
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
		_refresh_target_pressure();
		_sim_profile_ns_refresh_pressure_pre += sim_profile_elapsed_ns(t0);
	} else {
		_refresh_target_pressure();
	}
	if (profile_sim) {
		for (UnitState &unit : _units) {
			_update_unit(unit, true);
		}
	} else {
		for (UnitState &unit : _units) {
			_update_unit(unit, false);
		}
	}
	if (profile_sim) {
		auto t0 = std::chrono::steady_clock::now();
		_refresh_target_pressure(false);
		_sim_profile_ns_refresh_pressure_post += sim_profile_elapsed_ns(t0);
	} else {
		_refresh_target_pressure(false);
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
	_winner_team = _determine_winner();
	if (profile) {
		_sim_profile_emit_json_stderr();
	}
}

void TeamfightSimulationCore::_prune_assist_window(UnitState &unit) {
	// Python unit.py: cutoff = world.time - ASSIST_WINDOW; drop stale entries each tick.
	const double cutoff = _time - ASSIST_WINDOW;
	std::vector<int64_t> remove_ids;
	remove_ids.reserve(unit.damage_sources.size());
	for (const auto &entry : unit.damage_sources) {
		if (entry.second.last_time <= cutoff) {
			remove_ids.push_back(entry.first);
		}
	}
	for (int64_t id : remove_ids) {
		unit.damage_sources.erase(id);
	}
	remove_ids.clear();
	for (const auto &entry : unit.recent_benefactors) {
		if (entry.second <= cutoff) {
			remove_ids.push_back(entry.first);
		}
	}
	for (int64_t id : remove_ids) {
		unit.recent_benefactors.erase(id);
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

	const UnitStrategy &strategy = [&]() -> const UnitStrategy & {
		SimProfileAccScope _uu_cc(profile_sim, _sim_profile_uu_cooldowns_cc);
		const UnitStrategy &s = _strategy_for_unit(unit);

		unit.attack_cooldown = Math::max(0.0, unit.attack_cooldown - _tick_rate);
		unit.ability_cooldown = Math::max(0.0, unit.ability_cooldown - _tick_rate);
		unit.ultimate_cooldown = Math::max(0.0, unit.ultimate_cooldown - _tick_rate);
		unit.retarget_timer = Math::max(0.0, unit.retarget_timer - _tick_rate);
		unit.target_switch_lock_timer = Math::max(0.0, unit.target_switch_lock_timer - _tick_rate);
		unit.stun_remaining = Math::max(0.0, unit.stun_remaining - _tick_rate);
		unit.taunt_remaining = Math::max(0.0, unit.taunt_remaining - _tick_rate);
		unit.forced_target_remaining = Math::max(0.0, unit.forced_target_remaining - _tick_rate);
		unit.last_kite_timer = Math::max(0.0, unit.last_kite_timer - _tick_rate);
		if (unit.taunt_remaining <= 0.0) {
			unit.taunt_remaining = 0.0;
			unit.taunt_target_id = 0;
		}
		if (unit.forced_target_remaining <= 0.0) {
			unit.forced_target_remaining = 0.0;
			unit.forced_target_id = 0;
			unit.forced_target_kind = StringName();
		}
		return s;
	}();

	// Separation (Python: after CC tick, before threat decay; skipped while stun remains after decrement).
	{
		SimProfileAccScope _uu_sep(profile_sim, _sim_profile_uu_separation);
		if (unit.stun_remaining <= 0.0) {
			double move_speed = unit.combat.move_speed;
			if (move_speed > 0.0) {
				double attack_range = unit.combat.attack_range;
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

		_prune_assist_window(unit);
	}

	{
		SimProfileAccScope _uu_regen(profile_sim, _sim_profile_uu_regen_on_tick);
		double regen_accumulator = unit.regen_accumulator + _tick_rate;
		while (regen_accumulator >= REGEN_TICK_INTERVAL) {
			regen_accumulator -= REGEN_TICK_INTERVAL;
			const std::vector<EffectRecord> &effects = _collect_effects(unit, StringName("on_tick"));
			for (const EffectRecord &effect : effects) {
				EffectContext context = _build_context(unit, nullptr, nullptr, 0.0, StringName("auto"));
				_execute_effect(effect, context);
			}
		}
		unit.regen_accumulator = regen_accumulator;

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
			return;
		}
	}

	{
		SimProfileAccScope _uu_combat(profile_sim, _sim_profile_uu_combat);
		double distance = _distance_between(unit, *target);
		double effective_range = _effective_attack_range(unit);
		bool in_contact = (distance <= effective_range) || (Math::abs(distance - effective_range) <= MELEE_CONTACT_BUFFER);

		// Action priority (Python: casts/autos only if in_range).
		if (in_contact) {
			if (_try_cast_ultimate(unit, *target, distance)) {
				return;
			}
			if (_try_cast_ability(unit, *target, distance)) {
				return;
			}
			if (unit.attack_cooldown <= 0.0) {
				_perform_auto_attack(unit, *target, distance);
				return;
			}
		}
	}

	{
		SimProfileAccScope _uu_move(profile_sim, _sim_profile_uu_movement);
		// Movement: kite first if applicable, else move toward.
		if (strategy.prefers_kiting && unit.attack_cooldown > 0.0 && unit.taunt_remaining <= 0.0) {
			if (_kite_from_enemies(unit)) {
				return;
			}
		}
		_move_toward_target(unit, *target);
	}
}

bool TeamfightSimulationCore::_try_cast_ability(UnitState &unit, UnitState &target, double distance) {
	(void)distance;
	if (unit.ability_cooldown > 0.0) {
		return false;
	}
	return _start_cast(unit, target, distance, StringName("ability"));
}

bool TeamfightSimulationCore::_try_cast_ultimate(UnitState &unit, UnitState &target, double distance) {
	double max_mana = unit.combat.max_mana;
	if (unit.ultimate_cooldown > 0.0 || max_mana <= 0.0 || unit.mana < max_mana) {
		return false;
	}
	return _start_cast(unit, target, distance, StringName("ultimate"));
}

bool TeamfightSimulationCore::_start_cast(UnitState &unit, UnitState &target, double distance, const StringName &action_kind) {
	(void)distance;
	bool has_effect = action_kind == StringName("ability") ? unit.has_ability_effect : unit.has_ultimate_effect;
	if (!has_effect) {
		return false;
	}
	UnitState *target_ally = _select_ally_target(unit);
	if (action_kind == StringName("ability")) {
		unit.ability_cooldown = unit.combat.ability_cd;
		unit.abilities += 1;
	} else {
		unit.ultimate_cooldown = unit.combat.ultimate_cd;
		unit.ultimates += 1;
		unit.mana = Math::max(0.0, unit.mana - unit.combat.max_mana);
	}
	unit.casting_remaining = CASTING_WINDUP;
	unit.casting_kind = action_kind;
	unit.casting_effect = action_kind == StringName("ability") ? unit.ability_effect : unit.ultimate_effect;
	unit.has_casting_effect = true;
	unit.casting_target_id = unit.target_id != 0 ? unit.target_id : target.instance_id;
	unit.casting_ally_target_id = unit.current_ally_target_id != 0 ? unit.current_ally_target_id : (target_ally == nullptr ? 0 : target_ally->instance_id);

	_emit_trace(StringName("cast_start"), unit.instance_id, target.instance_id, action_kind == StringName("ultimate") ? 1.0 : 0.0);

	return true;
}

void TeamfightSimulationCore::_resolve_cast(UnitState &unit) {
	EffectRecord effect = unit.casting_effect;
	StringName action_kind = unit.casting_kind;
	UnitState *target = _unit_by_id(unit.casting_target_id);
	UnitState *target_ally = _unit_by_id(unit.casting_ally_target_id);
	bool had_effect = unit.has_casting_effect;
	unit.casting_remaining = 0.0;
	unit.casting_kind = StringName();
	unit.casting_effect = EffectRecord();
	unit.has_casting_effect = false;
	unit.casting_target_id = 0;
	unit.casting_ally_target_id = 0;
	if (!had_effect) {
		return;
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
			unit.ultimate_cooldown = 0.0;
			unit.mana = Math::min(unit.combat.max_mana, unit.mana + unit.combat.max_mana);
		}
		return;
	}
	EffectContext context = _build_context(unit, target, target_ally, 0.0, action_kind);
	_execute_effect(effect, context);
}

void TeamfightSimulationCore::_perform_auto_attack(UnitState &unit, UnitState &target, double distance) {
	unit.auto_attacks += 1;
	unit.attack_count += 1;
	// Python parity: damage + on_attack modifiers first, then projectile/hit,
	// then mana gain, then attack cooldown (resolve_attack → mana → cooldown).
	double damage = unit.combat.attack_damage;
	damage = _apply_attack_modifiers(unit, target, distance, damage);
	if (unit.combat.attack_range > RANGED_THRESHOLD) {
		ProjectileState projectile;
		projectile.source_id = unit.instance_id;
		projectile.target_id = target.instance_id;
		projectile.damage = damage;
		projectile.damage_type = StringName("physical");
		projectile.stun_duration = DEFAULT_PROJECTILE_STUN_DURATION;
		projectile.radius = unit.combat.projectile_radius;
		projectile.speed = Math::max(0.0001, unit.combat.projectile_speed);
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
		double life_steal = unit.combat.life_steal;
		if (life_steal > 0.0) {
			_heal_unit(unit, unit, dealt * life_steal);
		}
	}
	// Python parity: mana gain happens after attack resolution.
	double mana_gain = unit.combat.mana_per_attack;
	if (mana_gain > 0.0) {
		double max_mana = unit.combat.max_mana;
		unit.mana = Math::min(max_mana, unit.mana + mana_gain);
	}
	// Python parity: attack cooldown set after mana gain.
	double attack_speed = Math::max(0.0001, unit.combat.attack_speed);
	unit.attack_cooldown = 1.0 / attack_speed;
}

void TeamfightSimulationCore::_move_toward_target(UnitState &unit, UnitState &target) {
	double speed = unit.combat.move_speed * _tick_rate;
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
	double move_speed = unit.combat.move_speed;
	double step = move_speed * KITE_SPEED_MODIFIER * _tick_rate;
	unit.pos_x = Math::clamp(ux + vel_x * step, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
	unit.pos_y = Math::clamp(uy + vel_y * step, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
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
	if (projectile.action_kind == StringName("auto")) {
		double life_steal = source->combat.life_steal;
		if (life_steal > 0.0) {
			_heal_unit(*source, *source, dealt * life_steal);
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

void TeamfightSimulationCore::_execute_effect(const EffectRecord &effect, EffectContext &context) {
	if (context.source == nullptr) {
		return;
	}
	UnitState &source = *context.source;
	UnitState *target = context.target;
	UnitState *target_ally = context.target_ally;
	switch (effect.opcode) {
		case EFFECT_OPCODE_MULTI:
			for (const EffectRecord &child : effect.children) {
				_execute_effect(child, context);
			}
			return;
		case EFFECT_OPCODE_DAMAGE: {
			if (target == nullptr) {
				return;
			}
			double damage = source.combat.attack_damage * effect.scalar0;
			double dealt = _apply_damage(source, *target, damage, effect.damage_type.is_empty() ? StringName("physical") : effect.damage_type, context.action_kind, context);
			if (effect.scalar1 > 0.5) {
				_run_post_attack_effects(source, *target, dealt, context);
			}
			return;
		}
		case EFFECT_OPCODE_PROJECTILE: {
			if (target == nullptr) {
				return;
			}
			ProjectileState projectile_state;
			projectile_state.source_id = source.instance_id;
			projectile_state.target_id = target->instance_id;
			projectile_state.damage = source.combat.attack_damage * effect.scalar2;
			projectile_state.damage_type = effect.damage_type.is_empty() ? StringName("physical") : effect.damage_type;
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
			return;
		}
		case EFFECT_OPCODE_STUN:
			if (target != nullptr) {
				_apply_stun(source, *target, effect.scalar0);
			}
			return;
		case EFFECT_OPCODE_SHIELD: {
			UnitState &shield_target = target_ally == nullptr ? source : *target_ally;
			double amount = source.combat.max_hp * effect.scalar0;
			_add_shield(source, shield_target, amount);
			return;
		}
		case EFFECT_OPCODE_HEAL: {
			UnitState &heal_target = target_ally == nullptr ? source : *target_ally;
			double heal_amount = source.combat.max_hp * effect.scalar0 + heal_target.hp * effect.scalar1 + effect.scalar2;
			_heal_unit(source, heal_target, heal_amount);
			return;
		}
		case EFFECT_OPCODE_SELF_DAMAGE: {
			double self_damage = source.combat.max_hp * effect.scalar0;
			_apply_damage(source, source, self_damage, effect.damage_type.is_empty() ? StringName("true") : effect.damage_type, context.action_kind, context);
			return;
		}
		case EFFECT_OPCODE_SELF_SHIELD: {
			double self_shield = source.combat.max_hp * effect.scalar0;
			_add_shield(source, source, self_shield);
			return;
		}
		case EFFECT_OPCODE_SELF_AOE_TAUNT:
			_apply_aoe_taunt(source, effect.scalar0, effect.scalar1);
			return;
		case EFFECT_OPCODE_SELF_AOE_DAMAGE: {
			double aoe_damage = source.combat.attack_damage * effect.scalar1;
			_apply_aoe_damage(source, source, aoe_damage, effect.scalar0, effect.damage_type.is_empty() ? StringName("physical") : effect.damage_type, effect.reason, context.action_kind);
			return;
		}
		case EFFECT_OPCODE_SPLASH_DAMAGE:
			if (target != nullptr) {
				_apply_splash_damage(source, *target, context.damage, effect.scalar0, effect.damage_type.is_empty() ? StringName("physical") : effect.damage_type, context.action_kind, effect.reason, effect.scalar1);
			}
			return;
		case EFFECT_OPCODE_THRESHOLD_SPLASH_DAMAGE:
			if (context.damage > source.combat.attack_damage * effect.scalar0 && !effect.children.empty()) {
				_execute_effect(effect.children[0], context);
			}
			return;
		case EFFECT_OPCODE_MANA_REGEN:
			_restore_mana(source, source, effect.scalar0 + source.combat.max_mana * effect.scalar1);
			return;
		case EFFECT_OPCODE_POST_DAMAGE_MANA_GAIN:
			_restore_mana(source, source, context.damage * effect.scalar0);
			return;
		case EFFECT_OPCODE_DAMAGE_BASED_HEAL:
			_heal_unit(source, source, context.damage * effect.scalar0);
			return;
		case EFFECT_OPCODE_MANA_RESTORE_ON_HIT:
			_restore_mana(source, source, effect.scalar0);
			return;
		case EFFECT_OPCODE_DRAIN_TARGET_MANA_ON_HIT:
			if (target != nullptr) {
				target->mana = Math::max(0.0, target->mana - effect.scalar0);
			}
			return;
		case EFFECT_OPCODE_EVERY_N_ATTACKS_STUN:
			if (effect.int0 > 0 && target != nullptr && source.attack_count % effect.int0 == 0) {
				_apply_stun(source, *target, effect.scalar0);
			}
			return;
		case EFFECT_OPCODE_DODGE:
		case EFFECT_OPCODE_CONSTANT_MULTIPLIER:
		case EFFECT_OPCODE_TARGET_HP_THRESHOLD_MULTIPLIER:
		case EFFECT_OPCODE_DISTANCE_THRESHOLD_MULTIPLIER:
		case EFFECT_OPCODE_SELF_HP_THRESHOLD_MULTIPLIER:
		default:
			return;
	}
}

void TeamfightSimulationCore::_merge_result(Dictionary &target_result, const Dictionary &source_result) {
	Array keys = source_result.keys();
	for (int64_t index = 0; index < keys.size(); ++index) {
		Variant key = keys[index];
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
	_summary_cache["player_kills"] = int64_t(_player_kills);
	_summary_cache["enemy_kills"] = int64_t(_enemy_kills);
	_summary_cache["player_comp"] = _player_comp;
	_summary_cache["enemy_comp"] = _enemy_comp;
	_summary_unit_stats.clear();
	for (const UnitState &unit : _units) {
		Dictionary unit_summary;
		unit_summary["instance_id"] = unit.instance_id;
		unit_summary["archetype"] = String(unit.archetype_id);
		unit_summary["team"] = String(unit.team);
		unit_summary["won"] = _winner_team != StringName() && unit.team == _winner_team;
		unit_summary["damage_dealt"] = unit.damage_dealt;
		unit_summary["damage_dealt_auto"] = unit.damage_dealt_auto;
		unit_summary["damage_dealt_ability"] = unit.damage_dealt_ability;
		unit_summary["damage_dealt_ultimate"] = unit.damage_dealt_ultimate;
		unit_summary["damage_received"] = unit.damage_received;
		unit_summary["damage_mitigated"] = unit.damage_mitigated;
		unit_summary["healing_done"] = unit.healing_done;
		unit_summary["shielding_done"] = unit.shielding_done;
		unit_summary["auto_attacks"] = unit.auto_attacks;
		unit_summary["abilities"] = unit.abilities;
		unit_summary["ultimates"] = unit.ultimates;
		unit_summary["stuns"] = unit.stuns;
		unit_summary["kills"] = unit.kills;
		unit_summary["deaths"] = unit.deaths;
		unit_summary["assists"] = unit.assists;
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
}

bool TeamfightSimulationCore::match_ticks_exhausted() const {
	double effective_tick_rate = Math::max(_tick_rate, EPSILON);
	int64_t max_ticks = int64_t(Math::ceil(MATCH_DURATION / effective_tick_rate));
	return _tick >= max_ticks;
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

void TeamfightSimulationCore::_viewer_record_damage_fx(const UnitState &p_source, const UnitState &p_target, double p_total_damage, const StringName &p_action_kind) {
	ViewerFxEvent ev;
	ev.kind = StringName("damage");
	ev.target_id = p_target.instance_id;
	ev.src_id = p_source.instance_id;
	ev.pos_x = p_target.pos_x;
	ev.pos_y = p_target.pos_y;
	ev.val = p_total_damage;
	_viewer_fx_push(ev);
	if (p_action_kind == StringName("auto") && p_source.combat.attack_range <= RANGED_THRESHOLD) {
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

String TeamfightSimulationCore::_viewer_state_string(const UnitState &p_u) const {
	if (!p_u.alive) {
		return String("DEAD");
	}
	if (p_u.stun_remaining > 0.0) {
		return String("STUNNED");
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
		Dictionary d;
		d["id"] = u.instance_id;
		d["instance_id"] = u.instance_id;
		d["x"] = u.pos_x;
		d["y"] = u.pos_y;
		d["pos_x"] = u.pos_x;
		d["pos_y"] = u.pos_y;
		d["team"] = String(u.team);
		d["archetype_id"] = String(u.archetype_id);
		d["hp"] = u.hp;
		d["max_hp"] = u.combat.max_hp;
		d["shield"] = u.shield;
		d["mana"] = u.mana;
		d["max_mana"] = u.combat.max_mana;
		d["target"] = u.target_id;
		d["target_id"] = u.target_id;
		d["stun"] = u.stun_remaining;
		d["stun_remaining"] = u.stun_remaining;
		d["alive"] = u.alive;
		d["state"] = _viewer_state_string(u);
		d["acd"] = u.attack_cooldown;
		d["abi"] = u.ability_cooldown;
		d["ult"] = u.ultimate_cooldown;
		d["attack_cooldown"] = u.attack_cooldown;
		d["attack_range"] = u.combat.attack_range;
		d["attack_speed"] = u.combat.attack_speed;
		d["casting_remaining"] = u.casting_remaining;
		d["casting_kind"] = String(u.casting_kind);
		d["kills"] = u.kills;
		d["deaths"] = u.deaths;
		d["assists"] = u.assists;
		d["respawn_timer"] = u.respawn_timer;
		d["taunt_remaining"] = u.taunt_remaining;
		d["damage_dealt"] = u.damage_dealt;
		d["healing_done"] = u.healing_done;
		d["damage_mitigated"] = u.damage_mitigated;
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
