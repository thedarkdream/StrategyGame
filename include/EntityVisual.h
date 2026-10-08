#pragma once

// ---------------------------------------------------------------------------
// EntityVisual — backend-neutral description of how one entity looks right now.
//
// This is the only place that turns an Entity into drawing information: the
// static part comes from the entity's VisualDef (EntityData), the dynamic part
// (current frame, construction opacity) from the entity itself.  A renderer
// reads an EntityVisual and never needs to know which kind of entity it has.
// ---------------------------------------------------------------------------

#include "AnimatedSprite.h"
#include <SFML/Graphics/Color.hpp>

class Entity;

struct EntityVisual {
    SpriteFrame sprite;              // sprite.valid() == false: drawn as a plain box
    float       opacity        = 1.0f;
    float       spriteAnchor   = 0.25f;   // fraction of the sprite height below the ground point
    float       bodyHeight     = 32.0f;   // world units: stand-in box and picking height
    float       flyHeight      = 0.0f;    // > 0: hovers above the ground
    bool        selectable     = true;
    bool        showsHealthBar = true;
    sf::Color   color;                    // stand-in box colour

    bool hasSprite() const { return sprite.valid(); }
};

EntityVisual describeVisual(const Entity& entity);
