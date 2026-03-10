#pragma once

#include "Screen.h"
#include "Game.h"
#include <memory>
#include <string>

class GameScreen : public Screen {
public:
    GameScreen(sf::RenderWindow& window, const std::string& mapFile, int localPlayerSlot = 0);
    
    ScreenResult handleEvent(const sf::Event& event) override;
    ScreenResult update(float deltaTime) override;
    void render(sf::RenderWindow& window) override;
    
private:
    std::unique_ptr<Game> m_game;
};
