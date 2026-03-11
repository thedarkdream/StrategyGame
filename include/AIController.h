#pragma once

#include "Types.h"
#include "AIScript.h"
#include <random>
#include <vector>
#include <unordered_map>
#include <set>

class Player;
class Map;
class Game;
class Worker;
class PlayerActions;

class AIController {
public:
    AIController(Player& player, Game& game);
    
    void update(float deltaTime);
    
    // Load scripts from directory
    void loadScripts(const std::string& directory);
    
private:
    Player&        m_player;
    Game&          m_game;
    Map*           m_map     = nullptr;
    PlayerActions* m_actions = nullptr;
    
    std::mt19937 m_rng;
    
    // Script execution state
    std::vector<AIScript> m_scripts;
    const AIScript* m_currentScript = nullptr;
    size_t m_commandIndex = 0;
    
    // Timing
    float m_decisionTimer = 0.0f;
    float m_decisionInterval = 1.0f;  // Check progress every second
    
    // Track pending commands
    struct PendingTrain {
        EntityType unitType;
        int remaining;  // How many more to train
    };
    std::vector<PendingTrain> m_pendingTrains;
    
    struct PendingBuild {
        EntityType buildingType;
        bool started = false;
        Worker* assignedWorker = nullptr;  // Worker currently assigned to this build
    };
    std::vector<PendingBuild> m_pendingBuilds;
    
    // Attack group accumulation
    std::unordered_map<EntityType, int> m_attackGroupNeeded;  // Type -> count needed
    std::set<UnitPtr> m_attackGroupUnits;  // Units reserved for attack
    
    // Script execution
    void selectRandomScript();
    void executeNextCommand();
    bool isCurrentCommandComplete();
    void processCommand(const AICommand& cmd);
    
    // Command handlers
    void handleTrain(const AICommand& cmd);
    void handleBuild(const AICommand& cmd);
    void handleAttackAdd(const AICommand& cmd);
    void handleAttack();
    
    // Background tasks (run every update)
    void manageIdleWorkers();
    void processTrainQueue();
    void processBuildQueue();
    
    // Helpers
    BuildingPtr findBuildingForUnit(EntityType unitType);
    int countUnitsOfType(EntityType type);
    int countBuildingsOfType(EntityType type, bool includeIncomplete = true);
    sf::Vector2f findBuildLocation(EntityType buildingType);
    Worker* findIdleWorker();
    sf::Vector2f findEnemyBase();
};
