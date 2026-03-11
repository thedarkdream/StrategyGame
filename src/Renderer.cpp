#include "Renderer.h"
#include "FontManager.h"
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
#include "EffectsManager.h"
#include <sstream>
#include <iomanip>
#include <iostream>

Renderer::Renderer(sf::RenderWindow& window)
    : m_window(window)
{
    m_font = FontManager::instance().defaultFont();
}

void Renderer::render(Game& game) {
    m_window.clear(sf::Color(20, 20, 30));
    
    // Set camera view for world rendering
    m_window.setView(m_camera);
    
    // Render map
    renderMap(game.getMap());
    
    // Render all entities
    renderEntities(game);
    
    // Render visual effects (explosions, etc.) on top of entities
    EFFECTS.render(m_window);
    
    // Render rally points for selected buildings
    renderRallyPoints(game);
    
    // Render selection box if selecting
    InputHandler& input = game.getInput();
    renderSelectionBox(input);
    
    // Render build preview
    if (input.isInBuildMode()) {
        renderBuildPreview(input, game.getMap());
    }
    
    // Switch to pixel-perfect UI view (must match current window size, not initial size)
    sf::Vector2u windowSize = m_window.getSize();
    sf::View uiView(sf::FloatRect(sf::Vector2f(0.f, 0.f), 
                    sf::Vector2f(static_cast<float>(windowSize.x), static_cast<float>(windowSize.y))));
    m_window.setView(uiView);
    
    // Render UI elements
    renderUI(game);
}

void Renderer::renderMap(Map& map) {
    map.render(m_window, m_camera);
}

void Renderer::renderEntities(Game& game) {
    // Render all entities (including dying ones playing death animation)
    for (const auto& entity : game.getAllEntities()) {
        if (entity && (entity->isAlive() || entity->isDying())) {
            entity->render(m_window);
        }
    }
}

void Renderer::renderRallyPoints(Game& game) {
    Player& player = game.getPlayer();
    
    for (const auto& entity : player.getSelection()) {
        auto* building = entity->asBuilding();
        if (!building || !building->isConstructed()) continue;
        
        // Check if building can produce units
        if (auto* bldgDef = ENTITY_DATA.getBuildingDef(entity->getType())) {
            if (!bldgDef->canProduce) continue;
        } else {
            continue;
        }
        
        sf::Vector2f buildingPos = building->getPosition();
        sf::Vector2f rallyPoint = building->getRallyPoint();
        
        // Draw line from building to rally point
        sf::Vertex line[] = {
            sf::Vertex{buildingPos, sf::Color(0, 255, 100, 180)},
            sf::Vertex{rallyPoint, sf::Color(0, 255, 100, 180)}
        };
        m_window.draw(line, 2, sf::PrimitiveType::Lines);
        
        // Draw rally point flag/marker
        float flagSize = 8.0f;
        sf::ConvexShape flag;
        flag.setPointCount(3);
        flag.setPoint(0, sf::Vector2f(0.0f, 0.0f));
        flag.setPoint(1, sf::Vector2f(flagSize * 1.5f, flagSize * 0.5f));
        flag.setPoint(2, sf::Vector2f(0.0f, flagSize));
        flag.setPosition(rallyPoint);
        flag.setFillColor(sf::Color(0, 255, 100, 200));
        flag.setOutlineThickness(1.0f);
        flag.setOutlineColor(sf::Color(0, 150, 50));
        m_window.draw(flag);
        
        // Draw flag pole
        sf::RectangleShape pole(sf::Vector2f(2.0f, flagSize + 8.0f));
        pole.setPosition(rallyPoint - sf::Vector2f(1.0f, 8.0f));
        pole.setFillColor(sf::Color(100, 50, 20));
        m_window.draw(pole);
    }
}

