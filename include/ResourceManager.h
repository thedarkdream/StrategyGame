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
    
    // Cost lookups (now delegate to EntityRegistry)
    static int getMineralCost(EntityType type);
    static int getGasCost(EntityType type);
    static sf::Vector2i getBuildingSize(EntityType type);
};
