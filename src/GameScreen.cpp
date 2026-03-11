#include "GameScreen.h"
#include "GameStatistics.h"

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
    GameState state = m_game->getState();
    
    if (state == GameState::Playing) {
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

void GameScreen::render(sf::RenderWindow& /*window*/) {
    // Game uses its own stored window reference and Renderer
    m_game->render();
}
