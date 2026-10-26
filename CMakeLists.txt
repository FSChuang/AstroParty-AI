cmake_minimum_required(VERSION 3.21)

project(AstroPartyAI
    VERSION 0.1.0
    DESCRIPTION "AstroParty-AI: an extensible Astro Party-style game and AI experimentation platform"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

option(ASTROPARTY_BUILD_TESTS "Build the unit test suite" ON)
option(ASTROPARTY_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(ASTROPARTY_ENABLE_SANITIZERS "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

add_library(astroparty_warnings INTERFACE)
target_compile_options(astroparty_warnings INTERFACE
    -Wall -Wextra -Wpedantic
    $<$<BOOL:${ASTROPARTY_WARNINGS_AS_ERRORS}>:-Werror>
)

add_library(astroparty_sanitizers INTERFACE)
if(ASTROPARTY_ENABLE_SANITIZERS)
    target_compile_options(astroparty_sanitizers INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer -g)
    target_link_options(astroparty_sanitizers INTERFACE -fsanitize=address,undefined)
endif()

# --- JSON dependency (configuration files, map files) -----------------------
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
# nlohmann/json 3.11.3's own CMakeLists.txt declares a cmake_minimum_required
# floor below what CMake >= 4.0 will honor (the same issue tests/CMakeLists.txt
# already works around for doctest). This only relaxes that check for fetched
# dependencies, not for this project's own CMakeLists.txt files.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_MakeAvailable(nlohmann_json)
unset(CMAKE_POLICY_VERSION_MINIMUM)

# --- Simulation library: core gameplay state and rules, no SDL dependency. --
# This target is the concrete proof that the simulation is independent of
# rendering (docs/UPSTREAM_AUDIT.md, item C-7):
# it builds, and is fully unit-testable, without ever linking SDL2. The one
# controller implementation that needs no SDL (ScriptedShipController) lives
# here too, alongside IShipController's interface and the headless match
# runner, so the headless executable below never needs to link
# astroparty_platform.
add_library(astroparty_sim STATIC
    src/core/Vector2.cpp
    src/core/MatchState.cpp
    src/core/Collisions.cpp
    src/core/SimulationConfig.cpp
    src/core/MapDefinition.cpp
    src/core/World.cpp
    src/entities/Ship.cpp
    src/entities/Bullet.cpp
    src/physics/Arena.cpp
    src/physics/Collision.cpp
    src/input/ScriptedShipController.cpp
    src/headless/MatchResult.cpp
    src/headless/MatchRunner.cpp
)
target_include_directories(astroparty_sim PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(astroparty_sim PUBLIC astroparty_warnings astroparty_sanitizers nlohmann_json::nlohmann_json)

# --- SDL2 dependency -------------------------------------------------------
# Resolved via find_package only; no hardcoded Homebrew/vcpkg/system paths
# here. On a system where SDL2's CMake config files are not on the default
# search path (common with Homebrew on Apple Silicon), point CMake at them
# with, e.g., -DCMAKE_PREFIX_PATH="$(brew --prefix sdl2)" -- see README.md.
find_package(SDL2 REQUIRED)

if(NOT TARGET SDL2::SDL2)
    # Older SDL2 installations only provide module-mode variables
    # (SDL2_INCLUDE_DIRS / SDL2_LIBRARIES), not a CMake target. Wrap them in
    # an imported target so the rest of this file can depend on SDL2
    # uniformly either way.
    add_library(SDL2::SDL2 INTERFACE IMPORTED)
    target_include_directories(SDL2::SDL2 INTERFACE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(SDL2::SDL2 INTERFACE ${SDL2_LIBRARIES})
endif()

# --- Platform library: rendering and input, the only layers that touch SDL.-
add_library(astroparty_platform STATIC
    src/rendering/SdlLibraryGuard.cpp
    src/rendering/SdlWindow.cpp
    src/rendering/SdlRenderer.cpp
    src/rendering/GameRenderer.cpp
    src/input/KeyboardShipController.cpp
)
target_include_directories(astroparty_platform PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(astroparty_platform
    PUBLIC astroparty_sim SDL2::SDL2
    PRIVATE astroparty_warnings astroparty_sanitizers
)
if(TARGET SDL2::SDL2main)
    target_link_libraries(astroparty_platform PUBLIC SDL2::SDL2main)
endif()

# --- Executable -------------------------------------------------------------
add_executable(AstroPartyAI
    src/main.cpp
    src/Application.cpp
)
target_include_directories(AstroPartyAI PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(AstroPartyAI PRIVATE astroparty_platform astroparty_warnings astroparty_sanitizers)

# --- Headless executable ----------------------------------------------------
# Links astroparty_sim only -- no astroparty_platform, no SDL2 -- so it can
# run a full automated match with no display attached (CI, batch evaluation
# scripts). See docs/ARCHITECTURE.md for its JSON result format.
add_executable(AstroPartyAI-headless src/headless_main.cpp)
target_link_libraries(AstroPartyAI-headless PRIVATE astroparty_sim astroparty_warnings astroparty_sanitizers)

if(ASTROPARTY_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
