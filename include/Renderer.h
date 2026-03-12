#pragma once

#include "Types.h"
#include <SFML/Graphics.hpp>
class Game;
class Map;
class Player;
class InputHandler;

class Renderer {
public:
    Renderer(sf::RenderWindow& window);
    
    void render(Game& game);
    void setCamera(const sf::View& camera) { m_camera = camera; }

    // Call whenever the map tiles change so the minimap terrain is re-baked
    void invalidateMinimapTerrain() { m_minimapTerrainDirty = true; }
    
private:
    sf::RenderWindow& m_window;
    sf::View m_camera;
    const sf::Font* m_font{ nullptr };

    // Cached terrain layer for the minimap (one pixel per tile)
    sf::Texture m_minimapTerrainTex;
    bool        m_minimapTerrainDirty = true;
    void rebuildMinimapTerrain(Map& map);
    
    // Render layers
    void renderMap(Map& map);
    // Pre-fog pass: resource nodes (always) + ghost buildings/dead-resources
    // in the shroud.  Everything in this pass is naturally darkened by the fog
    // overlay that follows.
    void renderGhosts(Game& game);
    // The fog-of-war overlay texture scaled to cover the entire map.
    void renderFogOverlay(Game& game);
    // Pass 2 – all other entities (units / buildings) filtered by fog visibility.
    void renderEntities(Game& game);
    void renderRallyPoints(Game& game);
    void renderUI(Game& game);
    void renderSelectionBox(const InputHandler& input);
    void renderBuildPreview(const InputHandler& input, Map& map);
    void renderMinimap(Game& game);
    void renderResourceBar(Player& player);
    void renderUnitPanel(Game& game);
    void renderTargetingModeIndicator(Game& game);
};
