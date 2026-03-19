#pragma once

#include "Unit.h"
#include <chrono>

class Building;

class Worker : public Unit {
public:
    Worker(Team team, sf::Vector2f position);
    
    static void preload();  // Preload textures and sounds for this unit type
    
    void render(sf::RenderTarget& target) override;
    
    // Worker-specific commands
    void gather(EntityPtr resource);
    void returnResources();
    void buildAt(EntityPtr building);  // Move to building and construct it
    
    // Override base movement commands to handle collision escape
    void moveTo(sf::Vector2f target) override;
    void attackMoveTo(sf::Vector2f target) override;
    
    // Worker-specific state
    int getCarriedResources() const { return m_carriedResources; }
    void setHomeBase(EntityPtr base) { m_homeBase = base; }
    
    // Check if worker is building
    bool isBuilding() const { return m_state == UnitState::Building; }
    bool isGathering() const { return m_state == UnitState::Gathering || m_state == UnitState::Returning; }
    EntityPtr getBuildTarget() const { return m_buildTarget.lock(); }
    
    void onDeath() override;
    void onSpawned() override;
    void onSelected() override;

    // Workers are not collidable when gathering (to allow stacking at resources)
    bool isCollidable() const override;
    Worker*       asWorker()       override { return this; }
    const Worker* asWorker() const override { return this; }

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
    
    // Escape collision if overlapping with entities (after transitioning from non-collidable)
    void escapeCollisionIfNeeded();

    // Voice line system — static so cooldown is shared across all workers (prevents spam)
    enum class VoiceAction { Select = 0, Move = 1, Attack = 2, Gather = 3 };
    static constexpr int NUM_VOICE_ACTIONS = 4;
    static void playVoiceLine(VoiceAction action, sf::Vector2f position);
    static std::chrono::steady_clock::time_point s_voiceCooldownEnd[NUM_VOICE_ACTIONS];
};
