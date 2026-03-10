#include "Game.h"
#include "Unit.h"
#include "Worker.h"
#include "Soldier.h"
#include "Building.h"
#include "ResourceNode.h"
#include "ResourceManager.h"
#include "EffectsManager.h"
#include "SoundManager.h"
#include "TextureManager.h"
#include "LightTank.h"
#include "Projectile.h"
#include "Constants.h"
#include "MathUtil.h"
#include <algorithm>
#include <iostream>
#include <cmath>

Game::Game(sf::RenderWindow& window, const std::string& mapFile, int localPlayerSlot)
    : m_window(window)
    , m_mapFile(mapFile)
    , m_localSlot(localPlayerSlot)
{
    initialize();
}

void Game::handleEvent(const sf::Event& event) {
    if (const auto* resized = event.getIf<sf::Event::Resized>()) {
        m_input->onWindowResize(resized->size);
    }
    
    m_input->handleEvent(event);
}

void Game::update(float deltaTime) {
    // Update input (camera movement)
    m_input->update(deltaTime);
    
    // Update sound listener position to camera center
    SOUNDS.setListenerPosition(m_input->getCamera().getCenter());
    
    // Update all entities (units, buildings, projectiles, etc.) regardless of owner
    // NOTE: iterate by snapshot size - new entities spawned mid-loop (e.g. rockets)
    // are buffered in m_pendingEntities and flushed after the loop to avoid
    // iterator invalidation from vector reallocation.
    for (auto& entity : m_allEntities) {
        if (entity && entity->isAlive()) {
            entity->update(deltaTime);
            entity->updateHighlight(deltaTime);
        }
    }
    flushPendingEntities();  // Safe to add new entities now

    // Update all player controllers (AI ticks, network polling, etc.)
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (m_controllers[i])
            m_controllers[i]->update(deltaTime);
    }
    
    // Update visual effects
    EFFECTS.update(deltaTime);
    
    // Cleanup dead entities
    cleanupDeadEntities();
    
    // Check victory/defeat
    checkVictoryConditions();
}

void Game::render() {
    m_renderer->setCamera(m_input->getCamera());
    m_renderer->render(*this);
}

void Game::initialize() {
    Resources startingRes;
    startingRes.minerals = Constants::STARTING_MINERALS;
    startingRes.gas = Constants::STARTING_GAS;

    // --- Determine player count from the map file (before creating players) ---
    int playerCount = 2; // default for procedural / fallback
    std::optional<MapData> mapData;
    if (!m_mapFile.empty() && m_mapFile != "default") {
        std::string path = "maps/" + m_mapFile + ".stmap";
        mapData = MapSerializer::load(path);
        if (mapData && mapData->playerCount > 1)
            playerCount = std::min(mapData->playerCount, MAX_PLAYERS);
    }

    // --- Create exactly as many Player objects as the map needs ---
    for (int i = 0; i < playerCount; ++i)
        m_players[i] = std::make_unique<Player>(teamFromIndex(i), startingRes);

    // Create per-player action dispatchers (must be before controllers so AI can reference them)
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (!m_players[i]) continue;
        m_actions[i] = std::make_unique<PlayerActions>(*m_players[i], *this);
        m_actions[i]->setLocalPlayer(i == m_localSlot);
    }

    // Create input handler and renderer
    m_input    = std::make_unique<InputHandler>(m_window, *this);
    m_renderer = std::make_unique<Renderer>(m_window);

    // Assign controllers: human for the local slot, AI for all other occupied slots
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (!m_players[i]) continue;
        if (i == m_localSlot) {
            m_controllers[i] = std::make_unique<HumanController>();
        } else {
            m_controllers[i] = std::make_unique<AIPlayerController>(*m_players[i], *this);
        }
    }

    // Preload all assets so nothing freezes during gameplay
    preloadAssets();

    // Load map – game requires a valid .stmap file
    if (mapData) {
        setupFromMapData(*mapData);
    } else {
        std::cerr << "Game: failed to load map '" << m_mapFile << "'.\n";
    }
}

