#include "Map.h"
#include "Constants.h"
#include <random>
#include <queue>
#include <unordered_set>
#include <algorithm>
#include <cstdint>

Map::Map(int width, int height)
    : m_width(width)
    , m_height(height)
{
    // Initialize tiles
    m_tiles.resize(height);
    for (int y = 0; y < height; ++y) {
        m_tiles[y].resize(width);
    }
    
    generateRandomMap();
    buildVertexArray();
}

void Map::render(sf::RenderTarget& target, const sf::View& camera) {
    target.draw(m_tileVertices);
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
                m_tiles[tileY + y][tileX + x].type = TileType::Ground;
                m_tiles[tileY + y][tileX + x].walkable = true;
                m_tiles[tileY + y][tileX + x].buildable = true;
                m_tiles[tileY + y][tileX + x].occupant = nullptr;
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

void Map::generateRandomMap() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> blockDist(0, 100);
    
    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            // Add some random obstacles (5% chance)
            if (blockDist(gen) < 5) {
                m_tiles[y][x].type = TileType::Blocked;
                m_tiles[y][x].walkable = false;
                m_tiles[y][x].buildable = false;
            }
        }
    }
    
    // Clear starting areas (corners)
    int clearRadius = 10;
    
    // Player start area (top-left)
    for (int y = 0; y < clearRadius; ++y) {
        for (int x = 0; x < clearRadius; ++x) {
            m_tiles[y][x].type = TileType::Ground;
            m_tiles[y][x].walkable = true;
            m_tiles[y][x].buildable = true;
        }
    }
    
    // Enemy start area (bottom-right)
    for (int y = m_height - clearRadius; y < m_height; ++y) {
        for (int x = m_width - clearRadius; x < m_width; ++x) {
            m_tiles[y][x].type = TileType::Ground;
            m_tiles[y][x].walkable = true;
            m_tiles[y][x].buildable = true;
        }
    }
}

void Map::addMineralPatches(std::vector<sf::Vector2f>& mineralPositions) {
    // This is called by the game to register mineral positions
    for (const auto& pos : mineralPositions) {
        sf::Vector2i tile = worldToTile(pos);
        if (isValidTile(tile.x, tile.y)) {
            m_tiles[tile.y][tile.x].type = TileType::Resource;
            m_tiles[tile.y][tile.x].buildable = false;
        }
    }
}

void Map::buildVertexArray() {
    m_tileVertices.setPrimitiveType(sf::PrimitiveType::Triangles);
    m_tileVertices.resize(m_width * m_height * 6);  // 2 triangles per tile = 6 vertices
    
    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            int index = (y * m_width + x) * 6;
            
            float px = static_cast<float>(x * Constants::TILE_SIZE);
            float py = static_cast<float>(y * Constants::TILE_SIZE);
            float ts = static_cast<float>(Constants::TILE_SIZE);
            
            // First triangle (top-left, top-right, bottom-right)
            m_tileVertices[index + 0].position = sf::Vector2f(px, py);
            m_tileVertices[index + 1].position = sf::Vector2f(px + ts, py);
            m_tileVertices[index + 2].position = sf::Vector2f(px + ts, py + ts);
            
            // Second triangle (top-left, bottom-right, bottom-left)
            m_tileVertices[index + 3].position = sf::Vector2f(px, py);
            m_tileVertices[index + 4].position = sf::Vector2f(px + ts, py + ts);
            m_tileVertices[index + 5].position = sf::Vector2f(px, py + ts);
            
            // Color based on tile type
            sf::Color tileColor;
            switch (m_tiles[y][x].type) {
                case TileType::Ground:
                    tileColor = sf::Color(34, 139, 34);  // Forest green
                    break;
                case TileType::Blocked:
                    tileColor = sf::Color(80, 80, 80);   // Dark gray
                    break;
                case TileType::Resource:
                    tileColor = sf::Color(50, 150, 50);  // Slightly different green
                    break;
                default:
                    tileColor = sf::Color(34, 139, 34);
                    break;
            }
            
            // Add some variation
            int variation = ((x + y) % 2) * 10;
            tileColor.r = static_cast<std::uint8_t>(std::min(255, tileColor.r + variation));
            tileColor.g = static_cast<std::uint8_t>(std::min(255, tileColor.g + variation));
            tileColor.b = static_cast<std::uint8_t>(std::min(255, tileColor.b + variation));
            
            // Set color for all 6 vertices
            for (int i = 0; i < 6; ++i) {
                m_tileVertices[index + i].color = tileColor;
            }
        }
    }
}

float Map::heuristic(int x1, int y1, int x2, int y2) const {
    // Manhattan distance
    return static_cast<float>(std::abs(x2 - x1) + std::abs(y2 - y1));
}
