#ifndef BOOMER_CORE_TYPES_H
#define BOOMER_CORE_TYPES_H

#include <stdint.h>
#include <stdbool.h>

// Fixed width integer types
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

// Floating point types
typedef float  f32;
typedef double f64;

// Math types
typedef struct {
    f32 x, y;
} Vec2;

typedef struct {
    f32 x, y, z;
} Vec3;

typedef struct {
    f32 x, y, z, w;
} Vec4;

#include "raylib.h"
// Color type is now provided by raylib.h

#endif // BOOMER_CORE_TYPES_H
