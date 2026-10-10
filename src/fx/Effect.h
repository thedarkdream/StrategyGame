#pragma once

#include "sprite/AnimatedSprite.h"
#include <SFML/System/Vector2.hpp>
#include <string>

// Visual effect that plays once and auto-destroys
class Effect {
public:
    Effect(const std::string& animationPath, sf::Vector2f position, float scale = 1.0f);
    
    // Update - returns true when effect is finished and should be removed
    bool update(float deltaTime);
    
    // Check if finished
    bool isFinished() const { return m_finished; }
    
    // Position
    sf::Vector2f getPosition() const { return m_position; }
    void setPosition(sf::Vector2f pos) { m_position = pos; }
    
    // Optional: attach to an entity position (effect follows entity)
    void setOffset(sf::Vector2f offset) { m_offset = offset; }

    // Read-only visual state for renderers other than SFML.
    sf::Vector2f getWorldPosition() const { return m_position + m_offset; }
    const AnimatedSprite& getSprite() const { return m_sprite; }

    // Ground markers (e.g. move-command pings) lie flat on the terrain instead of
    // standing up as billboards in a 3D view.
    void setGroundMarker(bool onGround) { m_groundMarker = onGround; }
    bool isGroundMarker() const { return m_groundMarker; }
    
private:
    sf::Vector2f m_position;
    sf::Vector2f m_offset = {0.f, 0.f};
    AnimatedSprite m_sprite;
    bool m_finished = false;
    bool m_initialized = false;
    bool m_groundMarker = false;
};
