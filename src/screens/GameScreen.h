#pragma once

#include "screens/Screen.h"
#include "game/Game.h"
#include "ui/ActionBar.h"
#include <memory>
#include <string>

class IRenderer;
class InputHandler;
class DebugConsole;

// ---------------------------------------------------------------------------
// GameScreen — presents one running match (Game) to the player.
//
// Owns everything that is not simulation: the renderer (2D/3D, switched with
// F9), the input handler (camera, selection, commands), the action bar and the
// debug console.  Game itself knows none of them.
// ---------------------------------------------------------------------------
class GameScreen : public Screen {
public:
    GameScreen(sf::RenderWindow& window, const std::string& mapFile, int localPlayerSlot = 0);
    ~GameScreen() override;
    
    ScreenResult handleEvent(const sf::Event& event) override;
    ScreenResult update(float deltaTime) override;
    void render(sf::RenderWindow& window) override;
    
private:
    // F9: switch between Renderer2D and Renderer3D (stays on the current one if 3D is unavailable).
    void toggleRenderer();

    sf::RenderWindow&              m_window;
    std::unique_ptr<Game>          m_game;
    ActionBar                      m_actionBar;
    std::unique_ptr<IRenderer>     m_renderer;
    std::unique_ptr<InputHandler>  m_input;
    std::unique_ptr<DebugConsole>  m_debugConsole;
    bool                           m_use3D = false;
};
