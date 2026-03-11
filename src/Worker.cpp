#include "Worker.h"
#include "Building.h"
#include "ResourceNode.h"
#include "EntityData.h"
#include "Constants.h"
#include "Animation.h"
#include "MathUtil.h"
#include "Map.h"
#include "TextureManager.h"
#include "SoundManager.h"
#include <cmath>
#include <limits>

Worker::Worker(Team team, sf::Vector2f position)
    : Unit(EntityType::Worker, team, position)
{
    // Load worker sprites
    loadAnimations("units/worker");
}

bool Worker::isCollidable() const {
    // Workers are not collidable when gathering or returning resources
    // This allows them to stack at resource nodes
    return m_state != UnitState::Gathering && m_state != UnitState::Returning;
}

void Worker::escapeCollisionIfNeeded() {
    // When transitioning from gathering (non-collidable) to any other state (collidable),
    // the worker may be overlapping with a resource/building. Move them out.
    if (m_context) {
        if (m_context->checkPositionBlocked(m_position, getCollisionRadius(), this)) {
            sf::Vector2f freePos = m_context->findFreePosition(
                m_position, getCollisionRadius(), 100.0f, this);
            m_position = freePos;
        }
    }
}

void Worker::moveTo(sf::Vector2f target) {
    releaseMiningClaim();
    releaseBuildClaim();
    escapeCollisionIfNeeded();
    Unit::moveTo(target);
}

void Worker::attackMoveTo(sf::Vector2f target) {
    releaseMiningClaim();
    releaseBuildClaim();
    escapeCollisionIfNeeded();
    Unit::attackMoveTo(target);
}

void Worker::render(sf::RenderTarget& target) {
    // Draw animated sprite if available, otherwise fallback to shape
    if (m_hasSprite) {
        m_animatedSprite.render(target, m_position);
    } else {
        target.draw(m_shape);
    }
    
    // Draw selection indicator
    renderSelectionIndicator(target);
    
    // Draw health bar
    renderHealthBar(target);
    
    // Draw carried resources indicator
    if (m_carriedResources > 0) {
        sf::CircleShape resourceIndicator(4.0f);
        resourceIndicator.setOrigin(sf::Vector2f(4.0f, 4.0f));
        resourceIndicator.setPosition(sf::Vector2f(m_position.x + m_size.x / 2.0f, m_position.y - m_size.y / 2.0f));
        resourceIndicator.setFillColor(sf::Color::Cyan);
        target.draw(resourceIndicator);
    }
}

void Worker::gather(EntityPtr resource) {
    if (!resource) {
        m_state = UnitState::Idle;
        playAnimation(AnimationState::Idle);
        return;
    }
    
    // Release any previous mining claim
    releaseMiningClaim();
    
    m_resourceTarget = resource;
    m_targetPosition = resource->getPosition();
    m_state = UnitState::Gathering;
    m_gatherTimer = 0.0f;
    m_isActivelyMining = false;
    playAnimation(AnimationState::Walk);  // Walk to resource first
    findPath(resource->getPosition());
}

void Worker::returnResources() {
    // Release mining claim before leaving
    releaseMiningClaim();
    
    if (auto base = m_homeBase.lock()) {
        m_targetPosition = base->getPosition();
        m_state = UnitState::Returning;
        playAnimation(AnimationState::Walk);
        findPath(base->getPosition());
    } else {
        m_state = UnitState::Idle;
        playAnimation(AnimationState::Idle);
    }
}

void Worker::updateCustomState(float deltaTime) {
    switch (m_state) {
        case UnitState::Gathering:
            updateGathering(deltaTime);
            break;
        case UnitState::Returning:
            updateReturning(deltaTime);
            break;
        case UnitState::Building:
            updateBuilding(deltaTime);
            break;
        default:
            break;
    }
}

