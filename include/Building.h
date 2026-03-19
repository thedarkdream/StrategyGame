#pragma once

#include "Entity.h"
#include "IUnitContext.h"
#include <deque>
#include <vector>

class Building : public Entity {
public:
    Building(EntityType type, Team team, sf::Vector2f position);
    
    static void preload();  // Preload textures and sounds for all building types
    
    void update(float deltaTime) override;
    void render(sf::RenderTarget& target) override;
    
    // Override to spawn explosion on death
    void takeDamage(int damage) override;
    
    // Preview rendering (just the sprite with a color tint, no UI elements)
    void renderPreview(sf::RenderTarget& target, sf::Color tint);
    
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
    void startConstruction();  // Set building to under-construction state
    
    // Builder worker management
    bool hasBuilder() const;
    bool assignBuilder(EntityPtr worker);
    void releaseBuilder();
    EntityPtr getBuilder() const { return m_builder.lock(); }
    float getConstructionTime() const;
    
    // Rally point
    void setRallyPoint(sf::Vector2f point);
    void setRallyTarget(EntityPtr target);
    void clearRallyTarget() { m_rallyTarget.reset(); }
    sf::Vector2f getRallyPoint() const { return m_rallyPoint; }
    EntityPtr getRallyTarget() const { return m_rallyTarget.lock(); }
    bool hasRallyTarget() const { return !m_rallyTarget.expired(); }
    
    Building*       asBuilding()       override { return this; }
    const Building* asBuilding() const override { return this; }

    // Inject the game-world context (spatial queries, projectile spawning, etc.).
    // Called unconditionally by Game::spawnBuilding for every building type so
    // that subclasses (e.g. Turret) can rely on m_context without needing to
    // override a separate virtual hook.
    void setContext(IUnitContext* ctx) { m_context = ctx; }

private:
    // Construction
    float m_constructionProgress = 1.0f;  // 1.0 = fully built, 0.0 = just placed
    bool m_isConstructing = false;
    std::weak_ptr<Entity> m_builder;  // Currently assigned builder worker
    
    // Production queue
    struct ProductionOrder {
        EntityType unitType;
        float timeRequired;
        float timeElapsed = 0.0f;
    };
    std::deque<ProductionOrder> m_productionQueue;
    bool m_isProducing = false;
    
    // Rally point for produced units
    sf::Vector2f m_rallyPoint;
    std::weak_ptr<Entity> m_rallyTarget;  // Optional entity target
    
    void updateProduction(float deltaTime);
    float getTrainingTime(EntityType unitType) const;
    sf::Vector2f getSpawnPoint() const;

protected:
    IUnitContext* m_context = nullptr;
};
