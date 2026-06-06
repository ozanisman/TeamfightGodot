extends Node

func _ready() -> void:
	print("SIMPLE TEST START")
	var core := TeamfightSimulationCore.new()
	print("Core created")
	
	print("Calling run_stress_test...")
	var available := ["archer", "berserker", "cleric"]
	var report = core.run_stress_test(
		[],
		[],
		available,
		"res://stats_output",
		5,
		0.50,
		0.25,
		0.25,
		1.2,
		1.2
	)
	
	print("Report received")
	print("Report type: %s" % str(typeof(report)))
	print("Baseline top1: %s" % report.get("baseline_top1", ""))
	print("Score mean: %.6f" % report.get("score_mean", 0.0))
	
	get_tree().quit()
