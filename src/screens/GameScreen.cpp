#include "screens/GameScreen.h"
#include "game/GameStatistics.h"
#include "ui/InputHandler.h"
#include "ui/DebugConsole.h"
#include "render/IRenderer.h"
#include "render2d/Renderer2D.h"
#include "render2d/CameraSFML.h"
#include "render3d/Renderer3D.h"
#include "media/SoundManager.h"
#include <iostream>

GameScreen::GameScreen(sf::RenderWindow& window, const std::string& mapFile, int localPlayerSlot)
    : m_window(window)
    , m_game(std::make_unique<Game>(mapFile, localPlayerSlot))
    , m_renderer(std::make_unique<Renderer2D>(window))
    , m_input(std::make_unique<InputHandler>(window, *m_game, m_actionBar))
    , m_debugConsole(std::make_unique<DebugConsole>(window, *m_game))
{
    m_input->setRenderer(m_renderer.get());
    if (auto start = m_game->getLocalStartPosition())
        m_input->centerCameraAt(*start);
}

GameScreen::~GameScreen() = default;

void GameScreen::toggleRenderer() {
    if (m_use3D) {
        m_renderer = std::make_unique<Renderer2D>(m_window);
        m_use3D = false;
    } else {
        try {
            m_renderer = std::make_unique<Renderer3D>(m_window);
            m_use3D = true;
        } catch (const std::exception& e) {
            std::cerr << "3D renderer unavailable: " << e.what() << std::endl;
            return;
        }
    }
    m_input->setRenderer(m_renderer.get());
}

ScreenResult GameScreen::handleEvent(const sf::Event& event) {
    // F10 returns to the main menu
    if (const auto* keyPressed = event.getIf<sf::Event::KeyPressed>()) {
        if (keyPressed->code == sf::Keyboard::Key::F10) {
            return { ScreenResult::Action::BackToMenu, "" };
        }
        // F9 toggles between the 2D and the (experimental) 3D renderer.
        if (keyPressed->code == sf::Keyboard::Key::F9) {
            toggleRenderer();
            return {};
        }
    }
    
    if (const auto* resized = event.getIf<sf::Event::Resized>()) {
        m_input->onWindowResize(resized->size);
    }

    // Debug console gets first refusal; if it consumes the event, stop here.
    if (m_debugConsole->handleEvent(event)) {
        return {};
    }

    m_input->handleEvent(event);
    return {};
}

ScreenResult GameScreen::update(float deltaTime) {
    GameState state = m_game->getState();
    
    if (state == GameState::Playing) {
        m_input->update(deltaTime);   // camera movement
        SOUNDS.setListenerPosition(m_input->getCamera().getCenter());
        m_game->update(deltaTime);
        
        // Check if game state changed after update
        state = m_game->getState();
    }
    
    // Handle victory or defeat
    if (state == GameState::Victory || state == GameState::Defeat) {
        ScreenResult result;
        result.action = ScreenResult::Action::ShowVictory;
        result.isVictory = (state == GameState::Victory);
        result.localPlayerSlot = m_game->getLocalSlot();
        result.stats = std::make_shared<GameStatistics>(m_game->getStatistics());
        return result;
    }
    
    return {};
}

void GameScreen::render(sf::RenderWindow& window) {
    m_renderer->setCamera(m_input->getCamera());
    FrameContext frame{ *m_input, m_actionBar };
    m_renderer->render(*m_game, frame);  // ends with UI view active on the window

    // Debug overlays: world-space (waypoints + IDs), then the console in screen space
    window.setView(toSfView(m_input->getCamera()));
    m_debugConsole->renderWaypoints(window);
    m_debugConsole->renderIds(window);

    sf::Vector2u winSize = window.getSize();
    sf::View uiView(sf::FloatRect(
        sf::Vector2f(0.f, 0.f),
        sf::Vector2f(static_cast<float>(winSize.x), static_cast<float>(winSize.y))));
    window.setView(uiView);
    m_debugConsole->render(window);
}
