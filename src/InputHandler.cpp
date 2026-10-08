#include "InputHandler.h"
#include "Game.h"
#include "Constants.h"
#include "Minimap.h"
#include "Entity.h"
#include "EntityData.h"
#include "Unit.h"
#include "Building.h"
#include "ActionBar.h"
#include "EffectsManager.h"
#include "ResourceManager.h"
#include <cmath>
#include <algorithm>

InputHandler::InputHandler(sf::RenderWindow& window, Game& game)
    : m_window(window)
    , m_game(game)
{
    // Initialize camera with reference game dimensions
    // This defines the visible area in game units (always the same regardless of window size)
    m_camera.setSize(sf::Vector2f(Constants::BASE_WIDTH, Constants::BASE_HEIGHT));
    m_camera.setCenter(sf::Vector2f(Constants::BASE_WIDTH / 2.0f, Constants::BASE_HEIGHT / 2.0f));
}

void InputHandler::handleEvent(const sf::Event& event) {
    if (const auto* mousePressed = event.getIf<sf::Event::MouseButtonPressed>()) {
        handleMousePress(mousePressed->position, mousePressed->button);
    }
    else if (const auto* mouseReleased = event.getIf<sf::Event::MouseButtonReleased>()) {
        handleMouseRelease(mouseReleased->position, mouseReleased->button);
    }
    else if (const auto* mouseMoved = event.getIf<sf::Event::MouseMoved>()) {
        handleMouseMove(mouseMoved->position);
    }
    else if (const auto* keyPressed = event.getIf<sf::Event::KeyPressed>()) {
        handleKeyPress(keyPressed->code);
    }
}

void InputHandler::update(float deltaTime) {
    updateCameraEdgeScroll(deltaTime);
    updateCameraKeyScroll(deltaTime);
    clampCamera();
    
    // Clean up inspected enemy if it died
    if (auto enemy = m_inspectedEnemy.lock()) {
        if (!enemy->isAlive()) {
            enemy->setSelected(false);
            m_inspectedEnemy.reset();
        }
    }
}

sf::Vector2f InputHandler::screenToWorld(sf::Vector2i screenPos) const {
    return m_game.getRenderer().screenToWorld(m_camera, screenPos, m_game.getMap());
}

void InputHandler::onWindowResize(sf::Vector2u newSize) {
    // Maintain the same vertical game unit coverage regardless of window size
    // Height stays at BASE_HEIGHT game units, width adjusts for aspect ratio
    float aspectRatio = static_cast<float>(newSize.x) / static_cast<float>(newSize.y);
    float viewWidth = Constants::BASE_HEIGHT * aspectRatio;
    
    sf::Vector2f oldCenter = m_camera.getCenter();
    m_camera.setSize(sf::Vector2f(viewWidth, Constants::BASE_HEIGHT));
    m_camera.setCenter(oldCenter);  // Preserve camera position
    clampCamera();  // Ensure camera stays in bounds
}

void InputHandler::enterBuildMode(EntityType buildingType) {
    m_buildMode = true;
    m_buildingToBuild = buildingType;
}

void InputHandler::exitBuildMode() {
    m_buildMode = false;
    m_buildingToBuild = EntityType::None;
}

void InputHandler::updateCameraEdgeScroll(float deltaTime) {
    sf::Vector2i mousePos = sf::Mouse::getPosition(m_window);
    sf::Vector2u windowSize = m_window.getSize();
    sf::Vector2f movement(0.0f, 0.0f);
    
    if (mousePos.x < Constants::CAMERA_EDGE_MARGIN) {
        movement.x = -Constants::CAMERA_SPEED;
    } else if (mousePos.x > static_cast<int>(windowSize.x) - Constants::CAMERA_EDGE_MARGIN) {
        movement.x = Constants::CAMERA_SPEED;
    }
    
    if (mousePos.y < Constants::CAMERA_EDGE_MARGIN) {
        movement.y = -Constants::CAMERA_SPEED;
    } else if (mousePos.y > static_cast<int>(windowSize.y) - Constants::CAMERA_EDGE_MARGIN) {
        movement.y = Constants::CAMERA_SPEED;
    }
    
    m_camera.move(movement * deltaTime);
}

