#include "Map.h"
#include "Constants.h"
#include <random>

Map::Map(int width, int height)
    : m_width(width)
    , m_height(height)
{
    std::random_device rd;
    m_rng = std::mt19937(rd());

    std::uniform_int_distribution<int> grassDist(1, 8);
    m_tiles.resize(height);
    for (int y = 0; y < height; ++y) {
        m_tiles[y].resize(width);
        for (int x = 0; x < width; ++x)
            m_tiles[y][x].variant = static_cast<uint8_t>(grassDist(m_rng));
    }
}

// Delegates to TerrainRenderer.
void Map::render(sf::RenderTarget& target, const sf::View& camera) {
    m_terrainRenderer.render(target, camera, *this);
}

// ---- Tile access -----------------------------------------------------------

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

// ---- Coordinate conversion -------------------------------------------------

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

// ---- Building placement ----------------------------------------------------

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

void Map::placeBuilding(int tileX, int tileY, int width, int height) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (isValidTile(tileX + x, tileY + y)) {
                m_tiles[tileY + y][tileX + x].type     = TileType::Building;
                m_tiles[tileY + y][tileX + x].walkable  = false;
                m_tiles[tileY + y][tileX + x].buildable = false;
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
            }
        }
    }
}

// ---- Pathfinding -- delegates to Pathfinder --------------------------------

std::vector<sf::Vector2f> Map::findPath(sf::Vector2f start, sf::Vector2f end,
                                         float unitRadius,
                                         const std::unordered_map<int, float>& extraTileCosts) {
    return m_pathfinder.findPath(*this, start, end, unitRadius, extraTileCosts);
}

// ---- Editor ----------------------------------------------------------------

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
    tile.variant   = variant;
}
