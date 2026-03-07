#include "InputHandler.h"
#include "Game.h"
#include "Constants.h"
#include "Entity.h"
#include "EntityData.h"
#include "Unit.h"
#include "Building.h"
#include "ActionBar.h"
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
    return m_window.mapPixelToCoords(screenPos, m_camera);
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
    sf::Vector2f center = m_camera.getCenter();
    sf::Vector2f halfSize = m_camera.getSize() / 2.0f;
    
    float mapWidth = static_cast<float>(Constants::MAP_WIDTH * Constants::TILE_SIZE);
    float mapHeight = static_cast<float>(Constants::MAP_HEIGHT * Constants::TILE_SIZE);
    
    center.x = std::max(halfSize.x, std::min(center.x, mapWidth - halfSize.x));
    center.y = std::max(halfSize.y, std::min(center.y, mapHeight - halfSize.y));
    
    m_camera.setCenter(center);
}

bool InputHandler::isPositionOnMinimap(sf::Vector2i screenPos) const {
    sf::Vector2u windowSize = m_window.getSize();
    float minimapX = Constants::MINIMAP_PADDING;
    float minimapY = static_cast<float>(windowSize.y) - Constants::MINIMAP_SIZE - Constants::MINIMAP_PADDING;
    
    return screenPos.x >= minimapX && 
           screenPos.x <= minimapX + Constants::MINIMAP_SIZE &&
           screenPos.y >= minimapY && 
           screenPos.y <= minimapY + Constants::MINIMAP_SIZE;
}

sf::Vector2f InputHandler::minimapToWorld(sf::Vector2i screenPos) const {
    sf::Vector2u windowSize = m_window.getSize();
    float minimapX = Constants::MINIMAP_PADDING;
    float minimapY = static_cast<float>(windowSize.y) - Constants::MINIMAP_SIZE - Constants::MINIMAP_PADDING;
    
    // Calculate relative position within the minimap (0 to 1)
    float relX = (screenPos.x - minimapX) / Constants::MINIMAP_SIZE;
    float relY = (screenPos.y - minimapY) / Constants::MINIMAP_SIZE;
    
    // Convert to world coordinates
    float worldX = relX * Constants::MAP_WIDTH * Constants::TILE_SIZE;
    float worldY = relY * Constants::MAP_HEIGHT * Constants::TILE_SIZE;
    
    return sf::Vector2f(worldX, worldY);
}

void InputHandler::centerCameraAt(sf::Vector2f worldPos) {
    m_camera.setCenter(worldPos);
    clampCamera();
}

void InputHandler::handleMousePress(sf::Vector2i position, sf::Mouse::Button button) {
    // Check if clicking on minimap first
    if (button == sf::Mouse::Button::Left && isPositionOnMinimap(position)) {
        m_isDraggingMinimap = true;
        sf::Vector2f worldPos = minimapToWorld(position);
        centerCameraAt(worldPos);
        return;
    }
    
    // Check if clicking on action bar (left-click only)
    if (button == sf::Mouse::Button::Left && handleActionBarClick(position)) {
        return;  // Action bar handled the click
    }
    
    sf::Vector2f worldPos = screenToWorld(position);
    
    if (button == sf::Mouse::Button::Left) {
        if (m_targetingMode) {
            // Execute the targeted action
            Player& player = m_game.getPlayer();
            EntityPtr target = m_game.getEntityAtPosition(worldPos);
            
            switch (m_targetingAction) {
                case TargetingAction::Move:
                    m_game.issueMoveCommand(worldPos);
                    break;
                case TargetingAction::Attack:
                    if (target && target->getTeam() != player.getTeam()) {
                        m_game.issueAttackCommand(target);
                    } else {
                        // Attack-move to location (move while attacking enemies on the way)
                        m_game.issueAttackMoveCommand(worldPos);
                    }
                    break;
                case TargetingAction::Gather:
                    if (target && (target->getType() == EntityType::MineralPatch || 
                                   target->getType() == EntityType::GasGeyser)) {
                        m_game.issueGatherCommand(target);
                    }
                    break;
                default:
                    break;
            }
            exitTargetingMode();
        } else if (m_buildMode) {
            // Place building
            m_game.issueBuildCommand(m_buildingToBuild, worldPos);
            exitBuildMode();
        } else {
            // Start selection box
            m_isSelecting = true;
            m_selectionStart = worldPos;
            m_selectionEnd = worldPos;
        }
    } else if (button == sf::Mouse::Button::Right) {
        if (m_targetingMode) {
            // Cancel targeting mode
            exitTargetingMode();
        } else if (m_buildMode) {
            exitBuildMode();
        } else {
            // Issue command to selected units
            Player& player = m_game.getPlayer();
            if (player.hasSelection()) {
                // Check if clicking on an enemy or resource
                EntityPtr target = m_game.getEntityAtPosition(worldPos);
                
                if (target) {
                    if (target->getTeam() != player.getTeam() && target->getTeam() != Team::Neutral) {
                        // Attack enemy
                        m_game.issueAttackCommand(target);
                    } else if (target->getType() == EntityType::MineralPatch || 
                               target->getType() == EntityType::GasGeyser) {
                        // Gather resources
                        m_game.issueGatherCommand(target);
                    } else if (auto* building = dynamic_cast<Building*>(target.get())) {
                        // Check if incomplete building - send workers to continue building
                        if (!building->isConstructed() && target->getTeam() == player.getTeam()) {
                            m_game.issueContinueBuildCommand(target);
                        } else {
                            // Move to location (right-click on own completed building)
                            m_game.issueMoveCommand(worldPos);
                        }
                    } else {
                        // Move to location
                        m_game.issueMoveCommand(worldPos);
                    }
                } else {
                    // Move to location
                    m_game.issueMoveCommand(worldPos);
                }
            }
        }
    }
}

