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

AIController::AIController(Player& player, Game& game)
    : m_player(player)
    , m_game(game)
    , m_map(&game.getMap())
    , m_actions(&game.getActions(teamToIndex(player.getTeam())))
{
    std::random_device rd;
    m_rng = std::mt19937(rd());
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
    m_attackGroupNeeded.clear();
    m_attackGroupUnits.clear();
    
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
            // Check if we have enough of this unit type
            // Find the pending train for this command
            for (const auto& pt : m_pendingTrains) {
                if (pt.unitType == cmd.entityType && pt.remaining > 0) {
                    return false;  // Still training
                }
            }
            return true;
        }
        
        case AICommand::Type::Build: {
            // Check if building exists (complete or under construction)
            for (const auto& pb : m_pendingBuilds) {
                if (pb.buildingType == cmd.entityType && !pb.started) {
                    return false;  // Haven't started yet
                }
            }
            // Check if any building of this type exists
            return countBuildingsOfType(cmd.entityType, true) > 0;
        }
        
        case AICommand::Type::AttackAdd: {
            // Check if we have enough units reserved
            auto it = m_attackGroupNeeded.find(cmd.entityType);
            if (it != m_attackGroupNeeded.end() && it->second > 0) {
                // Count how many we have reserved
                int reserved = 0;
                for (const auto& unit : m_attackGroupUnits) {
                    if (unit && unit->isAlive() && unit->getType() == cmd.entityType) {
                        reserved++;
                    }
                }
                // Count total available (not in attack group)
                int available = 0;
                for (const auto& unit : m_player.getUnits()) {
                    if (unit->getType() == cmd.entityType && unit->isAlive()) {
                        if (m_attackGroupUnits.find(unit) == m_attackGroupUnits.end()) {
                            available++;
                        }
                    }
                }
                // Try to reserve more units
                int needed = it->second;
                for (const auto& unit : m_player.getUnits()) {
                    if (needed <= 0) break;
                    if (unit->getType() == cmd.entityType && unit->isAlive()) {
                        if (m_attackGroupUnits.find(unit) == m_attackGroupUnits.end()) {
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
    }
}

void AIController::handleTrain(const AICommand& cmd) {
    // Add to pending trains
    m_pendingTrains.push_back({cmd.entityType, cmd.count});
}

void AIController::handleBuild(const AICommand& cmd) {
    // Add to pending builds
    m_pendingBuilds.push_back({cmd.entityType, false});
}

void AIController::handleAttackAdd(const AICommand& cmd) {
    // Accumulate needed units for attack
    m_attackGroupNeeded[cmd.entityType] += cmd.count;
}

void AIController::handleAttack() {
    sf::Vector2f targetPos = findEnemyBase();
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
    
    // Clear attack group for next attack
    m_attackGroupUnits.clear();
    m_attackGroupNeeded.clear();
}

void AIController::manageIdleWorkers() {
    // Send idle workers to gather
    for (auto& unit : m_player.getUnits()) {
        if (unit->getType() == EntityType::Worker && unit->isIdle()) {
            EntityPtr nearestMineral;
            float nearestDist = 999999.0f;
            for (auto& entity : m_game.getAllEntities()) {
                if (entity->getType() == EntityType::MineralPatch) {
                    float dist = MathUtil::distance(entity->getPosition(), unit->getPosition());
                    if (dist < nearestDist) {
                        nearestDist = dist;
                        nearestMineral = entity;
                    }
                }
            }
            if (nearestMineral) {
                m_actions->gather({std::static_pointer_cast<Entity>(unit)}, nearestMineral);
            }
        }
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

void AIController::processBuildQueue() {
    // Process pending build commands
    for (auto& pb : m_pendingBuilds) {
        if (!pb.started) {
            // Check if building already exists (e.g. built via a different path)
            if (countBuildingsOfType(pb.buildingType, true) > 0) {
                pb.started = true;
                continue;
            }
            
            // Try to start construction
            if (m_player.canAfford(ENTITY_DATA.getMineralCost(pb.buildingType),
                                   ENTITY_DATA.getGasCost(pb.buildingType))) {
                sf::Vector2f buildPos = findBuildLocation(pb.buildingType);
                Worker* worker = findIdleWorker();
                
                if (buildPos.x >= 0.f && worker) {
                    m_actions->constructBuilding(pb.buildingType, buildPos, worker);
                    pb.started = true;
                    pb.assignedWorker = worker;
                }
            }
        } else {
            // Build was issued — check if the assigned worker has abandoned the job
            // (e.g. got stuck coming from a resource node, became idle, was re-tasked)
            if (pb.assignedWorker && pb.assignedWorker->isAlive() &&
                !pb.assignedWorker->isBuilding()) {
                // Find the incomplete building and re-assign a worker to it
                for (auto& bld : m_player.getBuildings()) {
                    if (bld->getType() == pb.buildingType && !bld->isConstructed()) {
                        Worker* newWorker = findIdleWorker();
                        if (newWorker) {
                            // Find the matching EntityPtr from the unit list
                            for (auto& unit : m_player.getUnits()) {
                                if (unit.get() == newWorker) {
                                    m_actions->continueConstruction(
                                        std::static_pointer_cast<Entity>(bld),
                                        {std::static_pointer_cast<Entity>(unit)});
                                    pb.assignedWorker = newWorker;
                                    break;
                                }
                            }
                        }
                        break;
                    }
                }
            }
        }
    }
    
    // Clean up completed builds
    m_pendingBuilds.erase(
        std::remove_if(m_pendingBuilds.begin(), m_pendingBuilds.end(),
            [this](const PendingBuild& pb) {
                return pb.started && countBuildingsOfType(pb.buildingType, true) > 0;
            }),
        m_pendingBuilds.end()
    );
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
    int count = 0;
    for (auto& unit : m_player.getUnits()) {
        if (unit->getType() == type && unit->isAlive()) {
            ++count;
        }
    }
    return count;
}

int AIController::countBuildingsOfType(EntityType type, bool includeIncomplete) {
    int count = 0;
    for (auto& building : m_player.getBuildings()) {
        if (building->getType() == type && building->isAlive()) {
            if (includeIncomplete || building->isConstructed()) {
                ++count;
            }
        }
    }
    return count;
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
    for (auto& unit : m_player.getUnits()) {
        if (unit->getType() == EntityType::Worker && unit->isAlive()) {
            if (Worker* w = unit->asWorker()) {
                if (!w->isBuilding() && w->isIdle())
                    return w;
            }
        }
    }
    // Fallback: any worker not actively building
    for (auto& unit : m_player.getUnits()) {
        if (unit->getType() == EntityType::Worker && unit->isAlive()) {
            if (Worker* w = unit->asWorker()) {
                if (!w->isBuilding())
                    return w;
            }
        }
    }
    return nullptr;
}

sf::Vector2f AIController::findEnemyBase() {
    // Find enemy base positions
    std::vector<sf::Vector2f> enemyBasePositions;
    
    for (const auto& entity : m_game.getAllEntities()) {
        if (!entity || !entity->isAlive()) continue;
        if (entity->getTeam() == m_player.getTeam() || entity->getTeam() == Team::Neutral) continue;
        if (entity->getType() == EntityType::Base)
            enemyBasePositions.push_back(entity->getPosition());
    }

    // Fallback to any enemy building
    if (enemyBasePositions.empty()) {
        for (const auto& entity : m_game.getAllEntities()) {
            if (!entity || !entity->isAlive()) continue;
            if (entity->getTeam() == m_player.getTeam() || entity->getTeam() == Team::Neutral) continue;
            if (entity->asBuilding())
                enemyBasePositions.push_back(entity->getPosition());
        }
    }

    if (enemyBasePositions.empty()) {
        return sf::Vector2f(-1, -1);
    }

    // Pick a random enemy base
    std::uniform_int_distribution<int> pick(0, static_cast<int>(enemyBasePositions.size()) - 1);
    return enemyBasePositions[pick(m_rng)];
}

