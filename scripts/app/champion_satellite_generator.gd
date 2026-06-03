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
	
	# Check for champion-specific generator
	if _generators.has(champion_id):
		var generator: Callable = _generators[champion_id]
		var result = generator.call(champion_id, unit_data)
		return result
	
	# Fall back to default summon parser
	return _parse_summon_effects(champion_id, unit_data)


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
