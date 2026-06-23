extends SceneTree

## Focused gate: main menu scene loads and exposes the expected route buttons.
## Run: .\run_godot.ps1 -- --check-main-menu

func _init() -> void:
	call_deferred("_run")


func _run() -> void:
	var scene: PackedScene = load("res://scenes/main_menu.tscn") as PackedScene
	if scene == null:
		push_error("main_menu: PackedScene load failed")
		quit(1)
		return

	var inst: Node = scene.instantiate()
	if inst == null:
		push_error("main_menu: instantiate failed")
		quit(1)
		return

	root.add_child(inst)
	await process_frame

	if inst.get_node_or_null("MenuBox/TitleLabel") == null:
		_fail(inst, "main_menu: TitleLabel missing")
		return
	if inst.get_node_or_null("MenuBox/ButtonRow/SimulationButton") == null:
		_fail(inst, "main_menu: SimulationButton missing")
		return
	if inst.get_node_or_null("MenuBox/ButtonRow/StatsButton") == null:
		_fail(inst, "main_menu: StatsButton missing")
		return
	if inst.get_node_or_null("MenuBox/ButtonRow/DraftTestingButton") == null:
		_fail(inst, "main_menu: DraftTestingButton missing")
		return

	inst.queue_free()
	print("check_main_menu_load: OK")
	quit(0)


func _fail(inst: Node, message: String) -> void:
	push_error(message)
	if is_instance_valid(inst):
		inst.queue_free()
	quit(1)
