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
#include "IdGenerator.h"
#include <algorithm>
#include <array>
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

    // Debug console gets first refusal; if it consumes the event, stop here.
    if (m_debugConsole && m_debugConsole->handleEvent(event)) {
        return;
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

    // Slide any unit that is physically inside a building outward each frame.
    // This handles workers that get trapped when a building is placed on them,
    // as well as any other overlap that occurs during normal gameplay.
    pushUnitsOutOfBuildings(deltaTime);

    // Update all player controllers (AI ticks, network polling, etc.)
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (m_controllers[i])
            m_controllers[i]->update(deltaTime);
    }
    
    // Update visual effects
    EFFECTS.update(deltaTime);
    
    // Cleanup dead entities
    cleanupDeadEntities();

    // Update fog of war for the local human player every frame.
    // This recomputes which tiles are visible based on current unit/building
    // positions and rebuilds the fog overlay texture used by the Renderer.
    getPlayer().updateFog(m_map, m_allEntities);

    // Check victory/defeat
    checkVictoryConditions();
}

void Game::render() {
    m_renderer->setCamera(m_input->getCamera());
    m_renderer->render(*this);  // ends with UI view active on the window

    if (m_debugConsole) {
        // World-space overlays (waypoints + IDs)
        m_window.setView(m_input->getCamera());
        m_debugConsole->renderWaypoints(m_window);
        m_debugConsole->renderIds(m_window);

        // Console input bar / log is drawn in screen space
        sf::Vector2u winSize = m_window.getSize();
        sf::View uiView(sf::FloatRect(
            sf::Vector2f(0.f, 0.f),
            sf::Vector2f(static_cast<float>(winSize.x), static_cast<float>(winSize.y))));
        m_window.setView(uiView);
        m_debugConsole->render(m_window);
    }
}

