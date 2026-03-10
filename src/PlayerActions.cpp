#include "PlayerActions.h"
#include "Game.h"
#include "Player.h"
#include "Worker.h"
#include "Building.h"
#include "ResourceManager.h"
#include "EntityData.h"
#include "Constants.h"
#include "MathUtil.h"
#include <limits>

PlayerActions::PlayerActions(Player& player, Game& game)
    : m_player(player)
    , m_game(game)
{}

// ---------------------------------------------------------------------------
// Economy / Construction
// ---------------------------------------------------------------------------

bool PlayerActions::trainUnit(BuildingPtr building, EntityType type) {
    if (!building || !building->isConstructed()) return false;
    int mineralCost = ENTITY_DATA.getMineralCost(type);
    int gasCost     = ENTITY_DATA.getGasCost(type);
    if (!m_player.canAfford(mineralCost, gasCost)) return false;
    if (!building->trainUnit(type)) return false;
    m_player.spendResources(mineralCost, gasCost);
    return true;
}

bool PlayerActions::constructBuilding(EntityType type, sf::Vector2f worldPos, Worker* worker) {
    int mineralCost = ResourceManager::getMineralCost(type);
    if (!m_player.canAfford(mineralCost, 0)) return false;

    // Dependency check
    for (const auto& action : ENTITY_DATA.getActions(EntityType::Worker)) {
        if (action.type == ActionDef::Type::Build && action.producesType == type) {
            if (action.requires != EntityType::None &&
                !m_player.hasCompletedBuilding(action.requires)) return false;
            break;
        }
    }

    // Placement check
    sf::Vector2i tileSize = ResourceManager::getBuildingSize(type);
    int tileX = static_cast<int>(worldPos.x / Constants::TILE_SIZE);
    int tileY = static_cast<int>(worldPos.y / Constants::TILE_SIZE);
    if (!m_game.getMap().canPlaceBuilding(tileX, tileY, tileSize.x, tileSize.y))
        return false;

    // Worker pick
    if (!worker) worker = findNearestIdleWorker(worldPos);
    if (!worker) return false;

    // Commit
    m_player.spendResources(mineralCost, 0);
    sf::Vector2f pixelSize = ENTITY_DATA.getSize(type);
    sf::Vector2f buildPos(
        tileX * Constants::TILE_SIZE + pixelSize.x / 2.0f,
        tileY * Constants::TILE_SIZE + pixelSize.y / 2.0f
    );
    EntityPtr newBuilding = m_game.spawnBuilding(type, m_player.getTeam(), buildPos, false);
    if (newBuilding) worker->buildAt(newBuilding);
    return newBuilding != nullptr;
}

void PlayerActions::continueConstruction(EntityPtr building, const std::vector<EntityPtr>& units) {
    if (!building) return;
    for (const auto& entity : units) {
        if (entity && entity->isAlive()) {
            if (Worker* w = entity->asWorker())
                w->buildAt(building);
        }
    }
}

void PlayerActions::cancelConstruction(EntityPtr entity) {
    if (!entity) return;
    Building* building = entity->asBuilding();
    if (!building || building->isConstructed()) return;

    building->releaseBuilder();

    sf::Vector2i bSize = ResourceManager::getBuildingSize(building->getType());
    sf::Vector2i tile = MathUtil::buildingTileOrigin(building->getPosition(), bSize, Constants::TILE_SIZE);
    m_game.getMap().removeBuilding(tile.x, tile.y, bSize.x, bSize.y);

    m_player.addResources(
        ENTITY_DATA.getMineralCost(building->getType()),
        ENTITY_DATA.getGasCost(building->getType()));
    m_player.clearSelection();
    m_game.removeEntity(entity);
}

// ---------------------------------------------------------------------------
// Unit orders
// ---------------------------------------------------------------------------

void PlayerActions::move(const std::vector<EntityPtr>& units, sf::Vector2f target) {
    for (const auto& e : units)
        if (auto* u = e->asUnit()) u->moveTo(target);
}

void PlayerActions::follow(const std::vector<EntityPtr>& units, EntityPtr target) {
    if (m_isLocal && target) target->startHighlight();
    for (const auto& e : units)
        if (auto* u = e->asUnit(); u && e != target) u->follow(target);
}

void PlayerActions::attack(const std::vector<EntityPtr>& units, EntityPtr target) {
    if (m_isLocal && target) target->startHighlight();
    for (const auto& e : units)
        if (auto* u = e->asUnit()) u->attack(target);
}

void PlayerActions::attackMove(const std::vector<EntityPtr>& units, sf::Vector2f target) {
    for (const auto& e : units)
        if (auto* u = e->asUnit()) u->attackMoveTo(target);
}

void PlayerActions::gather(const std::vector<EntityPtr>& units, EntityPtr resource) {
    if (m_isLocal && resource) resource->startHighlight();
    for (const auto& e : units)
        if (Worker* w = e->asWorker()) w->gather(resource);
}

void PlayerActions::stop(const std::vector<EntityPtr>& units) {
    for (const auto& e : units)
        if (auto* u = e->asUnit()) u->stop();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

Worker* PlayerActions::findNearestIdleWorker(sf::Vector2f pos) const {
    Worker* best = nullptr;
    float   bestDist = std::numeric_limits<float>::max();
    for (const auto& entity : m_game.getAllEntities()) {
        if (!entity || !entity->isAlive()) continue;
        if (entity->getTeam() != m_player.getTeam()) continue;
        if (entity->getType() != EntityType::Worker) continue;
        Worker* w = entity->asWorker();
        if (!w || w->isBuilding()) continue;
        float dist = MathUtil::distance(w->getPosition(), pos);
        if (dist < bestDist) { bestDist = dist; best = w; }
    }
    return best;
}
