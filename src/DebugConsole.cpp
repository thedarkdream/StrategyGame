#include "DebugConsole.h"
#include "FontManager.h"
#include "Game.h"
#include "Player.h"
#include "Unit.h"
#include "Entity.h"
#include <algorithm>
#include <sstream>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace {
    // Trim leading/trailing whitespace from a string
    std::string trim(const std::string& s) {
        size_t start = s.find_first_not_of(" \t");
        if (start == std::string::npos) return {};
        size_t end = s.find_last_not_of(" \t");
        return s.substr(start, end - start + 1);
    }
}

// ---------------------------------------------------------------------------
DebugConsole::DebugConsole(sf::RenderWindow& window, Game& game)
    : m_window(window)
    , m_game(game)
{
    m_font = FontManager::instance().defaultFont();
}

// ---------------------------------------------------------------------------
// Event handling
// ---------------------------------------------------------------------------
bool DebugConsole::handleEvent(const sf::Event& event) {
    // --- Key pressed ---
    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        if (!m_active) {
            // Open console on Enter
            if (kp->code == sf::Keyboard::Key::Enter) {
                m_active    = true;
                m_inputText.clear();
                return true;   // consumed
            }
            return false;      // not our event
        }

        // Console is active — handle navigation keys
        if (kp->code == sf::Keyboard::Key::Escape) {
            m_active = false;
            m_inputText.clear();
            return true;
        }
        if (kp->code == sf::Keyboard::Key::Enter) {
            std::string cmd = trim(m_inputText);
            if (!cmd.empty()) {
                executeCommand(cmd);
            }
            m_active = false;
            m_inputText.clear();
            return true;
        }
        if (kp->code == sf::Keyboard::Key::Backspace) {
            if (!m_inputText.empty()) {
                m_inputText.pop_back();
            }
            return true;
        }
        // Consume all other key presses while console is active so they
        // don't trigger hotkeys / unit commands.
        return true;
    }

    // --- Text entered (printable characters) ---
    if (const auto* te = event.getIf<sf::Event::TextEntered>()) {
        if (!m_active) return false;

        uint32_t ch = te->unicode;
        // Accept printable ASCII only; skip backspace (handled above) and
        // enter (handled above).
        if (ch >= 32 && ch < 127) {
            m_inputText += static_cast<char>(ch);
        }
        return true;
    }

    // If console active, swallow mouse events too so clicks don't issue
    // unit commands while typing.
    if (m_active) {
        if (event.is<sf::Event::MouseButtonPressed>() ||
            event.is<sf::Event::MouseButtonReleased>() ||
            event.is<sf::Event::MouseMoved>()) {
            return true;
        }
    }

    return false;
}

// ---------------------------------------------------------------------------
// Command execution
// ---------------------------------------------------------------------------
void DebugConsole::executeCommand(const std::string& cmd) {
    // Tokenise
    std::istringstream ss(cmd);
    std::string verb;
    ss >> verb;
    // Normalise to lowercase
    std::transform(verb.begin(), verb.end(), verb.begin(), ::tolower);

    if (verb == "show_waypoints") {
        m_showWaypoints = !m_showWaypoints;
        log(std::string("Waypoints display: ") + (m_showWaypoints ? "ON" : "OFF"));
    } else {
        log("Unknown command: " + verb);
    }
}

void DebugConsole::log(const std::string& msg) {
    m_log.push_back(msg);
    if (static_cast<int>(m_log.size()) > MAX_LOG_LINES) {
        m_log.erase(m_log.begin());
    }
}

