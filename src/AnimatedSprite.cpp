#include "AnimatedSprite.h"

void AnimatedSprite::setAnimationSet(const AnimationSet* animSet) {
    m_animationSet = animSet;
    m_currentAnimation = nullptr;
    m_currentAnimationName.clear();
    m_currentFrame = 0;
    m_frameTime = 0.0f;
    m_sprite.reset();
}

void AnimatedSprite::play(const std::string& animationName, bool force) {
    if (!m_animationSet) return;
    
    // Already playing this animation?
    if (!force && m_currentAnimationName == animationName && m_playing) {
        return;
    }
    
    const Animation* anim = m_animationSet->getAnimation(animationName);
    if (!anim) return;
    
    // Check if we need to switch textures
    bool textureChanged = (!m_currentAnimation || m_currentAnimation->getTexture() != anim->getTexture());
    
    m_currentAnimation = anim;
    m_currentAnimationName = animationName;
    m_currentFrame = 0;
    m_frameTime = 0.0f;
    m_playing = true;
    m_paused = false;
    m_finished = false;
    
    // Update texture if changed
    if (textureChanged) {
        updateSpriteTexture();
    }
    
    updateSprite();
}

void AnimatedSprite::updateSpriteTexture() {
    if (!m_currentAnimation || !m_currentAnimation->getTexture()) {
        m_sprite.reset();
        return;
    }
    
    // Create new sprite with the animation's texture
    m_sprite.emplace(*m_currentAnimation->getTexture());
}

void AnimatedSprite::stop() {
    m_playing = false;
    m_paused = false;
}

void AnimatedSprite::pause() {
    m_paused = true;
}

void AnimatedSprite::resume() {
    m_paused = false;
}

void AnimatedSprite::update(float deltaTime) {
    if (!m_playing || m_paused || !m_currentAnimation || m_finished) {
        return;
    }
    
    if (m_currentAnimation->getFrameCount() == 0) {
        return;
    }
    
    m_frameTime += deltaTime * m_playbackSpeed;
    
    const AnimationFrame& currentFrame = m_currentAnimation->getFrame(m_currentFrame);
    
    // Advance frames while we have time
    while (m_frameTime >= currentFrame.duration) {
        m_frameTime -= currentFrame.duration;
        m_currentFrame++;
        
        // End of animation?
        if (m_currentFrame >= m_currentAnimation->getFrameCount()) {
            if (m_currentAnimation->loops()) {
                m_currentFrame = 0;
            } else {
                m_currentFrame = m_currentAnimation->getFrameCount() - 1;
                m_finished = true;
                break;
            }
        }
    }
    
    updateSprite();
}

void AnimatedSprite::render(sf::RenderTarget& target, sf::Vector2f position) {
    if (!m_sprite) return;
    m_sprite->setPosition(position);
    target.draw(*m_sprite);
}

void AnimatedSprite::setOrigin(sf::Vector2f origin) {
    m_customOrigin = origin;
    m_useCustomOrigin = true;
    if (m_sprite) m_sprite->setOrigin(origin);
}

void AnimatedSprite::centerOrigin() {
    m_useCustomOrigin = false;
    if (!m_sprite) return;
    sf::Vector2f size = getFrameSize();
    m_sprite->setOrigin(sf::Vector2f(size.x / 2.0f, size.y / 2.0f));
}

void AnimatedSprite::setScale(sf::Vector2f scale) {
    m_scale = scale;
    if (m_sprite) m_sprite->setScale(m_scale);
}

void AnimatedSprite::setScale(float uniformScale) {
    setScale(sf::Vector2f(uniformScale, uniformScale));
}

void AnimatedSprite::setRotation(float degrees) {
    if (m_sprite) m_sprite->setRotation(sf::degrees(degrees));
}

void AnimatedSprite::setColor(sf::Color color) {
    if (m_sprite) m_sprite->setColor(color);
}

void AnimatedSprite::setDirection(Direction dir) {
    if (m_direction != dir) {
        m_direction = dir;
        updateSprite();
    }
}

void AnimatedSprite::setDirectionFromMovement(sf::Vector2f movement) {
    setDirection(directionFromVector(movement));
}

sf::Vector2f AnimatedSprite::getSize() const {
    sf::Vector2f frameSize = getFrameSize();
    return sf::Vector2f(frameSize.x * m_scale.x, frameSize.y * m_scale.y);
}

sf::Vector2f AnimatedSprite::getFrameSize() const {
    if (!m_currentAnimation) return sf::Vector2f(0.f, 0.f);
    return sf::Vector2f(
        static_cast<float>(m_currentAnimation->getFrameWidth()),
        static_cast<float>(m_currentAnimation->getFrameHeight())
    );
}

void AnimatedSprite::updateSprite() {
    if (!m_sprite || !m_currentAnimation) return;
    
    const AnimationFrame& frame = m_currentAnimation->getFrame(m_currentFrame);
    
    // Apply texture rect with direction row offset (if directional)
    sf::IntRect rect = frame.textureRect;
    
    // Offset Y position based on direction (each direction is a row)
    if (m_currentAnimation->isDirectional()) {
        int frameHeight = m_currentAnimation->getFrameHeight();
        rect.position.y = static_cast<int>(m_direction) * frameHeight;
    }
    
    m_sprite->setTextureRect(rect);
    
    // Handle origin
    if (m_useCustomOrigin) {
        m_sprite->setOrigin(m_customOrigin);
    } else {
        // Center origin based on frame size
        float halfWidth = static_cast<float>(std::abs(rect.size.x)) / 2.0f;
        float halfHeight = static_cast<float>(std::abs(rect.size.y)) / 2.0f;
        m_sprite->setOrigin(sf::Vector2f(halfWidth, halfHeight));
    }
    
    // Apply scale
    m_sprite->setScale(m_scale);
}
