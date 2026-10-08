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

    void update(float deltaTime) override;

    // Texture for the current facing / firing state (null if not loaded).
    // Consumed by the render layer.
    const sf::Texture* getCurrentTexture() const {
        return (m_fireTimer > 0.f ? m_fireTextures : m_idleTextures)[m_dirIndex];
    }

    // Whole texture, scaled down to world size.
    SpriteFrame getSpriteFrame() const override {
        const sf::Texture* tex = getCurrentTexture();
        if (!tex) return {};
        const sf::Vector2u ts = tex->getSize();
        return { tex,
                 sf::IntRect(sf::Vector2i(0, 0), sf::Vector2i(static_cast<int>(ts.x), static_cast<int>(ts.y))),
                 sf::Vector2f(static_cast<float>(ts.x) * kSpriteScale, static_cast<float>(ts.y) * kSpriteScale) };
    }

private:
    static constexpr float kSpriteScale = 0.25f;

    // 8 directions — assets are named turret_1.png … turret_8.png
    // (1=E, 2=SE, 3=S, 4=SW, 5=W, 6=NW, 7=N, 8=NE)
    static constexpr int DIR_COUNT = 8;

    // Returns the index (0-7) that best matches the delta vector.
    static int directionIndex(sf::Vector2f delta);

    // Per-direction textures: idle + fire
    std::array<sf::Texture*, DIR_COUNT> m_idleTextures{};
    std::array<sf::Texture*, DIR_COUNT> m_fireTextures{};

    // Current direction index (persists after target dies)
    int  m_dirIndex   = 3;  // default SW

    // Combat stats — initialised from EntityData::getCombatBuildingDef in constructor
    float m_attackRange     = 0.f;
    float m_attackCooldown  = 0.f;
    int   m_attackDamage    = 0;
    float m_bulletSpeed     = 0.f;
    float m_fireDisplayTime = 0.f;

    float m_attackTimer    = 0.f;
    float m_fireTimer      = 0.f;  // counts down while showing fire sprite
    std::weak_ptr<Entity> m_currentTarget;
};
