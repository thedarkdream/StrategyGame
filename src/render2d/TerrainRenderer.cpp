#include "render2d/TerrainRenderer.h"
#include "world/TerrainTiling.h"
#include "media/TextureManager.h"
#include "world/Map.h"
#include "core/Constants.h"
#include "core/Types.h"
#include <algorithm>

TerrainRenderer::TerrainRenderer() {
    m_textures.reserve(static_cast<size_t>(TerrainTiling::textureCount()));
    for (int id = 0; id < TerrainTiling::textureCount(); ++id)
        m_textures.push_back(TEXTURES.loadTexture(TerrainTiling::texturePath(id)));
}

void TerrainRenderer::render(sf::RenderTarget& target, const sf::FloatRect& visibleRect, const Map& map) {
    // Only draw tiles visible within the given world rectangle.
    sf::Vector2f topLeft = visibleRect.position;
    const float ts    = static_cast<float>(Constants::TILE_SIZE);
    const float scale = ts / 64.f;  // source textures are 64×64, tiles are 32×32

    int startX = std::max(0, static_cast<int>(topLeft.x / ts));
    int startY = std::max(0, static_cast<int>(topLeft.y / ts));
    int endX   = std::min(map.getWidth(),  static_cast<int>((topLeft.x + visibleRect.size.x) / ts) + 2);
    int endY   = std::min(map.getHeight(), static_cast<int>((topLeft.y + visibleRect.size.y) / ts) + 2);

    auto drawTex = [&](int id, int x, int y) {
        if (id == TerrainTiling::kNone) return;
        const sf::Texture* texture = m_textures[static_cast<size_t>(id)];
        if (!texture) return;
        sf::Sprite sp(*texture);
        sp.setScale(sf::Vector2f(scale, scale));
        sp.setPosition(sf::Vector2f(x * ts, y * ts));
        target.draw(sp);
    };

    for (int y = startY; y < endY; ++y) {
        for (int x = startX; x < endX; ++x) {
            const TerrainTiling::TileArt art = TerrainTiling::describe(map, x, y);
            drawTex(art.base, x, y);
            drawTex(art.overlay, x, y);
        }
    }
}
