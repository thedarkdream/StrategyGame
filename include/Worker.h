#pragma once

#include "Unit.h"
#include <functional>

class Building;

class Worker : public Unit {
public:
    Worker(Team team, sf::Vector2f position);
    
    void render(sf::RenderTarget& target) override;
    
    // Worker-specific commands
    void gather(EntityPtr resource);
    void returnResources();
    void buildAt(EntityPtr building);  // Move to building and construct it
    
    // Worker-specific state
    int getCarriedResources() const { return m_carriedResources; }
    void setHomeBase(EntityPtr base) { m_homeBase = base; }
    
    // Check if worker is building
    bool isBuilding() const { return m_state == UnitState::Building; }
    bool isGathering() const { return m_state == UnitState::Gathering || m_state == UnitState::Returning; }
    EntityPtr getBuildTarget() const { return m_buildTarget.lock(); }
    
    // Workers are not collidable when gathering (to allow stacking at resources)
    bool isCollidable() const override;
    
    // Callback when resources are deposited (int = amount deposited)
    std::function<void(int)> onResourceDeposit;
    
    // Callback to find nearest mineral patch
    std::function<EntityPtr(sf::Vector2f pos, float radius)> findNearestResource;
    
    // Callback to find nearest unoccupied mineral patch
    std::function<EntityPtr(sf::Vector2f pos, float radius, EntityPtr exclude)> findNearestAvailableResource;
    
protected:
    void updateCustomState(float deltaTime) override;
    
private:
    void updateGathering(float deltaTime);
    void updateReturning(float deltaTime);
    void updateBuilding(float deltaTime);
    
    // Resource gathering
    int m_carriedResources = 0;
    float m_gatherTimer = 0.0f;
    bool m_isActivelyMining = false;  // True when at resource and mining (claimed spot)
    std::weak_ptr<Entity> m_resourceTarget;
    std::weak_ptr<Entity> m_homeBase;
    
    // Building construction
    std::weak_ptr<Entity> m_buildTarget;
    
    // Release any claimed mining spot
    void releaseMiningClaim();
    void releaseBuildClaim();
};
