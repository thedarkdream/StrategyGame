#pragma once

#include "Screen.h"
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <optional>

class MenuScreen : public Screen {
public:
    MenuScreen();
    
    ScreenResult handleEvent(const sf::Event& event) override;
    ScreenResult update(float deltaTime) override;
    void render(sf::RenderWindow& window) override;
    
private:
    // Simple button struct
    struct Button {
        sf::RectangleShape shape;
        std::optional<sf::Text> label;
        bool hovered = false;
    };
    
    sf::Font m_font;
    bool m_fontLoaded = false;
    
    // Title
    std::optional<sf::Text> m_title;
    
    // Main buttons
    Button m_newGameButton;
    Button m_editorButton;
    Button m_quitButton;
    
    // Map selection sub-panel (shown when "New Game" is clicked)
    bool m_showMapSelection = false;
    std::vector<std::string> m_mapFiles;
    std::vector<Button> m_mapButtons;
    Button m_backButton;
    std::optional<sf::Text> m_mapSelectionTitle;
    
    // Slot picker sub-panel (shown after a map is selected)
    bool m_showSlotPicker = false;
    std::string m_selectedMapFile;       // map chosen in the map selection
    int  m_slotPlayerCount = 2;          // playerCount of the chosen map
    std::vector<Button> m_slotButtons;   // "Play as Player 1", etc.
    Button m_slotBackButton;
    std::optional<sf::Text> m_slotPickerTitle;
    
    void layoutButtons(sf::Vector2u windowSize);
    void layoutMapSelection(sf::Vector2u windowSize);
    void layoutSlotPicker(sf::Vector2u windowSize);
    void scanForMaps();
    int  getMapPlayerCount(const std::string& mapFile) const; // read playerCount from file
    Button createButton(const std::string& text, sf::Vector2f position, sf::Vector2f size);
    bool isMouseOver(const Button& button, sf::Vector2f mousePos) const;
    void updateButtonHover(Button& button, sf::Vector2f mousePos);

    sf::Vector2u m_lastWinSize = { 0u, 0u };
};
