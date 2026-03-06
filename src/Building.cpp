#include "Building.h"
#include "EntityData.h"
#include "Constants.h"

#include <cmath>
#include <cstdint>

Building::Building(EntityType type, Team team, sf::Vector2f position)
    : Entity(type, team, position)
{
    // Use ENTITY_DATA for size and health
    m_size = ENTITY_DATA.getSize(type);
    m_maxHealth = ENTITY_DATA.getHealth(type);
    m_health = m_maxHealth;
    
    m_rallyPoint = position + sf::Vector2f(m_size.x, 0.0f);
    updateShape();
    
    // Load building sprite
    switch (type) {
        case EntityType::Base:
            loadStaticSprite("buildings/base.png");
            break;
        case EntityType::Barracks:
            loadStaticSprite("buildings/barracks.png");
            break;
        case EntityType::Factory:
            loadStaticSprite("buildings/factory.png");
            break;
        default:
            break;
    }
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
    
    // Convert to pixels for rendering
    
    
    // Draw sprite if available, otherwise draw shape fallback
    if (m_hasSprite) {
        m_animatedSprite.render(target, m_position);
    } else {
        sf::RectangleShape shape = m_shape;
        shape.setFillColor(renderColor);
        target.draw(shape);
    }
    
    // Draw selection indicator
    renderSelectionIndicator(target);
    
    // Draw health bar
    renderHealthBar(target);
}

void Building::renderPreview(sf::RenderTarget& target, sf::Color tint) {
    // Convert to pixels for rendering
    
    
    
    if (m_hasSprite) {
        m_animatedSprite.setColor(tint);
        m_animatedSprite.render(target, m_position);
        m_animatedSprite.setColor(sf::Color::White);  // Reset tint
    } else {
        sf::RectangleShape shape = m_shape;
        shape.setFillColor(tint);
        target.draw(shape);
    }
    
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
    
    // Check if this building can produce this unit type using ENTITY_DATA
    if (auto* buildingDef = ENTITY_DATA.getBuildingDef(m_type)) {
        for (EntityType produceable : buildingDef->producesUnits) {
            if (produceable == unitType) {
                return true;
            }
        }
    }
    return false;
}

bool Building::trainUnit(EntityType unitType) {
    if (!canTrain(unitType)) return false;
    
    ProductionOrder order;
    order.unitType = unitType;
    order.timeRequired = getTrainingTime(unitType);
    order.timeElapsed = 0.0f;
    
    m_productionQueue.push_back(order);
    m_isProducing = true;
    
    return true;
}

float Building::getProductionProgress() const {
    if (m_productionQueue.empty()) return 0.0f;
    
    const auto& current = m_productionQueue.front();
    return current.timeElapsed / current.timeRequired;
}

EntityType Building::getCurrentProductionType() const {
    if (m_productionQueue.empty()) return EntityType::None;
    return m_productionQueue.front().unitType;
}

void Building::cancelProduction() {
    if (!m_productionQueue.empty()) {
        EntityType cancelledType = m_productionQueue.front().unitType;
        m_productionQueue.pop_front();
        m_isProducing = !m_productionQueue.empty();
        
        // Refund resources via callback
        if (onProductionCancelled) {
            onProductionCancelled(cancelledType);
        }
    }
}

void Building::cancelProductionAtIndex(int index) {
    if (index < 0 || index >= static_cast<int>(m_productionQueue.size())) {
        return;
    }
    
    EntityType cancelledType = m_productionQueue[index].unitType;
    m_productionQueue.erase(m_productionQueue.begin() + index);
    m_isProducing = !m_productionQueue.empty();
    
    // Refund resources via callback
    if (onProductionCancelled) {
        onProductionCancelled(cancelledType);
    }
}

std::vector<EntityType> Building::getProductionQueue() const {
    std::vector<EntityType> result;
    result.reserve(m_productionQueue.size());
    for (const auto& order : m_productionQueue) {
        result.push_back(order.unitType);
    }
    return result;
}

void Building::addConstructionProgress(float amount) {
    float oldProgress = m_constructionProgress;
    m_constructionProgress += amount;
    
    // Increase HP proportionally during construction
    // HP goes from 1 to maxHP as progress goes from 0 to 1
    int targetHp = 1 + static_cast<int>((m_maxHealth - 1) * m_constructionProgress);
    int oldTargetHp = 1 + static_cast<int>((m_maxHealth - 1) * oldProgress);
    int hpGain = targetHp - oldTargetHp;
    if (hpGain > 0) {
        m_health = std::min(m_health + hpGain, m_maxHealth);
    }
    
    if (m_constructionProgress >= 1.0f) {
        m_constructionProgress = 1.0f;
        m_isConstructing = false;
        releaseBuilder();
    }
}

void Building::startConstruction() {
    m_constructionProgress = 0.0f;
    m_isConstructing = true;
    m_health = 1;  // Start with 1 HP, increases during construction
}

bool Building::hasBuilder() const {
    return !m_builder.expired();
}

bool Building::assignBuilder(EntityPtr worker) {
    if (isConstructed()) return false;
    if (hasBuilder()) return false;  // Already has a builder
    m_builder = worker;
    m_isConstructing = true;
    return true;
}

void Building::releaseBuilder() {
    m_builder.reset();
    if (!isConstructed()) {
        m_isConstructing = false;  // Pause construction when no builder
    }
}

float Building::getConstructionTime() const {
    return ENTITY_DATA.getConstructionTime(m_type);
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
        
        m_productionQueue.pop_front();
        m_isProducing = !m_productionQueue.empty();
    }
}

float Building::getTrainingTime(EntityType unitType) const {
    return ENTITY_DATA.getTrainingTime(unitType);
}

sf::Vector2f Building::getSpawnPoint() const {
    // Spawn at rally point or bottom-right of building
    return m_rallyPoint;
}

