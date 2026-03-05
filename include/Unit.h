#pragma once

#include "Entity.h"
#include <functional>
#include <vector>

class Map;

class Unit : public Entity {
public:
    Unit(EntityType type, Team team, sf::Vector2f position, 
         float speed, int damage, float attackRange, float attackCooldown,
         sf::Vector2f size, int health);
    virtual ~Unit() = default;
    
    void update(float deltaTime) override;
    void render(sf::RenderTarget& target) override;
    
    // Commands
    void moveTo(sf::Vector2f target);
    virtual void attack(EntityPtr target);
    void stop();
    
    // Combat - override to auto-retaliate when attacked while idle
    void takeDamage(int damage, EntityPtr attacker) override;
    
    // State
    UnitState getState() const { return m_state; }
    bool isIdle() const { return m_state == UnitState::Idle; }
    
    // Stats getters
    float getSpeed() const { return m_speed; }
    int getDamage() const { return m_damage; }
    float getAttackRange() const { return m_attackRange; }
    float getAttackCooldown() const { return m_attackCooldown; }
    float getAttacksPerSecond() const { return m_attackCooldown > 0 ? 1.0f / m_attackCooldown : 0.0f; }
    
    // Reference to map for pathfinding
    void setMap(Map* map) { m_map = map; }
    
    // Collision
    float getCollisionRadius() const { return std::max(m_size.x, m_size.y) / 2.0f; }
    virtual bool isCollidable() const { return true; }  // Override in subclasses if needed
    
    // Callback for finding nearby enemies (set by Game)
    std::function<EntityPtr(sf::Vector2f pos, float radius, Team excludeTeam)> findNearestEnemy;
    
    // Callback for checking if a position is blocked (set by Game)
    // Returns true if the position is blocked, excludeSelf is the unit doing the check
    std::function<bool(sf::Vector2f pos, float radius, Entity* excludeSelf)> checkPositionBlocked;
    
protected:
    // State update methods - can be overridden by subclasses
    virtual void updateIdle(float deltaTime);
    virtual void updateMovement(float deltaTime);
    virtual void updateCombat(float deltaTime);
    virtual void updateCustomState(float deltaTime);
    
    // Movement helpers
    void moveTowardsTarget(float deltaTime);
    void followPath(float deltaTime);  // Follow current path waypoints
    bool hasReachedTarget() const;
    void findPath(sf::Vector2f target);
    float getDistanceTo(sf::Vector2f pos) const;
    float getDistanceTo(EntityPtr entity) const;
    
    // State
    UnitState m_state = UnitState::Idle;
    float m_speed;
    int m_damage;
    float m_attackRange;
    float m_attackCooldown;
    float m_attackTimer = 0.0f;
    float m_autoAttackRangeBonus = 0.0f;  // Extra range for auto-attack detection
    bool m_isCombatUnit = false;          // Has auto-attack behavior
    
    // Movement
    sf::Vector2f m_targetPosition;
    std::vector<sf::Vector2f> m_path;
    size_t m_pathIndex = 0;
    
    // Combat
    std::weak_ptr<Entity> m_targetEntity;
    
    Map* m_map = nullptr;
};
