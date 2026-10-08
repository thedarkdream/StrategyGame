#pragma once

#include "Effect.h"
#include <SFML/System/Vector2.hpp>
#include <vector>
#include <memory>
#include <string>

// Manages visual effects - spawning and updating (drawing is done by the renderers)
class EffectsManager {
public:
    static EffectsManager& instance();
    
    static void preload();  // Preload all effect animations

    // Spawn an effect at a position
    // animationPath: path to effect animation (e.g., "effects/explosion.png")
    // position: world position to spawn at
    // scale: size multiplier (1.0 = original size)
    // Returns the new effect, or nullptr when its animation could not be loaded.
    Effect* spawn(const std::string& animationPath, sf::Vector2f position, float scale = 1.0f);
    
    // Convenience methods for common effects
    void spawnExplosion(sf::Vector2f position, float scale = 1.0f);
    
    // Spawn move effect (visual indicator for move/attack-move commands)
    void spawnMoveEffect(sf::Vector2f position, float scale = 1.0f);

    // Update all active effects (call once per frame)
    void update(float deltaTime);
    
    // Clear all effects
    void clear();
    
    // Get count of active effects (for debugging)
    size_t getActiveCount() const { return m_effects.size(); }

    // Read-only access for renderers other than SFML.
    const std::vector<std::unique_ptr<Effect>>& all() const { return m_effects; }
    
private:
    EffectsManager() = default;
    EffectsManager(const EffectsManager&) = delete;
    EffectsManager& operator=(const EffectsManager&) = delete;
    
    std::vector<std::unique_ptr<Effect>> m_effects;
};

// Convenience macro
#define EFFECTS EffectsManager::instance()