void InputHandler::updateCameraKeyScroll(float deltaTime) {
    sf::Vector2f movement(0.0f, 0.0f);
    
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left)) {
        movement.x = -Constants::CAMERA_SPEED;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right)) {
        movement.x = Constants::CAMERA_SPEED;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up)) {
        movement.y = -Constants::CAMERA_SPEED;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down)) {
        movement.y = Constants::CAMERA_SPEED;
    }
    
    m_camera.move(movement * deltaTime);
}

void InputHandler::clampCamera() {
    const sf::Vector2u winSize = m_window.getSize();
    if (winSize.x == 0 || winSize.y == 0) return;

    sf::Vector2f center = m_camera.getCenter();
    
    const Map& map = m_game.getMap();
    float mapWidth = static_cast<float>(map.getWidth() * Constants::TILE_SIZE);
    float mapHeight = static_cast<float>(map.getHeight() * Constants::TILE_SIZE);

    // Ground distance from the camera centre to the edges of the screen, asked of
    // the active renderer so the clamp follows its projection.  In 2D this is
    // simply half the view size; in 3D the bottom of the screen is closer to the
    // camera and covers less ground than the top, so the margins differ per side.
    const IRenderer& renderer = m_game.getRenderer();
    const int w = static_cast<int>(winSize.x);
    const int h = static_cast<int>(winSize.y);
    const sf::Vector2f bottomLeft  = renderer.screenToWorld(m_camera, { 0, h },     map);
    const sf::Vector2f bottomRight = renderer.screenToWorld(m_camera, { w, h },     map);
    const sf::Vector2f topCenter   = renderer.screenToWorld(m_camera, { w / 2, 0 }, map);
    const float marginLeft   = std::max(0.0f, center.x - bottomLeft.x);
    const float marginRight  = std::max(0.0f, bottomRight.x - center.x);
    const float marginTop    = std::max(0.0f, center.y - topCenter.y);
    const float marginBottom = std::max(0.0f, bottomLeft.y - center.y);
    
    // If the view is larger than the map, centre on the map instead of clamping
    if (marginLeft + marginRight >= mapWidth)
        center.x = (mapWidth + marginLeft - marginRight) / 2.0f;
    else
        center.x = std::max(marginLeft, std::min(center.x, mapWidth - marginRight));

    if (marginTop + marginBottom >= mapHeight)
        center.y = (mapHeight + marginTop - marginBottom) / 2.0f;
    else
        center.y = std::max(marginTop, std::min(center.y, mapHeight - marginBottom));
    
    m_camera.setCenter(center);
}

bool InputHandler::isPositionOnMinimap(sf::Vector2i screenPos) const {
    return Minimap::isHit(screenPos, m_window.getSize());
}

sf::Vector2f InputHandler::minimapToWorld(sf::Vector2i screenPos) const {
    return Minimap::toWorldPos(screenPos, m_window.getSize(), m_game.getMap());
}

void InputHandler::centerCameraAt(sf::Vector2f worldPos) {
    m_camera.setCenter(worldPos);
    clampCamera();
}

