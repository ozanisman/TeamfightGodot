extends SceneTree

## Minimal test to check if CSV loading works

func _init() -> void:
	call_deferred("_run")

func _run() -> void:
	print("Testing CSV loading...")

	var combat_path: String = "res://stats_output_baseline/combat_stats.csv"
	var abs_path: String = ProjectSettings.globalize_path(combat_path)
	print("Absolute path: %s" % abs_path)

	var f := FileAccess.open(abs_path, FileAccess.READ)
	if f == null:
		print("ERROR: Failed to open file")
		quit(1)
		return

	var header: PackedStringArray = f.get_csv_line()
	print("Header: %s" % str(header))

	var line_count := 0
	while not f.eof_reached():
		var line: PackedStringArray = f.get_csv_line()
		if line.size() > 1:
			line_count += 1
			if line_count <= 3:
				print("Line %d: %s" % [line_count, str(line)])

	f.close()
	print("Total lines: %d" % line_count)

	quit(0)
