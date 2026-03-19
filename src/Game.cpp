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
    // Single pass: record statistics, free map tiles, and unlink each dying
    // entity from its owning Player's typed lists — all before the shared_ptr
    // is released by the erase below.  This keeps Player::m_units / m_buildings
    // in sync with m_allEntities without a separate cleanup call on Player.
    for (const auto& entity : m_allEntities) {
        if (!entity || !entity->isReadyForRemoval()) continue;

        Team killerTeam = entity->getLastAttackerTeam();
        Player* owner   = getPlayerByTeam(entity->getTeam());

        if (entity->asUnit()) {
            m_statistics.recordUnitDestroyed(entity->getTeam(), killerTeam);
            if (owner)
                owner->removeUnit(std::static_pointer_cast<Unit>(entity));
        } else if (entity->asBuilding()) {
            m_statistics.recordBuildingDestroyed(entity->getTeam(), killerTeam);
            // Free the map tiles so pathfinding and canPlaceBuilding see open land.
            sf::Vector2i bSize = ResourceManager::getBuildingSize(entity->getType());
            sf::Vector2i tile  = MathUtil::buildingTileOrigin(
                entity->getPosition(), bSize, Constants::TILE_SIZE);
            m_map.removeBuilding(tile.x, tile.y, bSize.x, bSize.y);
            if (owner)
                owner->removeBuilding(std::static_pointer_cast<Building>(entity));
        }
    }

    // Prune dead/dying units from each player's selection every frame so the
    // HUD reacts immediately when a unit's health hits zero (before the death
    // animation finishes and isReadyForRemoval becomes true).
    for (auto& p : m_players)
        if (p) p->cleanupSelection();

    // Erase from the master list (shared_ptr refcount drops to zero here for
    // entities with no other owners).
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
        return entity->getTeam() != excludeTeam
            && entity->getTeam() != Team::Neutral
            && entity->getType() != EntityType::Rocket;
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
        if (entity->getType() == EntityType::Rocket) continue;  // Rockets are not valid targets
        
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
    // Inset building/resource bounds slightly so a unit whose centre grazes the
    // corner pixel of two touching buildings isn't falsely blocked.
    constexpr float CORNER_INSET = 2.0f;

    auto insetContains = [&](const Entity* e) -> bool {
        sf::FloatRect b = e->getBounds();
        b.position.x += CORNER_INSET;
        b.position.y += CORNER_INSET;
        b.size.x     -= CORNER_INSET * 2.0f;
        b.size.y     -= CORNER_INSET * 2.0f;
        return b.contains(pos);
    };

    for (const EntityList* list : {&m_allEntities, &m_pendingEntities}) {
    for (auto& entity : *list) {
        if (!entity || !entity->isAlive()) continue;
        if (entity.get() == excludeSelf) continue;

        if (auto* unit = entity->asUnit()) {
            if (!unit->isCollidable()) continue;
            float dist = MathUtil::distance(entity->getPosition(), pos);
            if (dist < radius + unit->getCollisionRadius())
                return true;
        } else if (entity->asBuilding() || entity->asResourceNode()) {
            if (insetContains(entity.get()))
                return true;
        }
    }
    }
    return false;
}

sf::Vector2f Game::findFreePosition(sf::Vector2f pos, float radius, float maxSearchRadius, Entity* excludeSelf) {
    if (!checkPositionBlocked(pos, radius, excludeSelf))
        return pos;
    float stepRadius = radius * 2.0f;
    int maxRings = static_cast<int>(maxSearchRadius / stepRadius) + 1;
    return findNearestFreePosition(pos, radius, maxRings, excludeSelf);
}

sf::Vector2f Game::findSpawnPosition(sf::Vector2f origin, float unitRadius) {
    if (!checkPositionBlocked(origin, unitRadius, nullptr))
        return origin;
    return findNearestFreePosition(origin, unitRadius, 5, nullptr);
}