void InputHandler::handleMousePress(sf::Vector2i position, sf::Mouse::Button button) {
    const bool shift = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
                       sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);

    // Check if clicking on minimap
    bool onMinimap = isPositionOnMinimap(position);
    
    if (onMinimap) {
        sf::Vector2f worldPos = minimapToWorld(position);
        
        if (button == sf::Mouse::Button::Left) {
            if (m_targetingMode) {
                // Execute targeting action on minimap position
                executeTargetingAction(worldPos, nullptr, shift);
                exitTargetingMode();
                return;
            }
            // Normal left-click on minimap - pan camera
            m_isDraggingMinimap = true;
            centerCameraAt(worldPos);
            return;
        } else if (button == sf::Mouse::Button::Right) {
            if (m_targetingMode) {
                // Cancel targeting mode
                exitTargetingMode();
                return;
            }
            // Smart right-click on minimap
            m_game.getActions().issueSmartRightClick(worldPos, nullptr, shift);
            return;
        }
    }
    
    // Check if clicking on action bar (left-click only)
    if (button == sf::Mouse::Button::Left && handleActionBarClick(position)) {
        return;  // Action bar handled the click
    }
    
    sf::Vector2f worldPos = screenToWorld(position);
    
    if (button == sf::Mouse::Button::Left) {
        if (m_targetingMode) {
            // Execute the targeted action
            EntityPtr target = m_game.getRenderer().pickEntity(m_camera, position, m_game);
            executeTargetingAction(worldPos, target, shift);
            exitTargetingMode();
        } else if (m_buildMode) {
            // Place building
            m_game.issueBuildCommand(m_buildingToBuild, worldPos, shift);
            exitBuildMode();
        } else {
            // Start selection box
            m_isSelecting = true;
            m_selectionStart = worldPos;
            m_selectionEnd = worldPos;
            m_selectionStartPx = position;
            m_selectionEndPx = position;
        }
    } else if (button == sf::Mouse::Button::Right) {
        if (m_targetingMode) {
            // Cancel targeting mode
            exitTargetingMode();
        } else if (m_buildMode) {
            exitBuildMode();
        } else {
            // Smart right-click: delegate command dispatch to PlayerActions
            EntityPtr target = m_game.getRenderer().pickEntity(m_camera, position, m_game);
            m_game.getActions().issueSmartRightClick(worldPos, target, shift);
        }
    } else if (button == sf::Mouse::Button::Middle) {
        // Start map drag scrolling: the ground point under the cursor is pinned to it
        m_isDraggingMap = true;
        m_dragAnchorWorld = worldPos;
    }
}

void InputHandler::handleMouseRelease(sf::Vector2i position, sf::Mouse::Button button) {
    if (button == sf::Mouse::Button::Middle) {
        m_isDraggingMap = false;
        return;
    }
    
    if (button == sf::Mouse::Button::Left) {
        // Stop minimap dragging
        if (m_isDraggingMinimap) {
            m_isDraggingMinimap = false;
            return;
        }
        
        if (m_isSelecting) {
            m_isSelecting = false;
            
            // Check if it was a click or drag (measured in screen pixels)
            sf::FloatRect selectionBox = getSelectionBoxScreen();
            if (selectionBox.size.x < 5.0f && selectionBox.size.y < 5.0f) {
                // Check for double-click
                float timeSinceLastClick = m_lastClickClock.getElapsedTime().asSeconds();
                float distanceFromLastClick = std::hypot(
                    m_selectionStart.x - m_lastClickWorldPos.x,
                    m_selectionStart.y - m_lastClickWorldPos.y
                );
                
                bool isDoubleClick = (timeSinceLastClick < DOUBLE_CLICK_TIME && 
                                      distanceFromLastClick < DOUBLE_CLICK_DISTANCE);
                
                if (isDoubleClick) {
                    // Double-click: select all units of same type on screen
                    EntityPtr entity = m_game.getRenderer().pickEntity(m_camera, m_selectionStartPx, m_game);
                    if (entity && entity->getTeam() == m_game.getPlayer().getTeam()) {
                        if (entity->asUnit()) {
                            selectAllOfTypeOnScreen(entity->getType());
                        }
                    }
                } else {
                    // Single click selection
                    performSelection(m_selectionStartPx);
                }
                
                // Update last click tracking
                m_lastClickClock.restart();
                m_lastClickWorldPos = m_selectionStart;
            } else {
                // Box selection
                performBoxSelection();
            }
        }
    }
}

void InputHandler::handleMouseMove(sf::Vector2i position) {
    // Handle middle-click map dragging
    if (m_isDraggingMap) {
        // Shift the camera so the pinned ground point is back under the cursor.
        // Exact for the top-down view and for the perspective view alike.
        sf::Vector2f under = screenToWorld(position);
        m_camera.move(m_dragAnchorWorld - under);
        clampCamera();
        return;
    }
    
    // Handle minimap dragging
    if (m_isDraggingMinimap) {
        if (isPositionOnMinimap(position)) {
            sf::Vector2f worldPos = minimapToWorld(position);
            centerCameraAt(worldPos);
        }
        return;
    }
    
    sf::Vector2f worldPos = screenToWorld(position);
    
    if (m_isSelecting) {
        m_selectionEnd = worldPos;
        m_selectionEndPx = position;
    }
    
    if (m_buildMode) {
        // Snap to grid and calculate center position for preview
        sf::Vector2f pixelSize = ENTITY_DATA.getSize(m_buildingToBuild);
        int tileX = static_cast<int>(worldPos.x / Constants::TILE_SIZE);
        int tileY = static_cast<int>(worldPos.y / Constants::TILE_SIZE);
        // Center position: top-left corner + half the pixel size
        m_buildPreviewPos = sf::Vector2f(
            tileX * Constants::TILE_SIZE + pixelSize.x / 2.0f,
            tileY * Constants::TILE_SIZE + pixelSize.y / 2.0f
        );
    }
}