void Renderer::renderUI(Game& game) {
    renderResourceBar(game.getPlayer());
    renderMinimap(game);
    
    // Render action bar through ActionBar class
    ActionBar& actionBar = game.getActionBar();
    actionBar.setWindowSize(m_window.getSize());
    actionBar.setTargetingAction(game.getInput().getTargetingAction());
    actionBar.setBuildModeType(game.getInput().getBuildingType());
    if (m_font) {
        actionBar.setFont(m_font);
    }
    actionBar.render(m_window, game.getPlayer());
    
    renderUnitPanel(game);
    // Targeting mode is now shown via pressed button state in action bar
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
    
    // Get size from EntityData
    sf::Vector2f size = ENTITY_DATA.getSize(buildType);
    
    // Calculate tile position matching actual placement logic
    // The preview position is already at the center where the building will be placed
    // Work backwards to get the top-left tile for canPlaceBuilding check
    int tilesW = static_cast<int>(size.x / Constants::TILE_SIZE);
    int tilesH = static_cast<int>(size.y / Constants::TILE_SIZE);
    int tileX = static_cast<int>((pos.x - size.x / 2.0f) / Constants::TILE_SIZE);
    int tileY = static_cast<int>((pos.y - size.y / 2.0f) / Constants::TILE_SIZE);
    
    bool canPlace = map.canPlaceBuilding(tileX, tileY, tilesW, tilesH);
    
    // Draw placeholder rectangle
    sf::RectangleShape preview(size);
    preview.setOrigin(sf::Vector2f(size.x / 2.0f, size.y / 2.0f));
    preview.setPosition(pos);
    preview.setFillColor(canPlace ? sf::Color(0, 255, 0, 100) : sf::Color(255, 0, 0, 100));
    preview.setOutlineThickness(2.0f);
    preview.setOutlineColor(canPlace ? sf::Color::Green : sf::Color::Red);
    m_window.draw(preview);
    
    // Create a temporary building to render its sprite preview
    Building previewBuilding(buildType, Team::Player1, pos);
    sf::Color tint = canPlace ? sf::Color(150, 255, 150, 180) : sf::Color(255, 150, 150, 180);
    previewBuilding.renderPreview(m_window, tint);
}

// ---------------------------------------------------------------------------
// Minimap terrain cache
// ---------------------------------------------------------------------------
static sf::Color tileColor(TileType t) {
    switch (t) {
        case TileType::Grass:    return sf::Color( 42,  80,  42);   // dark green
        case TileType::Water:    return sf::Color( 30,  80, 130);   // dark blue
        case TileType::Resource: return sf::Color( 40, 130,  60);   // brighter green
        case TileType::Building: return sf::Color( 90,  90,  90);   // mid-grey
    }
    return sf::Color(42, 80, 42);
}

void Renderer::rebuildMinimapTerrain(Map& map) {
    const int W = map.getWidth();
    const int H = map.getHeight();

    sf::Image img;
    img.resize(sf::Vector2u(static_cast<unsigned>(W), static_cast<unsigned>(H)));

    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            img.setPixel(sf::Vector2u(static_cast<unsigned>(x),
                                      static_cast<unsigned>(y)),
                         tileColor(map.getTile(x, y).type));

    if (!m_minimapTerrainTex.loadFromImage(img))
        std::cerr << "Renderer: failed to upload minimap terrain texture\n";

    m_minimapTerrainDirty = false;
}

void Renderer::renderMinimap(Game& game) {
    sf::Vector2u windowSize = m_window.getSize();
    const float minimapSize = Constants::MINIMAP_SIZE;
    const float padding     = Constants::MINIMAP_PADDING;

    const float mmX = padding;
    const float mmY = static_cast<float>(windowSize.y) - minimapSize - padding;

    // --- Rebuild terrain texture if stale -----------------------------------
    if (m_minimapTerrainDirty)
        rebuildMinimapTerrain(game.getMap());

    // --- Background (border + fallback fill) --------------------------------
    sf::RectangleShape bg(sf::Vector2f(minimapSize, minimapSize));
    bg.setPosition(sf::Vector2f(mmX, mmY));
    bg.setFillColor(sf::Color(30, 30, 30, 220));
    bg.setOutlineThickness(2.0f);
    bg.setOutlineColor(sf::Color(110, 120, 130));
    m_window.draw(bg);

    // --- Terrain layer ------------------------------------------------------
    sf::Sprite terrainSprite(m_minimapTerrainTex);
    terrainSprite.setPosition(sf::Vector2f(mmX, mmY));
    // Scale the terrain image to fill minimapSize x minimapSize
    const Map& map = game.getMap();
    float tScaleX = minimapSize / static_cast<float>(map.getWidth());
    float tScaleY = minimapSize / static_cast<float>(map.getHeight());
    terrainSprite.setScale(sf::Vector2f(tScaleX, tScaleY));
    m_window.draw(terrainSprite);

    // --- Scale factors (world → minimap pixel) ------------------------------
    float scaleX = minimapSize / (map.getWidth()  * Constants::TILE_SIZE);
    float scaleY = minimapSize / (map.getHeight() * Constants::TILE_SIZE);

    // --- Entities -----------------------------------------------------------
    for (const auto& entity : game.getAllEntities()) {
        if (!entity || !entity->isAlive()) continue;

        sf::Vector2f pos = entity->getPosition();
        float x = mmX + pos.x * scaleX;
        float y = mmY + pos.y * scaleY;

        float dotR = 2.5f;
        sf::CircleShape dot(dotR);
        dot.setOrigin(sf::Vector2f(dotR, dotR));
        dot.setPosition(sf::Vector2f(x, y));
        dot.setFillColor(teamColor(entity->getTeam()));
        m_window.draw(dot);
    }

    // --- Camera view rectangle ----------------------------------------------
    sf::Vector2f camCenter = m_camera.getCenter();
    sf::Vector2f camSize   = m_camera.getSize();

    sf::RectangleShape camRect(sf::Vector2f(camSize.x * scaleX, camSize.y * scaleY));
    camRect.setPosition(sf::Vector2f(
        mmX + (camCenter.x - camSize.x / 2.0f) * scaleX,
        mmY + (camCenter.y - camSize.y / 2.0f) * scaleY
    ));
    camRect.setFillColor(sf::Color::Transparent);
    camRect.setOutlineThickness(1.0f);
    camRect.setOutlineColor(sf::Color::White);
    m_window.draw(camRect);
}

