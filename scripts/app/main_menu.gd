extends Control

const UI_WINDOW_MIN := Vector2(1920, 1080)

func _ready() -> void:
	custom_minimum_size = UI_WINDOW_MIN

func _on_simulation_pressed() -> void:
	get_tree().change_scene_to_file("res://scenes/simulation_viewer.tscn")

func _on_stats_pressed() -> void:
	get_tree().change_scene_to_file("res://scenes/stats_dashboard.tscn")

func _on_draft_testing_pressed() -> void:
	get_tree().change_scene_to_file("res://scenes/draft_testing.tscn")
