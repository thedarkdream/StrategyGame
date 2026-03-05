#include "Soldier.h"
#include "Constants.h"
#include <cmath>

Soldier::Soldier(Team team, sf::Vector2f position)
    : Unit(EntityType::Soldier, team, position,
           Constants::SOLDIER_SPEED,           // speed
           Constants::SOLDIER_DAMAGE,          // damage
           Constants::SOLDIER_ATTACK_RANGE,    // attack range
           Constants::SOLDIER_ATTACK_COOLDOWN, // attack cooldown
           sf::Vector2f(24.0f, 24.0f),         // size
           Constants::SOLDIER_HEALTH)          // health
{
}

void Soldier::updateIdle(float deltaTime) {
    // Auto-attack: if idle and an enemy is in range, attack it
    if (findNearestEnemy) {
        float autoAttackRange = m_attackRange + AUTO_ATTACK_RANGE_BONUS;
        EntityPtr enemy = findNearestEnemy(m_position, autoAttackRange, m_team);
        if (enemy && enemy->isAlive()) {
            attack(enemy);
        }
    }
}
