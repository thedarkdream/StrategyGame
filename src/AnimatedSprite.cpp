#include "AnimatedSprite.h"
#include <cmath>

void AnimatedSprite::setAnimationSet(const AnimationSet* animSet) {
    m_animationSet = animSet;
    m_currentAnimation = nullptr;
    m_currentAnimationName.clear();
    m_currentFrame = 0;
    m_frameTime = 0.0f;
}

void AnimatedSprite::play(const std::string& animationName, bool force) {
    if (!m_animationSet) return;
    
    // Already playing this animation?
    if (!force && m_currentAnimationName == animationName && m_playing) {
        return;
    }
    
    const Animation* anim = m_animationSet->getAnimation(animationName);
    if (!anim) return;
    
    m_currentAnimation = anim;
    m_currentAnimationName = animationName;
    m_currentFrame = 0;
    m_frameTime = 0.0f;
    m_playing = true;
    m_paused = false;
    m_finished = false;
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
}

void AnimatedSprite::setOrigin(sf::Vector2f origin) {
    m_customOrigin = origin;
    m_useCustomOrigin = true;
}

void AnimatedSprite::centerOrigin() {
    m_useCustomOrigin = false;
}

void AnimatedSprite::setScale(sf::Vector2f scale) {
    m_scale = scale;
}

void AnimatedSprite::setScale(float uniformScale) {
    setScale(sf::Vector2f(uniformScale, uniformScale));
}

void AnimatedSprite::setDirection(Direction dir) {
    m_direction = dir;
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

const sf::Texture* AnimatedSprite::getCurrentTexture() const {
    return m_currentAnimation ? m_currentAnimation->getTexture() : nullptr;
}

sf::IntRect AnimatedSprite::getCurrentTextureRect() const {
    if (!m_currentAnimation || m_currentAnimation->getFrameCount() == 0) return sf::IntRect();

    sf::IntRect rect = m_currentAnimation->getFrame(m_currentFrame).textureRect;

    // Each direction of a directional animation is a row of the sheet.
    if (m_currentAnimation->isDirectional())
        rect.position.y = static_cast<int>(m_direction) * m_currentAnimation->getFrameHeight();
    return rect;
}

sf::Vector2f AnimatedSprite::getOrigin() const {
    if (m_useCustomOrigin) return m_customOrigin;
    const sf::IntRect rect = getCurrentTextureRect();
    return sf::Vector2f(static_cast<float>(std::abs(rect.size.x)) / 2.0f,
                        static_cast<float>(std::abs(rect.size.y)) / 2.0f);
}
