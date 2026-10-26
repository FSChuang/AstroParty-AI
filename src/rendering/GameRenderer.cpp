#include "astroparty/rendering/GameRenderer.hpp"

#include <array>
#include <cmath>
#include <cstddef>

#include "astroparty/core/Vector2.hpp"

namespace astroparty::rendering {

using core::Vector2;

namespace {

constexpr std::array<SDL_Color, 2> kShipColors{{
    {80, 170, 255, 255},   // Team / ship-index 0: blue
    {255, 95, 95, 255},    // Team / ship-index 1: red
}};
constexpr SDL_Color kBulletColor{255, 221, 87, 255};    // yellow, matching upstream's bullet border color
constexpr SDL_Color kObstacleColor{120, 120, 130, 255};  // neutral gray
constexpr SDL_Color kResourceColor{80, 220, 140, 255};   // green
constexpr SDL_Color kBackgroundColor{10, 10, 20, 255};

// Cosmetic radius used only for drawing a bullet. This is not a simulation
// parameter -- it has no effect on collision outcomes (see
// physics::isPointWithinRadius, which treats bullets as dimensionless
// points) -- so it stays a rendering-only constant rather than living in
// SimulationConfig.
constexpr float kBulletRenderRadius = 3.0f;
constexpr float kResourceRenderRadius = 8.0f;
constexpr int kCircleSegments = 20;

// A ship is drawn as a triangle pointing along its heading, sized from the
// ship's actual collision radius (SimulationConfig::shipCollisionRadius) so
// the drawn silhouette always matches what can get hit. `noseScale` and the
// two base scales are fractions of that radius, chosen purely by eye to read
// clearly as a small ship; they carry no gameplay meaning.
constexpr float kNoseScale = 1.0f;
constexpr float kBaseBackScale = 0.6f;
constexpr float kBaseWidthScale = 0.6f;

void drawFilledSquare(SDL_Renderer* renderer, Vector2 center, float halfSize) {
    const SDL_FRect rect{center.x - halfSize, center.y - halfSize, halfSize * 2.0f, halfSize * 2.0f};
    SDL_RenderFillRectF(renderer, &rect);
}

// Approximates a circle outline with kCircleSegments short line segments --
// SDL2 has no built-in circle primitive without an extension library this
// project does not depend on.
void drawCircleOutline(SDL_Renderer* renderer, Vector2 center, float radius) {
    constexpr float kFullCircleRadians = 2.0f * 3.14159265358979323846f;
    Vector2 previous{center.x + radius, center.y};
    for (int segment = 1; segment <= kCircleSegments; ++segment) {
        const float angle = kFullCircleRadians * static_cast<float>(segment) / static_cast<float>(kCircleSegments);
        const Vector2 next{center.x + radius * std::cos(angle), center.y + radius * std::sin(angle)};
        SDL_RenderDrawLineF(renderer, previous.x, previous.y, next.x, next.y);
        previous = next;
    }
}

}  // namespace

GameRenderer::GameRenderer(SdlRenderer& renderer) noexcept : renderer_(renderer) {}

void GameRenderer::renderFrame(const core::World& world) {
    clearBackground();

    for (const core::ObstacleDefinition& obstacle : world.obstacles()) {
        drawObstacle(obstacle);
    }
    for (const core::HomeRegion& home : world.map().homeRegions) {
        drawHomeRegion(home);
    }

    const float collisionRadius = world.config().shipCollisionRadius;
    for (const entities::Ship& ship : world.ships()) {
        if (!ship.isAlive()) {
            continue;
        }
        const int teamId = world.teamIdOfShip(ship.id());
        const int colorIndex = (teamId != -1) ? teamId : (ship.id() % static_cast<int>(kShipColors.size()));
        drawShip(ship, collisionRadius, colorIndex);
    }

    for (const entities::Resource& resource : world.resources()) {
        if (resource.state() == entities::ResourceState::AtSpawn) {
            drawAvailableResource(resource);
        } else {
            const auto carrierIndex = static_cast<std::size_t>(*resource.carrierShipId());
            drawCarryingRing(world.ships()[carrierIndex].position(), collisionRadius);
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

void GameRenderer::drawShip(const entities::Ship& ship, float collisionRadius, int colorIndex) const {
    const SDL_Color color = kShipColors[static_cast<std::size_t>(colorIndex) % kShipColors.size()];
    SDL_SetRenderDrawColor(renderer_.handle(), color.r, color.g, color.b, color.a);

    // Build the triangle in world space directly from the ship's forward and
    // perpendicular directions, so no separate rotation math is needed here:
    // Vector2::fromAngleDegrees already encodes the project's screen-angle
    // convention (see Vector2.hpp).
    const Vector2 forward = Vector2::fromAngleDegrees(ship.headingDegrees());
    const Vector2 rightward = Vector2::fromAngleDegrees(ship.headingDegrees() + 90.0f);

    const Vector2 nose = ship.position() + forward * (collisionRadius * kNoseScale);
    const Vector2 baseCenter = ship.position() - forward * (collisionRadius * kBaseBackScale);
    const Vector2 baseLeft = baseCenter - rightward * (collisionRadius * kBaseWidthScale);
    const Vector2 baseRight = baseCenter + rightward * (collisionRadius * kBaseWidthScale);

    SDL_RenderDrawLineF(renderer_.handle(), nose.x, nose.y, baseLeft.x, baseLeft.y);
    SDL_RenderDrawLineF(renderer_.handle(), baseLeft.x, baseLeft.y, baseRight.x, baseRight.y);
    SDL_RenderDrawLineF(renderer_.handle(), baseRight.x, baseRight.y, nose.x, nose.y);
}

void GameRenderer::drawBullet(const entities::Bullet& bullet) const {
    SDL_SetRenderDrawColor(renderer_.handle(), kBulletColor.r, kBulletColor.g, kBulletColor.b, kBulletColor.a);
    drawFilledSquare(renderer_.handle(), bullet.position(), kBulletRenderRadius);
}

void GameRenderer::drawObstacle(const core::ObstacleDefinition& obstacle) const {
    SDL_SetRenderDrawColor(renderer_.handle(), kObstacleColor.r, kObstacleColor.g, kObstacleColor.b, kObstacleColor.a);
    drawCircleOutline(renderer_.handle(), obstacle.position, obstacle.radius);
}

void GameRenderer::drawHomeRegion(const core::HomeRegion& home) const {
    const SDL_Color color = kShipColors[static_cast<std::size_t>(home.teamId) % kShipColors.size()];
    SDL_SetRenderDrawColor(renderer_.handle(), color.r, color.g, color.b, color.a);
    drawCircleOutline(renderer_.handle(), home.position, home.radius);
}

void GameRenderer::drawAvailableResource(const entities::Resource& resource) const {
    SDL_SetRenderDrawColor(renderer_.handle(), kResourceColor.r, kResourceColor.g, kResourceColor.b, kResourceColor.a);
    drawFilledSquare(renderer_.handle(), resource.spawnPosition(), kResourceRenderRadius);
}

void GameRenderer::drawCarryingRing(Vector2 shipPosition, float shipCollisionRadius) const {
    SDL_SetRenderDrawColor(renderer_.handle(), kResourceColor.r, kResourceColor.g, kResourceColor.b, kResourceColor.a);
    drawCircleOutline(renderer_.handle(), shipPosition, shipCollisionRadius * 1.5f);
}

}  // namespace astroparty::rendering
