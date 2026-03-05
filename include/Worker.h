#pragma once

#include "Unit.h"
#include <functional>

class Worker : public Unit {
public:
    Worker(Team team, sf::Vector2f position);
    
    void render(sf::RenderTarget& target) override;
    
    // Worker-specific commands
    void gather(EntityPtr resource);
    void returnResources();
    
    // Worker-specific state
    int getCarriedResources() const { return m_carriedResources; }
    void setHomeBase(EntityPtr base) { m_homeBase = base; }
    
    // Workers are not collidable when gathering (to allow stacking at resources)
    bool isCollidable() const override;
    
    // Callback when resources are deposited (int = amount deposited)
    std::function<void(int)> onResourceDeposit;
    
protected:
    void updateCustomState(float deltaTime) override;
    
private:
    void updateGathering(float deltaTime);
    void updateReturning(float deltaTime);
    
    // Resource gathering
    int m_carriedResources = 0;
    float m_gatherTimer = 0.0f;
    std::weak_ptr<Entity> m_resourceTarget;
    std::weak_ptr<Entity> m_homeBase;
};
