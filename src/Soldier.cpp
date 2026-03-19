#include "Soldier.h"
#include "EntityData.h"
#include "SoundManager.h"

Soldier::Soldier(Team team, sf::Vector2f position)
    : Unit(EntityType::Soldier, team, position)
{
}

void Soldier::onDeath() {
    SoundManager& snd = m_context ? m_context->soundManager() : SOUNDS;
    snd.playSound("effects/soldier_death.wav", m_position);
}

void Soldier::preload() {
    SOUNDS.loadBuffer("effects/soldier_death.wav");
}
