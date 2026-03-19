#include "Pathfinder.h"
#include "Map.h"
#include "Constants.h"
#include <queue>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <cmath>

std::vector<sf::Vector2f> Pathfinder::findPath(const Map& map,
                                                sf::Vector2f start, sf::Vector2f end,
                                                float unitRadius,
                                                const std::unordered_map<int, float>& extraTileCosts) {
    sf::Vector2i startTile = map.worldToTile(start);
    sf::Vector2i endTile   = map.worldToTile(end);

    // If start or end is outside the map, return direct path
    if (!map.isValidTile(startTile.x, startTile.y) || !map.isValidTile(endTile.x, endTile.y)) {
        return { end };
    }

    // If the unit has somehow ended up on a non-walkable tile (e.g. pushed onto
    // water by RVO), snap the A* start to the nearest walkable neighbour so the
    // reconstructed path begins on solid ground and smoothPath doesn't build LOS
    // lines that originate inside water.
    if (!map.isWalkable(startTile.x, startTile.y)) {
        sf::Vector2i bestStart(-1, -1);
        float bestDist = std::numeric_limits<float>::max();
        for (int radius = 1; radius <= 5; ++radius) {
            for (int dx = -radius; dx <= radius; ++dx) {
                for (int dy = -radius; dy <= radius; ++dy) {
                    if (std::abs(dx) != radius && std::abs(dy) != radius) continue;
                    int nx = startTile.x + dx;
                    int ny = startTile.y + dy;
                    if (!map.isValidTile(nx, ny) || !map.isWalkable(nx, ny)) continue;
                    float d = static_cast<float>(dx * dx + dy * dy);
                    if (d < bestDist) { bestDist = d; bestStart = { nx, ny }; }
                }
            }
            if (bestStart.x != -1) break;
        }
        if (bestStart.x == -1) return { end };
        startTile = bestStart;
        start = map.tileToWorldCenter(startTile.x, startTile.y);
    }

    // Remember whether the destination itself was set on a walkable tile so we
    // can later replace the last tile-centre waypoint with the exact world
    // position the caller specified.
    bool endOnWalkable = map.isWalkable(endTile.x, endTile.y);

    // If end is not walkable, find the nearest walkable tile to the *start*
    // so units approach from whichever side they are already on.
    if (!endOnWalkable) {
        sf::Vector2i bestTile(-1, -1);
        float bestDist = std::numeric_limits<float>::max();
        bool found = false;
        for (int radius = 1; radius <= 10 && !found; ++radius) {
            for (int dx = -radius; dx <= radius; ++dx) {
                for (int dy = -radius; dy <= radius; ++dy) {
                    if (std::abs(dx) != radius && std::abs(dy) != radius) continue;
                    int nx = endTile.x + dx;
                    int ny = endTile.y + dy;
                    if (!map.isValidTile(nx, ny) || !map.isWalkable(nx, ny)) continue;
                    float d = static_cast<float>((nx - startTile.x) * (nx - startTile.x)
                                               + (ny - startTile.y) * (ny - startTile.y));
                    if (d < bestDist) {
                        bestDist = d;
                        bestTile = sf::Vector2i(nx, ny);
                        found = true;
                    }
                }
            }
            if (found) break;
        }
        if (!found) {
            return { end };
        }
        endTile = bestTile;
    }

    // If start == end, no path needed.
    if (startTile == endTile) {
        return { endOnWalkable ? end : map.tileToWorldCenter(endTile.x, endTile.y) };
    }

    // ── A* data structures ──────────────────────────────────────────────────
    struct Node {
        int x, y;
        float g, h;
        int parentX, parentY;
        float f() const { return g + h; }
    };

    auto hash = [&map](int x, int y) { return y * map.getWidth() + x; };

    std::vector<Node> openList;
    std::vector<bool> closed(map.getWidth() * map.getHeight(), false);
    std::vector<Node> allNodes(map.getWidth() * map.getHeight());

    Node startNode{
        startTile.x, startTile.y,
        0.0f, heuristic(startTile.x, startTile.y, endTile.x, endTile.y),
        -1, -1
    };
    openList.push_back(startNode);
    allNodes[hash(startTile.x, startTile.y)] = startNode;

    Node  bestClosedNode = startNode;
    float bestClosedH    = startNode.h;

    // Direction offsets (8-directional movement)
    const int   dx[]    = {  0, 1, 1,     1,  0, -1, -1, -1 };
    const int   dy[]    = { -1,-1, 0,     1,  1,  1,  0, -1 };
    const float costs[] = { 1.0f, 1.414f, 1.0f, 1.414f, 1.0f, 1.414f, 1.0f, 1.414f };

    while (!openList.empty()) {
        auto minIt = std::min_element(openList.begin(), openList.end(),
            [](const Node& a, const Node& b) { return a.f() < b.f(); });

        Node current = *minIt;
        openList.erase(minIt);

        int currentHash = hash(current.x, current.y);
        if (closed[currentHash]) continue;
        closed[currentHash] = true;

        if (current.h < bestClosedH) {
            bestClosedH    = current.h;
            bestClosedNode = current;
        }

        // Reached the goal — reconstruct path
        if (current.x == endTile.x && current.y == endTile.y) {
            std::vector<sf::Vector2f> path;
            int cx = current.x;
            int cy = current.y;

            while (cx != -1 && cy != -1) {
                path.push_back(map.tileToWorldCenter(cx, cy));
                Node& n = allNodes[hash(cx, cy)];
                int px = n.parentX;
                int py = n.parentY;
                cx = px;
                cy = py;
            }

            std::reverse(path.begin(), path.end());

            if (!path.empty())
                path.front() = start;
            if (endOnWalkable && !path.empty())
                path.back() = end;

            return smoothPath(map, path, unitRadius);
        }

        // Explore neighbours
        for (int i = 0; i < 8; ++i) {
            int nx = current.x + dx[i];
            int ny = current.y + dy[i];

            if (!map.isWalkable(nx, ny)) continue;

            int neighborHash = hash(nx, ny);
            if (closed[neighborHash]) continue;

            // Diagonal corner-cut guard.
            if (costs[i] > 1.0f) {
                bool blockedA = !map.isWalkable(current.x + dx[i], current.y);
                bool blockedB = !map.isWalkable(current.x, current.y + dy[i]);
                if (blockedA || blockedB) {
                    const float TS_F = static_cast<float>(Constants::TILE_SIZE);
                    bool canSqueeze = blockedA && blockedB
                                   && unitRadius > 0.0f
                                   && (unitRadius * 2.0f) < TS_F;
                    if (!canSqueeze) continue;
                }
            }

            // Soft clearance penalty: mildly prefer open tiles over wall-hugging ones.
            const int odx[] = {  0, 1, 0, -1 };
            const int ody[] = { -1, 0, 1,  0 };
            float clearancePenalty = 0.0f;
            for (int k = 0; k < 4; ++k) {
                if (!map.isWalkable(nx + odx[k], ny + ody[k]))
                    clearancePenalty += 0.5f;
            }
            // Extra cost for the corner-squeeze diagonal so it is a last resort.
            if (costs[i] > 1.0f
                && !map.isWalkable(current.x + dx[i], current.y)
                && !map.isWalkable(current.x, current.y + dy[i]))
            {
                clearancePenalty += 2.0f;
            }

            float newG = current.g + costs[i] + clearancePenalty;
            if (!extraTileCosts.empty()) {
                auto ecIt = extraTileCosts.find(neighborHash);
                if (ecIt != extraTileCosts.end())
                    newG += ecIt->second;
            }
            float newH = heuristic(nx, ny, endTile.x, endTile.y);

            Node& existing = allNodes[neighborHash];
            if (existing.g == 0 || newG < existing.g) {
                existing = Node{ nx, ny, newG, newH, current.x, current.y };
                openList.push_back(existing);
            }
        }
    }

    // No path found — return partial path to the closest explored tile.
    {
        std::vector<sf::Vector2f> partial;
        int cx = bestClosedNode.x;
        int cy = bestClosedNode.y;
        while (cx != -1 && cy != -1) {
            partial.push_back(map.tileToWorldCenter(cx, cy));
            Node& n = allNodes[hash(cx, cy)];
            int px = n.parentX;
            int py = n.parentY;
            cx = px;
            cy = py;
        }
        std::reverse(partial.begin(), partial.end());
        if (!partial.empty()) partial.front() = start;
        return smoothPath(map, partial, unitRadius);
    }
}

