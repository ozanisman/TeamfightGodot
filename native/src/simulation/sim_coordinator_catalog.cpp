#include "../teamfight_simulation_core.hpp"

#include "sim_catalog.hpp"
#include "sim_effects_compile.hpp"

sim::EffectRecord TeamfightSimulationCore::_compile_effect(const Dictionary &effect) const {
	return sim::effects::compile::compile_effect(effect);
}

std::vector<sim::EffectRecord> TeamfightSimulationCore::_compile_effect_array(const Array &effects) const {
	return sim::effects::compile::compile_effect_array(effects);
}

bool TeamfightSimulationCore::_effect_record_contains_opcode(
		const sim::EffectRecord &effect,
		sim::EffectOpcode opcode) const {
	const int64_t want = static_cast<int64_t>(opcode);
	if (effect.opcode == want) {
		return true;
	}
	for (const sim::EffectRecord &child : effect.children) {
		if (_effect_record_contains_opcode(child, opcode)) {
			return true;
		}
	}
	return false;
}

void TeamfightSimulationCore::_finalize_reflect_passives(sim::UnitState &unit, sim::UnitStateCold &cold) {
	cold.passive_reflect_entries.clear();
	for (const sim::EffectRecord &eff : cold.passive_effects[1]) {
		if (eff.opcode == sim::EFFECT_OPCODE_REFLECT_DAMAGE) {
			sim::PassiveReflectEntry entry;
			entry.percentage = eff.scalar0;
			entry.damage_type = eff.int0 == 1 ? StringName("all") : StringName("physical");
			entry.action_kind = StringName("passive");
			cold.passive_reflect_entries.push_back(entry);
		}
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

sim::EffectRecord TeamfightSimulationCore::_catalog_compile_effect(void *user_data, const Dictionary &effect) {
	return static_cast<TeamfightSimulationCore *>(user_data)->_compile_effect(effect);
}

Dictionary TeamfightSimulationCore::_effective_champion_for(const StringName &archetype_id) const {
	return sim::catalog::effective_champion_for(_catalog, archetype_id);
}

Dictionary TeamfightSimulationCore::_champion_for(const StringName &archetype_id) const {
	return sim::catalog::champion_for(_catalog, archetype_id);
}

void TeamfightSimulationCore::set_balance_patches(const Array &patches) {
	_ensure_catalog_loaded();
	sim::catalog::set_balance_patches(_catalog, patches, _catalog_hooks());
}

Array TeamfightSimulationCore::get_balance_patches() const {
	return sim::catalog::get_balance_patches(_catalog);
}

Dictionary TeamfightSimulationCore::effective_champion_for(const StringName &archetype_id) const {
	return _effective_champion_for(archetype_id);
}
