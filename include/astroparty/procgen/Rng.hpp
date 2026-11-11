#pragma once

#include <cstdint>
#include <random>

#include "astroparty/core/Vector2.hpp"

namespace astroparty::procgen {

// Owns one deterministic random stream for procedural map generation.
//
// This is the only source of randomness anywhere in Milestone 4's
// procedural generator, and it is explicitly, locally owned by whoever
// constructs it -- there is no global/static RNG state anywhere in this
// project. Given the same seed, the same sequence of calls on this object
// always produces the same sequence of results (on the same platform/build,
// the same determinism caveat the rest of this project's simulation already
// documents), which is what makes "same config + same seed -> same map" a
// guarantee rather than a hope.
//
// Deliberately NOT used anywhere inside astroparty_sim's actual simulation
// (World, Ship, Bullet, ...): generation-time randomness stays confined to
// procgen/, and the maps it produces are then consumed by the simulation
// exactly like any hand-authored MapDefinition, with no randomness at all
// once a match starts.
class Rng {
public:
    explicit Rng(std::uint32_t seed) noexcept : engine_(seed) {}

    // Returns a float uniformly distributed in [min, max).
    [[nodiscard]] float uniformFloat(float min, float max) noexcept {
        std::uniform_real_distribution<float> distribution(min, max);
        return distribution(engine_);
    }

    // Returns an int uniformly distributed in [min, max] (inclusive).
    [[nodiscard]] int uniformInt(int min, int max) noexcept {
        std::uniform_int_distribution<int> distribution(min, max);
        return distribution(engine_);
    }

    // Returns a point uniformly distributed inside the axis-aligned
    // rectangle [minCorner, maxCorner].
    [[nodiscard]] core::Vector2 uniformInRectangle(core::Vector2 minCorner, core::Vector2 maxCorner) noexcept {
        return core::Vector2{uniformFloat(minCorner.x, maxCorner.x), uniformFloat(minCorner.y, maxCorner.y)};
    }

    // Returns an offset from the origin whose two components are each drawn
    // from a normal distribution with the given standard deviation -- used
    // to scatter obstacles around a cluster center with most of them close
    // to it and progressively fewer further away, rather than uniformly
    // filling a hard-edged disk.
    [[nodiscard]] core::Vector2 gaussianOffset(float standardDeviation) noexcept {
        std::normal_distribution<float> distribution(0.0f, standardDeviation);
        return core::Vector2{distribution(engine_), distribution(engine_)};
    }

private:
    std::mt19937 engine_;
};

}  // namespace astroparty::procgen
