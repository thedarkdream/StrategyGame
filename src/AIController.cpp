#include "AIController.h"
#include "Game.h"
#include "Player.h"
#include "Unit.h"
#include "Worker.h"
#include "Building.h"
#include "Map.h"
#include "ResourceManager.h"
#include "Constants.h"
#include <algorithm>

AIController::AIController(Player& player, Game& game)
    : m_player(player)
    , m_game(game)
    , m_map(&game.getMap())
{
    std::random_device rd;
    m_rng = std::mt19937(rd());
}

void AIController::update(float deltaTime) {
    m_decisionTimer += deltaTime;
    
    // Make decisions periodically
    float interval = m_decisionInterval / static_cast<float>(m_difficulty);
    if (m_decisionTimer >= interval) {
        m_decisionTimer = 0.0f;
        makeDecision();
    }
}

void AIController::makeDecision() {
    // Simple priority-based AI
    manageEconomy();
    manageProduction();
    manageArmy();
}

void AIController::manageEconomy() {
    // Ensure we have enough workers
    int workerCount = countUnitsOfType(EntityType::Worker);
    int baseCount = countBuildingsOfType(EntityType::Base);
    
    // Ideal: 2-3 workers per mineral patch, let's aim for 6 workers per base
    int idealWorkers = baseCount * 6;
    
    if (workerCount < idealWorkers) {
        auto base = findIdleBase();
        if (base && m_player.canAfford(Constants::WORKER_COST_MINERALS, 0)) {
            base->trainUnit(EntityType::Worker);
            m_player.spendResources(Constants::WORKER_COST_MINERALS, 0);
        }
    }
    
    // Send idle workers to gather
    for (auto& unit : m_player.getUnits()) {
        if (unit->getType() == EntityType::Worker && unit->isIdle()) {
            // Find nearest mineral patch
            EntityPtr nearestMineral = nullptr;
            float nearestDist = 999999.0f;
            
            for (auto& entity : m_game.getAllEntities()) {
                if (entity->getType() == EntityType::MineralPatch) {
                    sf::Vector2f diff = entity->getPosition() - unit->getPosition();
                    float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);
                    if (dist < nearestDist) {
                        nearestDist = dist;
                        nearestMineral = entity;
                    }
                }
            }
            
            if (nearestMineral) {
                if (auto* worker = dynamic_cast<Worker*>(unit.get())) {
                    worker->gather(nearestMineral);
                }
            }
        }
    }
}

void AIController::manageProduction() {
    int barracksCount = countBuildingsOfType(EntityType::Barracks);
    int soldierCount = countUnitsOfType(EntityType::Soldier);
    
    // Build barracks if we have none and can afford it
    if (barracksCount < 1 && m_player.canAfford(Constants::BARRACKS_COST_MINERALS, 0)) {
        sf::Vector2f buildPos = findBuildLocation(EntityType::Barracks);
        if (buildPos.x > 0) {
            // TODO: Implement build command for AI
            // For now, we'll directly spawn the barracks
            m_game.spawnBuilding(EntityType::Barracks, m_player.getTeam(), buildPos);
            m_player.spendResources(Constants::BARRACKS_COST_MINERALS, 0);
        }
    }
    
    // Train soldiers
    if (barracksCount > 0) {
        auto barracks = findIdleBarracks();
        if (barracks && m_player.canAfford(Constants::SOLDIER_COST_MINERALS, 0)) {
            barracks->trainUnit(EntityType::Soldier);
            m_player.spendResources(Constants::SOLDIER_COST_MINERALS, 0);
        }
    }
}

void AIController::manageArmy() {
    int soldierCount = countUnitsOfType(EntityType::Soldier);
    
    // Attack when we have enough soldiers
    int attackThreshold = 3 + m_difficulty * 2;
    
    if (soldierCount >= attackThreshold) {
        attackEnemy();
    }
}

void AIController::sendScout() {
    // TODO: Implement scouting behavior
}

void AIController::attackEnemy() {
    // Find an enemy target
    EntityPtr target = findNearestEnemy(m_player.getBuildings()[0]->getPosition());
    
    if (!target) return;
    
    // Send all soldiers to attack
    for (auto& unit : m_player.getUnits()) {
        if (unit->getType() == EntityType::Soldier && unit->isIdle()) {
            unit->attack(target);
        }
    }
}

BuildingPtr AIController::findIdleBase() {
    for (auto& building : m_player.getBuildings()) {
        if (building->getType() == EntityType::Base && 
            building->isConstructed() && !building->isProducing()) {
            return building;
        }
    }
    return nullptr;
}

BuildingPtr AIController::findIdleBarracks() {
    for (auto& building : m_player.getBuildings()) {
        if (building->getType() == EntityType::Barracks && 
            building->isConstructed() && !building->isProducing()) {
            return building;
        }
    }
    return nullptr;
}

int AIController::countUnitsOfType(EntityType type) {
    int count = 0;
    for (auto& unit : m_player.getUnits()) {
        if (unit->getType() == type) {
            ++count;
        }
    }
    return count;
}

int AIController::countBuildingsOfType(EntityType type) {
    int count = 0;
    for (auto& building : m_player.getBuildings()) {
        if (building->getType() == type && building->isConstructed()) {
            ++count;
        }
    }
    return count;
}

sf::Vector2f AIController::findBuildLocation(EntityType buildingType) {
    // Find a location near our base
    if (m_player.getBuildings().empty()) {
        return sf::Vector2f(-1, -1);
    }
    
    sf::Vector2f basePos = m_player.getBuildings()[0]->getPosition();
    sf::Vector2i buildingSize = ResourceManager::getBuildingSize(buildingType);
    
    // Search in a spiral pattern around the base
    for (int radius = 3; radius < 10; ++radius) {
        for (int dx = -radius; dx <= radius; ++dx) {
            for (int dy = -radius; dy <= radius; ++dy) {
                if (std::abs(dx) != radius && std::abs(dy) != radius) continue;
                
                int tileX = static_cast<int>(basePos.x / Constants::TILE_SIZE) + dx;
                int tileY = static_cast<int>(basePos.y / Constants::TILE_SIZE) + dy;
                
                if (m_map->canPlaceBuilding(tileX, tileY, buildingSize.x, buildingSize.y)) {
                    return m_map->tileToWorldCenter(
                        tileX + buildingSize.x / 2, 
                        tileY + buildingSize.y / 2
                    );
                }
            }
        }
    }
    
    return sf::Vector2f(-1, -1);
}

EntityPtr AIController::findNearestEnemy(sf::Vector2f from) {
    EntityPtr nearest = nullptr;
    float nearestDist = 999999.0f;
    
    Player& enemy = m_game.getPlayer();  // AI is the "enemy" player, so player is our enemy
    
    // Check enemy units
    for (auto& unit : enemy.getUnits()) {
        if (unit->isAlive()) {
            sf::Vector2f diff = unit->getPosition() - from;
            float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);
            if (dist < nearestDist) {
                nearestDist = dist;
                nearest = unit;
            }
        }
    }
    
    // Check enemy buildings if no units found
    if (!nearest) {
        for (auto& building : enemy.getBuildings()) {
            if (building->isAlive()) {
                sf::Vector2f diff = building->getPosition() - from;
                float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);
                if (dist < nearestDist) {
                    nearestDist = dist;
                    nearest = building;
                }
            }
        }
    }
    
    return nearest;
}
