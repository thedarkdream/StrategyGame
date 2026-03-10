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
            
            return path;
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
            
            float newG = current.g + costs[i];
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
