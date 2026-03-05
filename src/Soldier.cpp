#include "Soldier.h"
#include "EntityData.h"

Soldier::Soldier(Team team, sf::Vector2f position)
    : Unit(EntityType::Soldier, team, position,
           ENTITY_DATA.getUnitDef(EntityType::Soldier)->speed,
           ENTITY_DATA.getUnitDef(EntityType::Soldier)->damage,
           ENTITY_DATA.getUnitDef(EntityType::Soldier)->attackRange,
           ENTITY_DATA.getUnitDef(EntityType::Soldier)->attackCooldown,
           ENTITY_DATA.getSize(EntityType::Soldier),
           ENTITY_DATA.getHealth(EntityType::Soldier))
{
    // All stats loaded from EntityData registry via Unit base class
}
