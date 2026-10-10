#pragma once

// ---------------------------------------------------------------------------
// EntityVisual — backend-neutral description of how one entity looks right now.
//
// This is the only place that turns an Entity into drawing information: the
// static part comes from the entity's VisualDef (EntityData), the dynamic part
// (current frame, construction opacity) from the entity itself.  A renderer
// reads an EntityVisual and never needs to know which kind of entity it has.
// ---------------------------------------------------------------------------

#include "sprite/AnimatedSprite.h"
#include <SFML/Graphics/Color.hpp>
#include <string>

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

    // glTF model replacing the sprite/box (empty path = none).
    std::string model;
    float       modelScale = 1.0f;
    float       modelYaw   = 0.0f;        // extra turn of the model, radians
    float       facing     = 0.0f;        // direction the entity looks, radians in the game plane (atan2(dy, dx))
    std::string modelClip;                // animation clip of a skinned model (see Entity::getModelClip)
    float       modelClipTime = 0.0f;     // seconds into the clip

    bool hasSprite() const { return sprite.valid(); }
    bool hasModel()  const { return !model.empty(); }
};

EntityVisual describeVisual(const Entity& entity);
