#pragma once

#include "Animation.h"
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <string>
#include <optional>

// Renders an animated sprite using an AnimationSet
class AnimatedSprite {
public:
    AnimatedSprite() = default;
    
    // Set the animation set to use (shared, managed by TextureManager)
    void setAnimationSet(const AnimationSet* animSet);
    const AnimationSet* getAnimationSet() const { return m_animationSet; }
    
    // Play an animation by name
    // If force is true, restarts even if already playing
    void play(const std::string& animationName, bool force = false);
    
    // Stop animation (freeze on current frame)
    void stop();
    
    // Pause/resume
    void pause();
    void resume();
    bool isPaused() const { return m_paused; }
    
    // Check if current animation has finished (only relevant for non-looping)
    bool isFinished() const { return m_finished; }
    
    // Get current animation name
    const std::string& getCurrentAnimationName() const { return m_currentAnimationName; }
    
    // Update animation timing
    void update(float deltaTime);
    
    // Render at position
    void render(sf::RenderTarget& target, sf::Vector2f position);
    
    // Set origin (center point for positioning)
    void setOrigin(sf::Vector2f origin);
    void centerOrigin();  // Center based on current frame size
    
    // Set scale
    void setScale(sf::Vector2f scale);
    void setScale(float uniformScale);
    
    // Set rotation (degrees)
    void setRotation(float degrees);
    
    // Set color tint
    void setColor(sf::Color color);
    
    // 8-directional facing
    void setDirection(Direction dir);
    Direction getDirection() const { return m_direction; }
    void setDirectionFromMovement(sf::Vector2f movement);
    
    // Get current frame bounds (after scaling)
    sf::Vector2f getSize() const;
    
    // Get unscaled frame size
    sf::Vector2f getFrameSize() const;
    
    // Playback speed multiplier (1.0 = normal)
    void setPlaybackSpeed(float speed) { m_playbackSpeed = speed; }
    float getPlaybackSpeed() const { return m_playbackSpeed; }
    
private:
    void updateSprite();
    void updateSpriteTexture();  // Switch texture when animation changes
    
    const AnimationSet* m_animationSet = nullptr;
    const Animation* m_currentAnimation = nullptr;
    std::string m_currentAnimationName;
    
    int m_currentFrame = 0;
    float m_frameTime = 0.0f;
    float m_playbackSpeed = 1.0f;
    
    bool m_playing = false;
    bool m_paused = false;
    bool m_finished = false;
    
    Direction m_direction = Direction::South;
    
    sf::Vector2f m_scale = {1.0f, 1.0f};
    sf::Vector2f m_customOrigin = {0.0f, 0.0f};
    bool m_useCustomOrigin = false;
    
    // Deferred sprite creation (SFML 3.0 requires texture in constructor)
    std::optional<sf::Sprite> m_sprite;
};
