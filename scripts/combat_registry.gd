extends RefCounted
class_name CombatRegistry

var abilities: Dictionary = {}
var ultimates: Dictionary = {}
var passives: Dictionary = {
	"on_attack": {},
	"on_defense": {},
	"on_tick": {},
	"post_attack": {},
	"post_take_damage": {},
}


func reset() -> void:
	abilities.clear()
	ultimates.clear()
	for hook in passives.keys():
		(passives[hook] as Dictionary).clear()


func register_ability(key: String, effect: Object) -> void:
	if key.is_empty() or effect == null:
		return
	abilities[key] = effect


func register_ultimate(key: String, effect: Object) -> void:
	if key.is_empty() or effect == null:
		return
	ultimates[key] = effect


func register_passive(hook: String, key: String, effect: Object) -> void:
	if key.is_empty() or effect == null:
		return
	if not passives.has(hook):
		passives[hook] = {}
	var bucket: Dictionary = passives[hook]
	if not bucket.has(key):
		bucket[key] = []
	var effects: Array = bucket[key]
	effects.append(effect)
	bucket[key] = effects
	passives[hook] = bucket


func get_ability(keys: Array[String]) -> Object:
	for key in keys:
		if key.is_empty():
			continue
		if abilities.has(key):
			return abilities[key]
	return null


func get_ultimate(keys: Array[String]) -> Object:
	for key in keys:
		if key.is_empty():
			continue
		if ultimates.has(key):
			return ultimates[key]
	return null


func get_passives(hook: String, keys: Array[String]) -> Array:
	var result: Array = []
	if not passives.has(hook):
		return result
	var bucket: Dictionary = passives[hook]
	for key in keys:
		if key.is_empty() or not bucket.has(key):
			continue
		for effect in bucket[key]:
			if not result.has(effect):
				result.append(effect)
	return result
