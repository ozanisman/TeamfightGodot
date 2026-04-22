extends CombatWorld
class_name CombatSimulationCore

const SimulationContractScript = preload("res://scripts/simulation_contract.gd")


func snapshot_units() -> Array[Dictionary]:
	var snapshots: Array[Dictionary] = []
	for unit in units:
		if is_instance_valid(unit):
			snapshots.append(SimulationContractScript.snapshot_unit(unit))
	return snapshots
