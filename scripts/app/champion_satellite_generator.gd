class_name ChampionSatelliteGenerator
extends RefCounted
## Generates satellite specs for champions based on their abilities.

static var SatelliteRegistriesScript = null
const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")

static var _generators: Dictionary = {}


## Register a champion-specific satellite generator.
static func register_generator(champion_id: StringName, generator: Callable) -> void:
	_generators[champion_id] = generator


## Generate satellites for a champion.
static func generate_satellites(champion_id: StringName, unit_data: Dictionary = {}) -> Array[SatelliteSpec]:
	ensure_initialized()
	
	var satellites: Array[SatelliteSpec] = []
	var minion_satellites: Array[SatelliteSpec] = []
	var effect_satellites: Array[SatelliteSpec] = []
	
	# Check for champion-specific generator
	if _generators.has(champion_id):
		var generator: Callable = _generators[champion_id]
		satellites = generator.call(champion_id, unit_data)
	else:
		# Fall back to default summon parser
		satellites = _parse_summon_effects(champion_id, unit_data)
	
	# Separate minion satellites from effect satellites
	for satellite in satellites:
		if String(satellite.id).begins_with("minion_"):
			minion_satellites.append(satellite)
		else:
			effect_satellites.append(satellite)
	
	# Add CC effects
	if not unit_data.is_empty():
		# In-game: use active CC effects from unit data
		var cc_satellites: Array[SatelliteSpec] = _parse_cc_effects(unit_data)
		effect_satellites.append_array(cc_satellites)
	else:
		# Draft phase: parse CC effects from champion abilities
		var cc_satellites: Array[SatelliteSpec] = _parse_cc_from_abilities(champion_id)
		effect_satellites.append_array(cc_satellites)
		
		# Draft phase: parse utility effects from champion abilities
		var utility_satellites: Array[SatelliteSpec] = _parse_utility_from_abilities(champion_id)
		effect_satellites.append_array(utility_satellites)
		
		# Draft phase: parse damage types from champion abilities
		var damage_satellites: Array[SatelliteSpec] = _parse_damage_types(champion_id)
		effect_satellites.append_array(damage_satellites)
	
	# Sort minions alphabetically by title
	minion_satellites.sort_custom(func(a, b): 
		var a_title: String = _extract_title_from_id(a.id)
		var b_title: String = _extract_title_from_id(b.id)
		return a_title < b_title
	)
	
	# Sort effects alphabetically by title
	effect_satellites.sort_custom(func(a, b): 
		var a_title: String = _extract_title_from_id(a.id)
		var b_title: String = _extract_title_from_id(b.id)
		return a_title < b_title
	)
	
	# Return minions first, then effects
	minion_satellites.append_array(effect_satellites)
	return minion_satellites


## Extract title from satellite ID for sorting.
## Examples: "damage_type_physical" → "Physical", "status_dash" → "Dash"
static func _extract_title_from_id(id: StringName) -> String:
	var id_str: String = String(id)
	# Split by underscore and take the last part
	var parts: PackedStringArray = id_str.split("_")
	if parts.is_empty():
		return id_str.capitalize()
	var title: String = parts[-1]
	return title.capitalize()


## Extract all effect keywords from champion descriptions using text-based approach.
static func _extract_keywords_from_text(champion_id: StringName) -> Dictionary:
	var champion = ChampionCatalogScript.get_champion(champion_id)
	if champion == null:
		return {}

	var champion_data: Dictionary = champion.to_dict()

	# Combine all descriptions into one text for keyword extraction
	var combined_text: String = ""
	combined_text += str(champion_data.get("passive_desc", "")) + " "
	combined_text += str(champion_data.get("ability_desc", "")) + " "
	combined_text += str(champion_data.get("ultimate_desc", ""))

	var found_keywords: Dictionary = {
		"CC": [],
		"UTILITY": [],
		"DAMAGE": []
	}

	# Extract keywords using the same logic as text highlighting
	for keyword in SimConstantsScript.EFFECT_METADATA:
		var metadata: Dictionary = SimConstantsScript.EFFECT_METADATA[keyword]
		var category: String = metadata.get("category", "")

		# Match word boundaries with case-insensitive flag
		var regex: RegEx = RegEx.new()
		regex.compile("(?i)\\b([a-zA-Z]*%s[a-zA-Z]*)\\b" % keyword)
		var result = regex.search(combined_text)

		if result and found_keywords.has(category):
			if not found_keywords[category].has(keyword):
				found_keywords[category].append(keyword)

	return found_keywords


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


## Parse CC effects from champion descriptions (draft phase).
static func _parse_cc_from_abilities(champion_id: StringName) -> Array[SatelliteSpec]:
	var satellites: Array[SatelliteSpec] = []
	var keywords: Dictionary = _extract_keywords_from_text(champion_id)
	var cc_keywords: Array = keywords.get("CC", [])

	for keyword in cc_keywords:
		var effect_type: StringName = StringName(keyword)
		var spec: SatelliteSpec = SatelliteRegistriesScript.status(effect_type)
		if spec != null:
			satellites.append(spec)

	return satellites


## Parse utility effects from champion descriptions (draft phase).
static func _parse_utility_from_abilities(champion_id: StringName) -> Array[SatelliteSpec]:
	var satellites: Array[SatelliteSpec] = []
	var keywords: Dictionary = _extract_keywords_from_text(champion_id)
	var utility_keywords: Array = keywords.get("UTILITY", [])

	for keyword in utility_keywords:
		# Map heal_over_time to heal for display
		var display_keyword: String = keyword
		if keyword == "heal_over_time":
			display_keyword = "heal"
		var effect_type: StringName = StringName(display_keyword)
		var spec: SatelliteSpec = SatelliteRegistriesScript.status(effect_type)
		if spec != null:
			satellites.append(spec)

	return satellites


## Parse damage types from champion descriptions (draft phase).
static func _parse_damage_types(champion_id: StringName) -> Array[SatelliteSpec]:
	var satellites: Array[SatelliteSpec] = []
	var keywords: Dictionary = _extract_keywords_from_text(champion_id)
	var damage_keywords: Array = keywords.get("DAMAGE", [])

	for keyword in damage_keywords:
		# Strip " damage" suffix to match existing usage
		var short_name: String = keyword.replace(" damage", "")
		var damage_type: StringName = StringName(short_name)
		var spec: SatelliteSpec = SatelliteRegistriesScript.damage_type(damage_type)
		if spec != null:
			satellites.append(spec)

	return satellites


## Parse crowd control effects from unit data.
static func _parse_cc_effects(unit_data: Dictionary) -> Array[SatelliteSpec]:
	var satellites: Array[SatelliteSpec] = []
	
	# Check for active CC effects
	var cc_effects: Array[StringName] = SimConstantsScript.get_effect_kinds()
	
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
