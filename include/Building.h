#pragma once

#include "Entity.h"
#include <queue>
#include <vector>
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
    void cancelProduction();                   // Cancel front of queue
    void cancelProductionAtIndex(int index);   // Cancel specific queue item
    EntityType getCurrentProductionType() const;
    int getQueueSize() const { return static_cast<int>(m_productionQueue.size()); }
    std::vector<EntityType> getProductionQueue() const;
    
    // Building state
    bool isConstructed() const { return m_constructionProgress >= 1.0f; }
    float getConstructionProgress() const { return m_constructionProgress; }
    void addConstructionProgress(float amount);
    
    // Rally point
    void setRallyPoint(sf::Vector2f point) { m_rallyPoint = point; }
    sf::Vector2f getRallyPoint() const { return m_rallyPoint; }
    
    // Callback when unit is produced
    std::function<void(EntityType, sf::Vector2f)> onUnitProduced;
    
    // Callback when production is cancelled (for refunding resources)
    std::function<void(EntityType)> onProductionCancelled;
    
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
    
    void updateProduction(float deltaTime);
    float getTrainingTime(EntityType unitType) const;
    sf::Vector2f getSpawnPoint() const;
};
