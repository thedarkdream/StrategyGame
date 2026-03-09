#pragma once

#include "Unit.h"

class Soldier : public Unit {
public:
    Soldier(Team team, sf::Vector2f position);
    
    static void preload();  // Preload textures and sounds for this unit type

protected:
    void onDeath() override;
};
