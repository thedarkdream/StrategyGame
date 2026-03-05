#include "InputHandler.h"
#include "Game.h"
#include "Constants.h"
#include "Entity.h"
#include "Unit.h"
#include "Building.h"
#include <cmath>
#include <algorithm>

InputHandler::InputHandler(sf::RenderWindow& window, Game& game)
    : m_window(window)
    , m_game(game)
{
    // Initialize camera
    m_camera.setSize(sf::Vector2f(static_cast<float>(Constants::WINDOW_WIDTH), 
                     static_cast<float>(Constants::WINDOW_HEIGHT)));
    m_camera.setCenter(sf::Vector2f(Constants::WINDOW_WIDTH / 2.0f, Constants::WINDOW_HEIGHT / 2.0f));
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
    sf::Vector2f movement(0.0f, 0.0f);
    
    if (mousePos.x < Constants::CAMERA_EDGE_MARGIN) {
        movement.x = -Constants::CAMERA_SPEED;
    } else if (mousePos.x > Constants::WINDOW_WIDTH - Constants::CAMERA_EDGE_MARGIN) {
        movement.x = Constants::CAMERA_SPEED;
    }
    
    if (mousePos.y < Constants::CAMERA_EDGE_MARGIN) {
        movement.y = -Constants::CAMERA_SPEED;
    } else if (mousePos.y > Constants::WINDOW_HEIGHT - Constants::CAMERA_EDGE_MARGIN) {
        movement.y = Constants::CAMERA_SPEED;
    }
    
    m_camera.move(movement * deltaTime);
}

void InputHandler::updateCameraKeyScroll(float deltaTime) {
    sf::Vector2f movement(0.0f, 0.0f);
    
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left) || 
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) {
        movement.x = -Constants::CAMERA_SPEED;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right) || 
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) {
        movement.x = Constants::CAMERA_SPEED;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up) || 
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) {
        movement.y = -Constants::CAMERA_SPEED;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down) || 
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) {
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
    float minimapX = Constants::MINIMAP_PADDING;
    float minimapY = Constants::WINDOW_HEIGHT - Constants::MINIMAP_SIZE - Constants::MINIMAP_PADDING;
    
    return screenPos.x >= minimapX && 
           screenPos.x <= minimapX + Constants::MINIMAP_SIZE &&
           screenPos.y >= minimapY && 
           screenPos.y <= minimapY + Constants::MINIMAP_SIZE;
}

sf::Vector2f InputHandler::minimapToWorld(sf::Vector2i screenPos) const {
    float minimapX = Constants::MINIMAP_PADDING;
    float minimapY = Constants::WINDOW_HEIGHT - Constants::MINIMAP_SIZE - Constants::MINIMAP_PADDING;
    
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
    
    sf::Vector2f worldPos = screenToWorld(position);
    
    if (button == sf::Mouse::Button::Left) {
        if (m_buildMode) {
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
        if (m_buildMode) {
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
        // Snap to grid
        int tileX = static_cast<int>(worldPos.x / Constants::TILE_SIZE);
        int tileY = static_cast<int>(worldPos.y / Constants::TILE_SIZE);
        m_buildPreviewPos = sf::Vector2f(
            tileX * Constants::TILE_SIZE + Constants::TILE_SIZE / 2.0f,
            tileY * Constants::TILE_SIZE + Constants::TILE_SIZE / 2.0f
        );
    }
}

void InputHandler::handleKeyPress(sf::Keyboard::Key code) {
    Player& player = m_game.getPlayer();
    
    switch (code) {
        case sf::Keyboard::Key::Escape:
            if (m_buildMode) {
                exitBuildMode();
            } else {
                player.clearSelection();
            }
            break;
            
        case sf::Keyboard::Key::B:
            // Build barracks
            if (player.canAfford(Constants::BARRACKS_COST_MINERALS, 0)) {
                enterBuildMode(EntityType::Barracks);
            }
            break;
            
        case sf::Keyboard::Key::H:
            // Build base/command center
            if (player.canAfford(Constants::BASE_COST_MINERALS, 0)) {
                enterBuildMode(EntityType::Base);
            }
            break;
            
        case sf::Keyboard::Key::T:
            // Train unit from selected building
            if (player.hasSelection()) {
                auto& selection = player.getSelection();
                for (auto& entity : selection) {
                    if (auto* building = dynamic_cast<Building*>(entity.get())) {
                        if (building->getType() == EntityType::Base) {
                            if (player.canAfford(Constants::WORKER_COST_MINERALS, 0)) {
                                if (building->trainUnit(EntityType::Worker)) {
                                    player.spendResources(Constants::WORKER_COST_MINERALS, 0);
                                }
                            }
                        } else if (building->getType() == EntityType::Barracks) {
                            if (player.canAfford(Constants::SOLDIER_COST_MINERALS, 0)) {
                                if (building->trainUnit(EntityType::Soldier)) {
                                    player.spendResources(Constants::SOLDIER_COST_MINERALS, 0);
                                }
                            }
                        }
                    }
                }
            }
            break;
            
        case sf::Keyboard::Key::Q:
            // Stop command
            for (auto& entity : player.getSelection()) {
                if (auto* unit = dynamic_cast<Unit*>(entity.get())) {
                    unit->stop();
                }
            }
            break;
            
        default:
            break;
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
        } else if (entity->getTeam() != Team::Neutral) {
            // Inspect enemy unit/building (view stats only)
            player.clearSelection();
            clearInspected();
            m_inspectedEnemy = entity;
            entity->setSelected(true);
        } else {
            // Clicked on neutral entity (resources) - clear all
            player.clearSelection();
            clearInspected();
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
