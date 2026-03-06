#include "ResourceNode.h"
#include "EntityData.h"
#include "Constants.h"
#include <cmath>

ResourceNode::ResourceNode(EntityType type, sf::Vector2f position, int resourceAmount, int visualVariant)
    : Entity(type, Team::Neutral, position)
    , m_resourceAmount(resourceAmount)
    , m_visualVariant(visualVariant)
{
    // Load size from ENTITY_DATA
    m_size = ENTITY_DATA.getSize(type);
    
    // Set color based on resource type
    switch (type) {
        case EntityType::MineralPatch:
            m_color = sf::Color(Constants::Colors::MINERAL_COLOR);
            break;
            
        case EntityType::GasGeyser:
            m_color = sf::Color(Constants::Colors::GAS_COLOR);
            break;
            
        default:
            break;
    }
    
    // Resources are indestructible (health only used for cleanup when depleted)
    m_maxHealth = 1;
    m_health = 1;
    
    updateShape();
    
    // Load static sprite for this resource type
    switch (type) {
        case EntityType::MineralPatch:
            loadStaticSprite("resources/minerals_" + std::to_string(m_visualVariant) + ".png");
            break;
        // Add more resource types here when textures are available
        default:
            break;
    }
}

void ResourceNode::update(float deltaTime) {
    // Clean up expired miner reference
    if (m_currentMiner.expired()) {
        m_currentMiner.reset();
    }
    
    // Mark as dead if depleted (for cleanup)
    if (m_resourceAmount <= 0 && m_health > 0) {
        m_health = 0;
    }
}

void ResourceNode::render(sf::RenderTarget& target) {
    // Draw sprite if available, otherwise draw shape fallback
    if (m_hasSprite) {
        m_animatedSprite.render(target, m_position);
    } else {
        target.draw(m_shape);
    }
    
    // Draw selection indicator if selected
    renderSelectionIndicator(target);
}

int ResourceNode::harvestResource() {
    if (m_resourceAmount <= 0) {
        m_health = 0;
        return 0;
    }
    
    int harvested = std::min(m_resourceAmount, Constants::MINERALS_PER_TRIP);
    m_resourceAmount -= harvested;
    
    if (m_resourceAmount <= 0) {
        m_health = 0;
    }
    
    return harvested;
}

bool ResourceNode::tryClaimMining(EntityPtr worker) {
    if (!worker) return false;
    
    // Check if already being mined by someone else
    if (auto currentMiner = m_currentMiner.lock()) {
        if (currentMiner != worker) {
            return false;  // Someone else is mining
        }
        return true;  // We already have the claim
    }
    
    // Claim is available
    m_currentMiner = worker;
    return true;
}

void ResourceNode::releaseMining(EntityPtr worker) {
    if (auto currentMiner = m_currentMiner.lock()) {
        if (currentMiner == worker) {
            m_currentMiner.reset();
        }
    }
}

bool ResourceNode::isBeingMined() const {
    return !m_currentMiner.expired();
}
