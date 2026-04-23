#include "teamfight_simulation_core.hpp"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

static Dictionary effect_dict(const StringName &kind, const Dictionary &params = Dictionary()) {
	Dictionary effect;
	effect["kind"] = String(kind);
	effect["params"] = params;
	return effect;
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
	return std::generate_canonical<double, 53>(_rng);
}

void TeamfightSimulationCore::_reset_runtime_state() {
	_units.clear();
	_projectiles.clear();
	_time = 0.0;
	_tick_rate = DEFAULT_TICK_RATE;
	_seed = 0;
	_winner_team = StringName("draw");
	_record_events = false;
	_player_comp.clear();
	_enemy_comp.clear();
	_player_kills = 0;
	_enemy_kills = 0;
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
		Dictionary unit = _build_unit_state(spawn_spec, team, next_instance_id);
		if (unit.is_empty()) {
			continue;
		}
		_units.append(unit);
		team_comp.append(unit["archetype_id"]);
		++next_instance_id;
	}
}

Dictionary TeamfightSimulationCore::_build_unit_state(const Dictionary &spawn_spec, const StringName &team, int64_t instance_id) {
	StringName archetype_id = StringName(String(spawn_spec.get("archetype_id", "")));
	Dictionary champion = _champion_for(archetype_id);
	if (champion.is_empty()) {
		UtilityFunctions::push_error(vformat("Unknown champion archetype: %s", String(archetype_id)));
		return Dictionary();
	}

	Dictionary stats = Dictionary(champion["stats"]);
	Dictionary role_config = Dictionary(_role_configs.get(stats.get("role", StringName()), Dictionary()));
	Dictionary stat_mods = Dictionary(role_config.get("stat_mods", Dictionary()));
	Array stat_keys = stat_mods.keys();
	for (int64_t key_index = 0; key_index < stat_keys.size(); ++key_index) {
		Variant key_value = stat_keys[key_index];
		stats[key_value] = stat_mods[key_value];
	}

	Dictionary passive_effects;
	passive_effects[StringName("on_attack")] = Array();
	passive_effects[StringName("on_defense")] = Array();
	passive_effects[StringName("on_tick")] = Array();
	passive_effects[StringName("post_attack")] = Array();
	passive_effects[StringName("post_take_damage")] = Array();

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
			Array bucket = Array(passive_effects.get(kind_value, Array()));
			for (int64_t e = 0; e < effects.size(); ++e) {
				bucket.append(effects[e]);
			}
			passive_effects[kind_value] = bucket;
		}
	}
	Variant role_tick = role_config.get("passive_on_tick", Variant());
	if (role_tick.get_type() != Variant::NIL) {
		Array bucket = Array(passive_effects[StringName("on_tick")]);
		bucket.append(role_tick);
		passive_effects[StringName("on_tick")] = bucket;
	}
	Variant role_take_damage = role_config.get("passive_post_take_damage", Variant());
	if (role_take_damage.get_type() != Variant::NIL) {
		Array bucket = Array(passive_effects[StringName("post_take_damage")]);
		bucket.append(role_take_damage);
		passive_effects[StringName("post_take_damage")] = bucket;
	}

	double max_hp = double(stats.get("max_hp", 0.0));
	double max_mana = double(stats.get("max_mana", 0.0));
	double x = double(spawn_spec.get("x", 0.0));
	double y = double(spawn_spec.get("y", 0.0));

	Dictionary unit;
	unit["instance_id"] = instance_id;
	unit["archetype_id"] = archetype_id;
	unit["team"] = team;
	unit["stats"] = stats;
	unit["passive_effects"] = passive_effects;
	unit["ability_effect"] = champion.get("ability", Variant());
	unit["ultimate_effect"] = champion.get("ultimate", Variant());
	unit["spawn_x"] = x;
	unit["spawn_y"] = y;
	unit["x"] = x;
	unit["y"] = y;
	unit["hp"] = max_hp;
	unit["shield"] = 0.0;
	unit["mana"] = 0.0;
	unit["attack_cooldown"] = 0.0;
	unit["ability_cooldown"] = double(stats.get("ability_cd", 0.0));
	unit["ultimate_cooldown"] = double(stats.get("ultimate_cd", 0.0));
	unit["casting_remaining"] = 0.0;
	unit["casting_kind"] = StringName();
	unit["casting_effect"] = Variant();
	unit["casting_target_id"] = 0;
	unit["casting_ally_target_id"] = 0;
	unit["stun_remaining"] = 0.0;
	unit["respawn_timer"] = 0.0;
	unit["respawned_this_tick"] = false;
	unit["alive"] = true;
	unit["attack_count"] = 0;
	unit["damage_dealt"] = 0.0;
	unit["damage_dealt_auto"] = 0.0;
	unit["damage_dealt_ability"] = 0.0;
	unit["damage_dealt_ultimate"] = 0.0;
	unit["damage_received"] = 0.0;
	unit["damage_mitigated"] = 0.0;
	unit["healing_done"] = 0.0;
	unit["shielding_done"] = 0.0;
	unit["auto_attacks"] = 0;
	unit["abilities"] = 0;
	unit["ultimates"] = 0;
	unit["stuns"] = 0;
	unit["kills"] = 0;
	unit["deaths"] = 0;
	unit["assists"] = 0;
	unit["taunt_target_id"] = 0;
	unit["taunt_remaining"] = 0.0;
	unit["damage_sources"] = Dictionary();
	unit["last_hit_time"] = 0.0;
	unit["regen_accumulator"] = 0.0;
	return unit;
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
	_rng.seed(static_cast<uint64_t>(_seed));

	int64_t next_instance_id = 1;
	_append_team_units(Array(match_input.get("player_units", Array())), StringName("player"), next_instance_id, _player_comp);
	_append_team_units(Array(match_input.get("enemy_units", Array())), StringName("enemy"), next_instance_id, _enemy_comp);
}

Dictionary TeamfightSimulationCore::_build_context(const Dictionary &source, const Variant &target, const Variant &target_ally, double damage, const StringName &action_kind) {
	Dictionary context;
	context["unit"] = source;
	context["target"] = target;
	context["target_ally"] = target_ally;
	context["damage"] = damage;
	context["action_kind"] = action_kind;
	context["distance"] = target.get_type() == Variant::DICTIONARY ? _distance_between(source, Dictionary(target)) : 0.0;
	return context;
}

