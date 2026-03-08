#include "Worker.h"
#include "Building.h"
#include "ResourceNode.h"
#include "EntityData.h"
#include "Constants.h"
#include "Animation.h"
#include "MathUtil.h"
#include <cmath>

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
        if (findNearestAvailableResource) {
            EntityPtr newResource = findNearestAvailableResource(m_position, 200.0f, nullptr);
            if (newResource) {
                gather(newResource);
                return;
            }
        } else if (findNearestResource) {
            EntityPtr newResource = findNearestResource(m_position, 200.0f);
            if (newResource) {
                gather(newResource);
                return;
            }
        }
        m_state = UnitState::Idle;
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
            ResourceNode* resourceNode = dynamic_cast<ResourceNode*>(resource.get());
            if (resourceNode) {
                // Try to claim this mining spot
                // Need shared_from_this - pass ourselves as claimer
                auto self = std::dynamic_pointer_cast<Entity>(shared_from_this());
                if (!resourceNode->tryClaimMining(self)) {
                    // Spot is occupied - find another field
                    if (findNearestAvailableResource) {
                        EntityPtr newResource = findNearestAvailableResource(m_position, 200.0f, resource);
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
            ResourceNode* resourceNode = dynamic_cast<ResourceNode*>(resource.get());
            if (resourceNode) {
                m_carriedResources = resourceNode->harvestResource();
            }
            if (m_carriedResources > 0) {
                returnResources();
            } else {
                // Resource depleted - release claim and find replacement
                releaseMiningClaim();
                m_resourceTarget.reset();
                if (findNearestAvailableResource) {
                    EntityPtr newResource = findNearestAvailableResource(m_position, 200.0f, nullptr);
                    if (newResource) {
                        gather(newResource);
                        return;
                    }
                } else if (findNearestResource) {
                    EntityPtr newResource = findNearestResource(m_position, 200.0f);
                    if (newResource) {
                        gather(newResource);
                        return;
                    }
                }
                m_state = UnitState::Idle;
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
        m_state = UnitState::Idle;
        return;
    }
    
    float distance = getDistanceTo(base);
    
    if (distance > 50.0f) {
        followPath(deltaTime);
    } else {
        // Deposit resources
        if (m_carriedResources > 0 && onResourceDeposit) {
            onResourceDeposit(m_carriedResources);
        }
        m_carriedResources = 0;
        
        // Resume gathering
        auto resource = m_resourceTarget.lock();
        if (resource && resource->isAlive()) {
            gather(resource);
        } else {
            // Original resource gone - try to find a nearby replacement
            m_resourceTarget.reset();
            if (findNearestAvailableResource) {
                EntityPtr newResource = findNearestAvailableResource(m_position, 200.0f, nullptr);
                if (newResource) {
                    gather(newResource);
                    return;
                }
            } else if (findNearestResource) {
                EntityPtr newResource = findNearestResource(m_position, 200.0f);
                if (newResource) {
                    gather(newResource);
                    return;
                }
            }
            m_state = UnitState::Idle;
        }
    }
}

void Worker::releaseMiningClaim() {
    if (!m_isActivelyMining) return;
    
    if (auto resource = m_resourceTarget.lock()) {
        if (auto* resourceNode = dynamic_cast<ResourceNode*>(resource.get())) {
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
    
    m_buildTarget = building;
    
    // Calculate the closest point on the building's edge from our current position
    sf::FloatRect bounds = building->getBounds();
    sf::Vector2f buildingCenter = building->getPosition();
    
    // Find closest point on the building's perimeter to the worker
    sf::Vector2f closestPoint;
    
    // Clamp worker position to building bounds to find nearest edge point
    float clampedX = std::max(bounds.position.x, std::min(m_position.x, bounds.position.x + bounds.size.x));
    float clampedY = std::max(bounds.position.y, std::min(m_position.y, bounds.position.y + bounds.size.y));
    
    // If worker is inside the building bounds, use center
    if (bounds.contains(m_position)) {
        closestPoint = buildingCenter;
    } else {
        // Move the target point slightly outside the building edge (add small offset for build range)
        sf::Vector2f dirToWorker = m_position - sf::Vector2f(clampedX, clampedY);
        float dist = std::sqrt(dirToWorker.x * dirToWorker.x + dirToWorker.y * dirToWorker.y);
        if (dist > 0.1f) {
            dirToWorker = dirToWorker / dist;
            // Offset by worker collision radius plus a small margin
            float offset = getCollisionRadius() + 5.0f;
            closestPoint = sf::Vector2f(clampedX, clampedY) + dirToWorker * offset;
        } else {
            closestPoint = sf::Vector2f(clampedX, clampedY);
        }
    }
    
    m_targetPosition = closestPoint;
    m_state = UnitState::Building;
    playAnimation(AnimationState::Walk);  // Walk to building first
    findPath(closestPoint);
}

void Worker::updateBuilding(float deltaTime) {
    auto target = m_buildTarget.lock();
    if (!target || !target->isAlive()) {
        // Building was destroyed
        releaseBuildClaim();
        m_buildTarget.reset();
        m_state = UnitState::Idle;
        return;
    }
    
    Building* building = dynamic_cast<Building*>(target.get());
    if (!building) {
        m_state = UnitState::Idle;
        return;
    }
    
    // Check if building is already constructed
    if (building->isConstructed()) {
        releaseBuildClaim();
        m_buildTarget.reset();
        m_state = UnitState::Idle;
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
        // Still moving to building
        followPath(deltaTime);
        // Walk animation already set when buildAt was called
    } else {
        // At building - start constructing
        // Try to assign ourselves as the builder
        auto self = std::dynamic_pointer_cast<Entity>(shared_from_this());
        if (!building->hasBuilder()) {
            building->assignBuilder(self);
        }
        
        // Only add progress if we are the assigned builder
        if (building->getBuilder().get() == this) {
            float constructionTime = building->getConstructionTime();
            float progressPerSecond = 1.0f / constructionTime;
            building->addConstructionProgress(progressPerSecond * deltaTime);
            
            // Face the building
            sf::Vector2f direction = target->getPosition() - m_position;
            updateSpriteDirection(direction);
            // Switch to idle/build animation only once when we start building
            if (m_animatedSprite.getCurrentAnimationName() != AnimationState::Idle) {
                playAnimation(AnimationState::Idle);  // Or a build animation if available
            }
        } else {
            // Another worker is building - stop and wait or go idle
            m_state = UnitState::Idle;
            m_buildTarget.reset();
        }
    }
}

void Worker::releaseBuildClaim() {
    if (auto target = m_buildTarget.lock()) {
        if (auto* building = dynamic_cast<Building*>(target.get())) {
            // Only release if we are the assigned builder
            if (building->getBuilder().get() == this) {
                building->releaseBuilder();
            }
        }
    }
}
