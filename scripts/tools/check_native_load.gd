extends SceneTree

const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const NativeExtensionPath := "res://teamfight_simulation_core.gdextension"
const NativeClassName := "TeamfightSimulationCore"
const HeadlessShutdownScript := preload("res://scripts/tools/headless_shutdown.gd")

func _init() -> void:
	call_deferred("_probe_native_load")

func _probe_native_load() -> void:
	if not NativeSimulationBackendScript.ensure_gdextension_loaded():
		push_error("Failed to load %s" % NativeExtensionPath)
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
