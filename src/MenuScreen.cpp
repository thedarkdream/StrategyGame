#include "MenuScreen.h"
#include "FontManager.h"
#include "Constants.h"
#include "MapSerializer.h"
#include <algorithm>
#include <iostream>

MenuScreen::MenuScreen() {
    m_font = FontManager::instance().defaultFont();

    // Title
    m_title.emplace(*m_font, "Strategy Game", 48);
    m_title->setFillColor(sf::Color::White);
    
    // Scan for available maps
    scanForMaps();
    
    // Layout will be done in render based on window size
    sf::Vector2u defaultSize(Constants::WINDOW_WIDTH, Constants::WINDOW_HEIGHT);
    layoutButtons(defaultSize);
    layoutMapSelection(defaultSize);
    layoutSlotPicker(defaultSize);
}

void MenuScreen::scanForMaps() {
    m_mapFiles.clear();

    // Always include "default" option (procedurally generated map)
    m_mapFiles.push_back("default");

    // Scan maps/ directory for .stmap files
    auto stems = MapSerializer::listMaps("maps");
    for (auto& s : stems)
        m_mapFiles.push_back(s);
}

MenuScreen::Button MenuScreen::createButton(const std::string& text, sf::Vector2f position, sf::Vector2f size) {
    Button button;
    
    button.shape.setSize(size);
    button.shape.setPosition(position);
    button.shape.setFillColor(sf::Color(60, 60, 60));
    button.shape.setOutlineColor(sf::Color(100, 100, 100));
    button.shape.setOutlineThickness(2.0f);
    
    button.label.emplace(*m_font, text, 22);
    button.label->setFillColor(sf::Color::White);
    
    // Center text in button
    sf::FloatRect textBounds = button.label->getLocalBounds();
    button.label->setOrigin(sf::Vector2f(
        textBounds.position.x + textBounds.size.x / 2.0f,
        textBounds.position.y + textBounds.size.y / 2.0f
    ));
    button.label->setPosition(sf::Vector2f(
        position.x + size.x / 2.0f,
        position.y + size.y / 2.0f
    ));
    
    return button;
}

bool MenuScreen::isMouseOver(const Button& button, sf::Vector2f mousePos) const {
    sf::FloatRect bounds(button.shape.getPosition(), button.shape.getSize());
    return bounds.contains(mousePos);
}

void MenuScreen::updateButtonHover(Button& button, sf::Vector2f mousePos) {
    bool wasHovered = button.hovered;
    button.hovered = isMouseOver(button, mousePos);
    
    if (button.hovered) {
        button.shape.setFillColor(sf::Color(80, 80, 100));
        button.shape.setOutlineColor(sf::Color(150, 150, 200));
    } else {
        button.shape.setFillColor(sf::Color(60, 60, 60));
        button.shape.setOutlineColor(sf::Color(100, 100, 100));
    }
}

void MenuScreen::layoutButtons(sf::Vector2u windowSize) {
    m_lastWinSize = windowSize;
    float centerX = windowSize.x / 2.0f;
    float centerY = windowSize.y / 2.0f;
    float buttonW = 280.0f;
    float buttonH = 50.0f;
    float spacing = 15.0f;
    
    // Title centered near top
    sf::FloatRect titleBounds = m_title->getLocalBounds();
    m_title->setOrigin(sf::Vector2f(
        titleBounds.position.x + titleBounds.size.x / 2.0f,
        titleBounds.position.y + titleBounds.size.y / 2.0f
    ));
    m_title->setPosition(sf::Vector2f(centerX, centerY - 120.0f));
    
    // Buttons centered below title
    float startY = centerY - 20.0f;
    
    m_newGameButton = createButton("New Game", 
        sf::Vector2f(centerX - buttonW / 2.0f, startY), 
        sf::Vector2f(buttonW, buttonH));
    
    m_editorButton = createButton("Map Editor",
        sf::Vector2f(centerX - buttonW / 2.0f, startY + buttonH + spacing),
        sf::Vector2f(buttonW, buttonH));
    
    m_quitButton = createButton("Quit",
        sf::Vector2f(centerX - buttonW / 2.0f, startY + 2 * (buttonH + spacing)),
        sf::Vector2f(buttonW, buttonH));
}

