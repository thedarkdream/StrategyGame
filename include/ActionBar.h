#pragma once

#include "Types.h"
#include "EntityData.h"
#include "InputHandler.h"
#include <SFML/Graphics.hpp>
#include <vector>
#include <unordered_map>
#include <string>

class Player;
class PlayerActions;

// Result from clicking on the action bar
struct ActionBarClickResult {
    enum class Type {
        None,           // No action (clicked empty area)
        TargetMove,     // Enter move targeting mode
        TargetAttack,   // Enter attack targeting mode
        TargetGather,   // Enter gather targeting mode
        TargetBuild,    // Enter build placement mode
        Handled,        // Action was executed (Stop, Train, Cancel queue)
        CancelBuilding  // Cancel building under construction
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
    void setTargetingAction(TargetingAction action) { m_targetingAction = action; }
    
    // Rendering
    void render(sf::RenderWindow& window, Player& player);
    
    // Hit detection
    bool isPositionOnPanel(sf::Vector2i screenPos, Player& player) const;
    int getButtonAtPosition(sf::Vector2i screenPos, EntityPtr entity) const;
    int getQueueItemAtPosition(sf::Vector2i screenPos, EntityPtr entity) const;
    
    // Click handling - executes instant actions, returns type for targeting actions
    ActionBarClickResult handleClick(sf::Vector2i screenPos, Player& player, PlayerActions& actions);
    
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
    void renderConstructionUI(sf::RenderWindow& window, Building* building);
    void renderTooltip(sf::RenderWindow& window, EntityPtr entity);
    std::string buildTooltipText(const ActionDef& action) const;

    // Texture helpers
    void ensureTexturesLoaded();
    // Returns pointer to normal+active textures for a label, or {nullptr,nullptr}
    std::pair<const sf::Texture*, const sf::Texture*> getActionTextures(const std::string& label) const;
    
    // Constants for queue rendering
    static constexpr float MAIN_ICON_SIZE   = 32.0f;
    static constexpr float QUEUED_ICON_SIZE = 20.0f;
    static constexpr float ICON_SPACING     =  4.0f;
    // Source images are 64x64; buttons display at ACTION_BAR_BUTTON_SIZE (32)
    static constexpr float ACTION_TEX_SRC_SIZE = 64.0f;
    
    const sf::Font* m_font = nullptr;
    sf::Vector2u m_windowSize{1280, 720};
    TargetingAction m_targetingAction = TargetingAction::None;

    // Texture cache: key = filename stem (e.g. "move"), value = {normal, active}
    struct ActionTexturePair {
        sf::Texture normal;
        sf::Texture active;
    };
    std::unordered_map<std::string, ActionTexturePair> m_textures;
    bool m_texturesLoaded = false;
};