bool Pathfinder::hasLineOfSight(const Map& map,
                                 int x0, int y0, int x1, int y1,
                                 float /*unitRadius*/) const {
    // Supercover DDA: use |dx|+|dy| steps so every tile boundary crossed by
    // the line is sampled.
    int steps = std::abs(x1 - x0) + std::abs(y1 - y0);
    if (steps == 0) return true;

    for (int i = 1; i <= steps; ++i) {
        float t  = static_cast<float>(i) / static_cast<float>(steps);
        float fx = x0 + t * (x1 - x0);
        float fy = y0 + t * (y1 - y0);

        int txf = static_cast<int>(std::floor(fx));
        int tyf = static_cast<int>(std::floor(fy));
        int txc = static_cast<int>(std::ceil(fx));
        int tyc = static_cast<int>(std::ceil(fy));
        if (!map.isWalkable(txf, tyf)) return false;
        if (!map.isWalkable(txf, tyc)) return false;
        if (!map.isWalkable(txc, tyf)) return false;
        if (!map.isWalkable(txc, tyc)) return false;
    }
    return true;
}

std::vector<sf::Vector2f> Pathfinder::smoothPath(const Map& map,
                                                   const std::vector<sf::Vector2f>& path,
                                                   float unitRadius) const {
    if (path.size() <= 2) return path;

    std::vector<sf::Vector2f> result;
    result.push_back(path.front());

    size_t anchor = 0;

    while (anchor < path.size() - 1) {
        size_t farthest = anchor + 1;
        for (size_t j = path.size() - 1; j > anchor + 1; --j) {
            sf::Vector2i ta = map.worldToTile(path[anchor]);
            sf::Vector2i tb = map.worldToTile(path[j]);
            if (hasLineOfSight(map, ta.x, ta.y, tb.x, tb.y, unitRadius)) {
                farthest = j;
                break;
            }
        }
        result.push_back(path[farthest]);
        anchor = farthest;
    }

    return result;
}

float Pathfinder::heuristic(int x1, int y1, int x2, int y2) const {
    // Manhattan distance
    return static_cast<float>(std::abs(x2 - x1) + std::abs(y2 - y1));
}
