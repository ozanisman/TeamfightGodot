extends RefCounted

func _init():
	print("=== Simple Test Started ===")
	print("Testing basic functionality...")
	
	# Load native extension
	var load_status = GDExtensionManager.load_extension("res://teamfight_simulation_core.gdextension")
	print("Extension load status:", load_status)
	
	var core = TeamfightSimulationCore.new()
	print("Core created:", core != null)
	
	print("=== Simple Test Complete ===")
	get_tree().quit()
