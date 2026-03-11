#include "Map.h"
#include "Constants.h"
#include <queue>
#include <unordered_set>
#include <algorithm>
#include <cstdint>
#include <random>
#include <iostream>

Map::Map(int width, int height)
    : m_width(width)
    , m_height(height)
{
    std::random_device rd;
    m_rng = std::mt19937(rd());

    loadTerrainTextures();

    std::uniform_int_distribution<int> grassDist(1, 8);
    m_tiles.resize(height);
    for (int y = 0; y < height; ++y) {
        m_tiles[y].resize(width);
        for (int x = 0; x < width; ++x)
            m_tiles[y][x].variant = static_cast<uint8_t>(grassDist(m_rng));
    }
}

void Map::render(sf::RenderTarget& target, const sf::View& camera) {
    // Only draw tiles visible within the current camera view
    sf::Vector2f topLeft = camera.getCenter() - camera.getSize() / 2.f;
    const float ts    = static_cast<float>(Constants::TILE_SIZE);
    const float scale = ts / 64.f;  // source textures are 64×64, tiles are 32×32

    int startX = std::max(0, static_cast<int>(topLeft.x / ts));
    int startY = std::max(0, static_cast<int>(topLeft.y / ts));
    int endX   = std::min(m_width,  static_cast<int>((topLeft.x + camera.getSize().x) / ts) + 2);
    int endY   = std::min(m_height, static_cast<int>((topLeft.y + camera.getSize().y) / ts) + 2);

    for (int y = startY; y < endY; ++y) {
        for (int x = startX; x < endX; ++x) {
            const Tile& tile = m_tiles[y][x];
            uint8_t v = tile.variant;
            const sf::Texture* tex = nullptr;

            if (tile.type == TileType::Grass && v >= 1 && v <= 8)
                tex = &m_grassTextures[v - 1];
            else if (tile.type == TileType::Water && v >= 1 && v <= 4)
                tex = &m_waterTextures[v - 1];
            else
                tex = &m_grassTextures[0];  // Resource/Building tiles use grass base

            sf::Sprite sprite(*tex);
            sprite.setScale(sf::Vector2f(scale, scale));
            sprite.setPosition(sf::Vector2f(x * ts, y * ts));
            target.draw(sprite);
        }
    }
}

Tile& Map::getTile(int x, int y) {
    static Tile invalidTile;
    if (!isValidTile(x, y)) return invalidTile;
    return m_tiles[y][x];
}

const Tile& Map::getTile(int x, int y) const {
    static Tile invalidTile;
    if (!isValidTile(x, y)) return invalidTile;
    return m_tiles[y][x];
}

Tile& Map::getTileAtPosition(sf::Vector2f position) {
    sf::Vector2i tileCoord = worldToTile(position);
    return getTile(tileCoord.x, tileCoord.y);
}

bool Map::isValidTile(int x, int y) const {
    return x >= 0 && x < m_width && y >= 0 && y < m_height;
}

bool Map::isWalkable(int x, int y) const {
    if (!isValidTile(x, y)) return false;
    return m_tiles[y][x].walkable;
}

bool Map::isBuildable(int x, int y) const {
    if (!isValidTile(x, y)) return false;
    return m_tiles[y][x].buildable;
}

sf::Vector2i Map::worldToTile(sf::Vector2f worldPos) const {
    return sf::Vector2i(
        static_cast<int>(worldPos.x / Constants::TILE_SIZE),
        static_cast<int>(worldPos.y / Constants::TILE_SIZE)
    );
}

sf::Vector2f Map::tileToWorld(int x, int y) const {
    return sf::Vector2f(
        static_cast<float>(x * Constants::TILE_SIZE),
        static_cast<float>(y * Constants::TILE_SIZE)
    );
}

