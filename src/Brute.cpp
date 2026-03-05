#include "Brute.h"
#include "EntityData.h"

Brute::Brute(Team team, sf::Vector2f position)
    : Unit(EntityType::Brute, team, position,
           ENTITY_DATA.getUnitDef(EntityType::Brute)->speed,
           ENTITY_DATA.getUnitDef(EntityType::Brute)->damage,
           ENTITY_DATA.getUnitDef(EntityType::Brute)->attackRange,
           ENTITY_DATA.getUnitDef(EntityType::Brute)->attackCooldown,
           ENTITY_DATA.getSize(EntityType::Brute),
           ENTITY_DATA.getHealth(EntityType::Brute))
{
    // All stats loaded from EntityData registry via Unit base class
}
