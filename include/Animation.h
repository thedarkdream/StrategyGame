#pragma once

#include "MathUtil.h"
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Vector2.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cmath>

// 8-directional facing for sprites
// Row order in sprite sheets: E, NE, N, NW, W, SW, S, SE
enum class Direction : int {
    East = 0,
    NorthEast = 1,
    North = 2,
    NorthWest = 3,
    West = 4,
    SouthWest = 5,
    South = 6,
    SouthEast = 7,
    Count = 8
};

// Convert movement vector to 8-way direction
inline Direction directionFromVector(sf::Vector2f movement) {
    if (movement.x == 0.f && movement.y == 0.f) {
        return Direction::South;  // Default facing
    }
    
    // Calculate angle in radians, then convert to direction
    // atan2 returns angle from -PI to PI, with 0 pointing right (East)
    float angle = std::atan2(movement.y, movement.x);
    
    // Convert to 0-360 degrees
    float degrees = angle * 180.0f / MathUtil::PI;
    if (degrees < 0) degrees += 360.0f;
    
    // Map to 8 directions (each direction covers 45 degrees)
    // East is 0°, angles go clockwise in screen space (Y increases downward)
    // 0° = East, 45° = SouthEast, 90° = South, etc.
    int sector = static_cast<int>((degrees + 22.5f) / 45.0f) % 8;
    
    // Map sector to Direction enum
    // sector 0 = East (0°), 1 = SE (45°), 2 = S (90°), 3 = SW (135°)
    // sector 4 = W (180°), 5 = NW (225°), 6 = N (270°), 7 = NE (315°)
    static constexpr Direction sectorToDir[] = {
        Direction::East,      // 0
        Direction::SouthEast, // 1
        Direction::South,     // 2
        Direction::SouthWest, // 3
        Direction::West,      // 4
        Direction::NorthWest, // 5
        Direction::North,     // 6
        Direction::NorthEast  // 7
    };
    
    return sectorToDir[sector];
}

// A single frame in an animation
struct AnimationFrame {
    sf::IntRect textureRect;   // Region of the sprite sheet
    float duration = 0.1f;      // How long this frame displays (seconds)
};

// A sequence of frames forming one animation (e.g., "walk", "attack")
// Each animation has its own texture and directional settings
class Animation {
public:
    Animation() = default;
    Animation(const std::string& name, bool loops = true);
    
    // Add frames
    void addFrame(const sf::IntRect& rect, float duration = 0.1f);
    void addFrames(const std::vector<sf::IntRect>& rects, float duration = 0.1f);
    
    // Create frames from a horizontal strip in sprite sheet
    // startX, startY: top-left of first frame
    // frameWidth, frameHeight: size of each frame
    // frameCount: number of frames
    void addFramesFromStrip(int startX, int startY, int frameWidth, int frameHeight, 
                            int frameCount, float frameDuration = 0.1f);
    
    // Accessors
    const std::string& getName() const { return m_name; }
    bool loops() const { return m_loops; }
    int getFrameCount() const { return static_cast<int>(m_frames.size()); }
    const AnimationFrame& getFrame(int index) const;
    float getTotalDuration() const;
    
    // Set looping
    void setLoops(bool loops) { m_loops = loops; }
    
    // Texture for this specific animation (each animation can have its own texture)
    void setTexture(sf::Texture* texture) { m_texture = texture; }
    sf::Texture* getTexture() const { return m_texture; }
    
    // Directional settings
    void setDirectional(bool directional) { m_isDirectional = directional; }
    bool isDirectional() const { return m_isDirectional; }
    
    // Frame dimensions (for directional row offset calculation)
    void setFrameWidth(int width) { m_frameWidth = width; }
    void setFrameHeight(int height) { m_frameHeight = height; }
    int getFrameWidth() const { return m_frameWidth; }
    int getFrameHeight() const { return m_frameHeight; }
    
private:
    std::string m_name;
    std::vector<AnimationFrame> m_frames;
    bool m_loops = true;
    sf::Texture* m_texture = nullptr;
    bool m_isDirectional = true;  // If true, sprite sheet has 8 direction rows
    int m_frameWidth = 0;
    int m_frameHeight = 0;
};

// Collection of animations for an entity (e.g., Worker has idle, walk, gather)
// The AnimationSet is primarily used as a key for caching in TextureManager
class AnimationSet {
public:
    AnimationSet() = default;
    explicit AnimationSet(const std::string& name) : m_name(name) {}
    
    // Name identifier for this animation set
    const std::string& getName() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }
    
    // Add/get animations
    void addAnimation(const Animation& animation);
    void addAnimation(Animation&& animation);
    const Animation* getAnimation(const std::string& name) const;
    bool hasAnimation(const std::string& name) const;
    
    // Get all animation names
    std::vector<std::string> getAnimationNames() const;
    
    // Default frame dimensions (from first loaded animation)
    int getDefaultFrameWidth() const { return m_defaultFrameWidth; }
    int getDefaultFrameHeight() const { return m_defaultFrameHeight; }
    void setDefaultFrameWidth(int width) { m_defaultFrameWidth = width; }
    void setDefaultFrameHeight(int height) { m_defaultFrameHeight = height; }
    
private:
    std::string m_name;
    std::unordered_map<std::string, Animation> m_animations;
    int m_defaultFrameWidth = 0;
    int m_defaultFrameHeight = 0;
};

// Common animation state names
namespace AnimationState {
    constexpr const char* Idle = "idle";
    constexpr const char* Walk = "walk";
    constexpr const char* Attack = "attack";
    constexpr const char* Gather = "gather";
    constexpr const char* Death = "death";
    constexpr const char* Build = "build";
}