Array TeamfightSimulationCore::_collect_effects(const Dictionary &unit, const StringName &kind) {
	Array effects;
	Dictionary passive_effects = Dictionary(unit.get("passive_effects", Dictionary()));
	Array bucket = Array(passive_effects.get(kind, Array()));
	for (int64_t index = 0; index < bucket.size(); ++index) {
		effects.append(bucket[index]);
	}
	return effects;
}

double TeamfightSimulationCore::_evaluate_multiplier_effect(const Variant &effect, const Dictionary &context, double current_value) {
	(void)current_value;
	Dictionary effect_dict = _effect_to_dict(effect);
	StringName kind = StringName(String(effect_dict.get("kind", "")));
	Dictionary params = Dictionary(effect_dict.get("params", Dictionary()));
	if (kind == StringName("constant_multiplier")) {
		return double(params.get("multiplier", 1.0));
	}
	if (kind == StringName("target_hp_threshold_multiplier")) {
		Dictionary target = Dictionary(context.get("target", Dictionary()));
		if (target.is_empty()) {
			return 1.0;
		}
		double hp_ratio_threshold = double(params.get("hp_ratio_threshold", 0.0));
		double target_hp = double(target.get("hp", 0.0));
		double target_max_hp = Math::max(0.0001, double(Dictionary(target.get("stats", Dictionary())).get("max_hp", 1.0)));
		if (target_hp / target_max_hp < hp_ratio_threshold) {
			return double(params.get("multiplier", 1.0));
		}
		return 1.0;
	}
	if (kind == StringName("distance_threshold_multiplier")) {
		double min_distance = double(params.get("min_distance", 0.0));
		if (double(context.get("distance", 0.0)) > min_distance) {
			return double(params.get("multiplier", 1.0));
		}
		return 1.0;
	}
	if (kind == StringName("self_hp_threshold_multiplier")) {
		Dictionary source = Dictionary(context.get("unit", Dictionary()));
		double min_hp_ratio = double(params.get("min_hp_ratio", 0.0));
		double hp_ratio = double(source.get("hp", 0.0)) / Math::max(0.0001, double(Dictionary(source.get("stats", Dictionary())).get("max_hp", 1.0)));
		if (hp_ratio > min_hp_ratio) {
			return double(params.get("multiplier", 1.0));
		}
		return 1.0;
	}
	if (kind == StringName("dodge")) {
		double dodge_chance = Math::clamp(double(params.get("dodge_chance", 0.0)), 0.0, 1.0);
		if (_randf() < dodge_chance) {
			return double(params.get("on_dodge_multiplier", 0.0));
		}
		return double(params.get("on_hit_multiplier", 1.0));
	}
	return 1.0;
}

double TeamfightSimulationCore::_defense_multiplier(Dictionary &target, Dictionary &source, double damage, const StringName &action_kind) {
	double multiplier = 1.0;
	Dictionary context = _build_context(source, target, Variant(), damage, action_kind);
	Array effects = _collect_effects(target, StringName("on_defense"));
	for (int64_t index = 0; index < effects.size(); ++index) {
		multiplier *= _evaluate_multiplier_effect(effects[index], context, multiplier);
	}
	return multiplier;
}

double TeamfightSimulationCore::_apply_attack_modifiers(Dictionary &unit, Dictionary &target, double distance, double damage) {
	(void)distance;
	Dictionary context = _build_context(unit, target, Variant(), damage, StringName("auto"));
	Array effects = _collect_effects(unit, StringName("on_attack"));
	double modified_damage = damage;
	for (int64_t index = 0; index < effects.size(); ++index) {
		modified_damage *= _evaluate_multiplier_effect(effects[index], context, modified_damage);
	}
	return modified_damage;
}

double TeamfightSimulationCore::_damage_type_multiplier(const Dictionary &target, const StringName &damage_type) {
	if (damage_type == StringName("physical")) {
		double armor = double(Dictionary(target.get("stats", Dictionary())).get("armor", 0.0));
		return Math::clamp(1.0 - armor, 0.05, 1.0);
	}
	if (damage_type == StringName("magic")) {
		double mr = double(Dictionary(target.get("stats", Dictionary())).get("magic_resist", 0.0));
		return Math::clamp(1.0 - mr, 0.05, 1.0);
	}
	return 1.0;
}

double TeamfightSimulationCore::_apply_damage(Dictionary &source, Dictionary &target, double damage, const StringName &damage_type, const StringName &action_kind, const Dictionary &context) {
	(void)context;
	if (!bool(target.get("alive", false))) {
		return 0.0;
	}
	double incoming = damage;
	if (damage_type != StringName("true")) {
		incoming *= _defense_multiplier(target, source, incoming, action_kind);
		incoming *= _damage_type_multiplier(target, damage_type);
	}
	if (incoming <= 0.0) {
		return 0.0;
	}
	double shield_before = double(target.get("shield", 0.0));
	double absorbed = Math::min(shield_before, incoming);
	target["shield"] = Math::max(0.0, shield_before - absorbed);
	double hp_loss = Math::max(0.0, incoming - absorbed);
	target["hp"] = Math::max(0.0, double(target.get("hp", 0.0)) - hp_loss);
	target["damage_received"] = double(target.get("damage_received", 0.0)) + incoming;
	target["damage_mitigated"] = double(target.get("damage_mitigated", 0.0)) + Math::max(0.0, damage - incoming) + absorbed;
	source["damage_dealt"] = double(source.get("damage_dealt", 0.0)) + incoming;
	if (action_kind == StringName("auto")) {
		source["damage_dealt_auto"] = double(source.get("damage_dealt_auto", 0.0)) + incoming;
	} else if (action_kind == StringName("ability")) {
		source["damage_dealt_ability"] = double(source.get("damage_dealt_ability", 0.0)) + incoming;
	} else if (action_kind == StringName("ultimate")) {
		source["damage_dealt_ultimate"] = double(source.get("damage_dealt_ultimate", 0.0)) + incoming;
	}
	if (hp_loss > 0.0 || absorbed > 0.0) {
		Dictionary damage_sources = Dictionary(target.get("damage_sources", Dictionary()));
		int64_t source_id = int64_t(source.get("instance_id", 0));
		Dictionary entry = Dictionary(damage_sources.get(source_id, Dictionary()));
		entry["damage"] = double(entry.get("damage", 0.0)) + incoming;
		entry["time"] = _time;
		damage_sources[source_id] = entry;
		target["damage_sources"] = damage_sources;
		target["last_hit_time"] = _time;
	}
	if (double(target.get("hp", 0.0)) <= 0.0) {
		_handle_death(source, target);
	}
	return incoming;
}

