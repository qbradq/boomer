# Boomer Engine Context

## Project Overview

Boomer is a Build-like, software-rendered 2.5D game engine designed for web and
desktop deployment. It utilizes a portal-sector world representation and an
Entity Component System (ECS) for game logic.

- **Engine Style:** 2.5D Portal-Sector (Build-style), Room-over-room support.
- **Rendering:** Software column/span drawing, direct pixel access.
- **Scripting:** QuickJS (JavaScript) for game entities.
- **Target Platforms:** Linux, Windows, macOS, Web (Emscripten).

## Key Technologies

- **Language:** C23 (Strict adherence).
- **Build System:** CMake 3.14+ with Ninja.
- **Libraries:**
  - `raylib` (5.5) - Platform layer, windowing, audio, input.
  - `quickjs` - Embedded JavaScript engine.
  - `miniz` - Zip archive handling.

## Build and Run Instructions

### Development Build (Debug + Sanitizers)

Use the `dev` preset for day-to-day development. This includes address and
undefined behavior sanitizers.

```bash
cmake --preset=dev
cmake --build --preset=dev
```

### Running the Engine

The engine defaults to loading the `games/demo` content if no path is provided.

```bash
./build/dev/boomer                # Run with default demo content
./build/dev/boomer games/my_game  # Run with specific game directory
./build/dev/boomer my_game.pak    # Run with specific game directory loaded from zip file
```

### Release Build

```bash
cmake --preset=release
cmake --build --preset=release
```

### Linting

```bash
cmake --build --preset=lint
```

## Development Standards (C23)

### Required Practices

- **Standard:** Always use `-std=c23`.
- **Types:**
  - Use `bool`, `true`, `false` from `<stdbool.h>`.
  - Use `nullptr` and `nullptr_t` from `<stddef.h>`.
  - Use fixed-width integers (`uint8_t`, `int32_t`, etc.) from `<stdint.h>`.
- **Attributes:** Apply `[[likely]]` and `[[unlikely]]` on critical branches.
- **Safe Arithmetic:** Use `stdckd_*` functions where overflow is possible.
- **Binary Data:** Use `#embed` for including binary assets if supported/needed.

### Formatting & Style

- **Indentation:** 4 spaces.
- **Column Limit:** 80 characters.
- **Braces:** K&R style (on the same line).
- **Naming:**
  - Functions: `snake_case` (e.g., `world_load_map`).
  - Variables: `snake_case` (e.g., `player_health`).
  - Macros: `UPPER_CASE` (e.g., `MAX_ENTITIES`).
  - Types: `snake_case` with `_t` suffix (e.g., `entity_t`) or PascalCase for
    structs if preferred by local convention (check surrounding code).
- **Comments:** Doxygen-style `/** ... */` for public API.

## Architecture Notes

- **World (`src/world/`):** Handles the sector/portal map data.
- **Entities (`src/game/`):** ECS implementation.
- **Renderer (`src/render/`):** Software rasterizer implementation.
- **Scripting (`src/core/script_sys.c`):** Bridges C game state with QuickJS.
- **Coordinate System:** 1 unit = 1 pixel. Positive Z points UP.

## Directory Structure

- `src/` - Engine source code.
- `games/` - Game content (maps, scripts, assets).
- `build/` - CMake build artifacts.
- `.gemini/` - Agent context and configuration.

# Development Process

Strictly stick to this development plan for each change that will change any
file.

1. Plan the changes without writing any code.
   1. Write this plan to `PLAN.md`.
   2. Include a explanation of the proposed changes.
   3. Include a list of all modified and new files.
2. Prompt the user to review the plan.
3. **STOP RUNNING THE PROMPT NOW! WAIT FOR USER INPUT**
4. If the user has requested changes to the plan, make those changes and loop
   back to step 2.
5. If the user approves the plan and asks for it to be implemented or executed,
   execute the plan.