void Game::preloadAssets() {
    Worker::preload();
    Soldier::preload();
    Building::preload();
    ResourceNode::preload();
    LightTank::preload();
    Projectile::preload();
    EffectsManager::preload();
}

void Game::cleanupDeadEntities() {
    for (auto& p : m_players) if (p) p->cleanupDeadEntities();
    
    // Remove entities that are ready for removal (dead and death animation finished)
    m_allEntities.erase(
        std::remove_if(m_allEntities.begin(), m_allEntities.end(),
            [](const EntityPtr& entity) { return !entity || entity->isReadyForRemoval(); }),
        m_allEntities.end()
    );
}

void Game::checkVictoryConditions() {
    if (m_players[m_localSlot] && m_players[m_localSlot]->isDefeated()) {
        m_state = GameState::Defeat;
    } else {
        bool anyActiveEnemy = false;
        for (int i = 0; i < MAX_PLAYERS; ++i) {
            if (i != m_localSlot && m_players[i] && !m_players[i]->isDefeated()) {
                anyActiveEnemy = true;
                break;
            }
        }
        if (!anyActiveEnemy) {
            m_state = GameState::Victory;
        }
    }
}

EntityPtr Game::getEntityAtPosition(sf::Vector2f position) {
    for (auto& entity : m_allEntities) {
        if (entity && entity->isAlive() && entity->getType() != EntityType::Rocket
            && entity->getBounds().contains(position)) {
            return entity;
        }
    }
    return nullptr;
}

std::vector<EntityPtr> Game::getEntitiesInRect(sf::FloatRect rect) {
    std::vector<EntityPtr> result;
    for (auto& entity : m_allEntities) {
        if (entity && entity->isAlive() && rect.findIntersection(entity->getBounds()).has_value()) {
            result.push_back(entity);
        }
    }
    return result;
}

std::vector<EntityPtr> Game::getEntitiesInRect(sf::FloatRect rect, Team team) {
    std::vector<EntityPtr> result;
    for (auto& entity : m_allEntities) {
        if (entity && entity->isAlive() && 
            entity->getTeam() == team && 
            rect.findIntersection(entity->getBounds()).has_value()) {
            result.push_back(entity);
        }
    }
    return result;
}

EntityPtr Game::findNearestEnemy(sf::Vector2f pos, float radius, Team excludeTeam) {
    return findNearest(pos, radius, [excludeTeam](const EntityPtr& entity) {
        return entity->getTeam() != excludeTeam && entity->getTeam() != Team::Neutral;
    });
}

EntityPtr Game::findNearestResource(sf::Vector2f pos, float radius) {
    return findNearest(pos, radius, [](const EntityPtr& entity) {
        return entity->getType() == EntityType::MineralPatch || 
               entity->getType() == EntityType::GasGeyser;
    });
}

EntityPtr Game::findNearestAvailableResource(sf::Vector2f pos, float radius, EntityPtr exclude) {
    return findNearest(pos, radius, [exclude](const EntityPtr& entity) {
        if (entity->getType() != EntityType::MineralPatch && 
            entity->getType() != EntityType::GasGeyser) return false;
        if (entity == exclude) return false;
        
        // Check if this resource is being actively mined
        if (auto* resourceNode = entity->asResourceNode()) {
            if (resourceNode->isBeingMined()) return false;
        }
        return true;
    });
}

void Game::setupUnit(UnitPtr& unit) {
    unit->setMap(&m_map);
    unit->setContext(this);
}

void Game::setupWorker(Worker* worker, EntityPtr homeBase) {
    worker->setHomeBase(homeBase);
}

