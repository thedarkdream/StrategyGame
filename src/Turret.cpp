#include "Turret.h"
#include "TextureManager.h"
#include "EntityData.h"
#include "Constants.h"
#include "MathUtil.h"
#include <cmath>
#include <cstdint>
#include <string>

// shorthand
#define TEXTURES TextureManager::instance()

Turret::Turret(Team team, sf::Vector2f position)
    : Building(EntityType::Turret, team, position)
{
    // Load per-direction textures (files: turret_1.png … turret_8.png)
    for (int i = 0; i < DIR_COUNT; ++i) {
        std::string n = std::to_string(i + 1);
        m_idleTextures[i] = TEXTURES.loadTexture("buildings/turret/turret_" + n + ".png");
        m_fireTextures[i] = TEXTURES.loadTexture("buildings/turret/turret_fire_" + n + ".png");
    }

    m_dirIndex = 3;  // default SW (index 3)
    updateSprite();
}

void Turret::preload() {
    for (int i = 0; i < DIR_COUNT; ++i) {
        std::string n = std::to_string(i + 1);
        TEXTURES.loadTexture("buildings/turret/turret_" + n + ".png");
        TEXTURES.loadTexture("buildings/turret/turret_fire_" + n + ".png");
    }
}

int Turret::directionIndex(sf::Vector2f delta) {
    // Maps delta to index 0-7 where 0=E,1=SE,2=S,3=SW,4=W,5=NW,6=N,7=NE
    // Corresponds to asset numbers 1-8 (index+1).
    float angle = std::atan2(delta.y, delta.x) * 180.f / 3.14159265f;
    if (angle < 0.f) angle += 360.f;
    return static_cast<int>((angle + 22.5f) / 45.f) % 8;
}

void Turret::updateSprite() {
    bool firing = (m_fireTimer > 0.f);
    sf::Texture* tex = firing ? m_fireTextures[m_dirIndex] : m_idleTextures[m_dirIndex];
    if (!tex) return;

    if (!m_sprite.has_value())
        m_sprite.emplace(*tex);
    else
        m_sprite->setTexture(*tex, /*resetRect=*/true);

    sf::FloatRect bounds = m_sprite->getLocalBounds();
    m_sprite->setOrigin({bounds.size.x / 2.f, bounds.size.y / 2.f});
    m_sprite->setScale({0.25f, 0.25f});
    m_sprite->setPosition(m_position);
}

void Turret::update(float deltaTime) {
    // Run normal Building update (construction, production, under-attack timer)
    Building::update(deltaTime);

    if (!isConstructed() || !isAlive()) return;

    // Tick attack cooldown
    if (m_attackTimer > 0.f) m_attackTimer -= deltaTime;
    // Tick fire display timer
    if (m_fireTimer  > 0.f) {
        m_fireTimer -= deltaTime;
        if (m_fireTimer < 0.f) m_fireTimer = 0.f;
    }

    if (!m_context) return;

    // Acquire / refresh target
    EntityPtr target = m_currentTarget.lock();
    if (!target || !target->isAlive() ||
        MathUtil::distance(target->getPosition(), m_position) > ATTACK_RANGE + 16.f) {
        // Search for new target
        target = m_context->findNearestEnemy(m_position, ATTACK_RANGE, m_team);
        m_currentTarget = target ? std::weak_ptr<Entity>(target) : std::weak_ptr<Entity>{};
    }

    if (target && target->isAlive()) {
        // Update facing direction
        sf::Vector2f delta = target->getPosition() - m_position;
        m_dirIndex = directionIndex(delta);

        // Fire if cooldown expired
        if (m_attackTimer <= 0.f) {
            m_context->spawnProjectile(shared_from_this(), target, ATTACK_DAMAGE, BULLET_SPEED);
            m_attackTimer = ATTACK_COOLDOWN;
            m_fireTimer   = FIRE_DISPLAY;
        }
    }

    updateSprite();
}

void Turret::render(sf::RenderTarget& target) {
    if (!m_sprite.has_value()) return;

    // Draw tinted sprite while under construction
    if (!isConstructed()) {
        sf::Color c = m_sprite->getColor();
        c.a = static_cast<uint8_t>(128 + static_cast<int>(127 * getConstructionProgress()));
        m_sprite->setColor(c);
    } else {
        m_sprite->setColor(sf::Color::White);
    }

    m_sprite->setPosition(m_position);
    target.draw(*m_sprite);

    renderSelectionIndicator(target);
    renderHealthBar(target);
}
