#pragma once

#include "Entity.h"
#include <SFML/Graphics/CircleShape.hpp>

// A lightweight homing projectile entity.
// - Not selectable, not collidable (flies over terrain/units)
// - Homes onto a target entity
// - Applies damage on impact, then removes itself
class Projectile : public Entity {
public:
    Projectile(EntityPtr source, EntityPtr target, int damage, float speed, sf::Color color = sf::Color::Yellow);
    
    void update(float deltaTime) override;
    void render(sf::RenderTarget& target) override;
    
    // Projectiles are not interactable
    bool isSelectable() const { return false; }
    void takeDamage(int /*damage*/) override { /* rockets are indestructible */ }
    void takeDamage(int /*damage*/, EntityPtr /*attacker*/) override { /* rockets are indestructible */ }

private:
    std::weak_ptr<Entity> m_source;
    std::weak_ptr<Entity> m_target;
    int m_damage;
    float m_speed;
    
    static constexpr float IMPACT_RADIUS = 8.0f;  // Distance at which rocket detonates
    
    sf::CircleShape m_circle;
};
