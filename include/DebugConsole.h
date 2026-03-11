#pragma once

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

class Game;

// In-game debug console.
// Press Enter to open, type a command, press Enter again to execute.
// Press Escape to dismiss without executing.
class DebugConsole {
public:
    DebugConsole(sf::RenderWindow& window, Game& game);

    // Feed an SFML event. Returns true if the event was consumed and should
    // NOT be forwarded to the normal input handler.
    bool handleEvent(const sf::Event& event);

    // Draw A* path waypoints for all currently-selected units (world-space).
    // Call this while the world camera view is active on the window.
    void renderWaypoints(sf::RenderWindow& window);

    // Draw the console input bar / log in screen-space (call after switching
    // to the UI view).
    void render(sf::RenderWindow& window);

    bool isActive()          const { return m_active; }
    bool isShowingWaypoints() const { return m_showWaypoints; }

private:
    sf::RenderWindow& m_window;
    Game&             m_game;

    bool        m_active        = false;
    bool        m_showWaypoints = false;
    std::string m_inputText;

    // Small scrollback log shown above the input bar
    std::vector<std::string> m_log;
    static constexpr int MAX_LOG_LINES = 4;

    const sf::Font* m_font{ nullptr };

    void executeCommand(const std::string& cmd);
    void log(const std::string& msg);
};
