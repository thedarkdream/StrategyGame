#pragma once

#include "Types.h"
#include <SFML/Graphics.hpp>
#include <functional>
#include <memory>
#include <string>

class Game;
class Entity;

// Action type for targeting mode
enum class TargetingAction {
    None,
    Move,
    Attack,
    Gather,
    RallyPoint
};

class InputHandler {
public:
    InputHandler(sf::RenderWindow& window, Game& game);
    
    void handleEvent(const sf::Event& event);
    void update(float deltaTime);
    
    // Camera
    sf::View& getCamera() { return m_camera; }
    const sf::View& getCamera() const { return m_camera; }
    sf::Vector2f screenToWorld(sf::Vector2i screenPos) const;
    void onWindowResize(sf::Vector2u newSize);
    
    // Build mode
    void enterBuildMode(EntityType buildingType);
    void exitBuildMode();
    bool isInBuildMode() const { return m_buildMode; }
    EntityType getBuildingType() const { return m_buildingToBuild; }
    sf::Vector2f getBuildPreviewPosition() const { return m_buildPreviewPos; }
    
    // Enemy inspection
    EntityPtr getInspectedEnemy() const { return m_inspectedEnemy.lock(); }
    void clearInspectedEnemy() { m_inspectedEnemy.reset(); }
    
    // Targeting mode (for action bar commands)
    void enterTargetingMode(TargetingAction action);
    void exitTargetingMode();
    bool isInTargetingMode() const { return m_targetingMode; }
    TargetingAction getTargetingAction() const { return m_targetingAction; }
    
    // Action bar click handling
    bool handleActionBarClick(sf::Vector2i screenPos);
    
    // Selection box
    bool isSelecting() const { return m_isSelecting; }
    sf::FloatRect getSelectionBox() const;

    // Camera control
    void centerCameraAt(sf::Vector2f worldPos);
    
private:
    sf::RenderWindow& m_window;
    Game& m_game;
    sf::View m_camera;
    
    // Selection box
    bool m_isSelecting = false;
    sf::Vector2f m_selectionStart;
    sf::Vector2f m_selectionEnd;
    
    // Minimap dragging
    bool m_isDraggingMinimap = false;
    
    // Middle-click map dragging
    bool m_isDraggingMap = false;
    sf::Vector2i m_dragStartScreenPos;
    sf::Vector2f m_dragStartCameraPos;
    
    // Build mode
    bool m_buildMode = false;
    EntityType m_buildingToBuild = EntityType::None;
    sf::Vector2f m_buildPreviewPos;
    
    // Targeting mode (for action bar commands)
    bool m_targetingMode = false;
    TargetingAction m_targetingAction = TargetingAction::None;
    
    // Enemy inspection (view stats without selecting)
    std::weak_ptr<Entity> m_inspectedEnemy;
    
    // Double-click detection
    sf::Clock m_lastClickClock;
    sf::Vector2f m_lastClickWorldPos;
    static constexpr float DOUBLE_CLICK_TIME = 0.3f;  // 300ms threshold
    static constexpr float DOUBLE_CLICK_DISTANCE = 20.0f;  // Max pixel distance between clicks
    
    // Camera edge scrolling
    void updateCameraEdgeScroll(float deltaTime);
    void updateCameraKeyScroll(float deltaTime);
    void clampCamera();
    
    // Minimap
    bool isPositionOnMinimap(sf::Vector2i screenPos) const;
    sf::Vector2f minimapToWorld(sf::Vector2i screenPos) const;
    
    // Input processing
    void handleMousePress(sf::Vector2i position, sf::Mouse::Button button);
    void handleMouseRelease(sf::Vector2i position, sf::Mouse::Button button);
    void handleMouseMove(sf::Vector2i position);
    void handleKeyPress(sf::Keyboard::Key code);
    std::string keyToHotkey(sf::Keyboard::Key code);
    
    // Selection
    void performSelection(sf::Vector2f worldPos);
    void performBoxSelection();
    void selectAllOfTypeOnScreen(EntityType type);
    
    // Targeting action execution
    void executeTargetingAction(sf::Vector2f worldPos, EntityPtr target, bool shift);
};
