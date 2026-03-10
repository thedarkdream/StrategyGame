#pragma once

#include "Types.h"
#include "Constants.h"
#include <SFML/Graphics.hpp>
#include <vector>

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
    void initEmpty();   // Reset all tiles to Ground
    void setTileType(int x, int y, TileType type);  // Paint a single tile
    
    // Getters
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    
private:
    int m_width;
    int m_height;
    std::vector<std::vector<Tile>> m_tiles;
    
    // Rendering
    sf::VertexArray m_tileVertices;
    void buildVertexArray();
    void rebuildTileVertices(int x, int y);  // Update a single tile's vertices
    
    // Pathfinding helper
    struct PathNode {
        int x, y;
        float g, h;
        PathNode* parent = nullptr;
        float f() const { return g + h; }
    };
    float heuristic(int x1, int y1, int x2, int y2) const;
};
