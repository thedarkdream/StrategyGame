#pragma once

#include "Types.h"
#include <SFML/Graphics/Color.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

// Forward declaration
struct EntityDef;

// Action definition for UI
struct ActionDef {
    std::string label;      // Display name (e.g., "Move", "Attack")
    std::string hotkey;     // Hotkey display (e.g., "M", "A")
    
    enum class Type {
        TargetMove,         // Requires position target
        TargetAttack,       // Requires entity target
        TargetGather,       // Requires resource target
        TargetRallyPoint,   // Set building rally point
        Instant,            // Executes immediately (e.g., Stop)
        Train,              // Trains a unit (requires EntityType)
        Build               // Constructs a building (requires EntityType)
    };
    Type type = Type::Instant;
    
    EntityType producesType = EntityType::None;  // For Train/Build actions
    EntityType requires = EntityType::None;      // Required building for this action
    int row = 0;  // Which row in the action bar (0 = first row, 1 = second row)
};

// Unit-specific data
struct UnitDef {
    float speed = 0.0f;
    int damage = 0;
    float attackRange = 0.0f;
    float attackCooldown = 0.0f;
    float autoAttackRangeBonus = 0.0f;  // Extra range for auto-attack detection
    float trainingTime = 5.0f;          // Time to train in seconds
    bool canGather = false;
    bool canBuild = false;
    bool isCombatUnit = false;          // Has auto-attack behavior
};

// Combat stats for buildings that can attack (e.g. Turret).
// Only populated for combat buildings; absent (nullopt) for everything else.
struct CombatBuildingDef {
    float attackRange     = 0.f;  // Max targeting radius in pixels
    int   attackDamage    = 0;    // Damage per projectile
    float attackCooldown  = 0.f;  // Seconds between shots
    float projectileSpeed = 0.f;  // Projectile travel speed in pixels/s
    float fireDisplayTime = 0.f;  // Seconds the "firing" sprite is shown after a shot
};

// Building-specific data
struct BuildingDef {
    sf::Vector2i tileSize = {1, 1};     // Size in tiles
    std::vector<EntityType> producesUnits;  // What units can be trained here
    bool isResourceNode = false;
    int resourceAmount = 0;             // For resource nodes
    float constructionTime = 10.0f;     // Time in seconds to construct

    // True when this building produces units (derived from producesUnits).
    bool canProduce() const { return !producesUnits.empty(); }

    // Present only for combat buildings (e.g. Turret); nullopt otherwise.
    std::optional<CombatBuildingDef> combat;
};

// Complete entity definition
struct EntityDef {
    EntityType type = EntityType::None;
    std::string name;                   // Display name
    std::string shortName;              // 1-3 char abbreviation for icons
    
    // Costs
    int mineralCost = 0;
    int gasCost = 0;
    
    // Common stats
    int health = 0;
    sf::Vector2f size = {0.0f, 0.0f};   // Visual/collision size in pixels

    // Sight radius in world units (how far this entity can reveal the fog of war)
    float visionRadius = 200.0f;
    
    // Type-specific data (only one will be populated)
    std::optional<UnitDef> unit;
    std::optional<BuildingDef> building;
    
    // Available actions for this entity
    std::vector<ActionDef> actions;
    
    // Helpers
    bool isUnit() const { return unit.has_value(); }
    bool isBuilding() const { return building.has_value(); }
    bool isResource() const { 
        return building.has_value() && building->isResourceNode; 
    }
};

// The registry - singleton access
class EntityRegistry {
public:
    static EntityRegistry& instance();
    
    // Get definition by type (returns nullptr if not found)
    const EntityDef* get(EntityType type) const;
    
    // Convenience accessors
    int getMineralCost(EntityType type) const;
    int getGasCost(EntityType type) const;
    int getHealth(EntityType type) const;
    sf::Vector2f getSize(EntityType type) const;
    sf::Vector2i getBuildingTileSize(EntityType type) const;
    const std::vector<ActionDef>& getActions(EntityType type) const;
    std::string getName(EntityType type) const;
    std::string getShortName(EntityType type) const;
    
    // Unit-specific
    const UnitDef* getUnitDef(EntityType type) const;
    float getTrainingTime(EntityType type) const;

    // Vision / Fog of War
    float getVisionRadius(EntityType type) const;
    
    // Building-specific  
    const BuildingDef*       getBuildingDef(EntityType type) const;
    const CombatBuildingDef* getCombatBuildingDef(EntityType type) const;
    float getConstructionTime(EntityType type) const;
    
private:
    EntityRegistry();
    void registerEntity(EntityDef def);
    void initializeDefaults();
    
    std::unordered_map<EntityType, EntityDef> m_definitions;
    static const std::vector<ActionDef> s_emptyActions;
};

// Macro for easy registry access
#define ENTITY_DATA EntityRegistry::instance()
