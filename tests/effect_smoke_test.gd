extends Node

const BattleUnitScript = preload("res://scripts/battle_unit.gd")
const CombatData = preload("res://scripts/combat_data.gd")
const CombatWorldScript = preload("res://scripts/combat_world.gd")
const ChampionCatalog = preload("res://scripts/champion_catalog.gd")
const DummyTargetScript = preload("res://tests/dummy_target.gd")

var failures: Array[String] = []


func _ready() -> void:
	var code := _run_smoke()
	for entry in failures:
		push_error(entry)
	if failures.is_empty():
		print("SMOKE OK")
	else:
		print("SMOKE FAIL: %d issue(s)" % failures.size())
	get_tree().quit(code)


func _run_smoke() -> int:
	var test_worlds: Array[Object] = []

	_test_swordsman(test_worlds)
	_test_guardian(test_worlds)
	_test_assassin(test_worlds)
	_test_archer(test_worlds)
	_test_mage(test_worlds)
	_test_oracle(test_worlds)

	for world in test_worlds:
		if is_instance_valid(world):
			world.clear()

	return 0 if failures.is_empty() else 1


func _setup_world(hero_id: String, enemy_specs: Array[Dictionary]) -> Dictionary:
	var world = CombatWorldScript.new()
	ChampionCatalog.bootstrap_combat_registry(world.get_combat_registry())

	var hero := _make_unit(hero_id, "player", Vector2(2.0, 5.0))
	var units: Array[Node2D] = [hero]

	var enemies: Array[DummyTarget] = []
	for spec in enemy_specs:
		var enemy := _make_dummy(String(spec.get("name", "Dummy")), Vector2(spec.get("x", 4.0)), Vector2(spec.get("y", 5.0)), float(spec.get("hp", 1000.0)))
		enemies.append(enemy)
		units.append(enemy)

	world.set_units(units)
	hero.call("set_combat_registry", world.get_combat_registry())
	hero.call("set_world_position", Vector2(2.0, 5.0))
	hero.call("set_spawn_position", Vector2(2.0, 5.0))
	for enemy in enemies:
		enemy.set_world_position(Vector2(enemy.world_pos))
	world.capture_spawn_positions()

	return {"world": world, "hero": hero, "enemies": enemies}


func _make_unit(hero_id: String, team: String, pos: Vector2) -> BattleUnitScript:
	var hero := ChampionCatalog.get_by_id(hero_id)
	var unit: BattleUnitScript = BattleUnitScript.new()
	unit.call(
		"set_identity",
		hero_id,
		String(hero.get("name", hero_id)),
		String(hero.get("role", "")),
		team,
		Color(1.0, 1.0, 1.0),
		false,
		String(hero.get("passive_id", ""))
	)
	unit.call(
		"set_combat_stats",
		float(hero.get("max_hp", 1.0)),
		float(hero.get("attack_damage", 0.0)),
		float(hero.get("attack_range", 0.3)),
		float(hero.get("attack_speed", 1.0)),
		float(hero.get("move_speed", 1.0)),
		float(hero.get("armor", 0.0)),
		float(hero.get("projectile_speed", CombatData.DEFAULT_PROJECTILE_SPEED)),
		float(hero.get("magic_resist", 0.0)),
		float(hero.get("tenacity", 0.0)),
		float(hero.get("life_steal", 0.0)),
		float(hero.get("respawn_time", CombatData.RESPAWN_TIME)),
		float(hero.get("mana_per_attack", 0.0)),
		float(hero.get("max_mana", 0.0)),
		float(hero.get("ability_cd", 0.0)),
		float(hero.get("ultimate_cd", 0.0)),
		float(hero.get("projectile_radius", CombatData.DEFAULT_PROJECTILE_RADIUS))
	)
	unit.call("set_spawn_position", pos)
	unit.call("set_world_position", pos)
	return unit


func _make_dummy(name: String, pos: Vector2, hp: float) -> DummyTargetScript:
	var dummy: DummyTargetScript = DummyTargetScript.new()
	dummy.display_name = name
	dummy.world_pos = pos
	dummy.max_hp = hp
	dummy.hp = hp
	dummy.shield = 0.0
	dummy.alive = true
	return dummy


func _bind_registry(unit: Object, world: Object) -> void:
	unit.call("set_combat_registry", world.get_combat_registry())


func _assert_close(actual: float, expected: float, label: String, epsilon: float = 0.01) -> void:
	if not is_equal_approx(actual, expected):
		failures.append("%s expected %.3f got %.3f" % [label, expected, actual])


func _assert_true(condition: bool, label: String) -> void:
	if not condition:
		failures.append(label)


func _resolve_first_projectile(world: Object, target: Object) -> void:
	if world.projectiles.is_empty():
		failures.append("expected projectile but none was spawned")
		return
	var projectile: Dictionary = world.projectiles[0]
	world.call("_resolve_projectile_hit", projectile, target)
	world.projectiles.clear()


