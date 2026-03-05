#pragma once

#include <SFML/Graphics/Texture.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>

// Singleton texture manager - loads and caches textures
class TextureManager {
public:
    static TextureManager& instance();
    
    // Load texture from file (returns nullptr if failed)
    // Caches result - subsequent calls with same path return cached texture
    sf::Texture* load(const std::string& filepath);
    
    // Get already-loaded texture (returns nullptr if not loaded)
    sf::Texture* get(const std::string& filepath);
    
    // Check if texture is loaded
    bool isLoaded(const std::string& filepath) const;
    
    // Preload multiple textures
    void preload(const std::vector<std::string>& filepaths);
    
    // Clear all cached textures
    void clear();
    
    // Get base asset path
    static std::string getAssetPath(const std::string& relativePath);
    
private:
    TextureManager() = default;
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;
    
    std::unordered_map<std::string, std::unique_ptr<sf::Texture>> m_textures;
    
    static std::string s_assetBasePath;
};

// Convenience macro
#define TEXTURES TextureManager::instance()
