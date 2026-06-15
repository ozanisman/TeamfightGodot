#ifndef SIM_CATALOG_HPP
#define SIM_CATALOG_HPP

#include "simulation_types.hpp"

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string_name.hpp>

#include <vector>

namespace sim {
namespace catalog {

struct CatalogHooks {
	void *user_data = nullptr;
	EffectRecord (*compile_effect)(void *user_data, const Dictionary &effect) = nullptr;
};

struct CatalogState {
	Dictionary champion_catalog;
	Dictionary minion_catalog;
	Dictionary role_configs;
	std::vector<BalancePatch> balance_patches;
	Dictionary ability_kits;
	Dictionary effective_champion_by_archetype;
	Dictionary passive_registry;
	Dictionary champion_tags; // champion_id -> Array of tag strings
	bool catalog_loaded = false;
};

void ensure_loaded(CatalogState &state, const CatalogHooks &hooks);
void parse_balance_patch_from_dict(const Dictionary &pd, BalancePatch &patch);
void rebuild_effective_champion_cache(CatalogState &state, const CatalogHooks &hooks);
Dictionary effective_champion_for(const CatalogState &state, const StringName &unit_id);
Dictionary champion_for(const CatalogState &state, const StringName &unit_id);
void set_balance_patches(CatalogState &state, const Array &patches, const CatalogHooks &hooks);
Array get_balance_patches(const CatalogState &state);
std::vector<StringName> tags_for(const CatalogState &state, const StringName &champion);
bool champion_has_tag(const CatalogState &state, const StringName &champion, const StringName &tag);

} // namespace catalog
} // namespace sim

#endif // SIM_CATALOG_HPP
