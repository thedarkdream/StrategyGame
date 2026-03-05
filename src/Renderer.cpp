#include "Renderer.h"
#include "Game.h"
#include "Constants.h"
#include "Entity.h"
#include "EntityData.h"
#include "Unit.h"
#include "Building.h"
#include "ResourceNode.h"
#include "Player.h"
#include "InputHandler.h"
#include "ActionBar.h"
#include <sstream>
#include <iomanip>

Renderer::Renderer(sf::RenderWindow& window)
    : m_window(window)
{
    // Try to load a basic font
    try {
        m_font.emplace("C:/Windows/Fonts/arial.ttf");
    } catch (...) {
        // Font loading failed, UI text won't be displayed
    }
}

void Renderer::render(Game& game) {
    m_window.clear(sf::Color(20, 20, 30));
    
    // Set camera view for world rendering
    m_window.setView(m_camera);
    
    // Render map
    renderMap(game.getMap());
    
    // Render all entities
    renderEntities(game);
    
    // Render selection box if selecting
    InputHandler& input = game.getInput();
    renderSelectionBox(input);
    
    // Render build preview
    if (input.isInBuildMode()) {
        renderBuildPreview(input, game.getMap());
    }
    
    // Switch to default view for UI
    m_window.setView(m_window.getDefaultView());
    
    // Render UI elements
    renderUI(game);
}

void Renderer::renderMap(Map& map) {
    map.render(m_window, m_camera);
}

void Renderer::renderEntities(Game& game) {
    // Render all entities
    for (const auto& entity : game.getAllEntities()) {
        if (entity && entity->isAlive()) {
            entity->render(m_window);
        }
    }
}

void Renderer::renderUI(Game& game) {
    renderResourceBar(game.getPlayer());
    renderMinimap(game);
    
    // Render action bar through ActionBar class
    ActionBar& actionBar = game.getActionBar();
    if (m_font) {
        actionBar.setFont(&(*m_font));
    }
    actionBar.render(m_window, game.getPlayer());
    
    renderUnitPanel(game);
    renderTargetingModeIndicator(game);
}

void Renderer::renderSelectionBox(const InputHandler& input) {
    if (!input.isSelecting()) return;
    
    sf::FloatRect box = input.getSelectionBox();
    
    sf::RectangleShape selectionRect(box.size);
    selectionRect.setPosition(box.position);
    selectionRect.setFillColor(sf::Color(0, 120, 200, 50));
    selectionRect.setOutlineThickness(1.0f);
    selectionRect.setOutlineColor(sf::Color(0, 180, 255, 200));
    m_window.draw(selectionRect);
}

void Renderer::renderBuildPreview(const InputHandler& input, Map& map) {
    sf::Vector2f pos = input.getBuildPreviewPosition();
    EntityType buildType = input.getBuildingType();
    
    sf::Vector2f size;
    switch (buildType) {
        case EntityType::Base:
            size = sf::Vector2f(96.0f, 96.0f);
            break;
        case EntityType::Barracks:
        case EntityType::Refinery:
            size = sf::Vector2f(64.0f, 64.0f);
            break;
        default:
            size = sf::Vector2f(32.0f, 32.0f);
            break;
    }
    
    // Check if placement is valid
    int tileX = static_cast<int>((pos.x - size.x / 2.0f) / Constants::TILE_SIZE);
    int tileY = static_cast<int>((pos.y - size.y / 2.0f) / Constants::TILE_SIZE);
    int tilesW = static_cast<int>(size.x / Constants::TILE_SIZE);
    int tilesH = static_cast<int>(size.y / Constants::TILE_SIZE);
    
    bool canPlace = map.canPlaceBuilding(tileX, tileY, tilesW, tilesH);
    
    sf::RectangleShape preview(size);
    preview.setOrigin(sf::Vector2f(size.x / 2.0f, size.y / 2.0f));
    preview.setPosition(pos);
    preview.setFillColor(canPlace ? sf::Color(0, 255, 0, 100) : sf::Color(255, 0, 0, 100));
    preview.setOutlineThickness(2.0f);
    preview.setOutlineColor(canPlace ? sf::Color::Green : sf::Color::Red);
    
    m_window.draw(preview);
}