void InputHandler::handleMouseRelease(sf::Vector2i position, sf::Mouse::Button button) {
    if (button == sf::Mouse::Button::Left) {
        // Stop minimap dragging
        if (m_isDraggingMinimap) {
            m_isDraggingMinimap = false;
            return;
        }
        
        if (m_isSelecting) {
            m_isSelecting = false;
            
            // Check if it was a click or drag
            sf::FloatRect selectionBox = getSelectionBox();
            if (selectionBox.size.x < 5.0f && selectionBox.size.y < 5.0f) {
                // Single click selection
                performSelection(m_selectionStart);
            } else {
                // Box selection
                performBoxSelection();
            }
        }
    }
}

void InputHandler::handleMouseMove(sf::Vector2i position) {
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
                if (auto* building = dynamic_cast<Building*>(entity.get())) {
                    if (!building->isConstructed()) {
                        m_game.cancelBuildingConstruction(entity);
                        return;
                    }
                }
            }
            
            // Check if selected building is producing - cancel production
            bool cancelledProduction = false;
            for (auto& entity : player.getSelection()) {
                if (auto* building = dynamic_cast<Building*>(entity.get())) {
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
    if (auto* building = dynamic_cast<Building*>(selectedEntity.get())) {
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
                // Stop command - apply to all selected units
                for (auto& entity : player.getSelection()) {
                    if (auto* unit = dynamic_cast<Unit*>(entity.get())) {
                        unit->stop();
                    }
                }
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
                // Train unit from selected building
                if (auto* building = dynamic_cast<Building*>(selectedEntity.get())) {
                    int mineralCost = ENTITY_DATA.getMineralCost(action.producesType);
                    int gasCost = ENTITY_DATA.getGasCost(action.producesType);
                    if (player.canAfford(mineralCost, gasCost)) {
                        if (building->trainUnit(action.producesType)) {
                            player.spendResources(mineralCost, gasCost);
                        }
                    }
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

void InputHandler::performSelection(sf::Vector2f worldPos) {
    EntityPtr entity = m_game.getEntityAtPosition(worldPos);
    
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
    sf::FloatRect selectionBox = getSelectionBox();
    
    std::vector<EntityPtr> selected = m_game.getEntitiesInRect(
        selectionBox, m_game.getPlayer().getTeam());
    
    // If we have both units and buildings, prefer units only
    bool hasUnits = false;
    for (const auto& entity : selected) {
        if (dynamic_cast<Unit*>(entity.get())) {
            hasUnits = true;
            break;
        }
    }
    
    if (hasUnits) {
        // Filter out buildings
        selected.erase(
            std::remove_if(selected.begin(), selected.end(),
                [](const EntityPtr& e) { return dynamic_cast<Building*>(e.get()) != nullptr; }),
            selected.end());
    }
    
    // Box selection only selects own units, clears any inspected enemy
    if (auto prev = m_inspectedEnemy.lock()) {
        prev->setSelected(false);
    }
    m_inspectedEnemy.reset();
    m_game.getPlayer().selectEntities(selected);
}

sf::FloatRect InputHandler::getSelectionBox() const {
    float left = std::min(m_selectionStart.x, m_selectionEnd.x);
    float top = std::min(m_selectionStart.y, m_selectionEnd.y);
    float width = std::abs(m_selectionEnd.x - m_selectionStart.x);
    float height = std::abs(m_selectionEnd.y - m_selectionStart.y);
    
    return sf::FloatRect(sf::Vector2f(left, top), sf::Vector2f(width, height));
}

void InputHandler::enterTargetingMode(TargetingAction action) {
    m_targetingMode = true;
    m_targetingAction = action;
}

void InputHandler::exitTargetingMode() {
    m_targetingMode = false;
    m_targetingAction = TargetingAction::None;
}

bool InputHandler::handleActionBarClick(sf::Vector2i screenPos) {
    ActionBar& actionBar = m_game.getActionBar();
    actionBar.setWindowSize(m_window.getSize());
    Player& player = m_game.getPlayer();
    
    ActionBarClickResult result = actionBar.handleClick(screenPos, player);
    
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
