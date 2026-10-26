#pragma once

#include <cmath>

namespace astroparty::core {

// A 2D point or displacement in simulation space.
//
// Coordinate convention: the origin is the top-left of the arena and Y
// increases downward, matching SDL's window coordinate system. This lets
// positions flow between the simulation and the renderer with no conversion.
struct Vector2 {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Vector2& operator+=(Vector2 other) noexcept {
        x += other.x;
        y += other.y;
        return *this;
    }

    constexpr Vector2& operator-=(Vector2 other) noexcept {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    [[nodiscard]] constexpr float lengthSquared() const noexcept { return x * x + y * y; }

    [[nodiscard]] float length() const noexcept { return std::sqrt(lengthSquared()); }

    // Returns this vector scaled down so its length does not exceed maxLength.
    // A vector already shorter than maxLength is returned unchanged. The zero
    // vector is returned unchanged rather than risking a divide-by-zero.
    [[nodiscard]] Vector2 clampedToLength(float maxLength) const noexcept;

    // Returns the unit vector for a heading expressed in degrees.
    //
    // Convention: 0 degrees points "up" on screen (negative Y) and angles
    // increase clockwise. This matches upstream CAstroParty's
    // Vector_direction() (astro-party-workspace/audit/03_code_architecture.md,
    // section 4), which offsets by -90 degrees before taking cos/sin for the
    // same reason: it makes 0 degrees a natural "facing up" default for a
    // ship sprite under SDL's Y-down coordinate system.
    [[nodiscard]] static Vector2 fromAngleDegrees(float degrees) noexcept;
};

[[nodiscard]] constexpr Vector2 operator+(Vector2 lhs, Vector2 rhs) noexcept {
    return Vector2{lhs.x + rhs.x, lhs.y + rhs.y};
}

[[nodiscard]] constexpr Vector2 operator-(Vector2 lhs, Vector2 rhs) noexcept {
    return Vector2{lhs.x - rhs.x, lhs.y - rhs.y};
}

[[nodiscard]] constexpr Vector2 operator*(Vector2 vector, float scalar) noexcept {
    return Vector2{vector.x * scalar, vector.y * scalar};
}

[[nodiscard]] constexpr bool operator==(Vector2 lhs, Vector2 rhs) noexcept {
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

[[nodiscard]] constexpr bool operator!=(Vector2 lhs, Vector2 rhs) noexcept { return !(lhs == rhs); }

[[nodiscard]] inline float distanceSquared(Vector2 a, Vector2 b) noexcept {
    return (a - b).lengthSquared();
}

}  // namespace astroparty::core
