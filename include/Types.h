#pragma once

#include <SFML/System/Vector2.hpp>
#include <cstdint>
#include <memory>
#include <vector>

// Forward declarations
class Entity;
class Unit;
class Worker;
class Soldier;
class Building;
class ResourceNode;
class Player;

// Type aliases
using EntityPtr = std::shared_ptr<Entity>;
using UnitPtr = std::shared_ptr<Unit>;
using BuildingPtr = std::shared_ptr<Building>;
using ResourceNodePtr = std::shared_ptr<ResourceNode>;
using PlayerPtr = std::shared_ptr<Player>;

using EntityList = std::vector<EntityPtr>;
using UnitList = std::vector<UnitPtr>;
using BuildingList = std::vector<BuildingPtr>;
using ResourceNodeList = std::vector<ResourceNodePtr>;

// Enums
enum class EntityType {
    None,
    // Units
    Worker,
    Soldier,
    Brute,  // Melee soldier
    // Buildings
    Base,
    Barracks,
    Refinery,
    Factory,
    // Resources
    MineralPatch,
    GasGeyser
};

enum class TileType {
    Ground,
    Blocked,
    Resource,
    Building
};

enum class UnitState {
    Idle,
    Moving,
    AttackMoving,  // Moving to target while attacking enemies in range
    Attacking,
    Following,     // Following an allied unit
    Gathering,
    Returning,
    Building
};

enum class GameState {
    Menu,
    Playing,
    Paused,
    Victory,
    Defeat
};

enum class Team {
    Player,
    Enemy,
    Neutral
};

// Structures
struct Resources {
    int minerals = 0;
    int gas = 0;
};

struct Tile {
    TileType type = TileType::Ground;
    bool walkable = true;
    bool buildable = true;
    EntityPtr occupant = nullptr;
};

struct Command {
    enum class Type {
        None,
        Move,
        Attack,
        Gather,
        Build,
        Train,
        Stop
    };
    
    Type type = Type::None;
    sf::Vector2f targetPosition;
    EntityPtr targetEntity;
    EntityType buildType;
};