void Renderer::renderResourceBar(Player& player) {
    sf::Vector2u windowSize = m_window.getSize();
    const float barHeight = 30.0f;
    
    // Background
    sf::RectangleShape bg(sf::Vector2f(static_cast<float>(windowSize.x), barHeight));
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
    sf::Vector2u windowSize = m_window.getSize();
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
    bool isEnemyInspection = isInspection &&
                               inspectedEnemy->getTeam() != Team::Neutral &&
                               inspectedEnemy->getTeam() != game.getPlayer().getTeam();
    
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
        static_cast<float>(windowSize.x) - panelWidth - padding,
        static_cast<float>(windowSize.y) - panelHeight - padding
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
    
    std::string typeStr = ENTITY_DATA.getName(entity->getType());
    
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
        static_cast<float>(windowSize.x) - panelWidth - padding + 10.0f,
        static_cast<float>(windowSize.y) - panelHeight - padding + 10.0f
    ));
    m_window.draw(nameText);
    
    sf::Text healthText(*m_font, "HP: " + std::to_string(entity->getHealth()) + "/" + 
                         std::to_string(entity->getMaxHealth()), 12);
    healthText.setFillColor(sf::Color(200, 200, 200));
    healthText.setPosition(sf::Vector2f(
        static_cast<float>(windowSize.x) - panelWidth - padding + 10.0f,
        static_cast<float>(windowSize.y) - panelHeight - padding + 35.0f
    ));
    m_window.draw(healthText);
    
    float panelX = static_cast<float>(windowSize.x) - panelWidth - padding + 10.0f;
    float statsY = static_cast<float>(windowSize.y) - panelHeight - padding + 55.0f;
    
    // Show unit-specific stats if this is a unit
    if (auto* unit = entity->asUnit()) {
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
    else if (auto* resourceNode = entity->asResourceNode()) {
        std::string resourceStr = "Remaining: " + std::to_string(resourceNode->getRemainingResources());
        sf::Text resourceText(*m_font, resourceStr, 12);
        resourceText.setFillColor(sf::Color(100, 200, 255));
        resourceText.setPosition(sf::Vector2f(panelX, statsY));
        m_window.draw(resourceText);
    }
}

void Renderer::renderTargetingModeIndicator(Game& game) {
    InputHandler& input = game.getInput();
    if (!input.isInTargetingMode()) return;
    
    if (!m_font) return;
    
    sf::Vector2u windowSize = m_window.getSize();
    
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
        (static_cast<float>(windowSize.x) - bounds.size.x) / 2.0f,
        60.0f
    ));
    
    // Draw background box
    sf::RectangleShape bg(sf::Vector2f(bounds.size.x + 20.0f, bounds.size.y + 14.0f));
    bg.setPosition(sf::Vector2f(
        (static_cast<float>(windowSize.x) - bounds.size.x) / 2.0f - 10.0f,
        56.0f
    ));
    bg.setFillColor(sf::Color(0, 0, 0, 180));
    m_window.draw(bg);
    m_window.draw(text);
}

