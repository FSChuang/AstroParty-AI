#pragma once

#include <optional>
#include <vector>

#include "astroparty/core/MapDefinition.hpp"
#include "astroparty/core/Vector2.hpp"

namespace astroparty::procgen {

// A coarse grid over an arena, used only to answer "is there room for a
// circle of some clearance radius to get from A to B at all" -- i.e.
// GEOMETRIC reachability, not motion-planning feasibility. A ship
// continuously accelerates and can only ever turn clockwise, so a grid path
// existing does NOT prove a ship can actually fly it; this class is
// deliberately scoped to the weaker, purely geometric question (see
// docs/PROCEDURAL_GENERATION.md's "Three notions of validity" section for
// why that distinction matters and is never blurred in this codebase).
//
// A cell is blocked if a circle of `clearanceRadius` centered at the cell
// could not fit there without touching an obstacle or leaving the arena --
// i.e. it models the SAME circular clearance obstacle collision already
// uses (physics::circlesOverlap), just discretized onto a grid for fast
// connectivity queries.
class OccupancyGrid {
public:
    OccupancyGrid(core::Vector2 arenaSize, const std::vector<core::ObstacleDefinition>& obstacles,
                   float clearanceRadius, float cellSize) noexcept;

    [[nodiscard]] int columns() const noexcept { return columns_; }
    [[nodiscard]] int rows() const noexcept { return rows_; }
    [[nodiscard]] float cellSize() const noexcept { return cellSize_; }
    [[nodiscard]] bool isBlocked(int column, int row) const noexcept;

    // True if `position` falls inside a free cell, or a free cell exists
    // within a small search radius of it (so a position exactly on a cell
    // boundary, or one cell's width off due to discretization, is not
    // spuriously treated as blocked).
    [[nodiscard]] bool isPositionUsable(core::Vector2 position) const noexcept;

    // True if a sequence of free, 8-connected cells links the cell
    // containing `from` to the cell containing `to`.
    [[nodiscard]] bool isReachable(core::Vector2 from, core::Vector2 to) const noexcept;

    // Shortest 8-connected path length between `from` and `to`, in world
    // units (orthogonal steps cost cellSize(), diagonal steps cost
    // cellSize() * sqrt(2)) -- a grid-resolution approximation of the
    // shortest traversable path, not an exact Euclidean geodesic. Returns
    // std::nullopt if unreachable.
    [[nodiscard]] std::optional<float> shortestPathLength(core::Vector2 from, core::Vector2 to) const noexcept;

    // The fraction of all free (non-blocked) cells reachable from `from`'s
    // cell -- 1.0 means the free space is one connected region; a lower
    // value means obstacles have walled off part of the arena into an
    // unreachable island.
    [[nodiscard]] float reachableFreeSpaceFraction(core::Vector2 from) const noexcept;

private:
    core::Vector2 arenaSize_;
    float clearanceRadius_;
    float cellSize_;
    int columns_;
    int rows_;
    std::vector<bool> blocked_;  // row-major, size columns_ * rows_.

    [[nodiscard]] int indexOf(int column, int row) const noexcept { return row * columns_ + column; }
    [[nodiscard]] std::optional<std::pair<int, int>> cellOfUsablePosition(core::Vector2 position) const noexcept;
    // Breadth-first distance (in grid steps, not world units) from
    // startCell to every cell; unreached or blocked cells read -1.
    [[nodiscard]] std::vector<float> bfsDistances(std::pair<int, int> startCell) const noexcept;
};

}  // namespace astroparty::procgen
