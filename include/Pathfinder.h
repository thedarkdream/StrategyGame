#pragma once

#include <SFML/System/Vector2.hpp>
#include <vector>
#include <unordered_map>

// Forward declaration – Pathfinder reads the map via its public const API only.
class Map;

// ---------------------------------------------------------------------------
// Pathfinder
//
// Encapsulates all pathfinding logic extracted from Map:
//   • A* search with 8-directional movement, clearance penalty, soft tile costs
//   • Supercover-DDA line-of-sight check
//   • String-pulling path smoother
//
// All methods are stateless with respect to the map: the caller passes a
// const Map reference so Pathfinder can be a plain value member of Map with
// no dangling-reference concerns on move/copy-assign.
// ---------------------------------------------------------------------------
class Pathfinder {
public:
    Pathfinder() = default;

    // Find a navigable path from [start] to [end] on [map].
    // unitRadius  : the unit's collision radius in pixels.  Non-zero values
    //               activate clearance-aware neighbour pruning and a fat-tube
    //               LOS check in smoothPath so the path is safe to traverse
    //               without clipping building corners.
    // extraTileCosts : optional map of (tileY*mapWidth + tileX) → extra A*
    //               g-cost; used by unit-aware replanning to steer around crowds.
    std::vector<sf::Vector2f> findPath(const Map& map,
                                       sf::Vector2f start, sf::Vector2f end,
                                       float unitRadius = 0.0f,
                                       const std::unordered_map<int, float>& extraTileCosts = {});

private:
    // Returns true if there is a clear tile-based line of sight between two tiles.
    bool hasLineOfSight(const Map& map,
                        int x0, int y0, int x1, int y1,
                        float unitRadius = 0.0f) const;

    // Remove redundant waypoints via string-pulling.
    std::vector<sf::Vector2f> smoothPath(const Map& map,
                                         const std::vector<sf::Vector2f>& path,
                                         float unitRadius = 0.0f) const;

    // Manhattan-distance heuristic.
    float heuristic(int x1, int y1, int x2, int y2) const;
};
