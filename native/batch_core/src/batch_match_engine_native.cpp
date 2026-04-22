#include "batch_match_engine_native.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

void BatchMatchEngineNative::_bind_methods() {
	ADD_SIGNAL(MethodInfo("combat_event_recorded", PropertyInfo(Variant::DICTIONARY, "event")));

	ClassDB::bind_method(D_METHOD("set_seed", "seed"), &BatchMatchEngineNative::set_seed);
	ClassDB::bind_method(D_METHOD("set_units", "units"), &BatchMatchEngineNative::set_units);
	ClassDB::bind_method(D_METHOD("step", "dt"), &BatchMatchEngineNative::step);
	ClassDB::bind_method(D_METHOD("get_match_result"), &BatchMatchEngineNative::get_match_result);
	ClassDB::bind_method(D_METHOD("snapshot_units"), &BatchMatchEngineNative::snapshot_units);
	ClassDB::bind_method(D_METHOD("clear"), &BatchMatchEngineNative::clear);
	ClassDB::bind_method(D_METHOD("build_units", "player_roster", "enemy_roster"), &BatchMatchEngineNative::build_units);
	ClassDB::bind_method(
			D_METHOD("run_match", "match_seed", "tick_rate", "max_ticks", "player_roster", "enemy_roster", "record_events"),
			&BatchMatchEngineNative::run_match
		);
	ClassDB::bind_method(D_METHOD("get_combat_registry"), &BatchMatchEngineNative::get_combat_registry);
}

void BatchMatchEngineNative::set_seed(int32_t p_seed) {
	core_.set_seed(p_seed);
}

void BatchMatchEngineNative::set_units(const Array &p_units) {
	std::vector<teamfight::UnitState> units;
	units.reserve(static_cast<size_t>(p_units.size()));
	for (int64_t i = 0; i < p_units.size(); ++i) {
		units.push_back(_unit_from_variant(p_units[i]));
	}
	core_.set_units(units);
}

void BatchMatchEngineNative::step(double p_dt) {
	core_.step(static_cast<float>(p_dt));
}

Dictionary BatchMatchEngineNative::get_match_result() const {
	const teamfight::MatchResult result = core_.get_match_result();
	if (result.termination.empty()) {
		return Dictionary();
	}
	return _match_result_to_dictionary(result);
}

Array BatchMatchEngineNative::snapshot_units() const {
	Array snapshots;
	const std::vector<teamfight::UnitState> units = core_.snapshot_units();
	snapshots.resize(static_cast<int>(units.size()));
	for (int64_t i = 0; i < static_cast<int64_t>(units.size()); ++i) {
		snapshots[i] = _unit_to_dictionary(units[static_cast<size_t>(i)]);
	}
	return snapshots;
}

void BatchMatchEngineNative::clear() {
	core_.clear();
}

Array BatchMatchEngineNative::build_units(const PackedStringArray &p_player_roster, const PackedStringArray &p_enemy_roster) {
	Ref<Resource> factory_resource = ResourceLoader::get_singleton()->load("res://scripts/batch_unit_factory.gd");
	GDScript *factory_script = Object::cast_to<GDScript>(factory_resource.ptr());
	if (factory_script == nullptr) {
		return Array();
	}

	Variant factory_instance = factory_script->new_();
	Object *factory_object = factory_instance.get_validated_object();
	if (factory_object == nullptr) {
		return Array();
	}

	const Variant result = factory_object->call("build_match_units", p_player_roster, p_enemy_roster);
	if (result.get_type() == Variant::ARRAY) {
		return result;
	}
	return Array();
}

Dictionary BatchMatchEngineNative::run_match(
		int32_t p_match_seed,
		double p_tick_rate,
		int32_t p_max_ticks,
		const PackedStringArray &p_player_roster,
		const PackedStringArray &p_enemy_roster,
		bool p_record_events
	) {
	(void)p_record_events;
	set_seed(p_match_seed);
	const Array units = build_units(p_player_roster, p_enemy_roster);
	set_units(units);

	Dictionary result;
	for (int32_t tick = 0; tick < p_max_ticks; ++tick) {
		step(p_tick_rate);
		result = get_match_result();
		if (!result.is_empty()) {
			break;
		}
	}

	if (result.is_empty()) {
		const teamfight::MatchResult summary = core_.get_match_result();
		result = _match_result_to_dictionary(summary);
		result["termination"] = "timeout";
		result["winner"] = "timeout";
	}

	Dictionary match_record;
	match_record["kind"] = "match_summary";
	match_record["schema_version"] = 1;
	match_record["termination"] = String(result.get("termination", "timeout"));
	match_record["winner"] = String(result.get("winner", "timeout"));
	match_record["player_kills"] = int(result.get("player_kills", 0));
	match_record["enemy_kills"] = int(result.get("enemy_kills", 0));
	match_record["time"] = float(result.get("time", 0.0));
	match_record["duration_seconds"] = float(result.get("time", 0.0));
	match_record["ticks"] = int(result.get("ticks", p_max_ticks));

	Dictionary output;
	output["match_record"] = match_record;
	output["unit_snapshots"] = snapshot_units();
	return output;
}

