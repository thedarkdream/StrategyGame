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
            
        case UnitState::Following:
            updateFollowing(deltaTime);
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
                case UnitState::Following:
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
    // Draw animated sprite if available, otherwise fallback to colored shape
    if (m_hasSprite) {
        m_animatedSprite.render(target, m_position);
    } else {
        target.draw(m_shape);
    }

    // Draw selection indicator
    renderSelectionIndicator(target);

    // Draw health bar
    renderHealthBar(target);
}

void Unit::moveTo(sf::Vector2f target) {
    m_targetPosition = target;
    m_state = UnitState::Moving;
    m_stuckTimer = 0.0f;
    m_lastDistanceToTarget = 0.0f;
    playAnimation(AnimationState::Walk);
    findPath(target);
}

void Unit::attackMoveTo(sf::Vector2f target) {
    m_targetPosition = target;
    m_targetEntity.reset();  // Clear any previous attack target
    m_state = UnitState::AttackMoving;
    m_stuckTimer = 0.0f;
    m_lastDistanceToTarget = 0.0f;
    playAnimation(AnimationState::Walk);
    findPath(target);
}

void Unit::attack(EntityPtr target) {
    beginAttacking(target, true);  // Explicit attack command
}

void Unit::beginAttacking(EntityPtr target, bool isExplicit) {
    if (!target || !target->isAlive() || target.get() == this) {
        m_state = UnitState::Idle;
        playAnimation(AnimationState::Idle);
        return;
    }
    
    m_targetEntity = target;
    m_targetPosition = target->getPosition();
    m_state = UnitState::Attacking;
    m_isExplicitAttack = isExplicit;
    resetChaseTracking(target);
    playAnimation(AnimationState::Attack);
    findPath(target->getPosition());
}

void Unit::switchTarget(EntityPtr newTarget) {
    if (!newTarget || !newTarget->isAlive()) return;
    
    m_targetEntity = newTarget;
    m_targetPosition = newTarget->getPosition();
    resetChaseTracking(newTarget);
    findPath(newTarget->getPosition());
}

void Unit::resetChaseTracking(EntityPtr target) {
    m_chaseTimer = 0.0f;
    m_chaseProgressTimer = 0.0f;
    m_lastChaseDistance = target ? getDistanceTo(target) : 0.0f;
}

void Unit::stop() {
    m_actionQueue.clear();
    m_state = UnitState::Idle;
    m_targetPosition = m_position;
    m_path.clear();
    m_followTarget.reset();
    m_velocity = sf::Vector2f(0.0f, 0.0f);
    playAnimation(AnimationState::Idle);
}

void Unit::follow(EntityPtr target) {
    if (!target || !target->isAlive() || target.get() == this) {
        return;
    }
    
    m_followTarget = target;
    m_targetPosition = target->getPosition();
    m_state = UnitState::Following;
    playAnimation(AnimationState::Walk);
    findPath(target->getPosition());
}

// ---------------------------------------------------------------------------
// Action queue
// ---------------------------------------------------------------------------

void Unit::clearActionQueue() {
    m_actionQueue.clear();
}

void Unit::appendToQueue(std::function<void()> action) {
    // Execute immediately if the unit is idle and has nothing pending;
    // otherwise append so it runs after the current action finishes.
    if (m_state == UnitState::Idle && m_actionQueue.empty()) {
        action();
    } else {
        m_actionQueue.push_back(std::move(action));
    }
}

bool Unit::popNextAction() {
    if (m_actionQueue.empty()) return false;
    auto action = std::move(m_actionQueue.front());
    m_actionQueue.pop_front();
    action();
    return true;
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
            beginAttacking(attacker, false);  // Auto-retaliation
        }
    }
    
    // Store last attacker for counter-attack logic
    if (attacker && attacker->isAlive() && attacker->getTeam() != m_team) {
        m_lastAttacker = attacker;
    }
}

void Unit::updateIdle(float deltaTime) {
    // Auto-attack: if idle combat unit and an enemy is in range, attack it
    if (m_isCombatUnit && m_context) {
        float autoAttackRange = m_attackRange + m_autoAttackRangeBonus;
        EntityPtr enemy = m_context->findPriorityEnemy(m_position, autoAttackRange, m_team);
        if (enemy && enemy->isAlive()) {
            beginAttacking(enemy, false);  // Auto-attack
        }
    }
}

