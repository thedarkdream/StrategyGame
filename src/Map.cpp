#include "Map.h"
#include "Constants.h"
#include <queue>
#include <unordered_set>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <random>
#include <iostream>

Map::Map(int width, int height)
    : m_width(width)
    , m_height(height)
{
    std::random_device rd;
    m_rng = std::mt19937(rd());

    loadTerrainTextures();
    loadTransitionTextures();

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

    auto drawTex = [&](const sf::Texture& tex, int x, int y) {
        sf::Sprite sp(tex);
        sp.setScale(sf::Vector2f(scale, scale));
        sp.setPosition(sf::Vector2f(x * ts, y * ts));
        target.draw(sp);
    };

    for (int y = startY; y < endY; ++y) {
        for (int x = startX; x < endX; ++x) {
            const Tile& tile = m_tiles[y][x];

            // ── Water tiles: always drawn as-is ───────────────────────────────
            if (tile.type == TileType::Water) {
                uint8_t v = tile.variant;
                drawTex((v >= 1 && v <= 4) ? m_waterTextures[v - 1] : m_waterTextures[0], x, y);
                continue;
            }

            // ── Everything else (Grass / Resource / Building) ───────────────────
            // Step 1: check cardinal neighbours for water.
            const uint8_t cmask = cardinalWaterMask(x, y);

            if (cmask != 0) {
                // ── Cardinal transition tile ────────────────────────────────────
                // Look up by index (not raw pointer) so the lookup table
                // stays valid after Map is move-assigned during initialize().
                const int8_t idx = m_cardinalLookup[cmask];
                if (idx >= 0) {
                    drawTex(m_edgeTransTextures[static_cast<size_t>(idx)], x, y);
                } else {
                    // Missing tile – fall back to water so the gap is obvious
                    drawTex(m_grassTextures[0], x, y);
                }
            } else {
                // ── No cardinal water neighbours ──────────────────────────────
                // Check diagonal neighbours: if any diagonal is water, use
                // the corresponding inner-corner tile (complete tile image
                // with a small water notch at the offending corner).
                // If several diagonals are water simultaneously, priority:
                // SW (bit0) > SE (bit1) > NW (bit2) > NE (bit3).
                // For the uncommon multi-diagonal case draw the plain grass
                // base first, then overlay the first inner-corner match.
                uint8_t dmask = diagonalWaterMask(x, y);

                // Always draw the base grass tile first.
                uint8_t v = tile.variant;
                drawTex((v >= 1 && v <= 8) ? m_grassTextures[v - 1] : m_grassTextures[0], x, y);

                // Check for paired diagonal water (same-side pairs) first.
                // These produce a strip tile that covers both corners at once.
                // dmask bits: bit0=SW, bit1=SE, bit2=NW, bit3=NE
                if      ((dmask & 0x0C) == 0x0C) drawTex(m_stripTransTextures[0], x, y); // NW+NE → N strip
                else if ((dmask & 0x03) == 0x03) drawTex(m_stripTransTextures[1], x, y); // SW+SE → S strip
                else if ((dmask & 0x0A) == 0x0A) drawTex(m_stripTransTextures[2], x, y); // NE+SE → E strip
                else if ((dmask & 0x05) == 0x05) drawTex(m_stripTransTextures[3], x, y); // NW+SW → W strip
                // Single diagonal: small water notch at that corner.
                else if (dmask & 0x01) drawTex(m_innerTransTextures[0], x, y); // SW → water_grass_NE
                else if (dmask & 0x02) drawTex(m_innerTransTextures[1], x, y); // SE → water_grass_NW
                else if (dmask & 0x04) drawTex(m_innerTransTextures[2], x, y); // NW → water_grass_SE
                else if (dmask & 0x08) drawTex(m_innerTransTextures[3], x, y); // NE → water_grass_SW
            }
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
    
    // Remember whether the destination itself was set on a walkable tile so we
    // can later replace the last tile-centre waypoint with the exact world
    // position the caller specified.  Non-walkable ends (buildings, etc.) are
    // redirected to an adjacent tile; in that case we keep tile centres.
    bool endOnWalkable = isWalkable(endTile.x, endTile.y);

    // If end is not walkable, find the nearest walkable tile to the *start*
    // so workers/units approach from whichever side they are already on,
    // rather than always converging on the topmost-leftmost adjacent tile.
    if (!endOnWalkable) {
        sf::Vector2i bestTile(-1, -1);
        float bestDist = std::numeric_limits<float>::max();
        bool found = false;
        for (int radius = 1; radius <= 10 && !found; ++radius) {
            for (int dx = -radius; dx <= radius; ++dx) {
                for (int dy = -radius; dy <= radius; ++dy) {
                    if (std::abs(dx) != radius && std::abs(dy) != radius) continue; // border only
                    int nx = endTile.x + dx;
                    int ny = endTile.y + dy;
                    if (!isValidTile(nx, ny) || !isWalkable(nx, ny)) continue;
                    float d = static_cast<float>((nx - startTile.x) * (nx - startTile.x)
                                               + (ny - startTile.y) * (ny - startTile.y));
                    if (d < bestDist) {
                        bestDist = d;
                        bestTile = sf::Vector2i(nx, ny);
                        found = true; // at least one candidate found this ring
                    }
                }
            }
            // Once we have scanned a full ring that contained at least one
            // valid tile, stop — the best tile in this ring is closer to the
            // building than anything in an outer ring.
            if (found) break;
        }
        if (!found) {
            return { end };  // No walkable tile found, return direct path
        }
        endTile = bestTile;
    }
    
    // If start == end, no path needed.  Return the exact destination so the
    // unit doesn't inch toward a tile centre that may be a few pixels away.
    if (startTile == endTile) {
        return { endOnWalkable ? end : tileToWorldCenter(endTile.x, endTile.y) };
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

    // Track the closed node closest to the goal for partial-path fallback.
    Node  bestClosedNode = startNode;
    float bestClosedH    = startNode.h;

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

        // Keep track of the explored tile nearest to the goal (heuristic).
        // Used below to reconstruct a partial path if the goal is unreachable.
        if (current.h < bestClosedH) {
            bestClosedH    = current.h;
            bestClosedNode = current;
        }

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
            
            // Reverse to get start-to-end order.
            std::reverse(path.begin(), path.end());

            // Replace the tile-centre placeholders at both ends with the real
            // world positions: start position for the first node (so smoothing
            // can shoot a straight line from the unit's actual pixel position)
            // and end position for the last node (so the unit walks directly to
            // the click point rather than stopping at the nearest tile centre).
            if (!path.empty()) {
                path.front() = start;
            }
            if (endOnWalkable && !path.empty()) {
                path.back() = end;
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

            // Clearance penalty: prefer tiles away from walls.
            // Count orthogonal non-walkable neighbours (use 4-connectivity so
            // the penalty is proportional to "how cornered" the tile is).
            const int odx[] = { 0, 1, 0, -1 };
            const int ody[] = { -1, 0, 1,  0 };
            float clearancePenalty = 0.0f;
            for (int k = 0; k < 4; ++k) {
                if (!isWalkable(nx + odx[k], ny + ody[k]))
                    clearancePenalty += 0.5f;
            }

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
    
    // No path to goal found.  Return a path to the closest reachable tile
    // instead of a direct line (which would make units walk through water).
    {
        std::vector<sf::Vector2f> partial;
        int cx = bestClosedNode.x;
        int cy = bestClosedNode.y;
        while (cx != -1 && cy != -1) {
            partial.push_back(tileToWorldCenter(cx, cy));
            Node& n = allNodes[hash(cx, cy)];
            int px = n.parentX;
            int py = n.parentY;
            cx = px;
            cy = py;
        }
        std::reverse(partial.begin(), partial.end());
        // Replace start tile-centre with real unit position (same as full path).
        if (!partial.empty()) partial.front() = start;
        return smoothPath(partial);
    }
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

void Map::loadTransitionTextures() {
    // Initialise all entries to -1 (no texture for this mask).
    for (int i = 0; i < 16; ++i) m_cardinalLookup[i] = -1;

    // The 14 cardinal/corner transition tiles.
    // Each entry: { storage index, cardinal mask, filename }
    struct TransEntry { int idx; uint8_t mask; const char* file; };
    static const TransEntry edges[] = {
        // Single edges
        {  0, 0b0001, "grass_water_N.png"   },  // N
        {  1, 0b0010, "grass_water_E.png"   },  // E
        {  2, 0b0100, "grass_water_S.png"   },  // S
        {  3, 0b1000, "grass_water_W.png"   },  // W
        // Outer corners (2 adjacent cardinal sides)
        {  4, 0b0011, "grass_water_NE.png"  },  // N+E
        {  5, 0b1001, "grass_water_NW.png"  },  // N+W
        {  6, 0b0110, "grass_water_SE.png"  },  // S+E
        {  7, 0b1100, "grass_water_SW.png"  },  // S+W
        // Opposite pairs (thin grass strip)
        {  8, 0b0101, "grass_water_NS.png"  },  // N+S
        {  9, 0b1010, "grass_water_EW.png"  },  // E+W
        // Three-sided (grass strip on one edge only)
        { 10, 0b0111, "grass_water_NES.png" },  // N+E+S
        { 11, 0b1110, "grass_water_SEW.png" },  // S+E+W
        { 12, 0b1101, "grass_water_NSW.png" },  // N+S+W
        { 13, 0b1011, "grass_water_NEW.png" },  // N+E+W
    };
    for (const auto& e : edges) {
        std::string path = std::string("assets/terrain/") + e.file;
        if (!m_edgeTransTextures[e.idx].loadFromFile(path))
            std::cerr << "Map: cannot load " << path << "\n";
        m_cardinalLookup[e.mask] = static_cast<int8_t>(e.idx);
    }
    // All 14 cardinal configurations are now covered; mask 0b1111 (all four
    // sides water) is handled by the water-tile branch in render() directly.

    // Inner corner tiles: tile images that contain grass + a small water notch.
    // Index in m_innerTransTextures / diagonal mask bit:
    //   0 = SW diagonal water  → water_grass_NE.png
    //   1 = SE diagonal water  → water_grass_NW.png
    //   2 = NW diagonal water  → water_grass_SE.png
    //   3 = NE diagonal water  → water_grass_SW.png
    static const char* inner[4] = {
        "water_grass_NE.png",
        "water_grass_NW.png",
        "water_grass_SE.png",
        "water_grass_SW.png",
    };
    for (int i = 0; i < 4; ++i) {
        std::string path = std::string("assets/terrain/") + inner[i];
        if (!m_innerTransTextures[i].loadFromFile(path))
            std::cerr << "Map: cannot load " << path << "\n";
    }

    // Strip tiles: two same-side diagonal neighbours are water, leaving a
    // grass strip pointing in the named direction.
    // Index 0=N (NW+NE water), 1=S (SW+SE water), 2=E (NE+SE water), 3=W (NW+SW water).
    static const char* strips[4] = {
        "grass_water_N_strip.png",
        "grass_water_S_strip.png",
        "grass_water_E_strip.png",
        "grass_water_W_strip.png",
    };
    for (int i = 0; i < 4; ++i) {
        std::string path = std::string("assets/terrain/") + strips[i];
        if (!m_stripTransTextures[i].loadFromFile(path))
            std::cerr << "Map: cannot load " << path << "\n";
    }
}

uint8_t Map::cardinalWaterMask(int x, int y) const {
    auto isW = [&](int tx, int ty) -> bool {
        if (tx < 0 || tx >= m_width || ty < 0 || ty >= m_height) return false;
        return m_tiles[ty][tx].type == TileType::Water;
    };
    uint8_t m = 0;
    if (isW(x,     y - 1)) m |= 0b0001;  // N
    if (isW(x + 1, y    )) m |= 0b0010;  // E
    if (isW(x,     y + 1)) m |= 0b0100;  // S
    if (isW(x - 1, y    )) m |= 0b1000;  // W
    return m;
}

uint8_t Map::diagonalWaterMask(int x, int y) const {
    auto isW = [&](int tx, int ty) -> bool {
        if (tx < 0 || tx >= m_width || ty < 0 || ty >= m_height) return false;
        return m_tiles[ty][tx].type == TileType::Water;
    };
    uint8_t m = 0;
    if (isW(x - 1, y + 1)) m |= 0x01;  // SW
    if (isW(x + 1, y + 1)) m |= 0x02;  // SE
    if (isW(x - 1, y - 1)) m |= 0x04;  // NW
    if (isW(x + 1, y - 1)) m |= 0x08;  // NE
    return m;
}

float Map::heuristic(int x1, int y1, int x2, int y2) const {
    // Manhattan distance
    return static_cast<float>(std::abs(x2 - x1) + std::abs(y2 - y1));
}
