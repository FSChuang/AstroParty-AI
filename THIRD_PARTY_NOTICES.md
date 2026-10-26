# Third-Party Notices

## Upstream reference project: CAstroParty

AstroParty-AI's gameplay constants (ship acceleration, turn rate, max speed,
bullet speed and lifetime) were derived arithmetically from values read in
the source of **CAstroParty**, a C/SDL2 multiplayer Astro Party clone:

- Repository: https://github.com/kkysen/CAstroParty
- Commit referenced during this project's Phase 1 audit: `3b3a618`
- Authors: Khyber Sen, Adris Jautakas, Jawadul Kadir
- License: MIT (Copyright (c) 2017)

**No source files from CAstroParty are copied into this repository**, and no
CAstroParty texture/image assets are used here — AstroParty-AI renders with
SDL2 primitives only. Where a value or mechanic was carried over, it is
documented at its point of use (see `include/astroparty/core/SimulationConfig.hpp`
and `docs/ARCHITECTURE.md`) with an explicit reference back to
`docs/UPSTREAM_AUDIT.md`, this repository's condensed record of that audit.

The original CAstroParty MIT license text is reproduced below in full, per
its terms:

```
MIT License

Copyright (c) 2017 Khyber Sen, Adris Jautakas, and Jawadul Kadir

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## Third-party libraries used by AstroParty-AI

| Library | Purpose | License | How it's obtained |
|---|---|---|---|
| [SDL2](https://www.libsdl.org/) | Window, rendering, keyboard input | zlib | System package (not vendored) |
| [doctest](https://github.com/doctest/doctest) | Unit testing framework | MIT | Fetched at configure time via CMake `FetchContent` (not vendored) |

Neither library's source is vendored in this repository; both are resolved
by the build system from the environment or a pinned upstream tag.