void TeamfightSimulationCore::_apply_stun(Dictionary &source, Dictionary &target, double duration) {
	if (duration <= 0.0 || target.is_empty()) {
		return;
	}
	target["stun_remaining"] = Math::max(double(target.get("stun_remaining", 0.0)), duration);
	source["stuns"] = int64_t(source.get("stuns", 0)) + 1;
}

void TeamfightSimulationCore::_add_shield(Dictionary &source, Dictionary &target, double amount) {
	if (amount <= 0.0 || target.is_empty()) {
		return;
	}
	target["shield"] = double(target.get("shield", 0.0)) + amount;
	source["shielding_done"] = double(source.get("shielding_done", 0.0)) + amount;
}

void TeamfightSimulationCore::_heal_unit(Dictionary &source, Dictionary &target, double amount) {
	if (amount <= 0.0 || target.is_empty()) {
		return;
	}
	double max_hp = double(Dictionary(target.get("stats", Dictionary())).get("max_hp", 0.0));
	double old_hp = double(target.get("hp", 0.0));
	double new_hp = Math::min(max_hp, old_hp + amount);
	double healed = Math::max(0.0, new_hp - old_hp);
	target["hp"] = new_hp;
	source["healing_done"] = double(source.get("healing_done", 0.0)) + healed;
}

void TeamfightSimulationCore::_restore_mana(Dictionary &source, Dictionary &target, double amount) {
	if (amount <= 0.0 || target.is_empty()) {
		return;
	}
	double max_mana = double(Dictionary(target.get("stats", Dictionary())).get("max_mana", 0.0));
	target["mana"] = Math::min(max_mana, double(target.get("mana", 0.0)) + amount);
	(void)source;
}

void TeamfightSimulationCore::_run_post_attack_effects(Dictionary &source, Dictionary &target, double damage, const Dictionary &context) {
	Array post_attack_effects = _collect_effects(source, StringName("post_attack"));
	if (post_attack_effects.is_empty()) {
		return;
	}
	Dictionary effect_context = context;
	effect_context["damage"] = damage;
	for (int64_t index = 0; index < post_attack_effects.size(); ++index) {
		_execute_effect(post_attack_effects[index], effect_context);
	}
}

void TeamfightSimulationCore::_apply_splash_damage(Dictionary &source, Dictionary &target, double damage, double radius, const StringName &damage_type, const StringName &action_kind, const String &reason, double splash_ratio) {
	(void)reason;
	if (target.is_empty() || radius <= 0.0) {
		return;
	}
	for (int64_t index = 0; index < _units.size(); ++index) {
		Dictionary unit = Dictionary(_units[index]);
		if (!bool(unit.get("alive", false))) {
			continue;
		}
		if (StringName(String(unit.get("team", ""))) == StringName(String(source.get("team", "")))) {
			continue;
		}
		if (int64_t(unit.get("instance_id", 0)) == int64_t(target.get("instance_id", 0))) {
			continue;
		}
		if (_distance_between_dicts(target, unit) <= radius) {
			double splash_damage = damage * splash_ratio;
			Dictionary context = _build_context(source, unit, Variant(), splash_damage, action_kind);
			_apply_damage(source, unit, splash_damage, damage_type, action_kind, context);
			_units[index] = unit;
		}
	}
}

void TeamfightSimulationCore::_apply_aoe_taunt(Dictionary &source, double radius, double duration) {
	for (int64_t index = 0; index < _units.size(); ++index) {
		Dictionary unit = Dictionary(_units[index]);
		if (!bool(unit.get("alive", false))) {
			continue;
		}
		if (StringName(String(unit.get("team", ""))) == StringName(String(source.get("team", "")))) {
			continue;
		}
		if (_distance_between_dicts(source, unit) <= radius) {
			unit["taunt_target_id"] = int64_t(source.get("instance_id", 0));
			unit["taunt_remaining"] = Math::max(double(unit.get("taunt_remaining", 0.0)), duration);
			_units[index] = unit;
		}
	}
}

void TeamfightSimulationCore::_apply_aoe_damage(Dictionary &source, Dictionary &center_source, double damage, double radius, const StringName &damage_type, const String &reason, const StringName &action_kind) {
	(void)reason;
	for (int64_t index = 0; index < _units.size(); ++index) {
		Dictionary unit = Dictionary(_units[index]);
		if (!bool(unit.get("alive", false))) {
			continue;
		}
		if (StringName(String(unit.get("team", ""))) == StringName(String(source.get("team", "")))) {
			continue;
		}
		if (_distance_between_dicts(center_source, unit) <= radius) {
			Dictionary context = _build_context(source, unit, Variant(), damage, action_kind);
			_apply_damage(source, unit, damage, damage_type, action_kind, context);
			_units[index] = unit;
		}
	}
}

Dictionary TeamfightSimulationCore::_select_enemy_target(const Dictionary &unit) {
	int64_t taunt_id = int64_t(unit.get("taunt_target_id", 0));
	if (taunt_id != 0 && double(unit.get("taunt_remaining", 0.0)) > 0.0) {
		Dictionary taunt_target = _unit_by_id(taunt_id);
		if (!taunt_target.is_empty() && bool(taunt_target.get("alive", false))) {
			return taunt_target;
		}
	}
	Dictionary best;
	for (int64_t index = 0; index < _units.size(); ++index) {
		Dictionary candidate = Dictionary(_units[index]);
		if (!bool(candidate.get("alive", false))) {
			continue;
		}
		if (StringName(String(candidate.get("team", ""))) == StringName(String(unit.get("team", "")))) {
			continue;
		}
		if (best.is_empty() || _is_better_target(unit, candidate, best)) {
			best = candidate;
		}
	}
	return best;
}

Dictionary TeamfightSimulationCore::_select_ally_target(const Dictionary &unit) {
	Dictionary best = unit;
	for (int64_t index = 0; index < _units.size(); ++index) {
		Dictionary candidate = Dictionary(_units[index]);
		if (!bool(candidate.get("alive", false))) {
			continue;
		}
		if (StringName(String(candidate.get("team", ""))) != StringName(String(unit.get("team", "")))) {
			continue;
		}
		if (_is_better_ally(unit, candidate, best)) {
			best = candidate;
		}
	}
	return best;
}

