#include "AIController.h"
#include "AIScript.h"
#include "Game.h"
#include "Player.h"
#include "PlayerActions.h"
#include "Unit.h"
#include "Worker.h"
#include "Building.h"
#include "Map.h"
#include "ResourceManager.h"
#include "EntityData.h"
#include "Constants.h"
#include "MathUtil.h"
#include <algorithm>
#include <limits>
#include <chrono>
#include <cmath>

AIController::AIController(Player& player, Game& game)
    : m_player(player)
    , m_game(game)
    , m_map(&game.getMap())
    , m_actions(&game.getActions(teamToIndex(player.getTeam())))
{
    // std::random_device is broken on MinGW/Windows (always returns the same
    // value), so we combine a high-resolution timestamp with the object address
    // to guarantee a unique seed for every AIController instance.
    auto timeSeed  = static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    auto addrSeed  = reinterpret_cast<uintptr_t>(this);
    std::seed_seq seq{ static_cast<uint32_t>(timeSeed),
                       static_cast<uint32_t>(timeSeed >> 32),
                       static_cast<uint32_t>(addrSeed) };
    m_rng = std::mt19937(seq);
}

std::string AIController::getCurrentScriptName() const {
    return m_currentScript ? m_currentScript->getName() : "(none)";
}

void AIController::loadScripts(const std::string& directory) {
    m_scripts = AIScriptLoader::loadAllScripts(directory);
    if (!m_scripts.empty()) {
        selectRandomScript();
    }
}

void AIController::selectRandomScript() {
    if (m_scripts.empty()) return;
    
    std::uniform_int_distribution<size_t> dist(0, m_scripts.size() - 1);
    m_currentScript = &m_scripts[dist(m_rng)];
    m_commandIndex = 0;
    
    // Clear state
    m_pendingTrains.clear();
    m_pendingBuilds.clear();
    m_buildTargets.clear();
    m_attackGroupNeeded.clear();
    m_attackGroupUnits.clear();
    m_deployedUnits.clear();
    m_loopCounters.clear();
    // Reset defense state so it is recomputed for the new script/match.
    m_ralliedUnits.clear();
    m_nextRallySlot = 0;
    m_defenseReady  = false;
    
    // Execute the first command
    if (!m_currentScript->getCommands().empty()) {
        executeNextCommand();
    }
}

void AIController::update(float deltaTime) {
    m_decisionTimer += deltaTime;
    
    // Process builds and trains first, then send remaining idle workers to gather.
    // This ensures any worker re-assigned to a build isn't immediately hijacked by
    // manageIdleWorkers in the same tick.
    processTrainQueue();
    processBuildQueue();
    manageIdleWorkers();
    releaseFinishedDeployments();
    checkAndRespondToAttack(deltaTime);

    // Compute choke point once — deferred to first update so buildings exist.
    if (!m_defenseReady) computeBaseDefense();
    rallyNewCombatUnits();
    
    // Check for command progression periodically
    if (m_decisionTimer >= m_decisionInterval) {
        m_decisionTimer = 0.0f;
        
        // Try to advance to next command if current is complete
        if (m_currentScript && m_commandIndex < m_currentScript->getCommands().size()) {
            if (isCurrentCommandComplete()) {
                m_commandIndex++;
                if (m_commandIndex < m_currentScript->getCommands().size()) {
                    executeNextCommand();
                }
            }
        }
    }
}

void AIController::executeNextCommand() {
    if (!m_currentScript || m_commandIndex >= m_currentScript->getCommands().size()) {
        return;
    }
    
    const AICommand& cmd = m_currentScript->getCommands()[m_commandIndex];
    processCommand(cmd);
}