void Game::initialize() {
    // Reset entity ID counter so each game starts IDs from 1.
    IdGenerator::instance().reset();

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

    // Initialize statistics for each active player slot
    for (int i = 0; i < playerCount; ++i) {
        m_statistics.setPlayerActive(i, teamFromIndex(i));
    }

    // Create per-player action dispatchers (must be before controllers so AI can reference them)
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (!m_players[i]) continue;
        m_actions[i] = std::make_unique<PlayerActions>(*m_players[i], *this);
        m_actions[i]->setLocalPlayer(i == m_localSlot);
    }

    // Create input handler, renderer, and debug console
    m_input        = std::make_unique<InputHandler>(m_window, *this);
    m_renderer     = std::make_unique<Renderer>(m_window);
    m_debugConsole = std::make_unique<DebugConsole>(m_window, *this);

    // Assign controllers: human for the local slot, AI for all other occupied slots
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (!m_players[i]) continue;
        if (i == m_localSlot) {
            m_controllers[i] = std::make_unique<HumanController>();
        } else {
            auto aiController = std::make_unique<AIPlayerController>(*m_players[i], *this);
            aiController->loadScripts("aiscripts");
            m_controllers[i] = std::move(aiController);
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
    
    // Record statistics for dying entities before removal
    for (const auto& entity : m_allEntities) {
        if (!entity || !entity->isReadyForRemoval()) continue;
        
        Team killerTeam = entity->getLastAttackerTeam();
        
        // Check if it's a unit
        if (entity->asUnit()) {
            m_statistics.recordUnitDestroyed(entity->getTeam(), killerTeam);
        }
        // Check if it's a building
        else if (entity->asBuilding()) {
            m_statistics.recordBuildingDestroyed(entity->getTeam(), killerTeam);
        }
    }
    
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

EntityPtr Game::findPriorityEnemy(sf::Vector2f pos, float radius, Team excludeTeam) {
    // Priority: combat units > workers > buildings
    // Within each category, pick the nearest
    EntityPtr bestCombatUnit = nullptr;
    EntityPtr bestWorker = nullptr;
    EntityPtr bestBuilding = nullptr;
    float bestCombatDist = radius;
    float bestWorkerDist = radius;
    float bestBuildingDist = radius;
    
    for (auto& entity : m_allEntities) {
        if (!entity || !entity->isAlive()) continue;
        if (entity->getTeam() == excludeTeam || entity->getTeam() == Team::Neutral) continue;
        
        float dist = MathUtil::distance(entity->getPosition(), pos);
        if (dist >= radius) continue;
        
        if (entity->asBuilding()) {
            if (dist < bestBuildingDist) {
                bestBuildingDist = dist;
                bestBuilding = entity;
            }
        } else if (entity->asUnit()) {
            // Distinguish workers from combat units
            if (entity->getType() == EntityType::Worker) {
                if (dist < bestWorkerDist) {
                    bestWorkerDist = dist;
                    bestWorker = entity;
                }
            } else {
                if (dist < bestCombatDist) {
                    bestCombatDist = dist;
                    bestCombatUnit = entity;
                }
            }
        }
    }
    
    // Return highest priority target found
    if (bestCombatUnit) return bestCombatUnit;
    if (bestWorker) return bestWorker;
    return bestBuilding;
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

sf::Vector2f Game::findFreePosition(sf::Vector2f pos, float radius, float maxSearchRadius, Entity* excludeSelf) {
    // Try the original position first
    if (!checkPositionBlocked(pos, radius, excludeSelf)) {
        return pos;
    }
    
    // Search in expanding circles
    float stepRadius = radius * 2.0f;
    int maxRings = static_cast<int>(maxSearchRadius / stepRadius) + 1;
    
    for (int ring = 1; ring <= maxRings; ++ring) {
        float ringRadius = stepRadius * ring;
        int points = 8 * ring;  // More points in outer rings
        
        for (int i = 0; i < points; ++i) {
            float angle = (2.0f * 3.14159f * i) / points;
            sf::Vector2f testPos = pos + sf::Vector2f(
                std::cos(angle) * ringRadius,
                std::sin(angle) * ringRadius
            );
            
            if (!checkPositionBlocked(testPos, radius, excludeSelf)) {
                return testPos;
            }
        }
    }
    
    // Fallback to original if no free spot found
    return pos;
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
        
        // Record unit creation in statistics
        m_statistics.recordUnitCreated(team);
        
        addEntity(unit);
    }
}

void Game::spawnUnitFromBuilding(EntityType type, Team team, Building* sourceBuilding) {
    if (!sourceBuilding) return;

    // Use the actual entity pixel size so findSpawnPosition keeps units apart
    sf::Vector2f entitySize = ENTITY_DATA.getSize(type);
    float unitRadius = std::max(entitySize.x, entitySize.y) / 2.0f;

    // Compute a spawn origin just outside the source building.
    // Try all four cardinal edges (right, bottom, left, top), preferring the one
    // that best aligns with the rally point direction. Pick the first edge whose
    // midpoint is not blocked, then hand off to findSpawnPosition for fine placement.
    sf::FloatRect bounds    = sourceBuilding->getBounds();
    sf::Vector2f  buildingPos = sourceBuilding->getPosition();
    const float   pad       = unitRadius + 4.f;

    sf::Vector2f rallyDir = sourceBuilding->getRallyPoint() - buildingPos;
    {
        float rLen = std::sqrt(rallyDir.x * rallyDir.x + rallyDir.y * rallyDir.y);
        if (rLen > 0.f) rallyDir /= rLen;
    }

    // Four candidate origins: Right, Bottom, Left, Top
    struct EdgeCandidate {
        sf::Vector2f pos;
        sf::Vector2f dir;   // outward normal
    };
    std::array<EdgeCandidate, 4> edges = {{
        { { bounds.position.x + bounds.size.x + pad, buildingPos.y }, {  1.f,  0.f } },
        { { buildingPos.x, bounds.position.y + bounds.size.y + pad }, {  0.f,  1.f } },
        { { bounds.position.x - pad,                buildingPos.y }, { -1.f,  0.f } },
        { { buildingPos.x, bounds.position.y - pad              }, {  0.f, -1.f } },
    }};

    // Sort edges: most aligned with rally direction first
    std::sort(edges.begin(), edges.end(), [&](const EdgeCandidate& a, const EdgeCandidate& b) {
        float da = a.dir.x * rallyDir.x + a.dir.y * rallyDir.y;
        float db = b.dir.x * rallyDir.x + b.dir.y * rallyDir.y;
        return da > db;
    });

    // Pick the first unblocked edge as the search origin; fall back to the best-aligned one
    sf::Vector2f spawnPos = edges[0].pos;
    for (const auto& edge : edges) {
        if (!checkPositionBlocked(edge.pos, unitRadius, nullptr)) {
            spawnPos = edge.pos;
            break;
        }
    }

    spawnPos = findSpawnPosition(spawnPos, unitRadius);
    
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
        
        // Record unit creation in statistics
        m_statistics.recordUnitCreated(team);
        
        addEntity(unit);
        
        // Issue rally point command
        EntityPtr rallyTarget = sourceBuilding->getRallyTarget();
        sf::Vector2f rallyPoint = sourceBuilding->getRallyPoint();
        
        if (rallyTarget && rallyTarget->isAlive()) {
            // Rally to entity
            if (type == EntityType::Worker) {
                // Workers gather from resources
                if (rallyTarget->getType() == EntityType::MineralPatch || 
                    rallyTarget->getType() == EntityType::GasGeyser) {
                    if (auto* worker = unit->asWorker()) {
                        worker->gather(rallyTarget);
                    }
                } else {
                    // Move to the target
                    unit->moveTo(rallyTarget->getPosition());
                }
            } else {
                // Combat units move to the target
                unit->moveTo(rallyTarget->getPosition());
            }
        } else {
            // Rally to position - attack-move for combat units, move for workers
            if (type == EntityType::Worker) {
                unit->moveTo(rallyPoint);
            } else {
                unit->attackMoveTo(rallyPoint);
            }
        }
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
            building = ResourceManager::createBuilding(EntityType::Factory, team, position);
            break;
        case EntityType::Turret:
            building = ResourceManager::createBuilding(EntityType::Turret, team, position);
            break;
        default:
            return nullptr;
    }
    
    if (building) {
        // Set construction state based on parameter
        if (!startComplete) {
            building->startConstruction();
        }
        
        building->onUnitProduced = [this, team](EntityType unitType, Building* sourceBuilding) {
            spawnUnitFromBuilding(unitType, team, sourceBuilding);
        };
        building->onProductionCancelled = [this, team](EntityType unitType) {
            if (Player* p = getPlayerByTeam(team)) p->addResources(ResourceManager::getMineralCost(unitType), 0);
        };

        // Give combat buildings (Turret, etc.) access to spatial queries.
        building->setupGameContext(this);
        
        // Place on map
        sf::Vector2i buildingSize = ResourceManager::getBuildingSize(type);
        sf::Vector2i tile = MathUtil::buildingTileOrigin(position, buildingSize, Constants::TILE_SIZE);
        m_map.placeBuilding(tile.x, tile.y, buildingSize.x, buildingSize.y, building);
        
        if (Player* p = getPlayerByTeam(team)) {
            p->addBuilding(building);
        }
        building->setIsLocalTeam(m_players[m_localSlot] && team == m_players[m_localSlot]->getTeam());
        
        // Record building creation in statistics (only for player-constructed buildings, not map setup)
        if (!startComplete) {
            m_statistics.recordBuildingCreated(team);
        }
        
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

void Game::issueMoveCommand(sf::Vector2f target, bool append) {
    getActions().move(getPlayer().getSelection(), target, append);
}

void Game::issueFollowCommand(EntityPtr target, bool append) {
    getActions().follow(getPlayer().getSelection(), target, append);
}

void Game::issueAttackMoveCommand(sf::Vector2f target, bool append) {
    getActions().attackMove(getPlayer().getSelection(), target, append);
}

void Game::issueAttackCommand(EntityPtr target, bool append) {
    getActions().attack(getPlayer().getSelection(), target, append);
}

void Game::issueGatherCommand(EntityPtr resource, bool append) {
    getActions().gather(getPlayer().getSelection(), resource, append);
}

void Game::issueReturnCargoCommand() {
    Player& player = getPlayer();
    for (const auto& entity : player.getSelection()) {
        if (!entity || !entity->isAlive()) continue;
        Worker* w = entity->asWorker();
        if (w && w->getCarriedResources() > 0) {
            w->returnResources();
        }
    }
}

void Game::issueBuildCommand(EntityType buildingType, sf::Vector2f position, bool append) {
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
    getActions().constructBuilding(buildingType, position, selectedWorker, append);
}

void Game::issueContinueBuildCommand(EntityPtr building, bool append) {
    getActions().continueConstruction(building, getPlayer().getSelection(), append);
}

void Game::cancelBuildingConstruction(EntityPtr building) {
    getActions().cancelConstruction(building);
}

void Game::pushUnitsOutOfBuildings(float deltaTime) {
    // Speed at which trapped units are pushed clear (pixels per second).
    // Fast enough to escape within a second but slow enough to look physical.
    constexpr float PUSH_SPEED = 150.0f;

    for (auto& entity : m_allEntities) {
        if (!entity || !entity->isAlive()) continue;
        Unit* unit = entity->asUnit();
        if (!unit || !unit->isCollidable()) continue;

        float     radius = unit->getCollisionRadius();
        sf::Vector2f pos = unit->getPosition();

        for (auto& other : m_allEntities) {
            if (!other || !other->isAlive()) continue;
            if (!other->asBuilding()) continue;

            sf::FloatRect bounds = other->getBounds();

            // Expand bounds by the unit's collision radius so we test whether
            // the unit's *body* (not just its centre) overlaps the building.
            sf::FloatRect expanded = bounds;
            expanded.position.x -= radius;
            expanded.position.y -= radius;
            expanded.size.x     += radius * 2.0f;
            expanded.size.y     += radius * 2.0f;

            if (!expanded.contains(pos)) continue;

            // AABB minimum-penetration depenetration:
            // compute how far the unit centre is from each of the four expanded
            // faces, then push along the axis with the smallest penetration.
            float dLeft   = pos.x - expanded.position.x;
            float dRight  = (expanded.position.x + expanded.size.x) - pos.x;
            float dTop    = pos.y - expanded.position.y;
            float dBottom = (expanded.position.y + expanded.size.y) - pos.y;

            float minH = std::min(dLeft, dRight);
            float minV = std::min(dTop,  dBottom);

            sf::Vector2f pushDir;
            if (minH <= minV) {
                pushDir = (dLeft <= dRight) ? sf::Vector2f(-1.f, 0.f)
                                            : sf::Vector2f( 1.f, 0.f);
            } else {
                pushDir = (dTop  <= dBottom) ? sf::Vector2f(0.f, -1.f)
                                             : sf::Vector2f(0.f,  1.f);
            }

            pos += pushDir * (PUSH_SPEED * deltaTime);
        }

        unit->setPosition(pos);
    }
}

void Game::setRallyPoint(sf::Vector2f position, EntityPtr target) {
    Player& player = getPlayer();
    bool anySet = false;
    
    for (const auto& entity : player.getSelection()) {
        if (auto* building = entity->asBuilding()) {
            // Check if building can produce units
            if (auto* bldgDef = ENTITY_DATA.getBuildingDef(entity->getType())) {
                if (bldgDef->canProduce) {
                    if (target) {
                        building->setRallyTarget(target);
                    } else {
                        building->setRallyPoint(position);
                    }
                    anySet = true;
                }
            }
        }
    }
    
    // Spawn visual effect at rally point
    if (anySet) {
        sf::Vector2f effectPos = target ? target->getPosition() : position;
        EFFECTS.spawnMoveEffect(effectPos, 1.0f);
    }
}
