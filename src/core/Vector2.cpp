#include "astroparty/core/Vector2.hpp"

namespace astroparty::core {

Vector2 Vector2::clampedToLength(float maxLength) const noexcept {
    const float currentLength = length();
    if (currentLength <= maxLength || currentLength == 0.0f) {
        return *this;
    }
    return *this * (maxLength / currentLength);
}

Vector2 Vector2::fromAngleDegrees(float degrees) noexcept {
    constexpr float kDegreesToRadians = 3.14159265358979323846f / 180.0f;
    const float radians = (degrees - 90.0f) * kDegreesToRadians;
    return Vector2{std::cos(radians), std::sin(radians)};
}

}  // namespace astroparty::core