void MenuScreen::layoutSlotPicker(sf::Vector2u windowSize) {
    float centerX = windowSize.x / 2.0f;
    float centerY = windowSize.y / 2.0f;
    float buttonW = 280.0f;
    float buttonH = 50.0f;
    float spacing = 15.0f;

    m_slotPickerTitle.emplace(*m_font, "Play as...", 32);
    m_slotPickerTitle->setFillColor(sf::Color::White);
    sf::FloatRect tb = m_slotPickerTitle->getLocalBounds();
    m_slotPickerTitle->setOrigin(sf::Vector2f(
        tb.position.x + tb.size.x / 2.0f,
        tb.position.y + tb.size.y / 2.0f
    ));
    m_slotPickerTitle->setPosition(sf::Vector2f(centerX, centerY - 120.0f));

    m_slotButtons.clear();
    float startY = centerY - 40.0f;
    for (int i = 0; i < m_slotPlayerCount; ++i) {
        std::string label = "Player " + std::to_string(i + 1);
        Button btn = createButton(label,
            sf::Vector2f(centerX - buttonW / 2.0f, startY + i * (buttonH + spacing)),
            sf::Vector2f(buttonW, buttonH));
        m_slotButtons.push_back(std::move(btn));
    }

    float backY = startY + m_slotPlayerCount * (buttonH + spacing) + 10.0f;
    m_slotBackButton = createButton("Back",
        sf::Vector2f(centerX - buttonW / 2.0f, backY),
        sf::Vector2f(buttonW, buttonH));
}

int MenuScreen::getMapPlayerCount(const std::string& mapFile) const {
    if (mapFile == "default") return 2;
    auto data = MapSerializer::load("maps/" + mapFile + ".stmap");
    if (data) return std::max(2, data->playerCount);
    return 2;
}

void MenuScreen::layoutMapSelection(sf::Vector2u windowSize) {
    float centerX = windowSize.x / 2.0f;
    float centerY = windowSize.y / 2.0f;
    float buttonW = 280.0f;
    float buttonH = 40.0f;
    float spacing = 10.0f;
    
    // Title for map selection
    m_mapSelectionTitle.emplace(*m_font, "Select Map", 32);
    m_mapSelectionTitle->setFillColor(sf::Color::White);
    sf::FloatRect titleBounds = m_mapSelectionTitle->getLocalBounds();
    m_mapSelectionTitle->setOrigin(sf::Vector2f(
        titleBounds.position.x + titleBounds.size.x / 2.0f,
        titleBounds.position.y + titleBounds.size.y / 2.0f
    ));
    m_mapSelectionTitle->setPosition(sf::Vector2f(centerX, centerY - 160.0f));
    
    // Map buttons
    m_mapButtons.clear();
    float startY = centerY - 100.0f;
    
    for (size_t i = 0; i < m_mapFiles.size(); ++i) {
        std::string label = m_mapFiles[i];
        if (label == "default") label = "Default Map";
        
        Button btn = createButton(label,
            sf::Vector2f(centerX - buttonW / 2.0f, startY + i * (buttonH + spacing)),
            sf::Vector2f(buttonW, buttonH));
        m_mapButtons.push_back(std::move(btn));
    }
    
    // Back button at bottom
    float backY = startY + m_mapFiles.size() * (buttonH + spacing) + 20.0f;
    m_backButton = createButton("Back",
        sf::Vector2f(centerX - buttonW / 2.0f, backY),
        sf::Vector2f(buttonW, buttonH));
}

