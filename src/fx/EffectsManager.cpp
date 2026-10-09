
#include "fx/EffectsManager.h"
#include "media/TextureManager.h"
#include <algorithm>

EffectsManager& EffectsManager::instance() {
    static EffectsManager manager;
    return manager;
}

Effect* EffectsManager::spawn(const std::string& animationPath, sf::Vector2f position, float scale) {
    auto effect = std::make_unique<Effect>(animationPath, position, scale);
    if (effect->isFinished()) return nullptr;  // Failed to initialise
    m_effects.push_back(std::move(effect));
    return m_effects.back().get();
}

void EffectsManager::spawnExplosion(sf::Vector2f position, float scale) {
    spawn("effects/explosion.png", position, scale);
}


void EffectsManager::spawnMoveEffect(sf::Vector2f position, float scale) {
    if (Effect* effect = spawn("effects/move.png", position, scale))
        effect->setGroundMarker(true);
}

void EffectsManager::update(float deltaTime) {
    // Update all effects and remove finished ones
    m_effects.erase(
        std::remove_if(m_effects.begin(), m_effects.end(),
            [deltaTime](std::unique_ptr<Effect>& effect) {
                return effect->update(deltaTime);
            }),
        m_effects.end()
    );
}

void EffectsManager::clear() {
    m_effects.clear();
}

void EffectsManager::preload() {
    TEXTURES.loadEffectAnimation("effects/explosion.png");
    TEXTURES.loadEffectAnimation("effects/move.png");
}
