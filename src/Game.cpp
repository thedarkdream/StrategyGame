#include "Game.h"
#include "Unit.h"
#include "Worker.h"
#include "Building.h"
#include "ResourceNode.h"
#include "ResourceManager.h"
#include "EffectsManager.h"
#include "SoundManager.h"
#include "TextureManager.h"
#include "EntityData.h"
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
    // NOTE: new entities spawned mid-loop (e.g. rockets) are buffered in the world's
    // pending list and flushed after the loop to avoid iterator invalidation.
    for (auto& entity : m_world.all()) {
        if (entity && entity->isAlive()) {
            entity->update(deltaTime);
            entity->updateHighlight(deltaTime);
        }
    }
    m_world.flush();  // Safe to add new entities now

    // Slide any unit that is physically inside a building outward each frame.
    m_world.pushUnitsOutOfBuildings(deltaTime);

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
    getPlayer().updateFog(m_map, m_world.all());

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

    // Wire each player to the entity world so getUnits()/getBuildings() work
    // without maintaining a separate per-player entity list.
    for (int i = 0; i < MAX_PLAYERS; ++i)
        if (m_players[i]) m_players[i]->setWorld(&m_world);

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
    // Each EntityDef may carry a preloadFn registered in EntityData.cpp.
    // Iterating the registry ensures every current and future unit/building
    // type is preloaded without any manual additions here.
    ENTITY_DATA.preloadAll();

    // Assets not tied to a specific EntityDef:
    Projectile::preload();
    EffectsManager::preload();
}

