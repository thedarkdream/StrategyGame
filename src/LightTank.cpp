#include "LightTank.h"
#include "EntityData.h"
#include "Constants.h"
#include "SoundManager.h"

LightTank::LightTank(Team team, sf::Vector2f position)
    : Unit(EntityType::LightTank, team, position)
{
    // Display size stays at 40×40 (m_size set by base class from EntityData).
    // Collision radius is exactly one tile so navigation mesh aligns correctly.
    m_collisionRadius = Constants::TILE_SIZE / 2.0f;  // 16 px
}

void LightTank::fireAttack(EntityPtr target) {
    if (!target || !target->isAlive()) return;
    
    if (m_context) {
        SOUNDS.playSound("units/lighttank/fire.wav", m_position);
        // Launch a homing rocket
        m_context->spawnProjectile(shared_from_this(), target, m_damage, ROCKET_SPEED);
    } else {
        // Fallback: instant damage if no callback set
        target->takeDamage(m_damage, shared_from_this());
    }
}

void LightTank::onDeath() {
    // Could play a tank death sound here when available
}

void LightTank::preload() {
    SOUNDS.loadBuffer("units/lighttank/fire.wav");
}
