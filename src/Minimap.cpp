#include "Minimap.h"
#include "Game.h"
#include "Map.h"
#include "Constants.h"
#include "Entity.h"
#include "FogOfWar.h"
#include <iostream>

// ---- Static helpers --------------------------------------------------------

sf::FloatRect Minimap::screenBounds(sf::Vector2u windowSize) {
    float x = Constants::MINIMAP_PADDING;
    float y = static_cast<float>(windowSize.y) - Constants::MINIMAP_SIZE - Constants::MINIMAP_PADDING;
    return sf::FloatRect(sf::Vector2f(x, y),
                         sf::Vector2f(Constants::MINIMAP_SIZE, Constants::MINIMAP_SIZE));
}

bool Minimap::isHit(sf::Vector2i screenPos, sf::Vector2u windowSize) {
    sf::FloatRect rect = screenBounds(windowSize);
    return screenPos.x >= rect.position.x &&
           screenPos.x <= rect.position.x + rect.size.x &&
           screenPos.y >= rect.position.y &&
           screenPos.y <= rect.position.y + rect.size.y;
}

sf::Vector2f Minimap::toWorldPos(sf::Vector2i screenPos, sf::Vector2u windowSize, const Map& map) {
    sf::FloatRect rect = screenBounds(windowSize);
    float relX = (screenPos.x - rect.position.x) / rect.size.x;
    float relY = (screenPos.y - rect.position.y) / rect.size.y;
    return sf::Vector2f(
        relX * map.getWidth()  * Constants::TILE_SIZE,
        relY * map.getHeight() * Constants::TILE_SIZE);
}

// ---- Terrain cache ---------------------------------------------------------

sf::Color Minimap::tileColor(TileType t) {
    switch (t) {
        case TileType::Grass:    return sf::Color( 42,  80,  42);
        case TileType::Water:    return sf::Color( 30,  80, 130);
        case TileType::Resource: return sf::Color( 40, 130,  60);
        case TileType::Building: return sf::Color( 90,  90,  90);
    }
    return sf::Color(42, 80, 42);
}

void Minimap::rebuildTerrain(const Map& map) {
    const int W = map.getWidth();
    const int H = map.getHeight();

    sf::Image img;
    img.resize(sf::Vector2u(static_cast<unsigned>(W), static_cast<unsigned>(H)));

    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            img.setPixel(sf::Vector2u(static_cast<unsigned>(x),
                                      static_cast<unsigned>(y)),
                         tileColor(map.getTile(x, y).type));

    if (!m_terrainTex.loadFromImage(img))
        std::cerr << "Minimap: failed to upload terrain texture\n";

    m_terrainDirty = false;
}

// ---- Render ----------------------------------------------------------------

void Minimap::render(sf::RenderTarget& target, const Game& game, const sf::View& camera) {
    sf::Vector2u windowSize = target.getSize();
    sf::FloatRect bounds    = screenBounds(windowSize);
    const float mmX = bounds.position.x;
    const float mmY = bounds.position.y;
    const float mmS = Constants::MINIMAP_SIZE;

    if (m_terrainDirty)
        rebuildTerrain(game.getMap());

    // ---- Background --------------------------------------------------------
    sf::RectangleShape bg(sf::Vector2f(mmS, mmS));
    bg.setPosition(sf::Vector2f(mmX, mmY));
    bg.setFillColor(sf::Color(30, 30, 30, 220));
    bg.setOutlineThickness(2.0f);
    bg.setOutlineColor(sf::Color(110, 120, 130));
    target.draw(bg);

    // ---- Terrain layer -----------------------------------------------------
    const Map& map = game.getMap();
    float scaleX = mmS / static_cast<float>(map.getWidth());
    float scaleY = mmS / static_cast<float>(map.getHeight());

    sf::Sprite terrainSprite(m_terrainTex);
    terrainSprite.setPosition(sf::Vector2f(mmX, mmY));
    terrainSprite.setScale(sf::Vector2f(scaleX, scaleY));
    target.draw(terrainSprite);

    // ---- Fog-of-war overlay ------------------------------------------------
    const FogOfWar& fog = game.getPlayer().getFog();
    sf::Sprite fogSprite(fog.getFogTexture());
    fogSprite.setPosition(sf::Vector2f(mmX, mmY));
    fogSprite.setScale(sf::Vector2f(scaleX, scaleY));
    target.draw(fogSprite);

    // ---- World → minimap pixel scale (entity dots) ------------------------
    float worldScaleX = mmS / (map.getWidth()  * Constants::TILE_SIZE);
    float worldScaleY = mmS / (map.getHeight() * Constants::TILE_SIZE);

    const Team myTeam = game.getPlayer().getTeam();

    // ---- Live entity dots --------------------------------------------------
    for (const auto& entity : game.getAllEntities()) {
        if (!entity || !entity->isAlive()) continue;

        // Hide enemy units outside current vision.
        const bool isOwn = (entity->getTeam() == myTeam);
        if (!isOwn && !fog.isVisible(
                static_cast<int>(entity->getPosition().x / Constants::TILE_SIZE),
                static_cast<int>(entity->getPosition().y / Constants::TILE_SIZE)))
            continue;

        float x = mmX + entity->getPosition().x * worldScaleX;
        float y = mmY + entity->getPosition().y * worldScaleY;

        constexpr float DOT_R = 2.5f;
        sf::CircleShape dot(DOT_R);
        dot.setOrigin(sf::Vector2f(DOT_R, DOT_R));
        dot.setPosition(sf::Vector2f(x, y));
        dot.setFillColor(teamColor(entity->getTeam()));
        target.draw(dot);
    }

    // ---- Ghost dots (explored but not currently visible) -------------------
    for (const auto& [id, ghost] : fog.getGhosts()) {
        if (!ghost.entity) continue;

        const int tx = static_cast<int>(ghost.entity->getPosition().x / Constants::TILE_SIZE);
        const int ty = static_cast<int>(ghost.entity->getPosition().y / Constants::TILE_SIZE);
        if (fog.isVisible(tx, ty)) continue;  // Live pass already drew it

        float x = mmX + ghost.entity->getPosition().x * worldScaleX;
        float y = mmY + ghost.entity->getPosition().y * worldScaleY;

        sf::Color base = teamColor(ghost.entity->getTeam());
        base.a = 110;

        constexpr float GHOST_R = 2.0f;
        sf::CircleShape dot(GHOST_R);
        dot.setOrigin(sf::Vector2f(GHOST_R, GHOST_R));
        dot.setPosition(sf::Vector2f(x, y));
        dot.setFillColor(base);
        target.draw(dot);
    }

    // ---- Camera viewport rectangle -----------------------------------------
    sf::Vector2f camCenter = camera.getCenter();
    sf::Vector2f camSize   = camera.getSize();

    sf::RectangleShape camRect(sf::Vector2f(camSize.x * worldScaleX, camSize.y * worldScaleY));
    camRect.setPosition(sf::Vector2f(
        mmX + (camCenter.x - camSize.x / 2.0f) * worldScaleX,
        mmY + (camCenter.y - camSize.y / 2.0f) * worldScaleY));
    camRect.setFillColor(sf::Color::Transparent);
    camRect.setOutlineThickness(1.0f);
    camRect.setOutlineColor(sf::Color::White);
    target.draw(camRect);
}