void InputHandler::handleKeyPress(sf::Keyboard::Key code) {
    Player& player = m_game.getPlayer();
    
    // Handle escape separately - it's a global key
    if (code == sf::Keyboard::Key::Escape) {
        if (m_targetingMode) {
            exitTargetingMode();
        } else if (m_buildMode) {
            exitBuildMode();
        } else {
            // Check if selected building is under construction - cancel it
            for (auto& entity : player.getSelection()) {
                if (auto* building = entity->asBuilding()) {
                    if (!building->isConstructed()) {
                        m_game.cancelBuildingConstruction(entity);
                        return;
                    }
                }
            }
            
            // Check if selected building is producing - cancel production
            bool cancelledProduction = false;
            for (auto& entity : player.getSelection()) {
                if (auto* building = entity->asBuilding()) {
                    if (building->isProducing()) {
                        building->cancelProduction();
                        cancelledProduction = true;
                        break;
                    }
                }
            }
            if (!cancelledProduction) {
                player.clearSelection();
            }
        }
        return;
    }
    
    // Convert key code to hotkey string
    std::string hotkey = keyToHotkey(code);
    if (hotkey.empty()) return;
    
    // Get first selected entity
    EntityPtr selectedEntity = player.getFirstOwnedSelectedEntity();
    if (!selectedEntity) return;
    
    // Don't process action hotkeys if building is under construction
    if (auto* building = selectedEntity->asBuilding()) {
        if (!building->isConstructed()) {
            return;
        }
    }
    
    // Look up actions for this entity type
    const auto& actions = ENTITY_DATA.getActions(selectedEntity->getType());
    
    // Find action matching the hotkey
    for (const auto& action : actions) {
        if (action.hotkey != hotkey) continue;
        
        // Execute action based on type
        switch (action.type) {
            case ActionDef::Type::TargetMove:
                enterTargetingMode(TargetingAction::Move);
                return;
                
            case ActionDef::Type::TargetAttack:
                enterTargetingMode(TargetingAction::Attack);
                return;
                
            case ActionDef::Type::TargetGather:
                enterTargetingMode(TargetingAction::Gather);
                return;
                
            case ActionDef::Type::Instant:
                m_game.getActions().stop(player.getSelection());
                return;
                
            case ActionDef::Type::Build:
                // Check dependencies
                if (action.requires != EntityType::None &&
                    !player.hasCompletedBuilding(action.requires)) {
                    return;
                }
                // Check affordability
                {
                    int mineralCost = ENTITY_DATA.getMineralCost(action.producesType);
                    int gasCost = ENTITY_DATA.getGasCost(action.producesType);
                    if (player.canAfford(mineralCost, gasCost)) {
                        enterBuildMode(action.producesType);
                    }
                }
                return;
                
            case ActionDef::Type::Train:
                if (selectedEntity->asBuilding()) {
                    m_game.getActions().trainUnit(
                        std::static_pointer_cast<Building>(selectedEntity),
                        action.producesType);
                }
                return;
        }
    }
}