Object *BatchMatchEngineNative::get_combat_registry() const {
	return nullptr;
}

teamfight::UnitState BatchMatchEngineNative::_unit_from_variant(const Variant &p_unit) const {
	teamfight::UnitState unit;
	Dictionary data;

	if (p_unit.get_type() == Variant::OBJECT) {
		Object *object = p_unit;
		if (object != nullptr && object->has_method("to_dictionary")) {
			const Variant result = object->call("to_dictionary");
			if (result.get_type() == Variant::DICTIONARY) {
				data = result;
			}
		}
	} else if (p_unit.get_type() == Variant::DICTIONARY) {
		data = p_unit;
	}

	unit.hero_id = String(data.get("hero_id", "")).utf8().get_data();
	unit.display_name = String(data.get("display_name", unit.hero_id.c_str())).utf8().get_data();
	unit.role = String(data.get("role", "")).utf8().get_data();
	unit.team = String(data.get("team", "")).utf8().get_data();
	unit.passive_id = String(data.get("passive_id", "")).utf8().get_data();
	const Vector2 spawn_position = data.get("spawn_position", Vector2());
	unit.position_x = static_cast<float>(spawn_position.x);
	unit.position_y = static_cast<float>(spawn_position.y);
	unit.max_hp = static_cast<float>(data.get("max_hp", 1.0));
	unit.attack_damage = static_cast<float>(data.get("attack_damage", 0.0));
	unit.attack_range = static_cast<float>(data.get("attack_range", 0.0));
	unit.attack_speed = static_cast<float>(data.get("attack_speed", 1.0));
	unit.move_speed = static_cast<float>(data.get("move_speed", 0.0));
	unit.armor = static_cast<float>(data.get("armor", 0.0));
	unit.projectile_speed = static_cast<float>(data.get("projectile_speed", 0.0));
	unit.magic_resist = static_cast<float>(data.get("magic_resist", 0.0));
	unit.tenacity = static_cast<float>(data.get("tenacity", 0.0));
	unit.life_steal = static_cast<float>(data.get("life_steal", 0.0));
	unit.respawn_time = static_cast<float>(data.get("respawn_time", 0.0));
	unit.mana_per_attack = static_cast<float>(data.get("mana_per_attack", 0.0));
	unit.max_mana = static_cast<float>(data.get("max_mana", 0.0));
	unit.ability_cd = static_cast<float>(data.get("ability_cd", 0.0));
	unit.ultimate_cd = static_cast<float>(data.get("ultimate_cd", 0.0));
	unit.projectile_radius = static_cast<float>(data.get("projectile_radius", 0.0));
	unit.hp = unit.max_hp;
	return unit;
}

Dictionary BatchMatchEngineNative::_unit_to_dictionary(const teamfight::UnitState &p_unit) const {
	Dictionary unit;
	unit["instance_id"] = p_unit.instance_id;
	unit["hero_id"] = String(p_unit.hero_id.c_str());
	unit["display_name"] = String(p_unit.display_name.c_str());
	unit["role"] = String(p_unit.role.c_str());
	unit["team"] = String(p_unit.team.c_str());
	unit["passive_id"] = String(p_unit.passive_id.c_str());
	unit["alive"] = p_unit.alive;
	unit["hp"] = p_unit.hp;
	unit["shield"] = p_unit.shield;
	unit["position"] = Vector2(p_unit.position_x, p_unit.position_y);
	unit["target_id"] = p_unit.target_id;
	unit["in_range"] = p_unit.in_range;
	unit["kills"] = p_unit.kills;
	unit["deaths"] = p_unit.deaths;
	unit["assists"] = p_unit.assists;
	unit["damage_dealt"] = p_unit.damage_dealt;
	unit["damage_dealt_auto"] = p_unit.damage_dealt_auto;
	unit["damage_dealt_ability"] = p_unit.damage_dealt_ability;
	unit["damage_dealt_ultimate"] = p_unit.damage_dealt_ultimate;
	unit["damage_received"] = p_unit.damage_received;
	unit["damage_mitigated"] = p_unit.damage_mitigated;
	unit["healing_done"] = p_unit.healing_done;
	unit["shielding_done"] = p_unit.shielding_done;
	unit["auto_attacks_done"] = p_unit.auto_attacks_done;
	unit["abilities_used"] = p_unit.abilities_used;
	unit["ultimates_used"] = p_unit.ultimates_used;
	unit["stuns_dealt"] = p_unit.stuns_dealt;
	return unit;
}

Dictionary BatchMatchEngineNative::_match_result_to_dictionary(const teamfight::MatchResult &p_result) const {
	Dictionary result;
	result["termination"] = String(p_result.termination.c_str());
	result["winner"] = String(p_result.winner.c_str());
	result["player_kills"] = p_result.player_kills;
	result["enemy_kills"] = p_result.enemy_kills;
	result["time"] = p_result.time;
	result["ticks"] = p_result.ticks;
	return result;
}
