#include "MenuScreen.h"
#include "Constants.h"
#include <filesystem>
#include <algorithm>
#include <iostream>

namespace fs = std::filesystem;

MenuScreen::MenuScreen() {
    // Try to load a font - use SFML's default or a system font
    if (!m_font.openFromFile("C:/Windows/Fonts/arial.ttf")) {
        // Try another common font
        if (!m_font.openFromFile("C:/Windows/Fonts/segoeui.ttf")) {
            std::cerr << "MenuScreen: Could not load any font" << std::endl;
        } else {
            m_fontLoaded = true;
        }
    } else {
        m_fontLoaded = true;
    }
    
    // Title
    m_title.emplace(m_font, "Strategy Game", 48);
    m_title->setFillColor(sf::Color::White);
    
    // Scan for available maps
    scanForMaps();
    
    // Layout will be done in render based on window size
    sf::Vector2u defaultSize(Constants::WINDOW_WIDTH, Constants::WINDOW_HEIGHT);
    layoutButtons(defaultSize);
    layoutMapSelection(defaultSize);
}

void MenuScreen::scanForMaps() {
    m_mapFiles.clear();
    
    // Always include "default" option (procedurally generated map)
    m_mapFiles.push_back("default");
    
    // Scan assets/maps/ for .map files
    std::string mapsDir = "assets/maps";
    try {
        if (fs::exists(mapsDir) && fs::is_directory(mapsDir)) {
            for (const auto& entry : fs::directory_iterator(mapsDir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".map") {
                    m_mapFiles.push_back(entry.path().stem().string());
                }
            }
        }
    } catch (...) {
        // Directory doesn't exist yet, that's fine
    }
    
    // Sort map names (keep "default" first)
    if (m_mapFiles.size() > 1) {
        std::sort(m_mapFiles.begin() + 1, m_mapFiles.end());
    }
}

MenuScreen::Button MenuScreen::createButton(const std::string& text, sf::Vector2f position, sf::Vector2f size) {
    Button button;
    
    button.shape.setSize(size);
    button.shape.setPosition(position);
    button.shape.setFillColor(sf::Color(60, 60, 60));
    button.shape.setOutlineColor(sf::Color(100, 100, 100));
    button.shape.setOutlineThickness(2.0f);
    
    button.label.emplace(m_font, text, 22);
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

void MenuScreen::layoutMapSelection(sf::Vector2u windowSize) {
    float centerX = windowSize.x / 2.0f;
    float centerY = windowSize.y / 2.0f;
    float buttonW = 280.0f;
    float buttonH = 40.0f;
    float spacing = 10.0f;
    
    // Title for map selection
    m_mapSelectionTitle.emplace(m_font, "Select Map", 32);
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
        
        if (m_showMapSelection) {
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
            
            if (m_showMapSelection) {
                // Check map buttons
                for (size_t i = 0; i < m_mapButtons.size(); ++i) {
                    if (isMouseOver(m_mapButtons[i], mousePos)) {
                        ScreenResult result;
                        result.action = ScreenResult::Action::StartGame;
                        result.mapFile = m_mapFiles[i];
                        return result;
                    }
                }
                
                // Back button
                if (isMouseOver(m_backButton, mousePos)) {
                    m_showMapSelection = false;
                }
            } else {
                // Main menu buttons
                if (isMouseOver(m_newGameButton, mousePos)) {
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
    
    if (m_showMapSelection) {
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
