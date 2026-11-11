#include "astroparty/procgen/OccupancyGrid.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <queue>
#include <utility>

namespace astroparty::procgen {

using core::ObstacleDefinition;
using core::Vector2;

namespace {
constexpr float kSqrt2 = 1.41421356237f;
}  // namespace

OccupancyGrid::OccupancyGrid(Vector2 arenaSize, const std::vector<ObstacleDefinition>& obstacles,
                              float clearanceRadius, float cellSize) noexcept
    : arenaSize_(arenaSize),
      clearanceRadius_(clearanceRadius),
      cellSize_(cellSize),
      columns_(std::max(1, static_cast<int>(std::ceil(arenaSize.x / cellSize)))),
      rows_(std::max(1, static_cast<int>(std::ceil(arenaSize.y / cellSize)))) {
    blocked_.assign(static_cast<std::size_t>(columns_ * rows_), false);

    for (int row = 0; row < rows_; ++row) {
        for (int column = 0; column < columns_; ++column) {
            const Vector2 center{(static_cast<float>(column) + 0.5f) * cellSize_,
                                  (static_cast<float>(row) + 0.5f) * cellSize_};
            bool isBlockedCell = center.x < clearanceRadius_ || center.x > arenaSize_.x - clearanceRadius_ ||
                                 center.y < clearanceRadius_ || center.y > arenaSize_.y - clearanceRadius_;
            if (!isBlockedCell) {
                for (const ObstacleDefinition& obstacle : obstacles) {
                    const float combinedRadius = obstacle.radius + clearanceRadius_;
                    if (core::distanceSquared(center, obstacle.position) < combinedRadius * combinedRadius) {
                        isBlockedCell = true;
                        break;
                    }
                }
            }
            blocked_[static_cast<std::size_t>(indexOf(column, row))] = isBlockedCell;
        }
    }
}

bool OccupancyGrid::isBlocked(int column, int row) const noexcept {
    if (column < 0 || column >= columns_ || row < 0 || row >= rows_) {
        return true;  // Outside the grid counts as blocked -- there is no cell to occupy there.
    }
    return blocked_[static_cast<std::size_t>(indexOf(column, row))];
}

std::optional<std::pair<int, int>> OccupancyGrid::cellOfUsablePosition(Vector2 position) const noexcept {
    const int column = std::clamp(static_cast<int>(position.x / cellSize_), 0, columns_ - 1);
    const int row = std::clamp(static_cast<int>(position.y / cellSize_), 0, rows_ - 1);
    if (!isBlocked(column, row)) {
        return std::make_pair(column, row);
    }

    // The exact cell is blocked (e.g. position sits right at a clearance
    // boundary due to float rounding) -- look for the nearest free cell in
    // a small surrounding box rather than immediately giving up.
    constexpr int kSearchRadius = 3;
    for (int radius = 1; radius <= kSearchRadius; ++radius) {
        for (int dRow = -radius; dRow <= radius; ++dRow) {
            for (int dColumn = -radius; dColumn <= radius; ++dColumn) {
                if (std::max(std::abs(dRow), std::abs(dColumn)) != radius) {
                    continue;  // Only scan the expanding ring's edge, not cells already checked.
                }
                const int candidateColumn = column + dColumn;
                const int candidateRow = row + dRow;
                if (candidateColumn < 0 || candidateColumn >= columns_ || candidateRow < 0 || candidateRow >= rows_) {
                    continue;
                }
                if (!isBlocked(candidateColumn, candidateRow)) {
                    return std::make_pair(candidateColumn, candidateRow);
                }
            }
        }
    }
    return std::nullopt;
}

bool OccupancyGrid::isPositionUsable(Vector2 position) const noexcept { return cellOfUsablePosition(position).has_value(); }

std::vector<float> OccupancyGrid::bfsDistances(std::pair<int, int> startCell) const noexcept {
    std::vector<float> distances(static_cast<std::size_t>(columns_ * rows_), -1.0f);
    const auto [startColumn, startRow] = startCell;
    if (startColumn < 0 || startColumn >= columns_ || startRow < 0 || startRow >= rows_ ||
        isBlocked(startColumn, startRow)) {
        return distances;
    }

    using QueueEntry = std::pair<float, int>;  // (distance, cell index)
    std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<>> frontier;
    const int startIndex = indexOf(startColumn, startRow);
    distances[static_cast<std::size_t>(startIndex)] = 0.0f;
    frontier.push({0.0f, startIndex});

    static constexpr int kColumnOffsets[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
    static constexpr int kRowOffsets[8] = {-1, -1, -1, 0, 0, 1, 1, 1};

    while (!frontier.empty()) {
        const auto [distance, index] = frontier.top();
        frontier.pop();
        if (distance > distances[static_cast<std::size_t>(index)]) {
            continue;  // Stale entry superseded by a shorter path already found.
        }
        const int column = index % columns_;
        const int row = index / columns_;

        for (int direction = 0; direction < 8; ++direction) {
            const int neighborColumn = column + kColumnOffsets[direction];
            const int neighborRow = row + kRowOffsets[direction];
            if (neighborColumn < 0 || neighborColumn >= columns_ || neighborRow < 0 || neighborRow >= rows_) {
                continue;
            }
            if (isBlocked(neighborColumn, neighborRow)) {
                continue;
            }
            const bool isDiagonal = kColumnOffsets[direction] != 0 && kRowOffsets[direction] != 0;
            const float stepCost = isDiagonal ? cellSize_ * kSqrt2 : cellSize_;
            const int neighborIndex = indexOf(neighborColumn, neighborRow);
            const float candidateDistance = distance + stepCost;
            float& neighborDistance = distances[static_cast<std::size_t>(neighborIndex)];
            if (neighborDistance < 0.0f || candidateDistance < neighborDistance) {
                neighborDistance = candidateDistance;
                frontier.push({candidateDistance, neighborIndex});
            }
        }
    }
    return distances;
}

bool OccupancyGrid::isReachable(Vector2 from, Vector2 to) const noexcept {
    return shortestPathLength(from, to).has_value();
}

std::optional<float> OccupancyGrid::shortestPathLength(Vector2 from, Vector2 to) const noexcept {
    const auto fromCell = cellOfUsablePosition(from);
    const auto toCell = cellOfUsablePosition(to);
    if (!fromCell || !toCell) {
        return std::nullopt;
    }
    const std::vector<float> distances = bfsDistances(*fromCell);
    const float distance = distances[static_cast<std::size_t>(indexOf(toCell->first, toCell->second))];
    return distance < 0.0f ? std::optional<float>{} : std::optional<float>{distance};
}

float OccupancyGrid::reachableFreeSpaceFraction(Vector2 from) const noexcept {
    const auto fromCell = cellOfUsablePosition(from);
    if (!fromCell) {
        return 0.0f;
    }
    const std::vector<float> distances = bfsDistances(*fromCell);

    int totalFreeCells = 0;
    int reachableCells = 0;
    for (int row = 0; row < rows_; ++row) {
        for (int column = 0; column < columns_; ++column) {
            if (isBlocked(column, row)) {
                continue;
            }
            ++totalFreeCells;
            if (distances[static_cast<std::size_t>(indexOf(column, row))] >= 0.0f) {
                ++reachableCells;
            }
        }
    }
    return totalFreeCells == 0 ? 0.0f : static_cast<float>(reachableCells) / static_cast<float>(totalFreeCells);
}

}  // namespace astroparty::procgen
