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

    // Pull combat stats from EntityData — single source of truth.
    if (const CombatBuildingDef* cd = ENTITY_DATA.getCombatBuildingDef(EntityType::Turret)) {
        m_attackRange     = cd->attackRange;
        m_attackDamage    = cd->attackDamage;
        m_attackCooldown  = cd->attackCooldown;
        m_bulletSpeed     = cd->projectileSpeed;
        m_fireDisplayTime = cd->fireDisplayTime;
    }

    m_dirIndex = 3;  // default SW (index 3)
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
    float angle = std::atan2(delta.y, delta.x) * 180.f / MathUtil::PI;
    if (angle < 0.f) angle += 360.f;
    return static_cast<int>((angle + 22.5f) / 45.f) % 8;
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
        MathUtil::distance(target->getPosition(), m_position) > m_attackRange + 16.f) {
        // Search for new target
        target = m_context->findNearestEnemy(m_position, m_attackRange, m_team);
        m_currentTarget = target ? std::weak_ptr<Entity>(target) : std::weak_ptr<Entity>{};
    }

    if (target && target->isAlive()) {
        // Update facing direction
        sf::Vector2f delta = target->getPosition() - m_position;
        m_dirIndex = directionIndex(delta);

        // Fire if cooldown expired
        if (m_attackTimer <= 0.f) {
            m_context->spawnProjectile(shared_from_this(), target, m_attackDamage, m_bulletSpeed);
            m_attackTimer = m_attackCooldown;
            m_fireTimer   = m_fireDisplayTime;
        }
    }
}
