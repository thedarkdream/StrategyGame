#pragma once

#include "Types.h"
#include <SFML/Graphics.hpp>
#include <functional>
#include <vector>

class Entity;
class Unit;

// ---------------------------------------------------------------------------
// EntityWorld
//
// Owns the master entity list and provides all spatial queries, collision
// helpers, and per-frame physics (unit-push-out).
//
// Game holds one EntityWorld and delegates IGameContext's spatial methods to
// it. Nothing here knows about Players, Statistics, Map, or game rules —
// it is a pure geometric/structural view of every live entity.
// ---------------------------------------------------------------------------
class EntityWorld {
public:
    EntityWorld() = default;

    // ---- Entity list management -------------------------------------------

    // Buffer entity for addition after the current update loop (prevents
    // iterator invalidation when entities spawn mid-loop e.g. rockets).
    void add(EntityPtr entity);

    // Erase entity from both the live list and the pending buffer.
    void remove(EntityPtr entity);

    // Move all pending entities into the live list. Call after each update pass.
    void flush();

    // Remove all entities for which isReadyForRemoval() is true. Returns the
    // set of removed entities so the caller can do bookkeeping (stats, map
    // tiles, player lists) without peeking inside the container.
    EntityList extractRemovable();

    // Read access for iteration (rendering, AI, etc.)
    const EntityList& all() const { return m_all; }

    // ---- Spatial queries --------------------------------------------------

    // First living entity whose bounds contain pos, excluding rockets.
    EntityPtr getAt(sf::Vector2f pos) const;

    std::vector<EntityPtr> getInRect(sf::FloatRect rect) const;
    std::vector<EntityPtr> getInRect(sf::FloatRect rect, Team team) const;

    // Nearest living enemy of any team except excludeTeam (rockets excluded).
    EntityPtr findNearestEnemy(sf::Vector2f pos, float radius, Team excludeTeam) const;

    // Nearest enemy by priority bucket: combat unit > non-combat unit > building.
    EntityPtr findPriorityEnemy(sf::Vector2f pos, float radius, Team excludeTeam) const;

    EntityPtr findNearestResource(sf::Vector2f pos, float radius) const;
    EntityPtr findNearestAvailableResource(sf::Vector2f pos, float radius, EntityPtr exclude) const;

    // First Base building owned by team, or nullptr.
    EntityPtr findHomeBase(Team team) const;

    // ---- Collision / free-position helpers --------------------------------

    bool         checkPositionBlocked(sf::Vector2f pos, float radius, Entity* excludeSelf) const;
    sf::Vector2f findFreePosition(sf::Vector2f pos, float radius, float maxSearchRadius, Entity* excludeSelf) const;
    sf::Vector2f findSpawnPosition(sf::Vector2f origin, float unitRadius) const;
    sf::Vector2f findNearestFreePosition(sf::Vector2f pos, float radius, int maxRings, Entity* excludeSelf) const;
    std::vector<RVONeighbor> getNearbyUnitsRVO(sf::Vector2f pos, float radius, Unit* excludeSelf) const;

    // ---- Per-frame physics ------------------------------------------------

    // Gently eject collidable units that are physically inside a building.
    void pushUnitsOutOfBuildings(float deltaTime);

private:
    EntityList m_all;
    EntityList m_pending;  // buffered; merged into m_all after each update loop

    // Generic nearest-entity finder — apply any predicate, return closest.
    template<typename Predicate>
    EntityPtr findNearest(sf::Vector2f pos, float radius, Predicate predicate) const;
};

// Template implementation must be in the header.
#include "Entity.h"
#include "MathUtil.h"

template<typename Predicate>
EntityPtr EntityWorld::findNearest(sf::Vector2f pos, float radius, Predicate predicate) const {
    EntityPtr nearest;
    float nearestDist = radius;

    for (const auto& entity : m_all) {
        if (!entity || !entity->isAlive()) continue;
        if (!predicate(entity)) continue;

        float dist = MathUtil::distance(entity->getPosition(), pos);
        if (dist < nearestDist) {
            nearestDist = dist;
            nearest = entity;
        }
    }
    return nearest;
}
