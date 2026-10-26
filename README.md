# AstroParty-AI

A from-scratch C++20 reimplementation of an Astro Party-style twin-stick
arcade shooter, built as an extensible platform for future multi-agent game
AI experiments (strategic decision-making, procedural content generation,
and automated evaluation).

This is **Milestone 1**: a minimal, fully playable, two-local-player game
with a simulation that is architecturally independent of rendering. No AI,
networking, or procedural generation is implemented yet — see "Planned AI
extensions" below.

## Project overview

AstroParty-AI is a clean-room C++20 rewrite, informed by (but not copied
from) [CAstroParty](https://github.com/kkysen/CAstroParty), a C/SDL2
reference implementation of the same game. A full audit of that reference
project — its architecture, build/runtime behavior, and a component-by-component
migration plan — lives in `astro-party-workspace/audit/`. See
`THIRD_PARTY_NOTICES.md` for the upstream license and provenance record, and
`docs/ARCHITECTURE.md` for how this project is organized and why.

## Technology stack

- **C++20** for all gameplay, simulation, and physics code
- **SDL2** for windowing, rendering, and keyboard input
- **CMake** (>= 3.21) as the build system
- **doctest** (fetched via CMake `FetchContent`) for unit testing, run through **CTest**
- **Python** (planned, not yet used) for future evaluation and ML experiments

## Build requirements

- A C++20 compiler (tested with Apple Clang 17 on macOS; GCC/Clang on Linux
  should also work)
- CMake 3.21 or newer
- SDL2 development headers and libraries
- Internet access the first time you configure the project (to fetch
  doctest for the test suite; see "Building without network access" below)

### Installing SDL2

```sh
# macOS (Homebrew)
brew install sdl2

# Debian/Ubuntu
sudo apt-get install libsdl2-dev

# Fedora
sudo dnf install SDL2-devel
```

## Build instructions

```sh
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
```

`CMakeLists.txt` locates SDL2 purely through `find_package(SDL2 REQUIRED)` —
it never hardcodes a Homebrew, vcpkg, or system path. If CMake cannot find
SDL2 automatically (most commonly on Apple Silicon with Homebrew, if your
shell doesn't already export `CMAKE_PREFIX_PATH`), point it at your SDL2
install explicitly:

```sh
cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix sdl2)"
```

### Build options

| Option | Default | Effect |
|---|---|---|
| `ASTROPARTY_BUILD_TESTS` | `ON` | Builds the `astroparty_tests` suite and registers it with CTest |
| `ASTROPARTY_WARNINGS_AS_ERRORS` | `OFF` | Turns `-Wall -Wextra -Wpedantic` into hard errors |
| `ASTROPARTY_ENABLE_SANITIZERS` | `OFF` | Builds with AddressSanitizer + UndefinedBehaviorSanitizer |

### Building without network access

The test suite depends on `doctest`, fetched automatically via CMake's
`FetchContent` the first time you configure with tests enabled. If you need
to configure offline, disable the test suite:

```sh
cmake -B build -DASTROPARTY_BUILD_TESTS=OFF
```

## How to run the game

```sh
./build/AstroPartyAI
```

Run it from the repository root, or from any working directory — the game
has no dependency on the current working directory (unlike the upstream
reference project, which required a path-relative `textures/` folder).

## Keyboard controls

| Player | Turn right | Shoot |
|---|---|---|
| Player 1 (blue) | `D` | `W` |
| Player 2 (red) | Right Arrow | Up Arrow |

| Action | Key |
|---|---|
| Restart the match | `R` |
| Quit | `Esc`, or close the window |

Both ships accelerate continuously and automatically — there is no "stop" or
"reverse" control, matching the original Astro Party mechanic: steering
(turning right) is your only way to avoid bullets and line up a shot.

The match outcome ("Player 1 wins!", "Draw!") and control reminder are shown
in the window's title bar.

## Current features (Milestone 1)

- Resizable-free, fixed 1280x720 SDL2 window with proper RAII-managed
  initialization and shutdown
- Fixed-timestep simulation (60 Hz), fully decoupled from render rate
- Two independently, locally keyboard-controlled ships
- Continuous forward acceleration, turning, and a max-speed clamp
- Edge-triggered shooting (one bullet per key press, not per frame held)
- Bullet movement and lifetime expiration
- Circular collision detection between bullets and ships (no self-damage)
- Ship elimination, match end detection (win or draw), and manual restart
- Simulation (`astroparty_sim`) has zero SDL dependency and is independently
  unit-tested

## Planned AI extensions (not yet implemented)

These are explicitly **out of scope** for this milestone; they are listed
here so expectations are clear, and detailed further in
`astro-party-workspace/audit/06_implementation_roadmap.md`:

- AI-controlled ships via the existing `input::IShipController` interface
- Headless (no-window) batch simulation for automated matches
- Deterministic, seeded simulation for reproducible experiments
- Team-based play, resource collection, and dynamic role assignment
- Procedurally generated arenas with validity checks
- Python-based evaluation tooling and statistics
- Multiplayer networking (deliberately deferred; see `docs/ARCHITECTURE.md`)

## Testing instructions

```sh
cmake -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

To build and run with sanitizers (recommended before trusting a change to
core simulation code):

```sh
cmake -B build-sanitized -DCMAKE_BUILD_TYPE=Debug -DASTROPARTY_ENABLE_SANITIZERS=ON
cmake --build build-sanitized -j
ctest --test-dir build-sanitized --output-on-failure
```

**Known environment limitation:** on macOS 26 ("Tahoe") with Apple Clang 17,
AddressSanitizer's own shadow-memory initialization hangs before `main()`
runs — confirmed with `sample` to be stuck inside
`__asan::InitializeShadowMemory()`, i.e. a toolchain/OS issue unrelated to
this project's code. `ASTROPARTY_ENABLE_SANITIZERS` enables both ASan and
UBSan together via `-fsanitize=address,undefined`; if it hangs on your
machine the same way, test with UBSan alone instead:

```sh
cmake -B build-ubsan -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-fsanitize=undefined -fno-omit-frame-pointer" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=undefined"
cmake --build build-ubsan -j
ctest --test-dir build-ubsan --output-on-failure
```

All 36 test cases (867 assertions) pass cleanly under UBSan. Memory safety
was additionally checked with macOS's `leaks` tool as an ASan substitute
(`leaks --atExit -- ./build/tests/astroparty_tests`), which reported **0
leaks for 0 total leaked bytes**.

The simulation (`astroparty_sim`) has no SDL dependency, so the entire test
suite runs with no window, no display, and no input device required —
suitable for CI.
