#include "Soldier.h"
#include "EntityData.h"
#include <cmath>

Soldier::Soldier(Team team, sf::Vector2f position)
    : Unit(EntityType::Soldier, team, position,
           ENTITY_DATA.getUnitDef(EntityType::Soldier)->speed,
           ENTITY_DATA.getUnitDef(EntityType::Soldier)->damage,
           ENTITY_DATA.getUnitDef(EntityType::Soldier)->attackRange,
           ENTITY_DATA.getUnitDef(EntityType::Soldier)->attackCooldown,
           ENTITY_DATA.getSize(EntityType::Soldier),
           ENTITY_DATA.getHealth(EntityType::Soldier))
{
    auto* unitDef = ENTITY_DATA.getUnitDef(EntityType::Soldier);
    m_autoAttackRangeBonus = unitDef->autoAttackRangeBonus;
    m_isCombatUnit = unitDef->isCombatUnit;
}

void Soldier::updateIdle(float deltaTime) {
    // Auto-attack: if idle and an enemy is in range, attack it
    if (m_isCombatUnit && findNearestEnemy) {
        float autoAttackRange = m_attackRange + m_autoAttackRangeBonus;
        EntityPtr enemy = findNearestEnemy(m_position, autoAttackRange, m_team);
        if (enemy && enemy->isAlive()) {
            attack(enemy);
        }
    }
}
