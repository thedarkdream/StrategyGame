#pragma once

#include "Entity.h"

class ResourceNode : public Entity {
public:
    ResourceNode(EntityType type, sf::Vector2f position, int resourceAmount, int visualVariant = 1);
    
    void update(float deltaTime) override;
    void render(sf::RenderTarget& target) override;
    
    // Resource harvesting
    int harvestResource();
    int getRemainingResources() const { return m_resourceAmount; }
    bool isDepleted() const { return m_resourceAmount <= 0; }
    
    // Visual variant (1-3 for minerals)
    int getVisualVariant() const { return m_visualVariant; }
    
    // Mining claim system - only one worker can actively mine at a time
    bool tryClaimMining(EntityPtr worker);    // Try to claim this spot for mining
    void releaseMining(EntityPtr worker);     // Release the mining claim
    bool isBeingMined() const;                // Check if actively being mined
    
private:
    int m_resourceAmount;
    int m_visualVariant;
    std::weak_ptr<Entity> m_currentMiner;
};

using ResourceNodePtr = std::shared_ptr<ResourceNode>;