bool AIController::isCurrentCommandComplete() {
    if (!m_currentScript || m_commandIndex >= m_currentScript->getCommands().size()) {
        return true;
    }
    
    const AICommand& cmd = m_currentScript->getCommands()[m_commandIndex];
    
    switch (cmd.type) {
        case AICommand::Type::Train: {
            // Complete once we have at least cmd.count free (uncommitted) units
            // of the requested type.  This lets idle survivors from previous
            // attacks satisfy the quota without training new units.
            for (const auto& pt : m_pendingTrains) {
                if (pt.unitType == cmd.entityType && pt.remaining > 0)
                    return false;  // Still waiting for queued production to finish
            }
            return countFreeUnits(cmd.entityType) >= cmd.count;
        }
        
        case AICommand::Type::Build: {
            // Not complete until the building is fully constructed.
            // This ensures prerequisites (e.g. Barracks before Factory) are
            // satisfied before the script tries to issue the next Build.
            return countBuildingsOfType(cmd.entityType, false) > 0;
        }
        
        case AICommand::Type::AttackAdd: {
            // Check if we have enough units reserved
            auto it = m_attackGroupNeeded.find(cmd.entityType);
            if (it != m_attackGroupNeeded.end() && it->second > 0) {
                // Try to reserve more units — skip deployed waves and already-staged units
                int needed = it->second;
                for (const auto& unit : m_player.getUnits()) {
                    if (needed <= 0) break;
                    if (unit->getType() == cmd.entityType && unit->isAlive()) {
                        if (m_attackGroupUnits.find(unit) == m_attackGroupUnits.end() &&
                            m_deployedUnits.find(unit)    == m_deployedUnits.end()) {
                            m_attackGroupUnits.insert(unit);
                            needed--;
                        }
                    }
                }
                m_attackGroupNeeded[cmd.entityType] = needed;
                return needed <= 0;
            }
            return true;
        }
        
        case AICommand::Type::Attack:
            // Attack is instant, always complete
            return true;

        case AICommand::Type::LoopStart:
        case AICommand::Type::LoopEnd:
            // These are flow-control markers; they complete instantly.
            return true;
    }
    
    return true;
}

void AIController::processCommand(const AICommand& cmd) {
    switch (cmd.type) {
        case AICommand::Type::Train:
            handleTrain(cmd);
            break;
        case AICommand::Type::Build:
            handleBuild(cmd);
            break;
        case AICommand::Type::AttackAdd:
            handleAttackAdd(cmd);
            break;
        case AICommand::Type::Attack:
            handleAttack();
            break;

        case AICommand::Type::LoopStart: {
            // Initialise counter only on the first pass through this loop header.
            // On subsequent passes (after a back-jump from LoopEnd) the counter
            // is already in the map and must NOT be reset.
            if (m_loopCounters.find(m_commandIndex) == m_loopCounters.end()) {
                // count==0 means infinite (-1 sentinel)
                m_loopCounters[m_commandIndex] = (cmd.count == 0) ? -1 : cmd.count;
            }
            break;
        }

        case AICommand::Type::LoopEnd: {
            if (cmd.loopStartIndex < 0) break;  // unmatched EndLoop, skip

            auto it = m_loopCounters.find(cmd.loopStartIndex);
            int remaining = (it != m_loopCounters.end()) ? it->second : -1;

            if (remaining == -1) {
                // Infinite loop — always jump back to the LoopStart.
                m_commandIndex = static_cast<size_t>(cmd.loopStartIndex);
            } else if (remaining > 1) {
                // More iterations left — decrement and jump back.
                m_loopCounters[cmd.loopStartIndex] = remaining - 1;
                m_commandIndex = static_cast<size_t>(cmd.loopStartIndex);
            } else {
                // Last iteration just finished — erase counter and fall through.
                m_loopCounters.erase(cmd.loopStartIndex);
            }
            break;
        }
    }
}

void AIController::handleTrain(const AICommand& cmd) {
    // Only queue the deficit — units already free (not committed to an attack)
    // count toward the goal, so we never over-produce.
    int deficit = cmd.count - countFreeUnits(cmd.entityType);
    if (deficit > 0)
        m_pendingTrains.push_back({cmd.entityType, deficit});
}

void AIController::handleBuild(const AICommand& cmd) {
    // Track total required count for this building type across all Build commands
    // executed so far — this is the value the repair pass will maintain forever.
    m_buildTargets[cmd.entityType]++;
    m_pendingBuilds.push_back({cmd.entityType, false});
}

void AIController::handleAttackAdd(const AICommand& cmd) {
    // Accumulate needed units for attack
    m_attackGroupNeeded[cmd.entityType] += cmd.count;
}

void AIController::handleAttack() {
    // Choose which enemy to attack this wave and remember the team so that
    // releaseFinishedDeployments can keep redirecting units to that same
    // enemy's remaining buildings rather than jumping to a different player.
    m_attackTargetTeam = pickEnemyTeam();
    sf::Vector2f targetPos = findEnemyBuildingOfTeam(m_attackTargetTeam);
    if (targetPos.x < 0) return;
    
    // Send all reserved attack units
    std::vector<EntityPtr> attackers;
    for (const auto& unit : m_attackGroupUnits) {
        if (unit && unit->isAlive()) {
            attackers.push_back(std::static_pointer_cast<Entity>(unit));
        }
    }
    
    if (!attackers.empty()) {
        m_actions->attackMove(attackers, targetPos);
    }
    
    // Move staged units into the deployed set so they cannot be re-recruited
    // by the next AttackAdd before they finish this wave.
    for (const auto& unit : m_attackGroupUnits) {
        if (unit && unit->isAlive())
            m_deployedUnits.insert(unit);
    }
    m_attackGroupUnits.clear();
    m_attackGroupNeeded.clear();
}