sf::Vector2f Map::tileToWorldCenter(int x, int y) const {
    return sf::Vector2f(
        x * Constants::TILE_SIZE + Constants::TILE_SIZE / 2.0f,
        y * Constants::TILE_SIZE + Constants::TILE_SIZE / 2.0f
    );
}

bool Map::canPlaceBuilding(int tileX, int tileY, int width, int height) const {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (!isBuildable(tileX + x, tileY + y)) {
                return false;
            }
        }
    }
    return true;
}

void Map::placeBuilding(int tileX, int tileY, int width, int height, EntityPtr building) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (isValidTile(tileX + x, tileY + y)) {
                m_tiles[tileY + y][tileX + x].type = TileType::Building;
                m_tiles[tileY + y][tileX + x].walkable = false;
                m_tiles[tileY + y][tileX + x].buildable = false;
                m_tiles[tileY + y][tileX + x].occupant = building;
            }
        }
    }
}

void Map::removeBuilding(int tileX, int tileY, int width, int height) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (isValidTile(tileX + x, tileY + y)) {
                m_tiles[tileY + y][tileX + x].type     = TileType::Grass;
                m_tiles[tileY + y][tileX + x].walkable  = true;
                m_tiles[tileY + y][tileX + x].buildable = true;
                m_tiles[tileY + y][tileX + x].occupant  = nullptr;
            }
        }
    }
}

