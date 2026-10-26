#include "astroparty/physics/Collision.hpp"

namespace astroparty::physics {

bool isPointWithinRadius(core::Vector2 point, core::Vector2 center, float radius) noexcept {
    return core::distanceSquared(point, center) < radius * radius;
}

}  // namespace astroparty::physics