bool Game::checkPositionBlocked(sf::Vector2f pos, float radius, Entity* excludeSelf) {
    // Check both live entities AND entities pending flush so units spawned on the
    // same frame (e.g. two factories completing on the same tick) don't overlap.
    for (const EntityList* list : {&m_allEntities, &m_pendingEntities}) {
    for (auto& entity : *list) {
        if (!entity || !entity->isAlive()) continue;
        if (entity.get() == excludeSelf) continue;
        
        // Check if it's a unit and collidable
        if (auto* unit = entity->asUnit()) {
            if (!unit->isCollidable()) continue;
            
            float otherRadius = unit->getCollisionRadius();
            float dist = MathUtil::distance(entity->getPosition(), pos);
            float minDist = radius + otherRadius;
            
            if (dist < minDist) {
                return true;
            }
        }
        // Check buildings
        else if (entity->asBuilding()) {
            sf::FloatRect bounds = entity->getBounds();
            // Expand bounds by radius
            bounds.position.x -= radius;
            bounds.position.y -= radius;
            bounds.size.x += radius * 2;
            bounds.size.y += radius * 2;
            
            if (bounds.contains(pos)) {
                return true;
            }
        }
        // Check resource nodes (mineral patches, gas geysers)
        else if (entity->getType() == EntityType::MineralPatch ||
                 entity->getType() == EntityType::GasGeyser) {
            sf::FloatRect bounds = entity->getBounds();
            // Expand bounds by radius
            bounds.position.x -= radius;
            bounds.position.y -= radius;
            bounds.size.x += radius * 2;
            bounds.size.y += radius * 2;
            
            if (bounds.contains(pos)) {
                return true;
            }
        }
    } // end entity loop
    } // end list loop

    return false;
}

sf::Vector2f Game::findSpawnPosition(sf::Vector2f origin, float unitRadius) {
    // Try the original position first
    if (!checkPositionBlocked(origin, unitRadius, nullptr)) {
        return origin;
    }
    
    // Search in expanding circles
    float searchRadius = unitRadius * 2.5f;
    for (int ring = 1; ring <= 5; ++ring) {
        float ringRadius = searchRadius * ring;
        int points = 8 * ring;  // More points in outer rings
        
        for (int i = 0; i < points; ++i) {
            float angle = (2.0f * 3.14159f * i) / points;
            sf::Vector2f testPos = origin + sf::Vector2f(
                std::cos(angle) * ringRadius,
                std::sin(angle) * ringRadius
            );
            
            if (!checkPositionBlocked(testPos, unitRadius, nullptr)) {
                return testPos;
            }
        }
    }
    
    // Fallback to original if no free spot found
    return origin;
}

std::vector<RVONeighbor> Game::getNearbyUnitsRVO(sf::Vector2f pos, float radius, Unit* excludeSelf) {
    std::vector<RVONeighbor> result;
    
    for (auto& entity : m_allEntities) {
        if (!entity || !entity->isAlive()) continue;
        
        if (auto* unit = entity->asUnit()) {
            if (unit == excludeSelf) continue;
            if (!unit->isCollidable()) continue;
            
            float dist = MathUtil::distance(entity->getPosition(), pos);
            if (dist < radius) {
                RVONeighbor neighbor;
                neighbor.position = unit->getPosition();
                neighbor.velocity = unit->getVelocity();
                neighbor.radius = unit->getCollisionRadius();
                result.push_back(neighbor);
            }
        }
    }
    
    return result;
}

void Game::addEntity(EntityPtr entity) {
    // Buffer the entity; it will be moved to m_allEntities after the current update
    // loop completes, preventing vector reallocation from invalidating loop iterators.
    m_pendingEntities.push_back(entity);
}

void Game::flushPendingEntities() {
    if (m_pendingEntities.empty()) return;
    m_allEntities.insert(m_allEntities.end(),
        std::make_move_iterator(m_pendingEntities.begin()),
        std::make_move_iterator(m_pendingEntities.end()));
    m_pendingEntities.clear();
}

void Game::removeEntity(EntityPtr entity) {
    auto it = std::find(m_allEntities.begin(), m_allEntities.end(), entity);
    if (it != m_allEntities.end()) {
        m_allEntities.erase(it);
    }
}

