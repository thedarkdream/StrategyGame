#include "Game.h"
#include "Unit.h"
#include "Worker.h"
#include "Building.h"
#include "ResourceNode.h"
#include "ResourceManager.h"
#include "EffectsManager.h"
#include "Constants.h"
#include "MathUtil.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

Game::Game()
    : m_window(sf::VideoMode(sf::Vector2u(Constants::WINDOW_WIDTH, Constants::WINDOW_HEIGHT)), 
               "Strategy Game", sf::Style::Default)  // Allow resize
{
    m_window.setFramerateLimit(Constants::FRAME_RATE);
    initialize();
}

void Game::run() {
    while (m_window.isOpen()) {
        m_deltaTime = m_clock.restart().asSeconds();
        
        processEvents();
        
        if (m_state == GameState::Playing) {
            update(m_deltaTime);
        }
        
        render();
    }
}

void Game::processEvents() {
    while (const auto event = m_window.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            m_window.close();
        }
        else if (const auto* resized = event->getIf<sf::Event::Resized>()) {
            // Update the game view to maintain proper scaling
            m_input->onWindowResize(resized->size);
        }
        
        m_input->handleEvent(*event);
    }
}

void Game::update(float deltaTime) {
    // Update input (camera movement)
    m_input->update(deltaTime);
    
    // Update players
    m_player->update(deltaTime);
    m_enemy->update(deltaTime);
    
    // Update AI
    m_ai->update(deltaTime);
    
    // Update visual effects
    EFFECTS.update(deltaTime);
    
    // Update entity highlights
    for (auto& entity : m_allEntities) {
        if (entity) {
            entity->updateHighlight(deltaTime);
        }
    }
    
    // Cleanup dead entities
    cleanupDeadEntities();
    
    // Check victory/defeat
    checkVictoryConditions();
}

void Game::render() {
    m_renderer->setCamera(m_input->getCamera());
    m_renderer->render(*this);
    m_window.display();
}