func _test_swordsman(_worlds: Array[Object]) -> void:
	var pack := _setup_world("swordsman", [{"name": "SwordsmanTarget", "x": 2.2, "y": 5.0, "hp": 1000.0}])
	var world: Object = pack["world"]
	var hero = pack["hero"]
	var target = pack["enemies"][0]
	_worlds.append(world)
	_bind_registry(hero, world)
	hero.call("set_combat_registry", world.get_combat_registry())
	hero.set("current_target", target)

	var base_hp := float(target.hp)
	var auto_result: Dictionary = hero.call("attack", target, world, "Smoke Auto")
	if auto_result.is_empty():
		_resolve_first_projectile(world, target)
	_assert_close(base_hp - float(target.hp), float(hero.attack_damage) * 1.2, "Swordsman auto damage")

	target.hp = base_hp
	hero.set("mana", 0.0)
	hero.set("ultimate_timer", 0.0)
	hero.set("ability_timer", 0.0)
	hero.call("_begin_cast", world, false)
	hero.call("update", CombatData.CASTING_WINDUP, world)
	_assert_close(base_hp - float(target.hp), float(hero.attack_damage) * 1.5, "Swordsman ability damage")

	target.hp = base_hp
	hero.set("mana", float(hero.max_mana))
	hero.set("ultimate_timer", 0.0)
	hero.call("_begin_cast", world, true)
	hero.call("update", CombatData.CASTING_WINDUP, world)
	_assert_close(base_hp - float(target.hp), float(hero.attack_damage) * 3.0, "Swordsman ultimate damage")


func _test_guardian(_worlds: Array[Object]) -> void:
	var pack := _setup_world("guardian", [{"name": "GuardianTarget", "x": 2.2, "y": 5.0, "hp": 1000.0}])
	var world: Object = pack["world"]
	var hero = pack["hero"]
	var target = pack["enemies"][0]
	_worlds.append(world)
	_bind_registry(hero, world)
	hero.set("current_target", target)
	hero.set("current_ally_target", hero)

	var shield_before := float(hero.shield)
	hero.set("ability_timer", 0.0)
	hero.call("_begin_cast", world, false)
	hero.call("update", CombatData.CASTING_WINDUP, world)
	_assert_close(float(hero.shield) - shield_before, float(hero.max_hp) * 0.2, "Guardian shield ability")

	var hp_before := float(hero.hp)
	var mana_before := float(hero.mana)
	hero.call("take_damage", 50.0, world, "physical", int(target.instance_id))
	var expected_loss := 50.0 * 0.9 * (1.0 - float(hero.armor))
	_assert_close(hp_before - float(hero.hp), expected_loss, "Guardian defense passive")
	_assert_close(float(hero.mana) - mana_before, expected_loss * 0.10, "Guardian post-damage mana gain")

	target.hp = 1000.0
	hero.set("mana", float(hero.max_mana))
	hero.set("ultimate_timer", 0.0)
	hero.call("_begin_cast", world, true)
	hero.call("update", CombatData.CASTING_WINDUP, world)
	_assert_close(1000.0 - float(target.hp), float(hero.attack_damage) * 2.0, "Guardian ultimate damage")


func _test_assassin(_worlds: Array[Object]) -> void:
	var pack := _setup_world("assassin", [{"name": "AssassinTarget", "x": 2.2, "y": 5.0, "hp": 1000.0}])
	var world: Object = pack["world"]
	var hero = pack["hero"]
	var target = pack["enemies"][0]
	_worlds.append(world)
	_bind_registry(hero, world)
	hero.set("current_target", target)

	hero.set("ability_timer", 0.0)
	hero.call("_begin_cast", world, false)
	hero.call("update", CombatData.CASTING_WINDUP, world)
	_assert_close(1000.0 - float(target.hp), float(hero.attack_damage) * 1.2, "Assassin ability damage")

	target.hp = 1000.0
	hero.set("mana", float(hero.max_mana))
	hero.set("ultimate_timer", 0.0)
	hero.call("_begin_cast", world, true)
	hero.call("update", CombatData.CASTING_WINDUP, world)
	_assert_close(1000.0 - float(target.hp), float(hero.attack_damage) * 3.5, "Assassin ultimate damage")

	target.hp = target.max_hp * 0.2
	var low_hp_before := float(target.hp)
	hero.call("attack", target, world, "Smoke Passive")
	if world.projectiles.is_empty():
		failures.append("Assassin passive attack did not spawn a projectile or damage")
	else:
		_resolve_first_projectile(world, target)
	_assert_close(low_hp_before - float(target.hp), float(hero.attack_damage) * 2.0, "Assassin execute passive")


