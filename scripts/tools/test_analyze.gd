extends SceneTree

func _init() -> void:
	call_deferred("_run")

func _run() -> void:
	print("test_analyze: STARTED")
	var input_path := "res://model_stats/draft_ab_test_200.csv"
	var global_path := ProjectSettings.globalize_path(input_path)
	print("test_analyze: global_path=%s" % global_path)
	
	var f := FileAccess.open(global_path, FileAccess.READ)
	if f == null:
		push_error("test_analyze: could not open file")
		quit(1)
		return
	
	var lines := f.get_as_text().split("\n")
	f.close()
	print("test_analyze: loaded %d lines" % lines.size())
	print("test_analyze: first line: %s" % lines[0])
	
	quit(0)