bool TeamfightSimulationCore::_is_better_target(const Dictionary &attacker, const Dictionary &candidate, const Dictionary &current_best) {
	double candidate_distance = _distance_between(attacker, candidate);
	double best_distance = _distance_between(attacker, current_best);
	if (candidate_distance < best_distance - EPSILON) {
		return true;
	}
	if (candidate_distance > best_distance + EPSILON) {
		return false;
	}
	double candidate_ratio = double(candidate.get("hp", 0.0)) / Math::max(0.0001, double(Dictionary(candidate.get("stats", Dictionary())).get("max_hp", 1.0)));
	double best_ratio = double(current_best.get("hp", 0.0)) / Math::max(0.0001, double(Dictionary(current_best.get("stats", Dictionary())).get("max_hp", 1.0)));
	if (candidate_ratio < best_ratio - EPSILON) {
		return true;
	}
	if (candidate_ratio > best_ratio + EPSILON) {
		return false;
	}
	return int64_t(candidate.get("instance_id", 0)) < int64_t(current_best.get("instance_id", 0));
}

bool TeamfightSimulationCore::_is_better_ally(const Dictionary &attacker, const Dictionary &candidate, const Dictionary &current_best) {
	(void)attacker;
	double candidate_ratio = double(candidate.get("hp", 0.0)) / Math::max(0.0001, double(Dictionary(candidate.get("stats", Dictionary())).get("max_hp", 1.0)));
	double best_ratio = double(current_best.get("hp", 0.0)) / Math::max(0.0001, double(Dictionary(current_best.get("stats", Dictionary())).get("max_hp", 1.0)));
	if (candidate_ratio < best_ratio - EPSILON) {
		return true;
	}
	if (candidate_ratio > best_ratio + EPSILON) {
		return false;
	}
	return int64_t(candidate.get("instance_id", 0)) < int64_t(current_best.get("instance_id", 0));
}

double TeamfightSimulationCore::_distance_between(const Dictionary &left, const Dictionary &right) const {
	return _distance_between_dicts(left, right);
}

double TeamfightSimulationCore::_distance_between_dicts(const Dictionary &left, const Dictionary &right) const {
	Vector2 left_position(double(left.get("x", 0.0)), double(left.get("y", 0.0)));
	Vector2 right_position(double(right.get("x", 0.0)), double(right.get("y", 0.0)));
	return left_position.distance_to(right_position);
}

double TeamfightSimulationCore::_attack_range(const Dictionary &unit) const {
	return double(Dictionary(unit.get("stats", Dictionary())).get("attack_range", 0.0));
}

double TeamfightSimulationCore::_effective_attack_range(const Dictionary &unit) const {
	double attack_range = _attack_range(unit);
	if (attack_range <= RANGED_THRESHOLD) {
		return attack_range + MELEE_CONTACT_BUFFER;
	}
	return attack_range;
}

Dictionary TeamfightSimulationCore::_unit_by_id(int64_t instance_id) const {
	for (int64_t index = 0; index < _units.size(); ++index) {
		Dictionary unit = Dictionary(_units[index]);
		if (int64_t(unit.get("instance_id", 0)) == instance_id) {
			return unit;
		}
	}
	return Dictionary();
}

