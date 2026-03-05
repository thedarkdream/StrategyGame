#pragma once

#include "Unit.h"

class Brute : public Unit {
public:
    Brute(Team team, sf::Vector2f position);
    
protected:
    void updateIdle(float deltaTime) override;
};
