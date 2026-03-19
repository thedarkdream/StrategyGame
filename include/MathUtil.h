#pragma once

#include <SFML/System/Vector2.hpp>
#include <cmath>

// Utility functions for vector math
namespace MathUtil {
    // Standard pi — use this instead of inline 3.14159… literals.
    constexpr float PI = 3.14159265f;
    // Distance squared (avoids sqrt - use when comparing distances)
    inline float distanceSquared(sf::Vector2f a, sf::Vector2f b) {
        sf::Vector2f diff = a - b;
        return diff.x * diff.x + diff.y * diff.y;
    }
    
    // Euclidean distance between two points
    inline float distance(sf::Vector2f a, sf::Vector2f b) {
        return std::sqrt(distanceSquared(a, b));
    }
    
    // Normalize a vector (return unit vector)
    inline sf::Vector2f normalize(sf::Vector2f v) {
        float length = std::sqrt(v.x * v.x + v.y * v.y);
        if (length > 0.0f) {
            return sf::Vector2f(v.x / length, v.y / length);
        }
        return sf::Vector2f(0.0f, 0.0f);
    }
    
    // Get the length/magnitude of a vector
    inline float length(sf::Vector2f v) {
        return std::sqrt(v.x * v.x + v.y * v.y);
    }

    // Return the tile-grid top-left corner of a building given its world-space
    // centre position and tile footprint size.
    inline sf::Vector2i buildingTileOrigin(sf::Vector2f centerPos,
                                           sf::Vector2i tileSize,
                                           int tilePixelSize) {
        return {
            static_cast<int>((centerPos.x - tileSize.x * tilePixelSize / 2.0f) / tilePixelSize),
            static_cast<int>((centerPos.y - tileSize.y * tilePixelSize / 2.0f) / tilePixelSize)
        };
    }
}
