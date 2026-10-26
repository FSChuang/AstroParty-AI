#include "astroparty/headless/TeamMatchResult.hpp"

#include <nlohmann/json.hpp>

namespace astroparty::headless {

namespace {

const char* outcomeToString(TeamMatchResultOutcome outcome) {
    switch (outcome) {
        case TeamMatchResultOutcome::TeamWon:
            return "TeamWon";
        case TeamMatchResultOutcome::Draw:
            return "Draw";
        case TeamMatchResultOutcome::Timeout:
            return "Timeout";
    }
    return "Timeout";  // Unreachable for a valid enum value; keeps -Wreturn-type satisfied.
}

}  // namespace

std::string TeamMatchResult::toJsonString() const {
    nlohmann::json json;
    json["outcome"] = outcomeToString(outcome);
    json["winningTeamId"] = winningTeamId ? nlohmann::json(*winningTeamId) : nlohmann::json(nullptr);
    json["ticksElapsed"] = ticksElapsed;
    json["teamScores"] = teamScores;
    json["resourcesCollected"] = resourcesCollected;
    json["shipsDestroyed"] = shipsDestroyed;
    return json.dump(2);
}

}  // namespace astroparty::headless
