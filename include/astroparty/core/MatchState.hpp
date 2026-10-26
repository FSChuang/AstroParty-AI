#pragma once

#include <optional>
#include <vector>

#include "astroparty/entities/Ship.hpp"

namespace astroparty::core {

enum class MatchOutcome { Undecided, ShipWon, Draw };

// Tracks whether the current match is still being played and, once decided,
// who won.
//
// This is the only rule this project currently implements -- "last ship
// alive wins" -- kept in its own small type so it can be replaced with
// team-based or scored rules in a later milestone without touching World's
// movement or collision logic. This replaces upstream's you_win_timer /
// InputHandler_button_restart mechanism, which the migration audit flagged
// as an acknowledged hack (docs/UPSTREAM_AUDIT.md,
// section 12): that mechanism also had no explicit "draw" outcome for
// simultaneous eliminations, which this version adds.
class MatchState {
public:
    // Re-evaluates the outcome from the ships' current isAlive() status. A
    // no-op once the match has already been decided -- call reset() first to
    // start evaluating a new match.
    void evaluate(const std::vector<entities::Ship>& ships) noexcept;

    void reset() noexcept;

    [[nodiscard]] MatchOutcome outcome() const noexcept { return outcome_; }
    [[nodiscard]] bool hasEnded() const noexcept { return outcome_ != MatchOutcome::Undecided; }

    // Valid only when outcome() == MatchOutcome::ShipWon.
    [[nodiscard]] std::optional<int> winningShipId() const noexcept { return winningShipId_; }

private:
    MatchOutcome outcome_ = MatchOutcome::Undecided;
    std::optional<int> winningShipId_;
};

}  // namespace astroparty::core