void Renderer::renderMinimap(Game& game) {
    const float minimapSize = Constants::MINIMAP_SIZE;
    const float padding = Constants::MINIMAP_PADDING;
    
    // Background
    sf::RectangleShape bg(sf::Vector2f(minimapSize, minimapSize));
    bg.setPosition(sf::Vector2f(padding, Constants::WINDOW_HEIGHT - minimapSize - padding));
    bg.setFillColor(sf::Color(30, 30, 30, 200));
    bg.setOutlineThickness(2.0f);
    bg.setOutlineColor(sf::Color(100, 100, 100));
    m_window.draw(bg);
    
    // Scale factor
    float scaleX = minimapSize / (Constants::MAP_WIDTH * Constants::TILE_SIZE);
    float scaleY = minimapSize / (Constants::MAP_HEIGHT * Constants::TILE_SIZE);
    
    // Draw entities on minimap
    for (const auto& entity : game.getAllEntities()) {
        if (!entity || !entity->isAlive()) continue;
        
        sf::Vector2f pos = entity->getPosition();
        float x = padding + pos.x * scaleX;
        float y = (Constants::WINDOW_HEIGHT - minimapSize - padding) + pos.y * scaleY;
        
        sf::CircleShape dot(2.0f);
        dot.setOrigin(sf::Vector2f(2.0f, 2.0f));
        dot.setPosition(sf::Vector2f(x, y));
        dot.setFillColor(getTeamColor(entity->getTeam()));
        m_window.draw(dot);
    }
    
    // Draw camera view rectangle
    sf::Vector2f camCenter = m_camera.getCenter();
    sf::Vector2f camSize = m_camera.getSize();
    
    sf::RectangleShape camRect(sf::Vector2f(camSize.x * scaleX, camSize.y * scaleY));
    camRect.setPosition(sf::Vector2f(
        padding + (camCenter.x - camSize.x / 2.0f) * scaleX,
        (Constants::WINDOW_HEIGHT - minimapSize - padding) + (camCenter.y - camSize.y / 2.0f) * scaleY
    ));
    camRect.setFillColor(sf::Color::Transparent);
    camRect.setOutlineThickness(1.0f);
    camRect.setOutlineColor(sf::Color::White);
    m_window.draw(camRect);
}

void Renderer::renderResourceBar(Player& player) {
    const float barHeight = 30.0f;
    
    // Background
    sf::RectangleShape bg(sf::Vector2f(static_cast<float>(Constants::WINDOW_WIDTH), barHeight));
    bg.setFillColor(sf::Color(40, 40, 50, 230));
    m_window.draw(bg);
    
    if (!m_font) return;
    
    // Minerals
    sf::Text mineralsText(*m_font, "Minerals: " + std::to_string(player.getResources().minerals), 16);
    mineralsText.setFillColor(sf::Color::Cyan);
    mineralsText.setPosition(sf::Vector2f(20.0f, 5.0f));
    m_window.draw(mineralsText);
    
    // Gas
    sf::Text gasText(*m_font, "Gas: " + std::to_string(player.getResources().gas), 16);
    gasText.setFillColor(sf::Color::Green);
    gasText.setPosition(sf::Vector2f(200.0f, 5.0f));
    m_window.draw(gasText);
    
    // Unit count
    sf::Text unitText(*m_font, "Units: " + std::to_string(player.getUnitCount()) + 
                       " | Buildings: " + std::to_string(player.getBuildingCount()), 16);
    unitText.setFillColor(sf::Color::White);
    unitText.setPosition(sf::Vector2f(350.0f, 5.0f));
    m_window.draw(unitText);
}

