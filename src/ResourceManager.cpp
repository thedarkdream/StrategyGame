#include "ResourceManager.h"
#include "EntityData.h"
#include "Unit.h"
#include "Worker.h"
#include "Soldier.h"
#include "Brute.h"
#include "LightTank.h"
#include "Building.h"
#include "Turret.h"
#include "ResourceNode.h"

UnitPtr ResourceManager::createUnit(EntityType type, Team team, sf::Vector2f position) {
    switch (type) {
        case EntityType::Worker:    return std::make_shared<Worker>(team, position);
        case EntityType::Soldier:   return std::make_shared<Soldier>(team, position);
        case EntityType::Brute:     return std::make_shared<Brute>(team, position);
        case EntityType::LightTank: return std::make_shared<LightTank>(team, position);
        default: return nullptr;
    }
}

BuildingPtr ResourceManager::createBuilding(EntityType type, Team team, sf::Vector2f position) {
    switch (type) {
        case EntityType::Base:      return std::make_shared<Building>(EntityType::Base, team, position);
        case EntityType::Barracks:  return std::make_shared<Building>(EntityType::Barracks, team, position);
        case EntityType::Refinery:  return std::make_shared<Building>(EntityType::Refinery, team, position);
        case EntityType::Factory:   return std::make_shared<Building>(EntityType::Factory, team, position);
        case EntityType::Turret:    return std::make_shared<Turret>(team, position);
        default: return nullptr;
    }
}

ResourceNodePtr ResourceManager::createResourceNode(EntityType type, sf::Vector2f position) {
    const EntityDef* def = ENTITY_DATA.get(type);
    if (!def || !def->building) return nullptr;
    int amount = def->building->resourceAmount;
    return std::make_shared<ResourceNode>(type, position, amount);
}

int ResourceManager::getMineralCost(EntityType type) {
    return ENTITY_DATA.getMineralCost(type);
}

int ResourceManager::getGasCost(EntityType type) {
    return ENTITY_DATA.getGasCost(type);
}

sf::Vector2i ResourceManager::getBuildingSize(EntityType type) {
    return ENTITY_DATA.getBuildingTileSize(type);
}
