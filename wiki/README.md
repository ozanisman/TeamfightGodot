# Wiki

A living knowledge base.

## Agents (read this first for native C++)

**[native_agent_guide.md](notes/native_agent_guide.md)** — canonical routing, task→file map, invariants, denylists, validation commands.  
Then [simulation_module_map.md](notes/simulation_module_map.md).

## Structure

- **notes** — atomic observations, module maps, perf status
- **concepts** — stable definitions of systems
- **projects** — system-level groupings and iteration exit snapshots

## Native simulation (current)

| Doc | Purpose |
|-----|---------|
| [native_agent_guide.md](notes/native_agent_guide.md) | **Agent entry** — routing, invariants, do-not-read list |
| [simulation_module_map.md](notes/simulation_module_map.md) | Module ownership, coordinator split, where to add features |
| [performance_optimization_status.md](notes/performance_optimization_status.md) | Dated benchmark + validation gate |
| [algorithmic_optimization_analysis.md](notes/algorithmic_optimization_analysis.md) | Deferred perf ideas (profiling backlog) |
| [ui_architecture_roadmap.md](notes/ui_architecture_roadmap.md) | UI theme/component direction, deferred ideas, and current avoid-list |

**Coordinator:** [`native/src/teamfight_simulation_core.{hpp,cpp}`](../native/src/teamfight_simulation_core.hpp) (~270 / ~293 lines) plus `sim_coordinator_{match,state,catalog,targeting,viewer,tick,bindings,host}.cpp`. Shared host API: [`sim_coordinator_host.hpp`](../native/src/simulation/sim_coordinator_host.hpp) only (no per-TU empty coordinator headers).

## Principles

- Write ideas down as they emerge
- Keep files small and focused
- Merge and refine over time
- Prefer linking over hierarchy