std::vector<sf::Vector2f> Map::findPath(sf::Vector2f start, sf::Vector2f end) {
    // A* pathfinding
    sf::Vector2i startTile = worldToTile(start);
    sf::Vector2i endTile = worldToTile(end);
    
    // If start or end is invalid, return direct path
    if (!isValidTile(startTile.x, startTile.y) || !isValidTile(endTile.x, endTile.y)) {
        return { end };
    }
    
    // If end is not walkable, find nearest walkable tile
    if (!isWalkable(endTile.x, endTile.y)) {
        // Search in expanding squares for a walkable tile
        bool found = false;
        for (int radius = 1; radius <= 10 && !found; ++radius) {
            for (int dx = -radius; dx <= radius && !found; ++dx) {
                for (int dy = -radius; dy <= radius && !found; ++dy) {
                    if (std::abs(dx) == radius || std::abs(dy) == radius) {
                        int nx = endTile.x + dx;
                        int ny = endTile.y + dy;
                        if (isWalkable(nx, ny)) {
                            endTile = sf::Vector2i(nx, ny);
                            found = true;
                        }
                    }
                }
            }
        }
        if (!found) {
            return { end };  // No walkable tile found, return direct path
        }
    }
    
    // If start == end, no path needed
    if (startTile == endTile) {
        return { tileToWorldCenter(endTile.x, endTile.y) };
    }
    
    // A* data structures
    struct Node {
        int x, y;
        float g, h;
        int parentX, parentY;
        float f() const { return g + h; }
    };
    
    auto hash = [this](int x, int y) { return y * m_width + x; };
    
    std::vector<Node> openList;
    std::vector<bool> closed(m_width * m_height, false);
    std::vector<Node> allNodes(m_width * m_height);
    
    // Initialize start node
    Node startNode{ startTile.x, startTile.y, 0.0f, heuristic(startTile.x, startTile.y, endTile.x, endTile.y), -1, -1 };
    openList.push_back(startNode);
    allNodes[hash(startTile.x, startTile.y)] = startNode;
    
    // Direction offsets (8-directional movement)
    const int dx[] = { 0, 1, 1, 1, 0, -1, -1, -1 };
    const int dy[] = { -1, -1, 0, 1, 1, 1, 0, -1 };
    const float costs[] = { 1.0f, 1.414f, 1.0f, 1.414f, 1.0f, 1.414f, 1.0f, 1.414f };
    
    while (!openList.empty()) {
        // Find node with lowest f score
        auto minIt = std::min_element(openList.begin(), openList.end(),
            [](const Node& a, const Node& b) { return a.f() < b.f(); });
        
        Node current = *minIt;
        openList.erase(minIt);
        
        int currentHash = hash(current.x, current.y);
        if (closed[currentHash]) continue;
        closed[currentHash] = true;
        
        // Check if we reached the goal
        if (current.x == endTile.x && current.y == endTile.y) {
            // Reconstruct path
            std::vector<sf::Vector2f> path;
            int cx = current.x;
            int cy = current.y;
            
            while (cx != -1 && cy != -1) {
                path.push_back(tileToWorldCenter(cx, cy));
                Node& n = allNodes[hash(cx, cy)];
                int px = n.parentX;
                int py = n.parentY;
                cx = px;
                cy = py;
            }
            
            // Reverse to get start-to-end order, then remove start position
            std::reverse(path.begin(), path.end());
            if (!path.empty()) {
                path.erase(path.begin());  // Remove start position
            }

            return smoothPath(path);
        }
        
        // Explore neighbors
        for (int i = 0; i < 8; ++i) {
            int nx = current.x + dx[i];
            int ny = current.y + dy[i];
            
            if (!isWalkable(nx, ny)) continue;
            
            int neighborHash = hash(nx, ny);
            if (closed[neighborHash]) continue;
            
            // For diagonal movement, check that we're not cutting corners
            if (costs[i] > 1.0f) {
                if (!isWalkable(current.x + dx[i], current.y) || 
                    !isWalkable(current.x, current.y + dy[i])) {
                    continue;  // Can't cut corners
                }
            }

            // Clearance penalty: strongly prefer paths away from walls.
            // The penalty is quadratic in the number of non-walkable orthogonal
            // neighbours so that tight junctions between two adjacent buildings
            // become very expensive relative to a longer detour:
            //   1 wall neighbour  → +1.5
            //   2 wall neighbours → +6.0
            //   3 wall neighbours → +13.5
            // A pinch point (walls on both opposite sides of the tile, i.e. a
            // corridor exactly 1 tile wide) gets an additional +8.0 penalty,
            // making A* strongly prefer routing around the cluster instead.
            const int odx[] = { 0, 1, 0, -1 };
            const int ody[] = { -1, 0, 1,  0 };
            int wallNeighbors = 0;
            for (int k = 0; k < 4; ++k) {
                if (!isWalkable(nx + odx[k], ny + ody[k]))
                    ++wallNeighbors;
            }
            float clearancePenalty = static_cast<float>(wallNeighbors * wallNeighbors) * 1.5f;
            // Extra hit for pinch points: walls on both left+right or top+bottom
            bool hPinch = !isWalkable(nx - 1, ny) && !isWalkable(nx + 1, ny);
            bool vPinch = !isWalkable(nx, ny - 1) && !isWalkable(nx, ny + 1);
            if (hPinch || vPinch) clearancePenalty += 8.0f;

            float newG = current.g + costs[i] + clearancePenalty;
            float newH = heuristic(nx, ny, endTile.x, endTile.y);
            
            // Check if this path is better
            Node& existing = allNodes[neighborHash];
            if (existing.g == 0 || newG < existing.g) {
                existing = Node{ nx, ny, newG, newH, current.x, current.y };
                openList.push_back(existing);
            }
        }
    }
    
    // No path found, return direct path
    return { end };
}