func _test_archer(_worlds: Array[Object]) -> void:
	var pack := _setup_world("archer", [
		{"name": "ArcherPrimary", "x": 5.0, "y": 5.0, "hp": 1000.0},
		{"name": "ArcherSplash", "x": 5.4, "y": 5.0, "hp": 1000.0},
	])
	var world: Object = pack["world"]
	var hero = pack["hero"]
	var primary = pack["enemies"][0]
	var splash = pack["enemies"][1]
	_worlds.append(world)
	_bind_registry(hero, world)
	hero.set("current_target", primary)

	hero.set("ability_timer", 0.0)
	hero.call("_begin_cast", world, false)
	hero.call("update", CombatData.CASTING_WINDUP, world)
	_resolve_first_projectile(world, primary)
	_assert_close(1000.0 - float(primary.hp), float(hero.attack_damage) * 1.5, "Archer ability projectile")

	primary.hp = 1000.0
	splash.hp = 1000.0
	hero.set("mana", float(hero.max_mana))
	hero.set("ultimate_timer", 0.0)
	hero.call("_begin_cast", world, true)
	hero.call("update", CombatData.CASTING_WINDUP, world)
	_resolve_first_projectile(world, primary)
	_assert_close(1000.0 - float(primary.hp), float(hero.attack_damage) * 4.0, "Archer ultimate primary damage")
	_assert_close(1000.0 - float(splash.hp), float(hero.attack_damage) * 4.0 * 0.5, "Archer ultimate splash damage")

	primary.hp = 1000.0
	hero.set("current_target", primary)
	hero.call("attack", primary, world, "Smoke Passive")
	_resolve_first_projectile(world, primary)
	_assert_close(1000.0 - float(primary.hp), float(hero.attack_damage) * 1.25, "Archer attack passive")


func _test_mage(_worlds: Array[Object]) -> void:
	var pack := _setup_world("mage", [
		{"name": "MagePrimary", "x": 5.0, "y": 5.0, "hp": 1000.0},
		{"name": "MageSplash", "x": 5.4, "y": 5.0, "hp": 1000.0},
	])
	var world: Object = pack["world"]
	var hero = pack["hero"]
	var primary = pack["enemies"][0]
	var splash = pack["enemies"][1]
	_worlds.append(world)
	_bind_registry(hero, world)
	hero.set("current_target", null)
	hero.set("mana", 0.0)
	hero.call("update", 1.0, world)
	_assert_close(float(hero.mana), 4.0, "Mage mana regen passive")

	hero.set("current_target", primary)
	hero.set("ability_timer", 0.0)
	hero.call("_begin_cast", world, false)
	hero.call("update", CombatData.CASTING_WINDUP, world)
	_resolve_first_projectile(world, primary)
	_assert_close(1000.0 - float(primary.hp), float(hero.attack_damage) * 2.0, "Mage ability projectile")

	primary.hp = 1000.0
	splash.hp = 1000.0
	hero.set("mana", float(hero.max_mana))
	hero.set("ultimate_timer", 0.0)
	hero.call("_begin_cast", world, true)
	hero.call("update", CombatData.CASTING_WINDUP, world)
	_resolve_first_projectile(world, primary)
	_assert_close(1000.0 - float(primary.hp), float(hero.attack_damage) * 6.0, "Mage ultimate primary damage")
	_assert_close(1000.0 - float(splash.hp), float(hero.attack_damage) * 6.0 * 0.5, "Mage ultimate splash damage")


func _test_oracle(_worlds: Array[Object]) -> void:
	var pack := _setup_world("oracle", [{"name": "OracleTarget", "x": 5.0, "y": 5.0, "hp": 1000.0}])
	var world: Object = pack["world"]
	var hero = pack["hero"]
	var target = pack["enemies"][0]
	_worlds.append(world)
	_bind_registry(hero, world)
	hero.set("current_target", target)
	hero.set("current_ally_target", hero)

	hero.hp = hero.max_hp * 0.5
	hero.set("ability_timer", 0.0)
	hero.call("_begin_cast", world, false)
	hero.call("update", CombatData.CASTING_WINDUP, world)
	_assert_close(float(hero.hp), hero.max_hp * 0.7, "Oracle heal ability")

	hero.set("mana", 0.0)
	hero.set("ultimate_timer", 0.0)
	hero.call("_begin_cast", world, true)
	hero.call("update", CombatData.CASTING_WINDUP, world)
	_assert_close(float(hero.shield), hero.max_hp * 0.4, "Oracle shield ultimate")

	hero.set("mana", 0.0)
	hero.set("current_target", target)
	hero.call("attack", target, world, "Smoke Passive")
	_resolve_first_projectile(world, target)
	_assert_close(float(hero.mana), 5.0, "Oracle post-attack mana restore")
