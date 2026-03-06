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
    m_animationSet = std::make_unique<AnimationSet>();
    
    // Try to load common animation states
    const std::vector<std::pair<std::string, std::string>> animFiles = {
        {AnimationState::Idle, basePath + "_idle.png"},
        {AnimationState::Walk, basePath + "_walk.png"},
        {AnimationState::Attack, basePath + "_attack.png"},
        {AnimationState::Gather, basePath + "_gather.png"},
        {AnimationState::Death, basePath + "_death.png"},
    };
    
    bool hasAnyAnim = false;
    int frameHeight = 0;  // Will be set from first loaded texture
    
    for (const auto& [stateName, filePath] : animFiles) {
        if (TEXTURES.load(filePath)) {
            // Get texture to determine frame count
            sf::Texture* tex = TEXTURES.get(filePath);
            if (!tex) continue;
            sf::Vector2u texSize = tex->getSize();
            
            int dirFrameHeight, frameWidth, frameCount;
            
            // Death animation is non-directional (single row)
            if (stateName == AnimationState::Death) {
                dirFrameHeight = static_cast<int>(texSize.y);
                frameWidth = dirFrameHeight;  // Assume square frames
                frameCount = static_cast<int>(texSize.x) / frameWidth;
            } else {
                // 8-directional sprite sheets: 8 rows (directions), N columns (frames)
                // Frame height = texture height / 8 directions
                // Frame width = frame height (assuming square frames)
                dirFrameHeight = static_cast<int>(texSize.y) / 8;
                frameWidth = dirFrameHeight;  // Square frames
                frameCount = static_cast<int>(texSize.x) / frameWidth;
            }
            
            if (frameCount > 0 && dirFrameHeight > 0) {
                Animation anim(stateName);
                float frameDuration = (stateName == AnimationState::Idle) ? 0.2f : 0.1f;
                // Load frames from row 0 (East) - direction offset applied at render time
                anim.addFramesFromStrip(0, 0, frameWidth, dirFrameHeight, frameCount, frameDuration);
                
                // Non-looping attack animation
                if (stateName == AnimationState::Attack) {
                    anim.setLoops(false);
                }
                // Death animation should not loop
                if (stateName == AnimationState::Death) {
                    anim.setLoops(false);
                }
                
                m_animationSet->addAnimation(anim);
                
                // Store frame height for direction row calculation (skip death since it's non-directional)
                if (!hasAnyAnim && stateName != AnimationState::Death) {
                    frameHeight = dirFrameHeight;
                }
                hasAnyAnim = true;
            }
        }
    }
    
    if (hasAnyAnim) {
        // Load idle as the main texture
        std::string idlePath = basePath + "_idle.png";
        if (TEXTURES.load(idlePath)) {
            m_animationSet->setTexture(TEXTURES.get(idlePath));
            m_animatedSprite.setAnimationSet(m_animationSet.get());
            m_animatedSprite.setFrameHeight(frameHeight);
            
            // Scale sprite to match entity's defined size
            if (frameHeight > 0) {
                // Assuming square frames for units
                float scaleX = m_size.x / static_cast<float>(frameHeight);
                float scaleY = m_size.y / static_cast<float>(frameHeight);
                m_animatedSprite.setScale(sf::Vector2f(scaleX, scaleY));
                // Set origin for centered positioning
                m_animatedSprite.setOrigin(sf::Vector2f(
                    static_cast<float>(frameHeight) / 2.0f,
                    static_cast<float>(frameHeight) / 2.0f
                ));
            } else {
                m_animatedSprite.centerOrigin();
            }
            
            m_hasSprite = true;
            playAnimation(AnimationState::Idle);
        }
    }
}

