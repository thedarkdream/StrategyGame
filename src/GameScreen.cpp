#include "GameScreen.h"

GameScreen::GameScreen(sf::RenderWindow& window, const std::string& mapFile, int localPlayerSlot)
    : m_game(std::make_unique<Game>(window, mapFile, localPlayerSlot))
{
}

ScreenResult GameScreen::handleEvent(const sf::Event& event) {
    // F10 returns to the main menu
    if (const auto* keyPressed = event.getIf<sf::Event::KeyPressed>()) {
        if (keyPressed->code == sf::Keyboard::Key::F10) {
            return { ScreenResult::Action::BackToMenu, "" };
        }
    }
    
    m_game->handleEvent(event);
    return {};
}

ScreenResult GameScreen::update(float deltaTime) {
    if (m_game->getState() == GameState::Playing) {
        m_game->update(deltaTime);
    }
    return {};
}

void GameScreen::render(sf::RenderWindow& /*window*/) {
    // Game uses its own stored window reference and Renderer
    m_game->render();
}
