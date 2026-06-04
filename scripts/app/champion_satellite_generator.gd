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
		
		# Draft phase: parse utility effects from champion abilities
		var utility_satellites: Array[SatelliteSpec] = _parse_utility_from_abilities(champion_id)
		satellites.append_array(utility_satellites)
		
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


## Parse utility effects from champion abilities (draft phase).
static func _parse_utility_from_abilities(champion_id: StringName) -> Array[SatelliteSpec]:
	var satellites: Array[SatelliteSpec] = []
	
	var champion = ChampionCatalogScript.get_champion(champion_id)
	if champion == null:
		return satellites
	
	var champion_data: Dictionary = champion.to_dict()
	var utility_effects: Array[StringName] = []
	
	# Parse ability for utility effects
	var ability = champion_data.get("ability")
	if ability and not ability.is_empty():
		_parse_single_utility(ability, utility_effects)
	
	# Parse ultimate for utility effects
	var ultimate = champion_data.get("ultimate")
	if ultimate and not ultimate.is_empty():
		_parse_single_utility(ultimate, utility_effects)
	
	# Parse passives for utility effects
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
						_parse_single_utility(effect_dict, utility_effects)
	
	# Create satellite specs for found utility effects
	for effect_type in utility_effects:
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
			if not damage_type.is_empty() and damage_type in SimConstants.get_damage_types() and not damage_types.has(damage_type):
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
	if kind in SimConstants.get_effect_kinds():
		if not cc_effects.has(kind):
			cc_effects.append(kind)
	
	# Check for AOE variants
	if str(kind).begins_with("aoe_"):
		var base_effect: StringName = SimConstants.get_base_effect_from_aoe(kind)
		# Only add if base_effect is actually a valid CC effect kind
		if base_effect in SimConstants.get_effect_kinds() and not cc_effects.has(base_effect):
			cc_effects.append(base_effect)
	
	# Check for passive triggers
	match kind:
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


## Parse a single effect for utility effects.
static func _parse_single_utility(effect: Dictionary, utility_effects: Array[StringName]) -> void:
	var kind: StringName = effect.get("kind", &"")
	
	# Check for direct utility kinds (heal, heal_over_time, shield, stealth, dodge, dash, summon)
	var utility_kinds: Array[StringName] = [&"heal", &"heal_over_time", &"shield", &"stealth", &"dodge", &"dash", &"summon"]
	if kind in utility_kinds:
		# Map heal_over_time to heal for display
		var display_kind: StringName = kind
		if kind == &"heal_over_time":
			display_kind = &"heal"
		if not utility_effects.has(display_kind):
			utility_effects.append(display_kind)
	
	# Check for auto_dodge (map to dodge)
	if kind == &"auto_dodge":
		if not utility_effects.has(&"dodge"):
			utility_effects.append(&"dodge")
	
	# Check for self_dash (map to dash)
	if kind == &"self_dash":
		if not utility_effects.has(&"dash"):
			utility_effects.append(&"dash")
	
	# Check for summon_ally (map to summon)
	if kind == &"summon_ally":
		if not utility_effects.has(&"summon"):
			utility_effects.append(&"summon")
	
	# Check for mana-related effects (map to mana)
	var mana_kinds: Array[StringName] = [&"mana_restore", &"mana_regen", &"mana_restore_on_hit", &"drain_target_mana_on_hit"]
	if kind in mana_kinds:
		if not utility_effects.has(&"mana"):
			utility_effects.append(&"mana")
	
	# Check for AOE variants
	if str(kind).begins_with("aoe_"):
		var base_effect: StringName = SimConstants.get_base_effect_from_aoe(kind)
		# Map aoe_heal_over_time to heal for display
		var display_base: StringName = base_effect
		if base_effect == &"heal_over_time":
			display_base = &"heal"
		if display_base in utility_kinds and not utility_effects.has(display_base):
			utility_effects.append(display_base)
	
	# Check for multi_effect
	match kind:
		&"multi_effect":
			var params: Dictionary = effect.get("params", {})
			var effects: Array = params.get("effects", [])
			for sub_effect in effects:
				_parse_single_utility(sub_effect, utility_effects)


## Parse crowd control effects from unit data.
static func _parse_cc_effects(unit_data: Dictionary) -> Array[SatelliteSpec]:
	var satellites: Array[SatelliteSpec] = []
	
	# Check for active CC effects
	var cc_effects: Array[StringName] = SimConstants.get_effect_kinds()
	
	for effect_type in cc_effects:
		var duration_key: StringName = StringName("%s_remaining" % effect_type)
		var duration: float = unit_data.get(duration_key, 0.0)
		
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
