#include "ResourceManager.h"
#include "EntityData.h"
#include "Unit.h"
#include "Worker.h"
#include "Soldier.h"
#include "Brute.h"
#include "Building.h"
#include "ResourceNode.h"

UnitPtr ResourceManager::createUnit(EntityType type, Team team, sf::Vector2f position) {
    switch (type) {
        case EntityType::Worker:
            return createWorker(team, position);
        case EntityType::Soldier:
            return createSoldier(team, position);
        case EntityType::Brute:
            return createBrute(team, position);
        default:
            return nullptr;
    }
}

BuildingPtr ResourceManager::createBuilding(EntityType type, Team team, sf::Vector2f position) {
    switch (type) {
        case EntityType::Base:
            return createBase(team, position);
        case EntityType::Barracks:
            return createBarracks(team, position);
        case EntityType::Refinery:
            return createRefinery(team, position);
        default:
            return nullptr;
    }
}

ResourceNodePtr ResourceManager::createResourceNode(EntityType type, sf::Vector2f position) {
    switch (type) {
        case EntityType::MineralPatch:
            return createMineralPatch(position);
        case EntityType::GasGeyser:
            return createGasGeyser(position);
        default:
            return nullptr;
    }
}

UnitPtr ResourceManager::createWorker(Team team, sf::Vector2f position) {
    return std::make_shared<Worker>(team, position);
}

UnitPtr ResourceManager::createSoldier(Team team, sf::Vector2f position) {
    return std::make_shared<Soldier>(team, position);
}

UnitPtr ResourceManager::createBrute(Team team, sf::Vector2f position) {
    return std::make_shared<Brute>(team, position);
}

BuildingPtr ResourceManager::createBase(Team team, sf::Vector2f position) {
    return std::make_shared<Building>(EntityType::Base, team, position);
}

BuildingPtr ResourceManager::createBarracks(Team team, sf::Vector2f position) {
    return std::make_shared<Building>(EntityType::Barracks, team, position);
}

BuildingPtr ResourceManager::createRefinery(Team team, sf::Vector2f position) {
    return std::make_shared<Building>(EntityType::Refinery, team, position);
}

ResourceNodePtr ResourceManager::createMineralPatch(sf::Vector2f position, int amount) {
    return std::make_shared<ResourceNode>(EntityType::MineralPatch, position, amount);
}

ResourceNodePtr ResourceManager::createGasGeyser(sf::Vector2f position, int amount) {
    return std::make_shared<ResourceNode>(EntityType::GasGeyser, position, amount);
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
