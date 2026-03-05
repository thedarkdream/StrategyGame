#include "TextureManager.h"
#include <iostream>

std::string TextureManager::s_assetBasePath = "assets/";

TextureManager& TextureManager::instance() {
    static TextureManager manager;
    return manager;
}

sf::Texture* TextureManager::load(const std::string& filepath) {
    // Check if already loaded
    auto it = m_textures.find(filepath);
    if (it != m_textures.end()) {
        return it->second.get();
    }
    
    // Try to load
    auto texture = std::make_unique<sf::Texture>();
    std::string fullPath = getAssetPath(filepath);
    
    if (!texture->loadFromFile(fullPath)) {
        std::cerr << "Failed to load texture: " << fullPath << std::endl;
        return nullptr;
    }
    
    sf::Texture* ptr = texture.get();
    m_textures[filepath] = std::move(texture);
    
    return ptr;
}

sf::Texture* TextureManager::get(const std::string& filepath) {
    auto it = m_textures.find(filepath);
    if (it != m_textures.end()) {
        return it->second.get();
    }
    return nullptr;
}

bool TextureManager::isLoaded(const std::string& filepath) const {
    return m_textures.find(filepath) != m_textures.end();
}

void TextureManager::preload(const std::vector<std::string>& filepaths) {
    for (const auto& path : filepaths) {
        load(path);
    }
}

void TextureManager::clear() {
    m_textures.clear();
}

std::string TextureManager::getAssetPath(const std::string& relativePath) {
    return s_assetBasePath + relativePath;
}
