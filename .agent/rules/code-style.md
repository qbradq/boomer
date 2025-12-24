---
trigger: always_on
glob:
description:
---
# C Code Style Guide

## Formatting
- 4-space indentation, 100-column limit.
- Braces on same line: `if (cond) {`.
- Spaces around operators: `x = y + 1;`.
- Use clang-format with `.clang-format-20` file.

## Naming Conventions
- Functions: `snake_case`.
- Macros: `UPPER_CASE`.
- Constants: `kebab_case` or `SNAKE_CASE`.
- Variables: `camelCase` for locals, `snake_case` for globals.

## Documentation
- Doxygen-style `/** */` comments on public functions.
- Always document parameters, returns, and C23-specific features.

## Best Practices
- Static analyzers: Enable all warnings (`-Wall -Wextra -Wpedantic`).
- Const-correctness everywhere possible.
- Prefer `enum` over `#define` for constants.
