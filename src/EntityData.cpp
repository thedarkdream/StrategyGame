#include "EntityData.h"
#include "Constants.h"

const std::vector<ActionDef> EntityRegistry::s_emptyActions = {};

EntityRegistry& EntityRegistry::instance() {
    static EntityRegistry registry;
    return registry;
}

EntityRegistry::EntityRegistry() {
    initializeDefaults();
}

void EntityRegistry::registerEntity(EntityDef def) {
    m_definitions[def.type] = std::move(def);
}

const EntityDef* EntityRegistry::get(EntityType type) const {
    auto it = m_definitions.find(type);
    return it != m_definitions.end() ? &it->second : nullptr;
}

int EntityRegistry::getMineralCost(EntityType type) const {
    auto* def = get(type);
    return def ? def->mineralCost : 0;
}

int EntityRegistry::getGasCost(EntityType type) const {
    auto* def = get(type);
    return def ? def->gasCost : 0;
}

int EntityRegistry::getHealth(EntityType type) const {
    auto* def = get(type);
    return def ? def->health : 0;
}

sf::Vector2f EntityRegistry::getSize(EntityType type) const {
    auto* def = get(type);
    return def ? def->size : sf::Vector2f{16.0f, 16.0f};
}

sf::Vector2i EntityRegistry::getBuildingTileSize(EntityType type) const {
    auto* def = get(type);
    if (def && def->building) {
        return def->building->tileSize;
    }
    return {1, 1};
}

const std::vector<ActionDef>& EntityRegistry::getActions(EntityType type) const {
    auto* def = get(type);
    return def ? def->actions : s_emptyActions;
}

std::string EntityRegistry::getName(EntityType type) const {
    auto* def = get(type);
    return def ? def->name : "Unknown";
}

std::string EntityRegistry::getShortName(EntityType type) const {
    auto* def = get(type);
    return def ? def->shortName : "?";
}

const UnitDef* EntityRegistry::getUnitDef(EntityType type) const {
    auto* def = get(type);
    return (def && def->unit) ? &def->unit.value() : nullptr;
}

const BuildingDef* EntityRegistry::getBuildingDef(EntityType type) const {
    auto* def = get(type);
    return (def && def->building) ? &def->building.value() : nullptr;
}

float EntityRegistry::getConstructionTime(EntityType type) const {
    auto* buildingDef = getBuildingDef(type);
    return buildingDef ? buildingDef->constructionTime : 10.0f;
}

float EntityRegistry::getTrainingTime(EntityType type) const {
    auto* unitDef = getUnitDef(type);
    return unitDef ? unitDef->trainingTime : 5.0f;
}

