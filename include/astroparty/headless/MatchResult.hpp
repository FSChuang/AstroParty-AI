#pragma once

#include <optional>
#include <string>

namespace astroparty::headless {

// How a headless match ended. Distinct from core::MatchOutcome: that type
// only knows about the in-simulation rule ("last ship alive wins" or
// "everyone died at once"); Timeout is a property of the *run* (the tick
// budget was exhausted before the match reached either of those), not of
// the simulation rules themselves, so it belongs at this layer rather than
// in core::MatchState.
enum class MatchResultOutcome { ShipWon, Draw, Timeout };

struct MatchResult {
    MatchResultOutcome outcome = MatchResultOutcome::Timeout;
    std::optional<int> winningShipId;
    int ticksElapsed = 0;

    // Serializes this result as a JSON object, e.g.:
    //   {"outcome": "ShipWon", "winningShipId": 0, "ticksElapsed": 212}
    // winningShipId is JSON null when the match did not end in a win.
    [[nodiscard]] std::string toJsonString() const;
};

}  // namespace astroparty::headless
