#pragma once
#include <algorithm>
#include <cmath>

namespace FovModel {
constexpr float DefaultConstant = 0.01745329238474369f;
constexpr float MinScale = 0.5f;
constexpr float MaxScale = 2.0f;

inline float Clamp(float scale) {
    return std::isfinite(scale) ? std::clamp(scale, MinScale, MaxScale) : 1.0f;
}
inline float Value(float original, float scale, bool enabled) {
    return enabled ? original * Clamp(scale) : original;
}
}
