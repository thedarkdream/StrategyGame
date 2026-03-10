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
#include <cstdlib>

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
    static const Team TEAM_ORDER[MAX_PLAYERS] = {
        Team::Player1, Team::Player2, Team::Player3, Team::Player4
    };
    for (int i = 0; i < playerCount; ++i)
        m_players[i] = std::make_unique<Player>(TEAM_ORDER[i], startingRes);

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

    // Setup starting units – either from the loaded map or the default procedural setup
    if (mapData) {
        setupFromMapData(*mapData);
    } else if (!m_mapFile.empty() && m_mapFile != "default") {
        // Map was specified but failed to load
        std::cerr << "Game: failed to load map '" << m_mapFile
                  << "', falling back to default setup.\n";
        setupStartingUnits();
    } else {
        setupStartingUnits();
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

void Game::setupStartingUnits() {
    // Player starting position - snap to tile grid
    // Base is 3x3 tiles, place at tile (5, 5) - center position
    int playerTileX = 5;
    int playerTileY = 5;
    sf::Vector2i baseSize = ResourceManager::getBuildingSize(EntityType::Base);
    sf::Vector2f playerStart(
        playerTileX * Constants::TILE_SIZE + baseSize.x * Constants::TILE_SIZE / 2.0f,
        playerTileY * Constants::TILE_SIZE + baseSize.y * Constants::TILE_SIZE / 2.0f
    );
    
    // Enemy starting position - snap to tile grid (bottom-right area)
    int enemyTileX = Constants::MAP_WIDTH - 5 - baseSize.x;
    int enemyTileY = Constants::MAP_HEIGHT - 5 - baseSize.y;
    sf::Vector2f enemyStart(
        enemyTileX * Constants::TILE_SIZE + baseSize.x * Constants::TILE_SIZE / 2.0f,
        enemyTileY * Constants::TILE_SIZE + baseSize.y * Constants::TILE_SIZE / 2.0f
    );
    
    // Create player's base
    auto playerBase = ResourceManager::createBase(Team::Player1, playerStart);
    playerBase->onUnitProduced = [this](EntityType type, sf::Vector2f pos) {
        spawnUnit(type, Team::Player1, pos);
    };
    playerBase->onProductionCancelled = [this](EntityType type) {
        if (Player* p = getPlayerByTeam(Team::Player1)) p->addResources(ResourceManager::getMineralCost(type), 0);
    };
    m_players[0]->addBuilding(playerBase);
    addEntity(playerBase);
    
    // Mark player base tiles on map
    m_map.placeBuilding(playerTileX, playerTileY, baseSize.x, baseSize.y, playerBase);
    
    // Create player's starting workers
    for (int i = 0; i < 4; ++i) {
        sf::Vector2f workerPos = playerStart + sf::Vector2f(60.0f + i * 30.0f, 70.0f);
        auto worker = ResourceManager::createWorker(Team::Player1, workerPos);
        setupUnit(worker);
        if (auto* w = dynamic_cast<Worker*>(worker.get())) {
            setupWorker(w, playerBase, Team::Player1);
        }
        m_players[0]->addUnit(worker);
        addEntity(worker);
    }

        // Create player's starting army
    for (int i = 0; i < 8; ++i) {
        sf::Vector2f soldierPos = playerStart + sf::Vector2f(60.0f + i * 30.0f, 100.0f);
        auto soldier = ResourceManager::createSoldier(Team::Player1, soldierPos);

        setupUnit(soldier);

        m_players[0]->addUnit(soldier);
        addEntity(soldier);
    }

    // Create enemy's base
    auto enemyBase = ResourceManager::createBase(Team::Player2, enemyStart);
    enemyBase->onUnitProduced = [this](EntityType type, sf::Vector2f pos) {
        spawnUnit(type, Team::Player2, pos);
    };
    enemyBase->onProductionCancelled = [this](EntityType type) {
        if (Player* p = getPlayerByTeam(Team::Player2)) p->addResources(ResourceManager::getMineralCost(type), 0);
    };
    m_players[1]->addBuilding(enemyBase);
    addEntity(enemyBase);
    
    // Mark enemy base tiles on map
    m_map.placeBuilding(enemyTileX, enemyTileY, baseSize.x, baseSize.y, enemyBase);
    
    // Create enemy's starting workers
    for (int i = 0; i < 4; ++i) {
        sf::Vector2f workerPos = enemyStart + sf::Vector2f(-60.0f - i * 30.0f, -70.0f);
        auto worker = ResourceManager::createWorker(Team::Player2, workerPos);
        setupUnit(worker);
        if (auto* w = dynamic_cast<Worker*>(worker.get())) {
            setupWorker(w, enemyBase, Team::Player2);
        }
        m_players[1]->addUnit(worker);
        addEntity(worker);
    }
    
    // Create mineral patches near player
    std::vector<sf::Vector2f> mineralPositions;
    for (int i = 0; i < 6; ++i) {
        sf::Vector2f mineralPos = playerStart + sf::Vector2f(150.0f + i * 50.0f, -100.0f);
        int variant = (std::rand() % 3) + 1;  // Random variant 1-3
        auto mineral = ResourceManager::createMineralPatch(mineralPos, 1500, variant);
        mineralPositions.push_back(mineralPos);
        addEntity(mineral);
    }
    
    // Create mineral patches near enemy
    for (int i = 0; i < 6; ++i) {
        sf::Vector2f mineralPos = enemyStart + sf::Vector2f(-150.0f - i * 50.0f, 100.0f);
        int variant = (std::rand() % 3) + 1;  // Random variant 1-3
        auto mineral = ResourceManager::createMineralPatch(mineralPos, 1500, variant);
        mineralPositions.push_back(mineralPos);
        addEntity(mineral);
    }
    
    m_map.addMineralPatches(mineralPositions);
    flushPendingEntities();  // Ensure all initialization entities are in m_allEntities
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
        for (int i = 0; i < MAX_PLAYERS; ++i) {
            if (i != m_localSlot && m_players[i] && m_players[i]->isDefeated()) {
                m_state = GameState::Victory;
                break;
            }
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
        if (auto* resourceNode = dynamic_cast<ResourceNode*>(entity.get())) {
            if (resourceNode->isBeingMined()) return false;
        }
        return true;
    });
}

