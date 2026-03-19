#pragma once

#include "Types.h"
#include <SFML/Graphics.hpp>

class Game;
class Map;

// ---------------------------------------------------------------------------
// Minimap
//
// Owns the per-tile terrain texture (rebuilt lazily when tiles change) and
// provides rendering, hit-testing, and coordinate conversion.
//
// Previously this logic was split between Renderer (rendering + terrain cache)
// and InputHandler (hit-test + coordinate conversion), causing the minimap
// screen position to be recalculated independently in two places.
// ---------------------------------------------------------------------------
class Minimap {
public:
    Minimap() = default;

    // Mark the terrain texture as stale. Call whenever map tiles change.
    void invalidate() { m_terrainDirty = true; }

    // Draw the full minimap (terrain, fog, entity dots, camera viewport rect)
    // to the render target in screen-space coordinates.
    void render(sf::RenderTarget& target, const Game& game, const sf::View& camera);

    // Returns the minimap's screen-space bounding rectangle for the given window.
    static sf::FloatRect screenBounds(sf::Vector2u windowSize);

    // Returns true when screenPos falls inside the minimap panel.
    static bool isHit(sf::Vector2i screenPos, sf::Vector2u windowSize);

    // Convert a minimap screen position to a world-space position.
    static sf::Vector2f toWorldPos(sf::Vector2i screenPos, sf::Vector2u windowSize, const Map& map);

private:
    sf::Texture m_terrainTex;
    bool        m_terrainDirty = true;

    void rebuildTerrain(const Map& map);

    static sf::Color tileColor(TileType t);
};
