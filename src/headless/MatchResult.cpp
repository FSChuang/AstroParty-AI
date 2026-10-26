#include "astroparty/headless/MatchResult.hpp"

#include <nlohmann/json.hpp>

namespace astroparty::headless {

namespace {

const char* outcomeToString(MatchResultOutcome outcome) {
    switch (outcome) {
        case MatchResultOutcome::ShipWon:
            return "ShipWon";
        case MatchResultOutcome::Draw:
            return "Draw";
        case MatchResultOutcome::Timeout:
            return "Timeout";
    }
    return "Timeout";  // Unreachable for a valid enum value; keeps -Wreturn-type satisfied.
}

}  // namespace

std::string MatchResult::toJsonString() const {
    nlohmann::json json;
    json["outcome"] = outcomeToString(outcome);
    json["winningShipId"] = winningShipId ? nlohmann::json(*winningShipId) : nlohmann::json(nullptr);
    json["ticksElapsed"] = ticksElapsed;
    return json.dump(2);
}

}  // namespace astroparty::headless
