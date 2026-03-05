#include "Worker.h"
#include "Building.h"
#include "Constants.h"
#include <cmath>

Worker::Worker(Team team, sf::Vector2f position)
    : Unit(EntityType::Worker, team, position,
           Constants::WORKER_SPEED,      // speed
           5,                            // damage
           30.0f,                        // attack range
           1.5f,                         // attack cooldown
           sf::Vector2f(20.0f, 20.0f),   // size
           Constants::WORKER_HEALTH)     // health
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
    
    m_resourceTarget = resource;
    m_targetPosition = resource->getPosition();
    m_state = UnitState::Gathering;
    m_gatherTimer = 0.0f;
    findPath(resource->getPosition());
}

void Worker::returnResources() {
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
    if (!resource) {
        m_state = UnitState::Idle;
        return;
    }
    
    // Move to resource
    float distance = getDistanceTo(resource);
    
    if (distance > 40.0f) {
        followPath(deltaTime);
    } else {
        // Gathering
        m_gatherTimer += deltaTime;
        if (m_gatherTimer >= Constants::GATHERING_TIME) {
            // Get resources from the resource node
            if (auto* building = dynamic_cast<Building*>(resource.get())) {
                m_carriedResources = building->harvestResource();
            }
            if (m_carriedResources > 0) {
                returnResources();
            } else {
                m_state = UnitState::Idle;
            }
        }
    }
}

void Worker::updateReturning(float deltaTime) {
    auto base = m_homeBase.lock();
    if (!base) {
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
        if (auto resource = m_resourceTarget.lock()) {
            gather(resource);
        } else {
            m_state = UnitState::Idle;
        }
    }
}
