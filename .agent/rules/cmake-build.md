---
trigger: always_on
glob:
description:
---
# CMake Build Workflow for C23 Projects

## Always Follow These Steps
- Use `cmake --build --preset=dev` to build the project.
- Use `cmake --build --preset=lint` to lint the project.
- Use `./build/dev/boomer` to run the project.

## Key Configurations
- Set `CMAKE_C_STANDARD=23` and `CMAKE_C_STANDARD_REQUIRED=ON`.
- Enable sanitizers: `-fsanitize=address,undefined` for dev preset.
- Use Ninja generator: `cmake -G Ninja`.
- Never modify CMakeLists.txt without generating a diff and plan first.

## Permissions
- Allow: `cmake`, `ninja`, `ctest`, `make`, `clang-tidy-20`, `cppcheck`.
- Ask before: Installing dependencies or modifying presets.
