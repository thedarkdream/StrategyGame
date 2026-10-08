#pragma once

#include "Types.h"
#include "Constants.h"
#include "Pathfinder.h"
#include "TerrainRenderer.h"
#include <SFML/Graphics.hpp>
#include <vector>
#include <random>
#include <unordered_map>

class Map {
public:
    Map(int width = Constants::MAP_WIDTH, int height = Constants::MAP_HEIGHT);

    // Delegates to TerrainRenderer.  `visibleRect` is the world-space area to draw.
    void render(sf::RenderTarget& target, const sf::FloatRect& visibleRect);

    // ── Tile access ──────────────────────────────────────────────────────────
    Tile& getTile(int x, int y);
    const Tile& getTile(int x, int y) const;
    Tile& getTileAtPosition(sf::Vector2f position);
    bool isValidTile(int x, int y) const;
    bool isWalkable(int x, int y) const;
    bool isBuildable(int x, int y) const;

    // ── Coordinate conversion ────────────────────────────────────────────────
    sf::Vector2i worldToTile(sf::Vector2f worldPos) const;
    sf::Vector2f tileToWorld(int x, int y) const;
    sf::Vector2f tileToWorldCenter(int x, int y) const;

    // ── Building placement ───────────────────────────────────────────────────
    bool canPlaceBuilding(int tileX, int tileY, int width, int height) const;
    void placeBuilding(int tileX, int tileY, int width, int height);
    void removeBuilding(int tileX, int tileY, int width, int height);

    // ── Pathfinding – delegates to Pathfinder ───────────────────────────────
    // unitRadius: the unit's collision radius in pixels.  Non-zero values
    // activate clearance-aware neighbour pruning in A* and a fat-tube LOS
    // check in smoothPath so the produced path is safe to traverse without
    // clipping building corners.
    //
    // extraTileCosts: optional map of (tileY*mapWidth + tileX) → additional
    // A* g-cost.  Used by unit-aware replanning to steer paths around crowds.
    std::vector<sf::Vector2f> findPath(sf::Vector2f start, sf::Vector2f end,
                                       float unitRadius = 0.0f,
                                       const std::unordered_map<int, float>& extraTileCosts = {});

    // ── Editor ───────────────────────────────────────────────────────────────
    void initEmpty();                                              // Reset all tiles to Grass
    void setTileType(int x, int y, TileType type);                // Paint tile (random variant)
    void setTileType(int x, int y, TileType type, uint8_t variant); // Paint tile with specific variant

    // ── Getters ──────────────────────────────────────────────────────────────
    int getWidth()  const { return m_width;  }
    int getHeight() const { return m_height; }

    // Elevation (heightmap).  Each tile has an integer elevation level.  The
    // terrain surface is defined on the (width+1) x (height+1) vertex grid: a
    // vertex takes the highest level of the (up to four) tiles touching it, so
    // raised tiles form flat plateaus and the neighbouring lower tiles carry
    // the slope.
    int   getTileElevation(int x, int y) const;
    void  setTileElevation(int x, int y, int level);      // clamped to 0..MAX_ELEVATION
    int   getVertexLevel(int vx, int vy) const;
    float getVertexHeight(int vx, int vy) const;          // world units
    float getHeightAt(sf::Vector2f worldPos) const;       // bilinear terrain height
    bool  isCliffTile(int x, int y) const;                // slope too steep to walk/build
    bool  hasElevation() const { return m_hasElevation; } // false for completely flat maps

    // Terrain-only line of sight between two world points (eye raised slightly
    // above the ground at `from`).  Always true on flat maps.
    bool  hasLineOfSight(sf::Vector2f from, sf::Vector2f to) const;

private:
    int m_width;
    int m_height;
    std::vector<std::vector<Tile>> m_tiles;
    std::mt19937 m_rng;

    // Terrain surface cache: level of each corner vertex ((width+1) x (height+1)).
    std::vector<std::uint8_t> m_vertexLevels;
    bool                      m_hasElevation = false;
    void resetTerrainCache();
    void recomputeTerrainAround(int tileX, int tileY);

    // Sub-objects that own pathfinding and terrain-rendering concerns.
    // Declared after the tile data so they are initialised last and can safely
    // receive *this if they ever cache a back-pointer (currently they do not).
    Pathfinder      m_pathfinder;
    TerrainRenderer m_terrainRenderer;
};
