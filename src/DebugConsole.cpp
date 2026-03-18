#include "DebugConsole.h"
#include "FontManager.h"
#include "Game.h"
#include "Player.h"
#include "FogOfWar.h"
#include "Unit.h"
#include "Entity.h"
#include "PlayerController.h"
#include "MathUtil.h"
#include <algorithm>
#include <sstream>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace {
    std::string trim(const std::string& s) {
        size_t start = s.find_first_not_of(" \t");
        if (start == std::string::npos) return {};
        size_t end = s.find_last_not_of(" \t");
        return s.substr(start, end - start + 1);
    }

    std::string toLower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return s;
    }
}

// ---------------------------------------------------------------------------
DebugConsole::DebugConsole(sf::RenderWindow& window, Game& game)
    : m_window(window)
    , m_game(game)
{
    m_font = FontManager::instance().defaultFont();
    registerConVars();
    log("Debug console ready.  Type a convar name to query its value.");
    log("Usage:  <convar>         - show value");
    log("        <convar> <value> - set value  (1=true, 0=false)");
    log("-------------------------------------------------------");
}

// ---------------------------------------------------------------------------
// ConVar registration
// ---------------------------------------------------------------------------
void DebugConsole::registerConVars() {
    // reveal_map — disable/enable FogOfWar for the local player
    m_convars["reveal_map"] = {
        [this]() -> int {
            return m_game.getPlayer().getFog().isRevealed() ? 1 : 0;
        },
        [this](int val) {
            FogOfWar& fog = m_game.getPlayer().getFog();
            fog.setRevealed(val != 0);
            fog.rebuildTexture();
        },
        "Reveal entire map for local player (disable fog of war)"
    };

    // show_waypoints — render A* path lines for selected units
    m_convars["show_waypoints"] = {
        [this]() -> int { return m_showWaypoints ? 1 : 0; },
        [this](int val) { m_showWaypoints = (val != 0); },
        "Draw A* path waypoints above selected units"
    };

    // show_ids — render entity IDs above selected entities
    m_convars["show_ids"] = {
        [this]() -> int { return m_showIds ? 1 : 0; },
        [this](int val) { m_showIds = (val != 0); },
        "Draw entity IDs above selected entities"
    };

    // show_state — render unit state (IDLE, MOVING, …) above selected units
    m_convars["show_state"] = {
        [this]() -> int { return m_showState ? 1 : 0; },
        [this](int val) { m_showState = (val != 0); },
        "Draw unit state above selected units (combines with show_ids)"
    };
}

// ---------------------------------------------------------------------------
// Built-in commands (non-convar)
// ---------------------------------------------------------------------------
void DebugConsole::cmdShowAIInfo() {
    bool anyAI = false;
    for (int slot = 0; slot < Game::MAX_PLAYERS; ++slot) {
        PlayerController* ctrl = m_game.getController(slot);
        if (!ctrl || ctrl->isLocalHuman()) continue;
        auto* aiCtrl = dynamic_cast<AIPlayerController*>(ctrl);
        if (!aiCtrl) continue;
        anyAI = true;
        log("  Player " + std::to_string(slot + 1) + ": " + aiCtrl->getCurrentScriptName());
    }
    if (!anyAI)
        log("  No AI players found.");
}

