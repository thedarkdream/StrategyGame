#pragma once

#include "Types.h"
#include <vector>
#include <memory>

class Unit;
class Building;
class ResourceNode;
class Map;

// Factory for creating entities
class ResourceManager {
public:
    ResourceManager() = default;
    
    // Generic unit creation by type
    static UnitPtr createUnit(EntityType type, Team team, sf::Vector2f position);
    
    // Generic building creation by type
    static BuildingPtr createBuilding(EntityType type, Team team, sf::Vector2f position);
    
    // Resource node creation
    static ResourceNodePtr createResourceNode(EntityType type, sf::Vector2f position);
    
    // Convenience methods (delegate to createUnit/createBuilding)
    static UnitPtr createWorker(Team team, sf::Vector2f position);
    static UnitPtr createSoldier(Team team, sf::Vector2f position);
    static UnitPtr createBrute(Team team, sf::Vector2f position);
    static BuildingPtr createBase(Team team, sf::Vector2f position);
    static BuildingPtr createBarracks(Team team, sf::Vector2f position);
    static BuildingPtr createRefinery(Team team, sf::Vector2f position);
    static BuildingPtr createFactory(Team team, sf::Vector2f position);
    static ResourceNodePtr createMineralPatch(sf::Vector2f position, int amount = 1500);
    static ResourceNodePtr createGasGeyser(sf::Vector2f position, int amount = 2000);
    
    // Cost lookups (now delegate to EntityRegistry)
    static int getMineralCost(EntityType type);
    static int getGasCost(EntityType type);
    static sf::Vector2i getBuildingSize(EntityType type);
};