std::string InputHandler::keyToHotkey(sf::Keyboard::Key code) {
    switch (code) {
        case sf::Keyboard::Key::A: return "A";
        case sf::Keyboard::Key::B: return "B";
        case sf::Keyboard::Key::C: return "C";
        case sf::Keyboard::Key::D: return "D";
        case sf::Keyboard::Key::E: return "E";
        case sf::Keyboard::Key::F: return "F";
        case sf::Keyboard::Key::G: return "G";
        case sf::Keyboard::Key::H: return "H";
        case sf::Keyboard::Key::I: return "I";
        case sf::Keyboard::Key::J: return "J";
        case sf::Keyboard::Key::K: return "K";
        case sf::Keyboard::Key::L: return "L";
        case sf::Keyboard::Key::M: return "M";
        case sf::Keyboard::Key::N: return "N";
        case sf::Keyboard::Key::O: return "O";
        case sf::Keyboard::Key::P: return "P";
        case sf::Keyboard::Key::Q: return "Q";
        case sf::Keyboard::Key::R: return "R";
        case sf::Keyboard::Key::S: return "S";
        case sf::Keyboard::Key::T: return "T";
        case sf::Keyboard::Key::U: return "U";
        case sf::Keyboard::Key::V: return "V";
        case sf::Keyboard::Key::W: return "W";
        case sf::Keyboard::Key::X: return "X";
        case sf::Keyboard::Key::Y: return "Y";
        case sf::Keyboard::Key::Z: return "Z";
        default: return "";
    }
}

void InputHandler::performSelection(sf::Vector2i pixel) {
    EntityPtr entity = m_game.getRenderer().pickEntity(m_camera, pixel, m_game);
    
    Player& player = m_game.getPlayer();
    
    // Helper to clear inspected enemy
    auto clearInspected = [this]() {
        if (auto prev = m_inspectedEnemy.lock()) {
            prev->setSelected(false);
        }
        m_inspectedEnemy.reset();
    };
    
    if (entity) {
        if (entity->getTeam() == player.getTeam()) {
            // Select own unit/building
            clearInspected();
            player.selectEntity(entity);
        } else {
            // Inspect enemy or neutral entity (view stats only)
            player.clearSelection();
            clearInspected();
            m_inspectedEnemy = entity;
            entity->setSelected(true);
        }
    } else {
        // Clicked empty space
        player.clearSelection();
        clearInspected();
    }
}

void InputHandler::performBoxSelection() {
    std::vector<EntityPtr> selected = m_game.getRenderer().pickEntitiesInRect(
        m_camera, m_selectionStartPx, m_selectionEndPx, m_game.getPlayer().getTeam(), m_game);
    
    // If we have both units and buildings, prefer units only
    bool hasUnits = false;
    for (const auto& entity : selected) {
        if (entity->asUnit()) {
            hasUnits = true;
            break;
        }
    }
    
    if (hasUnits) {
        // Filter out buildings
        selected.erase(
            std::remove_if(selected.begin(), selected.end(),
                [](const EntityPtr& e) { return e->asBuilding() != nullptr; }),
            selected.end());
    }
    
    // Box selection only selects own units, clears any inspected enemy
    if (auto prev = m_inspectedEnemy.lock()) {
        prev->setSelected(false);
    }
    m_inspectedEnemy.reset();
    m_game.getPlayer().selectEntities(selected);
}

void InputHandler::selectAllOfTypeOnScreen(EntityType type) {
    // Everything currently on screen: the whole window as a rubber-band
    const sf::Vector2u winSize = m_window.getSize();
    const sf::Vector2i lastPixel(static_cast<int>(winSize.x) - 1, static_cast<int>(winSize.y) - 1);
    
    // Get all player entities in the visible area
    Player& player = m_game.getPlayer();
    std::vector<EntityPtr> entitiesOnScreen = m_game.getRenderer().pickEntitiesInRect(
        m_camera, sf::Vector2i(0, 0), lastPixel, player.getTeam(), m_game);
    
    // Filter to only units of the specified type
    std::vector<EntityPtr> unitsOfType;
    for (const auto& entity : entitiesOnScreen) {
        if (entity->getType() == type && entity->asUnit()) {
            unitsOfType.push_back(entity);
        }
    }
    
    // Clear any inspected enemy and select all matching units
    if (auto prev = m_inspectedEnemy.lock()) {
        prev->setSelected(false);
    }
    m_inspectedEnemy.reset();
    
    if (!unitsOfType.empty()) {
        player.selectEntities(unitsOfType);
    }
}

sf::FloatRect InputHandler::getSelectionBox() const {
    float left = std::min(m_selectionStart.x, m_selectionEnd.x);
    float top = std::min(m_selectionStart.y, m_selectionEnd.y);
    float width = std::abs(m_selectionEnd.x - m_selectionStart.x);
    float height = std::abs(m_selectionEnd.y - m_selectionStart.y);
    
    return sf::FloatRect(sf::Vector2f(left, top), sf::Vector2f(width, height));
}

