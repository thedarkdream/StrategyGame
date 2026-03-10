#include "FontManager.h"
#include <iostream>

FontManager& FontManager::instance() {
    static FontManager inst;
    return inst;
}

const sf::Font* FontManager::loadFirstOf(const std::vector<std::string>& paths) {
    for (const auto& path : paths) {
        // Return cached entry if already loaded.
        auto it = m_cache.find(path);
        if (it != m_cache.end()) {
            return &it->second;
        }

        sf::Font font;
        if (font.openFromFile(path)) {
            auto [inserted, ok] = m_cache.emplace(path, std::move(font));
            return &inserted->second;
        }
    }
    return nullptr;
}

const sf::Font* FontManager::defaultFont() {
    if (!m_default) {
        m_default = loadFirstOf({
            "C:/Windows/Fonts/arial.ttf",
            "C:/Windows/Fonts/segoeui.ttf"
        });
        if (!m_default) {
            std::cerr << "FontManager: could not load any system font\n";
        }
    }
    return m_default;
}