// ---------------------------------------------------------------------------
// Waypoint rendering  (call with WORLD-SPACE view active on the window)
// ---------------------------------------------------------------------------
void DebugConsole::renderWaypoints(sf::RenderWindow& window) {
    if (!m_showWaypoints) return;

    // Collect all selected entities (own units + inspected enemy units)
    const EntityList& all = m_game.getAllEntities();
    for (const auto& entity : all) {
        if (!entity || !entity->isAlive()) continue;
        if (!entity->isSelected()) continue;

        const Unit* unit = entity->asUnit();
        if (!unit) continue;

        const auto& path = unit->getPath();
        if (path.empty()) continue;

        size_t pathIdx = unit->getPathIndex();

        // Draw lines between consecutive remaining waypoints
        for (size_t i = pathIdx; i + 1 < path.size(); ++i) {
            sf::Color lineColor(255, 200, 0, 180);  // orange-yellow
            sf::Vertex line[] = {
                sf::Vertex{path[i],     lineColor},
                sf::Vertex{path[i + 1], lineColor}
            };
            window.draw(line, 2, sf::PrimitiveType::Lines);
        }

        // Draw a circle at each remaining waypoint
        for (size_t i = pathIdx; i < path.size(); ++i) {
            constexpr float DOT_RADIUS = 5.0f;
            sf::CircleShape dot(DOT_RADIUS);
            dot.setOrigin(sf::Vector2f(DOT_RADIUS, DOT_RADIUS));
            dot.setPosition(path[i]);

            if (i == pathIdx) {
                // Current/next waypoint: bright green
                dot.setFillColor(sf::Color(0, 255, 100, 220));
                dot.setOutlineColor(sf::Color(0, 180, 60));
            } else {
                // Future waypoints: yellow
                dot.setFillColor(sf::Color(255, 220, 0, 180));
                dot.setOutlineColor(sf::Color(200, 160, 0));
            }
            dot.setOutlineThickness(1.5f);
            window.draw(dot);
        }

        // Draw a line from unit position to the first waypoint
        if (pathIdx < path.size()) {
            sf::Color leadColor(100, 200, 255, 160);
            sf::Vertex leadLine[] = {
                sf::Vertex{entity->getPosition(), leadColor},
                sf::Vertex{path[pathIdx],         leadColor}
            };
            window.draw(leadLine, 2, sf::PrimitiveType::Lines);
        }
    }
}

// ---------------------------------------------------------------------------
// Console UI rendering  (call in SCREEN-SPACE / UI view)
// ---------------------------------------------------------------------------
void DebugConsole::render(sf::RenderWindow& window) {
    // Always show log lines (even when console is closed) until cleared
    if (!m_active && m_log.empty()) return;
    if (!m_font) return;

    sf::Vector2u winSize = window.getSize();

    constexpr float LINE_HEIGHT  = 22.0f;
    constexpr float PADDING      = 8.0f;
    constexpr unsigned FONT_SIZE = 16;

    // Number of lines to show
    int logCount  = static_cast<int>(m_log.size());
    int lineCount = logCount + (m_active ? 1 : 0);
    if (lineCount == 0) return;

    float panelHeight = static_cast<float>(lineCount) * LINE_HEIGHT + PADDING * 2.0f;
    float panelY      = static_cast<float>(winSize.y) - panelHeight;

    // Semi-transparent background
    sf::RectangleShape bg(sf::Vector2f(static_cast<float>(winSize.x), panelHeight));
    bg.setPosition(sf::Vector2f(0.0f, panelY));
    bg.setFillColor(sf::Color(10, 10, 20, 200));
    bg.setOutlineThickness(1.0f);
    bg.setOutlineColor(sf::Color(80, 80, 120, 200));
    window.draw(bg);

    // Log lines
    for (int i = 0; i < logCount; ++i) {
        float y = panelY + PADDING + static_cast<float>(i) * LINE_HEIGHT;
        sf::Text line(*m_font, m_log[i], FONT_SIZE);
        line.setFillColor(sf::Color(180, 180, 180));
        line.setPosition(sf::Vector2f(PADDING, y));
        window.draw(line);
    }

    // Input bar (only when active)
    if (m_active) {
        float y = panelY + PADDING + static_cast<float>(logCount) * LINE_HEIGHT;
        std::string display = "> " + m_inputText + "_";
        sf::Text inputText(*m_font, display, FONT_SIZE);
        inputText.setFillColor(sf::Color::White);
        inputText.setPosition(sf::Vector2f(PADDING, y));
        window.draw(inputText);
    }
}
