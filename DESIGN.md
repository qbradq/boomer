# Boomer Design Document

**Goal**: Architect, design, and implement Boomer, a 2.5D engine (Doom/Build
style) as a fun and expressive platform for making and sharing games.

## Core Decisions
- **Language**: C23
- **Compiler**: LLVM/Clang
- **Build System**: CMake
- **Target Platforms**: Windows, Linux, macOS, Web (Emscripten)
- **Version Control**: Git / GitHub

## Architecture
- **Engine Style**: 2.5D (Sector-based with portals, similar to Build)
- **Philosophy**: Fun and expressive platform.

## World Representation
- **Structure**: Portal-Sector (Build-like) world representation.
- **Verticality**: Room-over-room (ROR) support via portals.

## Coordinate System
- **Units**: 1 unit equals 1 pixel.
- **axes**: The positive Z axis points up.
- **Math**: Floating point, 32-bit precision.

## Rendering
- **Technique**: Software column/span drawing. Direct pixel access.
- **Perspective**: 2.5D (Projected).

## Entities & Logic
- **Representation**: Entity Component System (ECS).
- **Scripting**: QuickJS.

## Resource Management
- **Format**: Standard .zip file archive for distribution, directory on disk for
  development.
- **Loading**: Lazy loading.
