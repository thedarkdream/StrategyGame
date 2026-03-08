#pragma once

#include "Types.h"
#include "Map.h"
#include "Player.h"
#include "InputHandler.h"
#include "Renderer.h"
#include "AIController.h"
#include "ActionBar.h"
#include <SFML/Graphics.hpp>
#include <memory>
#include <vector>

class Game {
public:
    Game();
    ~Game() = default;
    
    void run();
    
    // Game state
    GameState getState() const { return m_state; }
    void setState(GameState state) { m_state = state; }
    
    // Access to game components
    Map& getMap() { return m_map; }
    Player& getPlayer() { return *m_player; }
    Player& getEnemy() { return *m_enemy; }
    InputHandler& getInput() { return *m_input; }
    ActionBar& getActionBar() { return m_actionBar; }
    
    // Entity access
    const EntityList& getAllEntities() const { return m_allEntities; }
    EntityPtr getEntityAtPosition(sf::Vector2f position);
    std::vector<EntityPtr> getEntitiesInRect(sf::FloatRect rect);
    std::vector<EntityPtr> getEntitiesInRect(sf::FloatRect rect, Team team);
    EntityPtr findNearestEnemy(sf::Vector2f pos, float radius, Team excludeTeam);
    EntityPtr findNearestResource(sf::Vector2f pos, float radius);
    EntityPtr findNearestAvailableResource(sf::Vector2f pos, float radius, EntityPtr exclude = nullptr);
    
    // Collision
    bool checkPositionBlocked(sf::Vector2f pos, float radius, Entity* excludeSelf);
    sf::Vector2f findSpawnPosition(sf::Vector2f origin, float unitRadius);
    std::vector<struct RVONeighbor> getNearbyUnitsRVO(sf::Vector2f pos, float radius, Unit* excludeSelf);
    
    // Entity management
    void addEntity(EntityPtr entity);
    void removeEntity(EntityPtr entity);
    void spawnUnit(EntityType type, Team team, sf::Vector2f position);
    void spawnBuilding(EntityType type, Team team, sf::Vector2f position, bool startComplete = true);
    
    // Commands
    void issueCommand(const std::vector<EntityPtr>& entities, Command command);
    void issueMoveCommand(sf::Vector2f target);
    void issueFollowCommand(EntityPtr target);
    void issueAttackMoveCommand(sf::Vector2f target);
    void issueAttackCommand(EntityPtr target);
    void issueGatherCommand(EntityPtr resource);
    void issueBuildCommand(EntityType buildingType, sf::Vector2f position);
    void issueContinueBuildCommand(EntityPtr building);
    void cancelBuildingConstruction(EntityPtr building);
    
private:
    // Window
    sf::RenderWindow m_window;
    
    // Game state
    GameState m_state = GameState::Playing;
    
    // Core components
    Map m_map;
    ActionBar m_actionBar;
    std::unique_ptr<Player> m_player;
    std::unique_ptr<Player> m_enemy;
    std::unique_ptr<InputHandler> m_input;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<AIController> m_ai;
    
    // All entities in game
    EntityList m_allEntities;
    
    // Timing
    sf::Clock m_clock;
    float m_deltaTime = 0.0f;
    
    // Game loop
    void processEvents();
    void update(float deltaTime);
    void render();
    
    // Initialization
    void initialize();
    void setupStartingUnits();
    void cleanupDeadEntities();
    void checkVictoryConditions();
    
    // Unit setup helpers
    void setupUnit(UnitPtr& unit);
    void setupWorker(Worker* worker, EntityPtr homeBase, Team team);
    
    // Generic entity finder template
    template<typename Predicate>
    EntityPtr findNearest(sf::Vector2f pos, float radius, Predicate predicate);
};

// Template implementation (must be in header)
#include "Entity.h"
#include "MathUtil.h"

template<typename Predicate>
EntityPtr Game::findNearest(sf::Vector2f pos, float radius, Predicate predicate) {
    EntityPtr nearest = nullptr;
    float nearestDist = radius;
    
    for (auto& entity : m_allEntities) {
        if (!entity || !entity->isAlive()) continue;
        if (!predicate(entity)) continue;
        
        float dist = MathUtil::distance(entity->getPosition(), pos);
        
        if (dist < nearestDist) {
            nearestDist = dist;
            nearest = entity;
        }
    }
    
    return nearest;
}
