#pragma once

#include "Types.h"
#include <vector>
#include <memory>

class Unit;
class Building;
class Map;

// Factory for creating entities
class ResourceManager {
public:
    ResourceManager() = default;
    
    // Unit creation
    static UnitPtr createWorker(Team team, sf::Vector2f position);
    static UnitPtr createSoldier(Team team, sf::Vector2f position);
    
    // Building creation
    static BuildingPtr createBase(Team team, sf::Vector2f position);
    static BuildingPtr createBarracks(Team team, sf::Vector2f position);
    static BuildingPtr createRefinery(Team team, sf::Vector2f position);
    
    // Resource nodes
    static BuildingPtr createMineralPatch(sf::Vector2f position, int amount = 1500);
    static BuildingPtr createGasGeyser(sf::Vector2f position, int amount = 2000);
    
    // Get costs
    static int getMineralCost(EntityType type);
    static int getGasCost(EntityType type);
    
    // Get building size in tiles
    static sf::Vector2i getBuildingSize(EntityType type);
};
