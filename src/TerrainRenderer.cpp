#include "TerrainRenderer.h"
#include "Map.h"
#include "Constants.h"
#include "Types.h"
#include <iostream>

TerrainRenderer::TerrainRenderer() {
    loadTerrainTextures();
    loadTransitionTextures();
}

void TerrainRenderer::render(sf::RenderTarget& target, const sf::View& camera, const Map& map) {
    // Only draw tiles visible within the current camera view.
    sf::Vector2f topLeft = camera.getCenter() - camera.getSize() / 2.f;
    const float ts    = static_cast<float>(Constants::TILE_SIZE);
    const float scale = ts / 64.f;  // source textures are 64×64, tiles are 32×32

    int startX = std::max(0, static_cast<int>(topLeft.x / ts));
    int startY = std::max(0, static_cast<int>(topLeft.y / ts));
    int endX   = std::min(map.getWidth(),  static_cast<int>((topLeft.x + camera.getSize().x) / ts) + 2);
    int endY   = std::min(map.getHeight(), static_cast<int>((topLeft.y + camera.getSize().y) / ts) + 2);

    auto drawTex = [&](const sf::Texture& tex, int x, int y) {
        sf::Sprite sp(tex);
        sp.setScale(sf::Vector2f(scale, scale));
        sp.setPosition(sf::Vector2f(x * ts, y * ts));
        target.draw(sp);
    };

    for (int y = startY; y < endY; ++y) {
        for (int x = startX; x < endX; ++x) {
            const Tile& tile = map.getTile(x, y);

            // ── Water tiles: always drawn as-is ───────────────────────────────
            if (tile.type == TileType::Water) {
                uint8_t v = tile.variant;
                drawTex((v >= 1 && v <= 4) ? m_waterTextures[v - 1] : m_waterTextures[0], x, y);
                continue;
            }

            // ── Everything else (Grass / Resource / Building) ─────────────────
            // Step 1: check cardinal neighbours for water.
            const uint8_t cmask = cardinalWaterMask(map, x, y);

            if (cmask != 0) {
                // ── Cardinal transition tile ──────────────────────────────────
                const int8_t idx = m_cardinalLookup[cmask];
                if (idx >= 0) {
                    drawTex(m_edgeTransTextures[static_cast<size_t>(idx)], x, y);
                } else {
                    // Missing tile – fall back to grass so the gap is obvious.
                    drawTex(m_grassTextures[0], x, y);
                }
            } else {
                // ── No cardinal water neighbours ──────────────────────────────
                // Check diagonal neighbours first; if any diagonal is water,
                // overlay the corresponding inner-corner tile.
                uint8_t dmask = diagonalWaterMask(map, x, y);

                // Always draw the base grass tile first.
                uint8_t v = tile.variant;
                drawTex((v >= 1 && v <= 8) ? m_grassTextures[v - 1] : m_grassTextures[0], x, y);

                // Paired same-side diagonal water → strip tile covering both corners.
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

void TerrainRenderer::loadTerrainTextures() {
    for (int i = 0; i < 8; ++i) {
        std::string path = "assets/terrain/grass_" + std::to_string(i + 1) + ".png";
        if (!m_grassTextures[i].loadFromFile(path))
            std::cerr << "TerrainRenderer: cannot load " << path << "\n";
    }
    for (int i = 0; i < 4; ++i) {
        std::string path = "assets/terrain/water_" + std::to_string(i + 1) + ".png";
        if (!m_waterTextures[i].loadFromFile(path))
            std::cerr << "TerrainRenderer: cannot load " << path << "\n";
    }
}

void TerrainRenderer::loadTransitionTextures() {
    // Initialise all entries to -1 (no texture for this mask).
    for (int i = 0; i < 16; ++i) m_cardinalLookup[i] = -1;

    // The 14 cardinal/corner transition tiles.
    struct TransEntry { int idx; uint8_t mask; const char* file; };
    static const TransEntry edges[] = {
        // Single edges
        {  0, 0b0001, "grass_water_N.png"   },
        {  1, 0b0010, "grass_water_E.png"   },
        {  2, 0b0100, "grass_water_S.png"   },
        {  3, 0b1000, "grass_water_W.png"   },
        // Outer corners (2 adjacent cardinal sides)
        {  4, 0b0011, "grass_water_NE.png"  },
        {  5, 0b1001, "grass_water_NW.png"  },
        {  6, 0b0110, "grass_water_SE.png"  },
        {  7, 0b1100, "grass_water_SW.png"  },
        // Opposite pairs (thin grass strip)
        {  8, 0b0101, "grass_water_NS.png"  },
        {  9, 0b1010, "grass_water_EW.png"  },
        // Three-sided (grass strip on one edge only)
        { 10, 0b0111, "grass_water_NES.png" },
        { 11, 0b1110, "grass_water_SEW.png" },
        { 12, 0b1101, "grass_water_NSW.png" },
        { 13, 0b1011, "grass_water_NEW.png" },
    };
    for (const auto& e : edges) {
        std::string path = std::string("assets/terrain/") + e.file;
        if (!m_edgeTransTextures[e.idx].loadFromFile(path))
            std::cerr << "TerrainRenderer: cannot load " << path << "\n";
        m_cardinalLookup[e.mask] = static_cast<int8_t>(e.idx);
    }

    // Inner corner tiles: grass image with a small water notch at one corner.
    //   0 = SW diag water → water_grass_NE.png
    //   1 = SE diag water → water_grass_NW.png
    //   2 = NW diag water → water_grass_SE.png
    //   3 = NE diag water → water_grass_SW.png
    static const char* inner[4] = {
        "water_grass_NE.png",
        "water_grass_NW.png",
        "water_grass_SE.png",
        "water_grass_SW.png",
    };
    for (int i = 0; i < 4; ++i) {
        std::string path = std::string("assets/terrain/") + inner[i];
        if (!m_innerTransTextures[i].loadFromFile(path))
            std::cerr << "TerrainRenderer: cannot load " << path << "\n";
    }

    // Strip tiles: two same-side diagonal water neighbours.
    //   0=N (NW+NE water), 1=S (SW+SE water), 2=E (NE+SE water), 3=W (NW+SW water).
    static const char* strips[4] = {
        "grass_water_N_strip.png",
        "grass_water_S_strip.png",
        "grass_water_E_strip.png",
        "grass_water_W_strip.png",
    };
    for (int i = 0; i < 4; ++i) {
        std::string path = std::string("assets/terrain/") + strips[i];
        if (!m_stripTransTextures[i].loadFromFile(path))
            std::cerr << "TerrainRenderer: cannot load " << path << "\n";
    }
}

uint8_t TerrainRenderer::cardinalWaterMask(const Map& map, int x, int y) const {
    auto isW = [&map](int tx, int ty) -> bool {
        if (tx < 0 || tx >= map.getWidth() || ty < 0 || ty >= map.getHeight()) return false;
        return map.getTile(tx, ty).type == TileType::Water;
    };
    uint8_t m = 0;
    if (isW(x,     y - 1)) m |= 0b0001;  // N
    if (isW(x + 1, y    )) m |= 0b0010;  // E
    if (isW(x,     y + 1)) m |= 0b0100;  // S
    if (isW(x - 1, y    )) m |= 0b1000;  // W
    return m;
}

uint8_t TerrainRenderer::diagonalWaterMask(const Map& map, int x, int y) const {
    auto isW = [&map](int tx, int ty) -> bool {
        if (tx < 0 || tx >= map.getWidth() || ty < 0 || ty >= map.getHeight()) return false;
        return map.getTile(tx, ty).type == TileType::Water;
    };
    uint8_t m = 0;
    if (isW(x - 1, y + 1)) m |= 0x01;  // SW
    if (isW(x + 1, y + 1)) m |= 0x02;  // SE
    if (isW(x - 1, y - 1)) m |= 0x04;  // NW
    if (isW(x + 1, y - 1)) m |= 0x08;  // NE
    return m;
}
