#include "Unit.h"
#include "Map.h"
#include "Building.h"
#include <unordered_map>
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
    // Default collision radius = visual half-size; subclasses may override.
    m_collisionRadius = std::max(m_size.x, m_size.y) / 2.0f;
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

    tickUnderAttack(deltaTime);
    
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
    m_navSamplePos        = m_position;
    m_navSampleTimer      = 0.0f;
    m_navStuckTimer       = 0.0f;
    m_persistentStuckTime = 0.0f;
    playAnimation(AnimationState::Walk);
    findPath(target);
}

void Unit::attackMoveTo(sf::Vector2f target) {
    m_targetPosition = target;
    m_targetEntity.reset();  // Clear any previous attack target
    m_state = UnitState::AttackMoving;
    m_stuckTimer = 0.0f;
    m_lastDistanceToTarget = 0.0f;
    m_navSamplePos        = m_position;
    m_navSampleTimer      = 0.0f;
    m_navStuckTimer       = 0.0f;
    m_persistentStuckTime = 0.0f;
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
    // Check for enemies within awareness range while moving
    if (m_context) {
        // Use minimum awareness range so melee units can detect ranged threats
        float searchRange = std::max(m_attackRange + m_autoAttackRangeBonus, MIN_AWARENESS_RANGE);
        EntityPtr enemy = m_context->findPriorityEnemy(m_position, searchRange, m_team);
        
        if (enemy && enemy->isAlive()) {
            float distance = getDistanceTo(enemy);
            
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
            } else {
                // Enemy detected but outside attack range - chase them!
                // Queue the original destination as next action so we resume after killing
                sf::Vector2f destination = m_targetPosition;
                m_actionQueue.push_front([this, destination]() {
                    attackMoveTo(destination);
                });
                // Transition to Attacking state to chase the enemy
                beginAttacking(enemy, false);
                return;
            }
        }
    }
    
    // No enemies in awareness range - continue moving to destination
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
        // Determine current target's priority (lower number = higher priority)
        // 0 = combat unit, 1 = worker, 2 = building
        int currentPriority = 0;
        if (target->asBuilding()) {
            currentPriority = 2;
        } else if (target->getType() == EntityType::Worker) {
            currentPriority = 1;
        }
        
        // If not targeting highest priority (combat unit), scan for better targets
        if (currentPriority > 0) {
            float searchRange = std::max(m_attackRange + m_autoAttackRangeBonus, MIN_AWARENESS_RANGE);
            EntityPtr betterTarget = m_context->findPriorityEnemy(m_position, searchRange, m_team);
            
            if (betterTarget && betterTarget->isAlive() && betterTarget != target) {
                // Check if the new target has higher priority
                int newPriority = 0;
                if (betterTarget->asBuilding()) {
                    newPriority = 2;
                } else if (betterTarget->getType() == EntityType::Worker) {
                    newPriority = 1;
                }
                
                if (newPriority < currentPriority) {
                    // Found a higher-priority target - switch to it
                    switchTarget(betterTarget);
                    return;
                }
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
    // Reset the obstacle flag; moveTowardsTarget will set it if fully blocked.
    m_blockedByStaticObstacle = false;

    bool useWaypoint = !m_path.empty() && m_pathIndex < m_path.size();

    if (useWaypoint) {
        sf::Vector2f waypoint = m_path[m_pathIndex];
        float distToWaypoint = MathUtil::distance(waypoint, m_position);

        if (distToWaypoint < 10.0f) {
            m_pathIndex++;
            useWaypoint = m_pathIndex < m_path.size();
            if (useWaypoint)
                waypoint = m_path[m_pathIndex];
        }

        if (useWaypoint) {
            // Temporarily redirect movement toward the next waypoint while
            // keepingm_targetPosition pointing at the final destination for
            // the probe's progress calculation and for the replan below.
            sf::Vector2f originalTarget = m_targetPosition;
            m_targetPosition = waypoint;
            moveTowardsTarget(deltaTime);
            m_targetPosition = originalTarget;
        } else {
            moveTowardsTarget(deltaTime);  // path exhausted mid-tick
        }
    } else {
        moveTowardsTarget(deltaTime);  // no path, go direct
    }

    // --- Path-blocked replan -------------------------------------------
    // Only triggers when ALL probe directions are blocked (very rare with 7
    // probes, but handles truly pinned units). After the threshold the path
    // is rebuilt from the current position, giving A* a fresh start point
    // that naturally routes around whichever obstacle caused the jam.
    if (m_blockedByStaticObstacle) {
        m_pathBlockedTimer += deltaTime;
        if (m_pathBlockedTimer >= PATH_REPLAN_BLOCKED_TIME) {
            findPath(m_targetPosition);
            m_pathBlockedTimer = 0.0f;
        }
    } else {
        m_pathBlockedTimer = 0.0f;
    }

    // --- Net-displacement stuck detection ---------------------------------
    // Catches oscillation (e.g. bouncing at a building-corner junction) where
    // the unit is always finding some probe direction but never making real
    // forward progress.  Every NAV_SAMPLE_INTERVAL seconds we measure how far
    // the unit physically moved from the last sample point.  If net progress
    // is below NAV_MIN_PROGRESS we accumulate m_navStuckTimer; once it
    // exceeds NAV_STUCK_THRESHOLD we replan from the current position so A*
    // can find a fresh route (one that hopefully avoids the tight junction).
    if (!m_path.empty() && m_pathIndex < m_path.size()) {
        m_navSampleTimer += deltaTime;
        if (m_navSampleTimer >= NAV_SAMPLE_INTERVAL) {
            float moved = MathUtil::distance(m_position, m_navSamplePos);
            if (moved < NAV_MIN_PROGRESS) {
                m_navStuckTimer       += m_navSampleTimer;
                m_persistentStuckTime += m_navSampleTimer;  // never reset by replan
            } else {
                m_navStuckTimer = 0.0f;
            }
            m_navSamplePos   = m_position;
            m_navSampleTimer = 0.0f;
            if (m_navStuckTimer >= NAV_STUCK_THRESHOLD) {
                // If the blockage looks unit-caused (no static obstacle flag),
                // replan with soft unit-position costs so A* routes around the
                // crowd.  Static-obstacle jams still use a plain replan since
                // adding unit costs doesn't help when a wall is responsible.
                if (!m_blockedByStaticObstacle)
                    findPathAvoiding(m_targetPosition);
                else
                    findPath(m_targetPosition);
                m_navStuckTimer = 0.0f;
            }
        }
    } else {
        m_navSampleTimer = 0.0f;
        m_navStuckTimer  = 0.0f;
    }
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
    
    // Compute RVO-adjusted velocity to avoid collisions with other units.
    // Skip RVO in the final approach so the unit walks straight to its exact
    // destination without being deflected by neighbours clustering nearby.
    const float arrivalCutoff = getCollisionRadius() * 3.0f;
    sf::Vector2f newVelocity = (distance < arrivalCutoff)
        ? preferredVelocity
        : computeRVOVelocity(preferredVelocity, deltaTime);
    
    // Apply velocity
    sf::Vector2f newPosition = m_position + newVelocity * deltaTime;

    // Helper: returns true if a world position is on a water tile.
    // Intentionally only checks for TileType::Water, NOT TileType::Building.
    // Building tiles are non-walkable in the tile map, but their collision is
    // already enforced by the bounds.contains() check in checkPositionBlocked.
    // Checking building tiles here as well makes it geometrically impossible
    // for small units to squeeze diagonally between two corner-touching
    // buildings, because the unit center briefly enters the corner tile
    // (which belongs to one of the buildings) during the traverse.
    auto onWaterTile = [&](sf::Vector2f pos) -> bool {
        if (!m_map) return false;
        sf::Vector2i t = m_map->worldToTile(pos);
        if (!m_map->isValidTile(t.x, t.y)) return true; // out of bounds = blocked
        return m_map->getTile(t.x, t.y).type == TileType::Water;
    };

    // Still check for static obstacles (buildings, resources) AND water tiles.
    if (m_context && isCollidable()) {
        float radius = getCollisionRadius();
        bool cpb   = m_context->checkPositionBlocked(newPosition, radius, this);
        bool water = onWaterTile(newPosition);
        if (cpb || water) {
            // Probe 7 alternate directions (every 45°, skipping the already-blocked
            // forward direction) and choose the non-blocked one that makes the most
            // angular progress toward the actual destination. This cleanly handles
            // corners where the old ±90° slide would oscillate.
            constexpr float PROBE_DEG[] = { 90.f, -90.f, 45.f, -45.f, 135.f, -135.f, 180.f };
            constexpr float DEG_TO_RAD = MathUtil::PI / 180.0f;

            float baseAngle = std::atan2(direction.y, direction.x);
            float moveDist  = m_speed * deltaTime;

            // Direction vector toward the true destination (not necessarily the waypoint).
            sf::Vector2f toTarget = m_targetPosition - m_position;
            float ttLen = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y);
            sf::Vector2f targetDir = (ttLen > 0.01f) ? (toTarget / ttLen) : direction;

            sf::Vector2f bestPos;
            float bestDot = -2.0f;  // sentinel: no valid candidate yet

            for (float deg : PROBE_DEG) {
                float angle = baseAngle + deg * DEG_TO_RAD;
                sf::Vector2f probeDir(std::cos(angle), std::sin(angle));
                sf::Vector2f probePos = m_position + probeDir * moveDist;

                bool pb = m_context->checkPositionBlocked(probePos, radius, this);
                bool pw = onWaterTile(probePos);
                if (!pb && !pw) {
                    float dot = probeDir.x * targetDir.x + probeDir.y * targetDir.y;
                    if (dot > bestDot) {
                        bestDot = dot;
                        bestPos = probePos;
                    }
                }
            }

            if (bestDot > -1.5f) {
                // Found a legal direction — slide along the obstacle surface.
                m_velocity = (bestPos - m_position) / deltaTime;
                m_position = bestPos;
                // Not fully blocked; leave m_blockedByStaticObstacle as-is (false).
            } else {
                // All probe directions blocked — mark so the replan timer
                // requests a fresh A* path from the current position.
                m_velocity = sf::Vector2f(0.0f, 0.0f);
                m_blockedByStaticObstacle = true;
            }
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

    // Exactly at target.
    if (distance < 5.0f) {
        m_stuckTimer = 0.0f;
        return true;
    }

    // D: persistent stuck — unit has been making no net progress for too long
    // across multiple replan cycles and is clearly jammed behind allies.
    // Go idle regardless of distance so it stops trembling.
    if (m_persistentStuckTime >= PERSISTENT_STUCK_IDLE)
        return true;

    // If still far away there is no point tracking stuck time yet; the unit is
    // legitimately navigating toward the destination.
    if (distance > GROUP_ARRIVAL_RADIUS_FAR) {
        m_stuckTimer = 0.0f;
        m_lastDistanceToTarget = distance;
        return false;
    }

    // Within GROUP_ARRIVAL_RADIUS_FAR: measure progress toward destination.
    // Threshold is speed-proportional so slow units aren't mistakenly flagged
    // as stuck when they are still closing in on the target.
    const bool makingProgress = (m_lastDistanceToTarget > 0.0f &&
                                 m_lastDistanceToTarget - distance > 0.5f);
    if (makingProgress)
        m_stuckTimer = 0.0f;
    else
        m_stuckTimer += deltaTime;

    m_lastDistanceToTarget = distance;

    // Tight check: just arrived at the tile (original behaviour).
    if (distance <= GROUP_ARRIVAL_RADIUS && m_stuckTimer >= STUCK_THRESHOLD_TIME)
        return true;

    // Wide check: held back by packed allies — stop and become idle nearby.
    if (m_stuckTimer >= STUCK_THRESHOLD_FAR)
        return true;

    return false;
}

void Unit::findPath(sf::Vector2f target) {
    m_path.clear();
    m_pathIndex = 0;

    if (m_map) {
        sf::Vector2i goalTile = m_map->worldToTile(target);
        // Only a goal on a *walkable* tile can be a genuine "unreachable island"
        // partial-path case.  Non-walkable goals (buildings, water clicked directly)
        // are intentionally redirected by Map::findPath to the nearest adjacent
        // walkable tile; clamping m_targetPosition in that case would place it
        // 1-2 tiles away from the entity, breaking proximity checks like the
        // worker's 50 px delivery distance.
        bool goalWasWalkable = m_map->isValidTile(goalTile.x, goalTile.y)
                            && m_map->isWalkable(goalTile.x, goalTile.y);

        m_path = m_map->findPath(m_position, target, m_collisionRadius);

        if (!m_path.empty()) {
            sf::Vector2i reachedTile = m_map->worldToTile(m_path.back());
            // Clamp m_targetPosition only for genuine partial paths (disconnected
            // walkable regions) so followPath doesn't drive the unit into water
            // after exhausting the waypoints.
            if (goalWasWalkable && goalTile != reachedTile) {
                m_targetPosition = m_path.back();
            }
        } else {
            // Empty path = unit is already at the closest reachable tile.
            // Redirect m_targetPosition so hasReachedTarget() fires immediately.
            m_targetPosition = m_position;
        }
        return;
    }

    // No map — fall back to a direct move only as a last resort.
    m_path.push_back(target);
}

void Unit::findPathAvoiding(sf::Vector2f target) {
    if (!m_map || !m_context) { findPath(target); return; }

    // Build a per-tile soft-cost map from nearby nearly-stationary units.
    // Only idle/slow units contribute — moving ones are handled by RVO already.
    std::unordered_map<int, float> extraCosts;
    constexpr float GATHER_RADIUS  = 200.0f;
    constexpr float IDLE_SPEED_MAX = 30.0f;   // px/s — below this = "parked"
    constexpr float CENTER_PENALTY =  6.0f;   // cost added to the occupied tile
    constexpr float RING_PENALTY   =  2.0f;   // cost added to each cardinal neighbour

    const int mapW     = m_map->getWidth();
    sf::Vector2i goalT = m_map->worldToTile(target);

    auto neighbors = m_context->getNearbyUnitsRVO(m_position, GATHER_RADIUS, this);
    for (const auto& nb : neighbors) {
        float spdSq = nb.velocity.x * nb.velocity.x
                    + nb.velocity.y * nb.velocity.y;
        if (spdSq > IDLE_SPEED_MAX * IDLE_SPEED_MAX) continue;  // skip moving units

        sf::Vector2i t = m_map->worldToTile(nb.position);
        if (t == goalT) continue;   // never penalise the destination tile itself

        extraCosts[t.y * mapW + t.x] += CENTER_PENALTY;

        // Penalise cardinal neighbours so the path curves before the crowd,
        // not just one tile to the side of it.
        const int odx[] = { 1, -1,  0,  0 };
        const int ody[] = { 0,  0,  1, -1 };
        for (int k = 0; k < 4; ++k) {
            int tx = t.x + odx[k], ty = t.y + ody[k];
            if (m_map->isValidTile(tx, ty) && sf::Vector2i{tx, ty} != goalT)
                extraCosts[ty * mapW + tx] += RING_PENALTY;
        }
    }

    // Fall back to plain findPath when no nearby idle units were found.
    if (extraCosts.empty()) { findPath(target); return; }

    // Reuse all the same goal-snapping / partial-path logic as findPath().
    m_path.clear();
    m_pathIndex = 0;

    bool goalWasWalkable = m_map->isValidTile(goalT.x, goalT.y)
                        && m_map->isWalkable(goalT.x, goalT.y);

    m_path = m_map->findPath(m_position, target, m_collisionRadius, extraCosts);

    if (!m_path.empty()) {
        sf::Vector2i reachedTile = m_map->worldToTile(m_path.back());
        if (goalWasWalkable && goalT != reachedTile)
            m_targetPosition = m_path.back();
    } else {
        m_targetPosition = m_position;
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
