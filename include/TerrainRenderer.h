#pragma once

#include <SFML/Graphics.hpp>
#include <array>
#include <cstdint>

// Forward declaration — TerrainRenderer reads the map via its public const API.
class Map;

// ---------------------------------------------------------------------------
// TerrainRenderer
//
// Owns all terrain-texture data and the tile-rendering algorithm extracted
// from Map:
//   • Grass and water base textures
//   • Cardinal-edge, inner-corner, and strip transition textures (autotile)
//   • render() — culled, view-space tile draw loop
//
// Map::render() delegates directly to this class.  The Map class itself
// retains no texture state.
// ---------------------------------------------------------------------------
class TerrainRenderer {
public:
    // Loads all terrain textures on construction.
    TerrainRenderer();

    // Draw all tiles visible inside [camera] onto [target].
    void render(sf::RenderTarget& target, const sf::View& camera, const Map& map);

private:
    // ── Base terrain textures ────────────────────────────────────────────────
    // 8 grass variants + 4 water variants (source images are 64×64).
    std::array<sf::Texture, 8> m_grassTextures;
    std::array<sf::Texture, 4> m_waterTextures;

    // ── Transition / autotile textures ───────────────────────────────────────
    //
    // Cardinal edges + outer corners (14 tiles with art):
    //   m_edgeTransTextures[0..13] indexed by m_cardinalLookup[4-bit mask].
    //   Mask bit layout: bit0=N, bit1=E, bit2=S, bit3=W.
    //   -1 entry means no art for that configuration.
    std::array<sf::Texture, 14> m_edgeTransTextures;
    int8_t                      m_cardinalLookup[16] = {};

    // Inner corners (4 tiles): grass tile with a small water notch at one corner.
    //   Index 0=SW diag, 1=SE diag, 2=NW diag, 3=NE diag water.
    std::array<sf::Texture, 4> m_innerTransTextures;

    // Strip tiles: two same-side diagonal water neighbours.
    //   Index 0=N strip (NW+NE water), 1=S, 2=E, 3=W.
    std::array<sf::Texture, 4> m_stripTransTextures;

    // ── Helpers ─────────────────────────────────────────────────────────────
    void loadTerrainTextures();
    void loadTransitionTextures();

    // Returns a 4-bit mask of which cardinal neighbours are Water tiles.
    // bit0=N, bit1=E, bit2=S, bit3=W.  Out-of-bounds treated as non-water.
    uint8_t cardinalWaterMask(const Map& map, int x, int y) const;

    // Returns a 4-bit mask of which diagonal neighbours are Water tiles
    // (only checked when the cardinal mask is zero).
    // bit0=SW, bit1=SE, bit2=NW, bit3=NE.
    uint8_t diagonalWaterMask(const Map& map, int x, int y) const;
};
