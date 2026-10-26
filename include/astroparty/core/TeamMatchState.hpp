#pragma once

#include <array>
#include <optional>

namespace astroparty::core {

enum class TeamMatchOutcome { Undecided, TeamWon, Draw };

// Tracks whether a Team Resource Battle match is decided, mirroring
// MatchState's role for Classic Deathmatch but for a completely different
// rule: the first team to reach a configured score wins outright; if the
// match's tick budget runs out first, whichever team has the higher score
// at that point wins, and equal scores at that point is a Draw. Kept
// separate from MatchState because the two modes' win conditions share no
// logic -- see docs/ARCHITECTURE.md's "Game modes" section.
class TeamMatchState {
public:
    // Re-evaluates the outcome from current team scores and elapsed/budget
    // ticks. A no-op once the match has already been decided -- call
    // reset() first to start evaluating a new match. `maxMatchTicks <= 0`
    // means "no tick limit" (only the winningScore condition can end the
    // match).
    void evaluate(std::array<int, 2> teamScores, int winningScore, int elapsedTicks, int maxMatchTicks) noexcept;

    void reset() noexcept;

    [[nodiscard]] TeamMatchOutcome outcome() const noexcept { return outcome_; }
    [[nodiscard]] bool hasEnded() const noexcept { return outcome_ != TeamMatchOutcome::Undecided; }

    // Valid only when outcome() == TeamMatchOutcome::TeamWon.
    [[nodiscard]] std::optional<int> winningTeamId() const noexcept { return winningTeamId_; }

private:
    TeamMatchOutcome outcome_ = TeamMatchOutcome::Undecided;
    std::optional<int> winningTeamId_;
};

}  // namespace astroparty::core