void AIController::releaseFinishedDeployments() {
    // A deployed unit is considered "done" in one of two ways:
    //   a) It died — release it immediately.
    //   b) It went idle (finished its current attack order):
    //        • If the enemy still has buildings, redirect it to the next
    //          target and keep it deployed (so it chains through all remaining
    //          structures without waiting for the next script Attack command).
    //        • If the enemy has been eliminated, release it back into the pool.
    for (auto it = m_deployedUnits.begin(); it != m_deployedUnits.end(); ) {
        const UnitPtr& unit = *it;

        // Dead — always release.
        if (!unit || !unit->isAlive()) {
            m_ralliedUnits.erase(unit);
            it = m_deployedUnits.erase(it);
            continue;
        }

        // Still actively fighting or moving — leave it alone.
        if (!unit->isIdle()) {
            ++it;
            continue;
        }

        // Unit is idle.  Keep attacking the same enemy team until it is
        // fully eliminated, then release back into the free pool.
        sf::Vector2f nextTarget = findEnemyBuildingOfTeam(m_attackTargetTeam);
        if (nextTarget.x >= 0.f) {
            // That team still has buildings — redirect to the next one.
            unit->attackMoveTo(nextTarget);
            ++it;
        } else {
            // Target team eliminated — release back into the free pool.
            m_ralliedUnits.erase(unit);
            it = m_deployedUnits.erase(it);
        }
    }
}

void AIController::checkAndRespondToAttack(float deltaTime) {
    // Tick cooldown.
    if (m_defenseCooldown > 0.f) {
        m_defenseCooldown -= deltaTime;
        return;
    }

    const Team myTeam = m_player.getTeam();

    // --- Detect if anything is under attack --------------------------------
    // Only non-deployed units that are NOT already in combat count as triggers.
    bool triggered = false;
    sf::Vector2f triggerPos(-1.f, -1.f);  // position of a triggered entity

    for (const auto& unit : m_player.getUnits()) {
        if (!unit->isAlive()) continue;
        if (m_deployedUnits.find(unit) != m_deployedUnits.end()) continue;
        if (unit->getState() == UnitState::Attacking) continue;  // already fighting
        if (unit->isUnderAttack()) {
            triggered = true;
            triggerPos = unit->getPosition();
            break;
        }
    }

    if (!triggered) {
        for (const auto& building : m_player.getBuildings()) {
            if (!building->isAlive()) continue;
            if (building->isUnderAttack()) {
                triggered = true;
                triggerPos = building->getPosition();
                break;
            }
        }
    }

    if (!triggered) return;

    // --- Find the nearest enemy unit to the triggered position -------------
    sf::Vector2f attackerPos = triggerPos;
    float bestDist = std::numeric_limits<float>::max();
    for (const auto& entity : m_game.getAllEntities()) {
        if (!entity || !entity->isAlive()) continue;
        if (entity->getTeam() == myTeam || entity->getTeam() == Team::Neutral) continue;
        if (!entity->asUnit()) continue;
        float d = MathUtil::distance(entity->getPosition(), triggerPos);
        if (d < bestDist) { bestDist = d; attackerPos = entity->getPosition(); }
    }

    // --- Collect all idle, non-deployed responders -------------------------
    std::vector<EntityPtr> defenders;
    for (const auto& unit : m_player.getUnits()) {
        if (!unit->isAlive()) continue;
        if (m_deployedUnits.find(unit) != m_deployedUnits.end()) continue;
        if (!unit->isIdle()) continue;
        defenders.push_back(std::static_pointer_cast<Entity>(unit));
    }

    if (defenders.empty()) return;

    m_actions->attackMove(defenders, attackerPos);
    m_defenseCooldown = DEFENSE_COOLDOWN;
}

void AIController::manageIdleWorkers() {
    // Pre-collect mineral patches once so we don't re-scan all entities per worker.
    std::vector<EntityPtr> minerals;
    for (const auto& entity : m_game.getAllEntities()) {
        if (entity && entity->isAlive() && entity->getType() == EntityType::MineralPatch)
            minerals.push_back(entity);
    }
    if (minerals.empty()) return;

    for (auto& unit : m_player.getUnits()) {
        if (unit->getType() != EntityType::Worker || !unit->isIdle()) continue;

        EntityPtr nearest;
        float nearestDist = std::numeric_limits<float>::max();
        for (const auto& mineral : minerals) {
            float dist = MathUtil::distance(mineral->getPosition(), unit->getPosition());
            if (dist < nearestDist) { nearestDist = dist; nearest = mineral; }
        }
        if (nearest)
            m_actions->gather({std::static_pointer_cast<Entity>(unit)}, nearest);
    }
}

