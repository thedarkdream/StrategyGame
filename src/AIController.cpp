#include "AIController.h"
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
        if (auto base = findIdleBase())
            m_actions->trainUnit(base, EntityType::Worker);
    }

    // Send idle workers to gather
    for (auto& unit : m_player.getUnits()) {
        if (unit->getType() == EntityType::Worker && unit->isIdle()) {
            EntityPtr nearestMineral;
            float nearestDist = 999999.0f;
            for (auto& entity : m_game.getAllEntities()) {
                if (entity->getType() == EntityType::MineralPatch) {
                    float dist = MathUtil::distance(entity->getPosition(), unit->getPosition());
                    if (dist < nearestDist) { nearestDist = dist; nearestMineral = entity; }
                }
            }
            if (nearestMineral)
                m_actions->gather({ std::static_pointer_cast<Entity>(unit) }, nearestMineral);
        }
    }
}

void AIController::manageProduction() {
    int barracksCount = countBuildingsOfType(EntityType::Barracks);
    int soldierCount = countUnitsOfType(EntityType::Soldier);

    // Build barracks if we have none (including under construction)
    if (barracksCount < 1) {
        sf::Vector2f buildPos = findBuildLocation(EntityType::Barracks);
        if (buildPos.x >= 0.f && buildPos.y >= 0.f)
            m_actions->constructBuilding(EntityType::Barracks, buildPos, findIdleWorker());
    }

    // Train soldiers or brutes
    if (barracksCount > 0) {
        if (auto barracks = findIdleBarracks()) {
            std::uniform_int_distribution<int> dist(1, 10);
            if (dist(m_rng) <= 6)
                m_actions->trainUnit(barracks, EntityType::Soldier);
            else
                m_actions->trainUnit(barracks, EntityType::Brute);
        }
    }
}

void AIController::manageArmy() {
    int soldierCount = countUnitsOfType(EntityType::Soldier);
    int bruteCount = countUnitsOfType(EntityType::Brute);
    int armySize = soldierCount + bruteCount;
    
    // Attack when we have enough combat units
    int attackThreshold = 3 + m_difficulty * 2;
    
    if (armySize >= attackThreshold) {
        attackEnemy();
    }
}

void AIController::sendScout() {
    // TODO: Implement scouting behavior
}

void AIController::attackEnemy() {
    // Collect all enemy bases (prefer Base buildings; fall back to any enemy building)
    std::vector<sf::Vector2f> enemyBasePositions;
    for (const auto& entity : m_game.getAllEntities()) {
        if (!entity || !entity->isAlive()) continue;
        if (entity->getTeam() == m_player.getTeam() || entity->getTeam() == Team::Neutral) continue;
        if (entity->getType() == EntityType::Base)
            enemyBasePositions.push_back(entity->getPosition());
    }

    // Fallback: rally on any enemy building if no bases remain
    if (enemyBasePositions.empty()) {
        for (const auto& entity : m_game.getAllEntities()) {
            if (!entity || !entity->isAlive()) continue;
            if (entity->getTeam() == m_player.getTeam() || entity->getTeam() == Team::Neutral) continue;
            if (entity->asBuilding())
                enemyBasePositions.push_back(entity->getPosition());
        }
    }

    if (enemyBasePositions.empty()) return;

    // Pick a random enemy base to march toward
    std::uniform_int_distribution<int> pick(0, static_cast<int>(enemyBasePositions.size()) - 1);
    sf::Vector2f targetPos = enemyBasePositions[pick(m_rng)];

    // Issue attack-move so units fight enemies encountered on the way
    std::vector<EntityPtr> attackers;
    for (auto& unit : m_player.getUnits()) {
        if (unit->getType() != EntityType::Worker && unit->isIdle())
            attackers.push_back(std::static_pointer_cast<Entity>(unit));
    }
    if (!attackers.empty())
        m_actions->attackMove(attackers, targetPos);
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
    // Count both completed and under-construction buildings to avoid duplicate orders
    int count = 0;
    for (auto& building : m_player.getBuildings()) {
        if (building->getType() == type && building->isAlive()) {
            ++count;
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
    // so we can place buildings on the opposite side, away from the mining area.
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

    // Two passes: first avoid the mineral-facing side, then accept any free spot as fallback.
    for (int pass = 0; pass < 2; ++pass) {
        for (int radius = 3; radius < 12; ++radius) {
            for (int dx = -radius; dx <= radius; ++dx) {
                for (int dy = -radius; dy <= radius; ++dy) {
                    if (std::abs(dx) != radius && std::abs(dy) != radius) continue;

                    int tileX = baseTileX + dx;
                    int tileY = baseTileY + dy;

                    if (!m_map->canPlaceBuilding(tileX, tileY, buildingSize.x, buildingSize.y))
                        continue;

                    // On the first pass, skip tiles that face toward the minerals.
                    if (pass == 0 && mineralCount > 0) {
                        sf::Vector2f dir(static_cast<float>(dx), static_cast<float>(dy));
                        float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
                        if (len > 0.f) dir /= len;
                        // dot > 0.3 means the tile is within ~72° of the mineral direction.
                        if (dir.x * mineralDir.x + dir.y * mineralDir.y > 0.3f) continue;
                    }

                    // Return the top-left corner of the tile; constructBuilding
                    // derives tileX/Y via integer division and adds centering itself.
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
                if (!w->isBuilding())
                    return w;
            }
        }
    }
    return nullptr;
}

