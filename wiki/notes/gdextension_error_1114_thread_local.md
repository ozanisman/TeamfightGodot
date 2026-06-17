# GDExtension Error 1114 — namespace-scope `thread_local` Godot types

> **RESOLVED** — moved cache to function-local `thread_local` accessor.

## Symptom

Godot fails to load the native core on startup:

```text
ERROR: Can't open dynamic library: .../native/bin/teamfight_simulation_core.dll.
       Error: Error 1114: A dynamic link library (DLL) initialization routine failed..
ERROR: GDExtension dynamic library not found: 'res://teamfight_simulation_core.gdextension'.
ERROR: Error loading extension: 'res://teamfight_simulation_core.gdextension'.
```

The DLL builds and links fine, deps are standard release CRT (`MSVCP140`, `VCRUNTIME140`, `api-ms-win-crt-*`), and the godot-cpp version matches the editor. The failure is at load, not link.

## Root cause

A **namespace-scope `thread_local`** object holding Godot types (`String` / `StringName` / `Dictionary` / `Array`, or a struct containing them) was declared in `teamfight_simulation_core.cpp`:

```cpp
static thread_local String s_draft_ai_cached_stats_dir;
static thread_local sim::draft_ai::DraftStatsDatabase s_draft_ai_cached_database;
static thread_local sim::catalog::CatalogState s_draft_ai_cached_catalog;
```

Windows runs dynamic initializers for namespace-scope `thread_local` variables via **TLS callbacks inside `DllMain`** — i.e. *before* the `entry_symbol` (`teamfight_simulation_core_library_init`) is called. Default-constructing a Godot type at that point calls into the GDExtension interface, which is still null. The access violation makes `DllMain` return false → **Error 1114**.

Confirmed by godot-cpp behavior in [godotengine/godot#59688](https://github.com/godotengine/godot/issues/59688): creating `Object`/`String`/`StringName` requires the API to be loaded, which only happens at `entry_symbol`; statics initialize before that.

Note: this is distinct from `Error 126` (missing dependency DLL), which is a separate failure mode.

## Fix

Use **function-local** `static thread_local` (lazy init on first call, after the extension is loaded) behind an accessor, instead of namespace-scope:

```cpp
struct DraftAiThreadCache {
    String cached_stats_dir;
    sim::draft_ai::DraftStatsDatabase cached_database;
    sim::catalog::CatalogState cached_catalog;
    bool cache_loaded = false;
};

DraftAiThreadCache &draft_ai_thread_cache() {
    static thread_local DraftAiThreadCache cache; // initializes on first use, post-load
    return cache;
}
```

## Rule

Never declare namespace-scope / file-scope `static` or `thread_local` objects of Godot types (or structs containing them) in the native core. Use function-local statics or pointers initialized in `register`/first-use. Trivial types (`bool`, `float`, `int`, `std::atomic<int>`) are safe.