void AIController::processTrainQueue() {
    // Process pending train commands
    for (auto& pt : m_pendingTrains) {
        if (pt.remaining <= 0) continue;
        
        BuildingPtr building = findBuildingForUnit(pt.unitType);
        if (building && !building->isProducing()) {
            if (m_player.canAfford(ENTITY_DATA.getMineralCost(pt.unitType), 
                                   ENTITY_DATA.getGasCost(pt.unitType))) {
                m_actions->trainUnit(building, pt.unitType);
                pt.remaining--;
            }
        }
    }
    
    // Clean up completed trains
    m_pendingTrains.erase(
        std::remove_if(m_pendingTrains.begin(), m_pendingTrains.end(),
            [](const PendingTrain& pt) { return pt.remaining <= 0; }),
        m_pendingTrains.end()
    );
}

void AIController::reassignBuildWorker(PendingBuild& pb) {
    // Find the incomplete foundation for this build and send an idle worker to it.
    for (auto& bld : m_player.getBuildings()) {
        if (bld->getType() != pb.buildingType || bld->isConstructed()) continue;

        Worker* newWorker = findIdleWorker();
        if (!newWorker) return;

        for (auto& unit : m_player.getUnits()) {
            if (unit.get() == newWorker) {
                m_actions->continueConstruction(
                    std::static_pointer_cast<Entity>(bld),
                    {std::static_pointer_cast<Entity>(unit)});
                pb.assignedWorker = unit;
                return;
            }
        }
        return;  // found the building but no unit ptr (shouldn't happen)
    }
}

void AIController::processBuildQueue() {
    // --- Repair pass ---------------------------------------------------
    // For each building type in the script, ensure the world (including
    // incomplete foundations) plus unplaced pending entries together meet
    // the target count.  If a completed building or a foundation was
    // destroyed, this will re-add the missing entries automatically.
    for (auto& [type, targetCount] : m_buildTargets) {
        int inWorld  = countBuildingsOfType(type, true);
        int unplaced = static_cast<int>(std::count_if(
            m_pendingBuilds.begin(), m_pendingBuilds.end(),
            [type](const PendingBuild& pb){ return pb.buildingType == type && !pb.started; }));
        int deficit = targetCount - inWorld - unplaced;
        for (int i = 0; i < deficit; ++i)
            m_pendingBuilds.push_back({type, false});
    }

    for (auto& pb : m_pendingBuilds) {
        if (!pb.started) {
            if (m_player.canAfford(ENTITY_DATA.getMineralCost(pb.buildingType),
                                   ENTITY_DATA.getGasCost(pb.buildingType))) {
                sf::Vector2f buildPos = (pb.buildingType == EntityType::Turret)
                    ? findTurretLocation(countBuildingsOfType(EntityType::Turret, true))
                    : findBuildLocation(pb.buildingType);
                Worker* worker = findIdleWorker();
                if (buildPos.x >= 0.f && worker) {
                    bool ok = m_actions->constructBuilding(pb.buildingType, buildPos, worker);
                    if (ok) {
                        pb.started = true;
                        for (auto& unit : m_player.getUnits()) {
                            if (unit.get() == worker) { pb.assignedWorker = unit; break; }
                        }
                    }
                }
            }
        } else {
            // If ALL buildings of this type are gone (foundations included),
            // this entry is an orphan (its foundation was destroyed).  Reset
            // it so placement can be re-attempted on the next tick.  The
            // repair pass won't double-add because this entry now counts as
            // an unplaced entry when recalculating the deficit.
            if (countBuildingsOfType(pb.buildingType, true) == 0) {
                pb.started = false;
                pb.assignedWorker.reset();
                continue;
            }

            // Build was issued — check if the worker abandoned the job.
            auto assigned = pb.assignedWorker.lock();
            if (assigned && assigned->isAlive()) {
                Worker* w = assigned->asWorker();
                if (w && !w->isBuilding())
                    reassignBuildWorker(pb);
            } else if (!assigned) {
                // Worker was destroyed; try a fresh one.
                reassignBuildWorker(pb);
            }
        }
    }

    // --- Erase pass ----------------------------------------------------
    // Remove one started=true entry per completed building of each type
    // so multi-building scripts are handled correctly.
    std::unordered_map<EntityType, int> completedCount;
    for (const auto& b : m_player.getBuildings())
        if (b->isAlive() && b->isConstructed())
            completedCount[b->getType()]++;

    m_pendingBuilds.erase(
        std::remove_if(m_pendingBuilds.begin(), m_pendingBuilds.end(),
            [&](const PendingBuild& pb) {
                if (!pb.started) return false;
                auto it = completedCount.find(pb.buildingType);
                if (it != completedCount.end() && it->second > 0) {
                    --it->second;
                    return true;
                }
                return false;
            }),
        m_pendingBuilds.end());

    // Prune excess unstarted entries: if the world already satisfies the
    // target (e.g. a building was placed earlier this tick), don't leave
    // spare unstarted orders that would over-build.
    for (auto& [type, targetCount] : m_buildTargets) {
        int inWorld = countBuildingsOfType(type, true);
        if (inWorld >= targetCount) {
            m_pendingBuilds.erase(
                std::remove_if(m_pendingBuilds.begin(), m_pendingBuilds.end(),
                    [type](const PendingBuild& pb){
                        return pb.buildingType == type && !pb.started;
                    }),
                m_pendingBuilds.end());
        }
    }
}

