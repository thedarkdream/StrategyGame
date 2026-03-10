#pragma once

#include "Screen.h"
#include <SFML/Graphics.hpp>
#include <optional>

// Stub map editor screen - placeholder for future implementation
class MapEditorScreen : public Screen {
public:
    MapEditorScreen();
    
    ScreenResult handleEvent(const sf::Event& event) override;
    ScreenResult update(float deltaTime) override;
    void render(sf::RenderWindow& window) override;
    
private:
    sf::Font m_font;
    bool m_fontLoaded = false;
    std::optional<sf::Text> m_title;
    std::optional<sf::Text> m_subtitle;
    sf::RectangleShape m_backButton;
    std::optional<sf::Text> m_backLabel;
};
