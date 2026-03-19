#include "EntityWorld.h"
#include "Entity.h"
#include "Unit.h"
#include "Building.h"
#include "ResourceNode.h"
#include "MathUtil.h"
#include "Constants.h"
#include <algorithm>
#include <array>
#include <cmath>

// ---- Entity list management -----------------------------------------------

void EntityWorld::add(EntityPtr entity) {
    m_pending.push_back(entity);
}

void EntityWorld::remove(EntityPtr entity) {
    auto it = std::find(m_all.begin(), m_all.end(), entity);
    if (it != m_all.end())
        m_all.erase(it);

    auto pit = std::find(m_pending.begin(), m_pending.end(), entity);
    if (pit != m_pending.end())
        m_pending.erase(pit);
}

void EntityWorld::flush() {
    if (m_pending.empty()) return;
    m_all.insert(m_all.end(),
        std::make_move_iterator(m_pending.begin()),
        std::make_move_iterator(m_pending.end()));
    m_pending.clear();
}

EntityList EntityWorld::extractRemovable() {
    EntityList removed;

    for (const auto& entity : m_all)
        if (entity && entity->isReadyForRemoval())
            removed.push_back(entity);

    m_all.erase(
        std::remove_if(m_all.begin(), m_all.end(),
            [](const EntityPtr& e) { return !e || e->isReadyForRemoval(); }),
        m_all.end());

    return removed;
}

// ---- Spatial queries -------------------------------------------------------

EntityPtr EntityWorld::getAt(sf::Vector2f pos) const {
    for (const auto& entity : m_all) {
        if (!entity || !entity->isAlive()) continue;
        if (entity->getType() == EntityType::Rocket) continue;
        if (entity->getBounds().contains(pos))
            return entity;
    }
    return nullptr;
}

std::vector<EntityPtr> EntityWorld::getInRect(sf::FloatRect rect) const {
    std::vector<EntityPtr> result;
    for (const auto& entity : m_all) {
        if (entity && entity->isAlive() &&
            rect.findIntersection(entity->getBounds()).has_value())
            result.push_back(entity);
    }
    return result;
}

std::vector<EntityPtr> EntityWorld::getInRect(sf::FloatRect rect, Team team) const {
    std::vector<EntityPtr> result;
    for (const auto& entity : m_all) {
        if (entity && entity->isAlive() &&
            entity->getTeam() == team &&
            rect.findIntersection(entity->getBounds()).has_value())
            result.push_back(entity);
    }
    return result;
}

EntityPtr EntityWorld::findNearestEnemy(sf::Vector2f pos, float radius, Team excludeTeam) const {
    return findNearest(pos, radius, [excludeTeam](const EntityPtr& entity) {
        return entity->getTeam() != excludeTeam
            && entity->getTeam() != Team::Neutral
            && entity->getType() != EntityType::Rocket;
    });
}

EntityPtr EntityWorld::findPriorityEnemy(sf::Vector2f pos, float radius, Team excludeTeam) const {
    // Priority: combat unit > non-combat unit > building
    EntityPtr bestCombat, bestWorker, bestBuilding;
    float distCombat = radius, distWorker = radius, distBuilding = radius;

    for (const auto& entity : m_all) {
        if (!entity || !entity->isAlive()) continue;
        if (entity->getTeam() == excludeTeam || entity->getTeam() == Team::Neutral) continue;
        if (entity->getType() == EntityType::Rocket) continue;

        float dist = MathUtil::distance(entity->getPosition(), pos);
        if (dist >= radius) continue;

        if (entity->asBuilding()) {
            if (dist < distBuilding) { distBuilding = dist; bestBuilding = entity; }
        } else if (entity->asUnit()) {
            if (entity->asUnit()->isCombatUnit()) {
                if (dist < distCombat) { distCombat = dist; bestCombat = entity; }
            } else {
                if (dist < distWorker) { distWorker = dist; bestWorker = entity; }
            }
        }
    }

    if (bestCombat)  return bestCombat;
    if (bestWorker)  return bestWorker;
    return bestBuilding;
}

EntityPtr EntityWorld::findNearestResource(sf::Vector2f pos, float radius) const {
    return findNearest(pos, radius, [](const EntityPtr& entity) {
        return entity->isResource();
    });
}

EntityPtr EntityWorld::findNearestAvailableResource(sf::Vector2f pos, float radius, EntityPtr exclude) const {
    return findNearest(pos, radius, [exclude](const EntityPtr& entity) {
        if (!entity->isResource()) return false;
        if (entity == exclude) return false;
        if (auto* node = entity->asResourceNode())
            if (node->isBeingMined()) return false;
        return true;
    });
}

EntityPtr EntityWorld::findHomeBase(Team team) const {
    for (const auto& entity : m_all) {
        if (entity && entity->isAlive()
                && entity->getTeam() == team
                && entity->getType() == EntityType::Base)
            return entity;
    }
    return nullptr;
}

// ---- Collision / free-position helpers ------------------------------------

