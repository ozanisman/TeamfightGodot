extends SceneTree

const NativeClassName := "TeamfightSimulationCore"
const NativeExtensionPath := "res://teamfight_simulation_core.gdextension"
const HeadlessShutdownScript := preload("res://scripts/tools/headless_shutdown.gd")

func _init() -> void:
	call_deferred("_probe_native_load")

func _probe_native_load() -> void:
	var extension_path: String = ProjectSettings.globalize_path(NativeExtensionPath)
	var load_status: int = GDExtensionManager.load_extension(extension_path)
	if load_status != GDExtensionManager.LOAD_STATUS_OK and load_status != GDExtensionManager.LOAD_STATUS_ALREADY_LOADED:
		push_error("Failed to load %s (status %d)" % [NativeExtensionPath, load_status])
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	await process_frame
	if ClassDB.class_exists(NativeClassName):
		var instance: Variant = ClassDB.instantiate(NativeClassName)
		if instance != null:
			print("%s loaded" % NativeClassName)
			await HeadlessShutdownScript.teardown_extension_then_quit(self, 0)
			return

	push_error("%s is not registered" % NativeClassName)
	await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
