#include "Animation.h"

// Animation implementation

Animation::Animation(const std::string& name, bool loops)
    : m_name(name)
    , m_loops(loops)
{
}

void Animation::addFrame(const sf::IntRect& rect, float duration) {
    m_frames.push_back({rect, duration});
}

void Animation::addFrames(const std::vector<sf::IntRect>& rects, float duration) {
    for (const auto& rect : rects) {
        addFrame(rect, duration);
    }
}

void Animation::addFramesFromStrip(int startX, int startY, int frameWidth, int frameHeight,
                                    int frameCount, float frameDuration) {
    m_frameWidth = frameWidth;
    m_frameHeight = frameHeight;
    for (int i = 0; i < frameCount; ++i) {
        sf::IntRect rect(sf::Vector2i(startX + i * frameWidth, startY), 
                         sf::Vector2i(frameWidth, frameHeight));
        addFrame(rect, frameDuration);
    }
}

const AnimationFrame& Animation::getFrame(int index) const {
    static AnimationFrame emptyFrame;
    if (index < 0 || index >= static_cast<int>(m_frames.size())) {
        return emptyFrame;
    }
    return m_frames[index];
}

float Animation::getTotalDuration() const {
    float total = 0.0f;
    for (const auto& frame : m_frames) {
        total += frame.duration;
    }
    return total;
}

// AnimationSet implementation

void AnimationSet::addAnimation(const Animation& animation) {
    const std::string& name = animation.getName();
    m_animations[name] = animation;
    
    // Set default frame dimensions from first animation that has them
    if (m_defaultFrameWidth == 0 && animation.getFrameWidth() > 0) {
        m_defaultFrameWidth = animation.getFrameWidth();
        m_defaultFrameHeight = animation.getFrameHeight();
    }
}

void AnimationSet::addAnimation(Animation&& animation) {
    std::string name = animation.getName();
    
    // Set default frame dimensions from first animation that has them
    if (m_defaultFrameWidth == 0 && animation.getFrameWidth() > 0) {
        m_defaultFrameWidth = animation.getFrameWidth();
        m_defaultFrameHeight = animation.getFrameHeight();
    }
    
    m_animations[name] = std::move(animation);
}

const Animation* AnimationSet::getAnimation(const std::string& name) const {
    auto it = m_animations.find(name);
    if (it != m_animations.end()) {
        return &it->second;
    }
    return nullptr;
}

bool AnimationSet::hasAnimation(const std::string& name) const {
    return m_animations.find(name) != m_animations.end();
}

std::vector<std::string> AnimationSet::getAnimationNames() const {
    std::vector<std::string> names;
    names.reserve(m_animations.size());
    for (const auto& [name, _] : m_animations) {
        names.push_back(name);
    }
    return names;
}
