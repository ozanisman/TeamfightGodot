#pragma once

#include <cstdint>

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/gd_script.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/variant.hpp>

#include "batch_match_engine.h"

using namespace godot;

class BatchMatchEngineNative : public RefCounted {
	GDCLASS(BatchMatchEngineNative, RefCounted);

public:
	void set_seed(int32_t p_seed);
	void set_units(const Array &p_units);
	void step(double p_dt);
	Dictionary get_match_result() const;
	Array snapshot_units() const;
	void clear();
	Array build_units(const PackedStringArray &p_player_roster, const PackedStringArray &p_enemy_roster);
	Dictionary run_match(
			int32_t p_match_seed,
			double p_tick_rate,
			int32_t p_max_ticks,
			const PackedStringArray &p_player_roster,
			const PackedStringArray &p_enemy_roster,
			bool p_record_events
		);
	Object *get_combat_registry() const;

protected:
	static void _bind_methods();

private:
	teamfight::BatchMatchEngine core_;

	teamfight::UnitState _unit_from_variant(const Variant &p_unit) const;
	Dictionary _unit_to_dictionary(const teamfight::UnitState &p_unit) const;
	Dictionary _match_result_to_dictionary(const teamfight::MatchResult &p_result) const;
};