BuildingPtr AIController::findBuildingForUnit(EntityType unitType) {
    // Find the building type that produces this unit
    EntityType buildingType = EntityType::None;
    
    switch (unitType) {
        case EntityType::Worker:
            buildingType = EntityType::Base;
            break;
        case EntityType::Soldier:
        case EntityType::Brute:
            buildingType = EntityType::Barracks;
            break;
        case EntityType::LightTank:
            buildingType = EntityType::Factory;
            break;
        default:
            return nullptr;
    }
    
    // Find an idle building of that type
    for (auto& building : m_player.getBuildings()) {
        if (building->getType() == buildingType && 
            building->isConstructed() && !building->isProducing()) {
            return building;
        }
    }
    
    return nullptr;
}

int AIController::countUnitsOfType(EntityType type) {
    const auto& units = m_player.getUnits();
    return static_cast<int>(std::count_if(units.begin(), units.end(),
        [type](const UnitPtr& u) { return u->getType() == type && u->isAlive(); }));
}

int AIController::countFreeUnits(EntityType type) const {
    // A "free" unit is alive, of the right type, and not committed to any
    // attack wave (neither staged in m_attackGroupUnits nor already deployed).
    int count = 0;
    for (const auto& unit : m_player.getUnits()) {
        if (!unit->isAlive() || unit->getType() != type) continue;
        if (m_attackGroupUnits.find(unit) != m_attackGroupUnits.end()) continue;
        if (m_deployedUnits.find(unit)    != m_deployedUnits.end())    continue;
        ++count;
    }
    return count;
}

int AIController::countBuildingsOfType(EntityType type, bool includeIncomplete) {
    const auto& buildings = m_player.getBuildings();
    return static_cast<int>(std::count_if(buildings.begin(), buildings.end(),
        [type, includeIncomplete](const BuildingPtr& b) {
            return b->getType() == type && b->isAlive()
                && (includeIncomplete || b->isConstructed());
        }));
}

sf::Vector2f AIController::findBuildLocation(EntityType buildingType) {
    if (m_player.getBuildings().empty())
        return sf::Vector2f(-1, -1);

    sf::Vector2f basePos = m_player.getBuildings()[0]->getPosition();
    sf::Vector2i buildingSize = ResourceManager::getBuildingSize(buildingType);
    int baseTileX = static_cast<int>(basePos.x / Constants::TILE_SIZE);
    int baseTileY = static_cast<int>(basePos.y / Constants::TILE_SIZE);

    // Compute the average direction from the base toward nearby mineral patches
    sf::Vector2f mineralDir(0.f, 0.f);
    int mineralCount = 0;
    for (const auto& entity : m_game.getAllEntities()) {
        if (entity->getType() == EntityType::MineralPatch ||
            entity->getType() == EntityType::GasGeyser) {
            float dist = MathUtil::distance(entity->getPosition(), basePos);
            if (dist < 700.f) {
                sf::Vector2f d = entity->getPosition() - basePos;
                float len = std::sqrt(d.x * d.x + d.y * d.y);
                if (len > 0.f) mineralDir += d / len;
                ++mineralCount;
            }
        }
    }
    if (mineralCount > 0) {
        float len = std::sqrt(mineralDir.x * mineralDir.x + mineralDir.y * mineralDir.y);
        if (len > 0.f) mineralDir /= len;
    }

    // Two passes: first avoid the mineral-facing side, then accept any free spot
    for (int pass = 0; pass < 2; ++pass) {
        for (int radius = 3; radius < 12; ++radius) {
            for (int dx = -radius; dx <= radius; ++dx) {
                for (int dy = -radius; dy <= radius; ++dy) {
                    if (std::abs(dx) != radius && std::abs(dy) != radius) continue;

                    int tileX = baseTileX + dx;
                    int tileY = baseTileY + dy;

                    if (!m_map->canPlaceBuilding(tileX, tileY, buildingSize.x, buildingSize.y))
                        continue;

                    if (pass == 0 && mineralCount > 0) {
                        sf::Vector2f dir(static_cast<float>(dx), static_cast<float>(dy));
                        float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
                        if (len > 0.f) dir /= len;
                        if (dir.x * mineralDir.x + dir.y * mineralDir.y > 0.3f) continue;
                    }

                    return sf::Vector2f(
                        static_cast<float>(tileX * Constants::TILE_SIZE),
                        static_cast<float>(tileY * Constants::TILE_SIZE)
                    );
                }
            }
        }
    }

    return sf::Vector2f(-1, -1);
}