void Entity::loadStaticSprite(const std::string& texturePath) {
    // Load a static (non-animated, non-directional) sprite using the animation system
    sf::Texture* tex = TEXTURES.load(texturePath);
    if (!tex) return;
    
    m_animationSet = std::make_unique<AnimationSet>();
    m_animationSet->setTexture(tex);
    
    // Create a single-frame animation covering the entire texture
    sf::Vector2u texSize = tex->getSize();
    Animation staticAnim(AnimationState::Idle, true);
    staticAnim.addFrame(
        sf::IntRect(sf::Vector2i(0, 0), sf::Vector2i(static_cast<int>(texSize.x), static_cast<int>(texSize.y))),
        1.0f  // Duration doesn't matter for single frame
    );
    m_animationSet->addAnimation(std::move(staticAnim));
    
    m_animatedSprite.setAnimationSet(m_animationSet.get());
    // frameHeight = 0 means no directional row offset
    m_animatedSprite.setFrameHeight(0);
    m_animatedSprite.centerOrigin();
    m_animatedSprite.play(AnimationState::Idle);
    
    // Scale sprite to match entity's defined size
    if (texSize.x > 0 && texSize.y > 0) {
        float scaleX = m_size.x / static_cast<float>(texSize.x);
        float scaleY = m_size.y / static_cast<float>(texSize.y);
        m_animatedSprite.setScale(sf::Vector2f(scaleX, scaleY));
        // Re-center origin after scaling
        m_animatedSprite.setOrigin(sf::Vector2f(
            static_cast<float>(texSize.x) / 2.0f,
            static_cast<float>(texSize.y) / 2.0f
        ));
    }
    
    m_hasSprite = true;
}

void Entity::playAnimation(const std::string& animName) {
    if (!m_hasSprite) return;
    
    // Switch texture if needed for this animation
    std::string texPath;
    if (animName == AnimationState::Idle) {
        // Determine base path from entity type
        if (m_type == EntityType::Worker) texPath = "units/worker_idle.png";
    } else if (animName == AnimationState::Walk) {
        if (m_type == EntityType::Worker) texPath = "units/worker_walk.png";
    } else if (animName == AnimationState::Attack) {
        if (m_type == EntityType::Worker) texPath = "units/worker_attack.png";
    } else if (animName == AnimationState::Gather) {
        if (m_type == EntityType::Worker) texPath = "units/worker_gather.png";
    } else if (animName == AnimationState::Death) {
        if (m_type == EntityType::Worker) texPath = "units/worker_death.png";
    }
    
    if (!texPath.empty() && TEXTURES.load(texPath)) {
        sf::Texture* tex = TEXTURES.get(texPath);
        m_animationSet->setTexture(tex);
        m_animatedSprite.setAnimationSet(m_animationSet.get());
        
        // Recalculate and set frame height for directional offset
        if (tex) {
            int frameHeight = static_cast<int>(tex->getSize().y) / 8;
            m_animatedSprite.setFrameHeight(frameHeight);
        }
    }
    
    m_animatedSprite.play(animName);
}

void Entity::updateSpriteDirection(sf::Vector2f movement) {
    if (std::abs(movement.x) > 0.01f || std::abs(movement.y) > 0.01f) {
        m_animatedSprite.setDirectionFromMovement(movement);
    }
}

void Entity::startDeathAnimation() {
    if (m_isDying) return;  // Already dying
    
    if (m_hasSprite && m_animationSet->hasAnimation(AnimationState::Death)) {
        m_isDying = true;
        
        // Death animation is typically non-directional (single row)
        // Load the death texture and set up proper scaling
        std::string texPath;
        if (m_type == EntityType::Worker) texPath = "units/worker_death.png";
        
        if (!texPath.empty() && TEXTURES.load(texPath)) {
            sf::Texture* tex = TEXTURES.get(texPath);
            m_animationSet->setTexture(tex);
            m_animatedSprite.setAnimationSet(m_animationSet.get());
            
            if (tex) {
                // Death animation is single row (non-directional)
                int frameHeight = static_cast<int>(tex->getSize().y);
                int frameWidth = frameHeight;  // Assume square frames
                int frameCount = static_cast<int>(tex->getSize().x) / frameWidth;
                
                // Use frame height of 0 to disable directional offset
                m_animatedSprite.setFrameHeight(0);
                
                // Scale to match entity size
                float scaleX = m_size.x / static_cast<float>(frameWidth);
                float scaleY = m_size.y / static_cast<float>(frameHeight);
                m_animatedSprite.setScale(sf::Vector2f(scaleX, scaleY));
            }
        }
        
        m_animatedSprite.play(AnimationState::Death);
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