void Game::setupUnit(UnitPtr& unit) {
    unit->setMap(&m_map);
    unit->findNearestEnemy = [this](sf::Vector2f pos, float radius, Team excludeTeam) {
        return this->findNearestEnemy(pos, radius, excludeTeam);
    };
    unit->checkPositionBlocked = [this](sf::Vector2f pos, float radius, Entity* excludeSelf) {
        return this->checkPositionBlocked(pos, radius, excludeSelf);
    };
    unit->getNearbyUnitsRVO = [this](sf::Vector2f pos, float radius, Unit* excludeSelf) {
        return this->getNearbyUnitsRVO(pos, radius, excludeSelf);
    };
    unit->spawnProjectile = [this](EntityPtr source, EntityPtr target, int damage, float speed) {
        this->spawnProjectile(source, target, damage, speed);
    };
}

void Game::setupWorker(Worker* worker, EntityPtr homeBase, Team team) {
    worker->setHomeBase(homeBase);
    worker->findNearestResource = [this](sf::Vector2f pos, float radius) {
        return this->findNearestResource(pos, radius);
    };
    worker->findNearestAvailableResource = [this](sf::Vector2f pos, float radius, EntityPtr exclude) {
        return this->findNearestAvailableResource(pos, radius, exclude);
    };
    worker->onResourceDeposit = [this, team](int amount) {
        if (Player* p = getPlayerByTeam(team)) p->addResources(amount, 0);
    };
}

