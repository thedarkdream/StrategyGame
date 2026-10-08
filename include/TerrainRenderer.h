#pragma once

#include <SFML/Graphics.hpp>
#include <vector>

// Forward declaration — TerrainRenderer reads the map via its public const API.
class Map;

// ---------------------------------------------------------------------------
// TerrainRenderer
//
// 2D tile drawing extracted from Map:
//   • loads the terrain images listed by TerrainTiling
//   • render() — culled, view-space tile draw loop; which images a tile uses
//     (autotiling of the water/grass transitions) is decided by TerrainTiling,
//     which the 3D renderer shares.
//
// Map::render() delegates directly to this class.  The Map class itself
// retains no texture state.
// ---------------------------------------------------------------------------
class TerrainRenderer {
public:
    // Loads all terrain textures on construction.
    TerrainRenderer();

    // Draw all tiles inside the world-space [visibleRect] onto [target].
    void render(sf::RenderTarget& target, const sf::FloatRect& visibleRect, const Map& map);

private:
    // Indexed by TerrainTiling image id (source images are 64×64); owned by
    // TextureManager, null where an image failed to load.
    std::vector<sf::Texture*> m_textures;
};
