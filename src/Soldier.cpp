#include "Soldier.h"
#include "EntityData.h"

Soldier::Soldier(Team team, sf::Vector2f position)
    : Unit(EntityType::Soldier, team, position)
{
}
