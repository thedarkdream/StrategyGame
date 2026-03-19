#include "Player.h"
#include "Unit.h"
#include "Building.h"
#include "Entity.h"
#include "Map.h"
#include "EntityWorld.h"
#include <algorithm>

Player::Player(Team team, const Resources& startingResources)
    : m_team(team)
    , m_resources(startingResources)
{
}

// ---------------------------------------------------------------------------
// Entity sets — computed live from EntityWorld filtered by team
// ---------------------------------------------------------------------------
UnitList Player::getUnits() const {
    UnitList result;
    if (!m_world) return result;
    for (const auto& e : m_world->all()) {
        if (!e || !e->isAlive() || e->getTeam() != m_team) continue;
        if (auto u = std::dynamic_pointer_cast<Unit>(e))
            result.push_back(u);
    }
    return result;
}

BuildingList Player::getBuildings() const {
    BuildingList result;
    if (!m_world) return result;
    for (const auto& e : m_world->all()) {
        if (!e || !e->isAlive() || e->getTeam() != m_team) continue;
        if (auto b = std::dynamic_pointer_cast<Building>(e))
            result.push_back(b);
    }
    return result;
}

void Player::addResources(int minerals, int gas) {
    m_resources.minerals += minerals;
    m_resources.gas += gas;
}

bool Player::spendResources(int minerals, int gas) {
    if (!canAfford(minerals, gas)) {
        return false;
    }
    
    m_resources.minerals -= minerals;
    m_resources.gas -= gas;
    return true;
}

bool Player::canAfford(int minerals, int gas) const {
    return m_resources.minerals >= minerals && m_resources.gas >= gas;
}

void Player::clearSelection() {
    for (auto& entity : m_selection) {
        if (entity) {
            entity->setSelected(false);
        }
    }
    m_selection.clear();
}

void Player::selectEntity(EntityPtr entity) {
    clearSelection();
    if (entity) {
        m_selection.push_back(entity);
        entity->setSelected(true);
    }
}

void Player::selectEntities(const std::vector<EntityPtr>& entities) {
    clearSelection();
    for (auto& entity : entities) {
        if (entity && entity->getTeam() == m_team) {
            m_selection.push_back(entity);
            entity->setSelected(true);
        }
    }
}

// ---------------------------------------------------------------------------
bool Player::isDefeated() const {
    // Defeated when no alive buildings and no alive units in the world.
    if (!m_world) return false;
    for (const auto& e : m_world->all()) {
        if (!e || !e->isAlive() || e->getTeam() != m_team) continue;
        if (e->asUnit() || e->asBuilding()) return false;
    }
    return true;
}

int Player::getUnitCount() const {
    return static_cast<int>(getUnits().size());
}

int Player::getBuildingCount() const {
    return static_cast<int>(getBuildings().size());
}

void Player::update(float deltaTime) {
    // Entity updates are handled centrally by Game iterating m_allEntities.
    (void)deltaTime;
}

void Player::cleanupSelection() {
    // Deselect any entity whose health has reached zero (includes m_isDying units
    // that haven't been fully removed yet — we want the HUD to react immediately).
    m_selection.erase(
        std::remove_if(m_selection.begin(), m_selection.end(),
            [](const EntityPtr& entity) { return !entity || !entity->isAlive(); }),
        m_selection.end()
    );
}

EntityPtr Player::getFirstSelectedEntity() const {
    if (m_selection.empty()) return nullptr;
    EntityPtr entity = m_selection[0];
    if (!entity || !entity->isAlive()) return nullptr;
    return entity;
}

EntityPtr Player::getFirstOwnedSelectedEntity() const {
    EntityPtr entity = getFirstSelectedEntity();
    if (!entity || entity->getTeam() != m_team) return nullptr;
    return entity;
}

bool Player::hasCompletedBuilding(EntityType type) const {
    if (!m_world) return false;
    for (const auto& e : m_world->all()) {
        if (!e || !e->isAlive() || e->getTeam() != m_team) continue;
        const Building* b = e->asBuilding();
        if (b && b->getType() == type && b->isConstructed()) return true;
    }
    return false;
}

void Player::updateFog(const Map& map, const EntityList& allEntities) {
    m_fog.update(getUnits(), getBuildings(), map);
    m_fog.recordGhosts(allEntities, map);
    m_fog.rebuildTexture();
}
