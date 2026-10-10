#pragma once

#include "core/Types.h"
#include "world/Map.h"
#include "game/Player.h"
#include "ai/PlayerController.h"
#include "world/MapSerializer.h"
#include "entities/IGameContext.h"
#include "game/PlayerActions.h"
#include "game/GameStatistics.h"
#include "entities/EntityWorld.h"
#include <SFML/Graphics.hpp>
#include <memory>
#include <optional>
#include <vector>
#include <array>
#include <string>

// ---------------------------------------------------------------------------
// Game — one running match: the simulation and nothing else.
//
// Owns the map, the players, the entity world, the controllers (human/AI) and
// the statistics, implements IGameContext for the entities, and advances
// everything in update().  It knows no window, renderer, camera or widgets:
// those belong to the screen that presents the match (see GameScreen).
// ---------------------------------------------------------------------------
class Game : public IGameContext {
public:
    // localPlayerSlot: 0 = human controls Team::Player1,
    //                  1 = human controls Team::Player2, etc.
    static constexpr int MAX_PLAYERS = 4;

    Game(const std::string& mapFile = "", int localPlayerSlot = 0);
    ~Game() = default;
    
    // Advances the simulation by one tick.
    void update(float deltaTime);
    
    // Game state
    GameState getState() const { return m_state; }
    void setState(GameState state) { m_state = state; }
    
    // Access to game components
    Map& getMap() { return m_map; }
    const Map& getMap() const { return m_map; }
    // Local-human player (camera, selection, action bar)
    Player& getPlayer()       { return *m_players[m_localSlot]; }
    const Player& getPlayer() const { return *m_players[m_localSlot]; }
    // Direct slot access (slot 0 = Player1, slot 1 = Player2 …)
    Player& getPlayer(int slot)       { return *m_players[slot]; }
    const Player& getPlayer(int slot) const { return *m_players[slot]; }
    // First non-local occupied slot (convenience for 2-player games)
    Player& getEnemy();
    
    // Where the map places the local player's start (world units), if it defines one.
    // The screen uses it to point the camera.
    std::optional<sf::Vector2f> getLocalStartPosition() const { return m_localStart; }
    
    // Statistics tracking
    GameStatistics& getStatistics() { return m_statistics; }
    const GameStatistics& getStatistics() const { return m_statistics; }
    int getLocalSlot() const { return m_localSlot; }
    
    // Direct access to the entity world for read-only spatial queries.
    // Prefer this over individual pass-through helpers on Game.
    EntityWorld&       getWorld()       { return m_world; }
    const EntityWorld& getWorld() const { return m_world; }

    EntityPtr findNearestEnemy(sf::Vector2f pos, float radius, Team excludeTeam) override;
    EntityPtr findPriorityEnemy(sf::Vector2f pos, float radius, Team excludeTeam) override;
    EntityPtr findNearestResource(sf::Vector2f pos, float radius) override;
    EntityPtr findNearestAvailableResource(sf::Vector2f pos, float radius, EntityPtr exclude = nullptr) override;
    EntityPtr findHomeBase(Team team) override;
    
    // Collision
    bool checkPositionBlocked(sf::Vector2f pos, float radius, Entity* excludeSelf) override;
    sf::Vector2f findFreePosition(sf::Vector2f pos, float radius, float maxSearchRadius, Entity* excludeSelf) override;
    std::vector<RVONeighbor> getNearbyUnitsRVO(sf::Vector2f pos, float radius, Unit* excludeSelf) override;
    
    // Entity management
    void addEntity(EntityPtr entity);
    void removeEntity(EntityPtr entity);
    void spawnUnit(EntityType type, Team team, sf::Vector2f position);
    void spawnUnitFromBuilding(EntityType type, Team team, Building* sourceBuilding);
    EntityPtr spawnBuilding(EntityType type, Team team, sf::Vector2f position, bool startComplete = true);
    void spawnProjectile(EntityPtr source, EntityPtr target, int damage, float speed) override;
    void depositResources(Team team, int amount) override;
    void notifyUnitProduced(EntityType unitType, Building* sourceBuilding) override;
    void refundProductionCost(EntityType unitType, Team team) override;

    // IGameContext service accessors
    EntityRegistry& entityRegistry() override;
    EffectsManager& effectsManager() override;
    SoundManager&   soundManager()   override;
    
    // Per-player action dispatcher (one per occupied slot)
    PlayerActions& getActions()           { return *m_actions[m_localSlot]; }
    PlayerActions& getActions(int slot)   { return *m_actions[slot]; }

    // Raw controller access (may be nullptr for unused slots)
    PlayerController* getController(int slot) const { return m_controllers[slot].get(); }

    // For workers carrying minerals: return to base, then auto-gather again.
    void issueReturnCargoCommand();
    void issueBuildCommand(EntityType buildingType, sf::Vector2f position, bool append = false);
    void cancelBuildingConstruction(EntityPtr building);
    void setRallyPoint(sf::Vector2f position, EntityPtr target = nullptr);
    
private:
    std::string       m_mapFile;
    int               m_localSlot = 0;   // which m_players slot the human drives
    std::optional<sf::Vector2f> m_localStart;   // from the map's StartPosition marker

    // Game state
    GameState m_state = GameState::Playing;

    // Core components
    Map m_map;
    GameStatistics m_statistics;
    // Slots 0–3 correspond to Team::Player1–Player4; nullptr = slot unused
    std::array<std::unique_ptr<Player>,           MAX_PLAYERS> m_players;
    std::array<std::unique_ptr<PlayerController>, MAX_PLAYERS> m_controllers;
    std::array<std::unique_ptr<PlayerActions>,    MAX_PLAYERS> m_actions;
    
    // All entities in game
    EntityWorld m_world;
    
    // Game loop helpers
    
    // Initialization
    void initialize();
    void preloadAssets();   // Load all sounds & textures upfront to avoid mid-game hitches
    void setupFromMapData(const MapData& data);  // Initialize from editor-saved map
    void cleanupDeadEntities();
    void checkVictoryConditions();
    void flushPendingEntities();  // Merge m_pendingEntities into m_allEntities
    
    // Unit setup helpers
    void setupUnit(UnitPtr& unit);
    // Creates, wires up, and adds a unit to the world. Returns the new unit.
    UnitPtr spawnAndSetupUnit(EntityType type, Team team, sf::Vector2f pos,
                              Building* sourceBuilding);
    // Spawn position helpers (internal — used only within Game.cpp)
    sf::Vector2f findSpawnPosition(sf::Vector2f origin, float unitRadius);
    sf::Vector2f findNearestFreePosition(sf::Vector2f pos, float radius, int maxRings, Entity* excludeSelf);

    // Helper: find the Player slot that owns entities of the given Team
    Player* getPlayerByTeam(Team t);
};
