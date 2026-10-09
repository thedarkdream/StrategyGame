#pragma once

#include "sprite/Animation.h"
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/Color.hpp>
#include <string>

// What a renderer needs to draw one frame: the texture, the source rectangle
// inside it and the size in world units.  The texture is null when there is
// nothing to draw.
struct SpriteFrame {
    const sf::Texture* texture = nullptr;
    sf::IntRect        rect;
    sf::Vector2f       size;

    bool valid() const { return texture && size.x > 0.0f && size.y > 0.0f; }
};

// Animation playback state for an entity or effect.  It owns no drawable: a
// renderer reads the current texture, source rectangle, origin and scale and
// draws them however it likes.
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
    
    // Set origin (center point for positioning)
    void setOrigin(sf::Vector2f origin);
    void centerOrigin();  // Center based on current frame size
    
    // Set scale
    void setScale(sf::Vector2f scale);
    void setScale(float uniformScale);
    
    // 8-directional facing
    void setDirection(Direction dir);
    Direction getDirection() const { return m_direction; }
    void setDirectionFromMovement(sf::Vector2f movement);
    
    // What a renderer needs to draw the current frame.  The texture is null and
    // the rectangle empty until an animation has been played.
    const sf::Texture* getCurrentTexture() const;
    sf::IntRect getCurrentTextureRect() const;   // includes the direction row
    sf::Vector2f getOrigin() const;              // in unscaled frame pixels
    sf::Vector2f getScale() const { return m_scale; }

    // The current frame as one value (see SpriteFrame).
    SpriteFrame getFrame() const { return { getCurrentTexture(), getCurrentTextureRect(), getSize() }; }

    // Get current frame bounds (after scaling)
    sf::Vector2f getSize() const;
    
    // Get unscaled frame size
    sf::Vector2f getFrameSize() const;
    
    // Playback speed multiplier (1.0 = normal)
    void setPlaybackSpeed(float speed) { m_playbackSpeed = speed; }
    float getPlaybackSpeed() const { return m_playbackSpeed; }
    
private:
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
};
