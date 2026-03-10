#include "MapEditorScreen.h"
#include "Constants.h"
#include <iostream>

MapEditorScreen::MapEditorScreen() {
    if (!m_font.openFromFile("C:/Windows/Fonts/arial.ttf")) {
        if (!m_font.openFromFile("C:/Windows/Fonts/segoeui.ttf")) {
            std::cerr << "MapEditorScreen: Could not load font" << std::endl;
        } else {
            m_fontLoaded = true;
        }
    } else {
        m_fontLoaded = true;
    }
    
    m_title.emplace(m_font, "Map Editor", 48);
    m_title->setFillColor(sf::Color::White);
    
    m_subtitle.emplace(m_font, "Coming Soon", 24);
    m_subtitle->setFillColor(sf::Color(180, 180, 180));
    
    // Back button
    m_backButton.setSize(sf::Vector2f(200.0f, 45.0f));
    m_backButton.setFillColor(sf::Color(60, 60, 60));
    m_backButton.setOutlineColor(sf::Color(100, 100, 100));
    m_backButton.setOutlineThickness(2.0f);
    
    m_backLabel.emplace(m_font, "Back to Menu", 20);
    m_backLabel->setFillColor(sf::Color::White);
}

ScreenResult MapEditorScreen::handleEvent(const sf::Event& event) {
    if (const auto* mouseClick = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mouseClick->button == sf::Mouse::Button::Left) {
            sf::Vector2f mousePos(static_cast<float>(mouseClick->position.x),
                                 static_cast<float>(mouseClick->position.y));
            sf::FloatRect backBounds(m_backButton.getPosition(), m_backButton.getSize());
            if (backBounds.contains(mousePos)) {
                return { ScreenResult::Action::BackToMenu, "" };
            }
        }
    }
    
    if (const auto* mouseMove = event.getIf<sf::Event::MouseMoved>()) {
        sf::Vector2f mousePos(static_cast<float>(mouseMove->position.x),
                             static_cast<float>(mouseMove->position.y));
        sf::FloatRect backBounds(m_backButton.getPosition(), m_backButton.getSize());
        if (backBounds.contains(mousePos)) {
            m_backButton.setFillColor(sf::Color(80, 80, 100));
            m_backButton.setOutlineColor(sf::Color(150, 150, 200));
        } else {
            m_backButton.setFillColor(sf::Color(60, 60, 60));
            m_backButton.setOutlineColor(sf::Color(100, 100, 100));
        }
    }
    
    return { ScreenResult::Action::None, "" };
}

ScreenResult MapEditorScreen::update(float deltaTime) {
    return { ScreenResult::Action::None, "" };
}

void MapEditorScreen::render(sf::RenderWindow& window) {
    sf::Vector2u winSize = window.getSize();
    sf::View uiView(sf::FloatRect(sf::Vector2f(0.f, 0.f),
                    sf::Vector2f(static_cast<float>(winSize.x), static_cast<float>(winSize.y))));
    window.setView(uiView);
    
    window.clear(sf::Color(25, 30, 35));
    
    float centerX = winSize.x / 2.0f;
    float centerY = winSize.y / 2.0f;
    
    // Center title
    sf::FloatRect titleBounds = m_title->getLocalBounds();
    m_title->setOrigin(sf::Vector2f(titleBounds.position.x + titleBounds.size.x / 2.0f,
                                    titleBounds.position.y + titleBounds.size.y / 2.0f));
    m_title->setPosition(sf::Vector2f(centerX, centerY - 60.0f));
    window.draw(*m_title);
    
    // Center subtitle
    sf::FloatRect subBounds = m_subtitle->getLocalBounds();
    m_subtitle->setOrigin(sf::Vector2f(subBounds.position.x + subBounds.size.x / 2.0f,
                                       subBounds.position.y + subBounds.size.y / 2.0f));
    m_subtitle->setPosition(sf::Vector2f(centerX, centerY));
    window.draw(*m_subtitle);
    
    // Back button
    float btnW = m_backButton.getSize().x;
    float btnH = m_backButton.getSize().y;
    m_backButton.setPosition(sf::Vector2f(centerX - btnW / 2.0f, centerY + 60.0f));
    window.draw(m_backButton);
    
    sf::FloatRect backLabelBounds = m_backLabel->getLocalBounds();
    m_backLabel->setOrigin(sf::Vector2f(backLabelBounds.position.x + backLabelBounds.size.x / 2.0f,
                                        backLabelBounds.position.y + backLabelBounds.size.y / 2.0f));
    m_backLabel->setPosition(sf::Vector2f(centerX, centerY + 60.0f + btnH / 2.0f));
    window.draw(*m_backLabel);
}
