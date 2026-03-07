#pragma once

#include "AnimatedSprite.h"
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/System/Vector2.hpp>
#include <string>

// Visual effect that plays once and auto-destroys
class Effect {
public:
    Effect(const std::string& animationPath, sf::Vector2f position, float scale = 1.0f);
    
    // Update - returns true when effect is finished and should be removed
    bool update(float deltaTime);
    
    // Render the effect
    void render(sf::RenderTarget& target);
    
    // Check if finished
    bool isFinished() const { return m_finished; }
    
    // Position
    sf::Vector2f getPosition() const { return m_position; }
    void setPosition(sf::Vector2f pos) { m_position = pos; }
    
    // Optional: attach to an entity position (effect follows entity)
    void setOffset(sf::Vector2f offset) { m_offset = offset; }
    
private:
    sf::Vector2f m_position;
    sf::Vector2f m_offset = {0.f, 0.f};
    AnimatedSprite m_sprite;
    bool m_finished = false;
    bool m_initialized = false;
};