void Renderer::renderUnitPanel(Game& game) {
    Player& player = game.getPlayer();
    EntityPtr inspectedEnemy = game.getInput().getInspectedEnemy();
    
    // Check if we have something to display
    if (!player.hasSelection() && !inspectedEnemy) return;
    
    const float panelWidth = 300.0f;
    const float panelHeight = 140.0f;
    const float padding = 10.0f;
    
    // Determine what we're displaying and set panel color accordingly
    bool isInspection = !player.hasSelection() && inspectedEnemy;
    bool isNeutral = isInspection && inspectedEnemy->getTeam() == Team::Neutral;
    bool isEnemyInspection = isInspection && inspectedEnemy->getTeam() == Team::Enemy;
    
    sf::Color panelOutlineColor;
    if (isEnemyInspection) {
        panelOutlineColor = sf::Color(180, 60, 60);  // Red for enemy
    } else if (isNeutral) {
        panelOutlineColor = sf::Color(100, 180, 220);  // Cyan for neutral/resources
    } else {
        panelOutlineColor = sf::Color(100, 100, 100);  // Gray for own selection
    }
    
    // Background
    sf::RectangleShape bg(sf::Vector2f(panelWidth, panelHeight));
    bg.setPosition(sf::Vector2f(
        Constants::WINDOW_WIDTH - panelWidth - padding,
        Constants::WINDOW_HEIGHT - panelHeight - padding
    ));
    bg.setFillColor(sf::Color(40, 40, 50, 230));
    bg.setOutlineThickness(2.0f);
    bg.setOutlineColor(panelOutlineColor);
    m_window.draw(bg);
    
    if (!m_font) return;
    
    // Determine which entity to show info for
    EntityPtr entity;
    int selectionCount = 1;
    
    if (player.hasSelection()) {
        const auto& selection = player.getSelection();
        entity = selection[0];
        selectionCount = static_cast<int>(selection.size());
    } else {
        entity = inspectedEnemy;
    }
    
    std::string typeStr;
    switch (entity->getType()) {
        case EntityType::Worker: typeStr = "Worker"; break;
        case EntityType::Soldier: typeStr = "Soldier"; break;
        case EntityType::Brute: typeStr = "Brute"; break;
        case EntityType::Base: typeStr = "Command Center"; break;
        case EntityType::Barracks: typeStr = "Barracks"; break;
        case EntityType::MineralPatch: typeStr = "Mineral Patch"; break;
        case EntityType::GasGeyser: typeStr = "Gas Geyser"; break;
        default: typeStr = "Unknown"; break;
    }
    
    // Build name string - for enemy, show "Enemy" prefix; for neutral, just name; for own units, show count
    std::string nameStr;
    sf::Color nameColor;
    if (isEnemyInspection) {
        nameStr = "Enemy " + typeStr;
        nameColor = sf::Color(255, 150, 150);  // Red tint
    } else if (isNeutral) {
        nameStr = typeStr;
        nameColor = sf::Color(150, 220, 255);  // Cyan tint
    } else {
        nameStr = typeStr + " (x" + std::to_string(selectionCount) + ")";
        nameColor = sf::Color::White;
    }
    
    sf::Text nameText(*m_font, nameStr, 14);
    nameText.setFillColor(nameColor);
    nameText.setPosition(sf::Vector2f(
        Constants::WINDOW_WIDTH - panelWidth - padding + 10.0f,
        Constants::WINDOW_HEIGHT - panelHeight - padding + 10.0f
    ));
    m_window.draw(nameText);
    
    sf::Text healthText(*m_font, "HP: " + std::to_string(entity->getHealth()) + "/" + 
                         std::to_string(entity->getMaxHealth()), 12);
    healthText.setFillColor(sf::Color(200, 200, 200));
    healthText.setPosition(sf::Vector2f(
        Constants::WINDOW_WIDTH - panelWidth - padding + 10.0f,
        Constants::WINDOW_HEIGHT - panelHeight - padding + 35.0f
    ));
    m_window.draw(healthText);
    
    float panelX = Constants::WINDOW_WIDTH - panelWidth - padding + 10.0f;
    float statsY = Constants::WINDOW_HEIGHT - panelHeight - padding + 55.0f;
    
    // Show unit-specific stats if this is a unit
    if (auto* unit = dynamic_cast<Unit*>(entity.get())) {
        // Format attack speed as attacks per second
        std::ostringstream apsStream;
        apsStream << std::fixed << std::setprecision(2) << unit->getAttacksPerSecond();
        
        std::string statsLine1 = "DMG: " + std::to_string(unit->getDamage()) + 
                                  "  |  APS: " + apsStream.str();
        sf::Text statsText1(*m_font, statsLine1, 11);
        statsText1.setFillColor(sf::Color(180, 180, 180));
        statsText1.setPosition(sf::Vector2f(panelX, statsY));
        m_window.draw(statsText1);
        
        std::ostringstream speedStream, rangeStream;
        speedStream << std::fixed << std::setprecision(0) << unit->getSpeed();
        rangeStream << std::fixed << std::setprecision(0) << unit->getAttackRange();
        
        std::string statsLine2 = "Speed: " + speedStream.str() + 
                                  "  |  Range: " + rangeStream.str();
        sf::Text statsText2(*m_font, statsLine2, 11);
        statsText2.setFillColor(sf::Color(180, 180, 180));
        statsText2.setPosition(sf::Vector2f(panelX, statsY + 16.0f));
        m_window.draw(statsText2);
    }
    // Show resource info for mineral patches and gas geysers
    else if (auto* resourceNode = dynamic_cast<ResourceNode*>(entity.get())) {
        std::string resourceStr = "Remaining: " + std::to_string(resourceNode->getRemainingResources());
        sf::Text resourceText(*m_font, resourceStr, 12);
        resourceText.setFillColor(sf::Color(100, 200, 255));
        resourceText.setPosition(sf::Vector2f(panelX, statsY));
        m_window.draw(resourceText);
    }
    
    // Show hotkeys (only for own units, not inspected entities)
    if (!isInspection) {
        sf::Text hotkeyText(*m_font, "[T] Train | [B] Barracks | [H] Base", 11);
        hotkeyText.setFillColor(sf::Color(150, 150, 150));
        hotkeyText.setPosition(sf::Vector2f(
            Constants::WINDOW_WIDTH - panelWidth - padding + 10.0f,
            Constants::WINDOW_HEIGHT - panelHeight - padding + 115.0f
        ));
        m_window.draw(hotkeyText);
    }
}