void TeamfightSimulationCore::_handle_death(Dictionary &killer, Dictionary &target) {
	if (!bool(target.get("alive", false))) {
		return;
	}
	target["alive"] = false;
	target["respawn_timer"] = double(Dictionary(target.get("stats", Dictionary())).get("respawn_time", RESPAWN_TIME));
	target["deaths"] = int64_t(target.get("deaths", 0)) + 1;

	Dictionary damage_sources = Dictionary(target.get("damage_sources", Dictionary()));
	int64_t killer_id = 0;
	double killer_damage = -1.0;
	Array keys = damage_sources.keys();
	for (int64_t index = 0; index < keys.size(); ++index) {
		int64_t source_id = int64_t(keys[index]);
		Dictionary entry = Dictionary(damage_sources[keys[index]]);
		double dealt = double(entry.get("damage", 0.0));
		double hit_time = double(entry.get("time", 0.0));
		if (_time - hit_time > ASSIST_WINDOW) {
			continue;
		}
		if (dealt > killer_damage || (Math::is_equal_approx(dealt, killer_damage) && source_id < killer_id)) {
			killer_damage = dealt;
			killer_id = source_id;
		}
	}

	Dictionary killer_unit = _unit_by_id(killer_id);
	if (!killer_unit.is_empty()) {
		killer_unit["kills"] = int64_t(killer_unit.get("kills", 0)) + 1;
		if (StringName(String(killer_unit.get("team", ""))) == StringName("player")) {
			_player_kills += 1;
		} else if (StringName(String(killer_unit.get("team", ""))) == StringName("enemy")) {
			_enemy_kills += 1;
		}
	}

	for (int64_t index = 0; index < keys.size(); ++index) {
		int64_t source_id = int64_t(keys[index]);
		if (source_id == killer_id) {
			continue;
		}
		Dictionary entry = Dictionary(damage_sources[keys[index]]);
		if (_time - double(entry.get("time", 0.0)) <= ASSIST_WINDOW) {
			Dictionary assist_unit = _unit_by_id(source_id);
			if (!assist_unit.is_empty()) {
				assist_unit["assists"] = int64_t(assist_unit.get("assists", 0)) + 1;
			}
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

void TeamfightSimulationCore::_respawn_unit(Dictionary &unit) {
	if (bool(unit.get("alive", true))) {
		return;
	}
	unit["alive"] = true;
	unit["hp"] = double(Dictionary(unit.get("stats", Dictionary())).get("max_hp", 0.0));
	unit["mana"] = 0.0;
	unit["shield"] = 0.0;
	unit["attack_cooldown"] = 0.0;
	unit["ability_cooldown"] = double(Dictionary(unit.get("stats", Dictionary())).get("ability_cd", 0.0));
	unit["ultimate_cooldown"] = double(Dictionary(unit.get("stats", Dictionary())).get("ultimate_cd", 0.0));
	unit["stun_remaining"] = 0.0;
	unit["taunt_remaining"] = 0.0;
	unit["respawn_timer"] = 0.0;
	unit["damage_sources"] = Dictionary();
	unit["last_hit_time"] = 0.0;
	unit["respawned_this_tick"] = true;
	unit["x"] = double(unit.get("spawn_x", unit.get("x", 0.0)));
	unit["y"] = double(unit.get("spawn_y", unit.get("y", 0.0)));
}

void TeamfightSimulationCore::_step_tick() {
	_time += _tick_rate;
	for (int64_t index = 0; index < _units.size(); ++index) {
		Dictionary unit = Dictionary(_units[index]);
		unit["respawned_this_tick"] = false;
		_units[index] = unit;
	}
	_update_cooldowns_and_status();
	_update_projectiles();
	_process_actions();
}

void TeamfightSimulationCore::_simulate() {
	double effective_tick_rate = Math::max(_tick_rate, EPSILON);
	int64_t max_ticks = int64_t(Math::ceil(MATCH_DURATION / effective_tick_rate));
	for (int64_t tick_index = 0; tick_index < max_ticks; ++tick_index) {
		_step_tick();
	}
	_winner_team = _determine_winner();
}

void TeamfightSimulationCore::_update_cooldowns_and_status() {
	for (int64_t index = 0; index < _units.size(); ++index) {
		Dictionary unit = Dictionary(_units[index]);
		if (!bool(unit.get("alive", false))) {
			unit["respawn_timer"] = Math::max(0.0, double(unit.get("respawn_timer", 0.0)) - _tick_rate);
			if (double(unit.get("respawn_timer", 0.0)) <= 0.0) {
				_respawn_unit(unit);
			}
			_units[index] = unit;
			continue;
		}
		unit["attack_cooldown"] = Math::max(0.0, double(unit.get("attack_cooldown", 0.0)) - _tick_rate);
		unit["ability_cooldown"] = Math::max(0.0, double(unit.get("ability_cooldown", 0.0)) - _tick_rate);
		unit["ultimate_cooldown"] = Math::max(0.0, double(unit.get("ultimate_cooldown", 0.0)) - _tick_rate);
		unit["stun_remaining"] = Math::max(0.0, double(unit.get("stun_remaining", 0.0)) - _tick_rate);
		unit["taunt_remaining"] = Math::max(0.0, double(unit.get("taunt_remaining", 0.0)) - _tick_rate);
		if (double(unit.get("stun_remaining", 0.0)) <= 0.0) {
			unit["stun_remaining"] = 0.0;
		}
		if (double(unit.get("taunt_remaining", 0.0)) <= 0.0) {
			unit["taunt_remaining"] = 0.0;
		}
		if (double(unit.get("casting_remaining", 0.0)) > 0.0) {
			unit["casting_remaining"] = Math::max(0.0, double(unit.get("casting_remaining", 0.0)) - _tick_rate);
			if (double(unit.get("casting_remaining", 0.0)) <= 0.0) {
				_resolve_cast(unit);
			}
		}
		double regen_accumulator = double(unit.get("regen_accumulator", 0.0)) + _tick_rate;
		while (regen_accumulator >= REGEN_TICK_INTERVAL) {
			regen_accumulator -= REGEN_TICK_INTERVAL;
			Array effects = _collect_effects(unit, StringName("on_tick"));
			for (int64_t e = 0; e < effects.size(); ++e) {
				Dictionary context = _build_context(unit, Dictionary(), Dictionary(), 0.0, StringName("auto"));
				_execute_effect(effects[e], context);
			}
		}
		unit["regen_accumulator"] = regen_accumulator;
		_units[index] = unit;
	}
}

void TeamfightSimulationCore::_process_actions() {
	for (int64_t index = 0; index < _units.size(); ++index) {
		Dictionary unit = Dictionary(_units[index]);
		if (!bool(unit.get("alive", false)) || bool(unit.get("respawned_this_tick", false)) || double(unit.get("casting_remaining", 0.0)) > 0.0 || double(unit.get("stun_remaining", 0.0)) > 0.0) {
			continue;
		}
		Dictionary target = _select_enemy_target(unit);
		if (target.is_empty()) {
			continue;
		}
		double distance = _distance_between(unit, target);
		if (_try_cast_ultimate(unit, target, distance)) {
			_units[index] = unit;
			continue;
		}
		if (_try_cast_ability(unit, target, distance)) {
			_units[index] = unit;
			continue;
		}
		if (distance <= _effective_attack_range(unit)) {
			_perform_auto_attack(unit, target, distance);
		} else {
			_move_toward_target(unit, target);
		}
		_units[index] = unit;
	}
}

bool TeamfightSimulationCore::_try_cast_ability(Dictionary &unit, Dictionary &target, double distance) {
	(void)distance;
	if (double(unit.get("ability_cooldown", 0.0)) > 0.0) {
		return false;
	}
	return _start_cast(unit, target, distance, StringName("ability"));
}

bool TeamfightSimulationCore::_try_cast_ultimate(Dictionary &unit, Dictionary &target, double distance) {
	Dictionary stats = Dictionary(unit.get("stats", Dictionary()));
	double max_mana = double(stats.get("max_mana", 0.0));
	if (double(unit.get("ultimate_cooldown", 0.0)) > 0.0 || max_mana <= 0.0 || double(unit.get("mana", 0.0)) < max_mana) {
		return false;
	}
	return _start_cast(unit, target, distance, StringName("ultimate"));
}

bool TeamfightSimulationCore::_start_cast(Dictionary &unit, Dictionary &target, double distance, const StringName &action_kind) {
	(void)distance;
	Variant effect = action_kind == StringName("ability") ? unit.get("ability_effect", Variant()) : unit.get("ultimate_effect", Variant());
	if (effect.get_type() == Variant::NIL) {
		return false;
	}
	Dictionary target_ally = _select_ally_target(unit);
	Dictionary stats = Dictionary(unit.get("stats", Dictionary()));
	if (action_kind == StringName("ability")) {
		unit["ability_cooldown"] = double(stats.get("ability_cd", 0.0));
		unit["abilities"] = int64_t(unit.get("abilities", 0)) + 1;
	} else {
		unit["ultimate_cooldown"] = double(stats.get("ultimate_cd", 0.0));
		unit["ultimates"] = int64_t(unit.get("ultimates", 0)) + 1;
		unit["mana"] = Math::max(0.0, double(unit.get("mana", 0.0)) - double(stats.get("max_mana", 0.0)));
	}
	unit["casting_remaining"] = CASTING_WINDUP;
	unit["casting_kind"] = action_kind;
	unit["casting_effect"] = effect;
	unit["casting_target_id"] = int64_t(target.get("instance_id", 0));
	unit["casting_ally_target_id"] = target_ally.is_empty() ? 0 : int64_t(target_ally.get("instance_id", 0));
	return true;
}

void TeamfightSimulationCore::_resolve_cast(Dictionary &unit) {
	Variant effect = unit.get("casting_effect", Variant());
	StringName action_kind = StringName(String(unit.get("casting_kind", "")));
	Dictionary target = _unit_by_id(int64_t(unit.get("casting_target_id", 0)));
	Dictionary target_ally = _unit_by_id(int64_t(unit.get("casting_ally_target_id", 0)));
	unit["casting_remaining"] = 0.0;
	unit["casting_kind"] = StringName();
	unit["casting_effect"] = Variant();
	unit["casting_target_id"] = 0;
	unit["casting_ally_target_id"] = 0;
	if (effect.get_type() == Variant::NIL) {
		return;
	}
	if ((action_kind == StringName("ability") || action_kind == StringName("ultimate")) && target.is_empty() && target_ally.is_empty()) {
		return;
	}
	Dictionary context = _build_context(unit, target, target_ally, 0.0, action_kind);
	Dictionary result = _execute_effect(effect, context);
	if (action_kind == StringName("ability")) {
		unit["damage_dealt_ability"] = double(unit.get("damage_dealt_ability", 0.0)) + double(result.get("damage", 0.0));
	} else if (action_kind == StringName("ultimate")) {
		unit["damage_dealt_ultimate"] = double(unit.get("damage_dealt_ultimate", 0.0)) + double(result.get("damage", 0.0));
	}
}

void TeamfightSimulationCore::_perform_auto_attack(Dictionary &unit, Dictionary &target, double distance) {
	Dictionary stats = Dictionary(unit.get("stats", Dictionary()));
	double attack_speed = Math::max(0.0001, double(stats.get("attack_speed", 1.0)));
	unit["attack_cooldown"] = 1.0 / attack_speed;
	unit["auto_attacks"] = int64_t(unit.get("auto_attacks", 0)) + 1;
	unit["attack_count"] = int64_t(unit.get("attack_count", 0)) + 1;
	double mana_gain = double(stats.get("mana_per_attack", 0.0));
	if (mana_gain > 0.0) {
		double max_mana = double(stats.get("max_mana", 0.0));
		unit["mana"] = Math::min(max_mana, double(unit.get("mana", 0.0)) + mana_gain);
	}
	double damage = double(stats.get("attack_damage", 0.0));
	damage = _apply_attack_modifiers(unit, target, distance, damage);
	if (double(stats.get("attack_range", 0.0)) > RANGED_THRESHOLD) {
		Dictionary projectile;
		projectile["source_id"] = int64_t(unit.get("instance_id", 0));
		projectile["target_id"] = int64_t(target.get("instance_id", 0));
		projectile["damage"] = damage;
		projectile["damage_type"] = StringName("physical");
		projectile["stun_duration"] = 0.0;
		projectile["radius"] = double(stats.get("projectile_radius", DEFAULT_PROJECTILE_RADIUS));
		projectile["time_remaining"] = distance / Math::max(0.0001, double(stats.get("projectile_speed", DEFAULT_PROJECTILE_SPEED)));
		projectile["action_kind"] = StringName("auto");
		projectile["reason"] = String("Auto Attack");
		_projectiles.append(projectile);
		return;
	}
	Dictionary context = _build_context(unit, target, Dictionary(), damage, StringName("auto"));
	double dealt = _apply_damage(unit, target, damage, StringName("physical"), StringName("auto"), context);
	_run_post_attack_effects(unit, target, dealt, context);
}

void TeamfightSimulationCore::_move_toward_target(Dictionary &unit, Dictionary &target) {
	Dictionary stats = Dictionary(unit.get("stats", Dictionary()));
	double speed = double(stats.get("move_speed", 0.0)) * _tick_rate;
	if (speed <= 0.0) {
		return;
	}
	Vector2 origin(double(unit.get("x", 0.0)), double(unit.get("y", 0.0)));
	Vector2 destination(double(target.get("x", 0.0)), double(target.get("y", 0.0)));
	Vector2 direction = destination - origin;
	double distance = direction.length();
	if (distance <= EPSILON) {
		return;
	}
	double effective_range = _effective_attack_range(unit);
	double max_step = Math::min(speed, Math::max(0.0, distance - effective_range));
	if (max_step <= 0.0) {
		return;
	}
	Vector2 normalized = direction.normalized();
	unit["x"] = origin.x + normalized.x * max_step;
	unit["y"] = origin.y + normalized.y * max_step;
}

void TeamfightSimulationCore::_resolve_projectile(const Dictionary &projectile) {
	Dictionary source = _unit_by_id(int64_t(projectile.get("source_id", 0)));
	Dictionary target = _unit_by_id(int64_t(projectile.get("target_id", 0)));
	if (source.is_empty() || target.is_empty() || !bool(source.get("alive", false)) || !bool(target.get("alive", false))) {
		return;
	}
	double damage = double(projectile.get("damage", 0.0));
	StringName damage_type = StringName(String(projectile.get("damage_type", "physical")));
	StringName action_kind = StringName(String(projectile.get("action_kind", "auto")));
	Dictionary context = _build_context(source, target, Dictionary(), damage, action_kind);
	double dealt = _apply_damage(source, target, damage, damage_type, action_kind, context);
	if (double(projectile.get("stun_duration", 0.0)) > 0.0 && bool(target.get("alive", false))) {
		_apply_stun(source, target, double(projectile.get("stun_duration", 0.0)));
	}
	if (double(projectile.get("radius", 0.0)) > 0.0) {
		_apply_splash_damage(source, target, dealt, double(projectile.get("radius", 0.0)), damage_type, action_kind, String(projectile.get("reason", "Projectile Splash")));
	}
	_run_post_attack_effects(source, target, dealt, context);
}

void TeamfightSimulationCore::_update_projectiles() {
	Array next_projectiles;
	for (int64_t index = 0; index < _projectiles.size(); ++index) {
		Dictionary data = Dictionary(_projectiles[index]);
		data["time_remaining"] = double(data.get("time_remaining", 0.0)) - _tick_rate;
		if (double(data.get("time_remaining", 0.0)) > 0.0) {
			next_projectiles.append(data);
			continue;
		}
		_resolve_projectile(data);
	}
	_projectiles = next_projectiles;
}

Dictionary TeamfightSimulationCore::_execute_effect(const Variant &effect, const Dictionary &context) {
	Dictionary effect_dict = _effect_to_dict(effect);
	StringName kind = StringName(String(effect_dict.get("kind", "")));
	Dictionary params = Dictionary(effect_dict.get("params", Dictionary()));
	Dictionary source = Dictionary(context.get("unit", Dictionary()));
	Dictionary target = Dictionary(context.get("target", Dictionary()));
	Dictionary target_ally = Dictionary(context.get("target_ally", Dictionary()));
	Dictionary combined;

	if (kind == StringName("multi")) {
		Array child_effects = Array(params.get("effects", Array()));
		for (int64_t index = 0; index < child_effects.size(); ++index) {
			Dictionary child_result = _execute_effect(child_effects[index], context);
			_merge_result(combined, child_result);
		}
		return combined;
	}
	if (kind == StringName("damage")) {
		if (target.is_empty()) {
			return Dictionary();
		}
		double damage_multiplier = double(params.get("damage_multiplier", 1.0));
		StringName damage_type = StringName(String(params.get("damage_type", "physical")));
		double damage = double(Dictionary(source.get("stats", Dictionary())).get("attack_damage", 0.0)) * damage_multiplier;
		double dealt = _apply_damage(source, target, damage, damage_type, StringName(String(context.get("action_kind", StringName("auto")))), context);
		if (bool(params.get("trigger_on_hit", true))) {
			_run_post_attack_effects(source, target, dealt, context);
		}
		Dictionary result;
		result["damage"] = dealt;
		result["reason"] = String(params.get("reason", ""));
		return result;
	}
	if (kind == StringName("projectile")) {
		if (target.is_empty()) {
			return Dictionary();
		}
		double projectile_speed = double(params.get("speed_override", double(Dictionary(source.get("stats", Dictionary())).get("projectile_speed", DEFAULT_PROJECTILE_SPEED))));
		double radius = double(params.get("radius_override", 0.0));
		double damage_multiplier = double(params.get("damage_multiplier", 1.0));
		StringName damage_type = StringName(String(params.get("damage_type", "physical")));
		double stun_duration = double(params.get("stun_duration", 0.0));
		double damage = double(Dictionary(source.get("stats", Dictionary())).get("attack_damage", 0.0)) * damage_multiplier;
		double distance = _distance_between(source, target);
		double time_remaining = distance / Math::max(0.0001, projectile_speed);
		Dictionary projectile_state;
		projectile_state["source_id"] = int64_t(source.get("instance_id", 0));
		projectile_state["target_id"] = int64_t(target.get("instance_id", 0));
		projectile_state["damage"] = damage;
		projectile_state["damage_type"] = damage_type;
		projectile_state["stun_duration"] = stun_duration;
		projectile_state["radius"] = radius;
		projectile_state["time_remaining"] = time_remaining;
		projectile_state["action_kind"] = context.get("action_kind", StringName("auto"));
		projectile_state["reason"] = String(params.get("reason", ""));
		_projectiles.append(projectile_state);
		Dictionary result;
		result["damage"] = damage;
		result["reason"] = String(params.get("reason", ""));
		result["use_projectile"] = true;
		return result;
	}
	if (kind == StringName("stun")) {
		if (!target.is_empty()) {
			_apply_stun(source, target, double(params.get("duration", 0.0)));
		}
		Dictionary result;
		result["stun"] = double(params.get("duration", 0.0));
		result["reason"] = String(params.get("reason", ""));
		return result;
	}
	if (kind == StringName("shield")) {
		Dictionary shield_target = target_ally.is_empty() ? source : target_ally;
		double amount = double(Dictionary(source.get("stats", Dictionary())).get("max_hp", 0.0)) * double(params.get("max_hp_ratio", 0.0));
		_add_shield(source, shield_target, amount);
		Dictionary result;
		result["shield_added"] = amount;
		result["reason"] = String(params.get("reason", ""));
		return result;
	}
	if (kind == StringName("heal")) {
		Dictionary heal_target = target_ally.is_empty() ? source : target_ally;
		double heal_amount = double(Dictionary(source.get("stats", Dictionary())).get("max_hp", 0.0)) * double(params.get("max_hp_ratio", 0.0)) + double(heal_target.get("hp", 0.0)) * double(params.get("current_hp_ratio", 0.0)) + double(params.get("flat_amount", 0.0));
		_heal_unit(source, heal_target, heal_amount);
		Dictionary result;
		result["healing"] = heal_amount;
		result["reason"] = String(params.get("reason", ""));
		return result;
	}
	if (kind == StringName("self_damage")) {
		double self_damage = double(Dictionary(source.get("stats", Dictionary())).get("max_hp", 0.0)) * double(params.get("damage_ratio", 0.0));
		_apply_damage(source, source, self_damage, StringName(String(params.get("damage_type", "true"))), StringName(String(context.get("action_kind", StringName("auto")))), context);
		Dictionary result;
		result["damage"] = self_damage;
		result["reason"] = String(params.get("reason", ""));
		return result;
	}
	if (kind == StringName("self_shield")) {
		double self_shield = double(Dictionary(source.get("stats", Dictionary())).get("max_hp", 0.0)) * double(params.get("shield_ratio", 0.0));
		_add_shield(source, source, self_shield);
		Dictionary result;
		result["shield_added"] = self_shield;
		result["reason"] = String(params.get("reason", ""));
		return result;
	}
	if (kind == StringName("self_aoe_taunt")) {
		_apply_aoe_taunt(source, double(params.get("radius", 0.0)), double(params.get("duration", 0.0)));
		Dictionary result;
		result["taunt"] = double(params.get("duration", 0.0));
		result["reason"] = String(params.get("reason", ""));
		return result;
	}
	if (kind == StringName("self_aoe_damage")) {
		double radius_damage = double(params.get("radius", 0.0));
		double aoe_multiplier = double(params.get("damage_multiplier", 1.0));
		StringName aoe_type = StringName(String(params.get("damage_type", "physical")));
		double aoe_damage = double(Dictionary(source.get("stats", Dictionary())).get("attack_damage", 0.0)) * aoe_multiplier;
		_apply_aoe_damage(source, source, aoe_damage, radius_damage, aoe_type, String(params.get("reason", "")), StringName(String(context.get("action_kind", StringName("auto")))));
		Dictionary result;
		result["damage"] = aoe_damage;
		result["reason"] = String(params.get("reason", ""));
		return result;
	}
	if (kind == StringName("splash_damage")) {
		double splash_radius = double(params.get("radius", 0.0));
		double splash_ratio = double(params.get("ratio", 0.0));
		StringName splash_damage_type = StringName(String(params.get("damage_type", "physical")));
		String splash_reason = String(params.get("reason", "Splash"));
		_apply_splash_damage(source, target, double(context.get("damage", 0.0)), splash_radius, splash_damage_type, StringName(String(context.get("action_kind", StringName("auto")))), splash_reason, splash_ratio);
		Dictionary result;
		result["damage"] = double(context.get("damage", 0.0));
		result["reason"] = splash_reason;
		return result;
	}
	if (kind == StringName("threshold_splash_damage")) {
		double threshold_multiplier = double(params.get("threshold_multiplier", 1.0));
		if (double(context.get("damage", 0.0)) > double(Dictionary(source.get("stats", Dictionary())).get("attack_damage", 0.0)) * threshold_multiplier) {
			Variant nested = params.get("splash", Variant());
			if (nested.get_type() != Variant::NIL) {
				return _execute_effect(nested, context);
			}
		}
		return Dictionary();
	}
	if (kind == StringName("mana_regen")) {
		double mana_amount = double(params.get("flat_amount", 0.0)) + (double(Dictionary(source.get("stats", Dictionary())).get("max_mana", 0.0)) * double(params.get("max_mana_ratio", 0.0)));
		_restore_mana(source, source, mana_amount);
		Dictionary result;
		result["mana_restored"] = mana_amount;
		return result;
	}
	if (kind == StringName("post_damage_mana_gain")) {
		double gain = double(context.get("damage", 0.0)) * double(params.get("damage_ratio", 0.0));
		_restore_mana(source, source, gain);
		Dictionary result;
		result["mana_restored"] = gain;
		return result;
	}
	if (kind == StringName("damage_based_heal")) {
		double heal_ratio = double(params.get("heal_ratio", 0.0));
		double heal_amount = double(context.get("damage", 0.0)) * heal_ratio;
		_heal_unit(source, source, heal_amount);
		Dictionary result;
		result["healing"] = heal_amount;
		return result;
	}
	if (kind == StringName("mana_restore_on_hit")) {
		double restore_amount = double(params.get("flat_amount", 0.0));
		_restore_mana(source, source, restore_amount);
		Dictionary result;
		result["mana_restored"] = restore_amount;
		return result;
	}
	if (kind == StringName("drain_target_mana_on_hit")) {
		if (target.is_empty()) {
			return Dictionary();
		}
		double drain_amount = double(params.get("flat_amount", 0.0));
		target["mana"] = Math::max(0.0, double(target.get("mana", 0.0)) - drain_amount);
		Dictionary result;
		result["mana_drained"] = drain_amount;
		return result;
	}
	if (kind == StringName("every_n_attacks_stun")) {
		int64_t every_n = int64_t(params.get("every_n", 0));
		if (every_n > 0) {
			int64_t attack_count = int64_t(source.get("attack_count", 0));
			if (attack_count % every_n == 0 && !target.is_empty()) {
				_apply_stun(source, target, double(params.get("stun_duration", 0.0)));
			}
		}
		Dictionary result;
		result["reason"] = String(params.get("reason", ""));
		return result;
	}
	if (kind == StringName("dodge") || kind == StringName("constant_multiplier") || kind == StringName("target_hp_threshold_multiplier") || kind == StringName("distance_threshold_multiplier") || kind == StringName("self_hp_threshold_multiplier")) {
		return Dictionary();
	}
	return Dictionary();
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
	Dictionary summary;
	summary["seed"] = _seed;
	summary["winner_team"] = String(_winner_team);
	summary["duration"] = _time;
	summary["player_comp"] = _player_comp;
	summary["enemy_comp"] = _enemy_comp;
	Array unit_stats;
	for (int64_t index = 0; index < _units.size(); ++index) {
		Dictionary unit = Dictionary(_units[index]);
		Dictionary unit_summary;
		unit_summary["instance_id"] = int64_t(unit.get("instance_id", 0));
		unit_summary["archetype"] = String(unit.get("archetype_id", ""));
		unit_summary["team"] = String(unit.get("team", ""));
		unit_summary["won"] = _winner_team != StringName() && StringName(String(unit.get("team", ""))) == _winner_team;
		unit_summary["damage_dealt"] = double(unit.get("damage_dealt", 0.0));
		unit_summary["damage_dealt_auto"] = double(unit.get("damage_dealt_auto", 0.0));
		unit_summary["damage_dealt_ability"] = double(unit.get("damage_dealt_ability", 0.0));
		unit_summary["damage_dealt_ultimate"] = double(unit.get("damage_dealt_ultimate", 0.0));
		unit_summary["damage_received"] = double(unit.get("damage_received", 0.0));
		unit_summary["damage_mitigated"] = double(unit.get("damage_mitigated", 0.0));
		unit_summary["healing_done"] = double(unit.get("healing_done", 0.0));
		unit_summary["shielding_done"] = double(unit.get("shielding_done", 0.0));
		unit_summary["auto_attacks"] = int64_t(unit.get("auto_attacks", 0));
		unit_summary["abilities"] = int64_t(unit.get("abilities", 0));
		unit_summary["ultimates"] = int64_t(unit.get("ultimates", 0));
		unit_summary["stuns"] = int64_t(unit.get("stuns", 0));
		unit_summary["kills"] = int64_t(unit.get("kills", 0));
		unit_summary["deaths"] = int64_t(unit.get("deaths", 0));
		unit_summary["assists"] = int64_t(unit.get("assists", 0));
		unit_stats.append(unit_summary);
	}
	summary["unit_stats"] = unit_stats;
	return summary;
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
