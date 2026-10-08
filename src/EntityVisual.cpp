#include "EntityVisual.h"
#include "Entity.h"
#include "EntityData.h"
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
    visual.color          = (entity.getTeam() == Team::Neutral) ? entity.getColor() : teamColor(entity.getTeam());
    return visual;
}
