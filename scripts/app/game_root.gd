extends Node

const HeadlessRunnerScript := preload("res://scripts/simulation/headless_runner.gd")

func _ready() -> void:
	if OS.get_cmdline_user_args().has("--check-only"):
		get_tree().quit()
		return
	if OS.has_feature("headless") or OS.get_cmdline_user_args().has("--headless-run"):
		HeadlessRunnerScript.run_from_cli(get_tree())
		return

	# Presentation layer will attach here once the compiled simulation core exists.
	print("Teamfight Godot scaffold loaded.")
