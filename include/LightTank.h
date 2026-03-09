#pragma once

#include "Unit.h"
#include <SFML/Graphics/CircleShape.hpp>

// Light Tank - ranged unit that fires homing rockets
// Produced by the Factory, costs 150 minerals
// HP: 120, Damage: 25 per rocket, Attack range: 200, Speed: 150
class LightTank : public Unit {
public:
    LightTank(Team team, sf::Vector2f position);
    
    void render(sf::RenderTarget& target) override;

protected:
    void fireAttack(EntityPtr target) override;
    void onDeath() override;

private:
    sf::CircleShape m_circle;
    
    static constexpr float ROCKET_SPEED = 400.0f;
};
