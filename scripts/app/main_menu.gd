extends Control

func _ready() -> void:
	# Center the menu
	size = Vector2(1920, 1080)
	position = Vector2(0, 0)

func _on_simulation_pressed() -> void:
	get_tree().change_scene_to_file("res://scenes/simulation_viewer.tscn")

func _on_stats_pressed() -> void:
	get_tree().change_scene_to_file("res://scenes/stats_dashboard.tscn")