void Renderer::renderTargetingModeIndicator(Game& game) {
    InputHandler& input = game.getInput();
    if (!input.isInTargetingMode()) return;
    
    if (!m_font) return;
    
    // Show targeting mode text at top-center
    std::string actionText;
    sf::Color textColor;
    
    switch (input.getTargetingAction()) {
        case TargetingAction::Move:
            actionText = "Select Move Target (Right-click to cancel)";
            textColor = sf::Color(100, 200, 100);
            break;
        case TargetingAction::Attack:
            actionText = "Select Attack Target (Right-click to cancel)";
            textColor = sf::Color(200, 100, 100);
            break;
        case TargetingAction::Gather:
            actionText = "Select Resource to Gather (Right-click to cancel)";
            textColor = sf::Color(200, 200, 100);
            break;
        default:
            return;
    }
    
    sf::Text text(*m_font, actionText, 16);
    text.setFillColor(textColor);
    
    sf::FloatRect bounds = text.getLocalBounds();
    text.setPosition(sf::Vector2f(
        (Constants::WINDOW_WIDTH - bounds.size.x) / 2.0f,
        60.0f
    ));
    
    // Draw background box
    sf::RectangleShape bg(sf::Vector2f(bounds.size.x + 20.0f, bounds.size.y + 14.0f));
    bg.setPosition(sf::Vector2f(
        (Constants::WINDOW_WIDTH - bounds.size.x) / 2.0f - 10.0f,
        56.0f
    ));
    bg.setFillColor(sf::Color(0, 0, 0, 180));
    m_window.draw(bg);
    m_window.draw(text);
}

sf::Color Renderer::getTeamColor(Team team) {
    switch (team) {
        case Team::Player:
            return sf::Color(0x34, 0x98, 0xDB);  // Blue
        case Team::Enemy:
            return sf::Color(0xE7, 0x4C, 0x3C);  // Red
        case Team::Neutral:
            return sf::Color(0x95, 0xA5, 0xA6);  // Gray
        default:
            return sf::Color::White;
    }
}