Worker* AIController::findIdleWorker() {
    // Prefer a truly idle worker; fall back to any worker not actively building.
    Worker* fallback = nullptr;
    for (const auto& unit : m_player.getUnits()) {
        if (unit->getType() != EntityType::Worker || !unit->isAlive()) continue;
        Worker* w = unit->asWorker();
        if (!w || w->isBuilding()) continue;
        if (w->isIdle()) return w;
        if (!fallback) fallback = w;
    }
    return fallback;
}

sf::Vector2f AIController::findEnemyBase() {
    const Team myTeam = m_player.getTeam();

    // Single pass: collect base positions; if none exist, fall back to any building.
    std::vector<sf::Vector2f> targets;
    bool foundBase = false;

    for (const auto& entity : m_game.getAllEntities()) {
        if (!entity || !entity->isAlive()) continue;
        if (entity->getTeam() == myTeam || entity->getTeam() == Team::Neutral) continue;
        if (!entity->asBuilding()) continue;

        const bool isBase = (entity->getType() == EntityType::Base);
        if (isBase && !foundBase) {
            targets.clear();   // discard any fallback buildings collected so far
            foundBase = true;
        }
        if (isBase || !foundBase)
            targets.push_back(entity->getPosition());
    }

    if (targets.empty()) return sf::Vector2f(-1, -1);

    std::uniform_int_distribution<int> pick(0, static_cast<int>(targets.size()) - 1);
    return targets[pick(m_rng)];
}

Team AIController::pickEnemyTeam() {
    const Team myTeam = m_player.getTeam();

    // Collect enemy teams that have a Base (preferred) or any building (fallback).
    std::vector<Team> withBase;
    std::vector<Team> withBuilding;

    auto addUnique = [](std::vector<Team>& v, Team t) {
        if (std::find(v.begin(), v.end(), t) == v.end()) v.push_back(t);
    };

    for (const auto& entity : m_game.getAllEntities()) {
        if (!entity || !entity->isAlive()) continue;
        if (entity->getTeam() == myTeam || entity->getTeam() == Team::Neutral) continue;
        if (!entity->asBuilding()) continue;
        Team t = entity->getTeam();
        if (entity->getType() == EntityType::Base)
            addUnique(withBase, t);
        else
            addUnique(withBuilding, t);
    }

    // Prefer teams that still have a main Base.
    auto& pool = withBase.empty() ? withBuilding : withBase;
    if (pool.empty()) return Team::Neutral;

    std::uniform_int_distribution<int> pick(0, static_cast<int>(pool.size()) - 1);
    return pool[pick(m_rng)];
}

sf::Vector2f AIController::findEnemyBuildingOfTeam(Team team) {
    // If no specific team is known, fall back to the generic search.
    if (team == Team::Neutral) return findEnemyBase();

    // Prefer the team's Base; fall back to any of their buildings.
    std::vector<sf::Vector2f> targets;
    bool foundBase = false;

    for (const auto& entity : m_game.getAllEntities()) {
        if (!entity || !entity->isAlive()) continue;
        if (entity->getTeam() != team) continue;
        if (!entity->asBuilding()) continue;

        const bool isBase = (entity->getType() == EntityType::Base);
        if (isBase && !foundBase) {
            targets.clear();
            foundBase = true;
        }
        if (isBase || !foundBase)
            targets.push_back(entity->getPosition());
    }

    if (targets.empty()) return sf::Vector2f(-1, -1);

    std::uniform_int_distribution<int> pick(0, static_cast<int>(targets.size()) - 1);
    return targets[pick(m_rng)];
}

