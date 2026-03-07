#include "TextureManager.h"
#include "Animation.h"
#include <iostream>

std::string TextureManager::s_assetBasePath = "assets/";

TextureManager& TextureManager::instance() {
    static TextureManager manager;
    return manager;
}

sf::Texture* TextureManager::loadTexture(const std::string& filepath) {
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

sf::Texture* TextureManager::getTexture(const std::string& filepath) {
    auto it = m_textures.find(filepath);
    if (it != m_textures.end()) {
        return it->second.get();
    }
    return nullptr;
}

bool TextureManager::isTextureLoaded(const std::string& filepath) const {
    return m_textures.find(filepath) != m_textures.end();
}

void TextureManager::preload(const std::vector<std::string>& filepaths) {
    for (const auto& path : filepaths) {
        loadTexture(path);
    }
}

AnimationSet* TextureManager::loadAnimationSet(const std::string& basePath) {
    // Check if already loaded
    auto it = m_animationSets.find(basePath);
    if (it != m_animationSets.end()) {
        return it->second.get();
    }
    
    // Create new AnimationSet
    auto animSet = std::make_unique<AnimationSet>(basePath);
    
    // Try to load common animation states
    // Most animations are 8-directional, death is non-directional
    const std::vector<std::tuple<std::string, std::string, bool>> animFiles = {
        {AnimationState::Idle, basePath + "_idle.png", true},      // Directional
        {AnimationState::Walk, basePath + "_walk.png", true},      // Directional
        {AnimationState::Attack, basePath + "_attack.png", true},  // Directional
        {AnimationState::Gather, basePath + "_gather.png", true},  // Directional
        {AnimationState::Death, basePath + "_death.png", false},   // Non-directional
        {AnimationState::Build, basePath + "_build.png", true},    // Directional
    };
    
    bool hasAnyAnim = false;
    
    for (const auto& [stateName, filePath, isDirectional] : animFiles) {
        if (loadAnimationFromFile(*animSet, stateName, filePath, isDirectional)) {
            hasAnyAnim = true;
        }
    }
    
    if (!hasAnyAnim) {
        std::cerr << "Failed to load any animations for: " << basePath << std::endl;
        return nullptr;
    }
    
    AnimationSet* ptr = animSet.get();
    m_animationSets[basePath] = std::move(animSet);
    
    return ptr;
}

bool TextureManager::loadAnimationFromFile(AnimationSet& animSet, const std::string& stateName,
                                            const std::string& filePath, bool isDirectional) {
    sf::Texture* tex = loadTexture(filePath);
    if (!tex) {
        return false;
    }
    
    sf::Vector2u texSize = tex->getSize();
    
    int frameHeight, frameWidth, frameCount;
    
    if (isDirectional) {
        // 8-directional sprite sheets: 8 rows (directions), N columns (frames)
        // Frame height = texture height / 8 directions
        // Frame width = frame height (assuming square frames)
        frameHeight = static_cast<int>(texSize.y) / 8;
        frameWidth = frameHeight;  // Square frames
        frameCount = static_cast<int>(texSize.x) / frameWidth;
    } else {
        // Non-directional (single row)
        frameHeight = static_cast<int>(texSize.y);
        frameWidth = frameHeight;  // Assume square frames
        frameCount = static_cast<int>(texSize.x) / frameWidth;
    }
    
    if (frameCount <= 0 || frameHeight <= 0) {
        return false;
    }
    
    // Create animation
    Animation anim(stateName);
    anim.setTexture(tex);
    anim.setDirectional(isDirectional);
    anim.setFrameWidth(frameWidth);
    anim.setFrameHeight(frameHeight);
    
    // Set frame duration based on animation type
    float frameDuration = (stateName == AnimationState::Idle) ? 0.2f : 0.1f;
    
    // Load frames from row 0 (East) - direction offset applied at render time
    anim.addFramesFromStrip(0, 0, frameWidth, frameHeight, frameCount, frameDuration);
    
    // Non-looping animations
    if (stateName == AnimationState::Attack || stateName == AnimationState::Death) {
        anim.setLoops(false);
    }
    
    animSet.addAnimation(std::move(anim));
    
    return true;
}

AnimationSet* TextureManager::getAnimationSet(const std::string& basePath) {
    auto it = m_animationSets.find(basePath);
    if (it != m_animationSets.end()) {
        return it->second.get();
    }
    return nullptr;
}

bool TextureManager::isAnimationSetLoaded(const std::string& basePath) const {
    return m_animationSets.find(basePath) != m_animationSets.end();
}

AnimationSet* TextureManager::loadStaticSprite(const std::string& texturePath) {
    // Check if already loaded as an animation set
    auto it = m_animationSets.find(texturePath);
    if (it != m_animationSets.end()) {
        return it->second.get();
    }
    
    // Load texture
    sf::Texture* tex = loadTexture(texturePath);
    if (!tex) {
        return nullptr;
    }
    
    // Create AnimationSet with single-frame idle animation
    auto animSet = std::make_unique<AnimationSet>(texturePath);
    
    sf::Vector2u texSize = tex->getSize();
    
    Animation staticAnim(AnimationState::Idle, true);
    staticAnim.setTexture(tex);
    staticAnim.setDirectional(false);  // Non-directional
    staticAnim.setFrameWidth(static_cast<int>(texSize.x));
    staticAnim.setFrameHeight(static_cast<int>(texSize.y));
    staticAnim.addFrame(
        sf::IntRect(sf::Vector2i(0, 0), sf::Vector2i(static_cast<int>(texSize.x), static_cast<int>(texSize.y))),
        1.0f  // Duration doesn't matter for single frame
    );
    
    animSet->addAnimation(std::move(staticAnim));
    
    AnimationSet* ptr = animSet.get();
    m_animationSets[texturePath] = std::move(animSet);
    
    return ptr;
}

void TextureManager::clear() {
    m_animationSets.clear();
    m_textures.clear();
}

void TextureManager::clearAnimationSets() {
    m_animationSets.clear();
}

std::string TextureManager::getAssetPath(const std::string& relativePath) {
    return s_assetBasePath + relativePath;
}
