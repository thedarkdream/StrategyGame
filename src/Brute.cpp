#include "Brute.h"
#include "EntityData.h"

Brute::Brute(Team team, sf::Vector2f position)
    : Unit(EntityType::Brute, team, position)
{
}
