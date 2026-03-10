#pragma once

#include <SFML/Audio.hpp>
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <functional>

// Wrapper for sf::Sound that holds both the sound and its buffer reference
struct ActiveSound {
    std::unique_ptr<sf::Sound> sound;
    sf::SoundBuffer* buffer;  // Reference to buffer (owned by SoundManager)
    
    ActiveSound(sf::SoundBuffer& buf) 
        : sound(std::make_unique<sf::Sound>(buf)), buffer(&buf) {}
    
    bool isPlaying() const { 
        return sound && sound->getStatus() == sf::Sound::Status::Playing; 
    }
};

// Sound manager singleton - handles loading, caching, and playing audio
// Features:
// - Sound buffer caching (like TextureManager)
// - Pool of sf::Sound instances for concurrent playback
// - Position-based volume falloff relative to listener (camera)
class SoundManager {
public:
    static SoundManager& instance();
    
    // ============= Configuration =============
    
    // Set master volume (0.0 - 100.0)
    void setMasterVolume(float volume);
    float getMasterVolume() const { return m_masterVolume; }
    
    // Set sound effects volume (0.0 - 100.0)
    void setSFXVolume(float volume);
    float getSFXVolume() const { return m_sfxVolume; }
    
    // Set listener position (typically camera center)
    void setListenerPosition(sf::Vector2f position);
    sf::Vector2f getListenerPosition() const { return m_listenerPosition; }
    
    // Set audible range - sounds beyond this distance are silent
    void setAudibleRange(float range);
    float getAudibleRange() const { return m_audibleRange; }
    
    // ============= Playing Sounds =============
    
    // Play a sound at full volume (UI sounds, etc.)
    void playSound(const std::string& filepath);
    
    // Play a sound at a world position (volume decreases with distance from listener)
    void playSound(const std::string& filepath, sf::Vector2f worldPosition);
    
    // Play with explicit volume multiplier (0.0 - 1.0)
    void playSound(const std::string& filepath, sf::Vector2f worldPosition, float volumeMultiplier);
    
    // ============= Sound Buffer Management =============
    
    // Preload a sound buffer
    sf::SoundBuffer* loadBuffer(const std::string& filepath);
    
    // Preload multiple sound buffers at once
    void preload(const std::vector<std::string>& filepaths);
    
    // Check if buffer is loaded
    bool isBufferLoaded(const std::string& filepath) const;
    
    // Clear all cached buffers
    void clearBuffers();
    
    // ============= Lifecycle =============
    
    // Update sound pool (removes finished sounds)
    void update();
    
    // Stop all currently playing sounds
    void stopAll();
    
    // Clear everything
    void clear();
    
private:
    SoundManager();
    SoundManager(const SoundManager&) = delete;
    SoundManager& operator=(const SoundManager&) = delete;
    
    // Get or create an available sound from the pool
    ActiveSound* getOrCreateSound(sf::SoundBuffer& buffer);
    
    // Pre-allocate all sound objects (call once after first buffer is loaded)
    // Warms the OpenAL source pool so no driver allocations happen during gameplay
    void warmPool(sf::SoundBuffer& seedBuffer);
    bool m_poolWarmed = false;
    
    // Calculate volume based on distance from listener
    float calculatePositionalVolume(sf::Vector2f worldPosition) const;
    
    // Sound buffer cache
    std::unordered_map<std::string, std::unique_ptr<sf::SoundBuffer>> m_buffers;
    
    // Pool of active sounds (created on-demand, limited count)
    std::vector<std::unique_ptr<ActiveSound>> m_activeSounds;
    static constexpr size_t MAX_CONCURRENT_SOUNDS = 32;
    
    // Volume settings
    float m_masterVolume = 100.0f;
    float m_sfxVolume = 100.0f;
    
    // Positional audio settings
    sf::Vector2f m_listenerPosition{0.0f, 0.0f};
    float m_audibleRange = 800.0f;  // Sounds beyond this distance are silent
    float m_minVolumeDistance = 100.0f;  // Within this distance, full volume
    
    // Asset path helper
    static std::string getSoundPath(const std::string& relativePath);
};

// Convenience macro
#define SOUNDS SoundManager::instance()