ScreenResult MenuScreen::handleEvent(const sf::Event& event) {
    if (const auto* mouseMove = event.getIf<sf::Event::MouseMoved>()) {
        sf::Vector2f mousePos(static_cast<float>(mouseMove->position.x), 
                             static_cast<float>(mouseMove->position.y));
        
        if (m_showSlotPicker) {
            for (auto& btn : m_slotButtons) updateButtonHover(btn, mousePos);
            updateButtonHover(m_slotBackButton, mousePos);
        } else if (m_showMapSelection) {
            for (auto& btn : m_mapButtons) {
                updateButtonHover(btn, mousePos);
            }
            updateButtonHover(m_backButton, mousePos);
        } else {
            updateButtonHover(m_newGameButton, mousePos);
            updateButtonHover(m_editorButton, mousePos);
            updateButtonHover(m_quitButton, mousePos);
        }
    }
    
    if (const auto* mouseClick = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mouseClick->button == sf::Mouse::Button::Left) {
            sf::Vector2f mousePos(static_cast<float>(mouseClick->position.x),
                                 static_cast<float>(mouseClick->position.y));
            
            if (m_showSlotPicker) {
                // Slot buttons
                for (size_t i = 0; i < m_slotButtons.size(); ++i) {
                    if (isMouseOver(m_slotButtons[i], mousePos)) {
                        ScreenResult result;
                        result.action = ScreenResult::Action::StartGame;
                        result.mapFile = m_selectedMapFile;
                        result.localPlayerSlot = static_cast<int>(i);
                        return result;
                    }
                }
                if (isMouseOver(m_slotBackButton, mousePos)) {
                    m_showSlotPicker = false;
                    m_showMapSelection = true;
                }
            } else if (m_showMapSelection) {
                // Check map buttons
                for (size_t i = 0; i < m_mapButtons.size(); ++i) {
                    if (isMouseOver(m_mapButtons[i], mousePos)) {
                        m_selectedMapFile   = m_mapFiles[i];
                        m_slotPlayerCount   = getMapPlayerCount(m_mapFiles[i]);
                        layoutSlotPicker(m_lastWinSize);
                        m_showSlotPicker    = true;
                        m_showMapSelection  = false;
                        return {};
                    }
                }
                
                // Back button
                if (isMouseOver(m_backButton, mousePos)) {
                    m_showMapSelection = false;
                }
            } else {
                // Main menu buttons
                if (isMouseOver(m_newGameButton, mousePos)) {
                    scanForMaps();
                    layoutMapSelection(m_lastWinSize);
                    m_showMapSelection = true;
                }
                
                if (isMouseOver(m_editorButton, mousePos)) {
                    return { ScreenResult::Action::OpenEditor, "" };
                }
                
                if (isMouseOver(m_quitButton, mousePos)) {
                    return { ScreenResult::Action::Quit, "" };
                }
            }
        }
    }
    
    if (const auto* resized = event.getIf<sf::Event::Resized>()) {
        sf::Vector2u newSize = resized->size;
        sf::View view(sf::FloatRect(sf::Vector2f(0.f, 0.f), sf::Vector2f(static_cast<float>(newSize.x), static_cast<float>(newSize.y))));
        // View will be applied in render
        layoutButtons(newSize);
        layoutMapSelection(newSize);
        layoutSlotPicker(newSize);
    }
    
    return { ScreenResult::Action::None, "" };
}

ScreenResult MenuScreen::update(float deltaTime) {
    // Menu has no per-frame logic
    return { ScreenResult::Action::None, "" };
}

void MenuScreen::render(sf::RenderWindow& window) {
    // Set default view so UI renders in pixel coordinates
    sf::Vector2u winSize = window.getSize();
    sf::View uiView(sf::FloatRect(sf::Vector2f(0.f, 0.f), 
                    sf::Vector2f(static_cast<float>(winSize.x), static_cast<float>(winSize.y))));
    window.setView(uiView);
    
    window.clear(sf::Color(30, 35, 40));
    
    if (m_showSlotPicker) {
        // Draw slot picker panel
        window.draw(*m_slotPickerTitle);
        for (auto& btn : m_slotButtons) {
            window.draw(btn.shape);
            window.draw(*btn.label);
        }
        window.draw(m_slotBackButton.shape);
        window.draw(*m_slotBackButton.label);
    } else if (m_showMapSelection) {
        // Draw map selection panel
        window.draw(*m_mapSelectionTitle);
        
        for (auto& btn : m_mapButtons) {
            window.draw(btn.shape);
            window.draw(*btn.label);
        }
        
        window.draw(m_backButton.shape);
        window.draw(*m_backButton.label);
    } else {
        // Draw main menu
        window.draw(*m_title);
        
        window.draw(m_newGameButton.shape);
        window.draw(*m_newGameButton.label);
        
        window.draw(m_editorButton.shape);
        window.draw(*m_editorButton.label);
        
        window.draw(m_quitButton.shape);
        window.draw(*m_quitButton.label);
    }
}