void EntityRegistry::initializeDefaults() {
    // ==================== UNITS ====================
    
    // Worker
    {
        EntityDef def;
        def.type = EntityType::Worker;
        def.name = "Worker";
        def.shortName = "W";
        def.mineralCost = Constants::WORKER_COST_MINERALS;
        def.gasCost = 0;
        def.health = Constants::WORKER_HEALTH;
        def.size = {24.0f, 24.0f};
        
        UnitDef unit;
        unit.speed = Constants::WORKER_SPEED;
        unit.damage = 5;
        unit.attackRange = 30.0f;
        unit.attackCooldown = 1.5f;
        unit.autoAttackRangeBonus = 0.0f;
        unit.trainingTime = 3.0f;
        unit.canGather = true;
        unit.canBuild = true;
        unit.isCombatUnit = false;
        def.unit = unit;
        
        // Worker actions
        ActionDef buildBarracks;
        buildBarracks.label = "Barracks";
        buildBarracks.hotkey = "B";
        buildBarracks.type = ActionDef::Type::Build;
        buildBarracks.producesType = EntityType::Barracks;
        buildBarracks.row = 1;
        
        ActionDef buildBase;
        buildBase.label = "Base";
        buildBase.hotkey = "C";
        buildBase.type = ActionDef::Type::Build;
        buildBase.producesType = EntityType::Base;
        buildBase.row = 1;
        
        ActionDef buildFactory;
        buildFactory.label = "Factory";
        buildFactory.hotkey = "F";
        buildFactory.type = ActionDef::Type::Build;
        buildFactory.producesType = EntityType::Factory;
        buildFactory.requires = EntityType::Barracks;  // Requires completed Barracks
        buildFactory.row = 1;
        
        def.actions = {
            {"Move", "M", ActionDef::Type::TargetMove},
            {"Stop", "S", ActionDef::Type::Instant},
            {"Attack", "A", ActionDef::Type::TargetAttack},
            {"Gather", "G", ActionDef::Type::TargetGather},
            buildBarracks,
            buildBase,
            buildFactory
        };
        
        registerEntity(std::move(def));
    }
    
    // Soldier
    {
        EntityDef def;
        def.type = EntityType::Soldier;
        def.name = "Soldier";
        def.shortName = "S";
        def.mineralCost = Constants::SOLDIER_COST_MINERALS;
        def.gasCost = 0;
        def.health = Constants::SOLDIER_HEALTH;
        def.size = {24.0f, 24.0f};
        
        UnitDef unit;
        unit.speed = Constants::SOLDIER_SPEED;
        unit.damage = Constants::SOLDIER_DAMAGE;
        unit.attackRange = Constants::SOLDIER_ATTACK_RANGE;
        unit.attackCooldown = Constants::SOLDIER_ATTACK_COOLDOWN;
        unit.autoAttackRangeBonus = 50.0f;
        unit.trainingTime = 5.0f;
        unit.canGather = false;
        unit.canBuild = false;
        unit.isCombatUnit = true;
        def.unit = unit;
        
        // Combat unit actions
        def.actions = {
            {"Move", "M", ActionDef::Type::TargetMove},
            {"Stop", "S", ActionDef::Type::Instant},
            {"Attack", "A", ActionDef::Type::TargetAttack}
        };
        
        registerEntity(std::move(def));
    }
    
    // LightTank
    {
        EntityDef def;
        def.type = EntityType::LightTank;
        def.name = "Light Tank";
        def.shortName = "LT";
        def.mineralCost = 150;
        def.gasCost = 0;
        def.health = 120;
        def.size = {40.0f, 40.0f};  // 40px diameter circle
        
        UnitDef unit;
        unit.speed = 150.0f;
        unit.damage = 25;
        unit.attackRange = 200.0f;  // Long range - fires rockets
        unit.attackCooldown = 2.5f;
        unit.autoAttackRangeBonus = 50.0f;
        unit.trainingTime = 8.0f;
        unit.canGather = false;
        unit.canBuild = false;
        unit.isCombatUnit = true;
        def.unit = unit;
        
        def.actions = {
            {"Move", "M", ActionDef::Type::TargetMove},
            {"Stop", "S", ActionDef::Type::Instant},
            {"Attack", "A", ActionDef::Type::TargetAttack}
        };
        
        registerEntity(std::move(def));
    }
    
    // Brute
    {
        EntityDef def;
        def.type = EntityType::Brute;
        def.name = "Brute";
        def.shortName = "B";
        def.mineralCost = Constants::BRUTE_COST_MINERALS;
        def.gasCost = 0;
        def.health = Constants::BRUTE_HEALTH;
        def.size = {28.0f, 28.0f};
        
        UnitDef unit;
        unit.speed = Constants::BRUTE_SPEED;
        unit.damage = Constants::BRUTE_DAMAGE;
        unit.attackRange = Constants::BRUTE_ATTACK_RANGE;
        unit.attackCooldown = Constants::BRUTE_ATTACK_COOLDOWN;
        unit.autoAttackRangeBonus = 50.0f;
        unit.trainingTime = 4.0f;
        unit.canGather = false;
        unit.canBuild = false;
        unit.isCombatUnit = true;
        def.unit = unit;
        
        // Combat unit actions
        def.actions = {
            {"Move", "M", ActionDef::Type::TargetMove},
            {"Stop", "S", ActionDef::Type::Instant},
            {"Attack", "A", ActionDef::Type::TargetAttack}
        };
        
        registerEntity(std::move(def));
    }
    
    // ==================== BUILDINGS ====================
    
    // Base
    {
        EntityDef def;
        def.type = EntityType::Base;
        def.name = "Command Center";
        def.shortName = "CC";
        def.mineralCost = Constants::BASE_COST_MINERALS;
        def.gasCost = 0;
        def.health = 1500;
        def.size = {96.0f, 96.0f};
        
        BuildingDef building;
        building.tileSize = {3, 3};
        building.canProduce = true;
        building.producesUnits = {EntityType::Worker};
        building.isResourceNode = false;
        building.constructionTime = 30.0f;  // 30 seconds
        def.building = building;
        
        // Base actions - train workers
        ActionDef trainWorker;
        trainWorker.label = "Worker";
        trainWorker.hotkey = "W";
        trainWorker.type = ActionDef::Type::Train;
        trainWorker.producesType = EntityType::Worker;
        def.actions = {trainWorker};
        
        registerEntity(std::move(def));
    }
    
    // Barracks
    {
        EntityDef def;
        def.type = EntityType::Barracks;
        def.name = "Barracks";
        def.shortName = "BK";
        def.mineralCost = Constants::BARRACKS_COST_MINERALS;
        def.gasCost = 0;
        def.health = 1000;
        def.size = {96.0f, 64.0f};
        
        BuildingDef building;
        building.tileSize = {3, 2};
        building.canProduce = true;
        building.producesUnits = {EntityType::Soldier, EntityType::Brute};
        building.isResourceNode = false;
        building.constructionTime = 10.0f;  // 10 seconds
        def.building = building;
        
        // Barracks actions - train combat units
        ActionDef trainSoldier;
        trainSoldier.label = "Soldier";
        trainSoldier.hotkey = "S";
        trainSoldier.type = ActionDef::Type::Train;
        trainSoldier.producesType = EntityType::Soldier;
        
        ActionDef trainBrute;
        trainBrute.label = "Brute";
        trainBrute.hotkey = "B";
        trainBrute.type = ActionDef::Type::Train;
        trainBrute.producesType = EntityType::Brute;
        
        def.actions = {trainSoldier, trainBrute};
        
        registerEntity(std::move(def));
    }
    
    // Refinery
    {
        EntityDef def;
        def.type = EntityType::Refinery;
        def.name = "Refinery";
        def.shortName = "RF";
        def.mineralCost = Constants::REFINERY_COST_MINERALS;
        def.gasCost = 0;
        def.health = 500;
        def.size = {64.0f, 64.0f};
        
        BuildingDef building;
        building.tileSize = {2, 2};
        building.canProduce = false;
        building.isResourceNode = false;
        def.building = building;
        
        registerEntity(std::move(def));
    }
    
    // Factory
    {
        EntityDef def;
        def.type = EntityType::Factory;
        def.name = "Factory";
        def.shortName = "FC";
        def.mineralCost = 150;
        def.gasCost = 0;
        def.health = 1200;
        def.size = {96.0f, 64.0f};
        
        BuildingDef building;
        building.tileSize = {3, 2};
        building.canProduce = true;
        building.producesUnits = {EntityType::LightTank};
        building.isResourceNode = false;
        building.constructionTime = 15.0f;  // 15 seconds
        def.building = building;
        
        // Factory actions - train Light Tank
        ActionDef trainLightTank;
        trainLightTank.label = "Light Tank";
        trainLightTank.hotkey = "T";
        trainLightTank.type = ActionDef::Type::Train;
        trainLightTank.producesType = EntityType::LightTank;
        def.actions = {trainLightTank};
        
        registerEntity(std::move(def));
    }
    
    // ==================== RESOURCES ====================
    
    // Mineral Patch
    {
        EntityDef def;
        def.type = EntityType::MineralPatch;
        def.name = "Mineral Patch";
        def.shortName = "M";
        def.mineralCost = 0;
        def.health = 1;  // Immortal until depleted
        def.size = {48.0f, 32.0f};
        
        BuildingDef building;
        building.tileSize = {2, 1};
        building.canProduce = false;
        building.isResourceNode = true;
        building.resourceAmount = 1500;
        def.building = building;
        
        registerEntity(std::move(def));
    }
    
    // Gas Geyser
    {
        EntityDef def;
        def.type = EntityType::GasGeyser;
        def.name = "Vespene Geyser";
        def.shortName = "G";
        def.mineralCost = 0;
        def.health = 1;
        def.size = {64.0f, 64.0f};
        
        BuildingDef building;
        building.tileSize = {2, 2};
        building.canProduce = false;
        building.isResourceNode = true;
        building.resourceAmount = 2500;
        def.building = building;
        
        registerEntity(std::move(def));
    }
}
