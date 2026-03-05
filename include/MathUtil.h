#pragma once

#include <SFML/System/Vector2.hpp>
#include <cmath>

// Utility functions for vector math
namespace MathUtil {
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
}
