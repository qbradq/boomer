#ifndef BOOMER_CORE_MATH_UTILS_H
#define BOOMER_CORE_MATH_UTILS_H

#include "types.h"
#include <math.h>

#ifndef PI
#define PI 3.14159265359f
#endif
#define TAU 6.28318530718f
#ifndef DEG2RAD
#define DEG2RAD(d) ((d) * (PI / 180.0f))
#endif
#ifndef RAD2DEG
#define RAD2DEG(r) ((r) * (180.0f / PI))
#endif

// Vec2 Operations
static inline Vec2 vec2_add(Vec2 a, Vec2 b) { return (Vec2){a.x + b.x, a.y + b.y}; }
static inline Vec2 vec2_sub(Vec2 a, Vec2 b) { return (Vec2){a.x - b.x, a.y - b.y}; }
static inline Vec2 vec2_mul(Vec2 v, f32 s)  { return (Vec2){v.x * s, v.y * s}; }
static inline f32  vec2_dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
static inline f32  vec2_len(Vec2 v)         { return sqrtf(v.x * v.x + v.y * v.y); }
static inline Vec2 vec2_norm(Vec2 v) {
    f32 len = vec2_len(v);
    return len > 0 ? vec2_mul(v, 1.0f / len) : (Vec2){0};
}
static inline Vec2 vec2_lerp(Vec2 a, Vec2 b, f32 t) {
    return (Vec2){a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

// Vec3 Operations
static inline Vec3 vec3_add(Vec3 a, Vec3 b) { return (Vec3){a.x + b.x, a.y + b.y, a.z + b.z}; }
static inline Vec3 vec3_sub(Vec3 a, Vec3 b) { return (Vec3){a.x - b.x, a.y - b.y, a.z - b.z}; }
static inline Vec3 vec3_mul(Vec3 v, f32 s)  { return (Vec3){v.x * s, v.y * s, v.z * s}; }
static inline f32  vec3_dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline Vec3 vec3_cross(Vec3 a, Vec3 b) {
    return (Vec3){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}
static inline f32 vec3_len(Vec3 v) { return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z); }
static inline Vec3 vec3_norm(Vec3 v) {
    f32 len = vec3_len(v);
    return len > 0 ? vec3_mul(v, 1.0f / len) : (Vec3){0};
}
static inline Vec3 vec3_lerp(Vec3 a, Vec3 b, f32 t) {
    return (Vec3){a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
}

#endif // BOOMER_CORE_MATH_UTILS_H
