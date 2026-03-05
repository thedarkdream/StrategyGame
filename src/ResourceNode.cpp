#include "ResourceNode.h"
#include "Constants.h"
#include <cmath>

ResourceNode::ResourceNode(EntityType type, sf::Vector2f position, int resourceAmount)
    : Entity(type, Team::Neutral, position)
    , m_resourceAmount(resourceAmount)
{
    switch (type) {
        case EntityType::MineralPatch:
            m_size = sf::Vector2f(48.0f, 32.0f);
            m_color = sf::Color(0, 206, 209);  // Cyan
            break;
            
        case EntityType::GasGeyser:
            m_size = sf::Vector2f(64.0f, 64.0f);
            m_color = sf::Color(0, 255, 0);  // Green
            break;
            
        default:
            break;
    }
    
    // Resources are indestructible (health only used for cleanup when depleted)
    m_maxHealth = 1;
    m_health = 1;
    
    updateShape();
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
    target.draw(m_shape);
    
    // Draw selection indicator if selected
    renderSelectionIndicator(target);
    
    // Draw remaining resource amount as a simple bar
    float barWidth = m_size.x;
    float barHeight = 4.0f;
    float barY = m_position.y - m_size.y / 2.0f - 8.0f;
    
    // Background
    sf::RectangleShape barBg(sf::Vector2f(barWidth, barHeight));
    barBg.setPosition(sf::Vector2f(m_position.x - barWidth / 2.0f, barY));
    barBg.setFillColor(sf::Color(40, 40, 40));
    target.draw(barBg);
    
    // Determine max resources based on type (for percentage calculation)
    int maxResources = (m_type == EntityType::MineralPatch) ? 1500 : 2500;
    float percentage = static_cast<float>(m_resourceAmount) / static_cast<float>(maxResources);
    percentage = std::max(0.0f, std::min(1.0f, percentage));
    
    // Fill
    sf::RectangleShape barFill(sf::Vector2f(barWidth * percentage, barHeight));
    barFill.setPosition(sf::Vector2f(m_position.x - barWidth / 2.0f, barY));
    barFill.setFillColor(m_color);
    target.draw(barFill);
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
