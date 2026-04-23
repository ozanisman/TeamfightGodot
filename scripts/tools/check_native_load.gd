extends SceneTree

const NativeClassName := "TeamfightSimulationCore"
const NativeExtensionPath := "res://teamfight_simulation_core.gdextension"

func _init() -> void:
	call_deferred("_probe_native_load")

func _probe_native_load() -> void:
	var extension_path: String = ProjectSettings.globalize_path(NativeExtensionPath)
	var load_status: int = GDExtensionManager.load_extension(extension_path)
	if load_status != GDExtensionManager.LOAD_STATUS_OK and load_status != GDExtensionManager.LOAD_STATUS_ALREADY_LOADED:
		push_error("Failed to load %s (status %d)" % [NativeExtensionPath, load_status])
		quit(1)
		return

	await process_frame
	if ClassDB.class_exists(NativeClassName):
		var instance: Variant = ClassDB.instantiate(NativeClassName)
		if instance != null:
			print("%s loaded" % NativeClassName)
			quit(0)
			return

	push_error("%s is not registered" % NativeClassName)
	quit(1)
