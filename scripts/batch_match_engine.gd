extends RefCounted
class_name BatchMatchEngine

const BatchMatchEngineFallbackScript = preload("res://scripts/batch_match_engine_fallback.gd")
const BatchUnitFactoryScript = preload("res://scripts/batch_unit_factory.gd")
const BatchCoreExtensionPath = "res://native/batch_core/batch_core.gdextension"

signal combat_event_recorded(event: Dictionary)

var _impl: Object = null


func _init() -> void:
	_impl = _create_impl()
	if _impl != null and _impl.has_signal("combat_event_recorded"):
		_impl.connect("combat_event_recorded", Callable(self, "_on_impl_combat_event_recorded"))


func run_match(
		match_seed: int,
		tick_rate: float,
		max_ticks: int,
		player_roster: Array[String],
		enemy_roster: Array[String],
		record_events: bool
	) -> Dictionary:
	if _impl == null or not _impl.has_method("run_match"):
		return {}
	return _impl.call("run_match", match_seed, tick_rate, max_ticks, player_roster, enemy_roster, record_events)


func build_units(player_roster: Array[String], enemy_roster: Array[String]) -> Array:
	return BatchUnitFactoryScript.build_match_units(player_roster, enemy_roster)


func set_seed(match_seed: int) -> void:
	if _impl != null and _impl.has_method("set_seed"):
		_impl.call("set_seed", match_seed)


func set_units(units: Array) -> void:
	if _impl != null and _impl.has_method("set_units"):
		_impl.call("set_units", units)


func step(dt: float) -> void:
	if _impl != null and _impl.has_method("step"):
		_impl.call("step", dt)


func get_match_result() -> Dictionary:
	if _impl != null and _impl.has_method("get_match_result"):
		return _impl.call("get_match_result")
	return {}


func snapshot_units() -> Array[Dictionary]:
	if _impl != null and _impl.has_method("snapshot_units"):
		var snapshots: Variant = _impl.call("snapshot_units")
		if snapshots is Array:
			return _as_dictionary_array(snapshots)
	return []


func clear() -> void:
	if _impl != null and _impl.has_method("clear"):
		_impl.call("clear")


func get_combat_registry() -> Object:
	if _impl != null and _impl.has_method("get_combat_registry"):
		return _impl.call("get_combat_registry")
	return null


func _create_impl() -> Object:
	_ensure_native_extension_loaded()
	if ClassDB.class_exists("BatchMatchEngineNative"):
		var native: Object = ClassDB.instantiate("BatchMatchEngineNative")
		if native != null:
			return native
	return BatchMatchEngineFallbackScript.new()


func _ensure_native_extension_loaded() -> void:
	if ClassDB.class_exists("BatchMatchEngineNative"):
		return
	var extension_config: Resource = load(BatchCoreExtensionPath)
	if extension_config == null:
		return


func _as_dictionary_array(values: Array) -> Array[Dictionary]:
	var typed_values: Array[Dictionary] = []
	typed_values.resize(values.size())
	for index in range(values.size()):
		if values[index] is Dictionary:
			typed_values[index] = values[index]
		else:
			typed_values[index] = {}
	return typed_values


func _on_impl_combat_event_recorded(event: Dictionary) -> void:
	combat_event_recorded.emit(event)
