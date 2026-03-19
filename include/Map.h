#pragma once

#include "Types.h"
#include "Constants.h"
#include <SFML/Graphics.hpp>
#include <vector>
#include <array>
#include <random>
#include <unordered_map>

class Map {
public:
    Map(int width = Constants::MAP_WIDTH, int height = Constants::MAP_HEIGHT);
    
    void render(sf::RenderTarget& target, const sf::View& camera);
    
    // Tile access
    Tile& getTile(int x, int y);
    const Tile& getTile(int x, int y) const;
    Tile& getTileAtPosition(sf::Vector2f position);
    bool isValidTile(int x, int y) const;
    bool isWalkable(int x, int y) const;
    bool isBuildable(int x, int y) const;
    
    // Coordinate conversion
    sf::Vector2i worldToTile(sf::Vector2f worldPos) const;
    sf::Vector2f tileToWorld(int x, int y) const;
    sf::Vector2f tileToWorldCenter(int x, int y) const;
    
    // Building placement
    bool canPlaceBuilding(int tileX, int tileY, int width, int height) const;
    void placeBuilding(int tileX, int tileY, int width, int height);
    void removeBuilding(int tileX, int tileY, int width, int height);
    
    // Pathfinding (A* with optional unit-radius clearance)
    // unitRadius: the unit's collision radius in pixels.  Non-zero values
    // activate clearance-aware neighbour pruning in A* and a fat-tube line-
    // of-sight check in smoothPath so the produced path is safe to physically
    // traverse without clipping building corners.
    //
    // extraTileCosts: optional map of (tileY*mapWidth + tileX) → additional
    // A* g-cost for that tile.  Used by unit-aware replanning to steer paths
    // around crowds of stationary allies without hard-blocking any tile.
    std::vector<sf::Vector2f> findPath(sf::Vector2f start, sf::Vector2f end,
                                       float unitRadius = 0.0f,
                                       const std::unordered_map<int, float>& extraTileCosts = {});
    
    // Editor
    void initEmpty();   // Reset all tiles to Grass with random variants
    void setTileType(int x, int y, TileType type);                    // Paint tile (assigns random variant)
    void setTileType(int x, int y, TileType type, uint8_t variant);   // Paint tile with specific variant
    
    // Getters
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    
private:
    int m_width;
    int m_height;
    std::vector<std::vector<Tile>> m_tiles;

    // Terrain textures: 8 grass variants + 4 water variants (source images are 64×64)
    std::array<sf::Texture, 8> m_grassTextures;
    std::array<sf::Texture, 4> m_waterTextures;

    // Transition tile textures (autotile system).
    //
    // Cardinal edges + outer corners (8 tiles have art; 6 configs fall back to water):
    //   m_edgeTransTextures[0..7] stores N, E, S, W, NE, NW, SE, SW.
    // m_cardinalLookup[0..15] maps a 4-bit water-neighbour mask to a texture
    //   pointer (nullptr = missing tile → fall back to a water tile).
    //   Mask bit layout: bit0=N, bit1=E, bit2=S, bit3=W.
    //
    // Inner corners (detected when cardinal mask == 0 but a diagonal is water):
    //   m_innerTransTextures[0..3] for SW/SE/NW/NE diagonal water respectively.
    //   Index 0 = SW diag water → water_grass_NE
    //   Index 1 = SE diag water → water_grass_NW
    //   Index 2 = NW diag water → water_grass_SE
    //   Index 3 = NE diag water → water_grass_SW
    std::array<sf::Texture, 14> m_edgeTransTextures;
    std::array<sf::Texture, 4> m_innerTransTextures;
    // Strip tiles: two opposite-side diagonal neighbours are water.
    // Index 0=N (NW+NE water), 1=S (SW+SE water), 2=E (NE+SE water), 3=W (NW+SW water).
    std::array<sf::Texture, 4> m_stripTransTextures;
    // Maps a 4-bit cardinal-water mask to an index into m_edgeTransTextures.
    // -1 means no art is available for that configuration.
    // Using indices (not raw pointers) keeps the Map safe to move/copy-assign.
    int8_t m_cardinalLookup[16] = {};

    std::mt19937 m_rng;
    void loadTerrainTextures();
    void loadTransitionTextures();

    // Returns a 4-bit mask of which cardinal neighbours are water tiles.
    // bit0=N, bit1=E, bit2=S, bit3=W.  Out-of-bounds treated as non-water.
    uint8_t cardinalWaterMask(int x, int y) const;
    // Returns a 4-bit mask of which diagonal neighbours are water tiles
    // (only checked when the cardinal mask is zero).
    // bit0=SW, bit1=SE, bit2=NW, bit3=NE.
    uint8_t diagonalWaterMask(int x, int y) const;

    // Pathfinding helper
    struct PathNode {
        int x, y;
        float g, h;
        PathNode* parent = nullptr;
        float f() const { return g + h; }
    };
    float heuristic(int x1, int y1, int x2, int y2) const;
    // Returns true if there is a clear tile-based line of sight between two tiles.
    // unitRadius (pixels): when > 0 the check uses a fat tube of that width so
    // a unit of that radius won't clip building corners along the path.
    bool hasLineOfSight(int x0, int y0, int x1, int y1, float unitRadius = 0.0f) const;
    // Remove redundant waypoints: skip any waypoint that can be reached directly
    // in a straight line from the previous kept waypoint (string-pulling).
    std::vector<sf::Vector2f> smoothPath(const std::vector<sf::Vector2f>& path,
                                         float unitRadius = 0.0f) const;
};
