# Boomer Design Document

**Goal**: Architect, design, and implement Boomer, a 2.5D engine (Doom/Build style) as a fun and expressive platform for making and sharing games.

## Core Decisions
- **Language**: C23
- **Compiler**: LLVM/Clang
- **Build System**: CMake (replace Makefile)
- **Target Platforms**: Windows, Linux, macOS, Web (Emscripten)
- **Version Control**: Git / GitHub

## Dependencies
- **Windowing/Input**: SDL2
- **Image Loading**: `stb_image`
- **Audio Loading**: `stb_vorbis`
- **Text Rendering**: `stb_truetype`
- **Noise Generation**: `stb_perlin`

## Architecture
- **Engine Style**: 2.5D (Sector-based, similar to Doom/Build)
- **Philosophy**: Fun and expressive platform.

## World Representation (TBD)
- **Structure**: How do we represent the world?
    - *Options*: Vertex-Linedef-Sector (Doom), Portal-Sector (Build), true 3D
      meshes?
    - Answer: Portal-Sector (Build).
- **Verticality**: Do we support room-over-room (ROR)?
    - *Doom*: No (mostly). *Build*: Yes (via portals).
    - Answer: Yes (portals).

## Coordinate System (TBD)
- **Units**: What is 1.0?
  - Answer: 1.0 = 1 pixel. 
- **axes**: Y-up or Z-up?
  - Answer: Z-up.
- **Math**: Fixed point (retro feel/determinism) or Floating point (simplicity/modern)?
  - Answer: Floating point.

## Rendering (TBD)
- **Technique**:
    - *Software*: Classic retro feel, column/span drawing. Direct pixel access.
    - *Hardware (OpenGL/Vulkan)*: Modern performance, can emulate retro look.
    - Answer: Software
- **Perspective**: 2.5D (Projected) vs True 3D.
    - Answer: 2.5D (Projected).

## Entities & Logic (TBD)
- **Representation**: Structs with function pointers? Entity Component System
  (ECS)?
    - Answer: Entity Component System (ECS).
- **Scripting**: Hardcoded C? Lua? Custom C-like script?
    - Answer: Lua 5.4.

## Resource Management (TBD)
- **Format**: Directory on disk (dev friendly) vs Archives (WAD/GRP/PAK).
    - Answer: Standard .zip file archive for distribution, directory on disk for development.
- **Loading**: Lazy loading vs Eager loading?
    - Answer: Lazy loading.

