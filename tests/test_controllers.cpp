#include <doctest/doctest.h>

#include "astroparty/core/World.hpp"
#include "astroparty/input/ScriptedShipController.hpp"

using astroparty::core::World;
using astroparty::input::ScriptedShipController;

TEST_SUITE("ScriptedShipController") {
    TEST_CASE("turnEveryOtherTick alternates turnRight across successive ticks") {
        World world;  // Required by the IShipController interface; this controller ignores it.
        ScriptedShipController controller(/*turnEveryOtherTick=*/true, /*shootIntervalTicks=*/0);

        CHECK(controller.decide(world, 0).turnRight);
        CHECK_FALSE(controller.decide(world, 0).turnRight);
        CHECK(controller.decide(world, 0).turnRight);
        CHECK_FALSE(controller.decide(world, 0).turnRight);
    }

    TEST_CASE("turnEveryOtherTick disabled never turns") {
        World world;
        ScriptedShipController controller(/*turnEveryOtherTick=*/false, /*shootIntervalTicks=*/0);

        for (int i = 0; i < 4; ++i) {
            CHECK_FALSE(controller.decide(world, 0).turnRight);
        }
    }

    TEST_CASE("a positive shootIntervalTicks fires exactly once per interval") {
        World world;
        ScriptedShipController controller(/*turnEveryOtherTick=*/false, /*shootIntervalTicks=*/3);

        // Ticks 0,1,2,3,4,5 -> shoot on 0 and 3 only.
        CHECK(controller.decide(world, 0).shoot);
        CHECK_FALSE(controller.decide(world, 0).shoot);
        CHECK_FALSE(controller.decide(world, 0).shoot);
        CHECK(controller.decide(world, 0).shoot);
        CHECK_FALSE(controller.decide(world, 0).shoot);
    }

    TEST_CASE("a shootIntervalTicks of zero never fires") {
        World world;
        ScriptedShipController controller(/*turnEveryOtherTick=*/false, /*shootIntervalTicks=*/0);

        for (int i = 0; i < 5; ++i) {
            CHECK_FALSE(controller.decide(world, 0).shoot);
        }
    }

    TEST_CASE("two independent controller instances keep independent internal state") {
        World world;
        ScriptedShipController first(true, 0);
        ScriptedShipController second(true, 0);

        // Advance `first` three times before `second` is ever polled.
        (void)first.decide(world, 0);
        (void)first.decide(world, 0);
        (void)first.decide(world, 0);

        // `second` must still start from its own tick zero, unaffected by `first`.
        CHECK(second.decide(world, 1).turnRight);
    }

    TEST_CASE("the same controller configuration produces the same action sequence every run") {
        World world;
        ScriptedShipController controllerA(true, 4);
        ScriptedShipController controllerB(true, 4);

        for (int tick = 0; tick < 20; ++tick) {
            const auto actionA = controllerA.decide(world, 0);
            const auto actionB = controllerB.decide(world, 0);
            CHECK(actionA.turnRight == actionB.turnRight);
            CHECK(actionA.shoot == actionB.shoot);
        }
    }
}
