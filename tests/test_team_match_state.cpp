#include <doctest/doctest.h>

#include "astroparty/core/TeamMatchState.hpp"

using astroparty::core::TeamMatchOutcome;
using astroparty::core::TeamMatchState;

TEST_SUITE("TeamMatchState") {
    TEST_CASE("a fresh match with scores below the winning threshold is undecided") {
        TeamMatchState state;
        state.evaluate({1, 1}, /*winningScore=*/3, /*elapsedTicks=*/10, /*maxMatchTicks=*/0);

        CHECK(state.outcome() == TeamMatchOutcome::Undecided);
        CHECK_FALSE(state.hasEnded());
    }

    TEST_CASE("the first team to reach the winning score wins outright") {
        TeamMatchState state;
        state.evaluate({3, 1}, /*winningScore=*/3, /*elapsedTicks=*/50, /*maxMatchTicks=*/0);

        CHECK(state.outcome() == TeamMatchOutcome::TeamWon);
        REQUIRE(state.winningTeamId().has_value());
        CHECK(*state.winningTeamId() == 0);
    }

    TEST_CASE("both teams reaching the winning score simultaneously is resolved by higher score") {
        TeamMatchState state;
        state.evaluate({4, 3}, /*winningScore=*/3, /*elapsedTicks=*/50, /*maxMatchTicks=*/0);

        CHECK(state.outcome() == TeamMatchOutcome::TeamWon);
        CHECK(*state.winningTeamId() == 0);
    }

    TEST_CASE("both teams reaching an equal score at or above the threshold simultaneously is a draw") {
        TeamMatchState state;
        state.evaluate({3, 3}, /*winningScore=*/3, /*elapsedTicks=*/50, /*maxMatchTicks=*/0);

        CHECK(state.outcome() == TeamMatchOutcome::Draw);
        CHECK_FALSE(state.winningTeamId().has_value());
    }

    TEST_CASE("the higher-scoring team wins when the match tick budget expires") {
        TeamMatchState state;
        state.evaluate({2, 1}, /*winningScore=*/10, /*elapsedTicks=*/100, /*maxMatchTicks=*/100);

        CHECK(state.outcome() == TeamMatchOutcome::TeamWon);
        CHECK(*state.winningTeamId() == 0);
    }

    TEST_CASE("equal scores when the match tick budget expires is a draw") {
        TeamMatchState state;
        state.evaluate({2, 2}, /*winningScore=*/10, /*elapsedTicks=*/100, /*maxMatchTicks=*/100);

        CHECK(state.outcome() == TeamMatchOutcome::Draw);
    }

    TEST_CASE("a maxMatchTicks of zero means there is no tick limit") {
        TeamMatchState state;
        state.evaluate({2, 1}, /*winningScore=*/10, /*elapsedTicks=*/1000000, /*maxMatchTicks=*/0);

        CHECK(state.outcome() == TeamMatchOutcome::Undecided);
    }

    TEST_CASE("a decided match ignores further evaluation until reset") {
        TeamMatchState state;
        state.evaluate({3, 0}, 3, 10, 0);
        REQUIRE(state.outcome() == TeamMatchOutcome::TeamWon);

        // Even if scores later change implausibly, the already-decided
        // outcome must not change underneath the caller.
        state.evaluate({0, 5}, 3, 20, 0);

        CHECK(state.outcome() == TeamMatchOutcome::TeamWon);
        CHECK(*state.winningTeamId() == 0);
    }

    TEST_CASE("reset returns the match to Undecided with no winner") {
        TeamMatchState state;
        state.evaluate({3, 0}, 3, 10, 0);
        REQUIRE(state.hasEnded());

        state.reset();

        CHECK(state.outcome() == TeamMatchOutcome::Undecided);
        CHECK_FALSE(state.hasEnded());
        CHECK_FALSE(state.winningTeamId().has_value());
    }
}
