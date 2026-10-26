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
)
target_link_libraries(astroparty_tests PRIVATE
    astroparty_sim
    doctest::doctest
    astroparty_warnings
    astroparty_sanitizers
)

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(astroparty_tests)
