# Wiki Guidance

This project uses a lightweight wiki as shared memory.

## Purpose
The wiki contains evolving understanding of the project:
- notes → atomic observations and ideas
- concepts → stable definitions of systems
- projects → high-level system organization

## Usage

Use the wiki to understand existing ideas before working on the codebase.

**Native C++ simulation (refactored layout):** read [`wiki/notes/native_agent_guide.md`](wiki/notes/native_agent_guide.md) first, then [`wiki/notes/simulation_module_map.md`](wiki/notes/simulation_module_map.md). Do not use `wiki/projects/core-refactor-2026-05/` for current file paths.

Prefer:
- reusing existing concepts
- extending existing ideas
- updating understanding through new notes

Avoid:
- duplicating ideas already represented in the wiki
- introducing conflicting terminology without reflection

## Update Behavior

When new insights emerge:
- add them to wiki/notes
- if they stabilize over time, promote them to wiki/concepts