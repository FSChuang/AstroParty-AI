include(FetchContent)
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.11
)
# doctest 2.4.11's own CMakeLists.txt declares cmake_minimum_required(VERSION 3.0),
# a floor CMake >= 4.0 refuses to honor. This only relaxes that check for
# fetched dependencies, not for this project's own CMakeLists.txt files.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_MakeAvailable(doctest)
unset(CMAKE_POLICY_VERSION_MINIMUM)

add_executable(astroparty_tests
    test_main.cpp
    test_vector2.cpp
    test_ship.cpp
    test_bullet.cpp
    test_collisions.cpp
    test_match_state.cpp
    test_world.cpp
    test_simulation_config.cpp
    test_map_definition.cpp
    test_controllers.cpp
    test_headless_match_runner.cpp
    test_example_assets.cpp
    test_obstacle_physics.cpp
    test_team_match_state.cpp
    test_resources.cpp
    test_steering.cpp
    test_unit_behaviors.cpp
    test_team_strategies.cpp
    test_team_match_runner.cpp
)
target_link_libraries(astroparty_tests PRIVATE
    astroparty_sim
    doctest::doctest
    astroparty_warnings
    astroparty_sanitizers
)
# Lets test_example_assets.cpp find configs/ regardless of where the build
# directory lives, so the checked-in example config/map files are exercised
# as a regression guard against them silently going stale.
target_compile_definitions(astroparty_tests PRIVATE
    ASTROPARTY_CONFIGS_DIR="${CMAKE_SOURCE_DIR}/configs"
)

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(astroparty_tests)
