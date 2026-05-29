# Core refactor (May 2026)

> **Agents:** do **not** use this folder for current file paths or edit routing. Use [native_agent_guide.md](../../notes/native_agent_guide.md).

Historical exit snapshots for the native `TeamfightSimulationCore` modularization (`core-refactor` branch). Line counts and module names reflect the tree **at each iteration end** and may reference removed files (e.g. `sim_damage.cpp`, monolithic `sim_periodic.cpp`).

**Current layout:** [simulation_module_map.md](../../notes/simulation_module_map.md)  
**Perf / merge / validation:** [performance_optimization_status.md](../../notes/performance_optimization_status.md)

## Iterations

| Doc | Focus |
|-----|--------|
| [simulation_refactor_iteration_3.md](simulation_refactor_iteration_3.md) | Initial coordinator slim-down plan |
| [simulation_refactor_iteration_4.md](simulation_refactor_iteration_4.md) | Viewer, benchmark extraction |
| [simulation_refactor_iteration_5.md](simulation_refactor_iteration_5.md) | Effects compile split |
| [simulation_refactor_iteration_6.md](simulation_refactor_iteration_6.md) | Targeting, profile, benchmark host |
| [simulation_refactor_iteration_7.md](simulation_refactor_iteration_7.md) | Targeting select, periodic split |
| [simulation_refactor_iteration_8.md](simulation_refactor_iteration_8.md) | Unit tick, combat split |
| [simulation_refactor_iteration_9.md](simulation_refactor_iteration_9.md) | Coordinator glue, damage split |
| [simulation_refactor_iteration_10.md](simulation_refactor_iteration_10.md) | Encapsulation, exec status split |
| [simulation_refactor_iteration_11.md](simulation_refactor_iteration_11.md) | Compile opcodes, header slim (final structural iter) |

Read in order via the “Continuation after …” links at the top of each file.
