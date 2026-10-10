#include "world/Map.h"
#include "core/Constants.h"
#include <algorithm>
#include <cmath>
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
    resetTerrainCache();
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
    const Tile& t = m_tiles[y][x];
    return t.walkable && !t.cliff;
}

bool Map::isBuildable(int x, int y) const {
    if (!isValidTile(x, y)) return false;
    const Tile& t = m_tiles[y][x];
    return t.buildable && t.flat;
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

// ---- Elevation (heightmap) -------------------------------------------------

int Map::getTileElevation(int x, int y) const {
    return isValidTile(x, y) ? m_tiles[y][x].elevation : 0;
}

void Map::setTileElevation(int x, int y, int level) {
    if (!isValidTile(x, y)) return;
    m_tiles[y][x].elevation = static_cast<uint8_t>(std::clamp(level, 0, Constants::MAX_ELEVATION));
    if (m_tiles[y][x].elevation > 0) m_hasElevation = true;
    recomputeTerrainAround(x, y);
}

void Map::resetTerrainCache() {
    m_vertexLevels.assign(static_cast<size_t>((m_width + 1) * (m_height + 1)), 0);
    m_hasElevation = false;
    for (auto& row : m_tiles)
        for (Tile& t : row) { t.cliff = false; t.flat = true; }
}

// Refreshes the cached vertex levels around tile (x, y) and the slope flags of
// every tile that touches one of those vertices.
void Map::recomputeTerrainAround(int tileX, int tileY) {
    for (int vy = tileY; vy <= tileY + 1; ++vy) {
        for (int vx = tileX; vx <= tileX + 1; ++vx) {
            int level = 0;
            for (int dy = -1; dy <= 0; ++dy)
                for (int dx = -1; dx <= 0; ++dx)
                    level = std::max(level, getTileElevation(vx + dx, vy + dy));
            m_vertexLevels[static_cast<size_t>(vy * (m_width + 1) + vx)] = static_cast<std::uint8_t>(level);
        }
    }
    for (int ty = tileY - 1; ty <= tileY + 1; ++ty) {
        for (int tx = tileX - 1; tx <= tileX + 1; ++tx) {
            if (!isValidTile(tx, ty)) continue;
            const int a = getVertexLevel(tx,     ty);
            const int b = getVertexLevel(tx + 1, ty);
            const int c = getVertexLevel(tx,     ty + 1);
            const int d = getVertexLevel(tx + 1, ty + 1);
            const int spread = std::max(std::max(a, b), std::max(c, d))
                             - std::min(std::min(a, b), std::min(c, d));
            m_tiles[ty][tx].cliff = spread >= Constants::CLIFF_LEVEL_DELTA;
            m_tiles[ty][tx].flat  = (spread == 0);
        }
    }
}

// Vertex (vx, vy) is the top-left corner of tile (vx, vy); it touches tiles
// (vx-1..vx, vy-1..vy).  Served from the cache maintained by recomputeTerrainAround.
int Map::getVertexLevel(int vx, int vy) const {
    vx = std::clamp(vx, 0, m_width);
    vy = std::clamp(vy, 0, m_height);
    return m_vertexLevels[static_cast<size_t>(vy * (m_width + 1) + vx)];
}

float Map::getVertexHeight(int vx, int vy) const {
    return static_cast<float>(getVertexLevel(vx, vy)) * Constants::ELEVATION_STEP;
}

float Map::getHeightAt(sf::Vector2f worldPos) const {
    const float ts = static_cast<float>(Constants::TILE_SIZE);
    const float fx = std::clamp(worldPos.x / ts, 0.0f, static_cast<float>(m_width));
    const float fy = std::clamp(worldPos.y / ts, 0.0f, static_cast<float>(m_height));
    const int   tx = std::min(static_cast<int>(fx), m_width  - 1);
    const int   ty = std::min(static_cast<int>(fy), m_height - 1);
    const float u  = fx - static_cast<float>(tx);
    const float v  = fy - static_cast<float>(ty);

    const float h00 = getVertexHeight(tx,     ty);
    const float h10 = getVertexHeight(tx + 1, ty);
    const float h01 = getVertexHeight(tx,     ty + 1);
    const float h11 = getVertexHeight(tx + 1, ty + 1);
    return (h00 * (1.0f - u) + h10 * u) * (1.0f - v)
         + (h01 * (1.0f - u) + h11 * u) * v;
}

bool Map::isCliffTile(int x, int y) const {
    return isValidTile(x, y) && m_tiles[y][x].cliff;
}

bool Map::hasLineOfSight(sf::Vector2f from, sf::Vector2f to) const {
    if (!m_hasElevation) return true;

    const float eye     = getHeightAt(from) + Constants::ELEVATION_STEP * 0.75f;
    const float target  = getHeightAt(to);
    const float slack   = Constants::ELEVATION_STEP * 0.5f;   // one level of relief never blocks
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float dist = std::sqrt(dx * dx + dy * dy);
    const int steps = static_cast<int>(dist / static_cast<float>(Constants::TILE_SIZE));

    for (int i = 1; i < steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const sf::Vector2f p(from.x + dx * t, from.y + dy * t);
        const float sightLine = eye + (target - eye) * t;
        if (getHeightAt(p) > sightLine + slack) return false;
    }
    return true;
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
    resetTerrainCache();
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