// ---------------------------------------------------------------------------
// Player slot helpers
// ---------------------------------------------------------------------------
Player& Game::getEnemy() {
    // Returns the first non-local occupied slot (for 2-player games).
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (i != m_localSlot && m_players[i])
            return *m_players[i];
    }
    return *m_players[m_localSlot]; // Fallback — should never happen
}

Player* Game::getPlayerByTeam(Team t) {
    int idx = teamToIndex(t);
    if (idx < 0 || idx >= MAX_PLAYERS) return nullptr;  // Neutral
    return m_players[idx].get();
}

void Game::setupFromMapData(const MapData& data) {
    // Re-initialise the map to match the saved dimensions
    m_map = Map(data.width, data.height);

    // Apply saved tile types
    for (const auto& t : data.tiles)
        m_map.setTileType(t.x, t.y, t.type, t.variant);

    // Spawn all saved entities
    const float TS = static_cast<float>(Constants::TILE_SIZE);
    const Team localTeam = (m_players[m_localSlot]) ? m_players[m_localSlot]->getTeam() : Team::Player1;
    sf::Vector2f cameraTarget(-1.f, -1.f);

    for (const auto& e : data.entities) {
        // Start positions are editor-only markers: record camera target, don't spawn
        if (e.type == EntityType::StartPosition) {
            if (e.team == localTeam) {
                sf::Vector2i tileSize = ENTITY_DATA.getBuildingTileSize(e.type);
                cameraTarget = sf::Vector2f(
                    (e.tileX + tileSize.x * 0.5f) * TS,
                    (e.tileY + tileSize.y * 0.5f) * TS);
            }
            continue;
        }

        sf::Vector2i tileSize = ENTITY_DATA.getBuildingTileSize(e.type);
        // World-space centre of the footprint
        float wx = (e.tileX + tileSize.x * 0.5f) * TS;
        float wy = (e.tileY + tileSize.y * 0.5f) * TS;
        sf::Vector2f pos(wx, wy);

        const EntityDef* def = ENTITY_DATA.get(e.type);
        if (!def) continue;

        if (def->isResource()) {
            // Mineral patches / geysers
            ResourceNodePtr node = ResourceManager::createResourceNode(e.type, pos);
            if (node) addEntity(node);
        } else if (def->isBuilding()) {
            spawnBuilding(e.type, e.team, pos, /*startComplete=*/true);
        } else if (def->isUnit()) {
            spawnUnit(e.type, e.team, pos);
        }
    }

    // If the map defined a starting position for the local player, centre the camera there
    if (cameraTarget.x >= 0.f && m_input)
        m_input->centerCameraAt(cameraTarget);
}

void Game::spawnUnit(EntityType type, Team team, sf::Vector2f position) {
    // Use the actual entity pixel size so findSpawnPosition keeps units apart
    // by their real collision radius rather than a hardcoded constant.
    sf::Vector2f entitySize = ENTITY_DATA.getSize(type);
    float unitRadius = std::max(entitySize.x, entitySize.y) / 2.0f;
    
    // Find a free position to spawn the unit
    sf::Vector2f spawnPos = findSpawnPosition(position, unitRadius);
    
    UnitPtr unit;
    
    switch (type) {
        case EntityType::Worker:
            unit = ResourceManager::createWorker(team, spawnPos);
            break;
        case EntityType::Soldier:
            unit = ResourceManager::createSoldier(team, spawnPos);
            break;
        case EntityType::Brute:
            unit = ResourceManager::createBrute(team, spawnPos);
            break;
        case EntityType::LightTank:
            unit = ResourceManager::createLightTank(team, spawnPos);
            break;
        default:
            return;
    }
    
    if (unit) {
        setupUnit(unit);
        unit->setIsLocalTeam(m_players[m_localSlot] && team == m_players[m_localSlot]->getTeam());
        
        // Set home base for workers
        if (type == EntityType::Worker) {
            if (Player* playerPtr = getPlayerByTeam(team)) {
                for (auto& building : playerPtr->getBuildings()) {
                    if (building->getType() == EntityType::Base) {
                        if (auto* w = unit->asWorker()) {
                            setupWorker(w, building);
                        }
                        break;
                    }
                }
            }
        }
        
        if (Player* p = getPlayerByTeam(team)) {
            p->addUnit(unit);
        }
        
        addEntity(unit);
    }
}

