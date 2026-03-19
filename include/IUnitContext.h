#pragma once

#include "Types.h"

// Forward declarations — avoid pulling in full headers
class Entity;
class Unit;
class Building;

// ---------------------------------------------------------------------------
// IUnitContext
//
// Pure-virtual interface that the game world (Game) must implement.
// Unit and Worker hold a non-owning pointer to this interface instead of
// a collection of individually wired std::function callbacks.
//
// Benefits over the callback approach:
//   • Single injection point (setContext) instead of 7 separate assignments.
//   • Dependencies are explicit and documented in one place.
//   • Easy to stub/mock in tests — just subclass IUnitContext.
// ---------------------------------------------------------------------------
class IUnitContext {
public:
    virtual ~IUnitContext() = default;

    // ----- Spatial / combat queries ----------------------------------------

    // Nearest living enemy within [radius] of [pos] that is NOT on [excludeTeam].
    virtual EntityPtr findNearestEnemy(sf::Vector2f pos, float radius, Team excludeTeam) = 0;
    
    // Find best enemy target with priority: combat units > workers > buildings.
    // Used for auto-attack target selection.
    virtual EntityPtr findPriorityEnemy(sf::Vector2f pos, float radius, Team excludeTeam) = 0;

    // Returns true if [pos] (expanded by [radius]) overlaps a static obstacle
    // (building or resource node). [excludeSelf] is the calling unit, ignored.
    virtual bool checkPositionBlocked(sf::Vector2f pos, float radius, Entity* excludeSelf) = 0;

    // Find a non-blocked position near [pos] for an entity with [radius].
    // Searches in expanding circles up to [maxSearchRadius]. Returns [pos] if already free.
    // [excludeSelf] is the calling unit and will be ignored in collision checks.
    virtual sf::Vector2f findFreePosition(sf::Vector2f pos, float radius, float maxSearchRadius, Entity* excludeSelf) = 0;

    // Returns all collidable units within [radius] of [pos] for RVO avoidance.
    // [excludeSelf] is the calling unit and should not appear in the result.
    virtual std::vector<RVONeighbor> getNearbyUnitsRVO(sf::Vector2f pos, float radius, Unit* excludeSelf) = 0;

    // Spawns a homing projectile travelling from [source] to [target].
    virtual void spawnProjectile(EntityPtr source, EntityPtr target, int damage, float speed) = 0;

    // ----- Worker / resource queries ----------------------------------------

    // Nearest resource node (MineralPatch or GasGeyser) within [radius] of [pos].
    virtual EntityPtr findNearestResource(sf::Vector2f pos, float radius) = 0;

    // Nearest resource node within [radius] of [pos] that is not currently
    // claimed by another worker. [exclude] is the node to skip (may be null).
    virtual EntityPtr findNearestAvailableResource(sf::Vector2f pos, float radius, EntityPtr exclude) = 0;

    // Called by a Worker when it deposits harvested resources.
    // The Game credits the amount to the player that owns [team].
    virtual void depositResources(Team team, int amount) = 0;

    // ----- Building production ---------------------------------------------

    // Called by a Building when it finishes training a unit.
    // The Game spawns the unit at the correct exit position.
    virtual void notifyUnitProduced(EntityType unitType, Building* sourceBuilding) = 0;

    // Called by a Building when training of [unitType] is cancelled.
    // The Game refunds the mineral cost to the player that owns [team].
    virtual void refundProductionCost(EntityType unitType, Team team) = 0;
};
