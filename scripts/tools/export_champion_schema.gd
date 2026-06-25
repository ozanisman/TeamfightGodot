extends SceneTree

const SimulationSchemaScript := preload("res://scripts/simulation/simulation_schema.gd")

func _init():
	var success := SimulationSchemaScript.write_champion_schema_to_file()
	if success:
		print("Successfully generated champion schema to fixtures/goldens/champion_schema.json")
	else:
		print("Failed to generate champion schema")
	quit(0 if success else 1)