void Worker::updateGathering(float deltaTime) {
    auto resource = m_resourceTarget.lock();
    if (!resource || !resource->isAlive()) {
        // Resource gone or exhausted - release claim and find replacement
        releaseMiningClaim();
        m_resourceTarget.reset();
        if (m_context) {
            EntityPtr newResource = m_context->findNearestAvailableResource(m_position, 200.0f, nullptr);
            if (newResource) {
                gather(newResource);
                return;
            }
        }
        if (!popNextAction()) m_state = UnitState::Idle;
        return;
    }
    
    // Move to resource
    float distance = getDistanceTo(resource);
    
    if (distance > 40.0f) {
        // Still traveling - not actively mining yet
        if (m_isActivelyMining) {
            releaseMiningClaim();
        }
        followPath(deltaTime);
    } else {
        // At the resource - try to claim if not already
        if (!m_isActivelyMining) {
            ResourceNode* resourceNode = resource->asResourceNode();
            if (resourceNode) {
                // Try to claim this mining spot
                // Need shared_from_this - pass ourselves as claimer
                auto self = std::dynamic_pointer_cast<Entity>(shared_from_this());
                if (!resourceNode->tryClaimMining(self)) {
                    // Spot is occupied - find another field
                    if (m_context) {
                        EntityPtr newResource = m_context->findNearestAvailableResource(m_position, 200.0f, resource);
                        if (newResource) {
                            gather(newResource);
                            return;
                        }
                    }
                    // No alternative found - wait for spot to free up
                    // (could also just stay here and retry next frame)
                    return;
                }
                m_isActivelyMining = true;
                // Face the resource and play gather animation
                sf::Vector2f direction = resource->getPosition() - m_position;
                updateSpriteDirection(direction);
                playAnimation(AnimationState::Gather);
            }
        }
        
        // Gathering
        m_gatherTimer += deltaTime;
        if (m_gatherTimer >= Constants::GATHERING_TIME) {
            // Get resources from the resource node
            ResourceNode* resourceNode = resource->asResourceNode();
            if (resourceNode) {
                m_carriedResources = resourceNode->harvestResource();
            }
            if (m_carriedResources > 0) {
                returnResources();
            } else {
                // Resource depleted - release claim and find replacement
                releaseMiningClaim();
                m_resourceTarget.reset();
                if (m_context) {
                    EntityPtr newResource = m_context->findNearestAvailableResource(m_position, 200.0f, nullptr);
                    if (newResource) {
                        gather(newResource);
                        return;
                    }
                }
                if (!popNextAction()) m_state = UnitState::Idle;
            }
        }
    }
}

void Worker::updateReturning(float deltaTime) {
    auto base = m_homeBase.lock();
    if (!base || !base->isAlive()) {
        // Base destroyed - try to find another base
        m_homeBase.reset();
        
        // Search for a new base using the map callback (would need to be added)
        // For now, just drop resources and go idle
        m_carriedResources = 0;
        if (!popNextAction()) m_state = UnitState::Idle;
        return;
    }
    
    float distance = getDistanceTo(base);
    
    if (distance > 50.0f) {
        followPath(deltaTime);
    } else {
        // Deposit resources
        if (m_carriedResources > 0 && m_context) {
            m_context->depositResources(m_team, m_carriedResources);
        }
        m_carriedResources = 0;
        
        // Resume gathering
        auto resource = m_resourceTarget.lock();
        if (resource && resource->isAlive()) {
            gather(resource);
        } else {
            // Original resource gone - try to find a nearby replacement
            m_resourceTarget.reset();
            if (m_context) {
                EntityPtr newResource = m_context->findNearestAvailableResource(m_position, 200.0f, nullptr);
                if (newResource) {
                    gather(newResource);
                    return;
                }
            }
            if (!popNextAction()) m_state = UnitState::Idle;
        }
    }
}

void Worker::releaseMiningClaim() {
    if (!m_isActivelyMining) return;
    
    if (auto resource = m_resourceTarget.lock()) {
        if (auto* resourceNode = resource->asResourceNode()) {
            auto self = std::dynamic_pointer_cast<Entity>(shared_from_this());
            resourceNode->releaseMining(self);
        }
    }
    m_isActivelyMining = false;
}

