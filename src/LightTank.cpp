#include "LightTank.h"
#include "EntityData.h"
#include "Constants.h"
#include "SoundManager.h"

LightTank::LightTank(Team team, sf::Vector2f position)
    : Unit(EntityType::LightTank, team, position)
{
    // Set up circle (40px diameter = 20px radius)
    float radius = m_size.x / 2.0f;  // m_size loaded from EntityData (40x40)
    m_circle.setRadius(radius);
    m_circle.setOrigin({radius, radius});
    m_circle.setPosition(m_position);
    m_circle.setFillColor(m_color);
    m_circle.setOutlineColor(sf::Color::Black);
    m_circle.setOutlineThickness(1.5f);
}

void LightTank::render(sf::RenderTarget& target) {
    // Update circle position and color in case of selection/highlight
    m_circle.setPosition(m_position);
    
    // Show a bright outline when selected
    if (m_selected) {
        // Selection ring based on team
        sf::CircleShape ring;
        float radius = m_size.x / 2.0f;
        ring.setRadius(radius + 3.0f);
        ring.setOrigin({radius + 3.0f, radius + 3.0f});
        ring.setPosition(m_position);
        ring.setFillColor(sf::Color::Transparent);
        if (m_isLocalTeam) {
            ring.setOutlineColor(sf::Color(0, 220, 0));
        } else {
            ring.setOutlineColor(sf::Color(220, 0, 0));
        }
        ring.setOutlineThickness(2.0f);
        target.draw(ring);
    }
    
    target.draw(m_circle);
    
    // Health bar
    renderHealthBar(target);
}

void LightTank::fireAttack(EntityPtr target) {
    if (!target || !target->isAlive()) return;
    
    if (m_context) {
        SOUNDS.playSound("units/lighttank/fire.wav", m_position);
        // Launch a homing rocket
        m_context->spawnProjectile(shared_from_this(), target, m_damage, ROCKET_SPEED);
    } else {
        // Fallback: instant damage if no callback set
        target->takeDamage(m_damage, shared_from_this());
    }
}

void LightTank::onDeath() {
    // Could play a tank death sound here when available
}

void LightTank::preload() {
    SOUNDS.loadBuffer("units/lighttank/fire.wav");
}