bool Map::hasLineOfSight(int x0, int y0, int x1, int y1) const {
    // Walk along the line from (x0,y0) to (x1,y1) using a supercover approach.
    // At each step we check the primary tile *and* the two neighbouring tiles that
    // are orthogonally adjacent to the line. This makes the check conservative
    // enough that a unit with non-zero radius won't clip a corner even though the
    // line itself technically cleared it.
    int steps = std::max(std::abs(x1 - x0), std::abs(y1 - y0));
    if (steps == 0) return true;

    for (int i = 1; i <= steps; ++i) {
        float t  = static_cast<float>(i) / static_cast<float>(steps);
        float fx = x0 + t * (x1 - x0);
        float fy = y0 + t * (y1 - y0);

        // Primary tile (round)
        int tx = static_cast<int>(std::round(fx));
        int ty = static_cast<int>(std::round(fy));
        if (!isWalkable(tx, ty)) return false;

        // Conservative side tiles: check floor and ceil on each axis
        int txf = static_cast<int>(std::floor(fx));
        int tyf = static_cast<int>(std::floor(fy));
        int txc = static_cast<int>(std::ceil(fx));
        int tyc = static_cast<int>(std::ceil(fy));
        // Only the tiles that differ from the primary tile matter
        if ((txf != tx || tyf != ty) && !isWalkable(txf, tyf)) return false;
        if ((txc != tx || tyc != ty) && !isWalkable(txc, tyc)) return false;
    }
    return true;
}

std::vector<sf::Vector2f> Map::smoothPath(const std::vector<sf::Vector2f>& path) const {
    if (path.size() <= 2) return path;

    std::vector<sf::Vector2f> result;
    result.push_back(path.front());

    size_t anchor = 0;  // index of the last kept waypoint

    while (anchor < path.size() - 1) {
        // Try to jump as far forward as possible with direct LOS.
        size_t farthest = anchor + 1;
        for (size_t j = path.size() - 1; j > anchor + 1; --j) {
            sf::Vector2i ta = worldToTile(path[anchor]);
            sf::Vector2i tb = worldToTile(path[j]);
            if (hasLineOfSight(ta.x, ta.y, tb.x, tb.y)) {
                farthest = j;
                break;
            }
        }
        result.push_back(path[farthest]);
        anchor = farthest;
    }

    return result;
}

void Map::initEmpty() {
    std::uniform_int_distribution<int> grassDist(1, 8);
    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            m_tiles[y][x] = Tile{};  // Default: Grass, walkable, buildable
            m_tiles[y][x].variant = static_cast<uint8_t>(grassDist(m_rng));
        }
    }
}

void Map::setTileType(int x, int y, TileType type) {
    if (!isValidTile(x, y)) return;
    Tile& tile = m_tiles[y][x];
    tile.type      = type;
    tile.walkable  = (type == TileType::Grass || type == TileType::Resource);
    tile.buildable = (type == TileType::Grass);
    tile.occupant  = nullptr;
    // Assign a random variant for the new tile type
    if (type == TileType::Grass) {
        std::uniform_int_distribution<int> dist(1, 8);
        tile.variant = static_cast<uint8_t>(dist(m_rng));
    } else if (type == TileType::Water) {
        std::uniform_int_distribution<int> dist(1, 4);
        tile.variant = static_cast<uint8_t>(dist(m_rng));
    } else {
        tile.variant = 1;
    }
}

void Map::setTileType(int x, int y, TileType type, uint8_t variant) {
    if (!isValidTile(x, y)) return;
    Tile& tile = m_tiles[y][x];
    tile.type      = type;
    tile.walkable  = (type == TileType::Grass || type == TileType::Resource);
    tile.buildable = (type == TileType::Grass);
    tile.occupant  = nullptr;
    tile.variant   = variant;
}

void Map::loadTerrainTextures() {
    for (int i = 0; i < 8; ++i) {
        std::string path = "assets/terrain/grass_" + std::to_string(i + 1) + ".png";
        if (!m_grassTextures[i].loadFromFile(path))
            std::cerr << "Map: cannot load " << path << "\n";
    }
    for (int i = 0; i < 4; ++i) {
        std::string path = "assets/terrain/water_" + std::to_string(i + 1) + ".png";
        if (!m_waterTextures[i].loadFromFile(path))
            std::cerr << "Map: cannot load " << path << "\n";
    }
}

float Map::heuristic(int x1, int y1, int x2, int y2) const {
    // Manhattan distance
    return static_cast<float>(std::abs(x2 - x1) + std::abs(y2 - y1));
}
