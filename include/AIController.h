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
    void loadScripts(const std::string& directory);

private:
    // ----- Core references ------------------------------------------------
    Player&        m_player;
    Game&          m_game;
    Map*           m_map     = nullptr;
    PlayerActions* m_actions = nullptr;
    std::mt19937   m_rng;

    // ----- Script execution -----------------------------------------------
    std::vector<AIScript> m_scripts;
    const AIScript*       m_currentScript = nullptr;
    size_t                m_commandIndex  = 0;

    float m_decisionTimer    = 0.0f;
    float m_decisionInterval = 1.0f;  // seconds between script-progress checks

    // ----- Pending work ---------------------------------------------------
    struct PendingTrain {
        EntityType unitType;
        int        remaining = 0;
    };

    struct PendingBuild {
        EntityType          buildingType;
        bool                started        = false;
        std::weak_ptr<Unit> assignedWorker;  // worker currently walking to / constructing
    };

    std::vector<PendingTrain>             m_pendingTrains;
    std::vector<PendingBuild>             m_pendingBuilds;
    std::unordered_map<EntityType, int>   m_attackGroupNeeded;  // unitType -> count still needed
    std::set<UnitPtr>                     m_attackGroupUnits;   // units reserved for the attack

    // ----- Script lifecycle -----------------------------------------------
    void selectRandomScript();
    void executeNextCommand();
    // Returns true when the current command's goal is met.
    // NOTE: the AttackAdd case also reserves units as a side effect.
    bool isCurrentCommandComplete();
    void processCommand(const AICommand& cmd);

    // ----- Command handlers (called once when a command is issued) --------
    void handleTrain(const AICommand& cmd);
    void handleBuild(const AICommand& cmd);
    void handleAttackAdd(const AICommand& cmd);
    void handleAttack();

    // ----- Per-tick background tasks --------------------------------------
    void manageIdleWorkers();
    void processTrainQueue();
    void processBuildQueue();

    // ----- Helpers --------------------------------------------------------
    BuildingPtr  findBuildingForUnit(EntityType unitType);
    int          countUnitsOfType(EntityType type);
    int          countBuildingsOfType(EntityType type, bool includeIncomplete = true);
    sf::Vector2f findBuildLocation(EntityType buildingType);
    Worker*      findIdleWorker();
    sf::Vector2f findEnemyBase();

    // Re-find a worker for a started build whose original worker abandoned the job.
    void reassignBuildWorker(PendingBuild& pb);
};
