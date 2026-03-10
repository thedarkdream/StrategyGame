#pragma once

#include "Types.h"
#include "Constants.h"
#include <SFML/Graphics.hpp>
#include <vector>
#include <array>
#include <random>

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
    void placeBuilding(int tileX, int tileY, int width, int height, EntityPtr building);
    void removeBuilding(int tileX, int tileY, int width, int height);
    
    // Pathfinding (simple A*)
    std::vector<sf::Vector2f> findPath(sf::Vector2f start, sf::Vector2f end);
    
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
    std::mt19937 m_rng;
    void loadTerrainTextures();

    // Pathfinding helper
    struct PathNode {
        int x, y;
        float g, h;
        PathNode* parent = nullptr;
        float f() const { return g + h; }
    };
    float heuristic(int x1, int y1, int x2, int y2) const;
};
