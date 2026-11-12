#include "astroparty/diagnostics/MatchDiagnostics.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "astroparty/ai/TeamStrategy.hpp"

namespace astroparty::diagnostics {

using ai::ShipRole;
using core::GameMode;
using core::ShipAction;
using core::TeamMatchOutcome;
using core::Vector2;
using core::World;
using entities::ResourceState;
using headless::TeamMatchResult;
using headless::TeamMatchResultOutcome;
using input::IShipController;

namespace {

// Small margin beyond exact tangency so two circles resting against each
// other (as pushOutOfCircle leaves them) still read as "touching" despite
// floating-point noise.
constexpr float kObstacleContactEpsilon = 1.0f;

std::vector<std::vector<int>> teamShipIdsByTeam(const World& world) {
    std::vector<std::vector<int>> teamShipIds(2);
    for (std::size_t i = 0; i < world.ships().size(); ++i) {
        const int teamId = world.teamIdOfShip(static_cast<int>(i));
        if (teamId == 0 || teamId == 1) {
            teamShipIds[static_cast<std::size_t>(teamId)].push_back(static_cast<int>(i));
        }
    }
    return teamShipIds;
}

bool isTouchingAnyObstacle(Vector2 shipPosition, float shipCollisionRadius,
                            const std::vector<core::ObstacleDefinition>& obstacles) {
    for (const core::ObstacleDefinition& obstacle : obstacles) {
        const float combined = obstacle.radius + shipCollisionRadius + kObstacleContactEpsilon;
        if (core::distanceSquared(shipPosition, obstacle.position) <= combined * combined) {
            return true;
        }
    }
    return false;
}

}  // namespace

std::string MatchDiagnostics::toJsonString() const {
    nlohmann::json json;
    for (int teamId = 0; teamId < 2; ++teamId) {
        nlohmann::json teamJson;
        nlohmann::json shipsJson = nlohmann::json::array();
        for (const ShipDiagnostics& ship : teams[static_cast<std::size_t>(teamId)].ships) {
            shipsJson.push_back({{"shotsFired", ship.shotsFired},
                                  {"obstacleCollisionEvents", ship.obstacleCollisionEvents},
                                  {"lowProgressTicks", ship.lowProgressTicks},
                                  {"roleChanges", ship.roleChanges}});
        }
        teamJson["ships"] = shipsJson;
        teamJson["deliveryLatenciesTicks"] = teams[static_cast<std::size_t>(teamId)].deliveryLatenciesTicks;
        json["team" + std::to_string(teamId)] = teamJson;
    }
    return json.dump(2);
}

std::pair<TeamMatchResult, MatchDiagnostics> runInstrumentedTeamMatch(
    World& world, std::vector<std::unique_ptr<IShipController>>& controllers, int maxTicks,
    std::shared_ptr<ai::ITeamStrategy> team0Strategy, std::shared_ptr<ai::ITeamStrategy> team1Strategy,
    StuckDetectionConfig stuckConfig) {
    if (maxTicks <= 0) {
        throw std::invalid_argument("runInstrumentedTeamMatch: maxTicks must be positive");
    }
    if (world.mode() != GameMode::TeamResourceBattle) {
        throw std::invalid_argument("runInstrumentedTeamMatch: world must be in GameMode::TeamResourceBattle");
    }
    if (controllers.size() != world.ships().size()) {
        throw std::invalid_argument("runInstrumentedTeamMatch: controllers.size() must equal world.ships().size()");
    }

    const std::vector<std::vector<int>> teamShipIds = teamShipIdsByTeam(world);
    const std::array<std::shared_ptr<ai::ITeamStrategy>, 2> strategies{team0Strategy, team1Strategy};

    MatchDiagnostics diagnostics;
    for (int teamId = 0; teamId < 2; ++teamId) {
        diagnostics.teams[static_cast<std::size_t>(teamId)].ships.assign(
            teamShipIds[static_cast<std::size_t>(teamId)].size(), ShipDiagnostics{});
    }

    const std::size_t shipCount = world.ships().size();
    std::vector<bool> previousShootHeld(shipCount, false);
    std::vector<bool> previousTouchingObstacle(shipCount, false);
    std::vector<std::optional<ShipRole>> previousRole(shipCount, std::nullopt);
    std::vector<std::vector<Vector2>> positionHistory(shipCount);  // Ring-buffer-like: oldest at front.

    std::unordered_map<int, int> pickupTickByResourceId;
    std::unordered_map<int, int> pickupTeamByResourceId;
    std::unordered_map<int, ResourceState> previousResourceState;
    for (const entities::Resource& resource : world.resources()) {
        previousResourceState[resource.id()] = resource.state();
    }
    std::array<int, 2> previousScores = world.teamScores();

    const float deltaSeconds = world.config().fixedDeltaSeconds();
    const float shipCollisionRadius = world.config().shipCollisionRadius;

    int tick = 0;
    for (; tick < maxTicks; ++tick) {
        if (world.teamMatchState().hasEnded()) {
            break;
        }

        // Observe each team's role assignment for THIS tick's World state,
        // the same state every ai::TeamShipController on that team is
        // about to decide() from -- a read-only, redundant recomputation
        // (see ai::TeamShipController's own doc comment on why that is an
        // accepted, harmless pattern at this project's scale).
        for (int teamId = 0; teamId < 2; ++teamId) {
            const auto& strategy = strategies[static_cast<std::size_t>(teamId)];
            if (!strategy) {
                continue;
            }
            const std::vector<int>& ids = teamShipIds[static_cast<std::size_t>(teamId)];
            const std::vector<ai::ShipGoal> goals = strategy->assignGoals(world, teamId, ids);
            auto& shipDiagnostics = diagnostics.teams[static_cast<std::size_t>(teamId)].ships;
            for (std::size_t i = 0; i < ids.size(); ++i) {
                const int shipId = ids[i];
                if (previousRole[static_cast<std::size_t>(shipId)].has_value() &&
                    *previousRole[static_cast<std::size_t>(shipId)] != goals[i].role) {
                    ++shipDiagnostics[i].roleChanges;
                }
                previousRole[static_cast<std::size_t>(shipId)] = goals[i].role;
            }
        }

        std::vector<ShipAction> actions;
        actions.reserve(controllers.size());
        for (std::size_t i = 0; i < controllers.size(); ++i) {
            actions.push_back(controllers[i]->decide(world, static_cast<int>(i)));
        }

        for (std::size_t shipId = 0; shipId < shipCount; ++shipId) {
            const int teamId = world.teamIdOfShip(static_cast<int>(shipId));
            if (teamId != 0 && teamId != 1) {
                continue;
            }
            const auto& ids = teamShipIds[static_cast<std::size_t>(teamId)];
            const auto positionInTeam = std::find(ids.begin(), ids.end(), static_cast<int>(shipId));
            if (positionInTeam == ids.end()) {
                continue;
            }
            const std::size_t teamIndex = static_cast<std::size_t>(std::distance(ids.begin(), positionInTeam));

            const bool shootsNow = actions[shipId].shoot;
            if (shootsNow && !previousShootHeld[shipId]) {
                ++diagnostics.teams[static_cast<std::size_t>(teamId)].ships[teamIndex].shotsFired;
            }
            previousShootHeld[shipId] = shootsNow;
        }

        world.step(actions, deltaSeconds);

        const std::array<int, 2> newScores = world.teamScores();
        for (const entities::Resource& resource : world.resources()) {
            const ResourceState previousState = previousResourceState[resource.id()];
            const ResourceState currentState = resource.state();
            if (previousState == ResourceState::AtSpawn && currentState == ResourceState::Carried) {
                pickupTickByResourceId[resource.id()] = tick;
                pickupTeamByResourceId[resource.id()] = world.teamIdOfShip(*resource.carrierShipId());
            } else if (previousState == ResourceState::Carried && currentState == ResourceState::AtSpawn) {
                const auto teamIt = pickupTeamByResourceId.find(resource.id());
                if (teamIt != pickupTeamByResourceId.end()) {
                    const int team = teamIt->second;
                    if (newScores[static_cast<std::size_t>(team)] > previousScores[static_cast<std::size_t>(team)]) {
                        const auto tickIt = pickupTickByResourceId.find(resource.id());
                        if (tickIt != pickupTickByResourceId.end()) {
                            diagnostics.teams[static_cast<std::size_t>(team)].deliveryLatenciesTicks.push_back(
                                tick - tickIt->second);
                        }
                    }
                }
                pickupTickByResourceId.erase(resource.id());
                pickupTeamByResourceId.erase(resource.id());
            }
            previousResourceState[resource.id()] = currentState;
        }
        previousScores = newScores;

        for (std::size_t shipId = 0; shipId < shipCount; ++shipId) {
            const int teamId = world.teamIdOfShip(static_cast<int>(shipId));
            if (teamId != 0 && teamId != 1) {
                continue;
            }
            const auto& ids = teamShipIds[static_cast<std::size_t>(teamId)];
            const auto positionInTeam = std::find(ids.begin(), ids.end(), static_cast<int>(shipId));
            const std::size_t teamIndex = static_cast<std::size_t>(std::distance(ids.begin(), positionInTeam));
            ShipDiagnostics& shipDiagnostics = diagnostics.teams[static_cast<std::size_t>(teamId)].ships[teamIndex];

            const Vector2 position = world.ships()[shipId].position();
            const bool touching = isTouchingAnyObstacle(position, shipCollisionRadius, world.obstacles());
            if (touching && !previousTouchingObstacle[shipId]) {
                ++shipDiagnostics.obstacleCollisionEvents;
            }
            previousTouchingObstacle[shipId] = touching;

            std::vector<Vector2>& history = positionHistory[shipId];
            history.push_back(position);
            if (static_cast<int>(history.size()) > stuckConfig.windowTicks) {
                const Vector2 oldest = history.front();
                history.erase(history.begin());
                const float netDisplacement = std::sqrt(core::distanceSquared(position, oldest));
                if (netDisplacement < stuckConfig.minNetDisplacement) {
                    ++shipDiagnostics.lowProgressTicks;
                }
            }
        }
    }

    TeamMatchResult result;
    result.ticksElapsed = tick;
    result.teamScores = world.teamScores();
    result.resourcesCollected = world.resourcesCollected();
    result.shipsDestroyed = world.shipsDestroyed();
    const TeamMatchOutcome outcome = world.teamMatchState().outcome();
    if (outcome == TeamMatchOutcome::TeamWon) {
        result.outcome = TeamMatchResultOutcome::TeamWon;
        result.winningTeamId = world.teamMatchState().winningTeamId();
    } else if (outcome == TeamMatchOutcome::Draw) {
        result.outcome = TeamMatchResultOutcome::Draw;
    } else {
        result.outcome = TeamMatchResultOutcome::Timeout;
    }

    return {result, diagnostics};
}

}  // namespace astroparty::diagnostics