// ---------------------------------------------------------------------------
// Event handling
// ---------------------------------------------------------------------------
bool DebugConsole::handleEvent(const sf::Event& event) {
    // --- Key pressed ---
    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {

        // ~ (Grave) toggles the console regardless of state
        if (kp->code == sf::Keyboard::Key::Grave) {
            m_active = !m_active;
            if (m_active) {
                m_inputText.clear();
                m_historyIndex = -1;
                m_scrollOffset = 0;
            }
            return true;
        }

        if (!m_active) return false;

        // --- Console is open ---

        if (kp->code == sf::Keyboard::Key::Escape) {
            m_active = false;
            m_inputText.clear();
            m_historyIndex = -1;
            return true;
        }

        if (kp->code == sf::Keyboard::Key::Enter) {
            std::string cmd = trim(m_inputText);
            if (!cmd.empty()) {
                log("> " + cmd);
                pushHistory(cmd);
                executeCommand(cmd);
            }
            m_inputText.clear();
            m_historyIndex = -1;
            m_scrollOffset = 0;    // jump to newest after executing
            return true;
        }

        if (kp->code == sf::Keyboard::Key::Backspace) {
            if (!m_inputText.empty())
                m_inputText.pop_back();
            return true;
        }

        // History navigation
        if (kp->code == sf::Keyboard::Key::Up) {
            if (!m_history.empty()) {
                if (m_historyIndex == -1)
                    m_historyIndex = static_cast<int>(m_history.size()) - 1;
                else if (m_historyIndex > 0)
                    --m_historyIndex;
                m_inputText = m_history[static_cast<size_t>(m_historyIndex)];
            }
            return true;
        }
        if (kp->code == sf::Keyboard::Key::Down) {
            if (m_historyIndex != -1) {
                ++m_historyIndex;
                if (m_historyIndex >= static_cast<int>(m_history.size())) {
                    m_historyIndex = -1;
                    m_inputText.clear();
                } else {
                    m_inputText = m_history[static_cast<size_t>(m_historyIndex)];
                }
            }
            return true;
        }

        // Log scrolling
        if (kp->code == sf::Keyboard::Key::PageUp) {
            m_scrollOffset += 3;
            return true;
        }
        if (kp->code == sf::Keyboard::Key::PageDown) {
            m_scrollOffset = std::max(0, m_scrollOffset - 3);
            return true;
        }

        // Consume all other key presses (don't let hotkeys fire while typing)
        return true;
    }

    // --- Text entered (printable characters) ---
    if (const auto* te = event.getIf<sf::Event::TextEntered>()) {
        if (!m_active) return false;

        uint32_t ch = te->unicode;
        // Skip control chars, backspace, enter, and the ~ that opened the console
        if (ch >= 32 && ch < 127 && ch != '`' && ch != '~') {
            m_inputText += static_cast<char>(ch);
        }
        return true;
    }

    // --- Mouse wheel scroll (log) ---
    if (const auto* mw = event.getIf<sf::Event::MouseWheelScrolled>()) {
        if (!m_active) return false;
        m_scrollOffset -= static_cast<int>(mw->delta);
        if (m_scrollOffset < 0) m_scrollOffset = 0;
        return true;
    }

    // Swallow mouse button / move events while the console is open so they
    // don't trigger unit commands.
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
    std::istringstream ss(cmd);
    std::string name;
    ss >> name;
    name = toLower(name);

    // Check convar registry first
    auto it = m_convars.find(name);
    if (it != m_convars.end()) {
        ConVar& cv = it->second;
        std::string valueStr;
        if (ss >> valueStr) {
            // Set value
            try {
                int val = std::stoi(valueStr);
                cv.setter(val);
                log(name + " = " + std::to_string(cv.getter()));
            } catch (...) {
                log("Error: value must be an integer (e.g. 0 or 1)");
            }
        } else {
            // Query value
            log(name + " = " + std::to_string(cv.getter()));
        }
        return;
    }

    // Built-in: show_ai_info
    if (name == "show_ai_info") {
        log("AI script assignments:");
        cmdShowAIInfo();
        return;
    }

    // Built-in: help
    if (name == "help") {
        log("Registered convars:");
        for (auto& [cname, cv] : m_convars) {
            log("  " + cname + "  -  " + cv.description);
        }
        log("Built-in commands:");
        log("  show_ai_info  -  Print the AI script in use by each AI player");
        log("  help          -  Show this list");
        log("Note: show_ids and show_state combine into one label above each unit");
        return;
    }

    log("Unknown command: \"" + name + "\"  (type 'help' for a list)");
}

// ---------------------------------------------------------------------------
// Log helpers
// ---------------------------------------------------------------------------
void DebugConsole::log(const std::string& msg) {
    m_log.push_back(msg);
    if (static_cast<int>(m_log.size()) > MAX_LOG_LINES)
        m_log.erase(m_log.begin());
}

