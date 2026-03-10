#include "SoundManager.h"
#include <cmath>
#include <algorithm>
#include <iostream>

SoundManager& SoundManager::instance() {
    static SoundManager instance;
    return instance;
}

SoundManager::SoundManager() {
    // Reserve space for active sounds
    m_activeSounds.reserve(MAX_CONCURRENT_SOUNDS);
}

void SoundManager::setMasterVolume(float volume) {
    m_masterVolume = std::clamp(volume, 0.0f, 100.0f);
}

void SoundManager::setSFXVolume(float volume) {
    m_sfxVolume = std::clamp(volume, 0.0f, 100.0f);
}

void SoundManager::setListenerPosition(sf::Vector2f position) {
    m_listenerPosition = position;
}

void SoundManager::setAudibleRange(float range) {
    m_audibleRange = std::max(1.0f, range);
}

void SoundManager::playSound(const std::string& filepath) {
    // Play at full volume (no positional falloff)
    sf::SoundBuffer* buffer = loadBuffer(filepath);
    if (!buffer) return;
    
    ActiveSound* activeSound = getOrCreateSound(*buffer);
    if (!activeSound) return;
    
    float finalVolume = (m_masterVolume / 100.0f) * (m_sfxVolume / 100.0f) * 100.0f;
    activeSound->sound->setVolume(finalVolume);
    activeSound->sound->play();
}

void SoundManager::playSound(const std::string& filepath, sf::Vector2f worldPosition) {
    playSound(filepath, worldPosition, 1.0f);
}

void SoundManager::playSound(const std::string& filepath, sf::Vector2f worldPosition, float volumeMultiplier) {
    // Calculate positional volume
    float positionalVolume = calculatePositionalVolume(worldPosition);
    if (positionalVolume <= 0.01f) {
        // Too quiet, don't bother playing
        return;
    }
    
    sf::SoundBuffer* buffer = loadBuffer(filepath);
    if (!buffer) return;
    
    ActiveSound* activeSound = getOrCreateSound(*buffer);
    if (!activeSound) return;
    
    float finalVolume = (m_masterVolume / 100.0f) * (m_sfxVolume / 100.0f) 
                       * positionalVolume * volumeMultiplier * 100.0f;
    activeSound->sound->setVolume(std::clamp(finalVolume, 0.0f, 100.0f));
    activeSound->sound->play();
}

sf::SoundBuffer* SoundManager::loadBuffer(const std::string& filepath) {
    // Check cache first
    auto it = m_buffers.find(filepath);
    if (it != m_buffers.end()) {
        return it->second.get();
    }
    
    // Try to load
    std::string fullPath = getSoundPath(filepath);
    auto buffer = std::make_unique<sf::SoundBuffer>();
    
    if (!buffer->loadFromFile(fullPath)) {
        std::cerr << "SoundManager: Failed to load sound: " << fullPath << std::endl;
        return nullptr;
    }
    
    sf::SoundBuffer* ptr = buffer.get();
    m_buffers[filepath] = std::move(buffer);
    
    // Warm the pool on first buffer load so all sf::Sound (OpenAL source) objects
    // are allocated now instead of lazily during gameplay.
    if (!m_poolWarmed) {
        warmPool(*ptr);
    }
    
    return ptr;
}

bool SoundManager::isBufferLoaded(const std::string& filepath) const {
    return m_buffers.find(filepath) != m_buffers.end();
}

void SoundManager::preload(const std::vector<std::string>& filepaths) {
    for (const auto& path : filepaths) {
        loadBuffer(path);
    }
    // Pool is warmed automatically by loadBuffer() on first buffer - nothing extra needed.
}

void SoundManager::clearBuffers() {
    // Stop all sounds first (they reference buffers)
    stopAll();
    m_buffers.clear();
}

void SoundManager::update() {
    // Pool slots are reused in-place; nothing to erase.
}

void SoundManager::stopAll() {
    for (auto& s : m_activeSounds) {
        if (s) s->sound->stop();
    }
    // Keep pool intact - don't clear, slots will be reused.
}

void SoundManager::clear() {
    for (auto& s : m_activeSounds) {
        if (s) s->sound->stop();
    }
    m_activeSounds.clear();
    m_buffers.clear();
    m_poolWarmed = false;
}

ActiveSound* SoundManager::getOrCreateSound(sf::SoundBuffer& buffer) {
    // Scan for a slot that is not currently playing - reuse it (no allocation).
    for (auto& slot : m_activeSounds) {
        if (slot && !slot->isPlaying()) {
            slot->sound->setBuffer(buffer);
            slot->buffer = &buffer;
            return slot.get();
        }
    }
    
    // All slots are busy.
    if (m_activeSounds.size() < MAX_CONCURRENT_SOUNDS) {
        // Pool not yet full (pre-warm may not have run) - allocate one more.
        m_activeSounds.push_back(std::make_unique<ActiveSound>(buffer));
        return m_activeSounds.back().get();
    }
    
    // Steal the first (oldest) slot.
    m_activeSounds[0]->sound->stop();
    m_activeSounds[0]->sound->setBuffer(buffer);
    m_activeSounds[0]->buffer = &buffer;
    return m_activeSounds[0].get();
}

void SoundManager::warmPool(sf::SoundBuffer& seedBuffer) {
    if (m_poolWarmed) return;
    m_activeSounds.reserve(MAX_CONCURRENT_SOUNDS);
    while (m_activeSounds.size() < MAX_CONCURRENT_SOUNDS) {
        auto slot = std::make_unique<ActiveSound>(seedBuffer);
        slot->sound->stop();  // Ensure stopped, not playing
        m_activeSounds.push_back(std::move(slot));
    }
    m_poolWarmed = true;
}

float SoundManager::calculatePositionalVolume(sf::Vector2f worldPosition) const {
    // Calculate distance from listener
    sf::Vector2f diff = worldPosition - m_listenerPosition;
    float distance = std::sqrt(diff.x * diff.x + diff.y * diff.y);
    
    // Full volume within minimum distance
    if (distance <= m_minVolumeDistance) {
        return 1.0f;
    }
    
    // Beyond audible range, no sound
    if (distance >= m_audibleRange) {
        return 0.0f;
    }
    
    // Linear falloff between min distance and audible range
    // Could use inverse square law for more realistic falloff
    float falloffRange = m_audibleRange - m_minVolumeDistance;
    float distanceInFalloff = distance - m_minVolumeDistance;
    float t = 1.0f - (distanceInFalloff / falloffRange);
    
    // Apply slight curve for more natural falloff
    return t * t;  // Quadratic falloff feels more natural
}

std::string SoundManager::getSoundPath(const std::string& relativePath) {
    // If already an absolute path or starts with sounds/, use as-is
    if (relativePath.find("sounds/") == 0 || relativePath.find("sounds\\") == 0) {
        return relativePath;
    }
    return "sounds/" + relativePath;
}
