#include "Unit.h"
#include "Map.h"
#include "Building.h"
#include "Constants.h"
#include "EntityData.h"
#include "MathUtil.h"
#include "Animation.h"
#include "EffectsManager.h"
#include <cmath>

Unit::Unit(EntityType type, Team team, sf::Vector2f position)
    : Entity(type, team, position)
    , m_speed(0.0f)
    , m_damage(0)
    , m_attackRange(0.0f)
    , m_attackCooldown(1.0f)
{
    // Load all stats from ENTITY_DATA
    m_size = ENTITY_DATA.getSize(type);
    m_maxHealth = ENTITY_DATA.getHealth(type);
    m_health = m_maxHealth;
    m_targetPosition = position;
    
    if (auto* unitDef = ENTITY_DATA.getUnitDef(type)) {
        m_speed = unitDef->speed;
        m_damage = unitDef->damage;
        m_attackRange = unitDef->attackRange;
        m_attackCooldown = unitDef->attackCooldown;
        m_autoAttackRangeBonus = unitDef->autoAttackRangeBonus;
        m_isCombatUnit = unitDef->isCombatUnit;
    }
    
    updateShape();
}

void Unit::update(float deltaTime) {
    // Handle death animation
    if (m_isDying) {
        updateDeathAnimation(deltaTime);
        return;
    }
    
    if (!isAlive()) return;
    
    m_attackTimer -= deltaTime;
    if (m_attackTimer < 0.0f) m_attackTimer = 0.0f;
    
    UnitState prevState = m_state;
    
    switch (m_state) {
        case UnitState::Idle:
            updateIdle(deltaTime);
            break;
            
        case UnitState::Moving:
            updateMovement(deltaTime);
            break;
            
        case UnitState::AttackMoving:
            updateAttackMove(deltaTime);
            break;
            
        case UnitState::Attacking:
            updateCombat(deltaTime);
            break;
            
        default:
            // Custom states (Gathering, Returning, etc.) handled by subclasses
            updateCustomState(deltaTime);
            break;
    }
    
    // Update animations
    if (m_hasSprite) {
        m_animatedSprite.update(deltaTime);
        
        // Switch animation based on state
        if (m_state != prevState || m_animatedSprite.isFinished()) {
            switch (m_state) {
                case UnitState::Idle:
                    playAnimation(AnimationState::Idle);
                    break;
                case UnitState::Moving:
                case UnitState::AttackMoving:
                case UnitState::Returning:
                    playAnimation(AnimationState::Walk);
                    break;
                case UnitState::Attacking:
                    playAnimation(AnimationState::Attack);
                    break;
                case UnitState::Gathering:
                    playAnimation(AnimationState::Gather);
                    break;
                default:
                    break;
            }
        }
    }
    
    updateShape();
}

void Unit::render(sf::RenderTarget& target) {
    // Draw the unit shape
    target.draw(m_shape);
    
    // Draw selection indicator
    renderSelectionIndicator(target);
    
    // Draw health bar
    renderHealthBar(target);
}

void Unit::moveTo(sf::Vector2f target) {
    m_targetPosition = target;
    m_state = UnitState::Moving;
    playAnimation(AnimationState::Walk);
    findPath(target);
}

void Unit::attackMoveTo(sf::Vector2f target) {
    m_targetPosition = target;
    m_targetEntity.reset();  // Clear any previous attack target
    m_state = UnitState::AttackMoving;
    playAnimation(AnimationState::Walk);
    findPath(target);
}

void Unit::attack(EntityPtr target) {
    if (!target || !target->isAlive()) {
        m_state = UnitState::Idle;
        playAnimation(AnimationState::Idle);
        return;
    }
    
    m_targetEntity = target;
    m_targetPosition = target->getPosition();
    m_state = UnitState::Attacking;
    playAnimation(AnimationState::Attack);
    findPath(target->getPosition());
}

void Unit::stop() {
    m_state = UnitState::Idle;
    m_targetPosition = m_position;
    m_path.clear();
    playAnimation(AnimationState::Idle);
}

void Unit::takeDamage(int damage, EntityPtr attacker) {
    // Check if this damage will kill the unit
    bool willDie = (m_health > 0) && (m_health - damage <= 0);
    
    // Apply the damage first
    Entity::takeDamage(damage);
    
    // Spawn explosion effect when unit dies (scaled to unit size)
    if (willDie) {
        float explosionScale = std::max(m_size.x, m_size.y) / 64.0f;
        EFFECTS.spawnExplosion(m_position, explosionScale);
    }
    
    // Auto-retaliate if idle and not a worker
    if (m_state == UnitState::Idle && m_type != EntityType::Worker) {
        if (attacker && attacker->isAlive() && attacker->getTeam() != m_team) {
            attack(attacker);
        }
    }
}

void Unit::updateIdle(float deltaTime) {
    // Auto-attack: if idle combat unit and an enemy is in range, attack it
    if (m_isCombatUnit && findNearestEnemy) {
        float autoAttackRange = m_attackRange + m_autoAttackRangeBonus;
        EntityPtr enemy = findNearestEnemy(m_position, autoAttackRange, m_team);
        if (enemy && enemy->isAlive()) {
            attack(enemy);
        }
    }
}

void Unit::updateMovement(float deltaTime) {
    if (hasReachedTarget()) {
        m_state = UnitState::Idle;
        return;
    }
    followPath(deltaTime);
}

