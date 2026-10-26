#include <doctest/doctest.h>

#include <vector>

#include "astroparty/core/MatchState.hpp"
#include "astroparty/core/Vector2.hpp"
#include "astroparty/entities/Ship.hpp"

using astroparty::core::MatchOutcome;
using astroparty::core::MatchState;
using astroparty::core::Vector2;
using astroparty::entities::Ship;

TEST_SUITE("MatchState") {
    TEST_CASE("a match with every ship alive is undecided") {
        std::vector<Ship> ships;
        ships.emplace_back(0, Vector2{0.0f, 0.0f}, 0.0f);
        ships.emplace_back(1, Vector2{0.0f, 0.0f}, 0.0f);

        MatchState matchState;
        matchState.evaluate(ships);

        CHECK(matchState.outcome() == MatchOutcome::Undecided);
        CHECK_FALSE(matchState.hasEnded());
    }

    TEST_CASE("the sole surviving ship is declared the winner") {
        std::vector<Ship> ships;
        ships.emplace_back(0, Vector2{0.0f, 0.0f}, 0.0f);
        ships.emplace_back(1, Vector2{0.0f, 0.0f}, 0.0f);
        ships[1].eliminate();

        MatchState matchState;
        matchState.evaluate(ships);

        CHECK(matchState.outcome() == MatchOutcome::ShipWon);
        CHECK(matchState.hasEnded());
        REQUIRE(matchState.winningShipId().has_value());
        CHECK(*matchState.winningShipId() == 0);
    }

    TEST_CASE("simultaneous elimination of every ship is a draw") {
        std::vector<Ship> ships;
        ships.emplace_back(0, Vector2{0.0f, 0.0f}, 0.0f);
        ships.emplace_back(1, Vector2{0.0f, 0.0f}, 0.0f);
        ships[0].eliminate();
        ships[1].eliminate();

        MatchState matchState;
        matchState.evaluate(ships);

        CHECK(matchState.outcome() == MatchOutcome::Draw);
        CHECK(matchState.hasEnded());
        CHECK_FALSE(matchState.winningShipId().has_value());
    }

    TEST_CASE("a decided match ignores further evaluation until reset") {
        std::vector<Ship> ships;
        ships.emplace_back(0, Vector2{0.0f, 0.0f}, 0.0f);
        ships.emplace_back(1, Vector2{0.0f, 0.0f}, 0.0f);
        ships[1].eliminate();

        MatchState matchState;
        matchState.evaluate(ships);
        REQUIRE(matchState.outcome() == MatchOutcome::ShipWon);

        // Even if the surviving ship is later eliminated too (e.g. a stray
        // update after the match ended), the already-decided outcome must
        // not change underneath the caller.
        ships[0].eliminate();
        matchState.evaluate(ships);
        CHECK(matchState.outcome() == MatchOutcome::ShipWon);
        CHECK(*matchState.winningShipId() == 0);
    }

    TEST_CASE("reset returns the match to Undecided with no winner") {
        std::vector<Ship> ships;
        ships.emplace_back(0, Vector2{0.0f, 0.0f}, 0.0f);
        ships.emplace_back(1, Vector2{0.0f, 0.0f}, 0.0f);
        ships[1].eliminate();

        MatchState matchState;
        matchState.evaluate(ships);
        REQUIRE(matchState.hasEnded());

        matchState.reset();

        CHECK(matchState.outcome() == MatchOutcome::Undecided);
        CHECK_FALSE(matchState.hasEnded());
        CHECK_FALSE(matchState.winningShipId().has_value());
    }
}
