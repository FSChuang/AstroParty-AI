#include "astroparty/rendering/GameRenderer.hpp"

#include <array>
#include <cstddef>

#include "astroparty/core/Vector2.hpp"
#include "astroparty/physics/PhysicsConstants.hpp"

namespace astroparty::rendering {

using core::Vector2;

namespace {

constexpr std::array<SDL_Color, 2> kShipColors{{
    {80, 170, 255, 255},   // Ship 0: blue
    {255, 95, 95, 255},    // Ship 1: red
}};
constexpr SDL_Color kBulletColor{255, 221, 87, 255};  // yellow, matching upstream's bullet border color
constexpr SDL_Color kBackgroundColor{10, 10, 20, 255};

// A ship is drawn as a triangle pointing along its heading. `noseScale` and
// `baseScale` are fractions of the ship's collision radius, chosen purely by
// eye to read clearly as a small ship at the arena scale in
// PhysicsConstants.hpp; they carry no gameplay meaning.
constexpr float kNoseScale = 1.0f;
constexpr float kBaseBackScale = 0.6f;
constexpr float kBaseWidthScale = 0.6f;

void drawFilledSquare(SDL_Renderer* renderer, Vector2 center, float halfSize) {
    const SDL_FRect rect{center.x - halfSize, center.y - halfSize, halfSize * 2.0f, halfSize * 2.0f};
    SDL_RenderFillRectF(renderer, &rect);
}

}  // namespace

GameRenderer::GameRenderer(SdlRenderer& renderer) noexcept : renderer_(renderer) {}

void GameRenderer::renderFrame(const core::World& world) {
    clearBackground();

    for (const entities::Ship& ship : world.ships()) {
        if (ship.isAlive()) {
            drawShip(ship);
        }
    }
    for (const entities::Bullet& bullet : world.bullets()) {
        drawBullet(bullet);
    }

    SDL_RenderPresent(renderer_.handle());
}

void GameRenderer::clearBackground() {
    SDL_SetRenderDrawColor(renderer_.handle(), kBackgroundColor.r, kBackgroundColor.g, kBackgroundColor.b,
                            kBackgroundColor.a);
    SDL_RenderClear(renderer_.handle());
}

void GameRenderer::drawShip(const entities::Ship& ship) const {
    const SDL_Color color = kShipColors[static_cast<std::size_t>(ship.id()) % kShipColors.size()];
    SDL_SetRenderDrawColor(renderer_.handle(), color.r, color.g, color.b, color.a);

    // Build the triangle in world space directly from the ship's forward and
    // perpendicular directions, so no separate rotation math is needed here:
    // Vector2::fromAngleDegrees already encodes the project's screen-angle
    // convention (see Vector2.hpp).
    const Vector2 forward = Vector2::fromAngleDegrees(ship.headingDegrees());
    const Vector2 rightward = Vector2::fromAngleDegrees(ship.headingDegrees() + 90.0f);
    const float radius = physics::kShipCollisionRadius;

    const Vector2 nose = ship.position() + forward * (radius * kNoseScale);
    const Vector2 baseCenter = ship.position() - forward * (radius * kBaseBackScale);
    const Vector2 baseLeft = baseCenter - rightward * (radius * kBaseWidthScale);
    const Vector2 baseRight = baseCenter + rightward * (radius * kBaseWidthScale);

    SDL_RenderDrawLineF(renderer_.handle(), nose.x, nose.y, baseLeft.x, baseLeft.y);
    SDL_RenderDrawLineF(renderer_.handle(), baseLeft.x, baseLeft.y, baseRight.x, baseRight.y);
    SDL_RenderDrawLineF(renderer_.handle(), baseRight.x, baseRight.y, nose.x, nose.y);
}

void GameRenderer::drawBullet(const entities::Bullet& bullet) const {
    SDL_SetRenderDrawColor(renderer_.handle(), kBulletColor.r, kBulletColor.g, kBulletColor.b, kBulletColor.a);
    drawFilledSquare(renderer_.handle(), bullet.position(), physics::kBulletRenderRadius);
}

}  // namespace astroparty::rendering
