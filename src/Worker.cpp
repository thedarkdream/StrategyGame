#include "Worker.h"
#include "Building.h"
#include "ResourceNode.h"
#include "EntityData.h"
#include "Constants.h"
#include <cmath>

Worker::Worker(Team team, sf::Vector2f position)
    : Unit(EntityType::Worker, team, position)
{
}

bool Worker::isCollidable() const {
    // Workers are not collidable when gathering or returning resources
    // This allows them to stack at resource nodes
    return m_state != UnitState::Gathering && m_state != UnitState::Returning;
}

void Worker::render(sf::RenderTarget& target) {
    // Draw the unit shape
    target.draw(m_shape);
    
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
        return;
    }
    
    // Release any previous mining claim
    releaseMiningClaim();
    
    m_resourceTarget = resource;
    m_targetPosition = resource->getPosition();
    m_state = UnitState::Gathering;
    m_gatherTimer = 0.0f;
    m_isActivelyMining = false;
    findPath(resource->getPosition());
}

void Worker::returnResources() {
    // Release mining claim before leaving
    releaseMiningClaim();
    
    if (auto base = m_homeBase.lock()) {
        m_targetPosition = base->getPosition();
        m_state = UnitState::Returning;
        findPath(base->getPosition());
    } else {
        m_state = UnitState::Idle;
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
