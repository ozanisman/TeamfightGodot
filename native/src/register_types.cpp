#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>

#include "teamfight_simulation_core.hpp"

using namespace godot;

static void initialize_teamfight_module(ModuleInitializationLevel level) {
	if (level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	GDREGISTER_CLASS(TeamfightSimulationCore);
}

static void uninitialize_teamfight_module(ModuleInitializationLevel level) {
	if (level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}

extern "C" {

GDExtensionBool GDE_EXPORT teamfight_simulation_core_library_init(
	GDExtensionInterfaceGetProcAddress p_get_proc_address,
	GDExtensionClassLibraryPtr p_library,
	GDExtensionInitialization *r_initialization
) {
	GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
	init_obj.register_initializer(initialize_teamfight_module);
	init_obj.register_terminator(uninitialize_teamfight_module);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
	return init_obj.init();
}

}