void Unit::updateMovement(float deltaTime) {
    if (hasGroupArrived(deltaTime)) {
        if (!popNextAction()) m_state = UnitState::Idle;
        return;
    }
    followPath(deltaTime);
}

void Unit::updateAttackMove(float deltaTime) {
    // Check for enemies in attack range while moving
    if (m_context) {
        float autoAttackRange = m_attackRange + m_autoAttackRangeBonus;
        EntityPtr enemy = m_context->findPriorityEnemy(m_position, autoAttackRange, m_team);
        
        if (enemy && enemy->isAlive()) {
            float distance = getDistanceTo(enemy);
            
            // Counter-attack priority: if we're being attacked by a unit while
            // targeting a building, prefer to attack the threatening unit
            if (enemy->asBuilding()) {
                auto attacker = m_lastAttacker.lock();
                if (attacker && attacker->isAlive() && attacker->asUnit() && 
                    attacker->getTeam() != m_team) {
                    float attackerDist = getDistanceTo(attacker);
                    if (attackerDist <= autoAttackRange) {
                        // Switch to the attacking unit
                        enemy = attacker;
                        distance = attackerDist;
                        m_lastAttacker.reset();
                    }
                }
            }
            
            if (distance <= m_attackRange) {
                // In range - attack if cooldown ready
                if (m_attackTimer <= 0.0f) {
                    fireAttack(enemy);
                    m_attackTimer = m_attackCooldown;
                    playAnimation(AnimationState::Attack);
                }
                // Stay in AttackMoving state, don't transition to full Attacking
                // This way when the enemy dies, we continue moving
                return;
            }
            // If enemy is nearby but not in range, just continue moving to destination
            // Don't stand around waiting - attack-move means prioritize movement
        }
    }
    
    // No enemies in range (or enemies only in detection range) - continue moving to destination
    if (hasGroupArrived(deltaTime)) {
        if (!popNextAction()) m_state = UnitState::Idle;
        return;
    }
    followPath(deltaTime);
}

void Unit::updateCombat(float deltaTime) {
    auto target = m_targetEntity.lock();
    if (!target || !target->isAlive()) {
        if (!popNextAction()) m_state = UnitState::Idle;
        return;
    }
    
    float distance = getDistanceTo(target);
    
    // For auto-attacks only: check if we should switch targets
    if (!m_isExplicitAttack && m_context) {
        // Counter-attack: if we're being attacked by a higher-priority target, switch to it
        // Priority: combat units > workers > buildings
        auto attacker = m_lastAttacker.lock();
        if (attacker && attacker->isAlive() && attacker->asUnit() && attacker->getTeam() != m_team) {
            bool shouldSwitch = false;
            
            if (target->asBuilding()) {
                // Always switch from building to any unit attacking us
                shouldSwitch = true;
            } else if (target->getType() == EntityType::Worker && attacker->getType() != EntityType::Worker) {
                // Switch from worker to combat unit attacking us
                shouldSwitch = true;
            }
            
            if (shouldSwitch) {
                switchTarget(attacker);
                m_lastAttacker.reset();
                return;
            }
        }
        
        // Check if target is fleeing/unreachable
        if (distance > m_attackRange) {
            m_chaseTimer += deltaTime;
            m_chaseProgressTimer += deltaTime;
            
            // Check progress periodically
            if (m_chaseProgressTimer >= CHASE_PROGRESS_CHECK) {
                m_chaseProgressTimer = 0.0f;
                float progress = m_lastChaseDistance - distance;
                
                // If we're not getting closer (or target is getting away), increment chase timer faster
                if (progress < 5.0f) {
                    m_chaseTimer += CHASE_PROGRESS_CHECK;  // Double count this period
                }
                m_lastChaseDistance = distance;
            }
            
            // Give up chase if taking too long
            if (m_chaseTimer >= CHASE_TIMEOUT) {
                // Look for a new target with priority - use minimum awareness range
                // so melee units can detect ranged threats
                float searchRange = std::max(m_attackRange + m_autoAttackRangeBonus, MIN_AWARENESS_RANGE);
                EntityPtr newTarget = m_context->findPriorityEnemy(m_position, searchRange, m_team);
                
                if (newTarget && newTarget->isAlive() && newTarget != target) {
                    // Found better target - switch to it
                    switchTarget(newTarget);
                    return;
                } else {
                    // No better target - go back to idle
                    if (!popNextAction()) m_state = UnitState::Idle;
                    return;
                }
            }
        }
    }
    
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
            fireAttack(target);
            m_attackTimer = m_attackCooldown;
            // Reset chase timer on successful attack
            resetChaseTracking(target);
        }
    }
}

