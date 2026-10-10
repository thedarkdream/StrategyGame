#pragma once

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

class Game;

// ---------------------------------------------------------------------------
// In-game debug console, Half-Life / Quake style.
//
//  Toggle open/closed with the ~ key.
//  While open:
//    Enter        → execute command (console stays open)
//    Escape / ~   → close console
//    Up / Down    → navigate command history
//    PgUp / PgDn  → scroll log
//    Mouse wheel  → scroll log
//
//  Command syntax (convar system):
//    <varname>          → prints current value: "varname = 1"
//    <varname> <value>  → sets integer value (0 = false, 1 = true)
// ---------------------------------------------------------------------------
class DebugConsole {
public:
    DebugConsole(sf::RenderWindow& window, Game& game);

    // Feed an SFML event. Returns true if the event was consumed.
    bool handleEvent(const sf::Event& event);

    // Draw A* path waypoints for all selected units (call in world-space view).
    void renderWaypoints(sf::RenderWindow& window);

    // Draw entity IDs above all selected entities (call in world-space view).
    void renderIds(sf::RenderWindow& window);

    // Draw the console overlay in screen-space (call in UI view).
    void render(sf::RenderWindow& window);

    bool isActive()           const { return m_active; }
    bool isShowingWaypoints() const { return m_showWaypoints; }
    bool isShowingIds()       const { return m_showIds; }
    bool isShowingState()     const { return m_showState; }

private:
    sf::RenderWindow& m_window;
    Game&             m_game;

    bool        m_active        = false;
    bool        m_showWaypoints = false;
    bool        m_showIds       = false;
    bool        m_showState     = false;

    std::string m_inputText;

    // Full scrollback log (newest at the end)
    std::vector<std::string> m_log;
    static constexpr int MAX_LOG_LINES = 512;

    // How many lines are scrolled up from the bottom (0 = show newest)
    int m_scrollOffset = 0;

    // Command history (most-recent at end)
    std::vector<std::string> m_history;
    int m_historyIndex = -1;   // -1 = not navigating
    static constexpr int MAX_HISTORY = 64;

    const sf::Font* m_font{ nullptr };

    // Console variable registry
    struct ConVar {
        std::function<int()>     getter;
        std::function<void(int)> setter;
        std::string              description;
    };
    std::unordered_map<std::string, ConVar> m_convars;

    void registerConVars();
    void cmdShowAIInfo();
    void executeCommand(const std::string& cmd);
    void log(const std::string& msg);
    void pushHistory(const std::string& cmd);
};
