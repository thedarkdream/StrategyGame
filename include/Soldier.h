#pragma once

#include "Unit.h"

class Soldier : public Unit {
public:
    Soldier(Team team, sf::Vector2f position);
    
protected:
    void onDeath() override;
};