void Worker::buildAt(EntityPtr building) {
    if (!building) {
        m_state = UnitState::Idle;
        playAnimation(AnimationState::Idle);
        return;
    }
    
    // Release any previous claims
    releaseMiningClaim();
    releaseBuildClaim();
    
    // Escape collision if overlapping with entities
    escapeCollisionIfNeeded();
    
    m_buildTarget = building;

    // Pre-claim the builder slot immediately so no other worker can grab it while
    // this worker is walking.  Any previous builder is displaced — they will idle
    // when their updateBuilding runs and sees the slot is now taken by this worker.
    if (Building* b = building->asBuilding(); b && !b->isConstructed()) {
        b->releaseBuilder();  // No-op if slot is empty; otherwise displaces old holder
        auto self = std::dynamic_pointer_cast<Entity>(shared_from_this());
        b->assignBuilder(self);
    }
    
    // Find the closest *walkable* tile adjacent to the building footprint.
    // We must NOT use the raw geometric edge point because it often falls on a
    // non-walkable tile (the building itself, or an adjacent building).  A* would
    // then reroute to a different tile, but m_targetPosition would still point at
    // the blocked point, causing moveTowardsTarget to drive straight through the
    // building when the path is exhausted.
    sf::FloatRect bounds = building->getBounds();
    sf::Vector2f approachPoint = building->getPosition(); // fallback: center

    if (m_map) {
        // Tile footprint of the building
        sf::Vector2i bMin = m_map->worldToTile(bounds.position);
        sf::Vector2i bMax = m_map->worldToTile(
            sf::Vector2f(bounds.position.x + bounds.size.x - 1.0f,
                         bounds.position.y + bounds.size.y - 1.0f));

        // Search expanding rings around the footprint until a walkable tile is found.
        // Ring 1 = direct neighbours; ring 2+ handles buildings surrounded by others.
        bool found = false;
        for (int ring = 1; ring <= 5 && !found; ++ring) {
            float bestDist = std::numeric_limits<float>::max();
            sf::Vector2i bestTile(-1, -1);

            for (int tx = bMin.x - ring; tx <= bMax.x + ring; ++tx) {
                for (int ty = bMin.y - ring; ty <= bMax.y + ring; ++ty) {
                    // Only consider tiles on the current ring's border
                    bool onBorder = (tx == bMin.x - ring || tx == bMax.x + ring ||
                                     ty == bMin.y - ring || ty == bMax.y + ring);
                    if (!onBorder) continue;
                    if (!m_map->isValidTile(tx, ty)) continue;
                    if (!m_map->isWalkable(tx, ty)) continue;

                    sf::Vector2f tileCenter = m_map->tileToWorldCenter(tx, ty);
                    float dist = MathUtil::distance(tileCenter, m_position);
                    if (dist < bestDist) {
                        bestDist = dist;
                        bestTile = sf::Vector2i(tx, ty);
                    }
                }
            }

            if (bestTile.x >= 0) {
                approachPoint = m_map->tileToWorldCenter(bestTile.x, bestTile.y);
                found = true;
            }
        }
    }

    m_targetPosition = approachPoint;
    m_state = UnitState::Building;
    playAnimation(AnimationState::Walk);  // Walk to building first
    findPath(approachPoint);
}

