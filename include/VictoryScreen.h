#pragma once

#include "Screen.h"
#include "GameStatistics.h"
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <optional>

class VictoryScreen : public Screen {
public:
    VictoryScreen(bool isVictory, int localPlayerSlot, const GameStatistics& stats);
    
    ScreenResult handleEvent(const sf::Event& event) override;
    ScreenResult update(float deltaTime) override;
    void render(sf::RenderWindow& window) override;
    
private:
    struct Button {
        sf::RectangleShape shape;
        std::optional<sf::Text> label;
        bool hovered = false;
    };
    
    bool m_isVictory;
    int m_localPlayerSlot;
    GameStatistics m_stats;
    
    const sf::Font* m_font{ nullptr };
    
    // Title (Victory / Defeat)
    std::optional<sf::Text> m_title;
    
    // Statistics table
    std::vector<sf::Text> m_tableHeaders;
    std::vector<std::vector<sf::Text>> m_tableRows;  // [row][column]
    sf::RectangleShape m_tableBackground;
    
    // Continue button
    Button m_continueButton;
    
    sf::Vector2u m_lastWinSize = { 0u, 0u };
    
    void layoutUI(sf::Vector2u windowSize);
    void buildStatsTable();
    Button createButton(const std::string& text, sf::Vector2f position, sf::Vector2f size);
    bool isMouseOver(const Button& button, sf::Vector2f mousePos) const;
    void updateButtonHover(Button& button, sf::Vector2f mousePos);
    std::string getPlayerName(int slot) const;
    sf::Color getPlayerColor(int slot) const;
};
