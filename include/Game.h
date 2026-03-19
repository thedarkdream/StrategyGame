#pragma once

#include "Types.h"
#include "Map.h"
#include "Player.h"
#include "InputHandler.h"
#include "Renderer.h"
#include "PlayerController.h"
#include "ActionBar.h"
#include "MapSerializer.h"
#include "IUnitContext.h"
#include "PlayerActions.h"
#include "GameStatistics.h"
#include "DebugConsole.h"
#include "EntityWorld.h"
#include <SFML/Graphics.hpp>
#include <memory>
#include <vector>
#include <array>
#include <string>

class Game : public IUnitContext {
public:
    // localPlayerSlot: 0 = human controls Team::Player1,
    //                  1 = human controls Team::Player2, etc.
    static constexpr int MAX_PLAYERS = 4;

    Game(sf::RenderWindow& window, const std::string& mapFile = "", int localPlayerSlot = 0);
    ~Game() = default;
    
    // Per-frame interface (called by GameScreen)
    void handleEvent(const sf::Event& event);
    void update(float deltaTime);
    void render();
    
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
    InputHandler& getInput() { return *m_input; }
    ActionBar& getActionBar() { return m_actionBar; }
    
    // Statistics tracking
    GameStatistics& getStatistics() { return m_statistics; }
    const GameStatistics& getStatistics() const { return m_statistics; }
    int getLocalSlot() const { return m_localSlot; }
    
    // Entity access
    const EntityList& getAllEntities() const { return m_world.all(); }
    EntityPtr getEntityAtPosition(sf::Vector2f position);
    std::vector<EntityPtr> getEntitiesInRect(sf::FloatRect rect);
    std::vector<EntityPtr> getEntitiesInRect(sf::FloatRect rect, Team team);
    EntityPtr findNearestEnemy(sf::Vector2f pos, float radius, Team excludeTeam) override;
    EntityPtr findPriorityEnemy(sf::Vector2f pos, float radius, Team excludeTeam) override;
    EntityPtr findNearestResource(sf::Vector2f pos, float radius) override;
    EntityPtr findNearestAvailableResource(sf::Vector2f pos, float radius, EntityPtr exclude = nullptr) override;
    EntityPtr findHomeBase(Team team) override;
    
    // Collision
    bool checkPositionBlocked(sf::Vector2f pos, float radius, Entity* excludeSelf) override;
    sf::Vector2f findFreePosition(sf::Vector2f pos, float radius, float maxSearchRadius, Entity* excludeSelf) override;
    sf::Vector2f findSpawnPosition(sf::Vector2f origin, float unitRadius);
    sf::Vector2f findNearestFreePosition(sf::Vector2f pos, float radius, int maxRings, Entity* excludeSelf);
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
    
    // Per-player action dispatcher (one per occupied slot)
    PlayerActions& getActions()           { return *m_actions[m_localSlot]; }
    PlayerActions& getActions(int slot)   { return *m_actions[slot]; }

    // Raw controller access (may be nullptr for unused slots)
    PlayerController* getController(int slot) const { return m_controllers[slot].get(); }

    // For workers carrying minerals: return to base, then auto-gather again.
    void issueReturnCargoCommand();
    void issueBuildCommand(EntityType buildingType, sf::Vector2f position, bool append = false);
    void issueContinueBuildCommand(EntityPtr building, bool append = false);
    void cancelBuildingConstruction(EntityPtr building);
    void setRallyPoint(sf::Vector2f position, EntityPtr target = nullptr);
    
private:
    // Window (owned by Application, passed by reference)
    sf::RenderWindow& m_window;
    std::string       m_mapFile;
    int               m_localSlot = 0;   // which m_players slot the human drives

    // Game state
    GameState m_state = GameState::Playing;

    // Core components
    Map m_map;
    ActionBar m_actionBar;
    GameStatistics m_statistics;
    // Slots 0–3 correspond to Team::Player1–Player4; nullptr = slot unused
    std::array<std::unique_ptr<Player>,           MAX_PLAYERS> m_players;
    std::array<std::unique_ptr<PlayerController>, MAX_PLAYERS> m_controllers;
    std::array<std::unique_ptr<PlayerActions>,    MAX_PLAYERS> m_actions;
    std::unique_ptr<InputHandler>    m_input;
    std::unique_ptr<Renderer>          m_renderer;
    std::unique_ptr<DebugConsole>      m_debugConsole;
    
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

    // Helper: find the Player slot that owns entities of the given Team
    Player* getPlayerByTeam(Team t);
};
