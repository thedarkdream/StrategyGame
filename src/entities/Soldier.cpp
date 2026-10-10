#include "entities/Soldier.h"
#include "entities/EntityData.h"
#include "media/SoundManager.h"

Soldier::Soldier(Team team, sf::Vector2f position)
    : Unit(EntityType::Soldier, team, position)
{
}

void Soldier::onDeath() {
    if (m_context)
        m_context->soundManager().playSound("effects/soldier_death.wav", m_position);
}

void Soldier::preload() {
    SOUNDS.loadBuffer("effects/soldier_death.wav");
}