sf::FloatRect InputHandler::getSelectionBoxScreen() const {
    const int left   = std::min(m_selectionStartPx.x, m_selectionEndPx.x);
    const int top    = std::min(m_selectionStartPx.y, m_selectionEndPx.y);
    const int width  = std::abs(m_selectionEndPx.x - m_selectionStartPx.x);
    const int height = std::abs(m_selectionEndPx.y - m_selectionStartPx.y);

    return sf::FloatRect(sf::Vector2f(static_cast<float>(left), static_cast<float>(top)),
                         sf::Vector2f(static_cast<float>(width), static_cast<float>(height)));
}

void InputHandler::enterTargetingMode(TargetingAction action) {
    m_targetingMode = true;
    m_targetingAction = action;
}

void InputHandler::exitTargetingMode() {
    m_targetingMode = false;
    m_targetingAction = TargetingAction::None;
}

void InputHandler::executeTargetingAction(sf::Vector2f worldPos, EntityPtr target, bool shift) {
    Player& player = m_game.getPlayer();
    
    switch (m_targetingAction) {
        case TargetingAction::Move:
            // Check if clicking on an ally unit - issue follow command
            if (target && target->getTeam() == player.getTeam()) {
                if (target->asUnit()) {
                    m_game.getActions().follow(m_game.getPlayer().getSelection(), target, shift);
                } else {
                    // Clicked on own building - just move to location
                    EFFECTS.spawnMoveEffect(worldPos, 1.0f);
                    m_game.getActions().move(m_game.getPlayer().getSelection(), worldPos, shift);
                }
            } else {
                EFFECTS.spawnMoveEffect(worldPos, 1.0f);
                m_game.getActions().move(m_game.getPlayer().getSelection(), worldPos, shift);
            }
            break;
        case TargetingAction::Attack:
            if (target) {
                // Attack any target (including allies) when using Attack action
                m_game.getActions().attack(m_game.getPlayer().getSelection(), target, shift);
            } else {
                // Attack-move to location (move while attacking enemies on the way)
                EFFECTS.spawnMoveEffect(worldPos, 1.0f);
                m_game.getActions().attackMove(m_game.getPlayer().getSelection(), worldPos, shift);
            }
            break;
        case TargetingAction::Gather:
            if (target) {
                auto* def = ENTITY_DATA.get(target->getType());
                if (def && def->isResource())
                    m_game.getActions().gather(m_game.getPlayer().getSelection(), target, shift);
            }
            break;
        case TargetingAction::RallyPoint:
            // Set rally point on selected buildings
            m_game.setRallyPoint(worldPos, target);
            break;
        default:
            break;
    }
}

bool InputHandler::handleActionBarClick(sf::Vector2i screenPos) {
    ActionBar& actionBar = m_game.getActionBar();
    actionBar.setWindowSize(m_window.getSize());
    Player& player = m_game.getPlayer();
    
    ActionBarClickResult result = actionBar.handleClick(screenPos, player, m_game.getActions());
    
    switch (result.type) {
        case ActionBarClickResult::Type::None:
            return false;  // Not on action bar
            
        case ActionBarClickResult::Type::TargetMove:
            enterTargetingMode(TargetingAction::Move);
            break;
            
        case ActionBarClickResult::Type::TargetAttack:
            enterTargetingMode(TargetingAction::Attack);
            break;
            
        case ActionBarClickResult::Type::TargetGather:
            enterTargetingMode(TargetingAction::Gather);
            break;
            
        case ActionBarClickResult::Type::TargetRallyPoint:
            enterTargetingMode(TargetingAction::RallyPoint);
            break;
            
        case ActionBarClickResult::Type::TargetBuild:
            enterBuildMode(result.buildType);
            break;
            
        case ActionBarClickResult::Type::CancelBuilding:
            m_game.cancelBuildingConstruction(player.getFirstOwnedSelectedEntity());
            break;
            
        case ActionBarClickResult::Type::Handled:
            // Action was executed by ActionBar (Stop, Train, Cancel queue)
            break;
    }
    
    return true;
}
