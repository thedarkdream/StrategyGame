#include "render/EntityVisual.h"
#include "entities/Entity.h"
#include "entities/EntityData.h"
#include <algorithm>

EntityVisual describeVisual(const Entity& entity) {
    const VisualDef& def = ENTITY_DATA.getVisual(entity.getType());
    const sf::Vector2f size = entity.getSize();

    EntityVisual visual;
    visual.sprite         = entity.getSpriteFrame();
    visual.opacity        = entity.getVisualOpacity();
    visual.spriteAnchor   = def.spriteAnchor;
    visual.bodyHeight     = std::min(size.x, size.y) * def.bodyHeight;
    visual.flyHeight      = def.flyHeight;
    visual.selectable     = def.selectable;
    visual.showsHealthBar = def.showsHealthBar;
    visual.model          = def.model;
    visual.modelScale     = def.modelScale;
    visual.modelYaw       = def.modelYaw;
    visual.facing         = entity.getFacingAngle();
    visual.modelClip      = entity.getModelClip();
    // Offset by id so a group of units does not move in lockstep.
    visual.modelClipTime  = entity.getModelClipTime() + static_cast<float>(entity.getId() % 64) * 0.37f;
    visual.color          = (entity.getTeam() == Team::Neutral) ? entity.getColor() : teamColor(entity.getTeam());
    return visual;
}
