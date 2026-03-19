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

    // Returns the name of the currently active script, or "(none)" if not set.
    std::string getCurrentScriptName() const;

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

    // ----- Loop execution state ------------------------------------------
    // Maps the command index of a LoopStart to its remaining iteration count.
    // -1 means infinite; 0 means the entry has been erased (loop done).
    std::unordered_map<int, int> m_loopCounters;

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
    // How many of each building type the script has ordered in total.
    // Persists across the lifetime of the script so destroyed buildings are rebuilt.
    std::unordered_map<EntityType, int>   m_buildTargets;
    std::unordered_map<EntityType, int>   m_attackGroupNeeded;  // unitType -> count still needed
    std::set<UnitPtr>                     m_attackGroupUnits;   // units reserved for the attack
    std::set<UnitPtr>                     m_deployedUnits;      // units currently executing an attack wave

    float m_defenseCooldown = 0.f;  // counts down; defense trigger blocked while > 0
    static constexpr float DEFENSE_COOLDOWN = 12.f;  // minimum seconds between defense responses

    // ----- Base defense (computed once at game start) ----------------------
    // m_chokePoint  : world position of the narrowest corridor exit from base.
    // m_defenseDir  : normalised direction base → choke (or enemy if open map).
    // m_defensePerp : perpendicular to m_defenseDir (used for slot spreading).
    // m_hasRealChoke: false = open map, use semicircle fallback.
    sf::Vector2f m_basePos       = {-1.f, -1.f};
    sf::Vector2f m_chokePoint    = {-1.f, -1.f};
    sf::Vector2f m_defenseDir    = { 1.f,  0.f};
    sf::Vector2f m_defensePerp   = { 0.f,  1.f};
    bool         m_defenseReady  = false;
    bool         m_hasRealChoke  = false;

    // Rally tracking: each newly trained combat unit is sent to a slot on
    // the defense line. m_nextRallySlot wraps around MAX_RALLY_SLOTS.
    static constexpr int MAX_RALLY_SLOTS = 20;
    int               m_nextRallySlot = 0;
    std::set<UnitPtr> m_ralliedUnits;

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
    // Release deployed units that have gone idle or died back into the pool.
    void releaseFinishedDeployments();
    // Scan for attacks on own units/buildings and dispatch idle defenders.
    void checkAndRespondToAttack(float deltaTime);
    // Send newly trained combat units to the defense rally line.
    void rallyNewCombatUnits();

    // ----- Helpers --------------------------------------------------------
    BuildingPtr  findBuildingForUnit(EntityType unitType);
    int          countUnitsOfType(EntityType type);
    // Count units of a type that are not committed to an attack wave.
    int          countFreeUnits(EntityType type) const;
    int          countBuildingsOfType(EntityType type, bool includeIncomplete = true);
    sf::Vector2f findBuildLocation(EntityType buildingType);
    // Compute choke point from terrain (called once on first update).
    void         computeBaseDefense();
    // World position of defense rally slot `slotIndex`.
    sf::Vector2f getDefenseRallySlot(int slotIndex) const;
    // Build position for the `turretIndex`-th turret at the choke.
    sf::Vector2f findTurretLocation(int turretIndex);
    Worker*      findIdleWorker();
    sf::Vector2f findEnemyBase();

    // Re-find a worker for a started build whose original worker abandoned the job.
    void reassignBuildWorker(PendingBuild& pb);
};
