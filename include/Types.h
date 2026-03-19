#pragma once

#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/Color.hpp>
#include <cstdint>
#include <memory>
#include <vector>

// Forward declarations
class Entity;
class Unit;
class Worker;
class Soldier;
class Building;
class LightTank;
class Projectile;
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
    LightTank,  // Ranged, fires homing rockets
    // Projectiles
    Rocket,
    // Buildings
    Base,
    Barracks,
    Refinery,
    Factory,
    Turret,
    // Resources
    MineralPatch,
    GasGeyser,
    // Editor-only marker
    StartPosition
};

enum class TileType {
    Grass,      // passable terrain (formerly Ground)
    Water,      // impassable terrain (formerly Blocked)
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
    Neutral,
    Player1,
    Player2,
    Player3,
    Player4
};

// Structures
struct Resources {
    int minerals = 0;
    int gas = 0;
};

struct Tile {
    TileType type      = TileType::Grass;
    bool     walkable  = true;
    bool     buildable = true;
    uint8_t  variant   = 1;   // Which texture variant (1-8 for Grass, 1-4 for Water)
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

// Data for RVO (Reciprocal Velocity Obstacles) collision avoidance.
// Defined here so IGameContext can reference it without pulling in Unit.h.
struct RVONeighbor {
    sf::Vector2f position;
    sf::Vector2f velocity;
    float radius;
};

// ---------------------------------------------------------------------------
// Team utility functions — single source of truth for all team-related helpers
// ---------------------------------------------------------------------------

// Returns the canonical display colour for a team slot.
inline sf::Color teamColor(Team team) {
    switch (team) {
        case Team::Player1: return sf::Color(0x34, 0x98, 0xDB);  // Blue
        case Team::Player2: return sf::Color(0xE7, 0x4C, 0x3C);  // Red
        case Team::Player3: return sf::Color(0xF3, 0x9C, 0x12);  // Orange
        case Team::Player4: return sf::Color(0x2E, 0xCC, 0x71);  // Green
        case Team::Neutral: return sf::Color(0x95, 0xA5, 0xA6);  // Gray
        default:            return sf::Color::White;
    }
}

// Maps a 0-based player index to its Team enum (0→Player1 … 3→Player4).
inline Team teamFromIndex(int i) {
    switch (i) {
        case 0:  return Team::Player1;
        case 1:  return Team::Player2;
        case 2:  return Team::Player3;
        default: return Team::Player4;
    }
}

// Maps a Team enum back to a 0-based index (-1 for Neutral).
inline int teamToIndex(Team t) {
    switch (t) {
        case Team::Player1: return 0;
        case Team::Player2: return 1;
        case Team::Player3: return 2;
        case Team::Player4: return 3;
        default:            return -1;  // Neutral
    }
}
