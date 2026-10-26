#include "astroparty/core/TeamMatchState.hpp"

namespace astroparty::core {

namespace {

// Resolves a final tie-break between two scores: the higher score wins
// outright, equal scores are a draw. Shared by both the winningScore and
// the match-timeout evaluation paths below, since they apply the identical
// rule once a match-ending condition has been reached.
void resolveByScore(std::array<int, 2> teamScores, TeamMatchOutcome& outcome, std::optional<int>& winningTeamId) {
    if (teamScores[0] == teamScores[1]) {
        outcome = TeamMatchOutcome::Draw;
    } else {
        outcome = TeamMatchOutcome::TeamWon;
        winningTeamId = (teamScores[0] > teamScores[1]) ? 0 : 1;
    }
}

}  // namespace

void TeamMatchState::evaluate(std::array<int, 2> teamScores, int winningScore, int elapsedTicks,
                               int maxMatchTicks) noexcept {
    if (hasEnded()) {
        return;
    }

    const bool team0Reached = teamScores[0] >= winningScore;
    const bool team1Reached = teamScores[1] >= winningScore;
    if (team0Reached || team1Reached) {
        resolveByScore(teamScores, outcome_, winningTeamId_);
        return;
    }

    if (maxMatchTicks > 0 && elapsedTicks >= maxMatchTicks) {
        resolveByScore(teamScores, outcome_, winningTeamId_);
    }
}

void TeamMatchState::reset() noexcept {
    outcome_ = TeamMatchOutcome::Undecided;
    winningTeamId_.reset();
}

}  // namespace astroparty::core