bool EntityWorld::checkPositionBlocked(sf::Vector2f pos, float radius, Entity* excludeSelf) const {
    constexpr float CORNER_INSET = 2.0f;

    auto insetContains = [&](const Entity* e) -> bool {
        sf::FloatRect b = e->getBounds();
        b.position.x += CORNER_INSET;
        b.position.y += CORNER_INSET;
        b.size.x     -= CORNER_INSET * 2.0f;
        b.size.y     -= CORNER_INSET * 2.0f;
        return b.contains(pos);
    };

    for (const EntityList* list : {&m_all, &m_pending}) {
        for (const auto& entity : *list) {
            if (!entity || !entity->isAlive()) continue;
            if (entity.get() == excludeSelf)  continue;

            if (auto* unit = entity->asUnit()) {
                if (!unit->isCollidable()) continue;
                float dist = MathUtil::distance(entity->getPosition(), pos);
                if (dist < radius + unit->getCollisionRadius())
                    return true;
            } else if (entity->asBuilding() || entity->asResourceNode()) {
                if (insetContains(entity.get()))
                    return true;
            }
        }
    }
    return false;
}

sf::Vector2f EntityWorld::findFreePosition(sf::Vector2f pos, float radius,
                                            float maxSearchRadius, Entity* excludeSelf) const {
    if (!checkPositionBlocked(pos, radius, excludeSelf))
        return pos;
    float stepRadius = radius * 2.0f;
    int maxRings = static_cast<int>(maxSearchRadius / stepRadius) + 1;
    return findNearestFreePosition(pos, radius, maxRings, excludeSelf);
}

sf::Vector2f EntityWorld::findSpawnPosition(sf::Vector2f origin, float unitRadius) const {
    if (!checkPositionBlocked(origin, unitRadius, nullptr))
        return origin;
    return findNearestFreePosition(origin, unitRadius, 5, nullptr);
}

sf::Vector2f EntityWorld::findNearestFreePosition(sf::Vector2f pos, float radius,
                                                    int maxRings, Entity* excludeSelf) const {
    float stepRadius = radius * 2.0f;
    for (int ring = 1; ring <= maxRings; ++ring) {
        float ringRadius = stepRadius * ring;
        int   points     = 8 * ring;
        for (int i = 0; i < points; ++i) {
            float angle    = (2.0f * MathUtil::PI * i) / points;
            sf::Vector2f testPos = pos + sf::Vector2f(
                std::cos(angle) * ringRadius,
                std::sin(angle) * ringRadius);
            if (!checkPositionBlocked(testPos, radius, excludeSelf))
                return testPos;
        }
    }
    return pos;  // fallback: return original if no free spot found
}

std::vector<RVONeighbor> EntityWorld::getNearbyUnitsRVO(sf::Vector2f pos, float radius,
                                                         Unit* excludeSelf) const {
    std::vector<RVONeighbor> result;
    for (const auto& entity : m_all) {
        if (!entity || !entity->isAlive()) continue;
        auto* unit = entity->asUnit();
        if (!unit || unit == excludeSelf || !unit->isCollidable()) continue;

        float dist = MathUtil::distance(entity->getPosition(), pos);
        if (dist < radius) {
            RVONeighbor neighbor;
            neighbor.position = unit->getPosition();
            neighbor.velocity = unit->getVelocity();
            neighbor.radius   = unit->getCollisionRadius();
            result.push_back(neighbor);
        }
    }
    return result;
}

// ---- Per-frame physics -----------------------------------------------------

void EntityWorld::pushUnitsOutOfBuildings(float deltaTime) {
    constexpr float PUSH_SPEED = 150.0f;
    constexpr float INSET      = 2.0f;

    for (auto& entity : m_all) {
        if (!entity || !entity->isAlive()) continue;
        Unit* unit = entity->asUnit();
        if (!unit || !unit->isCollidable()) continue;

        sf::Vector2f pos = unit->getPosition();

        for (const auto& other : m_all) {
            if (!other || !other->isAlive()) continue;
            if (!other->asBuilding()) continue;

            sf::FloatRect bounds = other->getBounds();
            sf::FloatRect inner  = bounds;
            inner.position.x += INSET;
            inner.position.y += INSET;
            inner.size.x     -= INSET * 2.0f;
            inner.size.y     -= INSET * 2.0f;

            if (!inner.contains(pos)) continue;

            float dLeft   = pos.x - bounds.position.x;
            float dRight  = (bounds.position.x + bounds.size.x) - pos.x;
            float dTop    = pos.y - bounds.position.y;
            float dBottom = (bounds.position.y + bounds.size.y) - pos.y;

            float minH = std::min(dLeft, dRight);
            float minV = std::min(dTop,  dBottom);

            sf::Vector2f pushDir;
            if (minH <= minV) {
                pushDir = (dLeft <= dRight) ? sf::Vector2f(-1.f, 0.f)
                                            : sf::Vector2f( 1.f, 0.f);
            } else {
                pushDir = (dTop <= dBottom) ? sf::Vector2f(0.f, -1.f)
                                            : sf::Vector2f(0.f,  1.f);
            }

            pos += pushDir * (PUSH_SPEED * deltaTime);
        }

        unit->setPosition(pos);
    }
}
