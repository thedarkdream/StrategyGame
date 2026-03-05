#include "ResourceManager.h"
#include "Unit.h"
#include "Worker.h"
#include "Soldier.h"
#include "Building.h"
#include "Constants.h"

UnitPtr ResourceManager::createWorker(Team team, sf::Vector2f position) {
    return std::make_shared<Worker>(team, position);
}

UnitPtr ResourceManager::createSoldier(Team team, sf::Vector2f position) {
    return std::make_shared<Soldier>(team, position);
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

BuildingPtr ResourceManager::createMineralPatch(sf::Vector2f position, int amount) {
    auto patch = std::make_shared<Building>(EntityType::MineralPatch, Team::Neutral, position);
    return patch;
}

BuildingPtr ResourceManager::createGasGeyser(sf::Vector2f position, int amount) {
    auto geyser = std::make_shared<Building>(EntityType::GasGeyser, Team::Neutral, position);
    return geyser;
}

int ResourceManager::getMineralCost(EntityType type) {
    switch (type) {
        case EntityType::Worker:
            return Constants::WORKER_COST_MINERALS;
        case EntityType::Soldier:
            return Constants::SOLDIER_COST_MINERALS;
        case EntityType::Base:
            return Constants::BASE_COST_MINERALS;
        case EntityType::Barracks:
            return Constants::BARRACKS_COST_MINERALS;
        case EntityType::Refinery:
            return Constants::REFINERY_COST_MINERALS;
        default:
            return 0;
    }
}

int ResourceManager::getGasCost(EntityType type) {
    switch (type) {
        case EntityType::Soldier:
            return 0;  // Could require gas for advanced units
        default:
            return 0;
    }
}

sf::Vector2i ResourceManager::getBuildingSize(EntityType type) {
    switch (type) {
        case EntityType::Base:
            return sf::Vector2i(3, 3);
        case EntityType::Barracks:
        case EntityType::Refinery:
            return sf::Vector2i(2, 2);
        case EntityType::MineralPatch:
            return sf::Vector2i(2, 1);
        case EntityType::GasGeyser:
            return sf::Vector2i(2, 2);
        default:
            return sf::Vector2i(1, 1);
    }
}
