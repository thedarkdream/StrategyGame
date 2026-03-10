#include "Application.h"
#include "MenuScreen.h"
#include "GameScreen.h"
#include "MapEditorScreen.h"
#include "Constants.h"

Application::Application()
    : m_window(sf::VideoMode(sf::Vector2u(Constants::WINDOW_WIDTH, Constants::WINDOW_HEIGHT)),
               "Strategy Game", sf::Style::Default)
{
    m_window.setFramerateLimit(Constants::FRAME_RATE);
    switchToMenu();
}

void Application::run() {
    sf::Clock clock;
    
    while (m_window.isOpen()) {
        float deltaTime = clock.restart().asSeconds();
        
        // Process events
        while (const auto event = m_window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                m_window.close();
                return;
            }
            
            if (m_currentScreen) {
                ScreenResult result = m_currentScreen->handleEvent(*event);
                if (result.action != ScreenResult::Action::None) {
                    handleScreenResult(result);
                    break;  // Screen changed, stop processing old screen's events
                }
            }
        }
        
        // Update
        if (m_currentScreen) {
            ScreenResult result = m_currentScreen->update(deltaTime);
            if (result.action != ScreenResult::Action::None) {
                handleScreenResult(result);
            }
        }
        
        // Render
        m_window.clear(sf::Color(30, 30, 30));
        if (m_currentScreen) {
            m_currentScreen->render(m_window);
        }
        m_window.display();
    }
}

void Application::handleScreenResult(const ScreenResult& result) {
    switch (result.action) {
        case ScreenResult::Action::StartGame:
            switchToGame(result.mapFile);
            break;
        case ScreenResult::Action::OpenEditor:
            switchToEditor();
            break;
        case ScreenResult::Action::BackToMenu:
            switchToMenu();
            break;
        case ScreenResult::Action::Quit:
            m_window.close();
            break;
        default:
            break;
    }
}

void Application::switchToMenu() {
    if (m_currentScreen) m_currentScreen->onExit();
    m_currentScreen = std::make_unique<MenuScreen>();
    m_currentScreen->onEnter();
    
    // Reset view to default for menu
    m_window.setView(m_window.getDefaultView());
}

void Application::switchToGame(const std::string& mapFile) {
    if (m_currentScreen) m_currentScreen->onExit();
    m_currentScreen = std::make_unique<GameScreen>(m_window, mapFile);
    m_currentScreen->onEnter();
}

void Application::switchToEditor() {
    if (m_currentScreen) m_currentScreen->onExit();
    m_currentScreen = std::make_unique<MapEditorScreen>();
    m_currentScreen->onEnter();
    
    // Reset view to default for editor
    m_window.setView(m_window.getDefaultView());
}
