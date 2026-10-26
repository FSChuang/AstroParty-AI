#pragma once

#include "astroparty/core/World.hpp"
#include "astroparty/entities/Bullet.hpp"
#include "astroparty/entities/Ship.hpp"
#include "astroparty/rendering/SdlRenderer.hpp"

namespace astroparty::rendering {

// Draws a World's current state using simple SDL2 primitives (lines and
// filled rectangles) -- no textures, per the Phase 2 instruction to avoid
// spending time on art for Milestone 1.
//
// GameRenderer only reads World through its const accessors and never
// mutates simulation state: rendering is a one-way consumer of the
// simulation, never a participant in it. This is the architectural fix for
// the rendering/simulation coupling described in
// astro-party-workspace/audit/04_issues_and_risks.md (item C-7), where
// upstream's Player and Bullet structs directly embedded an SDL_Texture.
class GameRenderer {
public:
    explicit GameRenderer(SdlRenderer& renderer) noexcept;

    void renderFrame(const core::World& world);

private:
    SdlRenderer& renderer_;

    void clearBackground();
    void drawShip(const entities::Ship& ship, float collisionRadius) const;
    void drawBullet(const entities::Bullet& bullet) const;
};

}  // namespace astroparty::rendering
