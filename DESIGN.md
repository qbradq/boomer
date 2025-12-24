# Boomer Design Document

**Goal**: Architect, design, and implement Boomer, a 2.5D engine (Doom/Build
style) as a fun and expressive platform for making and sharing games.

## Core Decisions
- **Language**: C23
- **Compiler**: LLVM/Clang
- **Build System**: CMake
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
- **Structure**: Portal-Sector (Build-like) world representation.
- **Verticality**: Room-over-room (ROR) support via portals.

## Coordinate System (TBD)
- **Units**: 1 unit equals 64 pixels.
- **axes**: The positive Z axis points up.
- **Math**: Floating point, 32-bit precision.

## Rendering (TBD)
- **Technique**: Software column/span drawing. Direct pixel access.
- **Perspective**: 2.5D (Projected).

## Entities & Logic (TBD)
- **Representation**: Entity Component System (ECS).
- **Scripting**: Lua 5.4.

## Resource Management (TBD)
- **Format**: Standard .zip file archive for distribution, directory on disk for
  development.
- **Loading**: Lazy loading.

