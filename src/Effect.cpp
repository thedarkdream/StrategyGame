#include "Effect.h"
#include "TextureManager.h"
#include "Animation.h"

Effect::Effect(const std::string& animationPath, sf::Vector2f position, float scale)
    : m_position(position)
{
    // Load animation from TextureManager (effects use static sprite loading for single-row animations)
    const AnimationSet* animSet = TEXTURES.loadEffectAnimation(animationPath);
    
    if (animSet) {
        m_sprite.setAnimationSet(animSet);
        m_sprite.setScale(scale);
        m_sprite.play(AnimationState::Idle, true);  // Force play
        m_initialized = true;
    } else {
        m_finished = true;  // Failed to load, mark as finished for cleanup
    }
}

bool Effect::update(float deltaTime) {
    if (m_finished || !m_initialized) {
        return true;  // Remove this effect
    }
    
    m_sprite.update(deltaTime);
    
    if (m_sprite.isFinished()) {
        m_finished = true;
        return true;  // Remove this effect
    }
    
    return false;  // Keep this effect
}

void Effect::render(sf::RenderTarget& target) {
    if (m_finished || !m_initialized) return;
    
    m_sprite.render(target, m_position + m_offset);
}
