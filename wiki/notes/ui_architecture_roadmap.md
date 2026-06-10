# UI architecture roadmap

Code remains the source of truth. This note records the current UI architecture direction so UI cleanup can proceed incrementally without introducing a parallel framework.

## Good now

- Use `res://assets/theme/theme.tres` for shared default Godot control styling.
- Use `res://scripts/ui/ui_tokens.gd` for stable colors, spacing, font sizes, and draft layout sizes.
- Use small reusable component scenes when they own structure or behavior, starting with draft champion tiles.
- Use shared layout helpers for repeated draft-screen positioning between `draft_testing.gd` and `simulation_viewer.gd`.
- Prefer Godot containers and anchors over manual per-control coordinate updates.

## Maybe later

- Add a `scene_manager.gd` only if routing grows beyond simple `change_scene_to_file()` calls.
- Consolidate tooltip ownership once draft, roster, and stats tooltip behavior stabilizes.
- Move scenes into `scenes/screens/` and `scenes/components/` after screen paths stop changing frequently.
- Convert more procedural UI into scene-backed components as behavior boundaries become clear.

## Avoid for now

- Do not add an external UI library for the current positioning and consistency work.
- Do not create generic `ui_button.tscn` or `ui_panel.tscn` wrappers for style alone; use the global theme instead.
- Do not do broad folder moves before draft layout and component ownership are stable.
- Do not refactor combat HUD, world rendering, projectiles, or AoE as part of draft layout cleanup.
