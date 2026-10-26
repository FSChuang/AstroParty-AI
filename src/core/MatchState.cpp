#include "astroparty/core/MatchState.hpp"

namespace astroparty::core {

void MatchState::evaluate(const std::vector<entities::Ship>& ships) noexcept {
    if (hasEnded()) {
        return;
    }

    int aliveCount = 0;
    int lastAliveShipId = -1;
    for (const entities::Ship& ship : ships) {
        if (ship.isAlive()) {
            ++aliveCount;
            lastAliveShipId = ship.id();
        }
    }

    if (aliveCount == 1) {
        outcome_ = MatchOutcome::ShipWon;
        winningShipId_ = lastAliveShipId;
    } else if (aliveCount == 0) {
        outcome_ = MatchOutcome::Draw;
    }
}

void MatchState::reset() noexcept {
    outcome_ = MatchOutcome::Undecided;
    winningShipId_.reset();
}

}  // namespace astroparty::core
