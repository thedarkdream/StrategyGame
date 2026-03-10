#pragma once

#include <SFML/Graphics/Font.hpp>
#include <string>
#include <unordered_map>
#include <vector>

// Singleton font manager — caches loaded fonts so each path is opened at most once.
class FontManager {
public:
    static FontManager& instance();

    // Try each path in order; return the first successfully loaded font.
    // Returns nullptr if every path fails to load.
    const sf::Font* loadFirstOf(const std::vector<std::string>& paths);

    // Convenience: Arial → Segoe UI fallback used throughout the project.
    const sf::Font* defaultFont();

private:
    FontManager() = default;
    FontManager(const FontManager&) = delete;
    FontManager& operator=(const FontManager&) = delete;

    std::unordered_map<std::string, sf::Font> m_cache;
    const sf::Font* m_default{ nullptr };
};
