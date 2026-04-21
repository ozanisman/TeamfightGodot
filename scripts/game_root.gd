extends Control
class_name GameRoot

const WorldStateScript = preload("res://scripts/world_state.gd")
const ChampionCatalog = preload("res://scripts/champion_catalog.gd")

const PHASE_DRAFTING := 0
const PHASE_PREPARATION := 1
const PHASE_COMBAT := 2
const PHASE_MATCH_OVER := 3

const WORLD_SIZE := Vector2(10.0, 10.0)
const TICK_RATE := 0.1
const PREPARATION_DURATION := 1.0
const COMBAT_DURATION := 6.0
const DRAFT_SEQUENCE: Array[String] = [
	"P1_PICK",
	"P2_PICK",
	"P1_PICK",
	"P2_PICK",
	"P1_PICK",
	"P2_PICK",
	"P1_PICK",
	"P2_PICK",
	"P1_PICK",
	"P2_PICK",
]

@onready var draft_screen := $DraftScreen
@onready var battlefield := $Battlefield
@onready var phase_label: Label = $HUD/PhaseLabel
@onready var tick_label: Label = $HUD/TickLabel
@onready var selection_label: Label = $HUD/SelectionLabel
@onready var help_label: Label = $HUD/HelpLabel

var world_state
var _tick_accumulator: float = 0.0
var _rng: RandomNumberGenerator = RandomNumberGenerator.new()
var _match_running: bool = false
var player_picks: Array[String] = []
var enemy_picks: Array[String] = []
var banned_heroes: Array[String] = []
var draft_step_index: int = 0


func _ready() -> void:
	set_process(true)
	_rng.seed = 42
	world_state = WorldStateScript.new()
	world_state.tick_rate = TICK_RATE
	world_state.world_size = WORLD_SIZE
	battlefield.call("set_world_size", WORLD_SIZE)
	draft_screen.hero_selected.connect(_on_draft_hero_selected)
	draft_screen.random_draft_requested.connect(_on_random_draft_requested)
	draft_screen.start_match_requested.connect(_on_start_match_requested)
	battlefield.focus_changed.connect(_on_battlefield_focus_changed)
	world_state.phase_changed.connect(_on_phase_changed)
	_refresh_draft_screen()
	_sync_visibility()
	_refresh_ui()


func _process(delta: float) -> void:
	if not _match_running:
		_refresh_ui()
		return

	_tick_accumulator += max(delta, 0.0)
	while _tick_accumulator >= world_state.tick_rate:
		_tick_accumulator -= world_state.tick_rate
		world_state.advance_tick()
		battlefield.call("step_simulation", world_state.tick_rate)
		_step_simulation()

	_refresh_ui()


func _step_simulation() -> void:
	match int(world_state.phase):
		PHASE_PREPARATION:
			if world_state.time >= PREPARATION_DURATION:
				world_state.set_phase(PHASE_COMBAT)
		PHASE_COMBAT:
			if world_state.time >= COMBAT_DURATION:
				world_state.set_phase(PHASE_MATCH_OVER)
				_match_running = false
		PHASE_MATCH_OVER:
			_match_running = false
		PHASE_DRAFTING:
			pass


func _refresh_ui() -> void:
	phase_label.text = "PHASE: %s" % _phase_name(int(world_state.phase))
	tick_label.text = "TICK: %d  TIME: %.1fs" % [world_state.tick, world_state.time]
	selection_label.text = _selection_text()
	match int(world_state.phase):
		PHASE_DRAFTING:
			help_label.text = "Draft heroes, then press START MATCH."
		PHASE_PREPARATION:
			help_label.text = "Preparation phase: unit placement lands in the next step."
		PHASE_COMBAT:
			help_label.text = "Combat phase shell is active."
		PHASE_MATCH_OVER:
			help_label.text = "Match over."
		_:
			help_label.text = "Teamfight shell ready."


func _on_phase_changed(_new_phase: int) -> void:
	if int(world_state.phase) == PHASE_COMBAT:
		battlefield.call("capture_spawn_positions")
	battlefield.call("set_phase", int(world_state.phase))
	_sync_visibility()
	_refresh_ui()


func _sync_visibility() -> void:
	var drafting := int(world_state.phase) == PHASE_DRAFTING
	draft_screen.visible = drafting
	battlefield.visible = not drafting
	$HUD.visible = not drafting


func _on_draft_hero_selected(hero_id: String) -> void:
	if int(world_state.phase) != PHASE_DRAFTING:
		return
	_handle_draft_action(hero_id)


func _on_random_draft_requested() -> void:
	if int(world_state.phase) != PHASE_DRAFTING:
		return
	while draft_step_index < DRAFT_SEQUENCE.size():
		var available: Array[Dictionary] = ChampionCatalog.available_heroes(player_picks, enemy_picks, banned_heroes, [])
		if available.is_empty():
			break
		var hero: Dictionary = available[_rng.randi_range(0, available.size() - 1)]
		_handle_draft_action(String(hero.get("id", "")))


func _on_start_match_requested() -> void:
	if int(world_state.phase) != PHASE_DRAFTING:
		return
	if draft_step_index < DRAFT_SEQUENCE.size():
		return
	_begin_match()


func _handle_draft_action(hero_id: String) -> void:
	if draft_step_index >= DRAFT_SEQUENCE.size():
		return

	var step := DRAFT_SEQUENCE[draft_step_index]
	if step == "P1_PICK":
		player_picks.append(hero_id)
	elif step == "P2_PICK":
		enemy_picks.append(hero_id)
	draft_step_index += 1
	_refresh_draft_screen()

	if draft_step_index < DRAFT_SEQUENCE.size() and DRAFT_SEQUENCE[draft_step_index] == "P2_PICK":
		_run_ai_draft_step()


func _run_ai_draft_step() -> void:
	if draft_step_index >= DRAFT_SEQUENCE.size():
		return
	if DRAFT_SEQUENCE[draft_step_index] != "P2_PICK":
		return

	var available: Array[Dictionary] = ChampionCatalog.available_heroes(player_picks, enemy_picks, banned_heroes, [])
	var chosen: Dictionary = ChampionCatalog.choose_ai_pick(available, enemy_picks)
	if chosen.is_empty():
		return
	_handle_draft_action(String(chosen.get("id", "")))


func _begin_match() -> void:
	world_state.tick = 0
	world_state.time = 0.0
	_tick_accumulator = 0.0
	_match_running = true
	battlefield.call("load_rosters", player_picks, enemy_picks)
	battlefield.call("set_phase", PHASE_PREPARATION)
	world_state.set_phase(PHASE_PREPARATION)


func _refresh_draft_screen() -> void:
	draft_screen.set_draft_state(
		player_picks,
		enemy_picks,
		banned_heroes,
		draft_step_index,
		draft_screen.get_active_role_filters()
	)


func _phase_name(phase: int) -> String:
	match phase:
		PHASE_DRAFTING:
			return "DRAFTING"
		PHASE_PREPARATION:
			return "PREPARATION"
		PHASE_COMBAT:
			return "COMBAT"
		PHASE_MATCH_OVER:
			return "MATCH_OVER"
		_:
			return "UNKNOWN"


func _on_battlefield_focus_changed(text: String) -> void:
	selection_label.text = text


func _selection_text() -> String:
	if int(world_state.phase) == PHASE_DRAFTING:
		return "Selection: draft heroes, then start the match."
	return selection_label.text if selection_label.text != "" else "Selection: hover or drag a unit."