sf::Vector2f Game::findNearestFreePosition(sf::Vector2f pos, float radius, int maxRings, Entity* excludeSelf) {
    float stepRadius = radius * 2.0f;
    for (int ring = 1; ring <= maxRings; ++ring) {
        float ringRadius = stepRadius * ring;
        int points = 8 * ring;
        for (int i = 0; i < points; ++i) {
            float angle = (2.0f * MathUtil::PI * i) / points;
            sf::Vector2f testPos = pos + sf::Vector2f(
                std::cos(angle) * ringRadius,
                std::sin(angle) * ringRadius);
            if (!checkPositionBlocked(testPos, radius, excludeSelf))
                return testPos;
        }
    }
    return pos;  // fallback: return original if no free spot found
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
    // Remove from the live list (normal case: entity has already been flushed).
    auto it = std::find(m_allEntities.begin(), m_allEntities.end(), entity);
    if (it != m_allEntities.end()) {
        m_allEntities.erase(it);
    }
    // Also purge from the pending buffer in case removal is requested on the
    // same frame the entity was spawned (before the next flush).
    auto pit = std::find(m_pendingEntities.begin(), m_pendingEntities.end(), entity);
    if (pit != m_pendingEntities.end()) {
        m_pendingEntities.erase(pit);
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
    sf::Vector2f entitySize = ENTITY_DATA.getSize(type);
    float unitRadius = std::max(entitySize.x, entitySize.y) / 2.0f;
    sf::Vector2f spawnPos = findSpawnPosition(position, unitRadius);
    spawnAndSetupUnit(type, team, spawnPos, nullptr);
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

    UnitPtr unit = spawnAndSetupUnit(type, team, spawnPos, nullptr);
    if (!unit) return;

    // Issue rally point command
        EntityPtr rallyTarget = sourceBuilding->getRallyTarget();
        sf::Vector2f rallyPoint = sourceBuilding->getRallyPoint();

        // Assigns a concentric-ring formation slot around a rally position so
        // successive units don't all pile up on the exact same tile.
        // Counts allied units whose current position OR target is within
        // SEARCH_RADIUS of the rally point, then maps the next unit (index n)
        // to the appropriate ring/slot offset.  Ring 0 = center (n==0), ring k
        // holds k*6 slots at radius k*spacing — identical to buildFormationOffsets
        // in PlayerActions.cpp.
        auto computeRallySlot = [&](sf::Vector2f rp, float spacing) -> sf::Vector2f {
            constexpr float SEARCH_RADIUS = 200.0f;
            int n = 0;
            for (const auto& e : m_allEntities) {
                if (!e || !e->isAlive() || e->getTeam() != team) continue;
                const Unit* u = e->asUnit();
                if (!u) continue;
                bool nearPos    = MathUtil::distance(u->getPosition(),        rp) < SEARCH_RADIUS;
                bool nearTarget = MathUtil::distance(u->getTargetPosition(),   rp) < SEARCH_RADIUS;
                if (nearPos || nearTarget) ++n;
            }
            if (n == 0) return {0.f, 0.f};   // first unit gets the center slot
            int ring = 1, ringStart = 1;
            while (n >= ringStart + ring * 6) { ringStart += ring * 6; ++ring; }
            int slotInRing = n - ringStart;
            float angle = static_cast<float>(slotInRing) / static_cast<float>(ring * 6)
                          * 2.f * MathUtil::PI;
            return { ring * spacing * std::cos(angle), ring * spacing * std::sin(angle) };
        };
        // slot spacing = 2 * unitRadius + small gap, mirrors formationSpacing()
        const float slotSpacing = 2.0f * unitRadius + 8.0f;

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
            // Rally to position — spread units into formation slots to avoid
            // the entire queue piling up and shaking at one point.
            sf::Vector2f slot = computeRallySlot(rallyPoint, slotSpacing);
            if (type == EntityType::Worker) {
                unit->moveTo(rallyPoint + slot);
            } else {
                unit->attackMoveTo(rallyPoint + slot);
            }
        }
}

// ---------------------------------------------------------------------------
// Private helper — creates a unit, wires it up and adds it to the world.
// Returns the new unit (or nullptr for unknown types).
// ---------------------------------------------------------------------------
UnitPtr Game::spawnAndSetupUnit(EntityType type, Team team, sf::Vector2f pos,
                                 Building* /*sourceBuilding*/)
{
    UnitPtr unit = ResourceManager::createUnit(type, team, pos);
    if (!unit) return nullptr;

    setupUnit(unit);
    unit->setIsLocalTeam(m_players[m_localSlot] && team == m_players[m_localSlot]->getTeam());

    // Assign home base for workers
    if (type == EntityType::Worker) {
        if (Player* playerPtr = getPlayerByTeam(team)) {
            for (auto& building : playerPtr->getBuildings()) {
                if (building->getType() == EntityType::Base) {
                    if (auto* w = unit->asWorker())
                        setupWorker(w, building);
                    break;
                }
            }
        }
    }

    if (Player* p = getPlayerByTeam(team))
        p->addUnit(unit);

    m_statistics.recordUnitCreated(team);
    addEntity(unit);
    return unit;
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
        
        // Inject context so every building can perform spatial queries,
        // spawn projectiles, and notify the game of produced/cancelled units
        // through the same IUnitContext interface that Unit uses.
        building->setContext(this);
        
        // Place on map
        sf::Vector2i buildingSize = ResourceManager::getBuildingSize(type);
        sf::Vector2i tile = MathUtil::buildingTileOrigin(position, buildingSize, Constants::TILE_SIZE);
        m_map.placeBuilding(tile.x, tile.y, buildingSize.x, buildingSize.y);
        
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

void Game::notifyUnitProduced(EntityType unitType, Building* sourceBuilding) {
    spawnUnitFromBuilding(unitType, sourceBuilding->getTeam(), sourceBuilding);
}

void Game::refundProductionCost(EntityType unitType, Team team) {
    if (Player* p = getPlayerByTeam(team))
        p->addResources(ResourceManager::getMineralCost(unitType), 0);
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
    constexpr float PUSH_SPEED = 150.0f;
    // How far inside the building bounds the centre must be before we push.
    // Match the inset used in checkPositionBlocked so the two systems agree
    // on what counts as "inside" a building.
    constexpr float INSET = 2.0f;

    for (auto& entity : m_allEntities) {
        if (!entity || !entity->isAlive()) continue;
        Unit* unit = entity->asUnit();
        if (!unit || !unit->isCollidable()) continue;

        sf::Vector2f pos = unit->getPosition();

        for (auto& other : m_allEntities) {
            if (!other || !other->isAlive()) continue;
            if (!other->asBuilding()) continue;

            sf::FloatRect bounds = other->getBounds();

            // Inset the bounds slightly so corner-squeezers are not falsely
            // ejected while their centre is just grazing the building edge.
            sf::FloatRect inner = bounds;
            inner.position.x += INSET;
            inner.position.y += INSET;
            inner.size.x     -= INSET * 2.0f;
            inner.size.y     -= INSET * 2.0f;

            if (!inner.contains(pos)) continue;

            // Minimum-penetration depenetration against the full (non-inset)
            // bounds so the push distance is measured from the real wall face.
            float dLeft   = pos.x - bounds.position.x;
            float dRight  = (bounds.position.x + bounds.size.x) - pos.x;
            float dTop    = pos.y - bounds.position.y;
            float dBottom = (bounds.position.y + bounds.size.y) - pos.y;

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
                if (bldgDef->canProduce()) {
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
