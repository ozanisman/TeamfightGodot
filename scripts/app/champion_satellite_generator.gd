class_name ChampionSatelliteGenerator
extends RefCounted
## Generates satellite specs for champions based on their abilities.

static var SatelliteRegistriesScript = null
const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")

static var _generators: Dictionary = {}


## Register a champion-specific satellite generator.
static func register_generator(champion_id: StringName, generator: Callable) -> void:
	_generators[champion_id] = generator


## Generate satellites for a champion.
static func generate_satellites(champion_id: StringName, unit_data: Dictionary = {}) -> Array[SatelliteSpec]:
	ensure_initialized()
	
	var satellites: Array[SatelliteSpec] = []
	
	# Check for champion-specific generator
	if _generators.has(champion_id):
		var generator: Callable = _generators[champion_id]
		satellites = generator.call(champion_id, unit_data)
	else:
		# Fall back to default summon parser
		satellites = _parse_summon_effects(champion_id, unit_data)
	
	# Add CC effects
	if not unit_data.is_empty():
		# In-game: use active CC effects from unit data
		var cc_satellites: Array[SatelliteSpec] = _parse_cc_effects(unit_data)
		satellites.append_array(cc_satellites)
	else:
		# Draft phase: parse CC effects from champion abilities
		var cc_satellites: Array[SatelliteSpec] = _parse_cc_from_abilities(champion_id)
		satellites.append_array(cc_satellites)
		
		# Draft phase: parse damage types from champion abilities
		var damage_satellites: Array[SatelliteSpec] = _parse_damage_types(champion_id)
		satellites.append_array(damage_satellites)
	
	# Sort satellites alphabetically by ID
	satellites.sort_custom(func(a, b): return String(a.id) < String(b.id))
	
	return satellites


## Parse summon effects from champion abilities.
static func _parse_summon_effects(champion_id: StringName, unit_data: Dictionary) -> Array[SatelliteSpec]:
	var satellites: Array[SatelliteSpec] = []
	
	var champion = ChampionCatalogScript.get_champion(champion_id)
	if champion == null:
		return satellites
	
	var champion_data: Dictionary = champion.to_dict()
	
	# Parse ability for summon effects
	var ability = champion_data.get("ability")
	if ability and not ability.is_empty():
		_parse_single_summon(ability, satellites, champion_id)
	
	# Parse ultimate for summon effects
	var ultimate = champion_data.get("ultimate")
	if ultimate and not ultimate.is_empty():
		_parse_single_summon(ultimate, satellites, champion_id)
	
	return satellites


## Parse CC effects from champion abilities (draft phase).
static func _parse_cc_from_abilities(champion_id: StringName) -> Array[SatelliteSpec]:
	var satellites: Array[SatelliteSpec] = []
	
	var champion = ChampionCatalogScript.get_champion(champion_id)
	if champion == null:
		return satellites
	
	var champion_data: Dictionary = champion.to_dict()
	var cc_effects: Array[StringName] = []
	
	# Parse ability for CC effects
	var ability = champion_data.get("ability")
	if ability and not ability.is_empty():
		_parse_single_cc(ability, cc_effects)
	
	# Parse ultimate for CC effects
	var ultimate = champion_data.get("ultimate")
	if ultimate and not ultimate.is_empty():
		_parse_single_cc(ultimate, cc_effects)
	
	# Parse passives for CC effects
	var passive_ids: Array = champion_data.get("passive_ids", [])
	var passives_dict: Dictionary = ChampionCatalogScript.build_passive_registry()
	for passive_id in passive_ids:
		var passive: Dictionary = passives_dict.get(passive_id, {})
		if not passive.is_empty():
			# Passives have nested effect arrays (post_attack, on_attack, etc.)
			for key in passive:
				var value = passive[key]
				if value is Array:
					for effect in value:
						# Convert EffectSpec to dictionary if needed
						var effect_dict: Dictionary
						if effect.has_method("to_dict"):
							effect_dict = effect.to_dict()
						elif effect is Dictionary:
							effect_dict = effect
						else:
							continue
						_parse_single_cc(effect_dict, cc_effects)
	
	# Create satellite specs for found CC effects
	for effect_type in cc_effects:
		var spec: SatelliteSpec = SatelliteRegistriesScript.status(effect_type)
		if spec != null:
			satellites.append(spec)
	
	return satellites


## Parse damage types from champion abilities (draft phase).
static func _parse_damage_types(champion_id: StringName) -> Array[SatelliteSpec]:
	var satellites: Array[SatelliteSpec] = []
	
	var champion = ChampionCatalogScript.get_champion(champion_id)
	if champion == null:
		return satellites
	
	var champion_data: Dictionary = champion.to_dict()
	var damage_types: Array[StringName] = []
	
	# Parse ability for damage types
	var ability = champion_data.get("ability")
	if ability and not ability.is_empty():
		_parse_single_damage_type(ability, damage_types)
	
	# Parse ultimate for damage types
	var ultimate = champion_data.get("ultimate")
	if ultimate and not ultimate.is_empty():
		_parse_single_damage_type(ultimate, damage_types)
	
	# Parse passives for damage types
	var passive_ids: Array = champion_data.get("passive_ids", [])
	var passives_dict: Dictionary = ChampionCatalogScript.build_passive_registry()
	for passive_id in passive_ids:
		var passive: Dictionary = passives_dict.get(passive_id, {})
		if not passive.is_empty():
			# Passives have nested effect arrays (post_attack, on_attack, etc.)
			for key in passive:
				var value = passive[key]
				if value is Array:
					for effect in value:
						# Convert EffectSpec to dictionary if needed
						var effect_dict: Dictionary
						if effect.has_method("to_dict"):
							effect_dict = effect.to_dict()
						elif effect is Dictionary:
							effect_dict = effect
						else:
							continue
						_parse_single_damage_type(effect_dict, damage_types)
	
	# Create satellite specs for found damage types
	for damage_type in damage_types:
		var spec: SatelliteSpec = SatelliteRegistriesScript.damage_type(damage_type)
		if spec != null:
			satellites.append(spec)
	
	return satellites


