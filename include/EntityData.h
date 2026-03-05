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
        Instant,            // Executes immediately (e.g., Stop)
        Train,              // Trains a unit (requires EntityType)
        Build               // Constructs a building (requires EntityType)
    };
    Type type = Type::Instant;
    
    EntityType producesType = EntityType::None;  // For Train/Build actions
};

// Unit-specific data
struct UnitDef {
    float speed = 0.0f;
    int damage = 0;
    float attackRange = 0.0f;
    float attackCooldown = 0.0f;
    float autoAttackRangeBonus = 0.0f;  // Extra range for auto-attack detection
    bool canGather = false;
    bool canBuild = false;
    bool isCombatUnit = false;          // Has auto-attack behavior
};

// Building-specific data
struct BuildingDef {
    sf::Vector2i tileSize = {1, 1};     // Size in tiles
    bool canProduce = false;
    std::vector<EntityType> producesUnits;  // What units can be trained here
    bool isResourceNode = false;
    int resourceAmount = 0;             // For resource nodes
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
    
    // Building-specific  
    const BuildingDef* getBuildingDef(EntityType type) const;
    
private:
    EntityRegistry();
    void registerEntity(EntityDef def);
    void initializeDefaults();
    
    std::unordered_map<EntityType, EntityDef> m_definitions;
    static const std::vector<ActionDef> s_emptyActions;
};

// Macro for easy registry access
#define ENTITY_DATA EntityRegistry::instance()