// ---------------------------------------------------------------------------
// computeBaseDefense
// Scans radial rings of tiles outward from the AI base to find the FIRST
// (closest) narrow corridor the enemy must pass through.  This becomes the
// turret anchor and unit rally anchor.  If no narrow ring is found the map
// is considered open and a semicircle fallback is used instead.
// ---------------------------------------------------------------------------
void AIController::computeBaseDefense() {
    // Need at least one building to anchor the base position.
    m_basePos = {-1.f, -1.f};
    for (const auto& b : m_player.getBuildings()) {
        if (b->isAlive()) { m_basePos = b->getPosition(); break; }
    }
    if (m_basePos.x < 0.f) return;

    sf::Vector2i baseTile = m_map->worldToTile(m_basePos);

    // Use the nearest enemy base as a directional hint so the weighted
    // centroid of the choke ring faces the correct side.
    sf::Vector2f enemyPos = findEnemyBase();
    sf::Vector2f enemyDir = {1.f, 0.f};
    if (enemyPos.x >= 0.f) {
        sf::Vector2f d = enemyPos - m_basePos;
        float len = std::sqrt(d.x * d.x + d.y * d.y);
        if (len > 1.f) enemyDir = d / len;
    }

    // Ring scan parameters.
    constexpr int   MIN_R           = 5;    // start outside the base footprint
    constexpr int   MAX_R           = 28;   // ~28 tiles max search radius
    constexpr float CHOKE_THRESHOLD = 0.40f;

    float        bestPass     = 1.f;
    sf::Vector2f bestCentroid = m_basePos + enemyDir * float(MIN_R * Constants::TILE_SIZE);

    for (int R = MIN_R; R <= MAX_R; ++R) {
        int   total    = 0;
        int   walkable = 0;
        float cwx = 0.f, cwy = 0.f, wsum = 0.f;

        for (int dx = -(R + 1); dx <= (R + 1); ++dx) {
            for (int dy = -(R + 1); dy <= (R + 1); ++dy) {
                float dist = std::sqrt(float(dx * dx + dy * dy));
                // Only process tiles on this annular ring.
                if (dist < float(R) - 0.5f || dist >= float(R) + 0.5f) continue;

                int tx = baseTile.x + dx;
                int ty = baseTile.y + dy;

                ++total;
                // Out-of-bounds and non-walkable both count as walls.
                if (!m_map->isValidTile(tx, ty) || !m_map->isWalkable(tx, ty))
                    continue;

                ++walkable;
                // Weight toward enemy direction so the centroid lands on
                // the gap that faces the enemy, not a random side gap.
                sf::Vector2f dir{float(dx), float(dy)};
                if (dist > 0.f) dir /= dist;
                float w = std::max(0.01f, dir.x * enemyDir.x + dir.y * enemyDir.y + 0.5f);
                cwx  += float(tx) * w;
                cwy  += float(ty) * w;
                wsum += w;
            }
        }

        if (total == 0) continue;
        float pass = float(walkable) / float(total);

        // Track the minimum passability seen so far.
        if (pass < bestPass) {
            bestPass = pass;
            if (wsum > 0.f)
                bestCentroid = {
                    (cwx / wsum) * Constants::TILE_SIZE,
                    (cwy / wsum) * Constants::TILE_SIZE
                };
        }

        // Stop as soon as we find a ring below the choke threshold—this is
        // the CLOSEST exit from the base, which is what we want to defend.
        if (pass < CHOKE_THRESHOLD) break;
    }

    m_hasRealChoke = (bestPass < CHOKE_THRESHOLD);

    if (m_hasRealChoke) {
        m_chokePoint = bestCentroid;
        sf::Vector2f dir = m_chokePoint - m_basePos;
        float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        m_defenseDir = (len > 1.f) ? (dir / len) : enemyDir;
    } else {
        // Open map: place the defense anchor in the enemy direction.
        m_defenseDir = enemyDir;
        m_chokePoint = m_basePos + m_defenseDir * float(14 * Constants::TILE_SIZE);
    }

    m_defensePerp  = {-m_defenseDir.y, m_defenseDir.x};
    m_defenseReady = true;
}

// ---------------------------------------------------------------------------
// getDefenseRallySlot
// Slot 0 = centre of the defense line. Slots 1,2,3,4,… alternate outward
// (−1, +1, −2, +2 …) in tile-spacing units perpendicular to defenseDir.
// Real choke: straight perpendicular line behind the choke.
// Open map:   semicircle arc in front of the base, ±80° around defenseDir.
// ---------------------------------------------------------------------------
sf::Vector2f AIController::getDefenseRallySlot(int slotIndex) const {
    if (!m_defenseReady) {
        if (!m_player.getBuildings().empty())
            return m_player.getBuildings()[0]->getPosition();
        return {-1.f, -1.f};
    }

    // Map slot index to signed spread: 0→0, 1→−1, 2→+1, 3→−2, 4→+2, …
    int spread = (slotIndex == 0) ? 0
               : (slotIndex % 2 == 1) ? -((slotIndex + 1) / 2)
               :  (slotIndex / 2);

    if (m_hasRealChoke) {
        const float spacing = Constants::TILE_SIZE * 2.f;
        // Anchor 2 tiles behind the choke (toward own base).
        sf::Vector2f anchor = m_chokePoint - m_defenseDir * (Constants::TILE_SIZE * 2.f);
        return anchor + m_defensePerp * (float(spread) * spacing);
    } else {
        // Semicircle: spread slots as angles around defensedir.
        constexpr float PI       = 3.14159265f;
        constexpr float HALF_ARC = 80.f * PI / 180.f;  // ±80° in radians
        const float stepRad = HALF_ARC / 8.f;           // 8 slots per side
        const float radius  = 8.f * Constants::TILE_SIZE;
        float angle = std::atan2(m_defenseDir.y, m_defenseDir.x)
                    + float(spread) * stepRad;
        return m_basePos + sf::Vector2f{
            std::cos(angle) * radius,
            std::sin(angle) * radius
        };
    }
}

