#pragma once

#include "Building.h"
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <array>
#include <memory>
#include <optional>

// Turret — a stationary combat building.
// Automatically targets and fires at the nearest enemy within attack range.
// Uses 8-way directional sprites (idle + fire) matching the turret asset set.
class Turret : public Building {
public:
    Turret(Team team, sf::Vector2f position);

    static void preload();  // Preload all 16 direction textures

    void setupGameContext(IUnitContext* ctx) override { m_context = ctx; }

    void update(float deltaTime) override;
    void render(sf::RenderTarget& target) override;

private:
    // 8 directions — assets are named turret_1.png … turret_8.png
    // (1=E, 2=SE, 3=S, 4=SW, 5=W, 6=NW, 7=N, 8=NE)
    static constexpr int DIR_COUNT = 8;

    // Returns the index (0-7) that best matches the delta vector.
    static int directionIndex(sf::Vector2f delta);

    IUnitContext* m_context = nullptr;

    // Per-direction textures: idle + fire
    std::array<sf::Texture*, DIR_COUNT> m_idleTextures{};
    std::array<sf::Texture*, DIR_COUNT> m_fireTextures{};

    // SFML 3: Sprite requires a texture argument — stored in optional so it
    // can be default-constructed and then swapped per direction.
    std::optional<sf::Sprite> m_sprite;

    // Current direction index (persists after target dies)
    int  m_dirIndex   = 3;  // default SW

    // Combat
    static constexpr float ATTACK_RANGE    = 200.f;
    static constexpr float ATTACK_COOLDOWN = 1.0f;
    static constexpr int   ATTACK_DAMAGE   = 20;
    static constexpr float BULLET_SPEED    = 700.f;
    static constexpr float FIRE_DISPLAY    = 0.25f;  // seconds fire sprite is shown

    float m_attackTimer    = 0.f;
    float m_fireTimer      = 0.f;  // counts down while showing fire sprite
    std::weak_ptr<Entity> m_currentTarget;

    void updateSprite();
};
