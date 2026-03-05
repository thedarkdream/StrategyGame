#pragma once

#include "Unit.h"

class Soldier : public Unit {
public:
    Soldier(Team team, sf::Vector2f position);
    
protected:
    void updateIdle(float deltaTime) override;
    
private:
    static constexpr float AUTO_ATTACK_RANGE_BONUS = 10.0f;
};