bool Game::checkPositionBlocked(sf::Vector2f pos, float radius, Entity* excludeSelf) {
    for (auto& entity : m_allEntities) {
        if (!entity || !entity->isAlive()) continue;
        if (entity.get() == excludeSelf) continue;
        
        // Check if it's a unit and collidable
        if (auto* unit = dynamic_cast<Unit*>(entity.get())) {
            if (!unit->isCollidable()) continue;
            
            float otherRadius = unit->getCollisionRadius();
            float dist = MathUtil::distance(entity->getPosition(), pos);
            float minDist = radius + otherRadius;
            
            if (dist < minDist) {
                return true;
            }
        }
        // Check buildings
        else if (entity->getType() == EntityType::Base || 
                 entity->getType() == EntityType::Barracks ||
                 entity->getType() == EntityType::Refinery) {
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
    }
    
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
        
        if (auto* unit = dynamic_cast<Unit*>(entity.get())) {
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
    switch (t) {
        case Team::Player1: return m_players[0].get();
        case Team::Player2: return m_players[1].get();
        case Team::Player3: return m_players[2].get();
        case Team::Player4: return m_players[3].get();
        default:            return nullptr; // Neutral
    }
}

void Game::setupFromMapData(const MapData& data) {
    // Re-initialise the map to match the saved dimensions
    m_map = Map(data.width, data.height, /*generateRandom=*/false);

    // Apply saved tile types
    for (const auto& t : data.tiles)
        m_map.setTileType(t.x, t.y, t.type);

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
    // Determine unit size for spawn position search
    float unitRadius = (type == EntityType::Worker) ? 10.0f : 12.0f;
    
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
                        if (auto* w = dynamic_cast<Worker*>(unit.get())) {
                            setupWorker(w, building, team);
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
        int tileX = static_cast<int>((position.x - buildingSize.x * Constants::TILE_SIZE / 2.0f) / Constants::TILE_SIZE);
        int tileY = static_cast<int>((position.y - buildingSize.y * Constants::TILE_SIZE / 2.0f) / Constants::TILE_SIZE);
        m_map.placeBuilding(tileX, tileY, buildingSize.x, buildingSize.y, building);
        
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

void Game::issueCommand(const std::vector<EntityPtr>& entities, Command command) {
    for (auto& entity : entities) {
        if (auto* unit = dynamic_cast<Unit*>(entity.get())) {
            switch (command.type) {
                case Command::Type::Move:
                    unit->moveTo(command.targetPosition);
                    break;
                case Command::Type::Attack:
                    if (command.targetEntity) {
                        unit->attack(command.targetEntity);
                    }
                    break;
                case Command::Type::Gather:
                    if (command.targetEntity) {
                        if (auto* worker = dynamic_cast<Worker*>(unit)) {
                            worker->gather(command.targetEntity);
                        }
                    }
                    break;
                case Command::Type::Stop:
                    unit->stop();
                    break;
                default:
                    break;
            }
        }
    }
}

void Game::issueMoveCommand(sf::Vector2f target) {
    auto& selection = getPlayer().getSelection();
    for (auto& entity : selection) {
        if (auto* unit = dynamic_cast<Unit*>(entity.get())) {
            unit->moveTo(target);
        }
    }
}

void Game::issueFollowCommand(EntityPtr target) {
    if (target) {
        target->startHighlight();
    }
    auto& selection = getPlayer().getSelection();
    for (auto& entity : selection) {
        if (auto* unit = dynamic_cast<Unit*>(entity.get())) {
            // Don't follow yourself
            if (entity != target) {
                unit->follow(target);
            }
        }
    }
}

void Game::issueAttackMoveCommand(sf::Vector2f target) {
    auto& selection = getPlayer().getSelection();
    for (auto& entity : selection) {
        if (auto* unit = dynamic_cast<Unit*>(entity.get())) {
            unit->attackMoveTo(target);
        }
    }
}

void Game::issueAttackCommand(EntityPtr target) {
    if (target) {
        target->startHighlight();
    }
    auto& selection = getPlayer().getSelection();
    for (auto& entity : selection) {
        if (auto* unit = dynamic_cast<Unit*>(entity.get())) {
            unit->attack(target);
        }
    }
}

void Game::issueGatherCommand(EntityPtr resource) {
    if (resource) {
        resource->startHighlight();
    }
    auto& selection = getPlayer().getSelection();
    for (auto& entity : selection) {
        if (auto* worker = dynamic_cast<Worker*>(entity.get())) {
            worker->gather(resource);
        }
    }
}

void Game::issueBuildCommand(EntityType buildingType, sf::Vector2f position) {
    // Check if player can afford it
    int cost = ResourceManager::getMineralCost(buildingType);
    if (!getPlayer().canAfford(cost, 0)) {
        return;
    }
    
    // Check building dependencies
    const auto& actions = ENTITY_DATA.getActions(EntityType::Worker);
    for (const auto& action : actions) {
        if (action.type == ActionDef::Type::Build && action.producesType == buildingType) {
            if (action.requires != EntityType::None && 
                !getPlayer().hasCompletedBuilding(action.requires)) {
                return;  // Dependency not met
            }
            break;
        }
    }
    
    // Check if location is valid
    sf::Vector2i buildingTileSize = ResourceManager::getBuildingSize(buildingType);
    int tileX = static_cast<int>(position.x / Constants::TILE_SIZE);
    int tileY = static_cast<int>(position.y / Constants::TILE_SIZE);
    
    if (!m_map.canPlaceBuilding(tileX, tileY, buildingTileSize.x, buildingTileSize.y)) {
        return;
    }
    
    // Find a selected worker to build
    Worker* selectedWorker = nullptr;
    for (const auto& entity : getPlayer().getSelection()) {
        if (entity && entity->isAlive() && entity->getType() == EntityType::Worker) {
            selectedWorker = dynamic_cast<Worker*>(entity.get());
            if (selectedWorker) break;
        }
    }
    
    // If no selected worker, find the nearest idle worker
    if (!selectedWorker) {
        float nearestDist = std::numeric_limits<float>::max();
        for (const auto& entity : m_allEntities) {
            if (entity && entity->isAlive() && 
                entity->getTeam() == getPlayer().getTeam() &&
                entity->getType() == EntityType::Worker) {
                Worker* worker = dynamic_cast<Worker*>(entity.get());
                if (worker && !worker->isBuilding() && !worker->isGathering()) {
                    float dist = MathUtil::distance(worker->getPosition(), position);
                    if (dist < nearestDist) {
                        nearestDist = dist;
                        selectedWorker = worker;
                    }
                }
            }
        }
    }
    
    // No worker available to build
    if (!selectedWorker) {
        return;
    }
    
    // Spend resources and spawn incomplete building
    getPlayer().spendResources(cost, 0);
    
    // Calculate center position: top-left corner + half the pixel size
    sf::Vector2f pixelSize = ENTITY_DATA.getSize(buildingType);
    sf::Vector2f buildPos(
        tileX * Constants::TILE_SIZE + pixelSize.x / 2.0f,
        tileY * Constants::TILE_SIZE + pixelSize.y / 2.0f
    );
    
    // Spawn building (starts incomplete) and get it back directly
    EntityPtr newBuilding = spawnBuilding(buildingType, getPlayer().getTeam(), buildPos, false);
    
    if (newBuilding) {
        selectedWorker->buildAt(newBuilding);
    }
}

void Game::issueContinueBuildCommand(EntityPtr building) {
    if (!building) return;
    
    // Send selected workers to continue building
    for (const auto& entity : getPlayer().getSelection()) {
        if (entity && entity->isAlive() && entity->getType() == EntityType::Worker) {
            Worker* worker = dynamic_cast<Worker*>(entity.get());
            if (worker) {
                worker->buildAt(building);
            }
        }
    }
}

void Game::cancelBuildingConstruction(EntityPtr entity) {
    if (!entity) return;
    
    Building* building = dynamic_cast<Building*>(entity.get());
    if (!building || building->isConstructed()) return;
    
    // Release the builder worker
    building->releaseBuilder();
    
    // Free the tiles occupied by this building
    sf::Vector2i buildingSize = ResourceManager::getBuildingSize(building->getType());
    sf::Vector2f pos = building->getPosition();
    int tileX = static_cast<int>((pos.x - buildingSize.x * Constants::TILE_SIZE / 2.0f) / Constants::TILE_SIZE);
    int tileY = static_cast<int>((pos.y - buildingSize.y * Constants::TILE_SIZE / 2.0f) / Constants::TILE_SIZE);
    m_map.removeBuilding(tileX, tileY, buildingSize.x, buildingSize.y);
    
    // Refund a portion of the cost based on construction progress
    // For now, refund full cost since building didn't complete
    int mineralCost = ENTITY_DATA.getMineralCost(building->getType());
    int gasCost = ENTITY_DATA.getGasCost(building->getType());
    
    // Refund to the owning player
    Player* owner = getPlayerByTeam(building->getTeam());
    if (owner) {
        owner->addResources(mineralCost, gasCost);
    }
    
    // Clear selection (the building is being removed)
    getPlayer().clearSelection();
    
    // Remove the building from the game
    removeEntity(entity);
}