void Unit::updateAttackMove(float deltaTime) {
    // Check for enemies in attack range while moving
    if (findNearestEnemy) {
        float autoAttackRange = m_attackRange + m_autoAttackRangeBonus;
        EntityPtr enemy = findNearestEnemy(m_position, autoAttackRange, m_team);
        if (enemy && enemy->isAlive()) {
            // Found enemy in range - attack it but remember we're attack-moving
            float distance = getDistanceTo(enemy);
            
            if (distance <= m_attackRange) {
                // In range - attack if cooldown ready
                if (m_attackTimer <= 0.0f) {
                    enemy->takeDamage(m_damage, shared_from_this());
                    m_attackTimer = m_attackCooldown;
                    playAnimation(AnimationState::Attack);
                }
                // Stay in AttackMoving state, don't transition to full Attacking
                // This way when the enemy dies, we continue moving
                return;
            } else {
                // Enemy nearby but not in range - move towards them temporarily
                followPath(deltaTime);
                return;
            }
        }
    }
    
    // No enemies in range - continue moving to destination
    if (hasReachedTarget()) {
        m_state = UnitState::Idle;
        return;
    }
    followPath(deltaTime);
}

void Unit::updateCombat(float deltaTime) {
    auto target = m_targetEntity.lock();
    if (!target || !target->isAlive()) {
        m_state = UnitState::Idle;
        return;
    }
    
    float distance = getDistanceTo(target);
    
    if (distance > m_attackRange) {
        // Check if target moved significantly - recompute path
        sf::Vector2f targetPos = target->getPosition();
        sf::Vector2f pathEndDiff = targetPos - m_targetPosition;
        float pathEndDist = std::sqrt(pathEndDiff.x * pathEndDiff.x + pathEndDiff.y * pathEndDiff.y);
        
        if (pathEndDist > 50.0f || m_path.empty()) {
            m_targetPosition = targetPos;
            findPath(targetPos);
        }
        
        followPath(deltaTime);
    } else {
        // In range - attack if cooldown ready
        if (m_attackTimer <= 0.0f) {
            target->takeDamage(m_damage, shared_from_this());
            m_attackTimer = m_attackCooldown;
        }
    }
}

void Unit::updateCustomState(float deltaTime) {
    // Base implementation does nothing - subclasses override this
}

void Unit::followPath(float deltaTime) {
    // If no path or path exhausted, move directly to target
    if (m_path.empty() || m_pathIndex >= m_path.size()) {
        moveTowardsTarget(deltaTime);
        return;
    }
    
    // Move towards current waypoint
    sf::Vector2f waypoint = m_path[m_pathIndex];
    float distToWaypoint = MathUtil::distance(waypoint, m_position);
    
    if (distToWaypoint < 10.0f) {
        // Reached this waypoint, move to next
        m_pathIndex++;
        if (m_pathIndex >= m_path.size()) {
            // Path exhausted, move directly to final target
            moveTowardsTarget(deltaTime);
            return;
        }
        waypoint = m_path[m_pathIndex];
    }
    
    // Move towards waypoint
    sf::Vector2f originalTarget = m_targetPosition;
    m_targetPosition = waypoint;
    moveTowardsTarget(deltaTime);
    m_targetPosition = originalTarget;
}

void Unit::moveTowardsTarget(float deltaTime) {
    float distance = MathUtil::distance(m_targetPosition, m_position);
    
    if (distance < 1.0f) {
        m_position = m_targetPosition;
        return;
    }
    
    // Normalize direction
    sf::Vector2f diff = m_targetPosition - m_position;
    sf::Vector2f direction = diff / distance;
    
    // Update sprite facing direction
    updateSpriteDirection(direction);
    
    // Calculate desired move
    float moveDistance = m_speed * deltaTime;
    if (moveDistance > distance) {
        moveDistance = distance;
    }
    
    sf::Vector2f newPosition = m_position + direction * moveDistance;
    
    // Check collision if callback is set and we are collidable
    if (checkPositionBlocked && isCollidable()) {
        float radius = getCollisionRadius();
        if (checkPositionBlocked(newPosition, radius, this)) {
            // Try to slide along obstacles by checking perpendicular directions
            sf::Vector2f perpendicular(-direction.y, direction.x);
            
            // Try sliding right
            sf::Vector2f slideRight = m_position + perpendicular * moveDistance * 0.7f;
            if (!checkPositionBlocked(slideRight, radius, this)) {
                m_position = slideRight;
                return;
            }
            
            // Try sliding left
            sf::Vector2f slideLeft = m_position - perpendicular * moveDistance * 0.7f;
            if (!checkPositionBlocked(slideLeft, radius, this)) {
                m_position = slideLeft;
                return;
            }
            
            // Blocked completely, don't move
            return;
        }
    }
    
    m_position = newPosition;
}

bool Unit::hasReachedTarget() const {
    float distance = MathUtil::distance(m_targetPosition, m_position);
    return distance < 5.0f;
}

void Unit::findPath(sf::Vector2f target) {
    m_path.clear();
    m_pathIndex = 0;
    
    if (m_map) {
        m_path = m_map->findPath(m_position, target);
    }
    
    // Fallback to direct path if no path found or no map
    if (m_path.empty()) {
        m_path.push_back(target);
    }
}

float Unit::getDistanceTo(sf::Vector2f pos) const {
    return MathUtil::distance(pos, m_position);
}

float Unit::getDistanceTo(EntityPtr entity) const {
    if (!entity) return 0.0f;
    return getDistanceTo(entity->getPosition());
}