// ---------------------------------------------------------------------------
// findTurretLocation
// Returns a valid build tile for the turretIndex-th turret near the choke.
// Real choke: perpendicular spread at the choke tile.
// Open map:   angular spread along the semicircle arc.
// ---------------------------------------------------------------------------
sf::Vector2f AIController::findTurretLocation(int turretIndex) {
    if (!m_defenseReady) computeBaseDefense();
    if (!m_defenseReady) return findBuildLocation(EntityType::Turret);

    sf::Vector2i tileSize = ResourceManager::getBuildingSize(EntityType::Turret);

    // Map index to signed spread (same pattern as rally slots).
    auto spreadOf = [](int idx) -> int {
        if (idx == 0) return 0;
        return (idx % 2 == 1) ? -((idx + 1) / 2) : (idx / 2);
    };

    auto tryPlace = [&](sf::Vector2f worldPos) -> sf::Vector2f {
        sf::Vector2i tile = m_map->worldToTile(worldPos);
        if (m_map->canPlaceBuilding(tile.x, tile.y, tileSize.x, tileSize.y))
            return {float(tile.x) * Constants::TILE_SIZE,
                    float(tile.y) * Constants::TILE_SIZE};
        return {-1.f, -1.f};
    };

    if (m_hasRealChoke) {
        const float spacing = float(tileSize.x) * Constants::TILE_SIZE;
        for (int attempt = 0; attempt < 24; ++attempt) {
            int offset = spreadOf(turretIndex + attempt);
            sf::Vector2f pos = m_chokePoint + m_defensePerp * (float(offset) * spacing);
            sf::Vector2f result = tryPlace(pos);
            if (result.x >= 0.f) return result;
        }
    } else {
        constexpr float PI       = 3.14159265f;
        constexpr float HALF_ARC = 80.f * PI / 180.f;
        const float stepRad = HALF_ARC / 8.f;
        const float radius  = 10.f * Constants::TILE_SIZE;
        float baseAngle = std::atan2(m_defenseDir.y, m_defenseDir.x);
        for (int attempt = 0; attempt < 24; ++attempt) {
            int offset = spreadOf(turretIndex + attempt);
            float angle = baseAngle + float(offset) * stepRad;
            sf::Vector2f pos = m_basePos + sf::Vector2f{
                std::cos(angle) * radius,
                std::sin(angle) * radius
            };
            sf::Vector2f result = tryPlace(pos);
            if (result.x >= 0.f) return result;
        }
    }

    return findBuildLocation(EntityType::Turret);
}

// ---------------------------------------------------------------------------
// rallyNewCombatUnits
// Each tick, any non-worker, non-deployed, non-attack-group unit that hasn't
// been given a rally order yet is moved to the next slot on the defense line.
// ---------------------------------------------------------------------------
void AIController::rallyNewCombatUnits() {
    if (!m_defenseReady) return;

    for (const auto& unit : m_player.getUnits()) {
        if (!unit->isAlive()) continue;
        if (unit->getType() == EntityType::Worker) continue;
        if (m_deployedUnits.find(unit)    != m_deployedUnits.end())    continue;
        if (m_attackGroupUnits.find(unit) != m_attackGroupUnits.end()) continue;
        if (m_ralliedUnits.find(unit)     != m_ralliedUnits.end())     continue;

        int slotIndex = m_nextRallySlot % MAX_RALLY_SLOTS;
        ++m_nextRallySlot;

        sf::Vector2f dest = getDefenseRallySlot(slotIndex);
        if (dest.x >= 0.f)
            m_actions->move({std::static_pointer_cast<Entity>(unit)}, dest);

        m_ralliedUnits.insert(unit);
    }

    // Prune dead units so they don't linger in the set.
    for (auto it = m_ralliedUnits.begin(); it != m_ralliedUnits.end(); ) {
        if (!(*it) || !(*it)->isAlive()) it = m_ralliedUnits.erase(it);
        else ++it;
    }
}