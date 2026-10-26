#include "astroparty/physics/Arena.hpp"

#include <algorithm>
#include <cassert>

namespace astroparty::physics {

core::Vector2 clampToArena(core::Vector2 position, core::Vector2 arenaSize, float margin) noexcept {
    assert(arenaSize.x > 2.0f * margin && arenaSize.y > 2.0f * margin);

    const float minX = margin;
    const float maxX = arenaSize.x - margin;
    const float minY = margin;
    const float maxY = arenaSize.y - margin;

    return core::Vector2{
        std::clamp(position.x, minX, maxX),
        std::clamp(position.y, minY, maxY),
    };
}

}  // namespace astroparty::physics
