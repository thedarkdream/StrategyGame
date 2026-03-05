#include "Building.h"
#include "Constants.h"
#include <cmath>
#include <cstdint>

Building::Building(EntityType type, Team team, sf::Vector2f position)
    : Entity(type, team, position)
{
    switch (type) {
        case EntityType::Base:
            m_size = sf::Vector2f(96.0f, 96.0f);  // 3x3 tiles
            m_maxHealth = 1500;
            m_health = m_maxHealth;
            break;
            
        case EntityType::Barracks:
            m_size = sf::Vector2f(64.0f, 64.0f);  // 2x2 tiles
            m_maxHealth = 800;
            m_health = m_maxHealth;
            break;
            
        case EntityType::Refinery:
            m_size = sf::Vector2f(64.0f, 64.0f);  // 2x2 tiles
            m_maxHealth = 500;
            m_health = m_maxHealth;
            break;
            
        case EntityType::MineralPatch:
            m_size = sf::Vector2f(48.0f, 32.0f);
            m_maxHealth = 1;
            m_health = 1;
            m_color = sf::Color(0, 206, 209);  // Cyan
            m_resourceAmount = 1500;
            break;
            
        case EntityType::GasGeyser:
            m_size = sf::Vector2f(64.0f, 64.0f);
            m_maxHealth = 1;
            m_health = 1;
            m_color = sf::Color(0, 255, 0);  // Green
            m_resourceAmount = 2000;
            break;
            
        default:
            break;
    }
    
    m_rallyPoint = position + sf::Vector2f(m_size.x, 0.0f);
    updateShape();
}

void Building::update(float deltaTime) {
    if (!isAlive()) return;
    
    if (m_isConstructing) {
        // Construction progress handled externally by workers
    }
    
    if (isConstructed()) {
        updateProduction(deltaTime);
    }
}

void Building::render(sf::RenderTarget& target) {
    // Adjust alpha if under construction
    sf::Color renderColor = m_color;
    if (!isConstructed()) {
        renderColor.a = static_cast<std::uint8_t>(128 + 127 * m_constructionProgress);
    }
    
    sf::RectangleShape shape = m_shape;
    shape.setFillColor(renderColor);
    target.draw(shape);
    
    // Draw selection indicator
    renderSelectionIndicator(target);
    
    // Draw health bar
    renderHealthBar(target);
    
    // Draw construction/production progress
    if (!isConstructed() || m_isProducing) {
        float progress = isConstructed() ? getProductionProgress() : m_constructionProgress;
        
        const float barWidth = m_size.x * 0.8f;
        const float barHeight = 6.0f;
        const float yOffset = m_size.y / 2.0f + 10.0f;
        
        // Background
        sf::RectangleShape bgBar(sf::Vector2f(barWidth, barHeight));
        bgBar.setOrigin(sf::Vector2f(barWidth / 2.0f, barHeight / 2.0f));
        bgBar.setPosition(sf::Vector2f(m_position.x, m_position.y + yOffset));
        bgBar.setFillColor(sf::Color(50, 50, 50));
        target.draw(bgBar);
        
        // Progress
        sf::RectangleShape progressBar(sf::Vector2f(barWidth * progress, barHeight));
        progressBar.setOrigin(sf::Vector2f(barWidth / 2.0f, barHeight / 2.0f));
        progressBar.setPosition(sf::Vector2f(m_position.x, m_position.y + yOffset));
        progressBar.setFillColor(sf::Color(255, 200, 0));  // Yellow
        target.draw(progressBar);
    }
}

bool Building::canTrain(EntityType unitType) const {
    if (!isConstructed()) return false;
    
    switch (m_type) {
        case EntityType::Base:
            return unitType == EntityType::Worker;
            
        case EntityType::Barracks:
            return unitType == EntityType::Soldier;
            
        default:
            return false;
    }
}

bool Building::trainUnit(EntityType unitType) {
    if (!canTrain(unitType)) return false;
    
    ProductionOrder order;
    order.unitType = unitType;
    order.timeRequired = getTrainingTime(unitType);
    order.timeElapsed = 0.0f;
    
    m_productionQueue.push(order);
    m_isProducing = true;
    
    return true;
}

float Building::getProductionProgress() const {
    if (m_productionQueue.empty()) return 0.0f;
    
    const auto& current = m_productionQueue.front();
    return current.timeElapsed / current.timeRequired;
}

void Building::cancelProduction() {
    if (!m_productionQueue.empty()) {
        m_productionQueue.pop();
        m_isProducing = !m_productionQueue.empty();
    }
}

void Building::addConstructionProgress(float amount) {
    m_constructionProgress += amount;
    if (m_constructionProgress >= 1.0f) {
        m_constructionProgress = 1.0f;
        m_isConstructing = false;
    }
}

bool Building::isResourceBuilding() const {
    return m_type == EntityType::MineralPatch || m_type == EntityType::GasGeyser;
}

int Building::harvestResource() {
    if (m_resourceAmount <= 0) return 0;
    
    int harvested = std::min(m_resourceAmount, Constants::MINERALS_PER_TRIP);
    m_resourceAmount -= harvested;
    return harvested;
}

void Building::updateProduction(float deltaTime) {
    if (m_productionQueue.empty()) {
        m_isProducing = false;
        return;
    }
    
    auto& current = m_productionQueue.front();
    current.timeElapsed += deltaTime;
    
    if (current.timeElapsed >= current.timeRequired) {
        // Unit produced!
        if (onUnitProduced) {
            onUnitProduced(current.unitType, getSpawnPoint());
        }
        
        m_productionQueue.pop();
        m_isProducing = !m_productionQueue.empty();
    }
}

float Building::getTrainingTime(EntityType unitType) const {
    switch (unitType) {
        case EntityType::Worker:
            return 3.0f;
        case EntityType::Soldier:
            return 5.0f;
        default:
            return 5.0f;
    }
}

sf::Vector2f Building::getSpawnPoint() const {
    // Spawn at rally point or bottom-right of building
    return m_rallyPoint;
}