void DebugConsole::pushHistory(const std::string& cmd) {
    // Don't push duplicates of the last entry
    if (!m_history.empty() && m_history.back() == cmd) return;
    m_history.push_back(cmd);
    if (static_cast<int>(m_history.size()) > MAX_HISTORY)
        m_history.erase(m_history.begin());
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace {
    std::string unitStateString(UnitState state) {
        switch (state) {
            case UnitState::Idle:         return "IDLE";
            case UnitState::Moving:       return "MOVING";
            case UnitState::AttackMoving: return "ATKMOVE";
            case UnitState::Attacking:    return "ATTACK";
            case UnitState::Following:    return "FOLLOW";
            case UnitState::Gathering:    return "GATHER";
            case UnitState::Returning:    return "RETURN";
            case UnitState::Building:     return "BUILD";
        }
        return "?";
    }
}

// ---------------------------------------------------------------------------
// ID / state rendering  (world-space view active)
// Renders a single combined label per selected unit:
//   show_ids only  →  "#133"
//   show_state only →  "IDLE"
//   both active    →  "#133 IDLE"
// ---------------------------------------------------------------------------
void DebugConsole::renderIds(sf::RenderWindow& window) {
    if ((!m_showIds && !m_showState) || !m_font) return;

    for (const auto& entity : m_game.getAllEntities()) {
        if (!entity || !entity->isAlive()) continue;
        if (!entity->isSelected()) continue;

        // Build label text
        std::string text;
        if (m_showIds)
            text = "#" + std::to_string(entity->getId());

        if (m_showState) {
            if (const Unit* unit = entity->asUnit()) {
                if (!text.empty()) text += ' ';
                text += unitStateString(unit->getState());
            }
        }

        if (text.empty()) continue;

        constexpr unsigned CHAR_SIZE = 12;
        sf::Text label(*m_font, text, CHAR_SIZE);
        label.setFillColor(sf::Color(255, 255, 80));
        label.setOutlineColor(sf::Color(0, 0, 0, 200));
        label.setOutlineThickness(1.0f);

        sf::FloatRect bounds = label.getLocalBounds();
        label.setOrigin(sf::Vector2f(bounds.position.x + bounds.size.x / 2.0f,
                                     bounds.position.y + bounds.size.y));
        label.setPosition(sf::Vector2f(entity->getPosition().x,
                                       entity->getPosition().y - 24.0f));
        window.draw(label);
    }
}

// ---------------------------------------------------------------------------
// Waypoint rendering  (world-space view active)
// ---------------------------------------------------------------------------
void DebugConsole::renderWaypoints(sf::RenderWindow& window) {
    if (!m_showWaypoints) return;

    for (const auto& entity : m_game.getAllEntities()) {
        if (!entity || !entity->isAlive()) continue;
        if (!entity->isSelected()) continue;

        const Unit* unit = entity->asUnit();
        if (!unit) continue;

        const auto& path = unit->getPath();
        if (path.empty()) continue;

        size_t pathIdx = unit->getPathIndex();

        for (size_t i = pathIdx; i + 1 < path.size(); ++i) {
            sf::Color lineColor(255, 200, 0, 180);
            sf::Vertex line[] = {
                sf::Vertex{path[i],     lineColor},
                sf::Vertex{path[i + 1], lineColor}
            };
            window.draw(line, 2, sf::PrimitiveType::Lines);
        }

        for (size_t i = pathIdx; i < path.size(); ++i) {
            constexpr float DOT_RADIUS = 5.0f;
            sf::CircleShape dot(DOT_RADIUS);
            dot.setOrigin(sf::Vector2f(DOT_RADIUS, DOT_RADIUS));
            dot.setPosition(path[i]);
            if (i == pathIdx) {
                dot.setFillColor(sf::Color(0, 255, 100, 220));
                dot.setOutlineColor(sf::Color(0, 180, 60));
            } else {
                dot.setFillColor(sf::Color(255, 220, 0, 180));
                dot.setOutlineColor(sf::Color(200, 160, 0));
            }
            dot.setOutlineThickness(1.5f);
            window.draw(dot);
        }

        if (pathIdx < path.size()) {
            sf::Color leadColor(100, 200, 255, 160);
            sf::Vertex leadLine[] = {
                sf::Vertex{entity->getPosition(), leadColor},
                sf::Vertex{path[pathIdx],         leadColor}
            };
            window.draw(leadLine, 2, sf::PrimitiveType::Lines);
        }

        // When the path is exhausted the unit moves directly toward
        // m_targetPosition; draw a final segment + marker so the real
        // destination is always visible.
        sf::Vector2f dest = unit->getTargetPosition();
        sf::Vector2f unitPos = entity->getPosition();
        bool pathDone = (pathIdx >= path.size());
        bool destDiffersFromLast = path.empty() ||
            (MathUtil::distance(path.back(), dest) > 2.0f);
        if (pathDone && destDiffersFromLast) {
            sf::Color segColor(255, 100, 100, 180);
            sf::Vertex seg[] = {
                sf::Vertex{unitPos, segColor},
                sf::Vertex{dest,    segColor}
            };
            window.draw(seg, 2, sf::PrimitiveType::Lines);

            constexpr float DEST_RADIUS = 6.0f;
            sf::CircleShape destDot(DEST_RADIUS);
            destDot.setOrigin(sf::Vector2f(DEST_RADIUS, DEST_RADIUS));
            destDot.setPosition(dest);
            destDot.setFillColor(sf::Color(255, 80, 80, 200));
            destDot.setOutlineColor(sf::Color(180, 40, 40));
            destDot.setOutlineThickness(1.5f);
            window.draw(destDot);
        }
    }
}

// ---------------------------------------------------------------------------
// Console overlay rendering  (screen-space / UI view)
// ---------------------------------------------------------------------------
void DebugConsole::render(sf::RenderWindow& window) {
    if (!m_active) return;
    if (!m_font)   return;

    const sf::Vector2u winSize = window.getSize();
    const float winW = static_cast<float>(winSize.x);
    const float winH = static_cast<float>(winSize.y);

    // Console panel occupies the top half of the screen
    constexpr float PANEL_HEIGHT_RATIO = 0.5f;
    constexpr float LINE_HEIGHT  = 20.0f;
    constexpr float PADDING      = 8.0f;
    constexpr float INPUT_HEIGHT = 28.0f;
    constexpr unsigned FONT_SIZE = 15;

    const float panelH = winH * PANEL_HEIGHT_RATIO;
    const float inputY = panelH - INPUT_HEIGHT;

    // Background
    sf::RectangleShape bg(sf::Vector2f(winW, panelH));
    bg.setPosition(sf::Vector2f(0.f, 0.f));
    bg.setFillColor(sf::Color(8, 12, 20, 220));
    window.draw(bg);

    // Bottom border / separator above input
    sf::RectangleShape bottomBorder(sf::Vector2f(winW, 1.f));
    bottomBorder.setPosition(sf::Vector2f(0.f, panelH));
    bottomBorder.setFillColor(sf::Color(80, 160, 80, 200));
    window.draw(bottomBorder);

    // Input separator line
    sf::RectangleShape inputSep(sf::Vector2f(winW, 1.f));
    inputSep.setPosition(sf::Vector2f(0.f, inputY));
    inputSep.setFillColor(sf::Color(60, 100, 60, 180));
    window.draw(inputSep);

    // Input bar
    sf::RectangleShape inputBg(sf::Vector2f(winW, INPUT_HEIGHT));
    inputBg.setPosition(sf::Vector2f(0.f, inputY));
    inputBg.setFillColor(sf::Color(15, 25, 15, 230));
    window.draw(inputBg);

    {
        // Blinking cursor – show underscore based on time
        bool showCursor = (static_cast<int>(m_window.hasFocus() ? 1 : 0) == 1);
        std::string display = "] " + m_inputText + "_";
        sf::Text inputText(*m_font, display, FONT_SIZE);
        inputText.setFillColor(sf::Color(200, 255, 200));
        inputText.setPosition(sf::Vector2f(PADDING, inputY + (INPUT_HEIGHT - FONT_SIZE) * 0.3f));
        window.draw(inputText);
    }

    // Log area — how many lines fit above the input bar
    const float logAreaH = inputY - PADDING;
    const int maxVisible = std::max(1, static_cast<int>(logAreaH / LINE_HEIGHT));

    const int logSize = static_cast<int>(m_log.size());

    // Clamp scroll offset
    int maxScroll = std::max(0, logSize - maxVisible);
    m_scrollOffset = std::clamp(m_scrollOffset, 0, maxScroll);

    // Draw lines from the bottom of the log area upward (newest at bottom)
    // The visible window in the log is [startIdx, endIdx)
    int endIdx   = logSize - m_scrollOffset;           // exclusive
    int startIdx = std::max(0, endIdx - maxVisible);   // inclusive

    for (int i = startIdx; i < endIdx; ++i) {
        int row = i - startIdx;   // 0 = topmost visible
        float y = PADDING + static_cast<float>(row) * LINE_HEIGHT;

        sf::Color col(180, 200, 180);
        // Commands (lines starting with "> ") in a brighter tone
        if (m_log[i].size() >= 2 && m_log[i][0] == '>' && m_log[i][1] == ' ')
            col = sf::Color(255, 255, 120);

        sf::Text line(*m_font, m_log[i], FONT_SIZE);
        line.setFillColor(col);
        line.setPosition(sf::Vector2f(PADDING, y));
        window.draw(line);
    }

    // Scroll indicator (if not at bottom)
    if (m_scrollOffset > 0) {
        sf::Text scrollHint(*m_font,
            "-- scroll: " + std::to_string(m_scrollOffset) + " lines up (PgDn to descend) --",
            FONT_SIZE - 2);
        scrollHint.setFillColor(sf::Color(150, 150, 100, 200));
        // Place just above the input separator
        scrollHint.setPosition(sf::Vector2f(PADDING, inputY - LINE_HEIGHT));
        window.draw(scrollHint);
    }
}

