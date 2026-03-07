#include "Entity.h"
#include "Constants.h"
#include "TextureManager.h"
#include <cmath>

Entity::Entity(EntityType type, Team team, sf::Vector2f position)
    : m_type(type)
    , m_team(team)
    , m_position(position)
    , m_size(32.0f, 32.0f)
    , m_health(100)
    , m_maxHealth(100)
{
    // Set color based on team using Constants
    switch (team) {
        case Team::Player:
            m_color = sf::Color(Constants::Colors::PLAYER_COLOR);
            break;
        case Team::Enemy:
            m_color = sf::Color(Constants::Colors::ENEMY_COLOR);
            break;
        case Team::Neutral:
            m_color = sf::Color(Constants::Colors::NEUTRAL_COLOR);
            break;
    }
    
    updateShape();
}

sf::FloatRect Entity::getBounds() const {
    return sf::FloatRect(
        sf::Vector2f(m_position.x - m_size.x / 2.0f, m_position.y - m_size.y / 2.0f),
        m_size
    );
}

void Entity::takeDamage(int damage) {
    if (m_isDying) return;  // Already dying, don't process more damage
    
    m_health -= damage;
    if (m_health <= 0) {
        m_health = 0;
        startDeathAnimation();
    }
}

void Entity::takeDamage(int damage, EntityPtr attacker) {
    // Base implementation just applies damage
    takeDamage(damage);
}

void Entity::updateShape() {
    m_shape.setSize(m_size);
    m_shape.setOrigin(sf::Vector2f(m_size.x / 2.0f, m_size.y / 2.0f));
    m_shape.setPosition(m_position);
    m_shape.setFillColor(m_color);
    m_shape.setOutlineThickness(1.0f);
    m_shape.setOutlineColor(sf::Color::Black);
}

void Entity::renderHealthBar(sf::RenderTarget& target) {
    // Always show health bar when selected, otherwise only when damaged
    if (m_health >= m_maxHealth && !m_selected) return;
    
    const float barWidth = m_size.x;
    const float barHeight = 4.0f;
    const float yOffset = -m_size.y / 2.0f - 8.0f;
    
    // Background (red)
    sf::RectangleShape bgBar(sf::Vector2f(barWidth, barHeight));
    bgBar.setOrigin(sf::Vector2f(barWidth / 2.0f, barHeight / 2.0f));
    bgBar.setPosition(sf::Vector2f(m_position.x, m_position.y + yOffset));
    bgBar.setFillColor(sf::Color(180, 0, 0));
    target.draw(bgBar);
    
    // Health (green)
    float healthPercent = static_cast<float>(m_health) / static_cast<float>(m_maxHealth);
    sf::RectangleShape healthBar(sf::Vector2f(barWidth * healthPercent, barHeight));
    healthBar.setOrigin(sf::Vector2f(barWidth / 2.0f, barHeight / 2.0f));
    healthBar.setPosition(sf::Vector2f(m_position.x, m_position.y + yOffset));
    healthBar.setFillColor(sf::Color(0, 180, 0));
    target.draw(healthBar);
}

void Entity::renderSelectionIndicator(sf::RenderTarget& target) {
    if (!m_selected) return;
    
    sf::CircleShape circle(m_size.x / 2.0f + 4.0f);
    circle.setOrigin(sf::Vector2f(circle.getRadius(), circle.getRadius()));
    circle.setPosition(m_position);
    circle.setFillColor(sf::Color::Transparent);
    circle.setOutlineThickness(2.0f);
    circle.setOutlineColor(sf::Color::Green);
    target.draw(circle);
}

void Entity::loadAnimations(const std::string& basePath) {
    // Load AnimationSet from TextureManager (cached, shared between entities)
    const AnimationSet* animSet = TEXTURES.loadAnimationSet(basePath);
    
    if (!animSet) {
        return;
    }
    
    // Set up the animated sprite with the animation set
    m_animatedSprite.setAnimationSet(animSet);
    
    // Get frame dimensions for scaling
    int frameWidth = animSet->getDefaultFrameWidth();
    int frameHeight = animSet->getDefaultFrameHeight();
    
    // Scale sprite to match entity's defined size
    if (frameWidth > 0 && frameHeight > 0) {
        float scaleX = m_size.x / static_cast<float>(frameWidth);
        float scaleY = m_size.y / static_cast<float>(frameHeight);
        m_animatedSprite.setScale(sf::Vector2f(scaleX, scaleY));
    }
    
    m_hasSprite = true;
    
    // Start with idle animation
    if (animSet->hasAnimation(AnimationState::Idle)) {
        playAnimation(AnimationState::Idle);
    }
}

void Entity::loadStaticSprite(const std::string& texturePath) {
    // Load static sprite as AnimationSet from TextureManager (cached)
    const AnimationSet* animSet = TEXTURES.loadStaticSprite(texturePath);
    
    if (!animSet) {
        return;
    }
    
    m_animatedSprite.setAnimationSet(animSet);
    m_animatedSprite.centerOrigin();
    m_animatedSprite.play(AnimationState::Idle);
    
    // Scale sprite to match entity's defined size
    int frameWidth = animSet->getDefaultFrameWidth();
    int frameHeight = animSet->getDefaultFrameHeight();
    if (frameWidth > 0 && frameHeight > 0) {
        float scaleX = m_size.x / static_cast<float>(frameWidth);
        float scaleY = m_size.y / static_cast<float>(frameHeight);
        m_animatedSprite.setScale(sf::Vector2f(scaleX, scaleY));
    }
    
    m_hasSprite = true;
}

void Entity::playAnimation(const std::string& animName) {
    if (!m_hasSprite) return;
    
    // Simply play the animation - the AnimatedSprite handles texture switching
    m_animatedSprite.play(animName);
}

void Entity::updateSpriteDirection(sf::Vector2f movement) {
    if (std::abs(movement.x) > 0.01f || std::abs(movement.y) > 0.01f) {
        m_animatedSprite.setDirectionFromMovement(movement);
    }
}

void Entity::startDeathAnimation() {
    if (m_isDying) return;  // Already dying
    
    const AnimationSet* animSet = m_animatedSprite.getAnimationSet();
    if (m_hasSprite && animSet && animSet->hasAnimation(AnimationState::Death)) {
        m_isDying = true;
        playAnimation(AnimationState::Death);
    }
    // If no death animation, entity will be removed immediately (m_isDying stays false)
}

void Entity::updateDeathAnimation(float deltaTime) {
    if (!m_isDying) return;
    
    if (m_hasSprite) {
        m_animatedSprite.update(deltaTime);
        
        // Check if death animation finished
        if (m_animatedSprite.isFinished()) {
            m_isDying = false;  // Mark as ready for removal
        }
    } else {
        // No sprite, immediately ready for removal
        m_isDying = false;
    }
}
