#include "sim_catalog.hpp"

#include "sim_constants.hpp"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace sim {
namespace catalog {

namespace {

Dictionary load_json_required(const String &path) {
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

Dictionary load_json_optional(const String &path) {
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

bool patch_applies_to(const BalancePatch &patch, const StringName &archetype_id, const StringName &role) {
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

void apply_stat_patch_to_stats(const BalancePatch &patch, Dictionary &stats) {
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

void merge_kit_into_champion(Dictionary &champion, const Dictionary &kit) {
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

bool validate_effective_champion(
		const CatalogState &state,
		const CatalogHooks &hooks,
		const StringName &archetype_id,
		const Dictionary &champion) {
	bool ok = true;
	Variant ab = champion.get("ability", Variant());
	if (ab.get_type() == Variant::DICTIONARY) {
		Dictionary abd = Dictionary(ab);
		StringName kind = StringName(String(abd.get("kind", "")));
		EffectRecord compiled = hooks.compile_effect != nullptr ? hooks.compile_effect(hooks.user_data, abd) : EffectRecord{};
		if (!kind.is_empty() && compiled.opcode == EFFECT_OPCODE_UNKNOWN) {
			UtilityFunctions::push_error(vformat("Unknown or unsupported ability kind '%s' for archetype '%s'", String(kind), String(archetype_id)));
			ok = false;
		}
	}
	Variant ult = champion.get("ultimate", Variant());
	if (ult.get_type() == Variant::DICTIONARY) {
		Dictionary ultd = Dictionary(ult);
		StringName kind = StringName(String(ultd.get("kind", "")));
		EffectRecord compiled = hooks.compile_effect != nullptr ? hooks.compile_effect(hooks.user_data, ultd) : EffectRecord{};
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
		if (!state.passive_registry.has(pid)) {
			UtilityFunctions::push_error(vformat("Unknown passive_id '%s' for archetype '%s'", String(pid), String(archetype_id)));
			ok = false;
		}
	}
	return ok;
}

} // namespace

void parse_balance_patch_from_dict(const Dictionary &pd, BalancePatch &patch) {
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

void rebuild_effective_champion_cache(CatalogState &state, const CatalogHooks &hooks) {
	state.effective_champion_by_archetype.clear();
	Array archetype_keys = state.champion_catalog.keys();
	for (int64_t ki = 0; ki < archetype_keys.size(); ++ki) {
		Variant key_var = archetype_keys[ki];
		String key_str = String(key_var);

		// Skip non-champion entries like "passives", "role_configs", and "minions"
		if (key_str == "passives" || key_str == "role_configs" || key_str == "minions") {
			continue;
		}

		StringName archetype_id = StringName(key_str);
		Dictionary base = Dictionary(state.champion_catalog[key_var]);
		if (base.is_empty()) {
			continue;
		}
		Dictionary eff = base.duplicate(true);
		Dictionary stats = Dictionary(eff["stats"]);
		Dictionary role_config = Dictionary(state.role_configs.get(stats.get("role", StringName()), Dictionary()));
		Dictionary stat_mods = Dictionary(role_config.get("stat_mods", Dictionary()));
		Array stat_keys = stat_mods.keys();
		for (int64_t key_index = 0; key_index < stat_keys.size(); ++key_index) {
			Variant key_value = stat_keys[key_index];
			stats[key_value] = stat_mods[key_value];
		}
		StringName role_name = StringName(stats.get("role", StringName()));

		for (const BalancePatch &patch : state.balance_patches) {
			if (!patch_applies_to(patch, archetype_id, role_name)) {
				continue;
			}
			apply_stat_patch_to_stats(patch, stats);

			if (!patch.kit_id.is_empty()) {
				Variant kit_v = state.ability_kits.get(patch.kit_id, Variant());
				if (kit_v.get_type() == Variant::DICTIONARY) {
					merge_kit_into_champion(eff, Dictionary(kit_v));
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

		if (!validate_effective_champion(state, hooks, archetype_id, eff)) {
			UtilityFunctions::push_error(vformat("Effective champion validation failed for '%s'; using vanilla catalog entry.", key_str));
			Dictionary vanilla = base.duplicate(true);
			Dictionary vstats = Dictionary(vanilla["stats"]);
			Dictionary vrole_config = Dictionary(state.role_configs.get(vstats.get("role", StringName()), Dictionary()));
			Dictionary vstat_mods = Dictionary(vrole_config.get("stat_mods", Dictionary()));
			Array vk = vstat_mods.keys();
			for (int64_t i = 0; i < vk.size(); ++i) {
				Variant kv = vk[i];
				vstats[kv] = vstat_mods[kv];
			}
			vanilla["stats"] = vstats;
			state.effective_champion_by_archetype[key_str] = vanilla;
		} else {
			state.effective_champion_by_archetype[key_str] = eff;
		}
	}
}

void ensure_loaded(CatalogState &state, const CatalogHooks &hooks) {
	// Load catalog every time to pick up changes from champion_schema.json
	// This ensures stats generation uses the most recent champion data
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
			parse_balance_patch_from_dict(pd, patch);
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

	state.champion_catalog = champion_catalog;
	state.minion_catalog = minion_catalog;
	state.role_configs = role_configs;
	state.passive_registry = passive_registry;
	state.ability_kits = ability_kits;
	// On first load, use file patches; on hot reload preserve programmatic patches.
	if (!state.catalog_loaded) {
		state.balance_patches = balance_patches;
	}

	rebuild_effective_champion_cache(state, hooks);
	state.catalog_loaded = true;
}

Dictionary effective_champion_for(const CatalogState &state, const StringName &archetype_id) {
	String key = String(archetype_id);
	if (state.effective_champion_by_archetype.has(key)) {
		return Dictionary(state.effective_champion_by_archetype[key]);
	}
	return champion_for(state, archetype_id);
}

Dictionary champion_for(const CatalogState &state, const StringName &archetype_id) {
	// Check champion catalog first
	if (state.champion_catalog.has(archetype_id)) {
		return Dictionary(state.champion_catalog[archetype_id]);
	}
	// Fall back to minion catalog for minions
	if (state.minion_catalog.has(archetype_id)) {
		return Dictionary(state.minion_catalog[archetype_id]);
	}
	return Dictionary();
}

void set_balance_patches(CatalogState &state, const Array &patches, const CatalogHooks &hooks) {
	state.balance_patches.clear();
	for (int64_t i = 0; i < patches.size(); ++i) {
		Dictionary pd = Dictionary(patches[i]);
		BalancePatch patch;
		parse_balance_patch_from_dict(pd, patch);
		state.balance_patches.push_back(patch);
	}
	rebuild_effective_champion_cache(state, hooks);
}

Array get_balance_patches(const CatalogState &state) {
	Array result;
	for (const BalancePatch &patch : state.balance_patches) {
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

} // namespace catalog
} // namespace sim