## Parse a single effect for damage types.
static func _parse_single_damage_type(effect: Dictionary, damage_types: Array[StringName]) -> void:
	var kind: StringName = effect.get("kind", &"")
	
	# Check for damage kinds
	match kind:
		&"damage", &"aoe_damage":
			var params: Dictionary = effect.get("params", {})
			var damage_type: StringName = params.get("damage_type", &"")
			if not damage_type.is_empty() and not damage_types.has(damage_type):
				damage_types.append(damage_type)
		&"multi_effect":
			var params: Dictionary = effect.get("params", {})
			var effects: Array = params.get("effects", [])
			for sub_effect in effects:
				_parse_single_damage_type(sub_effect, damage_types)


## Parse a single effect for CC effects.
static func _parse_single_cc(effect: Dictionary, cc_effects: Array[StringName]) -> void:
	var kind: StringName = effect.get("kind", &"")
	
	# Check for direct CC kinds
	match kind:
		&"stun", &"silence", &"root", &"taunt", &"reflect":
			if not cc_effects.has(kind):
				cc_effects.append(kind)
		&"aoe_taunt":
			if not cc_effects.has(&"taunt"):
				cc_effects.append(&"taunt")
		&"aoe_stun":
			if not cc_effects.has(&"stun"):
				cc_effects.append(&"stun")
		&"aoe_silence":
			if not cc_effects.has(&"silence"):
				cc_effects.append(&"silence")
		&"aoe_root":
			if not cc_effects.has(&"root"):
				cc_effects.append(&"root")
		&"aoe_reflect":
			if not cc_effects.has(&"reflect"):
				cc_effects.append(&"reflect")
		&"every_n_attacks_stun":
			if not cc_effects.has(&"stun"):
				cc_effects.append(&"stun")
		&"multi_effect":
			var params: Dictionary = effect.get("params", {})
			var effects: Array = params.get("effects", [])
			for sub_effect in effects:
				_parse_single_cc(sub_effect, cc_effects)
		_:
			# Check params for CC-related fields
			var params: Dictionary = effect.get("params", {})
			if params.has("stun_duration") and params["stun_duration"] > 0:
				if not cc_effects.has(&"stun"):
					cc_effects.append(&"stun")
			if params.has("silence_duration") and params["silence_duration"] > 0:
				if not cc_effects.has(&"silence"):
					cc_effects.append(&"silence")
			if params.has("root_duration") and params["root_duration"] > 0:
				if not cc_effects.has(&"root"):
					cc_effects.append(&"root")
			if params.has("taunt_duration") and params["taunt_duration"] > 0:
				if not cc_effects.has(&"taunt"):
					cc_effects.append(&"taunt")


## Parse crowd control effects from unit data.
static func _parse_cc_effects(unit_data: Dictionary) -> Array[SatelliteSpec]:
	var satellites: Array[SatelliteSpec] = []
	
	# Check for active CC effects
	var cc_effects: Array[StringName] = [&"stun", &"silence", &"root", &"taunt", &"reflect"]
	
	for effect_type in cc_effects:
		var duration_key: StringName = StringName("%s_remaining" % effect_type)
		var duration: float = unit_data.get(duration_key, 0.0)
		
		# Reflect uses reflect_buff_remaining instead
		if effect_type == &"reflect":
			duration = unit_data.get("reflect_buff_remaining", 0.0)
		
		if duration > 0.0:
			var spec: SatelliteSpec = SatelliteRegistriesScript.status(effect_type)
			if spec != null:
				satellites.append(spec)
	
	return satellites


## Parse a single effect for summon_ally kind.
static func _parse_single_summon(effect: Dictionary, satellites: Array[SatelliteSpec], champion_id: StringName) -> void:
	var kind: StringName = effect.get("kind", &"")
	if kind != &"summon_ally":
		return
	
	var params: Dictionary = effect.get("params", {})
	var minions: Array = params.get("minions", [])
	
	for minion_data in minions:
		var minion_id: StringName = StringName(minion_data.get("minion_id", ""))
		var count: int = minion_data.get("count", 1)
		
		if minion_id.is_empty():
			continue
		
		# Create a satellite spec for this minion
		var spec: SatelliteSpec = SatelliteRegistriesScript.minion(minion_id)
		if spec != null:
			satellites.append(spec)


## Initialize default generators.
static func _static_init() -> void:
	# Register necromancer generator (uses default summon parser)
	register_generator(&"necromancer", func(champion_id: StringName, unit_data: Dictionary) -> Array[SatelliteSpec]:
		return _parse_summon_effects(champion_id, unit_data)
	)


## Static initialization helper.
static func ensure_initialized() -> void:
	if SatelliteRegistriesScript == null:
		SatelliteRegistriesScript = load("res://scripts/app/satellite_registries.gd")
	if _generators.is_empty():
		_static_init()
