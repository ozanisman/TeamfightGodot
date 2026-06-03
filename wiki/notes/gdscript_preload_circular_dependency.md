# GDScript Preload Circular Dependency Issue

## Problem

When using `preload()` to load scripts that have circular dependencies, Godot's parser fails with "Could not preload resource script" errors. This occurs because `preload()` runs at parse time, and when script A preloads script B which preloads script A, the compiler enters a deadlock.

## Specific Case

In the satellite tooltip framework:
- `satellite_tooltip_manager.gd` preloaded `satellite_registries.gd`
- `satellite_registries.gd` preloaded `satellite_context.gd` and used `SatelliteContext` as a type annotation
- `SatelliteContext` was only loaded via preload (not via `class_name`), so Godot's parser couldn't resolve the type annotation at parse time
- This caused a parse error in `satellite_registries.gd`, preventing it from being preloaded

## Solution

Use `load()` instead of `preload()` for scripts that create circular dependencies. `load()` runs at runtime, not at parse time, breaking the parse-time dependency chain.

```gdscript
# Before (causes parse error)
const SatelliteRegistriesScript := preload("res://scripts/app/satellite_registries.gd")

# After (works)
var SatelliteRegistriesScript = null

func _ready() -> void:
    if SatelliteRegistriesScript == null:
        SatelliteRegistriesScript = load("res://scripts/app/satellite_registries.gd")
```

## Best Practices

- Use `preload()` for small, frequently used resources that don't create circular dependencies
- Use `load()` for scripts or resources that are part of a dependency cycle
- Load at runtime in `_ready()` or initialization methods, not at parse time
- This is the standard solution for circular dependency issues in Godot 4

## References

- Bugnet article: "Fix: Godot preload() vs load() — When Each Causes Errors"
- GitHub issue #90362: Regression due to GDScript cache changes for preload
- GitHub issue #70985: Circular dependencies of scenes (by preload)
