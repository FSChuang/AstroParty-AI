# Contributing to AstroParty-AI

Thanks for your interest in the project. This document covers the practical
workflow for submitting a change; see `README.md` for what the project is
and how to build it, and `docs/ARCHITECTURE.md` for how it's organized.

## Before you start

1. Build the project and run the full test suite locally (see `README.md`'s
   "Installation / build" and "Testing" sections). Start from a passing
   baseline.
2. Read `docs/ARCHITECTURE.md`, at least the sections relevant to the area
   you're changing — the simulation/rendering separation and the controller
   interface are load-bearing design decisions, not incidental structure.

## Making a change

- Work on a feature branch; open a pull request against `main`.
- `main` is protected: every PR must pass the **Build & Test** CI check
  (Ubuntu and macOS) and receive at least one approving review before it
  can merge. Force-pushes and deletions of `main` are disabled at the
  repository level.
- Keep commits focused — prefer several small, clearly-described commits
  over one large one that mixes unrelated changes.
- Add or update tests for any behavior change. The simulation
  (`astroparty_sim`) has no SDL dependency specifically so that its logic
  can be exercised in plain unit tests with no window — new core/entities/physics/headless
  tests should not require a display, and CI will fail if they try to open one.

## Code style

This project follows Robert C. Martin's *Clean Code* principles throughout:
meaningful names, single-responsibility functions and classes, RAII and
const-correctness, minimal coupling, and no global mutable simulation
state. Comments should explain *why* a non-obvious decision was made
(an invariant, a physics convention, an intentional deviation from the
reference implementation), not restate what the code already says.

Avoid introducing a dependency, design pattern, or abstraction (an ECS,
a behavior-tree framework, an inheritance hierarchy) unless the current
code genuinely needs it — several design docs in this repo explicitly
record *why* a simpler alternative was chosen over a more general one.

## Questions

Open an issue if something in the architecture or build process is unclear
before investing time in a larger change.
