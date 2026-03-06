#pragma once

#include "Types.h"
#include "EntityData.h"
#include <SFML/Graphics.hpp>
#include <vector>

class Player;

// Result from clicking on the action bar
struct ActionBarClickResult {
    enum class Type {
        None,           // No action (clicked empty area)
        TargetMove,     // Enter move targeting mode
        TargetAttack,   // Enter attack targeting mode
        TargetGather,   // Enter gather targeting mode
        TargetBuild,    // Enter build placement mode
        Handled         // Action was executed (Stop, Train, Cancel queue)
    };
    
    Type type = Type::None;
    EntityType buildType = EntityType::None;  // For TargetBuild
};

class ActionBar {
public:
    ActionBar();
    
    // Set rendering context
    void setFont(const sf::Font* font) { m_font = font; }
    void setWindowSize(sf::Vector2u size) { m_windowSize = size; }
    
    // Rendering
    void render(sf::RenderWindow& window, Player& player);
    
    // Hit detection
    bool isPositionOnPanel(sf::Vector2i screenPos, Player& player) const;
    int getButtonAtPosition(sf::Vector2i screenPos, EntityPtr entity) const;
    int getQueueItemAtPosition(sf::Vector2i screenPos, EntityPtr entity) const;
    
    // Click handling - executes instant actions, returns type for targeting actions
    ActionBarClickResult handleClick(sf::Vector2i screenPos, Player& player);
    
private:
    // Layout helpers
    float getPanelX() const;
    float getPanelY() const;
    float getRow0Y() const;
    float getRow1Y() const;
    float getQueueY() const;
    
    // Rendering helpers
    void renderButtons(sf::RenderWindow& window, EntityPtr entity, Player& player);
    void renderProductionQueue(sf::RenderWindow& window, Building* building);
    
    // Constants for queue rendering
    static constexpr float MAIN_ICON_SIZE = 32.0f;
    static constexpr float QUEUED_ICON_SIZE = 20.0f;
    static constexpr float ICON_SPACING = 4.0f;
    
    const sf::Font* m_font = nullptr;
    sf::Vector2u m_windowSize{1280, 720};  // Default to BASE dimensions
};