EntityPtr Game::spawnBuilding(EntityType type, Team team, sf::Vector2f position, bool startComplete) {
    BuildingPtr building;
    
    switch (type) {
        case EntityType::Base:
            building = ResourceManager::createBase(team, position);
            break;
        case EntityType::Barracks:
            building = ResourceManager::createBarracks(team, position);
            break;
        case EntityType::Refinery:
            building = ResourceManager::createRefinery(team, position);
            break;
        case EntityType::Factory:
            building = ResourceManager::createFactory(team, position);
            break;
        default:
            return nullptr;
    }
    
    if (building) {
        // Set construction state based on parameter
        if (!startComplete) {
            building->startConstruction();
        }
        
        building->onUnitProduced = [this, team](EntityType unitType, sf::Vector2f pos) {
            spawnUnit(unitType, team, pos);
        };
        building->onProductionCancelled = [this, team](EntityType unitType) {
            if (Player* p = getPlayerByTeam(team)) p->addResources(ResourceManager::getMineralCost(unitType), 0);
        };
        
        // Place on map
        sf::Vector2i buildingSize = ResourceManager::getBuildingSize(type);
        sf::Vector2i tile = MathUtil::buildingTileOrigin(position, buildingSize, Constants::TILE_SIZE);
        m_map.placeBuilding(tile.x, tile.y, buildingSize.x, buildingSize.y, building);
        
        if (Player* p = getPlayerByTeam(team)) {
            p->addBuilding(building);
        }
        building->setIsLocalTeam(m_players[m_localSlot] && team == m_players[m_localSlot]->getTeam());
        
        addEntity(building);
        return building;
    }
    return nullptr;
}

void Game::spawnProjectile(EntityPtr source, EntityPtr target, int damage, float speed) {
    if (!source || !target || !target->isAlive()) return;
    auto projectile = std::make_shared<Projectile>(source, target, damage, speed);
    addEntity(projectile);
}

void Game::depositResources(Team team, int amount) {
    if (Player* p = getPlayerByTeam(team)) p->addResources(amount, 0);
}

void Game::issueMoveCommand(sf::Vector2f target) {
    getActions().move(getPlayer().getSelection(), target);
}

void Game::issueFollowCommand(EntityPtr target) {
    getActions().follow(getPlayer().getSelection(), target);
}

void Game::issueAttackMoveCommand(sf::Vector2f target) {
    getActions().attackMove(getPlayer().getSelection(), target);
}

void Game::issueAttackCommand(EntityPtr target) {
    getActions().attack(getPlayer().getSelection(), target);
}

void Game::issueGatherCommand(EntityPtr resource) {
    getActions().gather(getPlayer().getSelection(), resource);
}

void Game::issueBuildCommand(EntityType buildingType, sf::Vector2f position) {
    // Use the first selected worker so the player's chosen unit does the building,
    // rather than auto-picking the nearest idle worker.
    Worker* selectedWorker = nullptr;
    for (const auto& entity : getPlayer().getSelection()) {
        if (entity && entity->isAlive()) {
            if (Worker* w = entity->asWorker()) {
                selectedWorker = w;
                break;
            }
        }
    }
    getActions().constructBuilding(buildingType, position, selectedWorker);
}

void Game::issueContinueBuildCommand(EntityPtr building) {
    getActions().continueConstruction(building, getPlayer().getSelection());
}

void Game::cancelBuildingConstruction(EntityPtr building) {
    getActions().cancelConstruction(building);
}