void Game::cleanupDeadEntities() {
    // Extract all entities that have finished dying. Process the bookkeeping
    // (stats, map tiles, player typed-lists) on the returned snapshot before
    // the shared_ptrs are released.
    EntityList removed = m_world.extractRemovable();
    for (const auto& entity : removed) {
        Team killerTeam = entity->getLastAttackerTeam();

        if (entity->asUnit()) {
            m_statistics.recordUnitDestroyed(entity->getTeam(), killerTeam);
        } else if (entity->asBuilding()) {
            m_statistics.recordBuildingDestroyed(entity->getTeam(), killerTeam);
            sf::Vector2i bSize = ENTITY_DATA.getBuildingTileSize(entity->getType());
            sf::Vector2i tile  = MathUtil::buildingTileOrigin(
                entity->getPosition(), bSize, Constants::TILE_SIZE);
            m_map.removeBuilding(tile.x, tile.y, bSize.x, bSize.y);
        }
    }

    // Prune dead/dying units from each player's selection every frame.
    for (auto& p : m_players)
        if (p) p->cleanupSelection();
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

EntityPtr Game::findNearestEnemy(sf::Vector2f pos, float radius, Team excludeTeam) {
    return m_world.findNearestEnemy(pos, radius, excludeTeam);
}

EntityPtr Game::findPriorityEnemy(sf::Vector2f pos, float radius, Team excludeTeam) {
    return m_world.findPriorityEnemy(pos, radius, excludeTeam);
}

EntityPtr Game::findNearestResource(sf::Vector2f pos, float radius) {
    return m_world.findNearestResource(pos, radius);
}

EntityPtr Game::findNearestAvailableResource(sf::Vector2f pos, float radius, EntityPtr exclude) {
    return m_world.findNearestAvailableResource(pos, radius, exclude);
}

EntityPtr Game::findHomeBase(Team team) {
    return m_world.findHomeBase(team);
}

bool Game::checkPositionBlocked(sf::Vector2f pos, float radius, Entity* excludeSelf) {
    return m_world.checkPositionBlocked(pos, radius, excludeSelf);
}

sf::Vector2f Game::findFreePosition(sf::Vector2f pos, float radius, float maxSearchRadius, Entity* excludeSelf) {
    return m_world.findFreePosition(pos, radius, maxSearchRadius, excludeSelf);
}

sf::Vector2f Game::findSpawnPosition(sf::Vector2f origin, float unitRadius) {
    return m_world.findSpawnPosition(origin, unitRadius);
}

sf::Vector2f Game::findNearestFreePosition(sf::Vector2f pos, float radius, int maxRings, Entity* excludeSelf) {
    return m_world.findNearestFreePosition(pos, radius, maxRings, excludeSelf);
}

std::vector<RVONeighbor> Game::getNearbyUnitsRVO(sf::Vector2f pos, float radius, Unit* excludeSelf) {
    return m_world.getNearbyUnitsRVO(pos, radius, excludeSelf);
}

void Game::addEntity(EntityPtr entity) {
    m_world.add(entity);
}

void Game::removeEntity(EntityPtr entity) {
    m_world.remove(entity);
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

        // Assigns a concentric-ring formation slot around a rally position.
        auto computeRallySlot = [&](sf::Vector2f rp, float spacing) -> sf::Vector2f {
            constexpr float SEARCH_RADIUS = 200.0f;
            int n = 0;
            for (const auto& e : m_world.all()) {
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

        // Use EntityDef flags rather than comparing against concrete EntityType values.
        const UnitDef* unitDef = ENTITY_DATA.getUnitDef(type);
        const bool canGather = unitDef && unitDef->canGather;

        if (rallyTarget && rallyTarget->isAlive()) {
            // Rally to entity
            if (canGather) {
                // Gatherers auto-gather from resource nodes, otherwise just move
                if (rallyTarget->isResource()) {
                    if (auto* worker = unit->asWorker()) {
                        worker->gather(rallyTarget);
                    }
                } else {
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
            if (canGather) {
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
void Game::setupUnit(UnitPtr& unit) {
    unit->setMap(&m_map);
    unit->setContext(this);
}

UnitPtr Game::spawnAndSetupUnit(EntityType type, Team team, sf::Vector2f pos,
                                 Building* /*sourceBuilding*/)
{
    UnitPtr unit = ResourceManager::createUnit(type, team, pos);
    if (!unit) return nullptr;

    setupUnit(unit);
    unit->setIsLocalTeam(m_players[m_localSlot] && team == m_players[m_localSlot]->getTeam());
    unit->onSpawned(this);

    m_statistics.recordUnitCreated(team);
    addEntity(unit);
    return unit;
}

EntityPtr Game::spawnBuilding(EntityType type, Team team, sf::Vector2f position, bool startComplete) {
    // Validate via registry — only spawn types that are actually defined as buildings.
    const EntityDef* def = ENTITY_DATA.get(type);
    if (!def || !def->isBuilding() || def->isResource())
        return nullptr;

    BuildingPtr building = ResourceManager::createBuilding(type, team, position);
    if (!building) return nullptr;

    if (building) {
        // Set construction state based on parameter
        if (!startComplete) {
            building->startConstruction();
        }
        
        // Inject context so every building can perform spatial queries,
        // spawn projectiles, and notify the game of produced/cancelled units
        // through the same IGameContext interface that Unit uses.
        building->setContext(this);
        
        // Place on map
        sf::Vector2i buildingSize = ENTITY_DATA.getBuildingTileSize(type);
        sf::Vector2i tile = MathUtil::buildingTileOrigin(position, buildingSize, Constants::TILE_SIZE);
        m_map.placeBuilding(tile.x, tile.y, buildingSize.x, buildingSize.y);
        
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
        p->addResources(ENTITY_DATA.getMineralCost(unitType), 0);
}

EntityRegistry& Game::entityRegistry() { return ENTITY_DATA; }
EffectsManager& Game::effectsManager() { return EFFECTS; }
SoundManager&   Game::soundManager()   { return SOUNDS; }

void Game::spawnProjectile(EntityPtr source, EntityPtr target, int damage, float speed) {
    if (!source || !target || !target->isAlive()) return;
    auto projectile = std::make_shared<Projectile>(source, target, damage, speed);
    addEntity(projectile);
}

void Game::depositResources(Team team, int amount) {
    if (Player* p = getPlayerByTeam(team)) p->addResources(amount, 0);
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

void Game::cancelBuildingConstruction(EntityPtr building) {
    getActions().cancelConstruction(building);
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