void Worker::updateBuilding(float deltaTime) {
    auto target = m_buildTarget.lock();
    if (!target || !target->isAlive()) {
        // Building was destroyed
        releaseBuildClaim();
        m_buildTarget.reset();
        if (!popNextAction()) m_state = UnitState::Idle;
        return;
    }
    
    Building* building = target->asBuilding();
    if (!building) {
        if (!popNextAction()) m_state = UnitState::Idle;
        return;
    }
    
    // Check if building is already constructed
    if (building->isConstructed()) {
        releaseBuildClaim();
        m_buildTarget.reset();
        if (!popNextAction()) m_state = UnitState::Idle;
        return;
    }
    
    // Calculate distance to the nearest edge of the building, not the center
    sf::FloatRect bounds = building->getBounds();
    float clampedX = std::max(bounds.position.x, std::min(m_position.x, bounds.position.x + bounds.size.x));
    float clampedY = std::max(bounds.position.y, std::min(m_position.y, bounds.position.y + bounds.size.y));
    sf::Vector2f nearestEdgePoint(clampedX, clampedY);
    float distanceToEdge = MathUtil::distance(m_position, nearestEdgePoint);
    
    // Build range is now just a small distance from the building edge
    float buildRange = getCollisionRadius() + 15.0f;
    
    if (distanceToEdge > buildRange) {
        // Still moving toward building.
        // If the path is completely exhausted but we still have not reached build
        // range, the original approach point was unreachable (e.g. a second building
        // was placed between us and the target while we were walking).  Refresh the
        // approach point via the same tile-aware search used in buildAt() and replan.
        bool pathExhausted = m_path.empty() || m_pathIndex >= m_path.size();
        if (pathExhausted && m_map) {
            sf::Vector2i bMin = m_map->worldToTile(bounds.position);
            sf::Vector2i bMax = m_map->worldToTile(
                sf::Vector2f(bounds.position.x + bounds.size.x - 1.0f,
                             bounds.position.y + bounds.size.y - 1.0f));

            bool found = false;
            for (int ring = 1; ring <= 5 && !found; ++ring) {
                float bestDist = std::numeric_limits<float>::max();
                sf::Vector2i bestTile(-1, -1);

                for (int tx = bMin.x - ring; tx <= bMax.x + ring; ++tx) {
                    for (int ty = bMin.y - ring; ty <= bMax.y + ring; ++ty) {
                        bool onBorder = (tx == bMin.x - ring || tx == bMax.x + ring ||
                                         ty == bMin.y - ring || ty == bMax.y + ring);
                        if (!onBorder) continue;
                        if (!m_map->isValidTile(tx, ty)) continue;
                        if (!m_map->isWalkable(tx, ty)) continue;

                        sf::Vector2f tileCenter = m_map->tileToWorldCenter(tx, ty);
                        float dist = MathUtil::distance(tileCenter, m_position);
                        if (dist < bestDist) {
                            bestDist = dist;
                            bestTile = sf::Vector2i(tx, ty);
                        }
                    }
                }

                if (bestTile.x >= 0) {
                    m_targetPosition = m_map->tileToWorldCenter(bestTile.x, bestTile.y);
                    findPath(m_targetPosition);
                    found = true;
                }
            }
        }

        followPath(deltaTime);
        // Walk animation already set when buildAt was called
    } else {
        // At building - start constructing
        auto self = std::dynamic_pointer_cast<Entity>(shared_from_this());

        if (!building->hasBuilder()) {
            building->assignBuilder(self);
        } else if (building->getBuilder().get() != this) {
            // Someone else holds the builder slot — check if they are still building.
            // They may have walked away (issued a move/attack command) without releasing
            // the claim, in which case we can safely take over.
            EntityPtr currentBuilder = building->getBuilder();
            Worker* otherWorker = currentBuilder ? currentBuilder->asWorker() : nullptr;
            if (!otherWorker || !otherWorker->isBuilding()) {
                // Previous builder abandoned the construction — steal the claim
                building->releaseBuilder();
                building->assignBuilder(self);
            } else {
                // Another worker is actively building — nothing for us to do
                if (!popNextAction()) m_state = UnitState::Idle;
                m_buildTarget.reset();
                return;
            }
        }

        // We are the assigned builder — contribute progress
        if (building->getBuilder().get() == this) {
            float constructionTime = building->getConstructionTime();
            float progressPerSecond = 1.0f / constructionTime;
            building->addConstructionProgress(progressPerSecond * deltaTime);
            
            // Face the building
            sf::Vector2f direction = target->getPosition() - m_position;
            updateSpriteDirection(direction);
            if (m_animatedSprite.getCurrentAnimationName() != AnimationState::Idle) {
                playAnimation(AnimationState::Idle);
            }
        }
    }
}

void Worker::releaseBuildClaim() {
    if (auto target = m_buildTarget.lock()) {
        if (auto* building = target->asBuilding()) {
            // Only release if we are the assigned builder
            if (building->getBuilder().get() == this) {
                building->releaseBuilder();
            }
        }
    }
}

void Worker::preload() {
    TEXTURES.loadAnimationSet("units/worker");
    SOUNDS.loadBuffer("effects/worker_death.wav");
}
