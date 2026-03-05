#pragma once

#include "Types.h"
#include <SFML/Graphics.hpp>
#include <optional>

class Game;
class Map;
class Player;
class InputHandler;

class Renderer {
public:
    Renderer(sf::RenderWindow& window);
    
    void render(Game& game);
    void setCamera(const sf::View& camera) { m_camera = camera; }
    
private:
    sf::RenderWindow& m_window;
    sf::View m_camera;
    std::optional<sf::Font> m_font;
    
    // Render layers
    void renderMap(Map& map);
    void renderEntities(Game& game);
    void renderUI(Game& game);
    void renderSelectionBox(const InputHandler& input);
    void renderBuildPreview(const InputHandler& input, Map& map);
    void renderMinimap(Game& game);
    void renderResourceBar(Player& player);
    void renderUnitPanel(Game& game);
    
    // Helper
    sf::Color getTeamColor(Team team);
};
