#pragma once

#include "Entity.h"
#include <queue>
#include <functional>

class Building : public Entity {
public:
    Building(EntityType type, Team team, sf::Vector2f position);
    
    void update(float deltaTime) override;
    void render(sf::RenderTarget& target) override;
    
    // Production
    bool canTrain(EntityType unitType) const;
    bool trainUnit(EntityType unitType);
    bool isProducing() const { return m_isProducing; }
    float getProductionProgress() const;
    void cancelProduction();
    
    // Building state
    bool isConstructed() const { return m_constructionProgress >= 1.0f; }
    float getConstructionProgress() const { return m_constructionProgress; }
    void addConstructionProgress(float amount);
    
    // Rally point
    void setRallyPoint(sf::Vector2f point) { m_rallyPoint = point; }
    sf::Vector2f getRallyPoint() const { return m_rallyPoint; }
    
    // Resource buildings
    bool isResourceBuilding() const;
    int harvestResource();
    int getRemainingResources() const { return m_resourceAmount; }
    
    // Callback when unit is produced
    std::function<void(EntityType, sf::Vector2f)> onUnitProduced;
    
private:
    // Construction
    float m_constructionProgress = 1.0f;  // 1.0 = fully built
    bool m_isConstructing = false;
    
    // Production queue
    struct ProductionOrder {
        EntityType unitType;
        float timeRequired;
        float timeElapsed = 0.0f;
    };
    std::queue<ProductionOrder> m_productionQueue;
    bool m_isProducing = false;
    
    // Rally point for produced units
    sf::Vector2f m_rallyPoint;
    
    // Resources (for mineral patches / gas geysers)
    int m_resourceAmount = 0;
    
    void updateProduction(float deltaTime);
    float getTrainingTime(EntityType unitType) const;
    sf::Vector2f getSpawnPoint() const;
};
