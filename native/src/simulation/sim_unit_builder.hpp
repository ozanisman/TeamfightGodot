#ifndef SIM_UNIT_BUILDER_HPP
#define SIM_UNIT_BUILDER_HPP

#include "sim_catalog.hpp"
#include "simulation_types.hpp"

#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include <utility>

namespace sim {
namespace unit_builder {

struct UnitBuilderHost {
	void *user_data = nullptr;
	const catalog::CatalogState *catalog = nullptr;
	EffectRecord (*compile_effect)(void *user_data, const Dictionary &effect) = nullptr;
	Dictionary (*effective_champion_for)(void *user_data, const StringName &unit_id) = nullptr;
	void (*finalize_reflect_passives)(void *user_data, UnitState &unit, UnitStateCold &cold) = nullptr;
	int64_t (*assign_spawn_slot)(void *user_data, const StringName &team) = nullptr;
	Vector2 (*get_random_spawn_position)(void *user_data, const StringName &team, bool is_respawn) = nullptr;
};

struct BuiltUnit {
	UnitState unit;
	UnitStateCold cold;
	UnitStateRare rare;
};

BuiltUnit build_unit(
		const UnitBuilderHost &host,
		const Dictionary &spawn_spec,
		const StringName &team,
		int64_t instance_id);

} // namespace unit_builder
} // namespace sim

#endif // SIM_UNIT_BUILDER_HPP
