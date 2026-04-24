#include "teamfight_simulation_core.hpp"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <limits>
#include <utility>

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
}

TeamfightSimulationCore::~TeamfightSimulationCore() {
}

void TeamfightSimulationCore::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_ready"), &TeamfightSimulationCore::is_ready);
	ClassDB::bind_method(D_METHOD("clear"), &TeamfightSimulationCore::clear);
	ClassDB::bind_method(D_METHOD("run_match", "match_input"), &TeamfightSimulationCore::run_match);
	ClassDB::bind_method(D_METHOD("run_matches", "match_inputs"), &TeamfightSimulationCore::run_matches);
}

double TeamfightSimulationCore::_randf() {
	return _rng.next_unit_f64();
}

void TeamfightSimulationCore::_reset_runtime_state() {
	_units.clear();
	_projectiles.clear();
	_scratch_projectiles.clear();
	_summary_unit_stats.clear();
	_summary_cache.clear();
	_time = 0.0;
	_tick = 0;
	_tick_rate = DEFAULT_TICK_RATE;
	_seed = 0;
	_winner_team = StringName("draw");
	_record_events = false;
	_debug_duel_trace = false;
	_debug_duel_unit_a = 0;
	_debug_duel_unit_b = 0;
	_debug_targeting = false;
	_debug_combat_trace = false;
	_debug_fixture_name = "";
	_player_comp.clear();
	_enemy_comp.clear();
	_player_kills = 0;
	_enemy_kills = 0;
	_unit_index_map.clear();
	_alive_player_indices.clear();
	_alive_enemy_indices.clear();
}

bool TeamfightSimulationCore::_is_debug_duel_unit(int64_t instance_id) const {
	if (!_debug_duel_trace) {
		return false;
	}
	return instance_id == _debug_duel_unit_a || instance_id == _debug_duel_unit_b;
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
	_champion_catalog = _load_json_file(String(CHAMPION_SCHEMA_PATH));
	_build_role_configs();
	_build_passive_registry();
	_catalog_loaded = true;
}

void TeamfightSimulationCore::_build_role_configs() {
	Dictionary tank_mods;
	tank_mods[StringName("tenacity")] = 0.2;
	Dictionary fighter_mods;
	fighter_mods[StringName("life_steal")] = 0.1;
	fighter_mods[StringName("tenacity")] = 0.1;
	Dictionary marksman_mods;
	marksman_mods[StringName("attack_speed")] = 1.2;
	Dictionary assassin_mods;
	assassin_mods[StringName("move_speed")] = 1.5;
	Dictionary mage_mods;
	Dictionary support_mods;
	support_mods[StringName("ability_cd")] = 4.0;

	Dictionary tank;
	tank[StringName("stat_mods")] = tank_mods;
	tank[StringName("passive_on_tick")] = Variant();
	Dictionary tank_take_damage_params;
	tank_take_damage_params[StringName("damage_ratio")] = 0.1;
	tank[StringName("passive_post_take_damage")] = effect_dict(StringName("post_damage_mana_gain"), tank_take_damage_params);

	Dictionary fighter;
	fighter[StringName("stat_mods")] = fighter_mods;
	fighter[StringName("passive_on_tick")] = Variant();
	fighter[StringName("passive_post_take_damage")] = Variant();

	Dictionary marksman;
	marksman[StringName("stat_mods")] = marksman_mods;
	marksman[StringName("passive_on_tick")] = Variant();
	marksman[StringName("passive_post_take_damage")] = Variant();

	Dictionary assassin;
	assassin[StringName("stat_mods")] = assassin_mods;
	assassin[StringName("passive_on_tick")] = Variant();
	assassin[StringName("passive_post_take_damage")] = Variant();

	Dictionary mage;
	mage[StringName("stat_mods")] = mage_mods;
	Dictionary mage_tick_params;
	mage_tick_params[StringName("flat_amount")] = 1.0;
	mage[StringName("passive_on_tick")] = effect_dict(StringName("mana_regen"), mage_tick_params);
	mage[StringName("passive_post_take_damage")] = Variant();

	Dictionary support;
	support[StringName("stat_mods")] = support_mods;
	support[StringName("passive_on_tick")] = Variant();
	support[StringName("passive_post_take_damage")] = Variant();

	_role_configs[StringName("tank")] = tank;
	_role_configs[StringName("fighter")] = fighter;
	_role_configs[StringName("marksman")] = marksman;
	_role_configs[StringName("assassin")] = assassin;
	_role_configs[StringName("mage")] = mage;
	_role_configs[StringName("support")] = support;
}