void Unit::updateFollowing(float deltaTime) {
    auto target = m_followTarget.lock();
    if (!target || !target->isAlive()) {
        // Target gone - stop following
        m_followTarget.reset();
        if (!popNextAction()) m_state = UnitState::Idle;
        return;
    }
    
    float distance = getDistanceTo(target);
    
    if (distance <= FOLLOW_DISTANCE) {
        // Close enough - wait for target to move
        // Don't transition to Idle, stay in Following state
        // Just stop moving but keep tracking
        return;
    }
    
    // Target is far - move towards them
    sf::Vector2f targetPos = target->getPosition();
    sf::Vector2f pathEndDiff = targetPos - m_targetPosition;
    float pathEndDist = std::sqrt(pathEndDiff.x * pathEndDiff.x + pathEndDiff.y * pathEndDiff.y);
    
    // Recompute path if target moved significantly
    if (pathEndDist > 30.0f || m_path.empty()) {
        m_targetPosition = targetPos;
        findPath(targetPos);
    }
    
    followPath(deltaTime);
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
        m_velocity = sf::Vector2f(0.0f, 0.0f);
        return;
    }
    
    // Calculate preferred velocity (direct path to target at max speed)
    sf::Vector2f diff = m_targetPosition - m_position;
    sf::Vector2f direction = diff / distance;
    float desiredSpeed = std::min(m_speed, distance / deltaTime);
    sf::Vector2f preferredVelocity = direction * desiredSpeed;
    
    // Update sprite facing direction (use preferred direction, not RVO-adjusted)
    updateSpriteDirection(direction);
    
    // Compute RVO-adjusted velocity to avoid collisions with other units
    sf::Vector2f newVelocity = computeRVOVelocity(preferredVelocity, deltaTime);
    
    // Apply velocity
    sf::Vector2f newPosition = m_position + newVelocity * deltaTime;
    
    // Still check for static obstacles (buildings, resources)
    if (m_context && isCollidable()) {
        float radius = getCollisionRadius();
        if (m_context->checkPositionBlocked(newPosition, radius, this)) {
            // Try to slide along static obstacles
            sf::Vector2f perpendicular(-direction.y, direction.x);
            float moveDistance = m_speed * deltaTime;
            
            // Try sliding right
            sf::Vector2f slideRight = m_position + perpendicular * moveDistance * 0.7f;
            if (!m_context->checkPositionBlocked(slideRight, radius, this)) {
                m_position = slideRight;
                m_velocity = perpendicular * m_speed * 0.7f;
                return;
            }
            
            // Try sliding left
            sf::Vector2f slideLeft = m_position - perpendicular * moveDistance * 0.7f;
            if (!m_context->checkPositionBlocked(slideLeft, radius, this)) {
                m_position = slideLeft;
                m_velocity = -perpendicular * m_speed * 0.7f;
                return;
            }
            
            // Blocked completely by static obstacle
            m_velocity = sf::Vector2f(0.0f, 0.0f);
            return;
        }
    }
    
    m_position = newPosition;
    m_velocity = newVelocity;
}

bool Unit::hasReachedTarget() const {
    float distance = MathUtil::distance(m_targetPosition, m_position);
    return distance < 5.0f;
}

void Unit::fireAttack(EntityPtr target) {
    // Base implementation: instant melee/ranged damage
    if (target && target->isAlive()) {
        target->takeDamage(m_damage, shared_from_this());
    }
}

