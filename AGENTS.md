# AGENTS.md

## Change Discipline

- Make the smallest complete change that resolves the request.
- Do not modify unrelated code.
- Do not introduce abstractions, refactors, or dependencies unless required for correctness.
- Treat new headers, libraries, or build system changes as dependencies and avoid unless required.
- Preserve existing behavior unless explicitly changed.
- Do not implement speculative changes.
- Ask questions when implementation details are unclear.
- List all behavior changes explicitly in the response.
- Ensure no unrelated modifications.
- Ensure checks pass.

## Execution Modes

- `/caveman ultra` is the default mode.
- Apply `/caveman ultra` automatically unless explicitly overridden.
- It does not override Change Discipline constraints.

## Output

- Be concise unless more detail is explicitly requested.
- When proposing changes, prefer minimal diffs over prose.
- No emojis.
- Explanations only when required.

## Codebase

Use the wiki before modifying code.

- Consult relevant wiki notes before modifying simulation logic.
- Code is the source of truth if it conflicts with wiki.
- Do not update wiki unless introducing or invalidating stable concepts.

**Native C++ simulation:**
- Read `wiki/README.md`
- Read `wiki/notes/native_agent_guide.md`
- Then `wiki/notes/simulation_module_map.md`

## Wiki

- notes → observations
- concepts → definitions
- projects → organization

## Done

- Minimal complete change applied.
- No unrelated modifications.
- Behavior changes explicitly listed.
- Checks pass.
- Update wiki