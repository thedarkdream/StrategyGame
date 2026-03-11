#pragma once

#include "Screen.h"
#include "GameStatistics.h"
#include <SFML/Graphics.hpp>
#include <memory>

// Top-level application: owns the window and manages screen transitions.
class Application {
public:
    Application();
    void run();
    
private:
    sf::RenderWindow m_window;
    std::unique_ptr<Screen> m_currentScreen;
    
    void handleScreenResult(const ScreenResult& result);
    void switchToMenu();
    void switchToGame(const std::string& mapFile, int localPlayerSlot = 0);
    void switchToEditor();
    void switchToVictory(bool isVictory, int localPlayerSlot, const GameStatistics& stats);
};
