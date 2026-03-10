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
    void renderEntities(Game& game);
    void renderUI(Game& game);
    void renderSelectionBox(const InputHandler& input);
    void renderBuildPreview(const InputHandler& input, Map& map);
    void renderMinimap(Game& game);
    void renderResourceBar(Player& player);
    void renderUnitPanel(Game& game);
    void renderTargetingModeIndicator(Game& game);
};
