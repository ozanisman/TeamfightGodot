extends SceneTree

## Focused gate: draft UI scene loads, lays out core controls, and tile press updates in place.
## Run: .\run_godot.ps1 --check-draft-ui

const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")

func _init() -> void:
	if not NativeSimulationBackendScript.ensure_gdextension_loaded():
		push_error("draft_ui: native backend load failed")
		call_deferred("quit", 1)
		return
	call_deferred("_run")


func _run() -> void:
	var scene: PackedScene = load("res://scenes/draft_testing.tscn") as PackedScene
	if scene == null:
		push_error("draft_ui: PackedScene load failed")
		quit(1)
		return

	var inst: Node = scene.instantiate()
	if inst == null:
		push_error("draft_ui: instantiate failed")
		quit(1)
		return

	root.add_child(inst)
	await process_frame
	await process_frame

	var shell := inst.find_child("DraftScreenShell", true, false) as Control
	if shell == null:
		_fail(inst, "draft_ui: DraftScreenShell missing")
		return

	var draft_panel := shell.get_node_or_null("DraftPanel") as Control
	if draft_panel == null:
		_fail(inst, "draft_ui: DraftPanel missing")
		return

	if shell.get_node_or_null("DraftPanel/RoleFilterContainer") == null:
		_fail(inst, "draft_ui: RoleFilterContainer missing")
		return
	if shell.get_node_or_null("DraftPanel/ChampionScroll") == null:
		_fail(inst, "draft_ui: ChampionScroll missing")
		return
	if shell.get_node_or_null("DraftPanel/DraftActionBox/RandomDraftButton") == null:
		_fail(inst, "draft_ui: RandomDraftButton missing")
		return
	if shell.get_node_or_null("DraftPanel/DraftActionBox/StartMatchButton") == null:
		_fail(inst, "draft_ui: StartMatchButton missing")
		return
	if shell.get_node_or_null("RecommendationPanel") == null:
		_fail(inst, "draft_ui: RecommendationPanel missing")
		return

	var champion_grid := shell.get_node_or_null("DraftPanel/ChampionScroll/ChampionGrid") as GridContainer
	if champion_grid == null:
		_fail(inst, "draft_ui: ChampionGrid missing")
		return

	var tile := _first_draft_tile(champion_grid)
	if tile == null:
		_fail(inst, "draft_ui: draft champion tile missing")
		return

	var champion_id: StringName = tile.get("champion_id")
	if String(champion_id).is_empty():
		_fail(inst, "draft_ui: draft champion tile missing champion_id")
		return

	tile.emit_signal("champion_pressed", champion_id)
	await process_frame

	if not bool(tile.get("_is_banned")) and not bool(tile.get("_is_taken")):
		_fail(inst, "draft_ui: tile state did not update after press")
		return

	inst.queue_free()
	print("check_draft_ui_load: OK")
	quit(0)


func _first_draft_tile(grid: GridContainer) -> Button:
	for child in grid.get_children():
		if child is Button and child.has_signal("champion_pressed"):
			return child as Button
	return null


func _fail(inst: Node, message: String) -> void:
	push_error(message)
	if is_instance_valid(inst):
		inst.queue_free()
	quit(1)
