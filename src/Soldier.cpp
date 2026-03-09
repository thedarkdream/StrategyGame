#include "Soldier.h"
#include "EntityData.h"
#include "SoundManager.h"

Soldier::Soldier(Team team, sf::Vector2f position)
    : Unit(EntityType::Soldier, team, position)
{
}

void Soldier::onDeath() {
    SOUNDS.playSound("effects/soldier_death.wav", m_position);
}
