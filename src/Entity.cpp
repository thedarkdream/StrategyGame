#include "Entity.h"
#include "Constants.h"
#include "EntityData.h"
#include "TextureManager.h"
#include <cmath>

bool Entity::isResource() const {
    const EntityDef* def = ENTITY_DATA.get(m_type);
    return def && def->isResource();
}

Entity::Entity(EntityType type, Team team, sf::Vector2f position)
    : m_id(IdGenerator::instance().nextId())
    , m_type(type)
    , m_team(team)
    , m_position(position)
    , m_size(32.0f, 32.0f)
    , m_health(100)
    , m_maxHealth(100)
{
    // Set color based on team using the central teamColor utility
    m_color = teamColor(m_team);
    
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

    markUnderAttack();
    m_health -= damage;
    if (m_health <= 0) {
        m_health = 0;
        startDeathAnimation();
    }
}

void Entity::takeDamage(int damage, EntityPtr attacker) {
    // Store the last attacker's team for statistics tracking
    if (attacker) {
        m_lastAttackerTeam = attacker->getTeam();
    }
    // Base implementation just applies damage
    takeDamage(damage);
}

void Entity::takeDamage(int damage, Team attackerTeam) {
    // Store the attacker's team for statistics tracking
    m_lastAttackerTeam = attackerTeam;
    // Apply damage
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

void Entity::startHighlight(float duration) {
    m_highlightTimeRemaining = duration;
    m_highlightBlinkTimer = 0.0f;
}

void Entity::updateHighlight(float deltaTime) {
    if (m_highlightTimeRemaining > 0.0f) {
        m_highlightTimeRemaining -= deltaTime;
        m_highlightBlinkTimer += deltaTime;
        
        // Reset blink timer each period
        if (m_highlightBlinkTimer >= HIGHLIGHT_BLINK_PERIOD) {
            m_highlightBlinkTimer -= HIGHLIGHT_BLINK_PERIOD;
        }
    }
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
    
    // Call death hook (for sounds, effects, etc.)
    onDeath();
    
    const AnimationSet* animSet = m_animatedSprite.getAnimationSet();
    if (m_hasSprite && animSet && animSet->hasAnimation(AnimationState::Death)) {
        m_isDying = true;
        playAnimation(AnimationState::Death);
    }
    // If no death animation, entity will be removed immediately (m_isDying stays false)
}

void Entity::onDeath() {
    // Base implementation does nothing - subclasses override to play sounds, etc.
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