bool Unit::hasGroupArrived(float deltaTime) {
    float distance = MathUtil::distance(m_targetPosition, m_position);
    
    // If actually reached target, we're done
    if (distance < 5.0f) {
        return true;
    }
    
    // Only consider group arrival if within reasonable distance
    if (distance > GROUP_ARRIVAL_RADIUS) {
        m_stuckTimer = 0.0f;
        m_lastDistanceToTarget = distance;
        return false;
    }
    
    // Check if we're making progress
    float progressThreshold = 2.0f;  // Minimum movement to consider progress
    if (m_lastDistanceToTarget > 0.0f && m_lastDistanceToTarget - distance > progressThreshold) {
        // Making progress, reset timer
        m_stuckTimer = 0.0f;
    } else {
        // Not making progress, increment timer
        m_stuckTimer += deltaTime;
    }
    
    m_lastDistanceToTarget = distance;
    
    // If stuck for long enough near destination, consider arrived
    return m_stuckTimer >= STUCK_THRESHOLD_TIME;
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

sf::Vector2f Unit::computeRVOVelocity(sf::Vector2f preferredVelocity, float deltaTime) {
    // If no context, just return preferred velocity
    if (!m_context) {
        return preferredVelocity;
    }
    
    // Get nearby units
    std::vector<RVONeighbor> neighbors = m_context->getNearbyUnitsRVO(m_position, RVO_NEIGHBOR_DIST, this);
    
    if (neighbors.empty()) {
        return preferredVelocity;
    }
    
    float myRadius = getCollisionRadius();
    sf::Vector2f adjustedVelocity = preferredVelocity;
    
    // ORCA: For each neighbor, compute a half-plane of permitted velocities
    // and adjust our velocity to stay within all half-planes
    for (const RVONeighbor& neighbor : neighbors) {
        sf::Vector2f relativePosition = neighbor.position - m_position;
        sf::Vector2f relativeVelocity = m_velocity - neighbor.velocity;
        float combinedRadius = myRadius + neighbor.radius;
        
        float distSq = relativePosition.x * relativePosition.x + relativePosition.y * relativePosition.y;
        float dist = std::sqrt(distSq);
        
        // Skip if too far apart (no collision possible in time horizon)
        float collisionDist = combinedRadius + m_speed * RVO_TIME_HORIZON;
        if (dist > collisionDist) {
            continue;
        }
        
        // Calculate the direction from us to the neighbor
        sf::Vector2f toNeighbor = relativePosition / dist;
        
        // Time to collision if we continue with relative velocity
        // Project relative velocity onto line to neighbor
        float velProjection = relativeVelocity.x * toNeighbor.x + relativeVelocity.y * toNeighbor.y;
        
        // Distance to collision
        float collisionGap = dist - combinedRadius;
        
        // If already overlapping or will collide soon, we need to adjust
        if (collisionGap < 0.0f) {
            // Already overlapping - push apart immediately
            // Adjust velocity to move away from neighbor
            sf::Vector2f pushDir = -toNeighbor;  // Direction away from neighbor
            float pushStrength = m_speed * 0.8f;
            
            // Add push to adjusted velocity
            adjustedVelocity = adjustedVelocity + pushDir * pushStrength;
        } else if (velProjection > 0.0f) {
            // We're moving towards each other
            float timeToCollision = collisionGap / velProjection;
            
            if (timeToCollision < RVO_TIME_HORIZON) {
                // Collision predicted within time horizon
                // The closer the collision, the stronger the avoidance
                float urgency = 1.0f - (timeToCollision / RVO_TIME_HORIZON);
                urgency = urgency * urgency;  // Quadratic falloff - more urgent when close
                
                // Calculate avoidance direction (perpendicular to collision line)
                // Choose the direction that requires less change from preferred velocity
                sf::Vector2f perpRight(-toNeighbor.y, toNeighbor.x);
                sf::Vector2f perpLeft(toNeighbor.y, -toNeighbor.x);
                
                // Determine which side is "cheaper" based on preferred velocity
                float dotRight = preferredVelocity.x * perpRight.x + preferredVelocity.y * perpRight.y;
                sf::Vector2f avoidDir = (dotRight >= 0) ? perpRight : perpLeft;
                
                // RVO adjustment: each agent takes half responsibility
                float avoidStrength = velProjection * urgency * 0.5f;
                
                // Apply avoidance
                // Reduce velocity component towards neighbor
                adjustedVelocity.x -= toNeighbor.x * velProjection * urgency * 0.5f;
                adjustedVelocity.y -= toNeighbor.y * velProjection * urgency * 0.5f;
                
                // Add perpendicular avoidance component
                adjustedVelocity.x += avoidDir.x * avoidStrength;
                adjustedVelocity.y += avoidDir.y * avoidStrength;
            }
        }
    }
    
    // Clamp velocity magnitude to max speed
    float velMag = std::sqrt(adjustedVelocity.x * adjustedVelocity.x + adjustedVelocity.y * adjustedVelocity.y);
    if (velMag > m_speed) {
        adjustedVelocity = adjustedVelocity * (m_speed / velMag);
    }
    
    return adjustedVelocity;
}