void Game::initialize() {
    // Create players
    Resources startingRes;
    startingRes.minerals = Constants::STARTING_MINERALS;
    startingRes.gas = Constants::STARTING_GAS;
    
    m_player = std::make_unique<Player>(Team::Player, startingRes);
    m_enemy = std::make_unique<Player>(Team::Enemy, startingRes);
    
    // Create input handler and renderer
    m_input = std::make_unique<InputHandler>(m_window, *this);
    m_renderer = std::make_unique<Renderer>(m_window);
    
    // Create AI controller for enemy
    m_ai = std::make_unique<AIController>(*m_enemy, *this);
    
    // Setup starting units and buildings
    setupStartingUnits();
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
    auto playerBase = ResourceManager::createBase(Team::Player, playerStart);
    playerBase->onUnitProduced = [this](EntityType type, sf::Vector2f pos) {
        spawnUnit(type, Team::Player, pos);
    };
    playerBase->onProductionCancelled = [this](EntityType type) {
        m_player->addResources(ResourceManager::getMineralCost(type), 0);
    };
    m_player->addBuilding(playerBase);
    addEntity(playerBase);
    
    // Mark player base tiles on map
    m_map.placeBuilding(playerTileX, playerTileY, baseSize.x, baseSize.y, playerBase);
    
    // Create player's starting workers
    for (int i = 0; i < 4; ++i) {
        sf::Vector2f workerPos = playerStart + sf::Vector2f(60.0f + i * 30.0f, 70.0f);
        auto worker = ResourceManager::createWorker(Team::Player, workerPos);
        setupUnit(worker);
        if (auto* w = dynamic_cast<Worker*>(worker.get())) {
            setupWorker(w, playerBase, Team::Player);
        }
        m_player->addUnit(worker);
        addEntity(worker);
    }

        // Create player's starting army
    for (int i = 0; i < 8; ++i) {
        sf::Vector2f soldierPos = playerStart + sf::Vector2f(60.0f + i * 30.0f, 100.0f);
        auto soldier = ResourceManager::createSoldier(Team::Player, soldierPos);

        setupUnit(soldier);

        m_player->addUnit(soldier);
        addEntity(soldier);
    }

    // Create enemy's base
    auto enemyBase = ResourceManager::createBase(Team::Enemy, enemyStart);
    enemyBase->onUnitProduced = [this](EntityType type, sf::Vector2f pos) {
        spawnUnit(type, Team::Enemy, pos);
    };
    enemyBase->onProductionCancelled = [this](EntityType type) {
        m_enemy->addResources(ResourceManager::getMineralCost(type), 0);
    };
    m_enemy->addBuilding(enemyBase);
    addEntity(enemyBase);
    
    // Mark enemy base tiles on map
    m_map.placeBuilding(enemyTileX, enemyTileY, baseSize.x, baseSize.y, enemyBase);
    
    // Create enemy's starting workers
    for (int i = 0; i < 4; ++i) {
        sf::Vector2f workerPos = enemyStart + sf::Vector2f(-60.0f - i * 30.0f, -70.0f);
        auto worker = ResourceManager::createWorker(Team::Enemy, workerPos);
        setupUnit(worker);
        if (auto* w = dynamic_cast<Worker*>(worker.get())) {
            setupWorker(w, enemyBase, Team::Enemy);
        }
        m_enemy->addUnit(worker);
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
}

void Game::cleanupDeadEntities() {
    m_player->cleanupDeadEntities();
    m_enemy->cleanupDeadEntities();
    
    // Remove entities that are ready for removal (dead and death animation finished)
    m_allEntities.erase(
        std::remove_if(m_allEntities.begin(), m_allEntities.end(),
            [](const EntityPtr& entity) { return !entity || entity->isReadyForRemoval(); }),
        m_allEntities.end()
    );
}

void Game::checkVictoryConditions() {
    if (m_player->isDefeated()) {
        m_state = GameState::Defeat;
    } else if (m_enemy->isDefeated()) {
        m_state = GameState::Victory;
    }
}

EntityPtr Game::getEntityAtPosition(sf::Vector2f position) {
    for (auto& entity : m_allEntities) {
        if (entity && entity->isAlive() && entity->getBounds().contains(position)) {
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
}

void Game::setupWorker(Worker* worker, EntityPtr homeBase, Team team) {
    worker->setHomeBase(homeBase);
    worker->findNearestResource = [this](sf::Vector2f pos, float radius) {
        return this->findNearestResource(pos, radius);
    };
    worker->findNearestAvailableResource = [this](sf::Vector2f pos, float radius, EntityPtr exclude) {
        return this->findNearestAvailableResource(pos, radius, exclude);
    };
    if (team == Team::Player) {
        worker->onResourceDeposit = [this](int amount) {
            m_player->addResources(amount, 0);
        };
    } else {
        worker->onResourceDeposit = [this](int amount) {
            m_enemy->addResources(amount, 0);
        };
    }
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

void Game::addEntity(EntityPtr entity) {
    m_allEntities.push_back(entity);
}

void Game::removeEntity(EntityPtr entity) {
    auto it = std::find(m_allEntities.begin(), m_allEntities.end(), entity);
    if (it != m_allEntities.end()) {
        m_allEntities.erase(it);
    }
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
        default:
            return;
    }
    
    if (unit) {
        setupUnit(unit);
        
        // Set home base for workers
        if (type == EntityType::Worker) {
            Player& player = (team == Team::Player) ? *m_player : *m_enemy;
            for (auto& building : player.getBuildings()) {
                if (building->getType() == EntityType::Base) {
                    if (auto* w = dynamic_cast<Worker*>(unit.get())) {
                        setupWorker(w, building, team);
                    }
                    break;
                }
            }
        }
        
        if (team == Team::Player) {
            m_player->addUnit(unit);
        } else {
            m_enemy->addUnit(unit);
        }
        
        addEntity(unit);
    }
}

void Game::spawnBuilding(EntityType type, Team team, sf::Vector2f position, bool startComplete) {
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
            return;
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
            Player& player = (team == Team::Player) ? *m_player : *m_enemy;
            player.addResources(ResourceManager::getMineralCost(unitType), 0);
        };
        
        // Place on map
        sf::Vector2i buildingSize = ResourceManager::getBuildingSize(type);
        int tileX = static_cast<int>((position.x - buildingSize.x * Constants::TILE_SIZE / 2.0f) / Constants::TILE_SIZE);
        int tileY = static_cast<int>((position.y - buildingSize.y * Constants::TILE_SIZE / 2.0f) / Constants::TILE_SIZE);
        m_map.placeBuilding(tileX, tileY, buildingSize.x, buildingSize.y, building);
        
        if (team == Team::Player) {
            m_player->addBuilding(building);
        } else {
            m_enemy->addBuilding(building);
        }
        
        addEntity(building);
    }
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
    auto& selection = m_player->getSelection();
    for (auto& entity : selection) {
        if (auto* unit = dynamic_cast<Unit*>(entity.get())) {
            unit->moveTo(target);
        }
    }
}

void Game::issueAttackMoveCommand(sf::Vector2f target) {
    auto& selection = m_player->getSelection();
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
    auto& selection = m_player->getSelection();
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
    auto& selection = m_player->getSelection();
    for (auto& entity : selection) {
        if (auto* worker = dynamic_cast<Worker*>(entity.get())) {
            worker->gather(resource);
        }
    }
}

void Game::issueBuildCommand(EntityType buildingType, sf::Vector2f position) {
    // Check if player can afford it
    int cost = ResourceManager::getMineralCost(buildingType);
    if (!m_player->canAfford(cost, 0)) {
        return;
    }
    
    // Check building dependencies
    const auto& actions = ENTITY_DATA.getActions(EntityType::Worker);
    for (const auto& action : actions) {
        if (action.type == ActionDef::Type::Build && action.producesType == buildingType) {
            if (action.requires != EntityType::None && 
                !m_player->hasCompletedBuilding(action.requires)) {
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
    for (const auto& entity : m_player->getSelection()) {
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
                entity->getTeam() == Team::Player &&
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
    m_player->spendResources(cost, 0);
    
    // Calculate center position: top-left corner + half the pixel size
    sf::Vector2f pixelSize = ENTITY_DATA.getSize(buildingType);
    sf::Vector2f buildPos(
        tileX * Constants::TILE_SIZE + pixelSize.x / 2.0f,
        tileY * Constants::TILE_SIZE + pixelSize.y / 2.0f
    );
    
    // Spawn building (starts incomplete)
    spawnBuilding(buildingType, Team::Player, buildPos, false);
    
    // Find the building we just spawned and send worker to build it
    EntityPtr newBuilding = nullptr;
    for (auto it = m_allEntities.rbegin(); it != m_allEntities.rend(); ++it) {
        if ((*it)->getPosition() == buildPos && (*it)->getType() == buildingType) {
            newBuilding = *it;
            break;
        }
    }
    
    if (newBuilding) {
        selectedWorker->buildAt(newBuilding);
    }
}

void Game::issueContinueBuildCommand(EntityPtr building) {
    if (!building) return;
    
    // Send selected workers to continue building
    for (const auto& entity : m_player->getSelection()) {
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
    Player* owner = (building->getTeam() == Team::Player) ? m_player.get() : m_enemy.get();
    if (owner) {
        owner->addResources(mineralCost, gasCost);
    }
    
    // Clear selection (the building is being removed)
    m_player->clearSelection();
    
    // Remove the building from the game
    removeEntity(entity);
}
