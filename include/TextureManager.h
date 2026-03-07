#pragma once

#include <SFML/Graphics/Texture.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>

// Forward declarations
class AnimationSet;

// Singleton texture manager - loads and caches textures and animation sets
class TextureManager {
public:
    static TextureManager& instance();
    
    // Load texture from file (returns nullptr if failed)
    // Caches result - subsequent calls with same path return cached texture
    sf::Texture* loadTexture(const std::string& filepath);
    
    // Legacy alias for loadTexture
    sf::Texture* load(const std::string& filepath) { return loadTexture(filepath); }
    
    // Get already-loaded texture (returns nullptr if not loaded)
    sf::Texture* getTexture(const std::string& filepath);
    
    // Legacy alias for getTexture
    sf::Texture* get(const std::string& filepath) { return getTexture(filepath); }
    
    // Check if texture is loaded
    bool isTextureLoaded(const std::string& filepath) const;
    
    // Legacy alias for isTextureLoaded
    bool isLoaded(const std::string& filepath) const { return isTextureLoaded(filepath); }
    
    // Preload multiple textures
    void preload(const std::vector<std::string>& filepaths);
    
    // ============= AnimationSet Management =============
    
    // Load or get a cached AnimationSet by base path (e.g., "units/worker")
    // This looks for files like "units/worker_idle.png", "units/worker_walk.png", etc.
    // Returns nullptr if no animations could be loaded
    AnimationSet* loadAnimationSet(const std::string& basePath);
    
    // Get an already-loaded AnimationSet (returns nullptr if not loaded)
    AnimationSet* getAnimationSet(const std::string& basePath);
    
    // Check if an AnimationSet is loaded
    bool isAnimationSetLoaded(const std::string& basePath) const;
    
    // Load a static sprite as an AnimationSet (single-frame idle animation)
    // Useful for non-animated entities like buildings or resources
    AnimationSet* loadStaticSprite(const std::string& texturePath);
    
    // ============= Cleanup =============
    
    // Clear all cached textures and animation sets
    void clear();
    
    // Clear only animation sets (keeps textures)
    void clearAnimationSets();
    
    // Get base asset path
    static std::string getAssetPath(const std::string& relativePath);
    
private:
    TextureManager() = default;
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;
    
    // Internal helper to create Animation from texture file
    bool loadAnimationFromFile(AnimationSet& animSet, const std::string& stateName, 
                                const std::string& filePath, bool isDirectional);
    
    std::unordered_map<std::string, std::unique_ptr<sf::Texture>> m_textures;
    std::unordered_map<std::string, std::unique_ptr<AnimationSet>> m_animationSets;
    
    static std::string s_assetBasePath;
};

// Convenience macro
#define TEXTURES TextureManager::instance()
