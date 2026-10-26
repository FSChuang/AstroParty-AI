#pragma once

#include "astroparty/core/MapDefinition.hpp"
#include "astroparty/core/World.hpp"
#include "astroparty/entities/Bullet.hpp"
#include "astroparty/entities/Resource.hpp"
#include "astroparty/entities/Ship.hpp"
#include "astroparty/rendering/SdlRenderer.hpp"

namespace astroparty::rendering {

// Draws a World's current state using simple SDL2 primitives (lines and
// filled rectangles) -- no textures, per the Phase 2 instruction to avoid
// spending time on art for Milestone 1. This draws whatever the World
// actually contains: obstacles, home regions, and resources only appear
// when a loaded map defines them, so Classic Deathmatch's plain two-ship
// arena renders exactly as it always has.
//
// GameRenderer only reads World through its const accessors and never
// mutates simulation state: rendering is a one-way consumer of the
// simulation, never a participant in it. This is the architectural fix for
// the rendering/simulation coupling described in
// docs/UPSTREAM_AUDIT.md (item C-7), where
// upstream's Player and Bullet structs directly embedded an SDL_Texture.
class GameRenderer {
public:
    explicit GameRenderer(SdlRenderer& renderer) noexcept;

    void renderFrame(const core::World& world);

private:
    SdlRenderer& renderer_;

    void clearBackground();
    void drawShip(const entities::Ship& ship, float collisionRadius, int colorIndex) const;
    void drawBullet(const entities::Bullet& bullet) const;
    void drawObstacle(const core::ObstacleDefinition& obstacle) const;
    void drawHomeRegion(const core::HomeRegion& home) const;
    void drawAvailableResource(const entities::Resource& resource) const;
    void drawCarryingRing(core::Vector2 shipPosition, float shipCollisionRadius) const;
};

}  // namespace astroparty::rendering
