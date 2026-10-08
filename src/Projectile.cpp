#include "Projectile.h"
#include "MathUtil.h"
#include "SoundManager.h"
#include <cmath>

Projectile::Projectile(EntityPtr source, EntityPtr target, int damage, float speed, sf::Color color)
    : Entity(EntityType::Rocket, source ? source->getTeam() : Team::Neutral, 
             source ? source->getPosition() : sf::Vector2f{0,0})
    , m_source(source)
    , m_target(target)
    , m_sourceTeam(source ? source->getTeam() : Team::Neutral)
    , m_damage(damage)
    , m_speed(speed)
{
    m_size = {6.0f, 6.0f};
    m_health = 1;
    m_maxHealth = 1;
    m_color = color;
}

void Projectile::update(float deltaTime) {
    if (m_health <= 0) return;
    
    auto target = m_target.lock();
    
    // Target gone - vanish without dealing damage
    if (!target || !target->isAlive()) {
        m_health = 0;
        return;
    }
    
    sf::Vector2f targetPos = target->getPosition();
    sf::Vector2f diff = targetPos - m_position;
    float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);
    
    // Check for impact
    if (dist <= IMPACT_RADIUS) {
        // Hit! Apply damage and die
        SOUNDS.playSound("units/lighttank/explode.wav", m_position);
        target->takeDamage(m_damage, m_sourceTeam);
        m_health = 0;
        return;
    }
    
    // Move toward target
    sf::Vector2f direction = diff / dist;
    float step = m_speed * deltaTime;
    
    // Don't overshoot
    if (step >= dist) {
        m_position = targetPos;
    } else {
        m_position += direction * step;
    }
}

void Projectile::preload() {
    SOUNDS.loadBuffer("units/lighttank/explode.wav");
}
