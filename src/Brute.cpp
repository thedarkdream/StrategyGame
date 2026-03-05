#include "Brute.h"
#include "EntityData.h"
#include <cmath>

Brute::Brute(Team team, sf::Vector2f position)
    : Unit(EntityType::Brute, team, position,
           ENTITY_DATA.getUnitDef(EntityType::Brute)->speed,
           ENTITY_DATA.getUnitDef(EntityType::Brute)->damage,
           ENTITY_DATA.getUnitDef(EntityType::Brute)->attackRange,
           ENTITY_DATA.getUnitDef(EntityType::Brute)->attackCooldown,
           ENTITY_DATA.getSize(EntityType::Brute),
           ENTITY_DATA.getHealth(EntityType::Brute))
{
    auto* unitDef = ENTITY_DATA.getUnitDef(EntityType::Brute);
    m_autoAttackRangeBonus = unitDef->autoAttackRangeBonus;
    m_isCombatUnit = unitDef->isCombatUnit;
}

void Brute::updateIdle(float deltaTime) {
    // Auto-attack: if idle and an enemy is in range, attack it
    if (m_isCombatUnit && findNearestEnemy) {
        float autoAttackRange = m_attackRange + m_autoAttackRangeBonus;
        EntityPtr enemy = findNearestEnemy(m_position, autoAttackRange, m_team);
        if (enemy && enemy->isAlive()) {
            attack(enemy);
        }
    }
}
