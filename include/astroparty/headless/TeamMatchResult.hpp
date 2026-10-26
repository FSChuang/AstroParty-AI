#pragma once

#include <array>
#include <optional>
#include <string>

namespace astroparty::headless {

// How a headless Team Resource Battle match ended. Mirrors MatchResultOutcome's
// split between in-simulation rules and run-level timeout: TeamWon/Draw come
// from core::TeamMatchOutcome, while Timeout additionally covers this run's
// own maxTicks budget expiring before the simulation itself decided the
// match by score (which core::TeamMatchState also resolves by score once
// config().maxMatchTicks is reached -- the two timeouts normally coincide
// when the headless run's maxTicks matches the config, but need not).
enum class TeamMatchResultOutcome { TeamWon, Draw, Timeout };

struct TeamMatchResult {
    TeamMatchResultOutcome outcome = TeamMatchResultOutcome::Timeout;
    std::optional<int> winningTeamId;
    int ticksElapsed = 0;

    // Final delivered-resource score per team (team 0, team 1).
    std::array<int, 2> teamScores{};
    // Cumulative resource pickups per team, whether or not later delivered.
    std::array<int, 2> resourcesCollected{};
    // Cumulative ship eliminations per team.
    std::array<int, 2> shipsDestroyed{};

    // Serializes this result as a JSON object, e.g.:
    //   {
    //     "outcome": "TeamWon", "winningTeamId": 0, "ticksElapsed": 500,
    //     "teamScores": [3, 1], "resourcesCollected": [4, 2], "shipsDestroyed": [1, 3]
    //   }
    // winningTeamId is JSON null when the match did not end in a win.
    [[nodiscard]] std::string toJsonString() const;
};

}  // namespace astroparty::headless
