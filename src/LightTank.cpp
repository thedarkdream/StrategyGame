#include "LightTank.h"
#include "EntityData.h"
#include "Constants.h"
#include "SoundManager.h"
#include "EntityDrawing.h"

LightTank::LightTank(Team team, sf::Vector2f position)
    : Unit(EntityType::LightTank, team, position)
{
    // Display size stays at 40×40 (m_size set by base class from EntityData).
    // Collision radius is exactly one tile so navigation mesh aligns correctly.
    m_collisionRadius = Constants::TILE_SIZE / 2.0f;  // 16 px

    // Set up circle (40px diameter = 20px radius)
    float radius = m_size.x / 2.0f;  // visual radius — kept as 20 px
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
    EntityDrawing::drawHealthBar(target, *this);
}

void LightTank::fireAttack(EntityPtr target) {
    if (!target || !target->isAlive()) return;
    
    if (m_context) {
        m_context->soundManager().playSound("units/lighttank/fire.wav", m_position);
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