void TeamfightSimulationCore::_build_passive_registry() {
	Dictionary duelist;
	Array duelist_on_attack;
	Dictionary duelist_multiplier_params;
	duelist_multiplier_params[StringName("multiplier")] = 1.2;
	duelist_on_attack.append(effect_dict(StringName("constant_multiplier"), duelist_multiplier_params));
	duelist[StringName("on_attack")] = duelist_on_attack;

	Dictionary eagle_eye;
	Array eagle_eye_on_attack;
	Dictionary eagle_eye_params;
	eagle_eye_params[StringName("multiplier")] = 1.25;
	eagle_eye_on_attack.append(effect_dict(StringName("constant_multiplier"), eagle_eye_params));
	eagle_eye[StringName("on_attack")] = eagle_eye_on_attack;
	Array eagle_eye_post_attack;
	Dictionary threshold_params;
	threshold_params[StringName("threshold_multiplier")] = 3.0;
	Dictionary splash_params;
	splash_params[StringName("radius")] = 2.0;
	splash_params[StringName("ratio")] = 0.5;
	splash_params[StringName("damage_type")] = String("physical");
	splash_params[StringName("reason")] = String("Rain of Arrows");
	splash_params[StringName("color")] = Array();
	threshold_params[StringName("splash")] = effect_dict(StringName("splash_damage"), splash_params);
	eagle_eye_post_attack.append(effect_dict(StringName("threshold_splash_damage"), threshold_params));
	eagle_eye[StringName("post_attack")] = eagle_eye_post_attack;

	Dictionary bastion;
	Array bastion_on_defense;
	Dictionary bastion_params;
	bastion_params[StringName("multiplier")] = 0.9;
	bastion_on_defense.append(effect_dict(StringName("constant_multiplier"), bastion_params));
	bastion[StringName("on_defense")] = bastion_on_defense;

	Dictionary executioner;
	Array executioner_on_attack;
	Dictionary executioner_params;
	executioner_params[StringName("hp_ratio_threshold")] = 0.3;
	executioner_params[StringName("multiplier")] = 2.0;
	executioner_on_attack.append(effect_dict(StringName("target_hp_threshold_multiplier"), executioner_params));
	executioner[StringName("on_attack")] = executioner_on_attack;

	Dictionary mana_font;
	Array mana_font_on_tick;
	Dictionary mana_font_params;
	mana_font_params[StringName("flat_amount")] = 3.0;
	mana_font_on_tick.append(effect_dict(StringName("mana_regen"), mana_font_params));
	mana_font[StringName("on_tick")] = mana_font_on_tick;

	Dictionary marksman;
	Array marksman_on_attack;
	Dictionary marksman_params;
	marksman_params[StringName("min_distance")] = 3.0;
	marksman_params[StringName("multiplier")] = 1.25;
	marksman_on_attack.append(effect_dict(StringName("distance_threshold_multiplier"), marksman_params));
	marksman[StringName("on_attack")] = marksman_on_attack;

	Dictionary bloodlust;
	Array bloodlust_post_attack;
	Dictionary bloodlust_params;
	bloodlust_params[StringName("heal_ratio")] = 0.15;
	bloodlust_post_attack.append(effect_dict(StringName("damage_based_heal"), bloodlust_params));
	bloodlust[StringName("post_attack")] = bloodlust_post_attack;

	Dictionary rejuvenation;
	Array rejuvenation_on_tick;
	Dictionary rejuvenation_params;
	rejuvenation_params[StringName("flat_amount")] = 5.0;
	rejuvenation_params[StringName("reason")] = String("Rejuvenation");
	rejuvenation_on_tick.append(effect_dict(StringName("heal"), rejuvenation_params));
	rejuvenation[StringName("on_tick")] = rejuvenation_on_tick;

	Dictionary agility;
	Array agility_on_defense;
	Dictionary agility_params;
	agility_params[StringName("dodge_chance")] = 0.25;
	agility_params[StringName("on_dodge_multiplier")] = 0.0;
	agility_params[StringName("on_hit_multiplier")] = 1.0;
	agility_on_defense.append(effect_dict(StringName("dodge"), agility_params));
	agility[StringName("on_defense")] = agility_on_defense;

	Dictionary enlightenment;
	Array enlightenment_post_attack;
	Dictionary enlightenment_params;
	enlightenment_params[StringName("flat_amount")] = 5.0;
	enlightenment_post_attack.append(effect_dict(StringName("mana_restore_on_hit"), enlightenment_params));
	enlightenment[StringName("post_attack")] = enlightenment_post_attack;

	Dictionary tenacity;
	Array tenacity_on_defense;
	Dictionary tenacity_params;
	tenacity_params[StringName("multiplier")] = 0.9;
	tenacity_on_defense.append(effect_dict(StringName("constant_multiplier"), tenacity_params));
	tenacity[StringName("on_defense")] = tenacity_on_defense;

	Dictionary swiftness;
	Array swiftness_on_attack;
	Dictionary swiftness_params;
	swiftness_params[StringName("multiplier")] = 1.15;
	swiftness_on_attack.append(effect_dict(StringName("constant_multiplier"), swiftness_params));
	swiftness[StringName("on_attack")] = swiftness_on_attack;

	Dictionary vampirism;
	Array vampirism_post_attack;
	Dictionary vampirism_params;
	vampirism_params[StringName("flat_amount")] = 3.0;
	vampirism_params[StringName("reason")] = String("Vampirism");
	vampirism_post_attack.append(effect_dict(StringName("heal"), vampirism_params));
	vampirism[StringName("post_attack")] = vampirism_post_attack;

	Dictionary technique;
	Array technique_post_attack;
	Dictionary technique_params;
	technique_params[StringName("every_n")] = 3;
	technique_params[StringName("stun_duration")] = 0.5;
	technique_post_attack.append(effect_dict(StringName("every_n_attacks_stun"), technique_params));
	technique[StringName("post_attack")] = technique_post_attack;

	Dictionary demolition;
	Array demolition_post_attack;
	Dictionary demolition_params;
	demolition_params[StringName("radius")] = 0.5;
	demolition_params[StringName("ratio")] = 0.5;
	demolition_params[StringName("damage_type")] = String("physical");
	demolition_params[StringName("reason")] = String("Explosion");
	demolition_params[StringName("color")] = Array();
	demolition_post_attack.append(effect_dict(StringName("splash_damage"), demolition_params));
	demolition[StringName("post_attack")] = demolition_post_attack;

	Dictionary devotion;
	Array devotion_on_tick;
	Dictionary devotion_params;
	devotion_params[StringName("max_hp_ratio")] = 0.02;
	devotion_params[StringName("reason")] = String("Devotion");
	devotion_on_tick.append(effect_dict(StringName("heal"), devotion_params));
	devotion[StringName("on_tick")] = devotion_on_tick;

	Dictionary siphon;
	Array siphon_post_attack;
	Dictionary siphon_params;
	siphon_params[StringName("flat_amount")] = 5.0;
	siphon_post_attack.append(effect_dict(StringName("drain_target_mana_on_hit"), siphon_params));
	siphon[StringName("post_attack")] = siphon_post_attack;

	Dictionary bravery;
	Array bravery_on_attack;
	Dictionary bravery_params;
	bravery_params[StringName("min_hp_ratio")] = 0.8;
	bravery_params[StringName("multiplier")] = 1.2;
	bravery_on_attack.append(effect_dict(StringName("self_hp_threshold_multiplier"), bravery_params));
	bravery[StringName("on_attack")] = bravery_on_attack;

	_passive_registry[StringName("duelist")] = duelist;
	_passive_registry[StringName("eagle_eye")] = eagle_eye;
	_passive_registry[StringName("bastion")] = bastion;
	_passive_registry[StringName("executioner")] = executioner;
	_passive_registry[StringName("mana_font")] = mana_font;
	_passive_registry[StringName("marksman")] = marksman;
	_passive_registry[StringName("bloodlust")] = bloodlust;
	_passive_registry[StringName("rejuvenation")] = rejuvenation;
	_passive_registry[StringName("agility")] = agility;
	_passive_registry[StringName("enlightenment")] = enlightenment;
	_passive_registry[StringName("tenacity")] = tenacity;
	_passive_registry[StringName("swiftness")] = swiftness;
	_passive_registry[StringName("vampirism")] = vampirism;
	_passive_registry[StringName("technique")] = technique;
	_passive_registry[StringName("demolition")] = demolition;
	_passive_registry[StringName("devotion")] = devotion;
	_passive_registry[StringName("siphon")] = siphon;
	_passive_registry[StringName("bravery")] = bravery;
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
	Dictionary champion = _champion_for(archetype_id);
	if (champion.is_empty()) {
		UtilityFunctions::push_error(vformat("Unknown champion archetype: %s", String(archetype_id)));
		return unit;
	}

	Dictionary stats = Dictionary(champion["stats"]);
	Dictionary role_config = Dictionary(_role_configs.get(stats.get("role", StringName()), Dictionary()));
	Dictionary stat_mods = Dictionary(role_config.get("stat_mods", Dictionary()));
	Array stat_keys = stat_mods.keys();
	for (int64_t key_index = 0; key_index < stat_keys.size(); ++key_index) {
		Variant key_value = stat_keys[key_index];
		stats[key_value] = stat_mods[key_value];
	}

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
	unit.stats = stats;
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
	unit.ability_cooldown = double(stats.get("ability_cd", 0.0));
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

Dictionary TeamfightSimulationCore::_scale_priority_dict(const Dictionary &source) const {
	Dictionary scaled;
	Array keys = source.keys();
	for (int64_t index = 0; index < keys.size(); ++index) {
		Variant key = keys[index];
		scaled[key] = double(source[key]) * ROLE_PRIORITY_GLOBAL_SCALE;
	}
	return scaled;
}

Dictionary TeamfightSimulationCore::_strategy_for_unit(const UnitState &unit) const {
	StringName role = StringName(String(unit.stats.get("role", "")));
	Dictionary strategy;
	// Python parity: always provide all strategy knobs with defaults,
	// so future Python tuning doesn't silently fall back to different native defaults.
	auto apply_common_defaults = [&](Dictionary &s) {
		s["ally_distance_weight"] = s.has("ally_distance_weight") ? s["ally_distance_weight"] : Variant(1.0);
		s["ally_hp_weight"] = s.has("ally_hp_weight") ? s["ally_hp_weight"] : Variant(0.0);
		s["ally_threat_weight"] = s.has("ally_threat_weight") ? s["ally_threat_weight"] : Variant(0.0);
		s["ally_role_priorities"] = s.has("ally_role_priorities") ? s["ally_role_priorities"] : Variant(Dictionary());
		s["stickiness_bonus"] = s.has("stickiness_bonus") ? s["stickiness_bonus"] : Variant(STICKINESS_DEFAULT);
		s["prefers_kiting"] = s.has("prefers_kiting") ? s["prefers_kiting"] : Variant(false);
		s["bucket_margin"] = s.has("bucket_margin") ? s["bucket_margin"] : Variant(TARGET_BUCKET_MARGIN);
		s["bucket_order"] = s.has("bucket_order") ? s["bucket_order"] : Variant(Array());
		s["switch_margin"] = s.has("switch_margin") ? s["switch_margin"] : Variant(TARGET_SWITCH_MARGIN);
		s["in_range_bonus"] = s.has("in_range_bonus") ? s["in_range_bonus"] : Variant(IN_RANGE_BONUS_DEFAULT);
		s["tank_penalty"] = s.has("tank_penalty") ? s["tank_penalty"] : Variant(2.0);
		s["threat_response_weight"] = s.has("threat_response_weight") ? s["threat_response_weight"] : Variant(0.0);
		s["projectile_time_weight"] = s.has("projectile_time_weight") ? s["projectile_time_weight"] : Variant(0.0);
		s["execute_bonus_weight"] = s.has("execute_bonus_weight") ? s["execute_bonus_weight"] : Variant(0.0);
		s["carry_peel_weight"] = s.has("carry_peel_weight") ? s["carry_peel_weight"] : Variant(0.0);
		s["prey_instinct_weight"] = s.has("prey_instinct_weight") ? s["prey_instinct_weight"] : Variant(0.0);
		s["cluster_weight"] = s.has("cluster_weight") ? s["cluster_weight"] : Variant(0.0);
		s["spacing_weight"] = s.has("spacing_weight") ? s["spacing_weight"] : Variant(0.0);
		s["bodyguard_weight"] = s.has("bodyguard_weight") ? s["bodyguard_weight"] : Variant(0.0);
		s["obscurance_weight"] = s.has("obscurance_weight") ? s["obscurance_weight"] : Variant(0.0);
		s["flanking_weight"] = s.has("flanking_weight") ? s["flanking_weight"] : Variant(0.0);
		s["threat_decay_rate"] = s.has("threat_decay_rate") ? s["threat_decay_rate"] : Variant(THREAT_DECAY_DEFAULT);
		s["role_priorities"] = s.has("role_priorities") ? s["role_priorities"] : Variant(Dictionary());
	};
	if (role == StringName("tank")) {
		strategy["name"] = String("Protector");
		strategy["distance_weight"] = DISTANCE_WEIGHT_TANK;
		strategy["hp_weight"] = 0.0;
		strategy["ally_distance_weight"] = 1.0;
		strategy["ally_hp_weight"] = 0.0;
		strategy["ally_threat_weight"] = SCORE_THREAT_WEIGHT_SCALE * SCORE_THREAT_WEIGHT_SCALE;
		Dictionary ally_role_priorities;
		ally_role_priorities[StringName("marksman")] = -5.0 * ROLE_PRIORITY_GLOBAL_SCALE;
		ally_role_priorities[StringName("mage")] = -5.0 * ROLE_PRIORITY_GLOBAL_SCALE;
		ally_role_priorities[StringName("support")] = -3.0 * ROLE_PRIORITY_GLOBAL_SCALE;
		strategy["ally_role_priorities"] = ally_role_priorities;
		strategy["stickiness_bonus"] = STICKINESS_DEFAULT;
		strategy["in_range_bonus"] = IN_RANGE_BONUS_TANK;
		strategy["tank_penalty"] = TANK_PENALTY_TANK;
		strategy["threat_response_weight"] = THREAT_RESPONSE_TANK;
		strategy["execute_bonus_weight"] = 0.0;
		strategy["prefers_kiting"] = false;
		strategy["prey_instinct_weight"] = 0.0;
		strategy["cluster_weight"] = CLUSTER_WEIGHT_TANK;
		strategy["spacing_weight"] = 0.0;
		strategy["bodyguard_weight"] = BODYGUARD_WEIGHT_TANK;
		strategy["obscurance_weight"] = 0.0;
		strategy["carry_peel_weight"] = 0.0;
		strategy["flanking_weight"] = 0.0;
		strategy["threat_decay_rate"] = THREAT_DECAY_TANK;
		Dictionary role_priorities;
		role_priorities[StringName("assassin")] = -5.0 * ROLE_PRIORITY_GLOBAL_SCALE;
		role_priorities[StringName("fighter")] = -2.0 * ROLE_PRIORITY_GLOBAL_SCALE;
		strategy["role_priorities"] = role_priorities;
		strategy["switch_margin"] = TARGET_SWITCH_MARGIN;
		strategy["bucket_margin"] = TARGET_BUCKET_MARGIN;
		strategy["projectile_time_weight"] = 0.0;
		Array bucket_order;
		bucket_order.append(StringName("commit"));
		bucket_order.append(StringName("peel"));
		bucket_order.append(StringName("burst"));
		bucket_order.append(StringName("objective"));
		bucket_order.append(StringName("kite"));
		strategy["bucket_order"] = bucket_order;
		apply_common_defaults(strategy);
		return strategy;
	}
	if (role == StringName("assassin")) {
		strategy["name"] = String("Diver");
		strategy["distance_weight"] = DISTANCE_WEIGHT_ASSASSIN;
		strategy["hp_weight"] = HP_WEIGHT_ASSASSIN;
		strategy["stickiness_bonus"] = STICKINESS_DEFAULT;
		strategy["in_range_bonus"] = IN_RANGE_BONUS_ASSASSIN;
		strategy["tank_penalty"] = TANK_PENALTY_ASSASSIN;
		strategy["threat_response_weight"] = THREAT_RESPONSE_ASSASSIN;
		strategy["execute_bonus_weight"] = EXECUTE_BONUS_WEIGHT_DEFAULT;
		strategy["prey_instinct_weight"] = 2.0;
		strategy["prefers_kiting"] = false;
		strategy["cluster_weight"] = 0.0;
		strategy["spacing_weight"] = 0.0;
		strategy["bodyguard_weight"] = 0.0;
		strategy["obscurance_weight"] = 0.0;
		strategy["carry_peel_weight"] = 0.0;
		strategy["flanking_weight"] = FLANKING_WEIGHT_ASSASSIN;
		strategy["threat_decay_rate"] = THREAT_DECAY_FRAGILE;
		Dictionary role_priorities;
		role_priorities[StringName("marksman")] = -15.0 * ROLE_PRIORITY_GLOBAL_SCALE;
		role_priorities[StringName("mage")] = -15.0 * ROLE_PRIORITY_GLOBAL_SCALE;
		role_priorities[StringName("support")] = -10.0 * ROLE_PRIORITY_GLOBAL_SCALE;
		role_priorities[StringName("fighter")] = 10.0 * ROLE_PRIORITY_GLOBAL_SCALE;
		strategy["role_priorities"] = role_priorities;
		strategy["switch_margin"] = TARGET_SWITCH_MARGIN;
		strategy["bucket_margin"] = TARGET_BUCKET_MARGIN;
		strategy["projectile_time_weight"] = 0.0;
		Array bucket_order;
		bucket_order.append(StringName("commit"));
		bucket_order.append(StringName("burst"));
		bucket_order.append(StringName("objective"));
		bucket_order.append(StringName("peel"));
		bucket_order.append(StringName("kite"));
		strategy["bucket_order"] = bucket_order;
		apply_common_defaults(strategy);
		return strategy;
	}
	if (role == StringName("marksman")) {
		strategy["name"] = String("Kiter");
		strategy["distance_weight"] = 1.0;
		strategy["hp_weight"] = 0.5;
		strategy["stickiness_bonus"] = STICKINESS_MARKSMAN;
		strategy["in_range_bonus"] = IN_RANGE_BONUS_MARKSMAN;
		strategy["tank_penalty"] = TANK_PENALTY_MARKSMAN;
		strategy["threat_response_weight"] = THREAT_RESPONSE_MARKSMAN;
		strategy["execute_bonus_weight"] = EXECUTE_BONUS_WEIGHT_DEFAULT;
		strategy["prey_instinct_weight"] = 0.0;
		strategy["prefers_kiting"] = true;
		strategy["cluster_weight"] = 0.0;
		strategy["spacing_weight"] = SPACING_WEIGHT_MARKSMAN;
		strategy["bodyguard_weight"] = 0.0;
		strategy["obscurance_weight"] = OBSCURANCE_WEIGHT_DEFAULT;
		strategy["carry_peel_weight"] = 60.0;
		strategy["flanking_weight"] = 0.0;
		strategy["threat_decay_rate"] = THREAT_DECAY_FRAGILE;
		strategy["role_priorities"] = Dictionary();
		strategy["switch_margin"] = TARGET_SWITCH_MARGIN;
		strategy["bucket_margin"] = TARGET_BUCKET_MARGIN;
		strategy["projectile_time_weight"] = PROJECTILE_TIME_WEIGHT_MARKSMAN;
		Array bucket_order;
		bucket_order.append(StringName("commit"));
		bucket_order.append(StringName("peel"));
		bucket_order.append(StringName("burst"));
		bucket_order.append(StringName("objective"));
		bucket_order.append(StringName("kite"));
		strategy["bucket_order"] = bucket_order;
		apply_common_defaults(strategy);
		return strategy;
	}
	if (role == StringName("fighter")) {
		strategy["name"] = String("Brawler");
		strategy["distance_weight"] = DISTANCE_WEIGHT_FIGHTER_CLOSE;
		strategy["hp_weight"] = 0.5;
		strategy["stickiness_bonus"] = STICKINESS_DEFAULT;
		strategy["in_range_bonus"] = IN_RANGE_BONUS_FIGHTER;
		strategy["tank_penalty"] = TANK_PENALTY_FIGHTER;
		strategy["threat_response_weight"] = THREAT_RESPONSE_FIGHTER;
		strategy["execute_bonus_weight"] = EXECUTE_BONUS_WEIGHT_DEFAULT;
		strategy["prey_instinct_weight"] = 0.0;
		strategy["prefers_kiting"] = false;
		strategy["cluster_weight"] = 0.0;
		strategy["spacing_weight"] = 0.0;
		strategy["bodyguard_weight"] = 0.0;
		strategy["obscurance_weight"] = 0.0;
		strategy["carry_peel_weight"] = 0.0;
		strategy["flanking_weight"] = 0.0;
		strategy["threat_decay_rate"] = THREAT_DECAY_FIGHTER;
		Dictionary role_priorities;
		role_priorities[StringName("marksman")] = -1.0 * ROLE_PRIORITY_GLOBAL_SCALE;
		role_priorities[StringName("mage")] = -1.0 * ROLE_PRIORITY_GLOBAL_SCALE;
		strategy["role_priorities"] = role_priorities;
		strategy["switch_margin"] = TARGET_SWITCH_MARGIN;
		strategy["bucket_margin"] = TARGET_BUCKET_MARGIN;
		strategy["projectile_time_weight"] = 0.0;
		Array bucket_order;
		bucket_order.append(StringName("commit"));
		bucket_order.append(StringName("objective"));
		bucket_order.append(StringName("peel"));
		bucket_order.append(StringName("burst"));
		bucket_order.append(StringName("kite"));
		strategy["bucket_order"] = bucket_order;
		apply_common_defaults(strategy);
		return strategy;
	}
	if (role == StringName("mage")) {
		strategy["name"] = String("Spellcaster");
		strategy["distance_weight"] = DISTANCE_WEIGHT_MAGE;
		strategy["hp_weight"] = HP_WEIGHT_MAGE;
		strategy["stickiness_bonus"] = STICKINESS_DEFAULT;
		strategy["in_range_bonus"] = IN_RANGE_BONUS_MAGE;
		strategy["tank_penalty"] = TANK_PENALTY_MAGE;
		strategy["threat_response_weight"] = THREAT_RESPONSE_MAGE;
		strategy["execute_bonus_weight"] = EXECUTE_BONUS_WEIGHT_DEFAULT;
		strategy["prey_instinct_weight"] = 0.0;
		strategy["prefers_kiting"] = true;
		strategy["cluster_weight"] = CLUSTER_WEIGHT_MAGE;
		strategy["spacing_weight"] = SPACING_WEIGHT_MAGE;
		strategy["bodyguard_weight"] = 0.0;
		strategy["obscurance_weight"] = OBSCURANCE_WEIGHT_DEFAULT;
		strategy["carry_peel_weight"] = 60.0;
		strategy["flanking_weight"] = 0.0;
		strategy["threat_decay_rate"] = THREAT_DECAY_FRAGILE;
		Dictionary role_priorities;
		role_priorities[StringName("marksman")] = -4.0 * ROLE_PRIORITY_GLOBAL_SCALE;
		role_priorities[StringName("support")] = -2.0 * ROLE_PRIORITY_GLOBAL_SCALE;
		strategy["role_priorities"] = role_priorities;
		strategy["switch_margin"] = TARGET_SWITCH_MARGIN;
		strategy["bucket_margin"] = TARGET_BUCKET_MARGIN;
		strategy["projectile_time_weight"] = PROJECTILE_TIME_WEIGHT_MAGE;
		Array bucket_order;
		bucket_order.append(StringName("commit"));
		bucket_order.append(StringName("peel"));
		bucket_order.append(StringName("objective"));
		bucket_order.append(StringName("burst"));
		bucket_order.append(StringName("kite"));
		strategy["bucket_order"] = bucket_order;
		apply_common_defaults(strategy);
		return strategy;
	}
	if (role == StringName("support")) {
		strategy["name"] = String("Enchanter");
		strategy["distance_weight"] = DISTANCE_WEIGHT_SUPPORT;
		strategy["hp_weight"] = 0.0;
		strategy["ally_distance_weight"] = 1.0;
		strategy["ally_hp_weight"] = HP_WEIGHT_SUPPORT;
		strategy["ally_threat_weight"] = SCORE_THREAT_WEIGHT_SCALE * SCORE_THREAT_WEIGHT_SCALE;
		Dictionary ally_role_priorities;
		ally_role_priorities[StringName("marksman")] = -5.0 * ROLE_PRIORITY_GLOBAL_SCALE;
		ally_role_priorities[StringName("mage")] = -5.0 * ROLE_PRIORITY_GLOBAL_SCALE;
		ally_role_priorities[StringName("fighter")] = -2.0 * ROLE_PRIORITY_GLOBAL_SCALE;
		strategy["ally_role_priorities"] = ally_role_priorities;
		strategy["stickiness_bonus"] = STICKINESS_SUPPORT;
		strategy["in_range_bonus"] = IN_RANGE_BONUS_SUPPORT;
		strategy["tank_penalty"] = TANK_PENALTY_SUPPORT;
		strategy["threat_response_weight"] = THREAT_RESPONSE_SUPPORT;
		strategy["execute_bonus_weight"] = 0.0;
		strategy["prey_instinct_weight"] = 0.0;
		strategy["prefers_kiting"] = true;
		strategy["cluster_weight"] = 0.0;
		strategy["spacing_weight"] = SPACING_WEIGHT_SUPPORT;
		strategy["bodyguard_weight"] = BODYGUARD_WEIGHT_SUPPORT;
		strategy["obscurance_weight"] = OBSCURANCE_WEIGHT_DEFAULT;
		strategy["carry_peel_weight"] = 0.0;
		strategy["flanking_weight"] = 0.0;
		strategy["threat_decay_rate"] = THREAT_DECAY_FRAGILE;
		Dictionary role_priorities;
		role_priorities[StringName("assassin")] = -8.0 * ROLE_PRIORITY_GLOBAL_SCALE;
		role_priorities[StringName("fighter")] = -4.0 * ROLE_PRIORITY_GLOBAL_SCALE;
		strategy["role_priorities"] = role_priorities;
		strategy["switch_margin"] = TARGET_SWITCH_MARGIN;
		strategy["bucket_margin"] = TARGET_BUCKET_MARGIN;
		strategy["projectile_time_weight"] = PROJECTILE_TIME_WEIGHT_SUPPORT;
		Array bucket_order;
		bucket_order.append(StringName("commit"));
		bucket_order.append(StringName("peel"));
		bucket_order.append(StringName("objective"));
		bucket_order.append(StringName("kite"));
		bucket_order.append(StringName("burst"));
		strategy["bucket_order"] = bucket_order;
		apply_common_defaults(strategy);
		return strategy;
	}
	strategy["name"] = String("Default");
	strategy["distance_weight"] = 1.0;
	strategy["hp_weight"] = 0.0;
	strategy["ally_distance_weight"] = 1.0;
	strategy["ally_hp_weight"] = 0.0;
	strategy["ally_threat_weight"] = 0.0;
	strategy["ally_role_priorities"] = Dictionary();
	strategy["stickiness_bonus"] = STICKINESS_DEFAULT;
	strategy["in_range_bonus"] = IN_RANGE_BONUS_DEFAULT;
	strategy["tank_penalty"] = 2.0;
	strategy["threat_response_weight"] = 0.0;
	strategy["execute_bonus_weight"] = 0.0;
	strategy["prey_instinct_weight"] = 0.0;
	strategy["bodyguard_weight"] = 0.0;
	strategy["threat_decay_rate"] = THREAT_DECAY_DEFAULT;
	strategy["role_priorities"] = Dictionary();
	strategy["switch_margin"] = TARGET_SWITCH_MARGIN;
	strategy["projectile_time_weight"] = 0.0;
	apply_common_defaults(strategy);
	return strategy;
}

double TeamfightSimulationCore::_score_ally_target(const UnitState &unit, const UnitState &ally, const Dictionary &strategy) const {
	double dist = _distance_between(unit, ally);
	double hp_ratio = ally.hp / Math::max(0.0001, double(ally.stats.get("max_hp", 1.0)));
	double score = dist * double(strategy.get("ally_distance_weight", 1.0));
	score += hp_ratio * double(strategy.get("ally_hp_weight", 0.0)) * SCORE_HP_WEIGHT_SCALE;
	score -= ally.perceived_threat * double(strategy.get("ally_threat_weight", 0.0));
	score += double(Dictionary(strategy.get("ally_role_priorities", Dictionary())).get(StringName(String(ally.stats.get("role", ""))), 0.0));
	return score;
}

double TeamfightSimulationCore::_score_enemy_target(const UnitState &attacker, const UnitState &enemy, const Dictionary &strategy) const {
	double dist = _distance_between(attacker, enemy);
	double attack_range = _attack_range(attacker);
	double effective_range = _effective_attack_range(attacker);
	// Python parity: melee contact uses abs tolerance only (math.isclose rel_tol=0, abs_tol=MELEE_CONTACT_BUFFER).
	bool in_range = false;
	if (attack_range > RANGED_THRESHOLD) {
		in_range = dist <= attack_range;
	} else {
		in_range = (dist <= effective_range) || (Math::abs(dist - effective_range) <= MELEE_CONTACT_BUFFER);
	}
	double hp_ratio = enemy.hp / Math::max(0.0001, double(enemy.stats.get("max_hp", 1.0)));
	double score = 0.0;
	double range_gap = Math::max(0.0, dist - Math::max(effective_range, EPSILON));
	double norm_gap = range_gap / Math::max(effective_range, EPSILON);
	score += Math::pow(norm_gap, DISTANCE_EXPONENT) * double(strategy.get("distance_weight", 1.0)) * SCORE_DISTANCE_WEIGHT_SCALE;
	if (in_range) {
		score -= double(strategy.get("in_range_bonus", 0.0));
	}
	score += hp_ratio * double(strategy.get("hp_weight", 0.0)) * SCORE_HP_WEIGHT_SCALE;
	if (double(strategy.get("execute_bonus_weight", 0.0)) > 0.0 && in_range && hp_ratio <= TARGET_EXECUTE_HP_RATIO) {
		score -= double(strategy.get("execute_bonus_weight", 0.0));
	}
	score += double(Dictionary(strategy.get("role_priorities", Dictionary())).get(StringName(String(enemy.stats.get("role", ""))), 0.0));
	if (StringName(String(enemy.stats.get("role", ""))) == StringName("tank")) {
		score += double(strategy.get("tank_penalty", 0.0));
		// Python oracle: assassins apply an extra tank penalty if there are backliners alive.
		if (StringName(String(attacker.stats.get("role", ""))) == StringName("assassin")) {
			const std::vector<int64_t> &enemy_indices = _alive_indices_for_team(enemy.team);
			bool has_backliner = false;
			for (int64_t idx : enemy_indices) {
				const UnitState &other = _units[idx];
				if (!other.alive || other.instance_id == enemy.instance_id) {
					continue;
				}
				StringName other_role = StringName(String(other.stats.get("role", "")));
				if (other_role == StringName("marksman") || other_role == StringName("mage") || other_role == StringName("support")) {
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
		score -= double(strategy.get("threat_response_weight", 0.0)) * falloff;
	}
	double enemy_incoming = double(enemy.incoming_target_count);
	double prey_focus = enemy_incoming * PREY_INCOMING_TARGET_SCALE + enemy.perceived_threat * PREY_PERCEIVED_THREAT_SCALE;
	StringName enemy_role = StringName(String(enemy.stats.get("role", "")));
	if (enemy_role == StringName("tank") || enemy_role == StringName("fighter")) {
		prey_focus *= PREY_FRONTLINE_SCALE;
	}
	score -= prey_focus * double(strategy.get("prey_instinct_weight", 0.0));
	if (attacker.target_id == enemy.instance_id) {
		score -= Math::max(1.0, double(strategy.get("distance_weight", 1.0))) * double(strategy.get("stickiness_bonus", 0.0));
	}
	StringName attacker_role = StringName(String(attacker.stats.get("role", "")));
	if (attacker_role == StringName("support")) {
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
	double bodyguard_weight = double(strategy.get("bodyguard_weight", 0.0));
	if (bodyguard_weight > 0.0) {
		const std::vector<int64_t> &ally_indices = _alive_indices_for_team(attacker.team);
		for (int64_t ally_index : ally_indices) {
			const UnitState &ally = _units[ally_index];
			StringName ally_role = StringName(String(ally.stats.get("role", "")));
			if (ally_role != StringName("marksman") && ally_role != StringName("mage")) {
				continue;
			}
			double ally_dist = _distance_between(enemy, ally);
			if (ally_dist < BODYGUARD_RADIUS) {
				double guard_bonus = (1.0 - (ally_dist / BODYGUARD_RADIUS)) * bodyguard_weight;
				score -= guard_bonus;
			}
		}
	}

	// Cluster awareness (density).
	double cluster_weight = double(strategy.get("cluster_weight", 0.0));
	if (cluster_weight > 0.0) {
		// Python parity: score_target() uses a cached density count when provided; otherwise defaults to 0.
		// The current Python oracle does not populate this cache, so we keep it at the per-unit stored value.
		score -= double(enemy.last_density_count) * cluster_weight;
	}

	// Spacing awareness.
	double spacing_weight = double(strategy.get("spacing_weight", 0.0));
	if (spacing_weight > 0.0) {
		score += Math::pow(double(enemy.incoming_target_count), SPACING_EXPONENT) * spacing_weight;
	}

	// Carry peel (for threatened carries).
	double carry_peel_weight = double(strategy.get("carry_peel_weight", 0.0));
	if (carry_peel_weight > 0.0) {
		StringName attacker_role = StringName(String(attacker.stats.get("role", "")));
		if ((attacker_role == StringName("marksman") || attacker_role == StringName("mage")) && enemy.target_id == attacker.instance_id) {
			double falloff = 1.0 / (1.0 + Math::max(0.0, dist - attack_range) * THREAT_RESPONSE_RANGE_FALLOFF);
			score -= carry_peel_weight * falloff;
		}
	}

	// Projectile tempo: penalize time-to-hit for ranged attackers.
	double projectile_time_weight = double(strategy.get("projectile_time_weight", 0.0));
	if (projectile_time_weight > 0.0 && attack_range > RANGED_THRESHOLD) {
		double proj_speed = double(attacker.stats.get("projectile_speed", DEFAULT_PROJECTILE_SPEED));
		if (proj_speed > EPSILON) {
			double t_hit = dist / proj_speed;
			score += t_hit * projectile_time_weight;
		}
	}

	// Kiting tempo bonus inside preferred window.
	auto is_under_threat = [&](const UnitState &u) -> bool {
		return double(u.incoming_target_count) >= SUPPORT_PEEL_THREAT_THRESHOLD || u.perceived_threat >= SUPPORT_PEEL_THREAT_THRESHOLD;
	};
	// Python parity: kite tempo scoring applies whenever prefers_kiting is true.
	// Threat gating is only used for the KITE bucket classification, not this tempo term.
	if (bool(strategy.get("prefers_kiting", false))) {
		double min_w = effective_range * KITE_TARGET_WINDOW_MIN_FACTOR;
		double max_w = effective_range * KITE_TARGET_WINDOW_MAX_FACTOR;
		if (dist >= min_w && dist <= max_w && max_w > min_w) {
			double kite_ratio = (dist - min_w) / (max_w - min_w);
			score -= kite_ratio * SCORE_KITING_WEIGHT_SCALE;
		}
	}

	// Obscurance: penalize targeting past frontline blockers.
	double obscurance_weight = double(strategy.get("obscurance_weight", 0.0));
	if (obscurance_weight > 0.0) {
		const std::vector<int64_t> &enemy_indices = _alive_indices_for_team(enemy.team);
		double ux = attacker.pos_x;
		double uy = attacker.pos_y;
		double tx = enemy.pos_x;
		double ty = enemy.pos_y;
		double segx = tx - ux;
		double segy = ty - uy;
		double seg_len_sq = segx * segx + segy * segy;
		int blockers = 0;
		for (int64_t idx : enemy_indices) {
			const UnitState &other = _units[idx];
			if (other.instance_id == enemy.instance_id) {
				continue;
			}
			StringName other_role = StringName(String(other.stats.get("role", "")));
			if (other_role != StringName("tank") && other_role != StringName("fighter")) {
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
		score += double(blockers) * obscurance_weight;
	}

	// Flanking (assassin): reward isolation from enemy team center.
	double flanking_weight = double(strategy.get("flanking_weight", 0.0));
	if (flanking_weight > 0.0) {
		const std::vector<int64_t> &enemy_indices = _alive_indices_for_team(enemy.team);
		if (!enemy_indices.empty()) {
			double cx = 0.0;
			double cy = 0.0;
			int count = 0;
			for (int64_t idx : enemy_indices) {
				const UnitState &e = _units[idx];
				if (!e.alive) {
					continue;
				}
				cx += e.pos_x;
				cy += e.pos_y;
				count += 1;
			}
			if (count > 0) {
				cx /= double(count);
				cy /= double(count);
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
	}
	// Commit bucket: forced target is massively preferred (Python TAUNT_SCORE_BONUS).
	if (attacker.forced_target_id != 0 && attacker.forced_target_remaining > 0.0 && enemy.instance_id == attacker.forced_target_id) {
		score += TAUNT_SCORE_BONUS;
	}
	return score;
}

bool TeamfightSimulationCore::_should_switch(const UnitState &unit, double current_score, double new_score, const Dictionary &strategy) const {
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
			double attack_speed = Math::max(0.0001, double(unit.stats.get("attack_speed", 1.0)));
			double swing = 1.0 / attack_speed;
			double commit_window = Math::min(SWITCH_COMMIT_WINDOW_SECONDS, swing * SWITCH_COMMIT_WINDOW_SWING_FRACTION);
			if (unit.attack_cooldown <= commit_window) {
				return false;
			}
		}
	}
	double margin = double(strategy.get("switch_margin", TARGET_SWITCH_MARGIN));
	return new_score <= (current_score - margin);
}

void TeamfightSimulationCore::_set_current_target(UnitState &unit, const UnitState &target) {
	int64_t old_target_id = unit.target_id;
	int64_t new_target_id = target.instance_id;
	if (old_target_id == new_target_id) {
		return;
	}
	_adjust_target_pressure(old_target_id, new_target_id);
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

void TeamfightSimulationCore::_refresh_target_pressure() {
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

	// Python parity: cache per-unit density used by cluster scoring.
	// Python uses getattr(enemy, "_last_density_count", 0) unless explicit density is provided.
	for (int64_t index : _alive_player_indices) {
		_units[index].last_density_count = 0;
	}
	for (int64_t index : _alive_enemy_indices) {
		_units[index].last_density_count = 0;
	}
	if (_compute_density_cache) {
		auto compute_density_for_team = [&](const std::vector<int64_t> &indices) {
			const double r2 = AOE_DENSITY_RADIUS * AOE_DENSITY_RADIUS;
			for (int64_t i = 0; i < int64_t(indices.size()); ++i) {
				UnitState &u = _units[indices[i]];
				if (!u.alive) {
					u.last_density_count = 0;
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
					double d = _distance_between(u, other);
					if (d * d <= r2) {
						count += 1;
					}
				}
				u.last_density_count = count;
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
	_debug_targeting = bool(match_input.get("debug_targeting", false));
	_debug_combat_trace = bool(match_input.get("debug_combat_trace", false));
	_debug_fixture_name = String(match_input.get("debug_fixture_name", ""));
	_compute_density_cache = bool(match_input.get("compute_density_cache", false));
	_rng.seed(static_cast<uint64_t>(_seed), 54u);

	int64_t next_instance_id = 1;
	_append_team_units(Array(match_input.get("player_units", Array())), StringName("player"), next_instance_id, _player_comp);
	_append_team_units(Array(match_input.get("enemy_units", Array())), StringName("enemy"), next_instance_id, _enemy_comp);
	_refresh_target_pressure();

	if (_record_events) {
		_debug_duel_trace = _units.size() == 2;
		if (_debug_duel_trace) {
			_debug_duel_unit_a = _units[0].instance_id;
			_debug_duel_unit_b = _units[1].instance_id;
		}
		UtilityFunctions::print(String("[native-debug] seed=") + String::num_int64(_seed) + " tick_rate=" + String::num(_tick_rate));
		if (_debug_combat_trace && !_debug_fixture_name.is_empty()) {
			UtilityFunctions::print(String("[native-debug] combat_trace=ON fixture=") + _debug_fixture_name);
		}
		if (_debug_duel_trace) {
			UtilityFunctions::print(String("[native-debug] duel_trace=ON a=") + String::num_int64(_debug_duel_unit_a) + " (" + String(_units[0].archetype_id) + ") b=" + String::num_int64(_debug_duel_unit_b) + " (" + String(_units[1].archetype_id) + ")");
		}
	}
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
			double target_max_hp = Math::max(0.0001, double(context.target->stats.get("max_hp", 1.0)));
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
			double hp_ratio = context.source->hp / Math::max(0.0001, double(context.source->stats.get("max_hp", 1.0)));
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
		double armor = double(target.stats.get("armor", 0.0));
		final_damage *= Math::clamp(1.0 - armor, 0.05, 1.0);
	} else if (damage_type == StringName("magic")) {
		double mr = double(target.stats.get("magic_resist", 0.0));
		final_damage *= Math::clamp(1.0 - mr, 0.05, 1.0);
	}
	double incoming = Math::max(0.0, final_damage);
	if (incoming <= 0.0) {
		return 0.0;
	}
	bool combat_trace_window = (_debug_combat_trace && _debug_fixture_name == "backline_skirmish" && _time >= 5.0 && _time <= 9.0);
	if (_record_events && ((_is_debug_duel_unit(source.instance_id) && _is_debug_duel_unit(target.instance_id)) || combat_trace_window)) {
		UtilityFunctions::print(
			String("[native-debug] t=") + String::num(_time)
			+ " DMG pre src=" + String::num_int64(source.instance_id) + " (" + String(source.archetype_id) + ")"
			+ " -> tgt=" + String::num_int64(target.instance_id) + " (" + String(target.archetype_id) + ")"
			+ " kind=" + String(action_kind)
			+ " type=" + String(damage_type)
			+ " base=" + String::num(damage)
			+ " pre_res=" + String::num(pre_res)
			+ " incoming=" + String::num(incoming)
			+ " hp=" + String::num(target.hp)
			+ " shield=" + String::num(target.shield)
			+ " src_atk_cd=" + String::num(source.attack_cooldown)
			+ " src_cast=" + String::num(source.casting_remaining)
		);
	}
	double shield_before = target.shield;
	double absorbed = Math::min(shield_before, incoming);
	target.shield = Math::max(0.0, shield_before - absorbed);
	double hp_loss = Math::max(0.0, incoming - absorbed);
	target.hp = Math::max(0.0, target.hp - hp_loss);
	target.damage_received += hp_loss;
	target.damage_mitigated += Math::max(0.0, pre_res - final_damage);
	double total_damage = absorbed + hp_loss;
	double max_hp = double(target.stats.get("max_hp", 0.0));
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
		target.damage_sources[source.instance_id] = _time;
		target.last_hit_time = _time;
	}
	if (_record_events && ((_is_debug_duel_unit(source.instance_id) && _is_debug_duel_unit(target.instance_id)) || combat_trace_window)) {
		UtilityFunctions::print(
			String("[native-debug] t=") + String::num(_time)
			+ " DMG post total=" + String::num(total_damage)
			+ " absorbed=" + String::num(absorbed)
			+ " hp_loss=" + String::num(hp_loss)
			+ " tgt_hp=" + String::num(target.hp)
			+ " tgt_shield=" + String::num(target.shield)
		);
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

void TeamfightSimulationCore::_apply_stun(UnitState &source, UnitState &target, double duration) {
	if (duration <= 0.0) {
		return;
	}
	// Python parity: apply tenacity to reduce stun duration.
	double tenacity = double(target.stats.get("tenacity", 0.0));
	double effective_duration = duration * (1.0 - tenacity);
	if (effective_duration <= 0.0) {
		return;
	}
	target.stun_remaining = Math::max(target.stun_remaining, effective_duration);
	target.damage_sources[source.instance_id] = _time;
	source.stuns += 1;
}

void TeamfightSimulationCore::_add_shield(UnitState &source, UnitState &target, double amount) {
	if (amount <= 0.0) {
		return;
	}
	target.shield += amount;
	source.shielding_done += amount;
	if (source.instance_id != target.instance_id) {
		target.recent_benefactors[source.instance_id] = _time;
	}
}

void TeamfightSimulationCore::_heal_unit(UnitState &source, UnitState &target, double amount) {
	if (amount <= 0.0) {
		return;
	}
	double max_hp = double(target.stats.get("max_hp", 0.0));
	double old_hp = target.hp;
	double new_hp = Math::min(max_hp, old_hp + amount);
	target.hp = new_hp;
	source.healing_done += amount;
	if (source.instance_id != target.instance_id) {
		target.recent_benefactors[source.instance_id] = _time;
	}
}

void TeamfightSimulationCore::_restore_mana(UnitState &source, UnitState &target, double amount) {
	if (amount <= 0.0) {
		return;
	}
	double max_mana = double(target.stats.get("max_mana", 0.0));
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

TeamfightSimulationCore::UnitState *TeamfightSimulationCore::_select_enemy_target(UnitState &unit) {
	auto bucket_rank_for = [&](const Array &order, const StringName &bucket) -> int {
		for (int i = 0; i < order.size(); ++i) {
			if (StringName(String(order[i])) == bucket) {
				return i;
			}
		}
		return order.size();
	};
	auto is_under_threat = [&](const UnitState &u) -> bool {
		return double(u.incoming_target_count) >= SUPPORT_PEEL_THREAT_THRESHOLD || u.perceived_threat >= SUPPORT_PEEL_THREAT_THRESHOLD;
	};
	auto classify_bucket = [&](const UnitState &candidate, double dist, const Dictionary &strategy) -> StringName {
		// Python forced-target model: forced target is the COMMIT bucket.
		if (unit.forced_target_id != 0 && unit.forced_target_remaining > 0.0 && candidate.instance_id == unit.forced_target_id) {
			return StringName("commit");
		}
		StringName bucket = StringName("objective");
		double carry_peel_weight = double(strategy.get("carry_peel_weight", 0.0));
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
		if (double(strategy.get("execute_bonus_weight", 0.0)) > 0.0) {
			double hp_ratio = candidate.hp / Math::max(0.0001, double(candidate.stats.get("max_hp", 1.0)));
			double atk_dmg = double(unit.stats.get("attack_damage", 0.0));
			if (hp_ratio <= TARGET_EXECUTE_HP_RATIO || candidate.hp <= atk_dmg) {
				return StringName("burst");
			}
		}
		// Kite bucket.
		if (bool(strategy.get("prefers_kiting", false)) && !is_under_threat(unit)) {
			double effective_range = _effective_attack_range(unit);
			double min_w = effective_range * KITE_TARGET_WINDOW_MIN_FACTOR;
			double max_w = effective_range * KITE_TARGET_WINDOW_MAX_FACTOR;
			if (dist >= min_w && dist <= max_w) {
				return StringName("kite");
			}
		}
		return bucket;
	};
	auto adjusted_score_for = [&](const UnitState &candidate, double raw, double dist, const Dictionary &strategy, const Array &bucket_order) -> double {
		StringName bucket = classify_bucket(candidate, dist, strategy);
		int rank = bucket_rank_for(bucket_order, bucket);
		double bucket_margin = double(strategy.get("bucket_margin", TARGET_BUCKET_MARGIN));
		return raw + double(rank) * bucket_margin;
	};

	// Python forced-target model: if a forced target is active, selection collapses to it.
	int64_t forced_target_id = unit.forced_target_id;
	if (forced_target_id != 0 && unit.forced_target_remaining > 0.0) {
		UnitState *forced_target = _unit_by_id(forced_target_id);
		if (forced_target != nullptr && forced_target->alive && forced_target->team != unit.team) {
			if (_record_events && _debug_targeting) {
				UtilityFunctions::print(
					String("[native-debug] t=") + String::num(_time)
					+ " TARGET forced(" + String(unit.forced_target_kind) + ") src=" + String::num_int64(unit.instance_id)
					+ " -> " + String::num_int64(forced_target->instance_id)
				);
			}
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
			if (_record_events && _debug_targeting) {
				UtilityFunctions::print(
					String("[native-debug] t=") + String::num(_time)
					+ " TARGET forced(taunt) src=" + String::num_int64(unit.instance_id) + " (" + String(unit.archetype_id) + ")"
					+ " -> " + String::num_int64(taunt_target->instance_id) + " (" + String(taunt_target->archetype_id) + ")"
				);
			}
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

	Dictionary strategy = _strategy_for_unit(unit);
	Array bucket_order = Array(strategy.get("bucket_order", Array()));
	UnitState *best = nullptr;
	double best_adjusted = std::numeric_limits<double>::infinity();
	double best_raw = std::numeric_limits<double>::infinity();
	int best_bucket_rank = 0;
	double best_dist = std::numeric_limits<double>::infinity();
	StringName enemy_team = unit.team == StringName("player") ? StringName("enemy") : StringName("player");
	const std::vector<int64_t> &enemy_indices = _alive_indices_for_team(enemy_team);

	// Assassin frontline pressure bypass: evaluate even if retarget_timer > 0.
	bool assassin_pressuring_frontline = false;
	StringName unit_role = StringName(String(unit.stats.get("role", "")));
	if (!forced_reason && unit_role == StringName("assassin") && current_target != nullptr) {
		StringName cur_role = StringName(String(current_target->stats.get("role", "")));
		if (cur_role == StringName("tank") || cur_role == StringName("fighter")) {
			for (int64_t idx : enemy_indices) {
				const UnitState &enemy = _units[idx];
				if (!enemy.alive) {
					continue;
				}
				StringName er = StringName(String(enemy.stats.get("role", "")));
				if (er == StringName("marksman") || er == StringName("mage") || er == StringName("support")) {
					assassin_pressuring_frontline = true;
					break;
				}
			}
		}
	}

	// Periodic keep path: if we are within retarget interval and no special conditions, just keep current target.
	if (!forced_reason && unit.retarget_timer > 0.0 && !assassin_pressuring_frontline) {
		// Match Python: skip evaluation entirely; just return current target score (no switching).
		unit.current_target_score = _score_enemy_target(unit, *current_target, strategy);
		if (_record_events && _debug_targeting) {
			UtilityFunctions::print(
				String("[native-debug] t=") + String::num(_time)
				+ " TARGET hold src=" + String::num_int64(unit.instance_id)
				+ " lock=" + String::num(unit.target_switch_lock_timer)
				+ " retarget=" + String::num(unit.retarget_timer)
				+ " target=" + String::num_int64(current_target->instance_id)
				+ " score=" + String::num(unit.current_target_score)
			);
		}
		_set_current_target(unit, *current_target);
		return current_target;
	}

	// Python: whenever we do evaluate, we reset the retarget timer immediately (even if we end up keeping).
	unit.retarget_timer = RETARGET_INTERVAL;

	for (int64_t enemy_index : enemy_indices) {
		UnitState &candidate = _units[enemy_index];
		double raw = _score_enemy_target(unit, candidate, strategy);
		double dist = _distance_between(unit, candidate);
		StringName bucket = classify_bucket(candidate, dist, strategy);
		int rank = bucket_rank_for(bucket_order, bucket);
		double adjusted = adjusted_score_for(candidate, raw, dist, strategy, bucket_order);
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
		if (_record_events && _debug_targeting) {
			UtilityFunctions::print(
				String("[native-debug] t=") + String::num(_time)
				+ " TARGET pick(" + reason + ") src=" + String::num_int64(unit.instance_id)
				+ " -> " + String::num_int64(best->instance_id)
				+ " score=" + String::num(best_raw)
			);
		}
		return best;
	}

	double current_score = _score_enemy_target(unit, *current_target, strategy);
	double current_dist = _distance_between(unit, *current_target);
	if (best->instance_id == current_target->instance_id) {
		unit.current_target_score = current_score;
		if (_record_events && _debug_targeting) {
			UtilityFunctions::print(
				String("[native-debug] t=") + String::num(_time)
				+ " TARGET kept target src=" + String::num_int64(unit.instance_id)
				+ " cur=" + String::num_int64(current_target->instance_id)
				+ " cur_score=" + String::num(current_score)
				+ " best=" + String::num_int64(best->instance_id)
				+ " best_score=" + String::num(best_raw)
			);
		}
		_set_current_target(unit, *current_target);
		return current_target;
	}

	if (assassin_pressuring_frontline) {
		StringName best_role = StringName(String(best->stats.get("role", "")));
		bool best_is_backliner = (best_role == StringName("marksman") || best_role == StringName("mage") || best_role == StringName("support"));
		if (best_is_backliner) {
			_set_current_target(unit, *best);
			unit.target_switch_lock_timer = 0.0;
			unit.current_target_score = best_raw;
			if (_record_events && _debug_targeting) {
				UtilityFunctions::print(
					String("[native-debug] t=") + String::num(_time)
					+ " TARGET assassin breaks frontline lock src=" + String::num_int64(unit.instance_id)
					+ " " + String::num_int64(current_target->instance_id) + "->" + String::num_int64(best->instance_id)
					+ " best_score=" + String::num(best_raw)
				);
			}
			return best;
		}
	}

	if (!_should_switch(unit, current_score, best_raw, strategy)) {
		unit.current_target_score = current_score;
		if (_record_events && _debug_targeting) {
			UtilityFunctions::print(
				String("[native-debug] t=") + String::num(_time)
				+ " TARGET kept target(src=margin) src=" + String::num_int64(unit.instance_id)
				+ " cur=" + String::num_int64(current_target->instance_id)
				+ " cur_score=" + String::num(current_score)
				+ " best=" + String::num_int64(best->instance_id)
				+ " best_score=" + String::num(best_raw)
			);
		}
		_set_current_target(unit, *current_target);
		return current_target;
	}

	_set_current_target(unit, *best);
	unit.target_switch_lock_timer = TARGET_SWITCH_LOCK_DURATION;
	unit.current_target_score = best_raw;
	if (_record_events && _debug_targeting) {
		UtilityFunctions::print(
			String("[native-debug] t=") + String::num(_time)
			+ " TARGET switched src=" + String::num_int64(unit.instance_id)
			+ " " + String::num_int64(current_target->instance_id) + "->" + String::num_int64(best->instance_id)
			+ " cur_score=" + String::num(current_score)
			+ " best_score=" + String::num(best_raw)
		);
	}
	return best;
}

TeamfightSimulationCore::UnitState *TeamfightSimulationCore::_select_ally_target(UnitState &unit) {
	std::vector<int64_t> alive_allies;
	const std::vector<int64_t> &ally_indices = _alive_indices_for_team(unit.team);
	for (int64_t ally_index : ally_indices) {
		alive_allies.push_back(ally_index);
	}
	if (alive_allies.empty()) {
		unit.current_ally_target_id = 0;
		return nullptr;
	}
	std::vector<int64_t> critical_allies;
	for (int64_t ally_index : alive_allies) {
		UnitState &candidate = _units[ally_index];
		double hp_ratio = candidate.hp / Math::max(0.0001, double(candidate.stats.get("max_hp", 1.0)));
		if (hp_ratio <= ALLY_CRITICAL_HP_RATIO) {
			critical_allies.push_back(ally_index);
		}
	}
	const std::vector<int64_t> &pool = critical_allies.empty() ? alive_allies : critical_allies;
	Dictionary strategy = _strategy_for_unit(unit);
	UnitState *best = nullptr;
	double best_score = std::numeric_limits<double>::infinity();
	double best_dist = std::numeric_limits<double>::infinity();
	double best_hp_ratio = std::numeric_limits<double>::infinity();
	for (int64_t ally_index : pool) {
		UnitState &candidate = _units[ally_index];
		double score = _score_ally_target(unit, candidate, strategy);
		double dist = _distance_between(unit, candidate);
		double hp_ratio = candidate.hp / Math::max(0.0001, double(candidate.stats.get("max_hp", 1.0)));
		if (critical_allies.empty()) {
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
	return double(unit.stats.get("attack_range", 0.0));
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
	if (_record_events && target.archetype_id == StringName("guardian")) {
		UtilityFunctions::print(String("[native-debug] t=") + String::num(_time) + " guardian DIED mana=" + String::num(target.mana) + " ult_cd=" + String::num(target.ultimate_cooldown));
	}
	int64_t target_id = target.instance_id;
	int64_t target_index = _unit_index_by_id(target_id);
	if (target_index >= 0) {
		_remove_alive_index(target.team, target_index);
	}
	target.alive = false;
	target.respawn_timer = double(target.stats.get("respawn_time", RESPAWN_TIME));
	target.deaths += 1;

	const std::unordered_map<int64_t, double> &damage_sources = target.damage_sources;
	int64_t killer_id = 0;
	for (const auto &entry : damage_sources) {
		int64_t source_id = entry.first;
		double hit_time = entry.second;
		if (_time - hit_time > ASSIST_WINDOW) {
			continue;
		}
		if (hit_time > (damage_sources.count(killer_id) ? damage_sources.at(killer_id) : -1.0) || killer_id == 0) {
			killer_id = source_id;
		}
	}

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
		double hit_time = entry.second;
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
	if (_record_events && unit.archetype_id == StringName("guardian")) {
		UtilityFunctions::print(String("[native-debug] t=") + String::num(_time) + " guardian RESPAWN");
	}
	int64_t unit_index = _unit_index_by_id(unit.instance_id);
	unit.alive = true;
	unit.hp = double(unit.stats.get("max_hp", 0.0));
	unit.mana = 0.0;
	unit.shield = 0.0;
	unit.perceived_threat = 0.0;
	unit.ability_cooldown = double(unit.stats.get("ability_cd", 0.0));
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

void TeamfightSimulationCore::_step_tick() {
	// Python World.step(): tick++ then time = tick * tick_rate
	_tick += 1;
	_time = double(_tick) * _tick_rate;
	for (UnitState &unit : _units) {
		unit.respawned_this_tick = false;
		unit.cast_resolved_this_tick = false;
		if (unit.alive) {
			unit.retarget_timer = Math::max(0.0, unit.retarget_timer - _tick_rate);
			unit.target_switch_lock_timer = Math::max(0.0, unit.target_switch_lock_timer - _tick_rate);
		}
	}
	// Python: projectiles resolve before unit updates.
	_update_projectiles();
	_refresh_target_pressure(); // includes optional density cache
	// Python: units updated in instance_id order (our _units are built in that order).
	for (UnitState &unit : _units) {
		_update_unit(unit);
	}
	_refresh_target_pressure();
	// Position trace for 2-unit duel debugging.
	if (_units.size() == 2 && _time >= 39.8 && _time <= 58.0) {
		String line = String("[duel-pos] t=") + String::num(_time);
		for (const UnitState &u : _units) {
			line += String(" ") + String(u.archetype_id) +
					"(" + (u.alive ? "A" : "D") + ")" +
					" pos=(" + String::num(u.pos_x, 4) + "," + String::num(u.pos_y, 4) + ")" +
					" atk=" + String::num(u.attack_cooldown, 3) +
					" kit=" + String::num(u.last_kite_timer, 3) +
					" rsp=" + String::num(u.respawn_timer, 2);
		}
		UtilityFunctions::print(line);
	}
}

void TeamfightSimulationCore::_simulate() {
	double effective_tick_rate = Math::max(_tick_rate, EPSILON);
	int64_t max_ticks = int64_t(Math::ceil(MATCH_DURATION / effective_tick_rate));
	for (int64_t tick_index = 0; tick_index < max_ticks; ++tick_index) {
		_step_tick();
	}
	_winner_team = _determine_winner();
}

void TeamfightSimulationCore::_update_unit(UnitState &unit) {
	// Lifecycle/respawn.
	if (!unit.alive) {
		unit.respawn_timer = Math::max(0.0, unit.respawn_timer - _tick_rate);
		if (unit.respawn_timer <= 0.0) {
			_respawn_unit(unit);
		}
		return;
	}

	Dictionary strategy = _strategy_for_unit(unit);

	// Cooldowns/status (Python Unit.update begins with these decrements).
	unit.attack_cooldown = Math::max(0.0, unit.attack_cooldown - _tick_rate);
	unit.ability_cooldown = Math::max(0.0, unit.ability_cooldown - _tick_rate);
	unit.ultimate_cooldown = Math::max(0.0, unit.ultimate_cooldown - _tick_rate);
	unit.perceived_threat = Math::max(0.0, unit.perceived_threat - double(strategy.get("threat_decay_rate", THREAT_DECAY_DEFAULT)) * _tick_rate);
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

	// Regen tick effects.
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

	// Stun: no actions/movement (Python parity: stun pauses cast, checked before casting).
	if (unit.stun_remaining > 0.0) {
		return;
	}

	// Casting continuation/resolution.
	if (unit.casting_remaining > 0.0) {
		unit.casting_remaining = Math::max(0.0, unit.casting_remaining - _tick_rate);
		if (unit.casting_remaining <= 0.0) {
			_resolve_cast(unit);
			unit.cast_resolved_this_tick = true;
		}
		return;
	}

	// Separation (Python: before targeting/actions).
	{
		double move_speed = double(unit.stats.get("move_speed", 0.0));
		if (move_speed > 0.0) {
			double attack_range = double(unit.stats.get("attack_range", 0.0));
			double radius = attack_range > SEPARATION_RANGE_THRESHOLD ? SEPARATION_RADIUS_RANGED : SEPARATION_RADIUS_MELEE;
			double r2 = radius * radius;
			double ux = unit.pos_x;
			double uy = unit.pos_y;
			double sep_x = 0.0;
			double sep_y = 0.0;
			const std::vector<int64_t> &ally_indices = _alive_indices_for_team(unit.team);
			for (int64_t idx : ally_indices) {
				const UnitState &ally = _units[idx];
				if (!ally.alive || ally.instance_id == unit.instance_id) {
					continue;
				}
				double ax = ally.pos_x;
				double ay = ally.pos_y;
				double dx = ux - ax;
				double dy = uy - ay;
				double d2 = dx * dx + dy * dy;
				if (d2 <= EPSILON || d2 >= r2) {
					continue;
				}
				double d = Math::sqrt(d2);
				double force = (radius - d) / radius;
				sep_x += (dx / d) * force;
				sep_y += (dy / d) * force;
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

	// Targets.
	UnitState *target_ally = _select_ally_target(unit);
	unit.current_ally_target_id = target_ally == nullptr ? 0 : target_ally->instance_id;
	UnitState *target = _select_enemy_target(unit);
	if (target == nullptr) {
		unit.target_id = 0;
		return;
	}

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

	// Movement: kite first if applicable, else move toward.
	if (bool(strategy.get("prefers_kiting", false)) && unit.attack_cooldown > 0.0 && unit.taunt_remaining <= 0.0) {
		if (_kite_from_enemies(unit)) {
			return;
		}
	}
	_move_toward_target(unit, *target);
}

void TeamfightSimulationCore::_process_actions() {
	for (UnitState &unit : _units) {
		if (!unit.alive || unit.respawned_this_tick || unit.cast_resolved_this_tick || unit.casting_remaining > 0.0 || unit.stun_remaining > 0.0) {
			continue;
		}
		// Python parity: apply lightweight ally separation before targeting/actions.
		{
			double move_speed = double(unit.stats.get("move_speed", 0.0));
			if (move_speed > 0.0) {
				double attack_range = double(unit.stats.get("attack_range", 0.0));
				double radius = attack_range > SEPARATION_RANGE_THRESHOLD ? SEPARATION_RADIUS_RANGED : SEPARATION_RADIUS_MELEE;
				double r2 = radius * radius;
				double ux = unit.pos_x;
				double uy = unit.pos_y;
				double sep_x = 0.0;
				double sep_y = 0.0;
				const std::vector<int64_t> &ally_indices = _alive_indices_for_team(unit.team);
				for (int64_t idx : ally_indices) {
					const UnitState &ally = _units[idx];
					if (!ally.alive || ally.instance_id == unit.instance_id) {
						continue;
					}
					double ax = ally.pos_x;
					double ay = ally.pos_y;
					double dx = ux - ax;
					double dy = uy - ay;
					double d2 = dx * dx + dy * dy;
					if (d2 <= EPSILON || d2 >= r2) {
						continue;
					}
					double d = Math::sqrt(d2);
					double force = (radius - d) / radius;
					sep_x += (dx / d) * force;
					sep_y += (dy / d) * force;
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
		UnitState *target_ally = _select_ally_target(unit);
		unit.current_ally_target_id = target_ally == nullptr ? 0 : target_ally->instance_id;
		UnitState *target = _select_enemy_target(unit);
		if (target == nullptr) {
			continue;
		}
		double distance = _distance_between(unit, *target);
		double effective_range = _effective_attack_range(unit);
		bool in_contact = (distance <= effective_range) || (Math::abs(distance - effective_range) <= MELEE_CONTACT_BUFFER);
		if (_debug_combat_trace && (unit.instance_id == 5 || unit.instance_id == 2) && _debug_fixture_name == String("backline_skirmish") && _time >= 3.8 && _time <= 4.8) {
			UtilityFunctions::print(
					String("[native-debug] t=") + String::num(_time) + " PREACT src=" + String::num_int64(unit.instance_id) + " tgt=" + String::num(target->instance_id) +
					" dist=" + String::num(distance) + " eff=" + String::num(effective_range) + " in_contact=" + String(in_contact ? "true" : "false") +
					" abil_cd=" + String::num(unit.ability_cooldown) + " ult_cd=" + String::num(unit.ultimate_cooldown) + " atk_cd=" + String::num(unit.attack_cooldown) +
					" stun=" + String::num(unit.stun_remaining) + " cast=" + String::num(unit.casting_remaining) + " mana=" + String::num(unit.mana) +
				" pos=(" + String::num(unit.pos_x) + "," + String::num(unit.pos_y) + ")" +
				" tgt_pos=(" + String::num(target->pos_x) + "," + String::num(target->pos_y) + ")");
		}
		if (in_contact) {
			if (_try_cast_ultimate(unit, *target, distance)) {
				continue;
			}
			if (_try_cast_ability(unit, *target, distance)) {
				continue;
			}
		}
		if (in_contact && unit.attack_cooldown <= 0.0) {
			_perform_auto_attack(unit, *target, distance);
		} else {
			Dictionary strategy = _strategy_for_unit(unit);
			// Python parity: kiting movement for prefers_kiting units while on cooldown and not taunted.
			if (bool(strategy.get("prefers_kiting", false)) && unit.attack_cooldown > 0.0 && unit.taunt_remaining <= 0.0) {
				if (_kite_from_enemies(unit)) {
					continue;
				}
			}
			_move_toward_target(unit, *target);
		}
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
	double max_mana = double(unit.stats.get("max_mana", 0.0));
	if (unit.ultimate_cooldown > 0.0 || max_mana <= 0.0 || unit.mana < max_mana) {
		return false;
	}
	if (_record_events && unit.archetype_id == StringName("guardian")) {
		UtilityFunctions::print(String("[native-debug] t=") + String::num(_time) + " guardian ULT READY mana=" + String::num(unit.mana) + " cd=" + String::num(unit.ultimate_cooldown));
	}
	return _start_cast(unit, target, distance, StringName("ultimate"));
}

bool TeamfightSimulationCore::_start_cast(UnitState &unit, UnitState &target, double distance, const StringName &action_kind) {
	(void)distance;
	bool has_effect = action_kind == StringName("ability") ? unit.has_ability_effect : unit.has_ultimate_effect;
	if (!has_effect) {
		return false;
	}
	if (_units.size() == 2) {
		UtilityFunctions::print(String("[duel] t=") + String::num(_time) + " CAST " + String(unit.archetype_id) + " kind=" + String(action_kind) + " abil_cd=" + String::num(unit.ability_cooldown));
	}
	UnitState *target_ally = _select_ally_target(unit);
	if (action_kind == StringName("ability")) {
		unit.ability_cooldown = double(unit.stats.get("ability_cd", 0.0));
		unit.abilities += 1;
	} else {
		unit.ultimate_cooldown = double(unit.stats.get("ultimate_cd", 0.0));
		unit.ultimates += 1;
		unit.mana = Math::max(0.0, unit.mana - double(unit.stats.get("max_mana", 0.0)));
	}
	unit.casting_remaining = CASTING_WINDUP;
	unit.casting_kind = action_kind;
	unit.casting_effect = action_kind == StringName("ability") ? unit.ability_effect : unit.ultimate_effect;
	unit.has_casting_effect = true;
	unit.casting_target_id = unit.target_id != 0 ? unit.target_id : target.instance_id;
	unit.casting_ally_target_id = unit.current_ally_target_id != 0 ? unit.current_ally_target_id : (target_ally == nullptr ? 0 : target_ally->instance_id);

	if (
		_record_events
		&& (
			_is_debug_duel_unit(unit.instance_id)
			|| (_debug_combat_trace && _debug_fixture_name == "backline_skirmish" && _time >= 5.0 && _time <= 9.0)
		)
	) {
		UtilityFunctions::print(
			String("[native-debug] t=") + String::num(_time)
			+ " CAST start kind=" + String(action_kind)
			+ " src=" + String::num_int64(unit.instance_id) + " (" + String(unit.archetype_id) + ")"
			+ " tgt=" + String::num_int64(unit.casting_target_id)
			+ " dist=" + String::num(distance)
			+ " mana=" + String::num(unit.mana)
			+ " abil_cd=" + String::num(unit.ability_cooldown)
			+ " ult_cd=" + String::num(unit.ultimate_cooldown)
			+ " atk_cd=" + String::num(unit.attack_cooldown)
			+ " hp=" + String::num(unit.hp)
		);
	}

	if (_record_events && unit.archetype_id == StringName("guardian") && action_kind == StringName("ultimate")) {
		UtilityFunctions::print(String("[native-debug] t=") + String::num(_time) + " guardian START ultimate mana=" + String::num(unit.mana) + " cd=" + String::num(unit.ultimate_cooldown));
	}
	return true;
}

void TeamfightSimulationCore::_resolve_cast(UnitState &unit) {
	EffectRecord effect = unit.casting_effect;
	StringName action_kind = unit.casting_kind;
	UnitState *target = _unit_by_id(unit.casting_target_id);
	UnitState *target_ally = _unit_by_id(unit.casting_ally_target_id);
	bool had_effect = unit.has_casting_effect;
	int64_t debug_target_id = unit.casting_target_id;
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
			unit.mana = Math::min(double(unit.stats.get("max_mana", 0.0)), unit.mana + double(unit.stats.get("max_mana", 0.0)));
		}
		return;
	}
	if (
		_record_events
		&& (
			_is_debug_duel_unit(unit.instance_id)
			|| (_debug_combat_trace && _debug_fixture_name == "backline_skirmish" && _time >= 5.0 && _time <= 9.0)
		)
	) {
		UtilityFunctions::print(
			String("[native-debug] t=") + String::num(_time)
			+ " CAST resolve kind=" + String(action_kind)
			+ " src=" + String::num_int64(unit.instance_id) + " (" + String(unit.archetype_id) + ")"
			+ " tgt=" + String::num_int64(debug_target_id)
			+ " hp=" + String::num(unit.hp)
			+ " mana=" + String::num(unit.mana)
		);
	}
	EffectContext context = _build_context(unit, target, target_ally, 0.0, action_kind);
	_execute_effect(effect, context);
}

void TeamfightSimulationCore::_perform_auto_attack(UnitState &unit, UnitState &target, double distance) {
	unit.auto_attacks += 1;
	unit.attack_count += 1;
	if (_units.size() == 2) {
		UtilityFunctions::print(String("[duel] t=") + String::num(_time) + " AUTO " + String(unit.archetype_id) + "->" + String(target.archetype_id) + " atk_cd=" + String::num(unit.attack_cooldown) + " dist=" + String::num(distance));
	}
	if (
		_record_events
		&& (
			(_is_debug_duel_unit(unit.instance_id) && _is_debug_duel_unit(target.instance_id))
			|| (_debug_combat_trace && _debug_fixture_name == "backline_skirmish" && _time >= 5.0 && _time <= 9.0)
		)
	) {
		UtilityFunctions::print(
			String("[native-debug] t=") + String::num(_time)
			+ " AUTO src=" + String::num_int64(unit.instance_id) + " (" + String(unit.archetype_id) + ")"
			+ " -> tgt=" + String::num_int64(target.instance_id) + " (" + String(target.archetype_id) + ")"
			+ " dist=" + String::num(distance)
			+ " mana=" + String::num(unit.mana)
			+ " src_hp=" + String::num(unit.hp)
			+ " tgt_hp=" + String::num(target.hp)
		);
	}
	// Python parity: damage + on_attack modifiers first, then projectile/hit,
	// then mana gain, then attack cooldown (resolve_attack → mana → cooldown).
	double damage = double(unit.stats.get("attack_damage", 0.0));
	damage = _apply_attack_modifiers(unit, target, distance, damage);
	if (double(unit.stats.get("attack_range", 0.0)) > RANGED_THRESHOLD) {
		ProjectileState projectile;
		projectile.source_id = unit.instance_id;
		projectile.target_id = target.instance_id;
		projectile.damage = damage;
		projectile.damage_type = StringName("physical");
		projectile.stun_duration = DEFAULT_PROJECTILE_STUN_DURATION;
		projectile.radius = double(unit.stats.get("projectile_radius", DEFAULT_PROJECTILE_RADIUS));
		double projectile_speed = Math::max(0.0001, double(unit.stats.get("projectile_speed", DEFAULT_PROJECTILE_SPEED)));
		projectile.time_remaining = distance / projectile_speed;
		projectile.action_kind = StringName("auto");
		projectile.reason = String("Auto Attack");
		_projectiles.push_back(projectile);
	} else {
		EffectContext context = _build_context(unit, &target, nullptr, damage, StringName("auto"));
		double dealt = _apply_damage(unit, target, damage, StringName("physical"), StringName("auto"), context);
		_run_post_attack_effects(unit, target, dealt, context);
		double life_steal = double(unit.stats.get("life_steal", 0.0));
		if (life_steal > 0.0) {
			_heal_unit(unit, unit, dealt * life_steal);
		}
	}
	// Python parity: mana gain happens after attack resolution.
	double mana_gain = double(unit.stats.get("mana_per_attack", 0.0));
	if (mana_gain > 0.0) {
		double max_mana = double(unit.stats.get("max_mana", 0.0));
		unit.mana = Math::min(max_mana, unit.mana + mana_gain);
	}
	// Python parity: attack cooldown set after mana gain.
	double attack_speed = Math::max(0.0001, double(unit.stats.get("attack_speed", 1.0)));
	unit.attack_cooldown = 1.0 / attack_speed;
}

void TeamfightSimulationCore::_move_toward_target(UnitState &unit, UnitState &target) {
	double speed = double(unit.stats.get("move_speed", 0.0)) * _tick_rate;
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
	if (_debug_combat_trace && (unit.instance_id == 5 || unit.instance_id == 2) && _debug_fixture_name == String("backline_skirmish") && _time >= 3.8 && _time <= 4.8) {
		UtilityFunctions::print(String("[native-debug] t=") + String::num(_time) + " MOVE src=" + String::num_int64(unit.instance_id) + " dist=" + String::num(distance) + " eff=" + String::num(effective_range) + " speed=" + String::num(speed) + " desired=" + String::num(desired_step) + " step=" + String::num(max_step));
	}
	if (max_step <= 0.0) {
		return;
	}
	double nx = dx / distance;
	double ny = dy / distance;
	unit.pos_x = Math::clamp(unit.pos_x + nx * max_step, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
	unit.pos_y = Math::clamp(unit.pos_y + ny * max_step, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
}

bool TeamfightSimulationCore::_kite_from_enemies(UnitState &unit) {
	StringName enemy_team = unit.team == StringName("player") ? StringName("enemy") : StringName("player");
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
	double move_speed = double(unit.stats.get("move_speed", 0.0));
	double step = move_speed * KITE_SPEED_MODIFIER * _tick_rate;
	unit.pos_x = Math::clamp(ux + vel_x * step, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
	unit.pos_y = Math::clamp(uy + vel_y * step, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
	if (_debug_combat_trace && (unit.instance_id == 5 || unit.instance_id == 2) && _debug_fixture_name == String("backline_skirmish") && _time >= 3.8 && _time <= 4.8) {
		UtilityFunctions::print(String("[native-debug] t=") + String::num(_time) + " KITE src=" + String::num_int64(unit.instance_id) + " step=" + String::num(step) + " vel=(" + String::num(vel_x) + "," + String::num(vel_y) + ") pos=(" + String::num(unit.pos_x) + "," + String::num(unit.pos_y) + ")");
	}
	return true;
}

void TeamfightSimulationCore::_resolve_projectile(const ProjectileState &projectile) {
	UnitState *source = _unit_by_id(projectile.source_id);
	UnitState *target = _unit_by_id(projectile.target_id);
	if (source == nullptr || target == nullptr || !source->alive || !target->alive) {
		return;
	}
	double damage = projectile.damage;
	StringName damage_type = projectile.damage_type;
	StringName action_kind = projectile.action_kind;
	EffectContext context = _build_context(*source, target, nullptr, damage, action_kind);
	double dealt = _apply_damage(*source, *target, damage, damage_type, action_kind, context);
	_run_post_attack_effects(*source, *target, dealt, context);
	if (projectile.action_kind == StringName("auto")) {
		double life_steal = double(source->stats.get("life_steal", 0.0));
		if (life_steal > 0.0) {
			_heal_unit(*source, *source, dealt * life_steal);
		}
	}
	if (projectile.stun_duration > 0.0 && target->alive) {
		_apply_stun(*source, *target, projectile.stun_duration);
	}
}

void TeamfightSimulationCore::_update_projectiles() {
	_scratch_projectiles.clear();
	for (const ProjectileState &data : _projectiles) {
		UnitState *target = _unit_by_id(data.target_id);
		if (target == nullptr || !target->alive) {
			continue;
		}
		ProjectileState next = data;
		next.time_remaining = data.time_remaining - _tick_rate;
		if (next.time_remaining > 0.0) {
			_scratch_projectiles.push_back(next);
			continue;
		}
		_resolve_projectile(data);
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
			double damage = double(source.stats.get("attack_damage", 0.0)) * effect.scalar0;
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
			projectile_state.damage = double(source.stats.get("attack_damage", 0.0)) * effect.scalar2;
			projectile_state.damage_type = effect.damage_type.is_empty() ? StringName("physical") : effect.damage_type;
			projectile_state.stun_duration = effect.scalar3;
			// Python parity: null speed/radius override → fall back to unit's projectile stats.
			double projectile_speed = (effect.scalar0 < 0.0)
				? Math::max(0.0001, double(source.stats.get("projectile_speed", DEFAULT_PROJECTILE_SPEED)))
				: Math::max(0.0001, effect.scalar0);
			projectile_state.radius = (effect.scalar1 < 0.0)
				? double(source.stats.get("projectile_radius", DEFAULT_PROJECTILE_RADIUS))
				: effect.scalar1;
			projectile_state.time_remaining = Math::max(0.0, context.distance) / projectile_speed;
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
			double amount = double(source.stats.get("max_hp", 0.0)) * effect.scalar0;
			_add_shield(source, shield_target, amount);
			return;
		}
		case EFFECT_OPCODE_HEAL: {
			UnitState &heal_target = target_ally == nullptr ? source : *target_ally;
			double heal_amount = double(source.stats.get("max_hp", 0.0)) * effect.scalar0 + heal_target.hp * effect.scalar1 + effect.scalar2;
			_heal_unit(source, heal_target, heal_amount);
			return;
		}
		case EFFECT_OPCODE_SELF_DAMAGE: {
			double self_damage = double(source.stats.get("max_hp", 0.0)) * effect.scalar0;
			_apply_damage(source, source, self_damage, effect.damage_type.is_empty() ? StringName("true") : effect.damage_type, context.action_kind, context);
			return;
		}
		case EFFECT_OPCODE_SELF_SHIELD: {
			double self_shield = double(source.stats.get("max_hp", 0.0)) * effect.scalar0;
			_add_shield(source, source, self_shield);
			return;
		}
		case EFFECT_OPCODE_SELF_AOE_TAUNT:
			_apply_aoe_taunt(source, effect.scalar0, effect.scalar1);
			return;
		case EFFECT_OPCODE_SELF_AOE_DAMAGE: {
			double aoe_damage = double(source.stats.get("attack_damage", 0.0)) * effect.scalar1;
			_apply_aoe_damage(source, source, aoe_damage, effect.scalar0, effect.damage_type.is_empty() ? StringName("physical") : effect.damage_type, effect.reason, context.action_kind);
			return;
		}
		case EFFECT_OPCODE_SPLASH_DAMAGE:
			if (target != nullptr) {
				_apply_splash_damage(source, *target, context.damage, effect.scalar0, effect.damage_type.is_empty() ? StringName("physical") : effect.damage_type, context.action_kind, effect.reason, effect.scalar1);
			}
			return;
		case EFFECT_OPCODE_THRESHOLD_SPLASH_DAMAGE:
			if (context.damage > double(source.stats.get("attack_damage", 0.0)) * effect.scalar0 && !effect.children.empty()) {
				_execute_effect(effect.children[0], context);
			}
			return;
		case EFFECT_OPCODE_MANA_REGEN:
			_restore_mana(source, source, effect.scalar0 + double(source.stats.get("max_mana", 0.0)) * effect.scalar1);
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

Array TeamfightSimulationCore::run_matches(const Array &match_inputs) {
	Array summaries;
	summaries.resize(match_inputs.size());
	for (int64_t index = 0; index < match_inputs.size(); ++index) {
		summaries[index] = run_match(match_inputs[index]);
		clear();
	}
	return summaries;
}
