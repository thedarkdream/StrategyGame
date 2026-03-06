#include "Player.h"
#include "Unit.h"
#include "Building.h"
#include "Entity.h"
#include <algorithm>

Player::Player(Team team, const Resources& startingResources)
    : m_team(team)
    , m_resources(startingResources)
{
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

void Player::addUnit(UnitPtr unit) {
    m_units.push_back(unit);
}

void Player::addBuilding(BuildingPtr building) {
    m_buildings.push_back(building);
}

void Player::removeUnit(UnitPtr unit) {
    auto it = std::find(m_units.begin(), m_units.end(), unit);
    if (it != m_units.end()) {
        m_units.erase(it);
    }
    
    // Also remove from selection
    auto selIt = std::find(m_selection.begin(), m_selection.end(), unit);
    if (selIt != m_selection.end()) {
        m_selection.erase(selIt);
    }
}

void Player::removeBuilding(BuildingPtr building) {
    auto it = std::find(m_buildings.begin(), m_buildings.end(), building);
    if (it != m_buildings.end()) {
        m_buildings.erase(it);
    }
    
    // Also remove from selection
    auto selIt = std::find(m_selection.begin(), m_selection.end(), building);
    if (selIt != m_selection.end()) {
        m_selection.erase(selIt);
    }
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

bool Player::isDefeated() const {
    // Defeated if no buildings and no units
    return m_buildings.empty() && m_units.empty();
}

void Player::update(float deltaTime) {
    // Update all units
    for (auto& unit : m_units) {
        unit->update(deltaTime);
    }
    
    // Update all buildings
    for (auto& building : m_buildings) {
        building->update(deltaTime);
    }
}

void Player::cleanupDeadEntities() {
    // Remove units that are ready for removal (dead and death animation finished)
    m_units.erase(
        std::remove_if(m_units.begin(), m_units.end(),
            [](const UnitPtr& unit) { return unit->isReadyForRemoval(); }),
        m_units.end()
    );
    
    // Remove destroyed buildings
    m_buildings.erase(
        std::remove_if(m_buildings.begin(), m_buildings.end(),
            [](const BuildingPtr& building) { return building->isReadyForRemoval(); }),
        m_buildings.end()
    );
    
    // Clean up selection (dying units should be deselected immediately)
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
    for (const auto& building : m_buildings) {
        if (building && building->isAlive() && 
            building->getType() == type && building->isConstructed()) {
            return true;
        }
    }
    return false;
}
