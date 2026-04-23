extends SceneTree

const HeadlessRunnerScript := preload("res://scripts/simulation/headless_runner.gd")

func _init() -> void:
	call_deferred("_boot")

func _boot() -> void:
	await process_frame
	await HeadlessRunnerScript.run_from_cli(self)
